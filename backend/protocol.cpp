/**
 * ogon - Free Remote Desktop Services
 * Backend Library
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
 * Martin Haimberger <martin.haimberger@thincast.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ogon/backend.h>

#include "../common/global.h"
#include "protocol.h"

#define TAG OGON_TAG("backend.protocol")

typedef BOOL (*pfn_ogon_message_read)(wStream *s, ogon_message *msg);
typedef int (*pfn_ogon_message_prepare)(
		ogon_message *msg, ogon_protobuf_message *target);
typedef void (*pfn_ogon_message_unprepare)(ogon_protobuf_message *msg);
typedef void (*pfn_ogon_message_free)(ogon_message *msg);

typedef struct _message_descriptor {
	const char *Name;
	pfn_ogon_message_read Read;
	pfn_ogon_message_prepare Prepare;
	pfn_ogon_message_unprepare Unprepare;
	pfn_ogon_message_free Free;
} message_descriptor;

void ogon_read_message_header(wStream *s, UINT16 *type, UINT32 *len) {
	Stream_Read_UINT16(s, *type);
	Stream_Read_UINT32(s, *len);
}

/* ### CLIENT MESSAGES #################################################### */

/* === capabilities ======================================================= */

static BOOL ogon_read_capabilities(wStream *s, ogon_message *raw) {
	ogon_msg_capabilities *msg = (ogon_msg_capabilities *)raw;
	Ogon__Backend__Capabilities *proto;

	proto = ogon__backend__capabilities__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->desktopWidth = proto->desktopwidth;
	msg->desktopHeight = proto->desktopheight;
	msg->colorDepth = proto->colordepth;
	msg->keyboardLayout = proto->keyboardlayout;
	msg->keyboardType = proto->keyboardtype;
	msg->keyboardSubType = proto->keyboardsubtype;
	msg->clientId = proto->clientid;

	ogon__backend__capabilities__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_capabilities(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	Ogon__Backend__Capabilities *target = (Ogon__Backend__Capabilities *)traw;
	ogon_msg_capabilities *msg = (ogon_msg_capabilities *)mraw;
	ogon__backend__capabilities__init(target);

	target->desktopwidth = msg->desktopWidth;
	target->desktopheight = msg->desktopHeight;
	target->colordepth = msg->colorDepth;
	target->keyboardlayout = msg->keyboardLayout;
	target->keyboardtype = msg->keyboardType;
	target->keyboardsubtype = msg->keyboardSubType;
	target->clientid = msg->clientId;

	return ogon__backend__capabilities__get_packed_size(target);
}

static message_descriptor capabilities_descriptor = {"capabilities",
		ogon_read_capabilities, ogon_prepare_capabilities, nullptr, nullptr};

/* === version ============================================================ */

static BOOL ogon_read_version(wStream *s, ogon_message *raw) {
	ogon_msg_version *msg = (ogon_msg_version *)raw;
	Ogon__Backend__VersionReply *proto;
	BOOL ret = TRUE;

	proto = ogon__backend__version_reply__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->versionMajor = proto->vmajor;
	msg->versionMinor = proto->vminor;
	if (proto->cookie) {
		msg->cookie = strdup(proto->cookie);
		if (!msg->cookie) {
			WLog_ERR(TAG, "unable to duplicate cookie value");
			ret = FALSE;
		}
	} else {
		msg->cookie = nullptr;
	}

	ogon__backend__version_reply__free_unpacked(proto, nullptr);
	return ret;
}

static int ogon_prepare_version(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_version *msg = (ogon_msg_version *)mraw;
	Ogon__Backend__VersionReply *target = (Ogon__Backend__VersionReply *)traw;
	ogon__backend__version_reply__init(target);

	target->vmajor = msg->versionMajor;
	target->vminor = msg->versionMinor;
	target->cookie = msg->cookie;

	return ogon__backend__version_reply__get_packed_size(target);
}

static void ogon_free_version(ogon_message *msg) {
	ogon_msg_version *version = (ogon_msg_version *)msg;
	if (msg) free(version->cookie);
}

static message_descriptor version_descriptor = {"version", ogon_read_version,
		ogon_prepare_version, nullptr, ogon_free_version};

/* === synchronize keyboard event ========================================= */

static BOOL ogon_read_synchronize_keyboard_event(
		wStream *s, ogon_message *raw) {
	Ogon__Backend__KeyboardSync *proto;
	ogon_msg_synchronize_keyboard_event *msg =
			(ogon_msg_synchronize_keyboard_event *)raw;

	proto = ogon__backend__keyboard_sync__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto || !msg) {
		return FALSE;
	}

	msg->flags = proto->flags;
	msg->clientId = proto->clientid;
	ogon__backend__keyboard_sync__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_synchronize_keyboard_event(
		ogon_message *msg, ogon_protobuf_message *traw) {
	ogon_msg_synchronize_keyboard_event *ev =
			(ogon_msg_synchronize_keyboard_event *)msg;
	Ogon__Backend__KeyboardSync *target = (Ogon__Backend__KeyboardSync *)traw;
	if (!ev) return -1;
	ogon__backend__keyboard_sync__init(target);
	target->flags = ev->flags;
	target->clientid = ev->clientId;
	return ogon__backend__keyboard_sync__get_packed_size(target);
}

static message_descriptor synchronize_keyboard_descriptor = {
		"Synchronize keyboard", ogon_read_synchronize_keyboard_event,
		ogon_prepare_synchronize_keyboard_event, nullptr, nullptr};

/* === scancode keyboard event ============================================ */

static BOOL ogon_read_scancode_keyboard_event(wStream *s, ogon_message *raw) {
	ogon_msg_scancode_keyboard_event *msg =
			(ogon_msg_scancode_keyboard_event *)raw;
	Ogon__Backend__KeyboardScanCode *proto;

	proto = ogon__backend__keyboard_scan_code__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->flags = proto->flags;
	msg->code = proto->code;
	msg->keyboardType = proto->keyboardtype;
	msg->clientId = proto->clientid;
	ogon__backend__keyboard_scan_code__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_scancode_keyboard_event(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_scancode_keyboard_event *msg =
			(ogon_msg_scancode_keyboard_event *)mraw;
	Ogon__Backend__KeyboardScanCode *target =
			(Ogon__Backend__KeyboardScanCode *)traw;
	ogon__backend__keyboard_scan_code__init(target);
	target->flags = msg->flags;
	target->code = msg->code;
	target->keyboardtype = msg->keyboardType;
	target->clientid = msg->clientId;
	return ogon__backend__keyboard_scan_code__get_packed_size(target);
}

static message_descriptor scancode_keyboard_descriptor = {"Scancode keyboard",
		ogon_read_scancode_keyboard_event, ogon_prepare_scancode_keyboard_event,
		nullptr, nullptr};

/* === unicode keyboard event ============================================= */

static BOOL ogon_read_unicode_keyboard_event(wStream *s, ogon_message *raw) {
	ogon_msg_unicode_keyboard_event *msg =
			(ogon_msg_unicode_keyboard_event *)raw;
	Ogon__Backend__KeyboardUnicode *proto;

	proto = ogon__backend__keyboard_unicode__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->flags = proto->flags;
	msg->code = proto->code;
	msg->clientId = proto->clientid;
	ogon__backend__keyboard_unicode__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_unicode_keyboard_event(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_unicode_keyboard_event *msg =
			(ogon_msg_unicode_keyboard_event *)mraw;
	Ogon__Backend__KeyboardUnicode *target =
			(Ogon__Backend__KeyboardUnicode *)traw;
	ogon__backend__keyboard_unicode__init(target);
	target->flags = msg->flags;
	target->code = msg->code;
	target->clientid = msg->clientId;
	return ogon__backend__keyboard_unicode__get_packed_size(target);
}

static message_descriptor unicode_keyboard_descriptor = {"unicode keyboard",
		ogon_read_unicode_keyboard_event, ogon_prepare_unicode_keyboard_event,
		nullptr, nullptr};

/* === mouse event ======================================================== */

static BOOL ogon_read_mouse_event(wStream *s, ogon_message *raw) {
	ogon_msg_mouse_event *msg = (ogon_msg_mouse_event *)raw;
	Ogon__Backend__MouseEvent *proto;

	proto = ogon__backend__mouse_event__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->flags = proto->flags;
	msg->x = proto->x;
	msg->y = proto->y;
	msg->clientId = proto->clientid;
	ogon__backend__mouse_event__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_mouse_event(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_mouse_event *msg = (ogon_msg_mouse_event *)mraw;
	Ogon__Backend__MouseEvent *target = (Ogon__Backend__MouseEvent *)traw;
	ogon__backend__mouse_event__init(target);
	target->flags = msg->flags;
	target->x = msg->x;
	target->y = msg->y;
	target->clientid = msg->clientId;
	return ogon__backend__mouse_event__get_packed_size(target);
}

static message_descriptor mouse_descriptor = {"mouse event",
		ogon_read_mouse_event, ogon_prepare_mouse_event, nullptr, nullptr};

/* === extended mouse event =============================================== */

static BOOL ogon_read_extended_mouse_event(wStream *s, ogon_message *raw) {
	ogon_msg_extended_mouse_event *msg = (ogon_msg_extended_mouse_event *)raw;
	Ogon__Backend__MouseExtendedEvent *proto;

	proto = ogon__backend__mouse_extended_event__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->flags = proto->flags;
	msg->x = proto->x;
	msg->y = proto->y;
	msg->clientId = proto->clientid;
	ogon__backend__mouse_extended_event__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_extended_mouse_event(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_extended_mouse_event *msg = (ogon_msg_extended_mouse_event *)mraw;
	Ogon__Backend__MouseExtendedEvent *target =
			(Ogon__Backend__MouseExtendedEvent *)traw;
	ogon__backend__mouse_extended_event__init(target);
	target->flags = msg->flags;
	target->x = msg->x;
	target->y = msg->y;
	target->clientid = msg->clientId;
	return ogon__backend__mouse_extended_event__get_packed_size(target);
}

static message_descriptor extended_mouse_descriptor = {"extended mouse event",
		ogon_read_extended_mouse_event, ogon_prepare_extended_mouse_event,
		nullptr, nullptr};

/* === framebuffer sync request =========================================== */

static BOOL ogon_read_framebuffer_sync_request(wStream *s, ogon_message *raw) {
	ogon_msg_framebuffer_sync_request *msg =
			(ogon_msg_framebuffer_sync_request *)raw;
	Ogon__Backend__SyncRequest *proto;

	proto = ogon__backend__sync_request__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->bufferId = proto->bufferid;
	ogon__backend__sync_request__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_framebuffer_sync_request(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_framebuffer_sync_request *msg =
			(ogon_msg_framebuffer_sync_request *)mraw;
	Ogon__Backend__SyncRequest *target = (Ogon__Backend__SyncRequest *)traw;
	ogon__backend__sync_request__init(target);
	target->bufferid = msg->bufferId;
	return ogon__backend__sync_request__get_packed_size(target);
}

static message_descriptor framebuffer_sync_request_descriptor = {
		"framebuffer sync request", ogon_read_framebuffer_sync_request,
		ogon_prepare_framebuffer_sync_request, nullptr, nullptr};

/* === sbp reply ========================================================== */

static BOOL ogon_read_sbp_reply(wStream *s, ogon_message *raw) {
	ogon_msg_sbp_reply *msg = (ogon_msg_sbp_reply *)raw;
	Ogon__Backend__SbpReply *proto;
	BOOL ret = FALSE;

	proto = ogon__backend__sbp_reply__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	memset(msg, 0, sizeof(*msg));

	msg->status = static_cast<SBP_REPLY_STATUS>(proto->status);
	msg->tag = proto->tag;
	msg->sbpType = proto->sbptype;

	if (proto->data.len) {
		msg->data = static_cast<char *>(malloc(proto->data.len));
		if (!msg->data) {
			WLog_ERR(TAG,
					"read ipcs reply: unable to allocate data blob (len=%" PRIuz
					")",
					proto->data.len);
			goto out;
		}
		memcpy(msg->data, proto->data.data, proto->data.len);
		msg->dataLen = proto->data.len;
	}

	ret = TRUE;
out:
	ogon__backend__sbp_reply__free_unpacked(proto, nullptr);
	return ret;
}

static int ogon_prepare_sbp_reply(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_sbp_reply *msg = (ogon_msg_sbp_reply *)mraw;
	Ogon__Backend__SbpReply *target = (Ogon__Backend__SbpReply *)traw;
	ogon__backend__sbp_reply__init(target);
	target->status = msg->status;
	target->tag = msg->tag;
	target->sbptype = msg->sbpType;
	target->data.len = msg->dataLen;
	target->data.data = (uint8_t *)msg->data;
	target->data.len = msg->dataLen;
	return ogon__backend__sbp_reply__get_packed_size(target);
}

static void ogon_sbp_reply_free(ogon_message *raw) {
	ogon_msg_sbp_reply *msg = (ogon_msg_sbp_reply *)raw;
	if (msg) free(msg->data);
}

static message_descriptor sbp_reply_descriptor = {"sbp reply",
		ogon_read_sbp_reply, ogon_prepare_sbp_reply, nullptr,
		ogon_sbp_reply_free};

static message_descriptor immediate_sync_request_descriptor = {
		"framebuffer immediate sync request",
		ogon_read_framebuffer_sync_request,
		ogon_prepare_framebuffer_sync_request, nullptr, nullptr};

/* === seat new =========================================================== */

static BOOL ogon_read_seat_new(wStream *s, ogon_message *raw) {
	ogon_msg_seat_new *msg = (ogon_msg_seat_new *)raw;
	Ogon__Backend__SeatNew *proto;

	proto = ogon__backend__seat_new__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->clientId = proto->clientid;
	msg->keyboardLayout = proto->keyboardlayout;
	msg->keyboardType = proto->keyboardtype;
	msg->keyboardSubType = proto->keyboardsubtype;
	ogon__backend__seat_new__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_seat_new(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_seat_new *msg = (ogon_msg_seat_new *)mraw;
	Ogon__Backend__SeatNew *target = (Ogon__Backend__SeatNew *)traw;
	ogon__backend__seat_new__init(target);
	target->clientid = msg->clientId;
	target->keyboardlayout = msg->keyboardLayout;
	target->keyboardtype = msg->keyboardType;
	target->keyboardsubtype = msg->keyboardSubType;
	return ogon__backend__seat_new__get_packed_size(target);
}

static message_descriptor seat_new_descriptor = {"new seat announce",
		ogon_read_seat_new, ogon_prepare_seat_new, nullptr, nullptr};

/* === seat removed ======================================================= */

static BOOL ogon_read_seat_removed(wStream *s, ogon_message *raw) {
	ogon_msg_seat_removed *msg = (ogon_msg_seat_removed *)raw;
	Ogon__Backend__SeatRemoved *proto;

	proto = ogon__backend__seat_removed__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->clientId = proto->clientid;
	ogon__backend__seat_removed__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_seat_removed(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_seat_removed *msg = (ogon_msg_seat_removed *)mraw;
	Ogon__Backend__SeatRemoved *target = (Ogon__Backend__SeatRemoved *)traw;
	ogon__backend__seat_removed__init(target);
	target->clientid = msg->clientId;
	return ogon__backend__seat_removed__get_packed_size(target);
}

static message_descriptor seat_removed_descriptor = {"removed seat announce",
		ogon_read_seat_removed, ogon_prepare_seat_removed, nullptr, nullptr};

/* === message ============================================================ */

static BOOL ogon_read_string(char *parameter, char **data, UINT32 *len) {
	if (!data || !len) {
		return FALSE;
	}

	*data = strdup(parameter);
	*len = strlen(parameter);
	return *data != nullptr;
}

static BOOL ogon_read_user_message(wStream *s, ogon_message *raw) {
	ogon_msg_message *msg = (ogon_msg_message *)raw;
	Ogon__Backend__Message *proto;
	BOOL ret = FALSE;

	proto = ogon__backend__message__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	memset(msg, 0, sizeof(*msg));
	msg->message_id = proto->messageid;
	msg->message_type = proto->messagetype;
	msg->style = proto->style;
	msg->timeout = proto->timeout;
	msg->parameter_num = proto->n_parameters;

	if (msg->parameter_num >= 1) {
		if (!ogon_read_string(proto->parameters[0], &msg->parameter1,
					&msg->parameter1_len)) {
			goto out;
		}
	}

	if (msg->parameter_num >= 2) {
		if (!ogon_read_string(proto->parameters[1], &msg->parameter2,
					&msg->parameter2_len)) {
			goto out;
		}
	}

	if (msg->parameter_num >= 3) {
		if (!ogon_read_string(proto->parameters[2], &msg->parameter3,
					&msg->parameter3_len)) {
			goto out;
		}
	}

	if (msg->parameter_num >= 4) {
		if (!ogon_read_string(proto->parameters[3], &msg->parameter4,
					&msg->parameter4_len)) {
			goto out;
		}
	}

	if (msg->parameter_num >= 5) {
		if (!ogon_read_string(proto->parameters[4], &msg->parameter5,
					&msg->parameter5_len)) {
			goto out;
		}
	}

	ret = TRUE;
out:
	ogon__backend__message__free_unpacked(proto, nullptr);
	return ret;
}

static int ogon_prepare_user_message(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_message *msg = (ogon_msg_message *)mraw;
	Ogon__Backend__Message *target = (Ogon__Backend__Message *)traw;
	ogon__backend__message__init(target);
	target->messageid = msg->message_id;
	target->messagetype = msg->message_type;
	target->style = msg->style;
	target->timeout = msg->timeout;
	target->parameters = nullptr;
	target->n_parameters = 0;

	if (msg->parameter_num) {
		target->parameters = static_cast<char **>(
				calloc(msg->parameter_num, sizeof(char *)));
		if (!target->parameters) {
			WLog_ERR(TAG,
					"prepare user message: unable to allocate parameters "
					"(len=%" PRIu32 ")",
					msg->parameter_num);
			return -1;
		}
		target->n_parameters = msg->parameter_num;
	}

	if (msg->parameter_num >= 1) {
		target->parameters[0] = msg->parameter1;
	}

	if (msg->parameter_num >= 2) {
		target->parameters[1] = msg->parameter2;
	}

	if (msg->parameter_num >= 3) {
		target->parameters[2] = msg->parameter3;
	}

	if (msg->parameter_num >= 4) {
		target->parameters[3] = msg->parameter4;
	}

	if (msg->parameter_num >= 5) {
		target->parameters[4] = msg->parameter5;
	}

	return ogon__backend__message__get_packed_size(target);
}

static void ogon_unprepare_user_message(ogon_protobuf_message *traw) {
	Ogon__Backend__Message *target = (Ogon__Backend__Message *)traw;
	if (target) free(target->parameters);
}

static void ogon_user_message_free(ogon_message *raw) {
	ogon_msg_message *msg = (ogon_msg_message *)raw;
	if (!msg) return;
	free(msg->parameter1);
	free(msg->parameter2);
	free(msg->parameter3);
	free(msg->parameter4);
	free(msg->parameter5);
}

static message_descriptor user_message_descriptor = {"Message",
		ogon_read_user_message, ogon_prepare_user_message,
		ogon_unprepare_user_message, ogon_user_message_free};

/* ### SERVER MESSAGES #################################################### */

/* === set pointer ======================================================== */

static BOOL ogon_read_set_pointer(wStream *s, ogon_message *raw) {
	ogon_msg_set_pointer *msg = (ogon_msg_set_pointer *)raw;
	Ogon__Backend__SetPointerShape *proto;
	BOOL ret = FALSE;

	proto = ogon__backend__set_pointer_shape__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	memset(msg, 0, sizeof(*msg));

	msg->xPos = proto->xpos;
	msg->yPos = proto->ypos;
	msg->width = proto->width;
	msg->height = proto->height;
	msg->xorBpp = proto->xorbpp;

	if (proto->has_andmask) {
		msg->andMaskData = static_cast<BYTE *>(malloc(proto->andmask.len));
		if (!msg->andMaskData) {
			WLog_ERR(TAG,
					"read set pointer: unable to allocate andMaskData "
					"(len=%" PRIuz ")",
					proto->andmask.len);
			goto out;
		}
		memcpy(msg->andMaskData, proto->andmask.data, proto->andmask.len);
		msg->lengthAndMask = proto->andmask.len;
	}

	if (proto->has_xormask) {
		msg->xorMaskData = static_cast<BYTE *>(malloc(proto->xormask.len));
		if (!msg->xorMaskData) {
			WLog_ERR(TAG,
					"read set pointer: unable to allocate xorMaskData "
					"(len=%" PRIuz ")",
					proto->xormask.len);
			goto out;
		}
		memcpy(msg->xorMaskData, proto->xormask.data, proto->xormask.len);
		msg->lengthXorMask = proto->xormask.len;
	}

	msg->clientId = proto->clientid;
	ret = TRUE;
out:
	if (!ret) {
		free(msg->andMaskData);
		msg->andMaskData = nullptr;
		free(msg->xorMaskData);
		msg->xorMaskData = nullptr;
	}
	ogon__backend__set_pointer_shape__free_unpacked(proto, nullptr);
	return ret;
}

static int ogon_prepare_set_pointer(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_set_pointer *msg = (ogon_msg_set_pointer *)mraw;
	Ogon__Backend__SetPointerShape *target =
			(Ogon__Backend__SetPointerShape *)traw;
	ogon__backend__set_pointer_shape__init(target);
	target->xpos = msg->xPos;
	target->ypos = msg->yPos;
	target->width = msg->width;
	target->height = msg->height;
	target->xorbpp = msg->xorBpp;

	if (msg->andMaskData) {
		target->has_andmask = TRUE;
		target->andmask.len = msg->lengthAndMask;
		target->andmask.data = msg->andMaskData;
	}

	if (msg->xorMaskData) {
		target->has_xormask = TRUE;
		target->xormask.len = msg->lengthXorMask;
		target->xormask.data = msg->xorMaskData;
	}
	target->clientid = msg->clientId;

	return ogon__backend__set_pointer_shape__get_packed_size(target);
}

static void ogon_set_pointer_free(ogon_message *raw) {
	ogon_msg_set_pointer *msg = (ogon_msg_set_pointer *)raw;
	if (!msg) return;
	free(msg->andMaskData);
	free(msg->xorMaskData);
}

static message_descriptor set_pointer_descriptor = {"SetPointer",
		ogon_read_set_pointer, ogon_prepare_set_pointer, nullptr,
		ogon_set_pointer_free};

/* === set system pointer ================================================= */

static BOOL ogon_read_set_system_pointer(wStream *s, ogon_message *raw) {
	ogon_msg_set_system_pointer *msg = (ogon_msg_set_system_pointer *)raw;
	Ogon__Backend__SetSystemPointer *proto;

	proto = ogon__backend__set_system_pointer__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->ptrType = proto->type;
	msg->clientId = proto->clientid;
	ogon__backend__set_system_pointer__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_set_system_pointer(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_set_system_pointer *msg = (ogon_msg_set_system_pointer *)mraw;
	Ogon__Backend__SetSystemPointer *target =
			(Ogon__Backend__SetSystemPointer *)traw;
	ogon__backend__set_system_pointer__init(target);
	target->type = msg->ptrType;
	target->clientid = msg->clientId;
	return ogon__backend__set_system_pointer__get_packed_size(target);
}

static message_descriptor set_system_pointer_descriptor = {"SetSystemPointer",
		ogon_read_set_system_pointer, ogon_prepare_set_system_pointer, nullptr,
		nullptr};

/* === framebuffer info
 * ============================================================ */

static BOOL ogon_read_framebuffer_info(wStream *s, ogon_message *raw) {
	ogon_msg_framebuffer_info *msg = (ogon_msg_framebuffer_info *)raw;
	Ogon__Backend__FramebufferInfos *proto;

	proto = ogon__backend__framebuffer_infos__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->version = proto->version;
	msg->width = proto->width;
	msg->height = proto->height;
	msg->scanline = proto->scanline;
	msg->bitsPerPixel = proto->bitsperpixel;
	msg->bytesPerPixel = proto->bytesperpixel;
	msg->userId = proto->userid;
	msg->multiseatCapable =
			(proto->flags & OGON__BACKEND__BACKEND__FLAGS__MULTISEAT);
	ogon__backend__framebuffer_infos__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_framebuffer_info(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_framebuffer_info *msg = (ogon_msg_framebuffer_info *)mraw;
	Ogon__Backend__FramebufferInfos *target =
			(Ogon__Backend__FramebufferInfos *)traw;
	ogon__backend__framebuffer_infos__init(target);
	target->version = msg->version;
	target->width = msg->width;
	target->height = msg->height;
	target->scanline = msg->scanline;
	target->bitsperpixel = msg->bitsPerPixel;
	target->bytesperpixel = msg->bytesPerPixel;
	target->userid = msg->userId;
	target->flags = 0;
	if (msg->multiseatCapable)
		target->flags |= OGON__BACKEND__BACKEND__FLAGS__MULTISEAT;
	return ogon__backend__framebuffer_infos__get_packed_size(target);
}

static message_descriptor framebuffer_info_descriptor = {"FramebufferInfo",
		ogon_read_framebuffer_info, ogon_prepare_framebuffer_info, nullptr,
		nullptr};

/* === beep ============================================================ */

static BOOL ogon_read_beep(wStream *s, ogon_message *raw) {
	ogon_msg_beep *msg = (ogon_msg_beep *)raw;
	Ogon__Backend__Beep *proto;

	proto = ogon__backend__beep__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->duration = proto->duration;
	msg->frequency = proto->frequency;
	ogon__backend__beep__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_beep(ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_beep *msg = (ogon_msg_beep *)mraw;
	Ogon__Backend__Beep *target = (Ogon__Backend__Beep *)traw;
	ogon__backend__beep__init(target);
	target->duration = msg->duration;
	target->frequency = msg->frequency;
	return ogon__backend__beep__get_packed_size(target);
}

static message_descriptor beep_descriptor = {
		"Beep", ogon_read_beep, ogon_prepare_beep, nullptr, nullptr};

/* === sbp request ======================================================== */

static BOOL ogon_read_sbp_request(wStream *s, ogon_message *raw) {
	ogon_msg_sbp_request *msg = (ogon_msg_sbp_request *)raw;
	Ogon__Backend__SbpRequest *proto;
	BOOL ret = FALSE;

	proto = ogon__backend__sbp_request__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	memset(msg, 0, sizeof(*msg));

	msg->sbpType = proto->sbptype;
	msg->tag = proto->tag;

	if (proto->data.len) {
		msg->data = static_cast<char *>(malloc(proto->data.len));
		if (!msg->data) {
			WLog_ERR(TAG,
					"read sbp request: unable to allocate data blob "
					"(len=%" PRIuz ")",
					proto->data.len);
			goto out;
		}
		memcpy(msg->data, proto->data.data, proto->data.len);
		msg->dataLen = proto->data.len;
	}

	ret = TRUE;
out:
	ogon__backend__sbp_request__free_unpacked(proto, nullptr);
	return ret;
}

static int ogon_prepare_sbp_request(
		ogon_message *mraw, ogon_protobuf_message *traw) {
	ogon_msg_sbp_request *msg = (ogon_msg_sbp_request *)mraw;
	Ogon__Backend__SbpRequest *target = (Ogon__Backend__SbpRequest *)traw;
	ogon__backend__sbp_request__init(target);
	target->sbptype = msg->sbpType;
	target->tag = msg->tag;
	target->data.len = msg->dataLen;
	target->data.data = (uint8_t *)msg->data;
	return ogon__backend__sbp_request__get_packed_size(target);
}

static void ogon_sbp_request_free(ogon_message *raw) {
	ogon_msg_sbp_request *msg = (ogon_msg_sbp_request *)raw;
	if (msg) free(msg->data);
}

static message_descriptor sbp_request_descriptor = {"sbp request",
		ogon_read_sbp_request, ogon_prepare_sbp_request, nullptr,
		ogon_sbp_request_free};

/* === framebuffer sync reply ============================================= */

static BOOL ogon_read_framebuffer_sync_reply(wStream *s, ogon_message *raw) {
	ogon_msg_framebuffer_sync_reply *msg =
			(ogon_msg_framebuffer_sync_reply *)raw;
	Ogon__Backend__SyncReply *proto;

	proto = ogon__backend__sync_reply__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto) {
		return FALSE;
	}

	msg->bufferId = proto->bufferid;
	ogon__backend__sync_reply__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_write_framebuffer_sync_reply(
		ogon_message *rmsg, ogon_protobuf_message *raw) {
	ogon_msg_framebuffer_sync_reply *msg =
			(ogon_msg_framebuffer_sync_reply *)rmsg;
	Ogon__Backend__SyncReply *target = (Ogon__Backend__SyncReply *)raw;
	ogon__backend__sync_reply__init(target);
	target->bufferid = msg->bufferId;
	return ogon__backend__sync_reply__get_packed_size(target);
}

static message_descriptor framebuffer_sync_reply_descriptor = {
		"framebuffer sync reply", ogon_read_framebuffer_sync_reply,
		ogon_write_framebuffer_sync_reply, nullptr, nullptr};

/* === message reply ====================================================== */

static BOOL ogon_read_message_reply(wStream *s, ogon_message *raw) {
	ogon_msg_message_reply *msg = (ogon_msg_message_reply *)raw;
	Ogon__Backend__MessageReply *proto;

	proto = ogon__backend__message_reply__unpack(
			nullptr, Stream_Length(s), (uint8_t *)Stream_Pointer(s));
	if (!proto || !msg) {
		return FALSE;
	}

	msg->message_id = proto->messageid;
	msg->result = proto->result;
	ogon__backend__message_reply__free_unpacked(proto, nullptr);
	return TRUE;
}

static int ogon_prepare_message_reply(
		ogon_message *rmsg, ogon_protobuf_message *raw) {
	ogon_msg_message_reply *msg = (ogon_msg_message_reply *)rmsg;
	Ogon__Backend__MessageReply *target = (Ogon__Backend__MessageReply *)raw;
	ogon__backend__message_reply__init(target);
	target->messageid = msg->message_id;
	target->result = msg->result;
	return ogon__backend__message_reply__get_packed_size(target);
}

static message_descriptor message_reply_descriptor = {"message reply",
		ogon_read_message_reply, ogon_prepare_message_reply, nullptr, nullptr};

/* ######################################################################## */

static message_descriptor *messages[] = {
		&set_pointer_descriptor,			/* 0 */
		&framebuffer_info_descriptor,		/* 1 */
		&beep_descriptor,					/* 2 */
		&set_system_pointer_descriptor,		/* 3 */
		&sbp_request_descriptor,			/* 4 */
		&framebuffer_sync_reply_descriptor, /* 5 */
		&message_reply_descriptor,			/* 6 */
		&version_descriptor,				/* 7 */

		&capabilities_descriptor,			  /* 8 */
		&synchronize_keyboard_descriptor,	  /* 9 */
		&scancode_keyboard_descriptor,		  /* 10 */
		&unicode_keyboard_descriptor,		  /* 11 */
		&mouse_descriptor,					  /* 12 */
		&extended_mouse_descriptor,			  /* 13 */
		&framebuffer_sync_request_descriptor, /* 14 */
		&sbp_reply_descriptor,				  /* 15 */
		&immediate_sync_request_descriptor,	  /* 16 */
		&seat_new_descriptor,				  /* 17 */
		&seat_removed_descriptor,			  /* 18 */
		&user_message_descriptor,			  /* 19 */
		&version_descriptor,				  /* 20 */
};

#define DESCRIPTORS_NB (sizeof(messages) / sizeof(message_descriptor *))

const char *ogon_message_name(UINT32 type) {
	message_descriptor *msgDef;

	if (type >= DESCRIPTORS_NB) {
		return "<invalid>";
	}

	msgDef = messages[type];
	if (!msgDef || !msgDef->Name) {
		return "<invalid>";
	}

	return (const char *)msgDef->Name;
}

BOOL ogon_message_read(wStream *s, UINT16 type, ogon_message *msg) {
	message_descriptor *msgDef;

	if (type >= DESCRIPTORS_NB) {
		WLog_ERR(
				TAG, "not reading message with invalid type %" PRIu16 "", type);
		return FALSE;
	}

	msgDef = messages[type];

	if (!msgDef->Read) {
		WLog_ERR(TAG, "no read function for message type %" PRIu16 "", type);
		return FALSE;
	}

	return msgDef->Read(s, msg);
}

int ogon_message_prepare(UINT16 type, ogon_message *msg, void *encoded) {
	message_descriptor *msgDef;

	if (type >= DESCRIPTORS_NB) {
		WLog_ERR(TAG, "not preparing message with invalid type %" PRIu16 "",
				type);
		return -1;
	}

	msgDef = messages[type];
	if (!msgDef || !msgDef->Prepare) {
		return -1;
	}

	return msgDef->Prepare(msg, (ogon_protobuf_message *)encoded);
}

BOOL ogon_message_write(wStream *s, UINT16 type, int len, void *encoded) {
	if (!Stream_EnsureRemainingCapacity(s, 2 + 4 + len)) {
		return FALSE;
	}

	Stream_Write_UINT16(s, type);
	Stream_Write_UINT32(s, len);

	protobuf_c_message_pack(
			(const ProtobufCMessage *)encoded, (uint8_t *)Stream_Pointer(s));
	Stream_Seek(s, len);
	return TRUE;
}

void ogon_message_unprepare(UINT16 type, void *encoded) {
	message_descriptor *msgDef;

	if (type >= DESCRIPTORS_NB) {
		WLog_ERR(TAG, "not unpreparing message with invalid type %" PRIu16 "",
				type);
		return;
	}

	msgDef = messages[type];
	if (msgDef->Unprepare) {
		msgDef->Unprepare((ogon_protobuf_message *)encoded);
	}
}

void ogon_message_free(UINT16 type, ogon_message *msg, BOOL onlyInnerData) {
	message_descriptor *msgDef;

	if (type >= DESCRIPTORS_NB) {
		WLog_ERR(
				TAG, "not freeing message with invalid type %" PRIu16 "", type);
		return;
	}

	msgDef = messages[type];
	if (!msgDef) {
		WLog_ERR(TAG,
				"not freeing message type %" PRIu16 " with missing definition",
				type);
		return;
	}

	if (msgDef->Free) {
		msgDef->Free(msg);
	}

	if (!onlyInnerData) {
		free(msg);
	}
}

BOOL ogon_message_send(wStream *s, UINT16 type, ogon_message *msg) {
	BOOL ret = TRUE;
	int len;
	ogon_protobuf_message proto;

	len = ogon_message_prepare(type, msg, &proto);
	if (len < 0) {
		WLog_ERR(TAG, "error when preparing message with type %" PRIu16 "",
				type);
		return FALSE;
	}

	if (!Stream_EnsureRemainingCapacity(s, RDS_ORDER_HEADER_LENGTH + len)) {
		WLog_ERR(TAG, "error resizing stream for message type %" PRIu16 "",
				type);
		ret = FALSE;
		goto out;
	}

	if (!ogon_message_write(s, type, len, &proto)) {
		WLog_ERR(TAG, "error resizing stream for message type %" PRIu16 "",
				type);
		ret = FALSE;
		goto out;
	}
out:
	ogon_message_unprepare(type, &proto);
	return ret;
}
