/**
 * ogon - Free Remote Desktop Services
 * pbRPC: Simple Protocol Buffers based RPC
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
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
#include <winpr/thread.h>

#include <ogon/version.h>

#ifndef WIN32
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#else //WIN32
#include <Winsock2.h>
#endif //WIN32

#include "pbrpc_transport.h"
#include "pbRPC.pb-c.h"
#include "pbrpc_utils.h"
#include "pbrpc.h"

#include "../../../common/global.h"

#define TAG OGON_TAG("icp.pbrpc")

//#define WITH_DEBUG_PBRPC 1
#ifdef WITH_DEBUG_PBRPC
#define DEBUG_PBRPC(fmt, ...) WLog_DBG(TAG, fmt, ## __VA_ARGS__)
#else
#define DEBUG_PBRPC(fmt, ...) do { } while (0)
#endif


struct pbrpc_transaction
{
	pbRpcResponseCallback responseCallback;
	void *callbackArg;
};
typedef struct pbrpc_transaction pbRPCTransaction;


static pbRPCTransaction* pbrpc_transaction_new()
{
	return calloc(1, sizeof(pbRPCTransaction));
}

static void queue_item_free(void* obj)
{
	pbrpc_message_free((Ogon__Pbrpc__RPCBase*)obj, TRUE);
}

static void list_dictionary_item_free(void* value)
{
	free(value);
}

pbRPCContext* pbrpc_server_new(pbRPCTransportContext* transport, HANDLE shutdown)
{
	pbRPCContext* context;

	if (!(context = calloc(1, sizeof(pbRPCContext)))) {
		return NULL;
	}

	if (!(context->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		goto out_free;
	}

	context->transport = transport;
	if (!(context->transactions = ListDictionary_New(TRUE))) {
		goto out_close_handle;
	}

	context->transactions->objectValue.fnObjectFree = list_dictionary_item_free;
	if (!(context->writeQueue = Queue_New(TRUE, -1, -1))) {
		goto out_free_transactions;
	}

	context->writeQueue->object.fnObjectFree = queue_item_free;
	context->shutdown = shutdown;
	return context;

out_free_transactions:
	ListDictionary_Free(context->transactions);
out_close_handle:
	CloseHandle(context->stopEvent);
out_free:
	free(context);
	return NULL;
}

void pbrpc_server_free(pbRPCContext* context)
{
	if (!context)
		return;

	CloseHandle(context->stopEvent);
	CloseHandle(context->thread);
	ListDictionary_Free(context->transactions);
	Queue_Free(context->writeQueue);
	free(context);
}

static int pbrpc_receive_message(pbRPCContext* context, char** msg, int* msgLen)
{
	UINT32 msgLenWire, len;
	char *recvbuffer;
	int ret = 0;

	ret = context->transport->read(context->transport, (char *)&msgLenWire, 4);

	if (ret < 0) {
		return ret;
	}

	len = ntohl(msgLenWire);
	*msgLen = len;
	if (!(recvbuffer = malloc(len))) {
		return -1;
	}

	ret = context->transport->read(context->transport, recvbuffer, len);
	if (ret < 0) {
		free(recvbuffer);
		return ret;
	}

	*msg = recvbuffer;
	return ret;
}

static int pbrpc_send_message(pbRPCContext* context, char *msg, UINT32 msgLen)
{
	UINT32 msgLenWire;
	int ret;

	msgLenWire = htonl(msgLen);

	ret = context->transport->write(context->transport, (char *)&msgLenWire, 4);
	if (ret < 0) {
		return ret;
	}

	ret = context->transport->write(context->transport, msg, msgLen);
	if (ret < 0) {
		return ret;
	}
	return 0;
}

static int pbrpc_process_response(pbRPCContext* context, Ogon__Pbrpc__RPCBase *rpcmessage)
{
	pbRPCTransaction *ta;

	if (!(ta = ListDictionary_Remove(context->transactions, (void *)((UINT_PTR)rpcmessage->tag)))) {
		WLog_ERR(TAG, "Unsolicited response - ignoring (tag %"PRIu32")", rpcmessage->tag);
		ogon__pbrpc__rpcbase__free_unpacked(rpcmessage, NULL);
		return 1;
	}

	if (ta->responseCallback) {
		ta->responseCallback(rpcmessage->status, rpcmessage, ta->callbackArg);
	}

	free(ta);
	return 0;
}

static int pbrpc_process_message_out(pbRPCContext* context, Ogon__Pbrpc__RPCBase *msg)
{
	int ret;
	int msgLen = ogon__pbrpc__rpcbase__get_packed_size(msg);
	char *buf = malloc(msgLen);
	if (!buf)
		return -1;

	ret = ogon__pbrpc__rpcbase__pack(msg, (uint8_t *)buf);
	DEBUG_PBRPC("sending tag %"PRIu32", type %"PRIu32", response: %d", msg->tag, msg->msgtype, msg->isresponse);
	// packing failed..
	if (ret != msgLen) {
		ret = 1;
	} else {
		ret = pbrpc_send_message(context, buf, msgLen);
	}

	free(buf);
	return ret;
}

static pbRPCCallback pbrpc_callback_find(pbRPCContext* context, UINT32 type)
{
	pbRPCMethod *cb = NULL;
	int i = 0;

	if (!context->methods) {
		return NULL;
	}

	while ((cb = &(context->methods[i++])))	{
		if ((cb->type == 0) && (cb->cb == NULL)) {
			return NULL;
		}
		if (cb->type == type) {
			return cb->cb;
		}
	}
	return NULL;
}

static pbRPCPayload* pbrpc_fill_payload(Ogon__Pbrpc__RPCBase *message)
{
	pbRPCPayload *pl;

	if (!(pl = pbrpc_payload_new())) {
		return NULL;
	}

	pl->data = (char *)(message->payload.data);
	pl->dataLen = message->payload.len;
	pl->errorDescription = message->errordescription;

	return pl;
}

static Ogon__Pbrpc__RPCBase *pbrpc_response_new(pbRPCPayload *response, UINT32 status, UINT32 type, UINT32 tag) {
	Ogon__Pbrpc__RPCBase *pbresponse;

	if (!(pbresponse = pbrpc_message_new())) {
		return NULL;
	}

	pbrpc_prepare_response(pbresponse, tag);
	pbresponse->msgtype = type;
	pbresponse->status = status;

	if (status == 0) {
		if (response) {
			pbresponse->has_payload = 1;
			pbresponse->payload.data = (unsigned char*) response->data;
			pbresponse->payload.len = response->dataLen;
		}
	} else {
			if (response) {
				pbresponse->errordescription = response->errorDescription;
			}
	}

	return pbresponse;

}

static int pbrpc_send_response(pbRPCContext* context, pbRPCPayload *response, UINT32 status, UINT32 type, UINT32 tag)
{
	int ret;
	Ogon__Pbrpc__RPCBase *pbresponse;

	if (!(pbresponse = pbrpc_response_new(response, status, type, tag))) {
		WLog_ERR(TAG, "error creating new pbrpc response");
		pbrpc_free_payload(response);
		return PBRPC_FAILED;
	}

	ret = pbrpc_process_message_out(context, pbresponse);
	pbrpc_free_payload(response);
	pbrpc_message_free(pbresponse, FALSE);
	return ret;
}

int pbrpc_respond_method(pbRPCContext* context, pbRPCPayload *response, UINT32 status, UINT32 type, UINT32 tag) {
	Ogon__Pbrpc__RPCBase *pbresponse;

	if (!context->isConnected) {
		return PBRCP_TRANSPORT_ERROR;
	}

	if (!(pbresponse = pbrpc_response_new(response, status, type, tag))) {
		WLog_ERR(TAG, "error creating new pbrpc response");
		return PBRPC_FAILED;
	}

	if (!Queue_Enqueue(context->writeQueue, pbresponse)) {
		WLog_ERR(TAG, "error enqueuing pbrpc response");
		pbrpc_message_free(pbresponse, FALSE);
		return PBRPC_FAILED;
	}

	free(response);
	return PBRPC_SUCCESS;
}

static int pbrpc_process_request(pbRPCContext* context, Ogon__Pbrpc__RPCBase *rpcmessage)
{
	int ret = 0;
	pbRPCCallback cb;
	pbRPCPayload *request = NULL;
	pbRPCPayload *response = NULL;

	if (!(cb = pbrpc_callback_find(context, rpcmessage->msgtype))) {
		WLog_ERR(TAG, "server callback not found %"PRIu32"", rpcmessage->msgtype);
		ret = pbrpc_send_response(context, NULL, OGON__PBRPC__RPCBASE__RPCSTATUS__NOTFOUND, rpcmessage->msgtype, rpcmessage->tag);
		ogon__pbrpc__rpcbase__free_unpacked(rpcmessage, NULL);
		return ret;
	}

	if (!(request = pbrpc_fill_payload(rpcmessage))) {
		WLog_ERR(TAG, "memory allocation failed");
		ret = pbrpc_send_response(context, NULL, OGON__PBRPC__RPCBASE__RPCSTATUS__FAILED, rpcmessage->msgtype, rpcmessage->tag);
		ogon__pbrpc__rpcbase__free_unpacked(rpcmessage, NULL);
		return ret;
	}
	ret = cb(rpcmessage->tag, request, &response);
	free(request);

	/* If callback doesn't set a respond response needs to be sent ansync */
	if (!response) {
		ogon__pbrpc__rpcbase__free_unpacked(rpcmessage, NULL);
		return 0;
	}

	ret = pbrpc_send_response(context, response, ret, rpcmessage->msgtype, rpcmessage->tag);
	ogon__pbrpc__rpcbase__free_unpacked(rpcmessage, NULL);
	return ret;
}

