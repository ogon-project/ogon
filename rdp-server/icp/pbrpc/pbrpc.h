/**
 * ogon - Free Remote Desktop Services
 * pbRPC: Simple Protocol Buffers based RPC
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

#ifndef _OGON_RDPSRV_ICP_PBRPC_H_
#define _OGON_RDPSRV_ICP_PBRPC_H_

#include <winpr/synch.h>
#include <winpr/wtypes.h>
#include <winpr/collections.h>

#include "../common/icp.h"
#include "pbrpc_transport.h"

#define PBRPC_TIMEOUT 10000

typedef struct pbrpc_method pbRPCMethod;

struct  pbrpc_context
{
	HANDLE stopEvent;
	HANDLE thread;
	pbRPCTransportContext* transport;
	wListDictionary* transactions;
	wQueue* writeQueue;
	BOOL isConnected;
	UINT32 localVersionMajor;
	UINT32 localVersionMinor;
	UINT32 remoteVersionMajor;
	UINT32 remoteVersionMinor;
	BOOL runLoop;
	LONG tag;
	pbRPCMethod* methods;
	HANDLE shutdown;
	disconnected_callback disconnectedCb;
};
typedef struct pbrpc_context pbRPCContext;

struct pbrpc_payload
{
	char* data;
	UINT32 dataLen;
	char* errorDescription;
};
typedef struct pbrpc_payload pbRPCPayload;

typedef int (*pbRPCCallback)(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse);

struct pbrpc_method
{
	UINT32 type;
	pbRPCCallback cb;
};

/* Return codes 0-99 are used from the pbrpc protocol itself */
typedef enum pbrpc_status
{
	PBRPC_SUCCESS = 0,              /* everything is fine */
	PBRPC_FAILED = 1,               /* request failed optional error string might be set */
	PBRPC_NOTFOUND = 2,             /* method was not found on server */
	PBRPC_BAD_REQUEST_DATA = 100,   /* request couldn't be serialized */
	PBRPC_BAD_RESPONSE = 101,       /* response couldn't be  unserialized */
	PBRCP_TRANSPORT_ERROR = 102,    /* problem with transport */
	PBRCP_CALL_TIMEOUT = 103,       /* call timed out */
	PBRCP_OUTOFMEMORY = 104,        /* memory allocation failure */
} PBRPCSTATUS;


#ifndef PROTOBUF_C_pbRPC_2eproto__INCLUDED
typedef struct _Ogon__Pbrpc__RPCBase Ogon__Pbrpc__RPCBase;
#endif
typedef void (*pbRpcResponseCallback)(UINT32 reason, Ogon__Pbrpc__RPCBase* response, void *args);

pbRPCContext* pbrpc_server_new(pbRPCTransportContext* transport, HANDLE shutdown);
void pbrpc_server_free(pbRPCContext* context);
int pbrpc_server_start(pbRPCContext* context, UINT32 vmajor, UINT32 vminor);
int pbrpc_server_stop(pbRPCContext* context);
void pbrpc_register_methods(pbRPCContext* context, pbRPCMethod* methods);

int pbrpc_call_method(pbRPCContext* context, UINT32 type, pbRPCPayload* request, pbRPCPayload** response);
void pbrcp_call_method_async(pbRPCContext* context, UINT32 type, pbRPCPayload* request,
		pbRpcResponseCallback callback, void *callback_args);

int pbrpc_respond_method(pbRPCContext* context, pbRPCPayload *response, UINT32 status, UINT32 type, UINT32 tag);


#endif /* _OGON_RDPSRV_ICP_PBRPC_H_ */
