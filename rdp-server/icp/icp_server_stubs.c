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

#include "../../common/global.h"
#include "../app_context.h"
#include "../peer.h"

#include "ICP.pb-c.h"
#include "pbRPC.pb-c.h"
#include "icp_server_stubs.h"
#include "pbrpc_utils.h"

#define TAG OGON_TAG("icp.server")

#define ICP_SERVER_STUB_SETUP(camel, expanded) \
	Ogon__Icp__##camel ##Request *request; \
	Ogon__Icp__##camel ##Response response; \
	pbRPCPayload *payload; \
	size_t ret = 0; \
	ogon__icp__##expanded ##_response__init(&response); \
	request = ogon__icp__##expanded ##_request__unpack(NULL, pbrequest->dataLen, (uint8_t*)pbrequest->data);\
	if (!request) \
	{ \
		return PBRPC_BAD_REQUEST_DATA; \
	}

#define ICP_SERVER_STUB_RESPOND(camel, expanded) \
	ogon__icp__##expanded ##_request__free_unpacked(request, NULL); \
	payload = pbrpc_payload_new(); \
	if (!payload) { \
		return PBRPC_BAD_RESPONSE; \
	} \
	payload->dataLen = ogon__icp__##expanded ##_response__get_packed_size(&response); \
	payload->data = malloc(payload->dataLen); \
	if (!payload->data) { \
		free(payload); \
		return PBRPC_BAD_RESPONSE; \
	} \
	ret = ogon__icp__##expanded ##_response__pack(&response, (uint8_t*) payload->data); \
	if (ret != payload->dataLen) \
	{ \
		free(payload->data); \
		return PBRPC_BAD_RESPONSE; \
	} \
	*pbresponse = payload;

int ping(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse)
{
	OGON_UNUSED(tag);
	ICP_SERVER_STUB_SETUP(Ping, ping)

	// call functions with parameters from request and set answer to response
	response.pong = TRUE;

	ICP_SERVER_STUB_RESPOND(Ping, ping)

	// freeup response data if necessary

	return PBRPC_SUCCESS;
}

int switchTo(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse)
{
	ogon_backend_props *props = NULL;
	ICP_SERVER_STUB_SETUP(SwitchTo, switch_to)
	struct ogon_notification_switch *msg = calloc(1, sizeof(struct ogon_notification_switch));
	if (!msg) {
		goto out_fail;
	}
	props = &msg->props;
	msg->tag = tag;
	msg->maxWidth = request->maxwidth;
	msg->maxHeight = request->maxheight;
	props->serviceEndpoint = _strdup(request->serviceendpoint);
	props->backendCookie = _strdup(request->backendcookie);
	props->ogonCookie = _strdup(request->ogoncookie);
	if (!props->serviceEndpoint || !props->backendCookie ||!props->ogonCookie) {
		goto out_fail;
	}

	if (app_context_post_message_connection(request->connectionid, NOTIFY_SWITCHTO, (void *)msg, NULL)) {
		ogon__icp__switch_to_request__free_unpacked(request, NULL);
		*pbresponse = NULL;
		return 0;
	}
	WLog_ERR(TAG, "switchto error: no connection for %"PRIu32"", request->connectionid);

out_fail:
	if (msg) {
		ogon_backend_props_free(props);
		free(msg);
	}
	response.success = FALSE;
	ICP_SERVER_STUB_RESPOND(SwitchTo, switch_to)
	return PBRPC_SUCCESS;
}

int logoffUserSession(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse)
{
	ICP_SERVER_STUB_SETUP(LogoffUserSession, logoff_user_session)
	struct ogon_notification_logoff *msg = malloc(sizeof(struct ogon_notification_logoff));
	if (!msg) {
		goto out_fail;
	}
	msg->tag = tag;
	if (app_context_post_message_connection(request->connectionid, NOTIFY_LOGOFF, (void*) msg, NULL))
	{
		ogon__icp__logoff_user_session_request__free_unpacked(request, NULL);
		// response is sent after processing the notification
		*pbresponse = NULL;
		return 0;
	}
	WLog_ERR(TAG, "logoff user session error: no connection for %"PRIu32"", request->connectionid);
	free(msg);

out_fail:
	response.loggedoff = FALSE;

	ICP_SERVER_STUB_RESPOND(LogoffUserSession, logoff_user_session)
	return PBRPC_SUCCESS;
}

int otsapiVirtualChannelOpen(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse)
{
	ICP_SERVER_STUB_SETUP(OtsApiVirtualChannelOpen, ots_api_virtual_channel_open)
	struct ogon_notification_vc_connect *msg = calloc(1, sizeof(struct ogon_notification_vc_connect));
	if (!msg) {
		goto out_fail;
	}
	msg->tag = tag;
	msg->isDynamic = request->dynamicchannel;
	msg->flags = request->flags;
	msg->vcname = _strdup(request->virtualname);
	if (!msg->vcname) {
		goto out_fail;
	}
	if (app_context_post_message_connection(request->connectionid, NOTIFY_VC_CONNECT, (void*) msg, NULL))
	{
		ogon__icp__ots_api_virtual_channel_open_request__free_unpacked(request, NULL);
		// response is sent after processing the notification
		*pbresponse = NULL;
		return 0;
	}
	WLog_ERR(TAG, "virtual channel open error: no connection for %"PRIu32"", request->connectionid);

out_fail:
	if (msg) {
		free(msg->vcname);
		free(msg);
	}
	response.connectionstring = "";
	response.instance = 0;

	ICP_SERVER_STUB_RESPOND(OtsApiVirtualChannelOpen, ots_api_virtual_channel_open)
	return PBRPC_SUCCESS;
}

