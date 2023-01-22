/**
 * ogon - Free Remote Desktop Services
 * Backend Library
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
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

#ifndef OGON_BACKEND_H_
#define OGON_BACKEND_H_

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>

#include <ogon/api.h>


/* Common Data Types */

typedef struct {
	UINT32 Flags;
	UINT32 UserLength;
	UINT32 DomainLength;
	UINT32 PasswordLength;

	char* User;
	char* Domain;
	char* Password;
} ogon_msg_logon_user;

typedef struct {
	UINT32 Flags;
} ogon_msg_logoff_user;


/** @brief server message types */
enum {
	OGON_SERVER_SET_POINTER                  = 0,
	OGON_SERVER_FRAMEBUFFER_INFO             = 1,
	OGON_SERVER_BEEP                         = 2,
	OGON_SERVER_SET_SYSTEM_POINTER           = 3,
	OGON_SERVER_SBP_REQUEST          	    = 4,
	OGON_SERVER_FRAMEBUFFER_SYNC_REPLY       = 5,
	OGON_SERVER_MESSAGE_REPLY                = 6,
	OGON_SERVER_VERSION_REPLY                = 7,

	OGON_CLIENT_CAPABILITIES                 = 8,
	OGON_CLIENT_SYNCHRONIZE_KEYBOARD_EVENT   = 9,
	OGON_CLIENT_SCANCODE_KEYBOARD_EVENT      = 10,
	OGON_CLIENT_UNICODE_KEYBOARD_EVENT       = 11,
	OGON_CLIENT_MOUSE_EVENT                  = 12,
	OGON_CLIENT_EXTENDED_MOUSE_EVENT         = 13,
	OGON_CLIENT_FRAMEBUFFER_SYNC_REQUEST     = 14,
	OGON_CLIENT_SBP_REPLY                    = 15,
	OGON_CLIENT_IMMEDIATE_SYNC_REQUEST       = 16,
	OGON_CLIENT_SEAT_NEW                     = 17,
	OGON_CLIENT_SEAT_REMOVED                 = 18,
	OGON_CLIENT_MESSAGE                      = 19,
	OGON_CLIENT_VERSION                      = 20,
};

typedef struct _ogon_msg_synchronize_keyboard_event {
	UINT32 flags;
	UINT32 clientId;
} ogon_msg_synchronize_keyboard_event;

typedef struct _ogon_msg_scancode_keyboard_event {
	UINT32 flags;
	UINT32 code;
	UINT32 keyboardType;
	UINT32 clientId;
} ogon_msg_scancode_keyboard_event;

typedef struct _ogon_msg_unicode_keyboard_event {
	UINT32 flags;
	UINT32 code;
	UINT32 clientId;
} ogon_msg_unicode_keyboard_event;

typedef struct _ogon_msg_mouse_event {
	DWORD flags;
	DWORD x;
	DWORD y;
	UINT32 clientId;
} ogon_msg_mouse_event;

typedef struct _ogon_msg_extended_mouse_event {
	DWORD flags;
	DWORD x;
	DWORD y;
	UINT32 clientId;
} ogon_msg_extended_mouse_event;

typedef struct _ogon_msg_version {
	UINT32 versionMajor;
	UINT32 versionMinor;
	char *cookie;
} ogon_msg_version;

typedef struct _ogon_msg_capabilities {
	UINT32 desktopWidth;
	UINT32 desktopHeight;
	UINT32 colorDepth;
	UINT32 keyboardLayout;
	UINT32 keyboardType;
	UINT32 keyboardSubType;
	UINT32 clientId;
} ogon_msg_capabilities;

typedef struct _ogon_msg_framebuffer_sync_request {
	INT32 bufferId;
} ogon_msg_framebuffer_sync_request;

/* @brief return code for an SBP reply */
typedef enum {
	SBP_REPLY_SUCCESS = 0,
	SBP_REPLY_TRANSPORT_ERROR = 1,
	SBP_REPLY_TIMEOUT = 2,
	SBP_REPLY_NOT_FOUND = 3,
} SBP_REPLY_STATUS;

