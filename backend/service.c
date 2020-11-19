/**
 * ogon - Free Remote Desktop Services
 * Backend Library
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
 * Norbert Federa <norbert.federa@thincast.com>
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

#include <ogon/backend.h>
#include <ogon/service.h>
#include <ogon/version.h>
#include <winpr/stream.h>
#include <winpr/synch.h>
#include <winpr/file.h>
#include <winpr/pipe.h>

#include "../common/security.h"
#include "../common/global.h"
#include "protocol.h"

#define TAG OGON_TAG("backend.service")

typedef int (*pfn_ogon_service_accept)(ogon_backend_service *service, HANDLE remotePipe);
typedef int (*pfn_ogon_service_treat_input_bytes)(ogon_backend_service *service);

/** @brief */
struct _ogon_backend_service {
	DWORD sessionId;
	char *endPoint;

	HANDLE serverPipe;
	HANDLE remotePipe;

	wStream *inStream;
	wStream *outStream;
	BOOL waitingHeaders;
	UINT32 expectedBytes;
	UINT16 messageType;
	ogon_message clientMessage;

	ogon_client_interface client;
};

ogon_backend_service* ogon_service_new(DWORD sessionId, const char *endPoint) {
	ogon_backend_service *ret;

	if (!(ret = calloc(1, sizeof(ogon_backend_service)))) {
		return NULL;
	}

	ret->expectedBytes = RDS_ORDER_HEADER_LENGTH;
	ret->waitingHeaders = TRUE;
	ret->remotePipe = INVALID_HANDLE_VALUE;
	ret->sessionId = sessionId;
	ret->endPoint = _strdup(endPoint);
	if (!ret->endPoint) {
		goto out_free;
	}

	if (!(ret->inStream = Stream_New(NULL, 8192))) {
		goto out_endPoint;
	}

	if (!(ret->outStream = Stream_New(NULL, 8192))) {
		goto out_inStream;
	}

	return ret;

out_inStream:
	Stream_Free(ret->inStream, TRUE);
out_endPoint:
	free(ret->endPoint);
out_free:
	free(ret);
	WLog_ERR(TAG, "error creating rdsService");
	return NULL;
}


int ogon_service_server_fd(ogon_backend_service *service) {
	if (!service->serverPipe || service->serverPipe == INVALID_HANDLE_VALUE) {
		return -1;
	}
	return GetEventFileDescriptor(service->serverPipe);
}

void ogon_service_set_callbacks(ogon_backend_service *service, ogon_client_interface *cbs) {
	service->client = *cbs;
}

int ogon_service_client_fd(ogon_backend_service *service) {
	if (!service->remotePipe || service->remotePipe == INVALID_HANDLE_VALUE) {
		return -1;
	}
	return GetEventFileDescriptor(service->remotePipe);
}

