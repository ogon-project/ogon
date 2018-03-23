/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Event Loop
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * David Fort <contact@hardening-consulting.com>
 *
 * This file may be used under the terms of the GNU Affero General
 * Public License version 3 as published by the Free Software Foundation
 * and appearing in the file LICENSE-AGPL included in the distribution
 * of this file.
 *
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Core AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_EPOLL_H
#include <sys/epoll.h>
#else
#include <sys/select.h>
#include <sys/time.h>
#endif

#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <winpr/memory.h>
#include <winpr/file.h>
#include <winpr/synch.h>
#include <winpr/collections.h>
#include "../common/global.h"

#include "eventloop.h"

#define TAG OGON_TAG("core.eventloop")

struct _ogon_event_loop {
#ifdef HAVE_EPOLL_H
	int epollfd;
#else
	int maxFd;
	int numFd;
	fd_set readset;
	fd_set writeset;
	fd_set exceptset;
#endif

	wLinkedList *sources;
	wLinkedList *rescheduled;
	wLinkedList *cleanups;
};

struct _ogon_event_source {
	ogon_event_loop *eventloop;
	ogon_event_loop_cb callback;
	void *data;
	int fd;
	int mask;
	HANDLE handle;
	BOOL markedForRemove;
	int rescheduleMask;
};

ogon_event_loop *eventloop_create(void) {
	ogon_event_loop *ret = calloc(1, sizeof(ogon_event_loop) );
	if (!ret) {
		WLog_ERR(TAG, "error creating event loop");
		return NULL;
	}

#ifdef HAVE_EPOLL_H
	ret->epollfd = epoll_create(1);
	if (ret->epollfd < 0) {
		WLog_ERR(TAG, "error creating epollfd, epoll_create returned %d", errno);
		goto out_free_loop;
	}
#else
	FD_ZERO(&ret->readset);
	FD_ZERO(&ret->writeset);
	FD_ZERO(&ret->exceptset);
#endif

	ret->sources = LinkedList_New();
	if (!ret->sources) {
		WLog_ERR(TAG, "error creating sources LinkedList");
		goto out_epoll;
	}

	ret->cleanups = LinkedList_New();
	if (!ret->cleanups) {
		WLog_ERR(TAG, "error creating cleanups LinkedList");
		goto out_sources;
	}

	ret->rescheduled = LinkedList_New();
	if (!ret->rescheduled) {
		WLog_ERR(TAG, "error creating rescheduled LinkedList");
		goto out_cleanups;
	}

	return ret;

out_cleanups:
	LinkedList_Free(ret->cleanups);
out_sources:
	LinkedList_Free(ret->sources);
out_epoll:
#ifdef HAVE_EPOLL_H
	close(ret->epollfd);
out_free_loop:
#endif
	free(ret);
	return NULL;
}

static void treat_cleanups(ogon_event_loop *evloop) {
	ogon_event_source *source;

	LinkedList_Enumerator_Reset(evloop->cleanups);
	while(LinkedList_Enumerator_MoveNext(evloop->cleanups)) {
		source = LinkedList_Enumerator_Current(evloop->cleanups);
		LinkedList_Remove(evloop->sources, source);

		free(source);
	}

	LinkedList_Clear(evloop->cleanups);
}

static void treat_rescheduled(ogon_event_loop *evloop) {
	ogon_event_source *source;

	while (LinkedList_Count(evloop->rescheduled) > 0)	{
		source = LinkedList_First(evloop->rescheduled);

		if (!source->markedForRemove) {
			int mask = source->rescheduleMask;
			source->rescheduleMask = 0;

			source->callback(mask, source->fd, source->handle, source->data);
		}

		LinkedList_RemoveFirst(evloop->rescheduled);
	}
}

void eventloop_destroy(ogon_event_loop **pevloop) {
	ogon_event_loop *evloop = *pevloop;

	if (!evloop)
		return;

#ifdef HAVE_EPOLL_H
	close(evloop->epollfd);
#endif

	treat_cleanups(evloop);
	LinkedList_Free(evloop->cleanups);

	LinkedList_Enumerator_Reset(evloop->sources);
	while(LinkedList_Enumerator_MoveNext(evloop->sources)) 	{
		free(LinkedList_Enumerator_Current(evloop->sources));
	}
	LinkedList_Free(evloop->sources);
	LinkedList_Free(evloop->rescheduled);
	free(evloop);
	*pevloop = NULL;
}

#ifdef HAVE_EPOLL_H
static inline int computeEpollMask(int mask) {
	int ret = 0;
	if (mask & OGON_EVENTLOOP_READ) {
		ret |= EPOLLIN;
	}
	if (mask & OGON_EVENTLOOP_WRITE) {
		ret |= EPOLLOUT;
	}
	if (mask & OGON_EVENTLOOP_ERROR) {
		ret |= EPOLLERR;
	}
	return ret;
}
#endif