int otsapiVirtualChannelClose(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload** pbresponse)
{
	ICP_SERVER_STUB_SETUP(OtsApiVirtualChannelClose, ots_api_virtual_channel_close)
	struct ogon_notification_vc_disconnect *msg = calloc(1, sizeof(struct ogon_notification_vc_disconnect));
	if (!msg) {
		goto out_fail;
	}
	msg->tag = tag;
	msg->instance = request->instance;
	msg->vcname = _strdup(request->virtualname);
	if (!msg->vcname) {
		goto out_fail;
	}
	if (app_context_post_message_connection(request->connectionid, NOTIFY_VC_DISCONNECT, (void*) msg, NULL))
	{
		ogon__icp__ots_api_virtual_channel_close_request__free_unpacked(request, NULL);
		// response is sent after processing the notification
		*pbresponse = NULL;
		return 0;
	}
	WLog_ERR(TAG, "virtual channel close error: no connection for %"PRIu32"", request->connectionid);

out_fail:
	if (msg) {
		free(msg->vcname);
		free(msg);
	}
	response.success = FALSE;

	ICP_SERVER_STUB_RESPOND(OtsApiVirtualChannelClose, ots_api_virtual_channel_close)
	return PBRPC_SUCCESS;
}

int otsapiStartRemoteControl(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload **pbresponse)
{
	ICP_SERVER_STUB_SETUP(OtsApiStartRemoteControl, ots_api_start_remote_control)
	struct ogon_notification_start_remote_control *msg = malloc(sizeof(struct ogon_notification_start_remote_control));
	if (!msg ) {
		goto out_fail;
	}
	msg->tag = tag;
	msg->connectionId = request->connectionid;
	msg->targetId = request->targetconnectionid;
	msg->hotKeyVk = request->hotkeyvk;
	msg->hotKeyModifier = request->hotkeymodifiers;
	msg->flags = request->flags;
	if (app_context_post_message_connection(request->connectionid, NOTIFY_START_REMOTE_CONTROL, (void*) msg, NULL))
	{
		ogon__icp__ots_api_start_remote_control_request__free_unpacked(request, NULL);
		// response is sent after processing the notification
		*pbresponse = NULL;
		return 0;
	}
	else
	{
		free(msg);
		WLog_ERR(TAG, "start remote control error: no connection for %"PRIu32"", request->connectionid);
	}
out_fail:
	response.success = FALSE;

	ICP_SERVER_STUB_RESPOND(OtsApiStartRemoteControl, ots_api_start_remote_control)
	return PBRPC_SUCCESS;
}

int otsapiStopRemoteControl(LONG tag, pbRPCPayload* pbrequest, pbRPCPayload **pbresponse)
{
	OGON_UNUSED(tag);

	ICP_SERVER_STUB_SETUP(OtsApiStopRemoteControl, ots_api_stop_remote_control)

	struct rds_notification_stop_remote_control *msg = malloc(sizeof(struct rds_notification_stop_remote_control));
	if (!msg) {
		goto out_fail;
	}
	msg->tag = tag;

	if (app_context_post_message_connection(request->connectionid, NOTIFY_STOP_SHADOWING, (void*) msg, NULL)) {
		ogon__icp__ots_api_stop_remote_control_request__free_unpacked(request, NULL);
		// response is sent after processing the notification
		*pbresponse = NULL;
		return 0;
	} else {
		WLog_ERR(TAG, "stop remote control error: no connection for %"PRIu32"", request->connectionid);
		free(msg);
	}
out_fail:
	response.success = FALSE;

	ICP_SERVER_STUB_RESPOND(OtsApiStopRemoteControl, ots_api_stop_remote_control)
	return PBRPC_SUCCESS;
}

int message(LONG tag, pbRPCPayload *pbrequest, pbRPCPayload **pbresponse) {

	ICP_SERVER_STUB_SETUP(Message, message)

	struct ogon_notification_msg_message *msg = calloc(1, sizeof(struct ogon_notification_msg_message));
	if (!msg) {
		goto out_fail;
	}

	msg->tag = tag;
	msg->type = request->type;
	msg->style = request->style;
	msg->timeout = request->timeout;

	msg->parameter_num = request->parameternum;

	if (msg->parameter_num > 0 ) {
		if (!(msg->parameter1 = _strdup(request->parameter1))) {
			goto out_fail;
		}
	}
	if (msg->parameter_num > 1 ) {
		if (!(msg->parameter2 = _strdup(request->parameter2))) {
			goto out_fail;
		}
	}
	if (msg->parameter_num > 2 ) {
		if (!(msg->parameter3 = _strdup(request->parameter3))) {
			goto out_fail;
		}
	}
	if (msg->parameter_num > 3 ) {
		if (!(msg->parameter4 = _strdup(request->parameter4))) {
			goto out_fail;
		}
	}
	if (msg->parameter_num > 4 ) {
		if (!(msg->parameter5 = _strdup(request->parameter5))) {
			goto out_fail;
		}
	}

	if (app_context_post_message_connection(request->connectionid, NOTIFY_USER_MESSAGE, (void*)msg, NULL)) {
		ogon__icp__message_request__free_unpacked(request, NULL);
		// response is sent after processing the notification
		*pbresponse = NULL;
		return 0;
	} else {
		WLog_ERR(TAG, "message: no connection for %"PRIu32"", request->connectionid);
	}

out_fail:
	if (msg) {
		free(msg->parameter1);
		free(msg->parameter2);
		free(msg->parameter3);
		free(msg->parameter4);
		free(msg->parameter5);
		free(msg);
	}
	response.result = 0xFFFF;

	ICP_SERVER_STUB_RESPOND(Message, message)
	return PBRPC_SUCCESS;
}