ogon_incoming_bytes_result ogon_service_incoming_bytes(ogon_backend_service *service, void *cb_data) {
	DWORD readBytes;
	ogon_message *msg;
	ogon_client_interface *client;
	ogon_msg_version msgVersion;

	BOOL success = TRUE;

	if (!ReadFile(service->remotePipe, Stream_Pointer(service->inStream),
		service->expectedBytes, &readBytes, NULL) || !readBytes)
	{
		if (GetLastError() == ERROR_NO_DATA) {
			return OGON_INCOMING_BYTES_WANT_MORE_DATA;
		}
		return OGON_INCOMING_BYTES_BROKEN_PIPE;
	}

	Stream_Seek(service->inStream, readBytes);
	service->expectedBytes -= readBytes;
	if (service->expectedBytes) {
		return OGON_INCOMING_BYTES_OK;
	}


	if (service->waitingHeaders) {
		UINT32 len;

		Stream_SetPosition(service->inStream, 0);
		ogon_read_message_header(service->inStream, &service->messageType, &len);
		service->expectedBytes = len;

		Stream_SetPosition(service->inStream, 0);

		if (service->expectedBytes) {
			if (!Stream_EnsureCapacity(service->inStream, len)) {
				return OGON_INCOMING_BYTES_INVALID_MESSAGE;
			}

			service->waitingHeaders = FALSE;
			return OGON_INCOMING_BYTES_OK;
		}
	}

	Stream_SealLength(service->inStream);
	Stream_SetPosition(service->inStream, 0);

	if (!ogon_message_read(service->inStream, service->messageType, &service->clientMessage)) {
		WLog_ERR(TAG, "invalid message type %"PRIu16"", service->messageType);
		return OGON_INCOMING_BYTES_INVALID_MESSAGE;
	}

	client = &service->client;
	msg = &service->clientMessage;
	/* WLog_DBG(TAG, "message type %"PRIu16" (%s)", service->messageType, ogon_message_name(service->messageType)); */
	switch (service->messageType)
	{
	case OGON_CLIENT_CAPABILITIES:
		IFCALLRET(client->Capabilities, success, cb_data, &msg->capabilities);
		break;
	case OGON_CLIENT_SYNCHRONIZE_KEYBOARD_EVENT:
		IFCALLRET(client->SynchronizeKeyboardEvent, success, cb_data, msg->synchronizeKeyboard.flags,
				  msg->synchronizeKeyboard.clientId);
		break;
	case OGON_CLIENT_SCANCODE_KEYBOARD_EVENT:
		IFCALLRET(client->ScancodeKeyboardEvent, success, cb_data, msg->scancodeKeyboard.flags,
				  msg->scancodeKeyboard.code, msg->scancodeKeyboard.keyboardType, msg->scancodeKeyboard.clientId);
		break;
	case OGON_CLIENT_UNICODE_KEYBOARD_EVENT:
		IFCALLRET(client->UnicodeKeyboardEvent, success, cb_data, msg->unicodeKeyboard.flags, msg->unicodeKeyboard.code,
				  msg->unicodeKeyboard.clientId);
		break;
	case OGON_CLIENT_MOUSE_EVENT:
		IFCALLRET(client->MouseEvent, success, cb_data, msg->mouse.flags, msg->mouse.x, msg->mouse.y,
				  msg->mouse.clientId);
		break;
	case OGON_CLIENT_EXTENDED_MOUSE_EVENT:
		IFCALLRET(client->ExtendedMouseEvent, success, cb_data, msg->extendedMouse.flags, msg->extendedMouse.x,
				  msg->extendedMouse.y, msg->extendedMouse.clientId);
		break;
	case OGON_CLIENT_FRAMEBUFFER_SYNC_REQUEST:
		IFCALLRET(client->FramebufferSyncRequest, success, cb_data, msg->framebufferSyncRequest.bufferId);
		break;
	case OGON_CLIENT_SBP_REPLY:
		IFCALLRET(client->Sbp, success, cb_data, &msg->sbpReply);
		break;
	case OGON_CLIENT_IMMEDIATE_SYNC_REQUEST:
		IFCALLRET(client->ImmediateSyncRequest, success, cb_data, msg->immediateSyncRequest.bufferId);
		break;
	case OGON_CLIENT_SEAT_NEW:
		IFCALLRET(client->SeatNew, success, cb_data, &msg->seatNew);
		break;
	case OGON_CLIENT_SEAT_REMOVED:
		IFCALLRET(client->SeatRemoved, success, cb_data, msg->seatRemoved.clientId);
		break;
	case OGON_CLIENT_MESSAGE:
		IFCALLRET(client->Message, success, cb_data, &msg->message);
		break;
	case OGON_CLIENT_VERSION:
		msgVersion.versionMajor = OGON_PROTOCOL_VERSION_MAJOR;
		msgVersion.versionMinor = OGON_PROTOCOL_VERSION_MINOR;
		msgVersion.cookie = getenv("OGON_BACKEND_COOKIE");
		if (!ogon_service_write_message(service, OGON_SERVER_VERSION_REPLY, (ogon_message *) &msgVersion)) {
			WLog_ERR(TAG, "failed to write version reply message");
			success = FALSE;
		}
		else if (msg->version.versionMajor != OGON_PROTOCOL_VERSION_MAJOR) {
			WLog_ERR(TAG, "received protocol version info with %"PRIu32".%"PRIu32" but own protocol version is %"PRIu32".%"PRIu32"",
				msg->version.versionMajor, msg->version.versionMinor,
				OGON_PROTOCOL_VERSION_MAJOR, OGON_PROTOCOL_VERSION_MINOR);
			success = FALSE;
		}
		break;

	default:
		WLog_ERR(TAG, "Unhandled message with type %"PRIu16"!", service->messageType);
		success = FALSE;
		break;
	}

	ogon_message_free(service->messageType, &service->clientMessage, TRUE);

	if (!success) {
		WLog_ERR(TAG, "Error handling message of type %"PRIu16"", service->messageType);
		return OGON_INCOMING_BYTES_INVALID_MESSAGE;
	}

	Stream_SetPosition(service->inStream, 0);
	service->expectedBytes = RDS_ORDER_HEADER_LENGTH;
	service->waitingHeaders = TRUE;
	return OGON_INCOMING_BYTES_OK;
}