static ogon_event_source *eventloop_add_handle_fd(ogon_event_loop *evloop, int mask,
	HANDLE handle, int fd, ogon_event_loop_cb cb, void *cb_data)
{
#ifdef HAVE_EPOLL_H
	struct epoll_event epollev;
	int r;
#endif
	ogon_event_source *ret;

	if (!(ret = (ogon_event_source *)malloc(sizeof(ogon_event_source)))) {
		return NULL;
	}

	ret->fd = fd;
	ret->mask = mask;
	ret->eventloop = evloop;
	ret->callback = cb;
	ret->data = cb_data;
	ret->handle = handle;
	ret->markedForRemove = FALSE;
	ret->rescheduleMask = 0;

	if (!LinkedList_AddFirst(evloop->sources, ret)) {
		WLog_ERR(TAG, "error LinkedList_AddFirst");
		goto fail_add_list;
	}

#ifdef HAVE_EPOLL_H
	ZeroMemory(&epollev, sizeof(epollev));
	epollev.events = computeEpollMask(mask);
	epollev.data.ptr = ret;

	r = epoll_ctl(evloop->epollfd, EPOLL_CTL_ADD, ret->fd, &epollev);
	if (r < 0) {
		WLog_ERR(TAG, "error epoll_ctl failed with error %d", errno);
		goto fail_epoll_ctl;
	}
#else
	if (evloop->maxFd < ret->fd) {
		evloop->maxFd = ret->fd;
	}
	evloop->numFd++;

	if (mask & OGON_EVENTLOOP_READ) {
		FD_SET(ret->fd, &evloop->readset);
	}
	if (mask & OGON_EVENTLOOP_WRITE) {
		FD_SET(ret->fd, &evloop->writeset);
	}
	if (mask & OGON_EVENTLOOP_ERROR) {
		FD_SET(ret->fd, &evloop->exceptset);
	}
#endif

	return ret;

#ifdef HAVE_EPOLL_H
fail_epoll_ctl:
	LinkedList_Remove(evloop->sources, ret);
#endif
fail_add_list:
	free(ret);
	return NULL;
}


ogon_event_source *eventloop_add_handle(ogon_event_loop *evloop, int mask, HANDLE handle,
	ogon_event_loop_cb cb, void *cb_data)
{
	int fd = GetEventFileDescriptor(handle);
	if (fd < 0) {
		WLog_ERR(TAG, "error GetEventFileDescriptor failed");
		return 0;
	}

	return eventloop_add_handle_fd(evloop, mask, handle, fd, cb, cb_data);
}

ogon_event_source *eventloop_add_fd(ogon_event_loop *evloop, int mask, int fd,
	ogon_event_loop_cb cb, void *cb_data)
{
	if (fd < 0) {
		WLog_ERR(TAG, "error fd cannot be less than 0");
		return 0;
	}
	return eventloop_add_handle_fd(evloop, mask, INVALID_HANDLE_VALUE, fd, cb, cb_data);
}

int eventsource_mask(const ogon_event_source *source) {
	return source->mask;
}

int eventsource_fd(const ogon_event_source *source) {
	return source->fd;
}

void eventsource_store_state(ogon_event_source *source, ogon_source_state *state) {
	assert(source);
	assert(state);

	state->handle = source->handle;
	state->fd = source->fd;
	state->mask = source->mask;
	state->cb = source->callback;
	state->cbdata = source->data;
}

ogon_event_source *eventloop_restore_source(ogon_event_loop *evloop,
	ogon_source_state *state)
{
	ogon_event_source *ret = eventloop_add_fd(evloop, state->mask, state->fd, state->cb, state->cbdata);
	if (ret) {
		ret->handle = state->handle;
	}
	return ret;
}

BOOL eventsource_change_source(ogon_event_source *source, int newMask) {
#ifdef HAVE_EPOLL_H
	int r;
	struct epoll_event epevent;
#endif
	ogon_event_loop *evloop = source->eventloop;

	if (source->mask == newMask) {
		return TRUE;
	}
#ifdef HAVE_EPOLL_H
	ZeroMemory(&epevent, sizeof(epevent));
	epevent.events = computeEpollMask(newMask);
	epevent.data.ptr = source;
	r = epoll_ctl(evloop->epollfd, EPOLL_CTL_MOD, source->fd, &epevent);
	if (r < 0) {
		WLog_ERR(TAG, "error epoll_ctl failed with error %d", errno);
		return FALSE;
	}
#else
	FD_CLR(source->fd, &evloop->readset);
	if (newMask & OGON_EVENTLOOP_READ) {
		FD_SET(source->fd, &evloop->readset);
	}
	FD_CLR(source->fd, &evloop->writeset);
	if (newMask & OGON_EVENTLOOP_WRITE) {
		FD_SET(source->fd, &evloop->writeset);
	}
	FD_CLR(source->fd, &evloop->exceptset);
	if (newMask & OGON_EVENTLOOP_ERROR) {
		FD_SET(source->fd, &evloop->exceptset);
	}
#endif
	source->mask = newMask;
	return TRUE;
}

static BOOL eventsource_reschedule(ogon_event_source *source, int updateMask) {
	int newMask;
	BOOL ret = TRUE;

	assert(source);

	newMask = source->rescheduleMask | updateMask;
	if (!source->rescheduleMask) {
		ogon_event_loop *evloop = source->eventloop;
		ret = LinkedList_AddLast(evloop->rescheduled, source);
	}
	source->rescheduleMask = newMask;
	return ret;
}