// errors < 0 transport erros, errors > 0 pb errors
int pbrpc_process_message_in(pbRPCContext* context)
{
	char *msg;
	int msgLen;
	int ret = 0;
	Ogon__Pbrpc__RPCBase* rpcmessage;

	if (pbrpc_receive_message(context, &msg, &msgLen) < 0) {
		return -1;
	}
	/* WLog_DBG(TAG, "received message with len %d", msgLen); */
	rpcmessage = ogon__pbrpc__rpcbase__unpack(NULL, msgLen, (uint8_t*) msg);

	free(msg);

	if (!rpcmessage) {
		return 1;
	}

	DEBUG_PBRPC("received tag %"PRIu32" size %"PRIuz", type %"PRIu32" status %d, response %d", rpcmessage->tag,
							rpcmessage->payload.len, rpcmessage->msgtype, rpcmessage->status,
							rpcmessage->isresponse);

	if (rpcmessage->isresponse) {
		ret = pbrpc_process_response(context, rpcmessage);
	} else {
		ret = pbrpc_process_request(context, rpcmessage);
	}

	return ret;
}

static int pbrpc_transport_open(pbRPCContext* context)
{
	UINT32 sleepInterval = 200;

	while (1) {
		if (0 == context->transport->open(context->transport, sleepInterval)) {
			return 0;
		}
		if (WaitForSingleObject(context->stopEvent, 0) == WAIT_OBJECT_0) {
			return -1;
		}
		Sleep(sleepInterval);
	}
}