typedef struct _ogon_msg_sbp_reply {
	UINT32 sbpType;
	UINT32 tag;
	SBP_REPLY_STATUS status;
	UINT32 dataLen;
	char *data;
} ogon_msg_sbp_reply;

typedef struct _ogon_msg_seat_new {
	UINT32 clientId;
	UINT32 keyboardLayout;
	UINT32 keyboardType;
	UINT32 keyboardSubType;
} ogon_msg_seat_new;

typedef struct _ogon_msg_seat_removed {
	UINT32 clientId;
} ogon_msg_seat_removed;

typedef struct _ogon_msg_message {
	UINT32 message_id;
	UINT32 message_type;
	UINT32 style;
	UINT32 timeout;
	UINT32 parameter_num;
	UINT32 parameter1_len;
	char *parameter1;
	UINT32 parameter2_len;
	char *parameter2;
	UINT32 parameter3_len;
	char *parameter3;
	UINT32 parameter4_len;
	char *parameter4;
	UINT32 parameter5_len;
	char *parameter5;
	UINT32 icp_tag;
	UINT32 icp_type;
} ogon_msg_message;

typedef struct _ogon_msg_set_pointer {
	UINT32 xorBpp;
	UINT32 xPos;
	UINT32 yPos;
	UINT32 width;
	UINT32 height;
	UINT32 lengthAndMask;
	UINT32 lengthXorMask;
	BYTE* xorMaskData;
	BYTE* andMaskData;
	UINT32 clientId;
} ogon_msg_set_pointer;

typedef struct _ogon_msg_set_system_pointer {
	UINT32 ptrType;
	UINT32 clientId;
} ogon_msg_set_system_pointer;

typedef struct _ogon_msg_beep {
	UINT32 duration;
	UINT32 frequency;
} ogon_msg_beep;

typedef struct _ogon_msg_framebuffer_info {
	UINT32 version;
	UINT32 width;
	UINT32 height;
	UINT32 scanline;
	UINT32 bitsPerPixel;
	UINT32 bytesPerPixel;
	UINT32 userId;
	BOOL multiseatCapable;
} ogon_msg_framebuffer_info;

typedef struct _ogon_msg_sbp_request {
	UINT32 sbpType;
	UINT32 tag;
	UINT32 dataLen;
	char *data;
} ogon_msg_sbp_request;

typedef struct _ogon_msg_framebuffer_sync_reply {
	INT32 bufferId;
} ogon_msg_framebuffer_sync_reply;

typedef struct _ogon_msg_message_reply {
	UINT32 message_id;
	UINT32 result;
} ogon_msg_message_reply;


typedef BOOL (*pfn_ogon_client_capabilities)(void *backend, ogon_msg_capabilities *capabilities);
typedef BOOL (*pfn_ogon_client_synchronize_keyboard_event)(void *backend, DWORD flags, UINT32 clientId);
typedef BOOL (*pfn_ogon_client_scancode_keyboard_event)(void *backend, DWORD flags, DWORD code, DWORD keyboardType, UINT32 clientId);
typedef BOOL (*pfn_ogon_client_unicode_keyboard_event)(void *backend, DWORD flags, DWORD code, UINT32 clientId);
typedef BOOL (*pfn_ogon_client_mouse_event)(void *backend, DWORD flags, DWORD x, DWORD y, UINT32 clientId);
typedef BOOL (*pfn_ogon_client_extended_mouse_event)(void *backend, DWORD flags, DWORD x, DWORD y, UINT32 clientId);
typedef BOOL (*pfn_ogon_client_framebuffer_sync_request)(void *backend, INT32 bufferId);
typedef BOOL (*pfn_ogon_client_sbp)(void *backend, ogon_msg_sbp_reply *msg);
typedef BOOL (*pfn_ogon_client_immediate_sync_request)(void *backend, INT32 bufferId);
typedef BOOL (*pfn_ogon_client_seat_new)(void *backend, ogon_msg_seat_new *newSeat);
typedef BOOL (*pfn_ogon_client_seat_removed)(void *backend, UINT32 clientId);
typedef BOOL (*pfn_ogon_client_message)(void *backend, ogon_msg_message *msg);

