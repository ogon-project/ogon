/**
 * ogon - Free Remote Desktop Services
 * pbRPC: Simple Protocol Buffers based RPC
 * Named Pipe Transport
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

#ifndef OGON_RDPSRV_ICP_PIPETRANSPORT_H_
#define OGON_RDPSRV_ICP_PIPETRANSPORT_H_

#include "pbrpc.h"
#include "pbrpc_transport.h"

pbRPCTransportContext* tp_npipe_new();
void tp_npipe_free(pbRPCTransportContext *context);
#endif /* OGON_RDPSRV_ICP_PIPETRANSPORT_H_ */
