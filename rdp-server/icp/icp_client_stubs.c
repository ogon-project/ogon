/**
 * ogon - Free Remote Desktop Services
 * Internal Communication Protocol (ICP)
 * Client Stubs
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

#include "../../common/global.h"
#include "../../common/icp.h"
#include "../ogon.h"
#include "../backend.h"
#include "../channels.h"

#include "ICP.pb-c.h"
#include "pbrpc_utils.h"
#include "icp_client_stubs.h"

#define TAG OGON_TAG("icp.client")

#define ICP_CLIENT_STUB_SETUP(camel, expanded) \
	UINT32 type = OGON__ICP__MSGTYPE__##camel ; \
	pbRPCPayload pbrequest; \
	pbRPCPayload *pbresponse = NULL; \
	size_t ret; \
	Ogon__Icp__##camel ## Request request; \
	Ogon__Icp__##camel ## Response *response = NULL; \
	pbRPCContext* context = (pbRPCContext*) ogon_icp_get_context(); \
	if (!context) \
		return PBRPC_FAILED; \
	ogon__icp__ ##expanded ##_request__init(&request);

#define ICP_CLIENT_STUB_SETUP_ASYNC(camel, expanded) \
	UINT32 type = OGON__ICP__MSGTYPE__##camel ; \
	pbRPCPayload pbrequest; \
	size_t ret; \
	Ogon__Icp__##camel ## Request request; \
	pbRPCContext* context = (pbRPCContext*) ogon_icp_get_context(); \
	if (!context) \
		return PBRPC_FAILED; \
	ogon__icp__ ##expanded ##_request__init(&request);


#define ICP_CLIENT_STUB_CALL(camel, expanded) \
	pbrequest.dataLen = ogon__icp__##expanded ##_request__get_packed_size(&request); \
	if (!(pbrequest.data = malloc(pbrequest.dataLen))) \
		return PBRPC_FAILED; \
	ret = ogon__icp__##expanded ##_request__pack(&request, (uint8_t*) pbrequest.data); \
	if (ret == pbrequest.dataLen) \
	{ \
		ret = pbrpc_call_method(context, type, &pbrequest, &pbresponse); \
	} \
	else \
	{ \
		ret = PBRPC_BAD_REQEST_DATA; \
	}

void dummyCallback(UINT32 reason, Ogon__Pbrpc__RPCBase* response, void *args) {
	OGON_UNUSED(reason);
	OGON_UNUSED(args);
	pbrpc_message_free(response, TRUE);
}


#define ICP_CLIENT_STUB_CALL_ASYNC(camel, expanded) \
	pbrequest.dataLen = ogon__icp__##expanded ##_request__get_packed_size(&request); \
	if (!(pbrequest.data = malloc(pbrequest.dataLen))) \
		return PBRPC_FAILED; \
	ret = ogon__icp__##expanded ##_request__pack(&request, (uint8_t*) pbrequest.data); \
	if (ret == pbrequest.dataLen) \
	{ \
		pbrcp_call_method_async(context, type, &pbrequest, dummyCallback, (void *)NULL); \
	} \
	free(pbrequest.data);

#define ICP_CLIENT_STUB_UNPACK_RESPONSE(camel, expanded) \
	response = ogon__icp__##expanded ##_response__unpack(NULL, pbresponse->dataLen, (uint8_t*) pbresponse->data); \
	pbrpc_free_payload(pbresponse);

#define ICP_CLIENT_STUB_CLEANUP(camel, expanded) \
	if (response) \
		ogon__icp__##expanded ##_response__free_unpacked(response, NULL);

#define ICP_CLIENT_SEND_PREPARE(camel, expanded) \
			Ogon__Icp__##camel ##Response response; \
			ogon__icp__##expanded ##_response__init(&response)

#define ICP_CLIENT_SEND_PACK(camel, expanded) \
			if (status == 0) {\
				pbresponse->dataLen = ogon__icp__##expanded ##_response__get_packed_size(&response); \
				if (!(pbresponse->data = malloc(pbresponse->dataLen))) { \
					return -1; \
				} \
				ret = ogon__icp__##expanded ##_response__pack(&response, (uint8_t*) pbresponse->data); \
			} else { \
				pbresponse->dataLen = 0; \
				pbresponse->data = NULL; \
				ret = 0; \
			}

int ogon_icp_sendResponse(UINT32 tag, UINT32 type, UINT32 status, BOOL success, void *responseparam1)
{
	int rtype = 0;
	pbRPCPayload *pbresponse = pbrpc_payload_new();
	if (!pbresponse) {
		return -1;
	}
	pbRPCContext* context = (pbRPCContext*) ogon_icp_get_context();
	size_t ret = 0;

	if (!context)
		return -1;

	switch (type) {
		case NOTIFY_SWITCHTO: {
			rtype = OGON__ICP__MSGTYPE__SwitchTo;
			ICP_CLIENT_SEND_PREPARE(SwitchTo, switch_to);
			response.success = success;
			ICP_CLIENT_SEND_PACK(SwitchTo, switch_to);
			break;
		}
		case NOTIFY_LOGOFF:	{
			rtype = OGON__ICP__MSGTYPE__LogoffUserSession;
			ICP_CLIENT_SEND_PREPARE(LogoffUserSession, logoff_user_session);
			response.loggedoff = success;
			ICP_CLIENT_SEND_PACK(LogoffUserSession, logoff_user_session);
			break;
		}
		case NOTIFY_VC_CONNECT:	{
			rtype = OGON__ICP__MSGTYPE__OtsApiVirtualChannelOpen;
			ICP_CLIENT_SEND_PREPARE(OtsApiVirtualChannelOpen, ots_api_virtual_channel_open);
			registered_virtual_channel *channel = (registered_virtual_channel *)responseparam1;
			if (channel) {
				response.connectionstring = channel->pipe_name;
				response.instance = channel->channel_instance + 1;
			} else {
				response.connectionstring = "";
				response.instance = 0;
			}
			ICP_CLIENT_SEND_PACK(OtsApiVirtualChannelOpen, ots_api_virtual_channel_open);
			break;
		}
		case NOTIFY_VC_DISCONNECT: {
			rtype = OGON__ICP__MSGTYPE__OtsApiVirtualChannelClose;
			ICP_CLIENT_SEND_PREPARE(OtsApiVirtualChannelClose, ots_api_virtual_channel_close);
			response.success = success;
			ICP_CLIENT_SEND_PACK(OtsApiVirtualChannelClose, ots_api_virtual_channel_close);
			break;
		}
		case NOTIFY_START_REMOTE_CONTROL: {
			rtype =  OGON__ICP__MSGTYPE__OtsApiStartRemoteControl;
			ICP_CLIENT_SEND_PREPARE(OtsApiStartRemoteControl, ots_api_start_remote_control);
			response.success = success;
			ICP_CLIENT_SEND_PACK(OtsApiStartRemoteControl, ots_api_start_remote_control);
			break;
		}
		case NOTIFY_STOP_SHADOWING: {
			rtype =  OGON__ICP__MSGTYPE__OtsApiStopRemoteControl;
			ICP_CLIENT_SEND_PREPARE(OtsApiStopRemoteControl, ots_api_stop_remote_control);
			response.success = success;
			ICP_CLIENT_SEND_PACK(OtsApiStartRemoteControl, ots_api_stop_remote_control);
			break;
		}
		case NOTIFY_USER_MESSAGE: {
			rtype = OGON__ICP__MSGTYPE__Message;
			ICP_CLIENT_SEND_PREPARE(Message, message);
			response.result = (UINT32)(intptr_t)responseparam1;
			ICP_CLIENT_SEND_PACK(Message, message);
			break;
		}

		default:
			/* type not found */
			WLog_ERR(TAG, "don't know how to handle type %"PRIu32"", type);
			return -1;
	}

	if (ret != pbresponse->dataLen)
	{
		WLog_ERR(TAG, "pack error for %"PRIu32"", type);
		pbrpc_free_payload(pbresponse);
		return -1;
	}

	ret = pbrpc_respond_method(context, pbresponse, status, rtype, tag);
	if (ret != 0) {
		pbrpc_free_payload(pbresponse);
	}
	return ret;
}

