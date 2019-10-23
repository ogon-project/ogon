/**
 * ogon - Free Remote Desktop Services
 * Internal Communication Protocol (ICP)
 * Server Stubs
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
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifndef _OGON_RDPSRV_ICPSERVERSTUBS_H_
#define _OGON_RDPSRV_ICPSERVERSTUBS_H_

#include "pbrpc.h"

int ping(LONG tag, pbRPCPayload *request, pbRPCPayload **response);
int switchTo(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse);
int logoffUserSession(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse);
int otsapiVirtualChannelOpen(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse);
int otsapiVirtualChannelClose(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse);
int otsapiStartRemoteControl(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload **pbresponse);
int otsapiStopRemoteControl(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload **pbresponse);
int message(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse);
int icpLogonInfo(LONG tag, pbRPCPayload *pbrequest, pbRPCPayload **pbresponse);
int icpConnectionStats(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse);

#endif /* _OGON_RDPSRV_ICPSERVERSTUBS_H_ */
