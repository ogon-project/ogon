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

#include <winpr/crt.h>
#include <winpr/interlocked.h>

#include "pbRPC.pb-c.h"
#include "pbrpc_utils.h"

DWORD pbrpc_getTag(pbRPCContext *context)
{
	return InterlockedIncrement(&(context->tag));
}

Ogon__Pbrpc__RPCBase* pbrpc_message_new()
{
	Ogon__Pbrpc__RPCBase* msg;

	if (!(msg = calloc(1, sizeof(Ogon__Pbrpc__RPCBase)))) {
		return NULL;
	}

	ogon__pbrpc__rpcbase__init(msg);
	return msg;
}

void pbrpc_message_free(Ogon__Pbrpc__RPCBase* msg, BOOL freePayload)
{
	if (!msg) {
		return;
	}

	if (freePayload && msg->has_payload) {
		free(msg->payload.data);
	}

	if (freePayload) {
		free(msg->errordescription);
	}

	free(msg);
}

void pbrpc_prepare_request(pbRPCContext* context, Ogon__Pbrpc__RPCBase* msg)
{
	msg->tag = pbrpc_getTag(context);
	msg->isresponse = FALSE;
	msg->status = OGON__PBRPC__RPCBASE__RPCSTATUS__SUCCESS;
}

void pbrpc_prepare_response(Ogon__Pbrpc__RPCBase* msg, UINT32 tag)
{
	msg->isresponse = TRUE;
	msg->tag = tag;
}

void pbrpc_prepare_error(Ogon__Pbrpc__RPCBase* msg, UINT32 tag, char *error)
{
	pbrpc_prepare_response(msg, tag);
	msg->status = OGON__PBRPC__RPCBASE__RPCSTATUS__FAILED;
	msg->errordescription = error;
}

pbRPCPayload* pbrpc_payload_new()
{
	pbRPCPayload* pl = calloc(1, sizeof(pbRPCPayload));
	return pl;
}

void pbrpc_free_payload(pbRPCPayload* response)
{
	if (!response) {
		return;
	}

	free(response->data);

	if (response->errorDescription) {
		free(response->errorDescription);
	}

	free(response);
}


void pbrpc_message_free_response(Ogon__Pbrpc__RPCBase* msg) {
	if (!msg) {
		return;
	}

	ogon__pbrpc__rpcbase__free_unpacked(msg, NULL);
}