int ogon_icp_Ping(BOOL* pong)
{
	int retValue = PBRPC_SUCCESS;
	ICP_CLIENT_STUB_SETUP(Ping, ping)

	ICP_CLIENT_STUB_CALL(Ping, ping)

	if (ret != 0)
	{
		retValue = ret;
		goto out_cleanup;
	}

	ICP_CLIENT_STUB_UNPACK_RESPONSE(Ping, ping)

	if (NULL == response)
	{
		// unpack error
		retValue = PBRPC_BAD_RESPONSE;
		goto out_cleanup;
	}

	*pong = response->pong;

out_cleanup:
	ICP_CLIENT_STUB_CLEANUP(Ping, ping)
	return retValue;
}

int ogon_icp_DisconnectUserSession(UINT32 connectionId, BOOL* disconnected)
{
	ICP_CLIENT_STUB_SETUP(DisconnectUserSession, disconnect_user_session)

	request.connectionid = connectionId;

	ICP_CLIENT_STUB_CALL(DisconnectUserSession, disconnect_user_session)

	if (ret != 0)
	{
		return ret;
	}

	ICP_CLIENT_STUB_UNPACK_RESPONSE(DisconnectUserSession, disconnect_user_session)

	if (NULL == response)
	{
		return PBRPC_BAD_RESPONSE;
	}

	*disconnected = response->disconnected;

	ICP_CLIENT_STUB_CLEANUP(DisconnectUserSession, disconnect_user_session)

	return PBRPC_SUCCESS;
}