static BOOL pbrpc_connect(pbRPCContext* context, DWORD timeout)
{
	int msgLen;
	Ogon__Pbrpc__RPCBase* message;
	Ogon__Pbrpc__VersionInfo versionInfo;
	DWORD ret;
	DWORD nCount;
	HANDLE events[32];
	HANDLE thandle;
	char *msg = NULL;
	int status;

	if (0 != pbrpc_transport_open(context)) {
		return FALSE;
	}

	ogon__pbrpc__version_info__init(&versionInfo);
	versionInfo.vmajor = context->localVersionMajor;
	versionInfo.vminor = context->localVersionMinor;

	if (!(message = pbrpc_message_new())) {
		WLog_ERR(TAG, "error while creating a new pbrpc message, disconnecting!");
		goto error_out;
	}

	pbrpc_prepare_request(context, message);
	message->msgtype = -1;
	message->versioninfo = &versionInfo;

	status = pbrpc_process_message_out(context, message);
	pbrpc_message_free(message, TRUE);
	if (status < 0) {
		WLog_ERR(TAG, "fatal error, unable to send version message");
		goto error_out;
	}

	if (!(thandle = context->transport->get_fds(context->transport))) {
		WLog_ERR(TAG, "fatal error, failed to get transport fds");
		goto error_out;
	}
	nCount = 0;
	events[nCount++] = context->stopEvent;
	events[nCount++] = thandle;

	ret = WaitForMultipleObjects(nCount, events, FALSE, timeout);

	if (ret == WAIT_FAILED)	{
		WLog_ERR(TAG, "WaitForMultipleObjects failed!");
		goto error_out;
	}

	if (WaitForSingleObject(context->stopEvent, 0) == WAIT_OBJECT_0) {
		WLog_INFO(TAG, "pbrpc stop event received!");
		goto error_out;
	}

	if (pbrpc_receive_message(context, &msg, &msgLen) < 0) {
		WLog_ERR(TAG, "receive message failed!");
		goto error_out;
	}
	message = ogon__pbrpc__rpcbase__unpack(NULL, msgLen, (uint8_t*) msg);
	free(msg);

	if (!message->isresponse) {
		WLog_ERR(TAG, "error, there was no versionInfo reply packet!");
		goto error_out;
	}

	if (message->versioninfo == NULL) {
		WLog_ERR(TAG, "error, was not versionInfo reply packet!");
		goto error_out;
	}

	if (message->versioninfo->vmajor != OGON_PROTOCOL_VERSION_MAJOR) {
		WLog_ERR(TAG, "error, ogon-session-manager (protocol %"PRIu32".%"PRIu32") is not compatible with ogon(%u.%u)!",
			message->versioninfo->vmajor, message->versioninfo->vminor,
			OGON_PROTOCOL_VERSION_MAJOR, OGON_PROTOCOL_VERSION_MINOR);
		context->transport->close(context->transport);
		goto error_out;
	}

	WLog_DBG(TAG, "Version information received from session-manager: %"PRIu32".%"PRIu32"",
			message->versioninfo->vmajor, message->versioninfo->vminor);
	context->remoteVersionMajor = message->versioninfo->vmajor;
	context->remoteVersionMinor = message->versioninfo->vminor;

	ogon__pbrpc__rpcbase__free_unpacked(message, NULL);
	context->isConnected = TRUE;
	return TRUE;

error_out:
	context->isConnected = FALSE;
	return FALSE;
}