BOOL eventsource_reschedule_for_read(ogon_event_source *source) {
	return eventsource_reschedule(source, OGON_EVENTLOOP_READ);
}

BOOL eventsource_reschedule_for_write(ogon_event_source *source) {
	return eventsource_reschedule(source, OGON_EVENTLOOP_WRITE);
}

BOOL eventloop_remove_source(ogon_event_source **sourceP) {
	assert(sourceP);

	ogon_event_source *source = *sourceP;
	ogon_event_loop *evloop = source->eventloop;
	BOOL ret = TRUE;

#ifdef HAVE_EPOLL_H
	if (epoll_ctl(evloop->epollfd, EPOLL_CTL_DEL, source->fd, 0) < 0) {
		WLog_ERR(TAG, "error epoll_ctl failed with error %d", errno);
		ret = FALSE;
	}
#else
	if ((evloop->maxFd == source->fd) && evloop->maxFd) {
		evloop->maxFd--;
	}
	evloop->numFd--;

	FD_CLR(source->fd, &evloop->readset);
	FD_CLR(source->fd, &evloop->writeset);
	FD_CLR(source->fd, &evloop->exceptset);
#endif

	if (!LinkedList_AddFirst(evloop->cleanups, source)) {
		/**
		 * There is nothing we can or should do in this case and since
		 * source->markedForRemove is flagged the only drawback is that
		 * source stays in memory until the eventloop is destroyed
		 */
	}
	source->markedForRemove = TRUE;
	*sourceP = 0;
	return ret;
}




#ifdef HAVE_EPOLL_H

int eventloop_dispatch_loop(ogon_event_loop *evloop, long timeout) {
	struct epoll_event events[64];
	struct epoll_event *epoll_event = events;
	int count, i;
	int mask;

	do {
		count = epoll_wait(evloop->epollfd, events, 64, timeout);
	} while (count < 0 && errno == EINTR);

	if (count < 0) {
		return -1;
	}

	for (i = 0; i < count; i++, epoll_event++) {
		ogon_event_source *source = (ogon_event_source *)epoll_event->data.ptr;
		if (source->markedForRemove) {
			continue;
		}

		mask = 0;
		if ((source->mask & OGON_EVENTLOOP_READ) && (epoll_event->events & EPOLLIN)) {
			mask |= OGON_EVENTLOOP_READ;
		}
		if ((source->mask & OGON_EVENTLOOP_WRITE) && (epoll_event->events & EPOLLOUT)) {
			mask |= OGON_EVENTLOOP_WRITE;
		}
		if ((source->mask & OGON_EVENTLOOP_ERROR) && (epoll_event->events & EPOLLERR)) {
			mask |= OGON_EVENTLOOP_ERROR;
		}
		if ((source->mask & OGON_EVENTLOOP_HANGUP) &&(epoll_event->events & EPOLLHUP)) {
			mask |= OGON_EVENTLOOP_HANGUP;
		}

		source->callback(mask, source->fd, source->handle, source->data);
	}

	treat_rescheduled(evloop);

	if (LinkedList_Count(evloop->cleanups)) {
		treat_cleanups(evloop);
	}
	return count;
}
#else
int eventloop_dispatch_loop(ogon_event_loop *evloop, long timeout) {
	ogon_event_source *source;
	int mask;
	int status, ret;
	fd_set readset, writeset, exceptset;
	struct timeval due;
	struct timeval *duePtr = 0;

	ret = 0;
	if (timeout) {
		due.tv_sec = timeout / 1000;
		due.tv_usec = (timeout % 1000) * 1000;
		duePtr = &due;
	}

	do {
		memcpy(&readset, &evloop->readset, sizeof(readset));
		memcpy(&writeset, &evloop->writeset, sizeof(writeset));
		memcpy(&exceptset, &evloop->exceptset, sizeof(exceptset));

		status = select(evloop->maxFd+1, &readset, &writeset, &exceptset, duePtr);
	} while (status < 0 && errno == EINTR);

	if (status < 0) {
		return -1;
	}

	LinkedList_Enumerator_Reset(evloop->sources);
	while(LinkedList_Enumerator_MoveNext(evloop->sources) && status > 0) {
		source = LinkedList_Enumerator_Current(evloop->sources);
		mask = 0;

		if (FD_ISSET(source->fd, &readset)) {
			mask |= OGON_EVENTLOOP_READ;
			status--;
		}

		if (FD_ISSET(source->fd, &writeset)) {
			mask |= OGON_EVENTLOOP_WRITE;
			status--;
		}

		if (FD_ISSET(source->fd, &exceptset)) {
			mask |= OGON_EVENTLOOP_ERROR;
			status--;
		}

		if (mask) {
			source->callback(mask, source->fd, source->handle, source->data);
			ret++;
		}
	}

	treat_rescheduled(evloop);

	if (LinkedList_Count(evloop->cleanups)) {
		treat_cleanups(evloop);
	}
	return ret;
}
#endif
