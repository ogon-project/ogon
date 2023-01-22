/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Frontend
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#ifndef OGON_RDPSRV_FRONTEND_H_
#define OGON_RDPSRV_FRONTEND_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void handle_wait_timer_state(ogon_connection *conn);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OGON_RDPSRV_FRONTEND_H_ */
