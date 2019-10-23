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

#ifndef _OGON_RDPSRV_APPCONTEXT_H_
#define _OGON_RDPSRV_APPCONTEXT_H_

#include <winpr/collections.h>

#include "commondefs.h"

/** @brief the global ogon context */
typedef struct _ogon_app_context {
	wListDictionary *connections;
	volatile LONG connectionId;
} ogon_app_context;

BOOL app_context_init();
void app_context_uninit();
long app_context_get_connectionid();
BOOL app_context_get_connection_stats(long id, UINT64 *inBytes, UINT64 *outBytes, UINT64 *inPackets, UINT64 *outPackets);
BOOL app_context_add_connection(ogon_connection *connection);
void app_context_remove_connection(long id);
BOOL app_context_post_message_connection(long id, UINT32 type,void *wParam, void *lParam);
BOOL app_context_stop_all_connections();

#endif /* _OGON_RDPSRV_APPCONTEXT_H_ */