typedef struct _ogon_client_interface {
	pfn_ogon_client_capabilities Capabilities;
	pfn_ogon_client_synchronize_keyboard_event SynchronizeKeyboardEvent;
	pfn_ogon_client_scancode_keyboard_event ScancodeKeyboardEvent;
	pfn_ogon_client_unicode_keyboard_event UnicodeKeyboardEvent;
	pfn_ogon_client_mouse_event MouseEvent;
	pfn_ogon_client_extended_mouse_event ExtendedMouseEvent;
	pfn_ogon_client_framebuffer_sync_request FramebufferSyncRequest;
	pfn_ogon_client_sbp Sbp;
	pfn_ogon_client_immediate_sync_request ImmediateSyncRequest;
	pfn_ogon_client_seat_new SeatNew;
	pfn_ogon_client_seat_removed SeatRemoved;
	pfn_ogon_client_message Message;
} ogon_client_interface;

/**
 * Backend Interface
 */

typedef union _ogon_message {
	/* server part */
	ogon_msg_beep beep;
	ogon_msg_set_pointer setPointer;
	ogon_msg_set_system_pointer setSystemPointer;
	ogon_msg_framebuffer_info framebufferInfo;
	ogon_msg_logon_user logonUser;
	ogon_msg_logoff_user logoffUser;
	ogon_msg_sbp_request sbpRequest;
	ogon_msg_framebuffer_sync_reply framebufferSyncReply;
	ogon_msg_message_reply messageReply;

	/* client part */
	ogon_msg_synchronize_keyboard_event synchronizeKeyboard;
	ogon_msg_scancode_keyboard_event scancodeKeyboard;
	ogon_msg_unicode_keyboard_event unicodeKeyboard;
	ogon_msg_mouse_event mouse;
	ogon_msg_extended_mouse_event extendedMouse;
	ogon_msg_capabilities capabilities;
	ogon_msg_framebuffer_sync_request framebufferSyncRequest;
	ogon_msg_sbp_reply sbpReply;
	ogon_msg_framebuffer_sync_request immediateSyncRequest;
	ogon_msg_seat_new seatNew;
	ogon_msg_seat_removed seatRemoved;
	ogon_msg_message message;
	ogon_msg_version version;
} ogon_message;

#ifdef __cplusplus
extern "C" {
#endif


OGON_API void ogon_read_message_header(wStream *s, UINT16 *type, UINT32 *len);

OGON_API const char* ogon_message_name(UINT32 type);

OGON_API BOOL ogon_message_read(wStream *s, UINT16 type, ogon_message *msg);
OGON_API int ogon_message_prepare(UINT16 type, ogon_message *msg, void *encoded);
OGON_API void ogon_message_unprepare(UINT16 type, void *encoded);
OGON_API BOOL ogon_message_write(wStream *s, UINT16 type, int len, void *encoded);
OGON_API void ogon_message_free(UINT16 type, ogon_message *msg, BOOL onlyInnerData);
OGON_API BOOL ogon_message_send(wStream *s, UINT16 type, ogon_message *msg);


OGON_API void ogon_named_pipe_get_endpoint_name(DWORD id, const char *endpoint, char *dest, size_t len);
OGON_API BOOL ogon_named_pipe_clean(const char* pipeName);
OGON_API HANDLE ogon_named_pipe_connect(const char* pipeName, DWORD nTimeOut);
OGON_API HANDLE ogon_named_pipe_connect_endpoint(DWORD id, const char* endpoint, DWORD nTimeOut);


OGON_API HANDLE ogon_named_pipe_create(const char* pipeName);
OGON_API BOOL ogon_named_pipe_clean_endpoint(DWORD id, const char* endpoint);
OGON_API HANDLE ogon_named_pipe_create_endpoint(DWORD id, const char* endpoint);
OGON_API HANDLE ogon_named_pipe_accept(HANDLE hServerPipe);

OGON_API const char* ogon_evdev_keyname(DWORD evdevcode);
OGON_API DWORD ogon_rdp_scancode_to_evdev_code(DWORD flags, DWORD scancode, DWORD keyboardType);

#ifdef __cplusplus
}
#endif

#endif /* OGON_BACKEND_H_ */
