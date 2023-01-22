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

#ifndef OGON_RDPSRV_EVENTLOOP_H_
#define OGON_RDPSRV_EVENTLOOP_H_

#include <winpr/handle.h>
#include <winpr/wtypes.h>

struct _ogon_event_loop;
struct _ogon_event_source;

typedef struct _ogon_event_loop ogon_event_loop;
typedef struct _ogon_event_source ogon_event_source;

/** @brief flag to test in the event loop */
enum {
	OGON_EVENTLOOP_READ		= 0x01,
	OGON_EVENTLOOP_WRITE		= 0x02,
	OGON_EVENTLOOP_HANGUP	= 0x04,
	OGON_EVENTLOOP_ERROR		= 0x08,
};

typedef int (*ogon_event_loop_cb)(int mask, int fd, HANDLE handle, void *data);

/** @brief timer callback */
typedef void (*ogon_event_loop_timer_cb)(void *data);


/** @returns a newly created event loop, NULL in case of error */
ogon_event_loop *eventloop_create(void);

/** destroys the eventloop and all associated internal data
 * @param evloop the eventloop
 */
void eventloop_destroy(ogon_event_loop **evloop);

/**	adds a winPR handle in the event loop and returns the eventSource that can be
 * used to remove the source or change tested mask
 *
 * @param evloop the event loop
 * @param mask the bits to test
 * @param handle the handle that should be monitored
 * @param cb a callback function
 * @param cb_data the data that will be passed to the callback function
 * @return an ogon_event_source, NULL if it failed
 */
ogon_event_source *eventloop_add_handle(ogon_event_loop *evloop, int mask, HANDLE handle,
	ogon_event_loop_cb cb, void *cb_data);

/** adds a raw file descriptor in the event loop and returns the associated evenSource.
 *
 * @param evloop the event loop
 * @param mask the bits to test
 * @param fd a file descriptor to monitor
 * @param cb a callback function
 * @param cb_data the data that will be passed to the callback function
 * @return an ogon_event_source, NULL if it failed
 */
ogon_event_source *eventloop_add_fd(ogon_event_loop *evloop, int mask, int fd,
	ogon_event_loop_cb cb, void *cb_data);

/** registers a timer against the event loop and returns the associated eventSource
 *
 * @param evloop the event loop
 * @param timeout
 * @param cb callback to call
 * @param cb_data data to pass to the callback
 * @return an ogon_event_source, NULL if it failed
 */
ogon_event_source *eventloop_add_timer(ogon_event_loop *evloop, UINT32 timeout,
		ogon_event_loop_timer_cb cb, void *cb_data);

/** @brief contains the informations to save and restore an eventsource */
struct _ogon_source_state {
	HANDLE handle;
	int fd;
	int mask;
	ogon_event_loop_cb cb;
	void *cbdata;
};
typedef struct _ogon_source_state ogon_source_state;

/** dumps the information needed to reimplant an eventsource using eventloop_restore_source
 *
 * @param source a ogon_event_source to dump
 * @param state dumps the state here
 */
void eventsource_store_state(ogon_event_source *source, ogon_source_state *state);

/** restores an event source using a previously stored state
 *
 * @param evloop the target event loop
 * @param state the informations about the event source
 * @return a new ogon_event_source
 */
ogon_event_source *eventloop_restore_source(ogon_event_loop *evloop, ogon_source_state *state);

/** changes the tested items for the given source
 *
 * @param source the eventSource
 * @param newmask new items to test
 * @return if the operation was successful
 */
BOOL eventsource_change_source(ogon_event_source *source, int newmask);

/** returns the current mask for this event source
 * @param source the eventSource
 * @return if the current mask od tested items
 */
int eventsource_mask(const ogon_event_source *source);

/** returns the file descriptor associated with this eventsource
 * @param source
 * @return
 */
int eventsource_fd(const ogon_event_source *source);

/** Artificially set this eventsource as read ready.
 * @param source the eventsource
 * @return nonzero on success, 0 otherwise
 */
BOOL eventsource_reschedule_for_read(ogon_event_source *source);

/** Artificially set this eventsource as write ready.
 * @param source the eventsource
 * @return nonzero on success, 0 otherwise
 */
BOOL eventsource_reschedule_for_write(ogon_event_source *source);

/** removes a file descriptor or HANDLE from the eventloop, the eventSource is
 * removed from the eventloop, its memory is freed and the corresponding pointer
 * is zeroed.
 *
 * @param evloop the event loop
 * @param source a pointer on an eventSource
 * @return if the operations was successful
 */
BOOL eventloop_remove_source(ogon_event_source **sourceP);


/** do one cycle of epoll()/select(), dispatch events, treat cleanups and return
 * the number of event that have been treated.
 *
 * @param evloop the event loop
 * @param timeout the timeout in milliseconds
 * @return the number of treated events
 */
int eventloop_dispatch_loop(ogon_event_loop *evloop, long timeout);

#endif /* OGON_RDPSRV_EVENTLOOP_H_ */
