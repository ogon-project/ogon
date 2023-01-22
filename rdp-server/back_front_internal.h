/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Backend
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
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

#ifndef OGON_RDPSRV_BACK_FRONT_INTERNAL_H_
#define OGON_RDPSRV_BACK_FRONT_INTERNAL_H_

#include <freerdp/codec/region.h>
#include <freerdp/utils/ringbuffer.h>
#include <winpr/collections.h>

#include <ogon/backend.h>

#include "commondefs.h"
#include "eventloop.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BOOL ogon_connection_init_front(ogon_connection *conn);
int frontend_handle_frame_sent(ogon_connection *conn);
void handle_wait_timer_state(ogon_connection *conn);
BOOL ogon_frontend_install_frame_timer(ogon_connection *conn);
int ogon_resize_frontend(
		ogon_connection *conn, ogon_backend_connection *backend);
void frontend_destroy(ogon_front_connection *front);
int frontend_handle_sync_reply(ogon_connection *conn);

int ogon_backend_consume_damage(ogon_connection *conn);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OGON_RDPSRV_BACKEND_H_ */
