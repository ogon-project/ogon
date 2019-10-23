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


#define TAG OGON_TAG("core.appcontext")

static ogon_app_context *g_app_context = NULL;

BOOL app_context_init() {
	g_app_context = malloc(sizeof(ogon_app_context));
	if (!g_app_context) {
		return FALSE;
	}
	g_app_context->connections = ListDictionary_New(FALSE);
	if (!g_app_context->connections) {
		free(g_app_context);
		return FALSE;
	}
	g_app_context->connectionId = 0;
	return TRUE;
}

void app_context_uninit() {
	ListDictionary_Free(g_app_context->connections);
	free(g_app_context);
	g_app_context = NULL;
}

long app_context_get_connectionid() {
	ogon_connection *connection = NULL;
	long connection_id;

	ListDictionary_Lock(g_app_context->connections);

	do {
		g_app_context->connectionId++;
		if (g_app_context->connectionId <= 0) {
			g_app_context->connectionId = 1;
		}
		connection_id = g_app_context->connectionId;
		connection = ListDictionary_GetItemValue(g_app_context->connections, (void*) connection_id);
	} while (connection);

	ListDictionary_Unlock(g_app_context->connections);
	return connection_id;
}

BOOL app_context_get_connection_stats(long id, UINT64 *inBytes, UINT64 *outBytes, UINT64 *inPackets, UINT64 *outPackets) {
	ogon_connection *connection;

	ListDictionary_Lock(g_app_context->connections);
	connection = ListDictionary_GetItemValue(g_app_context->connections, (void*) id);
	if (connection) {
		freerdp_get_stats(connection->context.rdp, inBytes, outBytes, inPackets, outPackets);
	}
	ListDictionary_Unlock(g_app_context->connections);
	return connection != NULL;
}

BOOL app_context_add_connection(ogon_connection *connection) {
	BOOL ret;
	ListDictionary_Lock(g_app_context->connections);
	if (!(ret = ListDictionary_Add(g_app_context->connections, (void*) connection->id, connection))) {
		WLog_ERR(TAG, "error adding connection %ld to list", connection->id);
	} else {
		WLog_DBG(TAG, "added connection %ld", connection->id);
	}
	ListDictionary_Unlock(g_app_context->connections);
	return ret;
}

void app_context_remove_connection(long id) {
	ListDictionary_Lock(g_app_context->connections);
	ListDictionary_Remove(g_app_context->connections, (void*) id);
	ListDictionary_Unlock(g_app_context->connections);
	WLog_DBG(TAG, "removed connection %ld", id);
}

BOOL app_context_post_message_connection(long id, UINT32 type, void *wParam, void *lParam) {
	ogon_connection *connection = NULL;
	BOOL retVal = FALSE;
	ListDictionary_Lock(g_app_context->connections);
	connection = ListDictionary_GetItemValue(g_app_context->connections, (void*) id);
	if (connection) {
		retVal = MessageQueue_Post(connection->commandQueue, (void *)connection, type, wParam, lParam);
	}
	ListDictionary_Unlock(g_app_context->connections);
	return retVal;
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
	 * Sessions being shadowed will unwire the spies when handling the stop event.
	 * These (previous) spies will be stopped in the next loop iteration(s)
	 */

	for (;;)
	{
		HANDLE hThread;
		ogon_connection *conn = NULL;
		long *keys = NULL;
		int nkeys = 0;

		ListDictionary_Lock(g_app_context->connections);

		nkeys = ListDictionary_GetKeys(g_app_context->connections, (ULONG_PTR **) (&keys));
		if (nkeys < 1) {
			ret = nkeys < 0 ? FALSE : TRUE;
			break;
		}

		while (--nkeys >= 0) {
			conn = ListDictionary_GetItemValue(g_app_context->connections, (void *) keys[nkeys]);
			if (conn && conn->shadowing == conn) {
				conn->externalStop = TRUE;
				Queue_Enqueue(threads, conn->runloop->workThread);
				SetEvent(conn->stopEvent);
			}
		}
		free(keys);

		ListDictionary_Unlock(g_app_context->connections);

		while ((hThread = Queue_Dequeue(threads)) != NULL) {
			WaitForSingleObject(hThread, INFINITE);
			CloseHandle(hThread);
		}
	}

	ListDictionary_Unlock(g_app_context->connections);
	Queue_Free(threads);
	return ret;
}