BOOL ogon_service_write_message(ogon_backend_service *service, UINT16 type, ogon_message *msg) {
	DWORD written, toWrite;
	BYTE *ptr;
	int len;
	ogon_protobuf_message encoded;
	BOOL ret = TRUE;


	len = ogon_message_prepare(type, msg, &encoded);
	if (len < 0) {
		WLog_ERR(TAG, "error when preparing server message type %"PRIu16"", type);
		return FALSE;
	}

	if (!ogon_message_write(service->outStream, type, len, &encoded)) {
		ret = FALSE;
		goto out;
	}

	Stream_SealLength(service->outStream);
	toWrite = Stream_Length(service->outStream);
	ptr = Stream_Buffer(service->outStream);
	while (toWrite) {
		if (!WriteFile(service->remotePipe, ptr, toWrite, &written, NULL)) {
			ret = FALSE;
			goto out;
		}
		ptr += written;
		toWrite -= written;
	}

	Stream_SetPosition(service->outStream, 0);
out:
	ogon_message_unprepare(type, &encoded);
	return ret;
}

void ogon_service_kill_client(ogon_backend_service *service) {
	if (!service->remotePipe || service->remotePipe == INVALID_HANDLE_VALUE) {
		return;
	}

	DisconnectNamedPipe(service->remotePipe);
	service->remotePipe = INVALID_HANDLE_VALUE;

	Stream_SetPosition(service->inStream, 0);
	Stream_SetPosition(service->outStream, 0);
	service->expectedBytes = RDS_ORDER_HEADER_LENGTH;
	service->waitingHeaders = TRUE;
}


BOOL ogon_check_pid_and_uid(int fd, BOOL checkUid, uid_t targetUid, BOOL checkPid, pid_t targetPid) {
	BOOL haveUid, havePid;
	uid_t uid;
	pid_t pid;

	if (!ogon_socket_credentials(fd, &uid, &haveUid, &pid, &havePid)) {
		return FALSE;
	}

	if (haveUid) {
		if (checkUid && (targetUid != uid)) {
			return FALSE;
		}
	}

	if (havePid) {
		if (checkPid && (targetPid != pid)) {
			return FALSE;
		}
	}

	return TRUE;
}

BOOL ogon_check_peer_credentials(int fd) {
	pid_t targetPid = 0;
	uid_t targetUid = 0;
	char *endPtr;
	BOOL checkUid = FALSE;
	BOOL checkPid = FALSE;

	const char *envStr = getenv("OGON_PID");
	if (envStr) {
		targetPid = (pid_t)strtol(envStr, &endPtr, 10);
		if (*endPtr != '\0') {
			WLog_WARN(TAG, "invalid OGON_PID env var, skipping pid checking");
		} else {
			checkPid = TRUE;
		}
	}

	envStr = getenv("OGON_UID");
	if (!envStr) {
		WLog_WARN(TAG, "OGON_UID env var not set");
	} else {
		targetUid = strtol(envStr, &endPtr, 10);
		if (*endPtr != '\0') {
			WLog_WARN(TAG, "invalid OGON_UID env variable");
		} else {
			checkUid = TRUE;
		}
	}

	if (!checkPid && !checkUid) {
		/* nothing to check */
		return TRUE;
	}

	return ogon_check_pid_and_uid(fd, checkUid, targetUid, checkPid, targetPid);
}

BOOL ogon_service_check_peer_credentials(ogon_backend_service *service) {
	int fd = ogon_service_client_fd(service);
	if (fd < 0) {
		return FALSE;
	}

	return ogon_check_peer_credentials(fd);
}

HANDLE ogon_service_accept(ogon_backend_service *service) {
	HANDLE rpipe = ogon_named_pipe_accept(service->serverPipe);
	if (!rpipe || rpipe == INVALID_HANDLE_VALUE) {
		return INVALID_HANDLE_VALUE;
	}

	if (service->remotePipe && service->remotePipe != INVALID_HANDLE_VALUE)	{
		CloseHandle(service->remotePipe);

		service->expectedBytes = RDS_ORDER_HEADER_LENGTH;
		service->waitingHeaders = TRUE;
		Stream_SetPosition(service->inStream, 0);
	}

	service->remotePipe = rpipe;

	if (!ogon_service_check_peer_credentials(service)) {
		WLog_ERR(TAG, "unsolicited or forbidden connection on the named pipe");
		CloseHandle(rpipe);
		return INVALID_HANDLE_VALUE;
	}

	return rpipe;
}

HANDLE ogon_service_bind_endpoint(ogon_backend_service *service) {
	service->serverPipe = ogon_named_pipe_create_endpoint(service->sessionId, service->endPoint);
	return service->serverPipe;
}

void ogon_service_free(ogon_backend_service *service) {
	Stream_Free(service->inStream, TRUE);
	free(service->endPoint);
	free(service);
}