int ogon_icp_DisconnectUserSession_async(UINT32 connectionId)
{
	ICP_CLIENT_STUB_SETUP_ASYNC(DisconnectUserSession, disconnect_user_session)

	request.connectionid = connectionId;

	ICP_CLIENT_STUB_CALL_ASYNC(DisconnectUserSession, disconnect_user_session)

	return ret;
}

int ogon_icp_LogonUser(UINT32 connectionId, const char* username, const char* domain,
		const char* password, const char* clientHostName,
		const char* clientAddress, UINT32 clientBuild, UINT16 clientProductId, UINT32 hardwareID, UINT16 protocol,
		UINT32 width, UINT32 height, UINT32 bbp, ogon_backend_props *props,
		UINT32* maxWidth, UINT32* maxHeight)
{
	ICP_CLIENT_STUB_SETUP(LogonUser, logon_user)

	request.connectionid = connectionId;
	request.width = width;
	request.height = height;
	request.colordepth = bbp;
	request.clientbuildnumber= clientBuild;
	request.clientproductid =clientProductId;
	request.clienthardwareid = hardwareID;
	request.clientprotocoltype = protocol;
#if __clang__ || (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif // __clang__ || (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
	request.username = (char *)username;
	request.password = (char *)password;
	request.domain = (char *)domain;
	request.clienthostname = (char *)clientHostName;
	request.clientaddress = (char *)clientAddress;
#if __clang__ || (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic pop
#endif //  __clang__ || (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))

	ICP_CLIENT_STUB_CALL(LogonUser, logon_user)
	if (ret != 0) {
		return ret;
	}

	ICP_CLIENT_STUB_UNPACK_RESPONSE(LogonUser, logon_user)

	if (NULL == response) {
		return PBRPC_BAD_RESPONSE;
	}

	props->serviceEndpoint = _strdup(response->serviceendpoint);
	props->backendCookie = _strdup(response->backendcookie);
	props->ogonCookie = _strdup(response->ogoncookie);
	*maxWidth = response->maxwidth;
	*maxHeight = response->maxheight;

	ICP_CLIENT_STUB_CLEANUP(LogonUser, logon_user)

	if (!props->serviceEndpoint || !props->backendCookie || !props->ogonCookie) {
		free(props->serviceEndpoint);
		free(props->backendCookie);
		free(props->ogonCookie);
		return -1;
	}

	return PBRPC_SUCCESS;
}

