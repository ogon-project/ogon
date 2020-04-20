/**
 * ogon - Free Remote Desktop Services
 * Internal Communication Protocol (ICP)
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * Martin Haimberger <martin.haimberger@thincast.com>
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
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Library AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifndef _OGON_ICP_H_
#define _OGON_ICP_H_

#include <winpr/synch.h>

int ogon_icp_start(HANDLE shutdown, UINT32 vmajor, UINT32 vminor);
int ogon_icp_shutdown();
void *ogon_icp_get_context();
BOOL ogon_icp_get_protocol_version(void *context, UINT32 *vmajor, UINT32 *vminor);

typedef void (*disconnected_callback)();
void ogon_icp_set_disconnect_cb(disconnected_callback cb);

#endif /* _OGON_ICP_H_ */
