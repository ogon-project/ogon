/**
 * ogon - Free Remote Desktop Services
 * pbRPC: Simple Protocol Buffers based RPC
 * Utility Functions
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

#ifndef _OGON_RDPSRV_ICP_PBRPCUTILS_H_
#define _OGON_RDPSRV_ICP_PBRPCUTILS_H_

#include "pbRPC.pb-c.h"
#include "pbrpc.h"

DWORD pbrpc_getTag(pbRPCContext *context);
Ogon__Pbrpc__RPCBase *pbrpc_message_new();
void pbrpc_message_free(Ogon__Pbrpc__RPCBase* msg, BOOL freePayload);
void pbrpc_message_free_response(Ogon__Pbrpc__RPCBase* msg);
void pbrpc_prepare_request(pbRPCContext* context, Ogon__Pbrpc__RPCBase* msg);
void pbrpc_prepare_response(Ogon__Pbrpc__RPCBase* msg, UINT32 tag);
void pbrpc_prepare_error(Ogon__Pbrpc__RPCBase* msg, UINT32 tag, char* error);
pbRPCPayload* pbrpc_payload_new();
void pbrpc_free_payload(pbRPCPayload* response);

#endif /* _OGON_RDPSRV_ICP_PBRPCUTILS_H_ */