int ogon_icp_get_property_bool(UINT32 connectionId,char * path,BOOL * value)
{
	if (value == NULL)
		return -1;

	ICP_CLIENT_STUB_SETUP(PropertyBool, property_bool)

	request.connectionid = connectionId;
	request.path = path;

	ICP_CLIENT_STUB_CALL(PropertyBool, property_bool)

	if (ret != 0)
	{
		return ret;
	}

	ICP_CLIENT_STUB_UNPACK_RESPONSE(PropertyBool, property_bool)

	if (NULL == response)
	{
		return PBRPC_BAD_RESPONSE;
	}

	if (!response->success) {
		ICP_CLIENT_STUB_CLEANUP(PropertyBool, property_bool)
		return -1;
	}

	*value = response->value;

	ICP_CLIENT_STUB_CLEANUP(PropertyBool, property_bool)
	return PBRPC_SUCCESS;
}

int ogon_icp_get_property_number(UINT32 connectionId,char * path,INT32 * value)
{
	if (value == NULL)
		return -1;

	ICP_CLIENT_STUB_SETUP(PropertyNumber, property_number)

	request.connectionid = connectionId;
	request.path = path;

	ICP_CLIENT_STUB_CALL(PropertyNumber, property_number)

	if (ret != 0)
	{
		return ret;
	}

	ICP_CLIENT_STUB_UNPACK_RESPONSE(PropertyNumber, property_number)

	if (NULL == response)
	{
		return PBRPC_BAD_RESPONSE;
	}

	if (!response->success) {
		ICP_CLIENT_STUB_CLEANUP(PropertyNumber, property_number)
		return -1;
	}

	*value = response->value;

	ICP_CLIENT_STUB_CLEANUP(PropertyNumber, property_number)
	return PBRPC_SUCCESS;
}

int ogon_icp_get_property_string(UINT32 connectionId, char *path, char** value)
{
	if (value == NULL)
		return -1;

	ICP_CLIENT_STUB_SETUP(PropertyString, property_string)

	request.connectionid = connectionId;
	request.path = path;

	ICP_CLIENT_STUB_CALL(PropertyString, property_string)

	if (ret != 0)
	{
		return ret;
	}

	ICP_CLIENT_STUB_UNPACK_RESPONSE(PropertyString, property_string)

	if (NULL == response)
	{
		return PBRPC_BAD_RESPONSE;
	}

	if (!response->success) {
		ICP_CLIENT_STUB_CLEANUP(PropertyString, property_string)
		return -1;
	}

	*value = _strdup(response->value);

	ICP_CLIENT_STUB_CLEANUP(PropertyString, property_string)
	if (!*value){
		return -1;
	}
	return PBRPC_SUCCESS;
}

int ogon_icp_RemoteControlEnded(UINT32 spyId, UINT32 spiedId)
{
	int retVal = -1;
	ICP_CLIENT_STUB_SETUP(RemoteControlEnded, remote_control_ended)

	request.spyid = spyId;
	request.spiedid = spiedId;

	ICP_CLIENT_STUB_CALL(RemoteControlEnded, remote_control_ended)

	if (ret != 0) {
		return ret;
	}

	ICP_CLIENT_STUB_UNPACK_RESPONSE(RemoteControlEnded, remote_control_ended)

	if (!response) {
		return PBRPC_BAD_RESPONSE;
	}

	retVal = response->success ? PBRPC_SUCCESS : PBRPC_FAILED;
	ICP_CLIENT_STUB_CLEANUP(RemoteControlEnded, remote_control_ended)
	return retVal;
}