static BOOL pbrpc_reconnect(pbRPCContext* context)
{
	pbRPCTransaction *ta;

	context->isConnected = FALSE;
	context->transport->close(context->transport);
	Queue_Clear(context->writeQueue);

	while ((ta = ListDictionary_Remove_Head(context->transactions))) {
		ta->responseCallback(PBRCP_TRANSPORT_ERROR, 0, ta->callbackArg);
		free(ta);
	}

	while (context->runLoop && !pbrpc_connect(context, 2 * 1000)) {
		Sleep(1 * 1000UL);
		WLog_DBG(TAG, "retrying connection...");
	}

	return context->runLoop;
}

static void pbrpc_mainloop(pbRPCContext* context)
{
	int status;
	DWORD ret;
	DWORD nCount;
	HANDLE events[32];
	HANDLE thandle;
	BOOL reconnect, firstLoop;

#ifndef _WIN32
	sigset_t set;
	sigfillset(&set);
	status = pthread_sigmask(SIG_BLOCK, &set, NULL);
	if (0 != status)
		WLog_ERR(TAG, "error setting signal mask (%s)", strerror(errno));
#endif

	reconnect = TRUE;
	firstLoop = TRUE;

	while (context->runLoop) {
		if (reconnect) {
			if (!firstLoop) {
				if (context->disconnectedCb)
					context->disconnectedCb();

				context->remoteVersionMajor = 0;
				context->remoteVersionMinor = 0;
			}

			firstLoop = FALSE;
			if (!pbrpc_reconnect(context))
				continue;
			reconnect = FALSE;
		}

		if (!(thandle = context->transport->get_fds(context->transport))) {
			WLog_ERR(TAG, "fatal error, failed to get transport fds");
			reconnect = TRUE;
			continue;
		}

		nCount = 0;
		events[nCount++] = context->stopEvent;
		events[nCount++] = Queue_Event(context->writeQueue);
		events[nCount++] = thandle;

		ret = WaitForMultipleObjects(nCount, events, FALSE, 2 * 1000);
		if ((ret == WAIT_FAILED) || (ret == WAIT_TIMEOUT)) {
			continue;
		}

		if (WaitForSingleObject(context->stopEvent, 0) == WAIT_OBJECT_0) {
			context->runLoop = FALSE;
			continue;
		}

		if (WaitForSingleObject(thandle, 0) == WAIT_OBJECT_0) {
			status = pbrpc_process_message_in(context);
			if (status < 0) {
				WLog_ERR(TAG, "transport problem, trying to reconnect ...");
				reconnect = TRUE;
				continue;
			}
		}

		if (WaitForSingleObject(Queue_Event(context->writeQueue), 0) == WAIT_OBJECT_0) {
			Ogon__Pbrpc__RPCBase* msg = NULL;

			while((msg = Queue_Dequeue(context->writeQueue)) && context->runLoop) {
				status = pbrpc_process_message_out(context, msg);
				pbrpc_message_free(msg, TRUE);

				if (status < 0)	{
					WLog_ERR(TAG, "transport problem, reconnecting ...");
					reconnect = TRUE;
					break;
				}
			}
		}
	}
}

