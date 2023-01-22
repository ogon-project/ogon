/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Application Context
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
 * Martin Haimberger <martin.haimberger@thincast.com>
 * Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/interlocked.h>

#include "../common/global.h"
#include "app_context.h"
#include "peer.h"

#include <map>
#include <mutex>
#include <string>

#define TAG OGON_TAG("core.appcontext")

/** @brief the global ogon context */
typedef struct _ogon_app_context {
	std::map<long, ogon_connection *> map;
	std::mutex mux;
	LONG connectionId = 0;
} ogon_app_context;

static ogon_app_context *g_app_context = nullptr;

BOOL app_context_init(void) {
	g_app_context = new (ogon_app_context);
	if (!g_app_context) {
		return FALSE;
	}
	return TRUE;
}

void app_context_uninit(void) {
	delete (g_app_context);
	g_app_context = nullptr;
}

long app_context_get_connectionid() {
	std::lock_guard lock(g_app_context->mux);

	while (g_app_context->map.find(g_app_context->connectionId) !=
			g_app_context->map.end())
		g_app_context->connectionId++;

	return g_app_context->connectionId;
}

BOOL app_context_add_connection(ogon_connection *connection) {
	BOOL ret;

	std::lock_guard lock(g_app_context->mux);
	g_app_context->map.emplace(connection->id, connection);
	WLog_DBG(TAG, "added connection %ld", connection->id);

	return ret;
}

void app_context_remove_connection(long id) {
	std::lock_guard lock(g_app_context->mux);
	g_app_context->map.erase(id);
	WLog_DBG(TAG, "removed connection %ld", id);
}

BOOL app_context_post_message_connection(
		long id, UINT32 type, void *wParam, void *lParam) {
	std::lock_guard lock(g_app_context->mux);
	auto connection = g_app_context->map.find(id);
	if (connection == g_app_context->map.end()) {
		return FALSE;
	}

	return MessageQueue_Post(connection->second->commandQueue,
			(void *)connection->second, type, wParam, lParam);
}

BOOL app_context_stop_all_connections(void) {
	wQueue *threads;
	BOOL ret = TRUE;

	if (!(threads = Queue_New(FALSE, 0, 0))) {
		return FALSE;
	}

	/**
	 * In order to avoid race conditions we may only stop connections that are
	 * currently not remote controlling other sessions.
	 * Sessions being shadowed will unwire the spies when handling the stop
	 * event. These (previous) spies will be stopped in the next loop
	 * iteration(s)
	 */

	for (;;) {
		HANDLE hThread;
		ogon_connection *conn = NULL;
		int nkeys = 0;

		std::lock_guard lock(g_app_context->mux);
		for (auto iter : g_app_context->map) {
			auto conn = iter.second;
			if (conn && (conn->shadowing == conn)) {
				conn->externalStop = TRUE;
				Queue_Enqueue(threads, conn->runloop->workThread);
				SetEvent(conn->stopEvent);
			}
		}

		while ((hThread = Queue_Dequeue(threads)) != NULL) {
			WaitForSingleObject(hThread, INFINITE);
			CloseHandle(hThread);
		}
	}

	Queue_Free(threads);
	return ret;
}