int pbrpc_server_start(pbRPCContext* context, UINT32 vmajor, UINT32 vminor)
{
	context->localVersionMajor = vmajor;
	context->localVersionMinor = vminor;
	context->isConnected = FALSE;
	context->runLoop = TRUE;
	context->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)pbrpc_mainloop, context, 0, NULL);
	return context->thread ? 0 : -1;
}

int pbrpc_server_stop(pbRPCContext* context)
{
	context->isConnected = FALSE;
	context->runLoop = FALSE;
	SetEvent(context->stopEvent);
	WaitForSingleObject(context->thread, INFINITE);
	context->transport->close(context->transport);
	return 0;
}

/** @brief contextual data to handle a local call */
struct pbrpc_local_call_context {
	HANDLE event;
	Ogon__Pbrpc__RPCBase *response;
	PBRPCSTATUS status;
};

static void pbrpc_response_local_cb(PBRPCSTATUS reason, Ogon__Pbrpc__RPCBase* response, void *args) {
	struct pbrpc_local_call_context *context = (struct pbrpc_local_call_context *)args;
	context->response = response;
	context->status = reason;
	SetEvent(context->event);
}


int pbrpc_call_method(pbRPCContext* context, UINT32 type, pbRPCPayload* request, pbRPCPayload** response)
{
	Ogon__Pbrpc__RPCBase* message;
	pbRPCTransaction* ta;
	UINT32 ret;
	UINT32 tag;
	DWORD wait_ret;

	struct pbrpc_local_call_context local_context;

	if (!context->isConnected) {
		return PBRCP_TRANSPORT_ERROR;
	}

	if (!(message = pbrpc_message_new())) {
		WLog_ERR(TAG, "error creating new message");
		return PBRPC_FAILED;
	}
	pbrpc_prepare_request(context, message);
	message->payload.data = (unsigned char*)request->data;
	message->payload.len = request->dataLen;
	message->has_payload = 1;
	message->msgtype = type;
	tag = message->tag;

	ZeroMemory(&local_context, sizeof(local_context));

	if (!(local_context.event = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		WLog_ERR(TAG, "error creating event");
		goto fail_create_event;
	}
	local_context.status = PBRCP_CALL_TIMEOUT;

	if (!(ta = pbrpc_transaction_new())) {
		WLog_ERR(TAG, "error creating new transaction");
		goto fail_transaction_new;
	}
	ta->responseCallback = pbrpc_response_local_cb;
	ta->callbackArg = &local_context;

	if (!ListDictionary_Add(context->transactions, (void*)((UINT_PTR)(message->tag)), ta)) {
		WLog_ERR(TAG, "error adding transaction to dictionary");
		goto fail_dictionary_add;
	}

	if (!Queue_Enqueue(context->writeQueue, message)) {
		WLog_ERR(TAG, "error enqueuing message in write queue");
		goto fail_queue_enqueue;
	}

	wait_ret = WaitForSingleObject(local_context.event, PBRPC_TIMEOUT);
	if (wait_ret != WAIT_OBJECT_0) {
		pbRPCTransaction *fa;
		if(!(fa = ListDictionary_Remove(context->transactions, (void*)((UINT_PTR)(tag))))) {
			/**
			 * timeout occurred but request is being processed by the pbrpc thread,
			 * wait for the event to be notified so that we can safely close the
			 * Event handle (the pbRPCTransaction fa will be freed by the pbrpc thread).
			 */
			WaitForSingleObject(local_context.event, INFINITE);
		} else {
			free(fa);
		}
	}

	CloseHandle(local_context.event);
	message = local_context.response;

	if (!message) {
		if (local_context.status) {
			ret = local_context.status;
		} else {
			ret = PBRPC_FAILED;
		}
	}
	else {
		*response = pbrpc_fill_payload(message);
		if (!(*response)) {
			ret = PBRPC_FAILED;
			pbrpc_message_free(message, TRUE);
		} else {
			ret = message->status;
			pbrpc_message_free(message, FALSE);
		}
	}

	return ret;

fail_queue_enqueue:
	ListDictionary_Remove(context->transactions, (void*)((UINT_PTR)(message->tag)));
fail_dictionary_add:
	free(ta);
fail_transaction_new:
	CloseHandle(local_context.event);
fail_create_event:
	pbrpc_message_free(message, FALSE);

	return PBRPC_FAILED;
}

void pbrpc_register_methods(pbRPCContext* context, pbRPCMethod *methods)
{
	context->methods = methods;
}

void pbrcp_call_method_async(pbRPCContext* context, UINT32 type, pbRPCPayload* request,
		pbRpcResponseCallback callback, void *callback_args)
{
	Ogon__Pbrpc__RPCBase* message;
	pbRPCTransaction *ta;
	void *payloadData = NULL;
	size_t payloadLength = 0;
	UINT32 err;

	if (!context->isConnected) {
		err = PBRCP_TRANSPORT_ERROR;
		goto fail;
	}

	if (request->data && request->dataLen) {
		payloadLength = request->dataLen;
		if (!(payloadData = malloc(payloadLength))) {
			WLog_ERR(TAG, "failed to allocate payload data");
			err = PBRCP_OUTOFMEMORY;
			goto fail;
		}
		memcpy(payloadData, request->data, payloadLength);
	}

	if (!(ta = pbrpc_transaction_new())) {
		WLog_ERR(TAG, "failed to create new transaction");
		err = PBRCP_OUTOFMEMORY;
		goto fail_transaction_new;
	}

	ta->responseCallback = callback;
	ta->callbackArg = callback_args;

	if (!(message = pbrpc_message_new())) {
		WLog_ERR(TAG, "failed to create new message");
		err = PBRCP_OUTOFMEMORY;
		goto fail_message_new;
	}

	pbrpc_prepare_request(context, message);

	message->payload.data = (uint8_t*)payloadData;
	message->payload.len = payloadLength;
	message->has_payload = 1;
	message->msgtype = type;

	if (!ListDictionary_Add(context->transactions, (void*)((UINT_PTR)(message->tag)), ta)) {
		WLog_ERR(TAG, "error adding transaction to dictionary");
		err = PBRCP_OUTOFMEMORY;
		goto fail_add_transaction;
	}

	if (!Queue_Enqueue(context->writeQueue, message)) {
		WLog_ERR(TAG, "error enqueuing message in write queue");
		err = PBRCP_OUTOFMEMORY;
		goto fail_queue_enqueue;
	}
	return;

fail_queue_enqueue:
	ListDictionary_Remove(context->transactions, (void*)((UINT_PTR)(message->tag)));
fail_add_transaction:
	pbrpc_message_free(message, FALSE);
fail_message_new:
	free(ta);
fail_transaction_new:
	free(payloadData);
fail:
	callback(err, 0, callback_args);
	return;
}
