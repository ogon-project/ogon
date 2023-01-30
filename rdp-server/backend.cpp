/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Backend
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
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifndef _WIN32
#include <errno.h>
#endif

#include <winpr/file.h>
#include <winpr/pipe.h>
#include <winpr/print.h>

#include <ogon/backend.h>
#include <ogon/dmgbuf.h>
#include <ogon/version.h>

#include "../common/global.h"
#include "../common/icp.h"

#include "../backend/protocol.h"

#include "icp/icp_client_stubs.h"
#include "icp/pbrpc/pbrpc_utils.h"

#include "PMurHash.h"
#include "app_context.h"
#include "back_front_internal.h"
#include "backend.h"
#include "encoder.h"
#include "peer.h"

#define TAG OGON_TAG("core.backend")

static BOOL backend_drain_output(ogon_backend_connection *backend);

typedef struct _message_answer {
	UINT32 message_id;
	UINT32 icp_tag;
	UINT32 icp_type;
} message_answer;

static BOOL drain_ringbuffer_to_pipe(
		RingBuffer *rb, HANDLE pipe, BOOL *writeReady) {
	DataChunk chunks[2];
	int nbChunks, i;
	DWORD written, toWrite;
	const BYTE *ptr;
	size_t commitBytes;
	BOOL r;

	while ((nbChunks = ringbuffer_peek(rb, chunks, 0xffff))) {
		commitBytes = 0;

		for (i = 0; i < nbChunks; i++) {
			toWrite = chunks[i].size;
			ptr = chunks[i].data;
			while (toWrite) {
				r = WriteFile(pipe, ptr, toWrite, &written, nullptr);
				if (!r || !written) {
					/* broken IO: r = False written=undefined
					 * or EWOULDBLOCK: r = TRUE written=0 */
					ringbuffer_commit_read_bytes(rb, commitBytes);
					*writeReady = !r;
					return r;
				}

#if 0
				WLog_DBG(TAG, "tried to send %"PRIu32" bytes, written=%"PRIu32"", toWrite, written);
				winpr_HexDump(TAG, WLOG_DEBUG, ptr, written);
#endif
				ptr += written;
				toWrite -= written;
				commitBytes += written;
			}
		}

		ringbuffer_commit_read_bytes(rb, commitBytes);
	} /* while */

	*writeReady = TRUE;
	return TRUE;
}

static BOOL backend_write_rds_message(
		ogon_backend_connection *backend, UINT16 type, ogon_message *msg) {
	wStream *s;
	BYTE *buf;
	int len;
	ogon_protobuf_message protobufMessage;

	len = ogon_message_prepare(type, msg, &protobufMessage);
	if (len < 0) {
		WLog_ERR(TAG, "invalid message");
		return FALSE;
	}

	buf = ringbuffer_ensure_linear_write(
			&backend->xmitBuffer, len + RDS_ORDER_HEADER_LENGTH);
	if (!buf) {
		WLog_ERR(TAG, "can't grow xmit ringbuffer");
		return FALSE;
	}

	if (!(s = Stream_New((BYTE *)buf, len + RDS_ORDER_HEADER_LENGTH))) {
		return FALSE;
	}

	if (!ogon_message_write(s, type, len, &protobufMessage)) {
		Stream_Free(s, FALSE);
		return FALSE;
	}
	Stream_Free(s, FALSE);

	ogon_message_unprepare(type, &protobufMessage);

	if (!ringbuffer_commit_written_bytes(
				&backend->xmitBuffer, RDS_ORDER_HEADER_LENGTH + len)) {
		return FALSE;
	}

	return backend_drain_output(backend);
}

static BOOL backend_synchronize_keyboard_event(
		void *rbackend, DWORD flags, UINT32 connectionId) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	ogon_msg_synchronize_keyboard_event msg;

	msg.flags = flags;
	msg.clientId = connectionId;
	return backend_write_rds_message(backend,
			OGON_CLIENT_SYNCHRONIZE_KEYBOARD_EVENT, (ogon_message *)&msg);
}

static BOOL backend_scancode_keyboard_event(void *rbackend, DWORD flags,
		DWORD code, DWORD keyboardType, UINT32 connectionId) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	ogon_msg_scancode_keyboard_event msg;
	ZeroMemory(&msg, sizeof(msg));

	msg.flags = flags;
	msg.code = code;
	msg.keyboardType = keyboardType;
	msg.clientId = connectionId;
	return backend_write_rds_message(
			backend, OGON_CLIENT_SCANCODE_KEYBOARD_EVENT, (ogon_message *)&msg);
}

static BOOL backend_unicode_keyboard_event(
		void *rbackend, DWORD flags, DWORD code, UINT32 connectionId) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	ogon_msg_unicode_keyboard_event msg;

	msg.flags = flags;
	msg.code = code;
	msg.clientId = connectionId;
	return backend_write_rds_message(
			backend, OGON_CLIENT_UNICODE_KEYBOARD_EVENT, (ogon_message *)&msg);
}

static BOOL backend_mouse_event(
		void *rbackend, DWORD flags, DWORD x, DWORD y, UINT32 connectionId) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	ogon_msg_mouse_event msg;

	msg.flags = flags;
	msg.x = x;
	msg.y = y;
	msg.clientId = connectionId;
	return backend_write_rds_message(
			backend, OGON_CLIENT_MOUSE_EVENT, (ogon_message *)&msg);
}

static BOOL backend_extended_mouse_event(
		void *rbackend, DWORD flags, DWORD x, DWORD y, UINT32 connectionId) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	ogon_msg_extended_mouse_event msg;

	msg.flags = flags;
	msg.x = x;
	msg.y = y;
	msg.clientId = connectionId;

	return backend_write_rds_message(
			backend, OGON_CLIENT_EXTENDED_MOUSE_EVENT, (ogon_message *)&msg);
}

static BOOL backend_framebuffer_sync_request(void *rbackend, INT32 bufferId) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	if (bufferId < 0) {
		return FALSE;
	}
	backend->framebufferSyncRequest.bufferId = bufferId;
	return backend_write_rds_message(backend,
			OGON_CLIENT_FRAMEBUFFER_SYNC_REQUEST,
			(ogon_message *)&backend->framebufferSyncRequest);
}

static BOOL backend_immediate_sync_request(void *rbackend, INT32 bufferId) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	if (bufferId < 0) {
		return FALSE;
	}
	backend->immediateSyncRequest.bufferId = bufferId;
	return backend_write_rds_message(backend,
			OGON_CLIENT_IMMEDIATE_SYNC_REQUEST,
			(ogon_message *)&backend->immediateSyncRequest);
}

static BOOL backend_sbp_reply(void *rbackend, ogon_msg_sbp_reply *msg) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	return backend_write_rds_message(
			backend, OGON_CLIENT_SBP_REPLY, (ogon_message *)msg);
}

static BOOL backend_seat_new(void *rbackend, ogon_msg_seat_new *msg) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	return backend_write_rds_message(
			backend, OGON_CLIENT_SEAT_NEW, (ogon_message *)msg);
}

static BOOL backend_seat_removed(void *rbackend, UINT32 id) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;
	ogon_msg_seat_removed *msg = &backend->seatRemoved;
	msg->clientId = id;
	return backend_write_rds_message(
			backend, OGON_CLIENT_SEAT_REMOVED, (ogon_message *)msg);
}

static void list_dictionary_message_free(void *item) { free(item); }

static BOOL backend_message(void *rbackend, ogon_msg_message *msg) {
	ogon_backend_connection *backend = (ogon_backend_connection *)rbackend;

	auto answer_helper =
			static_cast<message_answer *>(calloc(1, sizeof(message_answer)));
	if (!answer_helper) {
		WLog_ERR(TAG, "failed to allocate message answer");
		return FALSE;
	}
	msg->message_id = backend->next_message_id++;
	answer_helper->message_id = msg->message_id;
	answer_helper->icp_tag = msg->icp_tag;
	answer_helper->icp_type = msg->icp_type;
	if (!ListDictionary_Add(backend->message_answer_list,
				(void *)(intptr_t)msg->message_id, answer_helper)) {
		free(answer_helper);
		WLog_ERR(TAG, "failed to add answer to message answers list");
		return FALSE;
	}

	return backend_write_rds_message(
			backend, OGON_CLIENT_MESSAGE, (ogon_message *)msg);
}

static BOOL backend_send_capabilities(ogon_connection *conn,
		ogon_backend_connection *backend, rdpSettings *settings, UINT32 width,
		UINT32 height) {
	ogon_msg_capabilities *capa = &backend->capabilities;

	capa->desktopWidth = width;
	capa->desktopHeight = height;
	capa->keyboardLayout = settings->KeyboardLayout;
	capa->keyboardType = settings->KeyboardType;
	capa->keyboardSubType = settings->KeyboardSubType;
	capa->clientId = conn->id;

	return backend_write_rds_message(
			conn->backend, OGON_CLIENT_CAPABILITIES, (ogon_message *)capa);
}

BOOL ogon_backend_initialize(ogon_connection *conn,
		ogon_backend_connection *backend, rdpSettings *settings, UINT32 width,
		UINT32 height) {
	return backend_send_capabilities(conn, backend, settings, width, height) &&
		   backend_synchronize_keyboard_event(
				   backend, conn->front.indicators, conn->id);
}

static BOOL ogon_backend_send_version(ogon_backend_connection *backend) {
	ogon_msg_version version;
	version.versionMajor = OGON_PROTOCOL_VERSION_MAJOR;
	version.versionMinor = OGON_PROTOCOL_VERSION_MINOR;
	version.cookie = backend->properties.ogonCookie;

	return backend_write_rds_message(
			backend, OGON_CLIENT_VERSION, (ogon_message *)&version);
}

int ogon_resize_frontend(
		ogon_connection *conn, ogon_backend_connection *backend) {
	freerdp_peer *client = conn->context.peer;
	rdpSettings *settings = conn->context.settings;
	ogon_screen_infos *screenInfos = &backend->screenInfos;
	ogon_front_connection *front = &conn->front;
	BOOL doResize = FALSE;

	if (ogon_state_get(front->state) == OGON_STATE_WAITING_RESIZE) {
		/* peer is already re-sizing save the size
		 * actual re-size will be handled in the next activation */
		front->pendingResizeWidth = screenInfos->width;
		front->pendingResizeHeight = screenInfos->height;
	} else {
		settings->DesktopWidth = screenInfos->width;
		settings->DesktopHeight = screenInfos->height;
		doResize = TRUE;
	}

	if (front->encoder) {
		ogon_bitmap_encoder_free(front->encoder);
		front->encoder = nullptr;
	}

	if (!(front->encoder = ogon_bitmap_encoder_new(screenInfos->width,
				  screenInfos->height, screenInfos->bpp,
				  screenInfos->bytesPerPixel, screenInfos->scanline,
				  settings->ColorDepth, settings->MultifragMaxRequestSize))) {
		WLog_DBG(TAG, "failed to recreate bitmap encoder for connection %ld",
				conn->id);
		freerdp_set_error_info(
				conn->context.rdp, ERRCONNECT_PRE_CONNECT_FAILED);
		ogon_connection_close(conn);
		return -1;
	}

	if (!doResize) {
		return 0;
	}

	/* we can safely start the reactivate sequence */
	client->context->update->DesktopResize(client->context->update->context);
	ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_TRIGGER_RESIZE);
	return 0;
}

static int ogon_server_framebuffer_info(
		ogon_connection *connection, const ogon_message *rawmsg) {
	const ogon_msg_framebuffer_info *msg =
			(const ogon_msg_framebuffer_info *)rawmsg;
	ogon_backend_connection *backend = connection->backend;
	ogon_screen_infos *screenInfos = &backend->screenInfos;
	BOOL newSize;
	BOOL newEncoders;

	backend->waitingSyncReply = FALSE;
	backend->backendVersion = msg->version;
	backend->multiseatCapable = msg->multiseatCapable;

	WLog_DBG(TAG,
			"framebuffer info: message: %" PRIu32 "x%" PRIu32 "@%" PRIu32
			"/%" PRIu32 " scanline=%" PRIu32 " userid=%" PRIu32 "",
			msg->width, msg->height, msg->bitsPerPixel, msg->bytesPerPixel,
			msg->scanline, msg->userId);

	if (!backend->active) {
		WLog_DBG(TAG, "backend is not active: ignoring framebuffer info");
		return 0;
	}

	newSize = (msg->width != screenInfos->width) ||
			  (msg->height != screenInfos->height);

	/**
	 * Note: if newSize is true we will call ogon_resize_frontend() for
	 * all connections which also (re)creates the frontends' encoders.
	 * Otherwise we recreate the encoders here if required.
	 */
	newEncoders = !newSize && (msg->scanline != screenInfos->scanline);

	if (newSize || newEncoders) {
		if (backend->damage) ogon_dmgbuf_free(backend->damage);

		backend->damage =
				ogon_dmgbuf_new(msg->width, msg->height, msg->scanline);
		if (!backend->damage) {
			WLog_ERR(TAG, "Problem creating dmgbuf");
			return -1;
		}
	}

	if (ogon_dmgbuf_set_user(backend->damage, msg->userId) != 0) {
		WLog_ERR(TAG, "Failed to set the userId to the dmgbuf");
		return -1;
	}
	screenInfos->width = msg->width;
	screenInfos->height = msg->height;
	screenInfos->scanline = msg->scanline;

	/* /!\ /!\ /!\ /!\
	 * here we assume that bytesPerPixel and bitsPerPixel are 4 and 32, and we
	 * don't handle changes of these numbers. Any sane backend shouldn't change
	 * the depth in the middle of a session ;) however, we could pass the
	 * current encoder to ogon_bitmap_encoder_new() and do the required checks
	 * there so it could simply return or update the current encoder or create a
	 * new one. this would be the right thing to do because actually only the
	 * bitmap encoder really knows how to handle the framebuffer
	 * properties/parameters
	 */

	screenInfos->bytesPerPixel = msg->bytesPerPixel;
	screenInfos->bpp = msg->bitsPerPixel;

	LinkedList_Enumerator_Reset(connection->frontConnections);
	while (LinkedList_Enumerator_MoveNext(connection->frontConnections)) {
		auto frontConnection = static_cast<ogon_connection *>(
				LinkedList_Enumerator_Current(connection->frontConnections));
		ogon_front_connection *front = &frontConnection->front;
		rdpSettings *settings = frontConnection->context.settings;

		WLog_DBG(TAG,
				"framebuffer info: processing frontConnection %ld newSize=%s",
				frontConnection->id, newSize ? "yes" : "no");

		if (!newSize && (!front->encoder || newEncoders)) {
			if (front->encoder) {
				ogon_bitmap_encoder_free(front->encoder);
				front->encoder = nullptr;
			}

			if (!(front->encoder = ogon_bitmap_encoder_new(screenInfos->width,
						  screenInfos->height, screenInfos->bpp,
						  screenInfos->bytesPerPixel, screenInfos->scanline,
						  settings->ColorDepth,
						  settings->MultifragMaxRequestSize))) {
				WLog_ERR(TAG,
						"failed to (re-)create bitmap encoder for "
						"frontConnection %ld",
						frontConnection->id);
				ogon_connection_close(frontConnection);
				continue;
			}
		}

		ogon_state_set_event(front->state, OGON_EVENT_BACKEND_ATTACHED);
		if (newSize) {
			if (ogon_resize_frontend(frontConnection, backend) < 0) {
				WLog_DBG(TAG, "error resizing connection %ld",
						frontConnection->id);
				ogon_connection_close(frontConnection);
				continue;
			}
		} else {
			handle_wait_timer_state(frontConnection);
		}
	}

	WLog_DBG(TAG,
			"framebuffer info: screen infos: %" PRIu32 "x%" PRIu32 "@%" PRIu32
			"/%" PRIu32 " scanline=%" PRIu32 "",
			screenInfos->width, screenInfos->height, screenInfos->bpp,
			screenInfos->bytesPerPixel, screenInfos->scanline);

	return 0;
}

void ogon_connection_clear_pointer_cache(ogon_connection *connection) {
	UINT32 i;
	rdpSettings *settings = connection->context.settings;
	ogon_pointer_cache_entry *cache = connection->front.pointerCache;

	for (i = 0; i < settings->PointerCacheSize; i++) {
		cache[i].hits = 0;
		cache[i].hash = 0xffffffff;
	}
}

static BOOL ogon_server_set_pointer_cache_index(
		ogon_connection *connection, POINTER_COLOR_UPDATE *p) {
	UINT32 hash = 0;
	UINT32 seed = 0;
	UINT32 carry = 0;
	UINT32 i;
	rdpSettings *settings = connection->context.settings;
	ogon_front_connection *front = &connection->front;
	ogon_pointer_cache_entry *cache = front->pointerCache;

	p->cacheIndex = 0;
	if (!cache || !settings->PointerCacheSize) {
		return FALSE;
	}

	PMurHash32_Process(&seed, &carry, &p->xPos, 4);
	PMurHash32_Process(&seed, &carry, &p->yPos, 4);
	PMurHash32_Process(&seed, &carry, &p->width, 4);
	PMurHash32_Process(&seed, &carry, &p->height, 4);
	PMurHash32_Process(&seed, &carry, &p->lengthAndMask, 4);
	PMurHash32_Process(&seed, &carry, &p->lengthXorMask, 4);
	if (p->lengthAndMask) {
		PMurHash32_Process(&seed, &carry, p->andMaskData, p->lengthAndMask);
	}
	if (p->lengthXorMask) {
		PMurHash32_Process(&seed, &carry, p->xorMaskData, p->lengthXorMask);
	}
	hash = PMurHash32_Result(
			seed, carry, 24 + p->lengthAndMask + p->lengthXorMask);

	for (i = 0; i < settings->PointerCacheSize; i++) {
		if (cache[i].hash == hash) {
			cache[i].hits++;
			p->cacheIndex = i;
			/* WLog_DBG(TAG, "found hash %"PRIu32" at index %"PRIu32"", hash,
			 * p->cacheIndex); */
			return TRUE;
		}
	}

	/* hash not found, overwrite least used slot */
	for (i = 0; i < settings->PointerCacheSize; i++) {
		if (cache[i].hits < cache[p->cacheIndex].hits) {
			p->cacheIndex = i;
		}
		if (cache[p->cacheIndex].hits == 0) {
			break;
		}
	}

	cache[p->cacheIndex].hits = 1;
	cache[p->cacheIndex].hash = hash;

	/* WLog_DBG(TAG, "conn %ld adding hash %"PRIu32" at index %"PRIu32"",
	 * connection->id, hash, p->cacheIndex); */
	return FALSE;
}

static BOOL ogon_new_pointer_to_mono_color_pointer(
		POINTER_NEW_UPDATE *pointerNew, BOOL isRdesktop) {
	POINTER_COLOR_UPDATE *pointerColor = &(pointerNew->colorPtrAttr);

	BYTE *andMsk = pointerColor->andMaskData;
	BYTE *xorSrc = pointerColor->xorMaskData;
	BYTE *xorDst = pointerColor->xorMaskData;
	UINT32 x, y, andMskStride;

	/* currently we only deal with 32bpp sources */
	if (pointerNew->xorBpp != 32) {
		return FALSE;
	}

	andMskStride = ((pointerColor->width + 15) >> 4) * 2;

	if (isRdesktop) {
		/**
		 * rdesktop does not implement the 2-byte boundary AND mask padding
		 * as defined in MS-RDPBCGR 2.2.9.1.1.4.4.
		 * In order to support the legacy clients we have to use the same
		 * incorrect AND mask stride calculation
		 */
		andMskStride = ((pointerColor->width + 7) >> 3);
	}

	pointerNew->xorBpp = 24;
	pointerColor->lengthXorMask =
			pointerColor->width * pointerColor->height * 3;
	pointerColor->lengthAndMask = pointerColor->height * andMskStride;

	memset(andMsk, 0, pointerColor->lengthAndMask);

	for (y = 0; y < pointerColor->height; y++) {
		for (x = 0; x < pointerColor->width; x++, xorDst += 3, xorSrc += 4) {
			BOOL setAndMaskBit = TRUE;
			BYTE colorValue = 0x00;
			BYTE andMskValue;
			int andMskOffset;
			/* if pixel is at least semi tramsparent ... */
			if (xorSrc[3] > 127) {
				/**
				 * ... convert bgr to black/white:
				 * - calculate the the greyscale value (Y)
				 * - if Y is below 128 the pixel is black otherwise it is white
				 * In order to calculate Y we use the the ITU-R BT.601 factors
				 * See
				 * http://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion Y
				 * = 0.299*R + 0.587*G + 0.114*B For better performance we
				 * prevent floating point multiplication and use integer factors
				 * with the maximum possible shift so that the higest possible
				 * result will still fit into 32 bits: 0.299*(1<<24) = 5016388
				 * 0.587*(1<<24) = 9848226
				 * 0.114*(1<<24) = 1912603
				 */
				UINT32 b = xorSrc[0];
				UINT32 g = xorSrc[1];
				UINT32 r = xorSrc[2];
				colorValue =
						((5016388 * r + 9848226 * g + 1912603 * b) >> 24) >> 7
								? 0xFF
								: 0x00;
				setAndMaskBit = FALSE;
			}

			andMskOffset = andMskStride * y + (x >> 3);
			andMskValue = 0x80 >> (x & 7);
			if (setAndMaskBit) {
				andMsk[andMskOffset] |= andMskValue;
			} else {
				andMsk[andMskOffset] &= ~andMskValue;
			}

			memset(xorDst, colorValue, 3);
		}
	}

	return TRUE;
}

void ogon_connection_set_pointer(
		ogon_connection *connection, const ogon_msg_set_pointer *msg) {
	POINTER_CACHED_UPDATE pointerCached = {0};
	POINTER_NEW_UPDATE pointerNew = {0};
	POINTER_COLOR_UPDATE *pointerColor = &(pointerNew.colorPtrAttr);
	BOOL isRdesktop = FALSE;
	char *clientProductId = connection->context.settings->ClientProductId;
	rdpPointerUpdate *pointer =
			connection->context.peer->context->update->pointer;

	/**
	 * Note: As msg values got already validated in ogon_server_set_pointer()
	 * there is no need to reverify them here
	 */

	pointerColor->cacheIndex = 0;
	pointerColor->xPos = msg->xPos;
	pointerColor->yPos = msg->yPos;
	pointerColor->width = msg->width;
	pointerColor->height = msg->height;
	pointerColor->lengthAndMask = msg->lengthAndMask;
	pointerColor->lengthXorMask = msg->lengthXorMask;
	pointerColor->xorMaskData = msg->xorMaskData;
	pointerColor->andMaskData = msg->andMaskData;
	pointerNew.xorBpp = msg->xorBpp;

	/**
	 * At a minimum the Color Pointer Update MUST be supported by any
	 * RDP client. Only very old clients lack support for the New Pointer
	 * update (RDPBCGR 2.2.9.1.1.4.5). This is indicated by a zero value
	 * of the PointerCacheSize setting.
	 * In addition the majority of the clients in the field that lack
	 * support for the New Pointer Update also seem to be limited to
	 * monochrome pointer support.
	 *
	 * Some newer rdesktop clients do announce the New Pointer Update
	 * capability but their decoder does not seem to produce an acceptable
	 * result for 32-bpp pointer data.
	 * For now we enforce monochrome color conversion if FreeRDP has detected
	 * a rdesktop client.
	 *
	 * In addition rdesktop also does not implement the 2-byte boundary
	 * AND mask padding required by MS-RDPBCGR 2.2.9.1.1.4.4 which results
	 * in garbled pointer shapes depending on the width.
	 * ogon_new_pointer_to_mono_color_pointer() has a workaround for
	 * this issue.
	 */

	if (clientProductId && !strcmp(clientProductId, "rdesktop")) {
		isRdesktop = TRUE;
	}

	if (isRdesktop || !connection->context.settings->PointerCacheSize) {
		ogon_new_pointer_to_mono_color_pointer(&pointerNew, isRdesktop);
		if (pointerNew.xorBpp == 24) {
			IFCALL(pointer->PointerColor, &connection->context, pointerColor);
		}
		return;
	}

	if (!ogon_server_set_pointer_cache_index(connection, pointerColor)) {
		IFCALL(pointer->PointerNew, &connection->context, &pointerNew);
		return;
	}

	pointerCached.cacheIndex = pointerColor->cacheIndex;
	IFCALL(pointer->PointerCached, &connection->context, &pointerCached);
}

static int ogon_server_set_pointer(
		ogon_connection *connection, const ogon_message *rawmsg) {
	const ogon_msg_set_pointer *msg = (const ogon_msg_set_pointer *)rawmsg;
	ogon_backend_connection *backend = connection->backend;
	ogon_msg_set_pointer *last = &backend->lastSetPointer;

	/* basic verification of message values */
	if (!msg->width || msg->width > 96 || !msg->height || msg->height > 96) {
		WLog_ERR(TAG, "invalid pointer size: %" PRIu32 "x%" PRIu32 "",
				msg->width, msg->height);
		return -1;
	}
	/**
	 * Note: The MSDN CreateCursor page does not define valid ranges for
	 * xHotSpot and yHotSpot (which are this msg's xPos/yPos values)
	 * However, XCURSOR(3) says that the values must be less than *or equal*
	 * to the cursor's width/height
	 */
	if (msg->xPos > msg->width || msg->yPos > msg->height) {
		WLog_ERR(TAG,
				"invalid pointer hotspot: %" PRIu32 "x%" PRIu32 " (%" PRIu32
				" x %" PRIu32 ")",
				msg->xPos, msg->yPos, msg->width, msg->height);
		return -1;
	}
	if (msg->xorBpp != 24 && msg->xorBpp != 32) {
		WLog_ERR(TAG, "unsupported pointer color depth: %" PRIu32 "",
				msg->xorBpp);
		return -1;
	}
	if (!msg->lengthXorMask || msg->lengthXorMask > (96 * 96 * 4)) {
		WLog_ERR(TAG, "pointer with invalid XOR mask length: %" PRIu32 "",
				msg->lengthXorMask);
		return -1;
	}
	if (!msg->lengthAndMask || msg->lengthAndMask > (96 * 96 / 8)) {
		WLog_ERR(TAG, "pointer with invalid AND mask length: %" PRIu32 "",
				msg->lengthAndMask);
		return -1;
	}
	if (!msg->andMaskData || !msg->xorMaskData) {
		WLog_ERR(TAG, "pointer is missing AND/XOR mask data");
		return -1;
	}

	/* copy these informations so that we can restore the last pointer */
	backend->haveBackendPointer = TRUE;
	backend->lastSetSystemPointer = SYSPTR_DEFAULT;
	last->width = msg->width;
	last->height = msg->height;
	last->xPos = msg->xPos;
	last->yPos = msg->yPos;
	last->xorBpp = msg->xorBpp;

	auto newAlloc =
			static_cast<BYTE *>(realloc(last->xorMaskData, msg->lengthXorMask));
	if (!newAlloc) {
		WLog_ERR(TAG, "failed to realloc xorMask");
		return -1;
	}
	last->lengthXorMask = msg->lengthXorMask;
	last->xorMaskData = newAlloc;
	memcpy(newAlloc, msg->xorMaskData, msg->lengthXorMask);

	newAlloc =
			static_cast<BYTE *>(realloc(last->andMaskData, msg->lengthAndMask));
	if (!newAlloc) {
		WLog_ERR(TAG, "failed to realloc andMask");
		return -1;
	}
	last->lengthAndMask = msg->lengthAndMask;
	last->andMaskData = newAlloc;
	memcpy(newAlloc, msg->andMaskData, msg->lengthAndMask);

	if (!backend->active) {
		WLog_ERR(TAG, "not treating pointer as backend is not active");
		return 0;
	}

	WLog_DBG(TAG, "(%ld) set_pointer_shape message for %" PRIu32 "",
			connection->id, msg->clientId);

	/* then broadcast the new pointer to all target front connections */
	LinkedList_Enumerator_Reset(connection->frontConnections);
	while (LinkedList_Enumerator_MoveNext(connection->frontConnections)) {
		auto frontConnection = static_cast<ogon_connection *>(
				LinkedList_Enumerator_Current(connection->frontConnections));

		if (!msg->clientId || (frontConnection->id == msg->clientId)) {
			ogon_connection_set_pointer(frontConnection, msg);
		}
	}

	return 0;
}

static int ogon_server_set_system_pointer(
		ogon_connection *connection, const ogon_message *rawmsg) {
	const ogon_msg_set_system_pointer *msg =
			(const ogon_msg_set_system_pointer *)rawmsg;
	ogon_backend_connection *backend = connection->backend;

	backend->lastSetSystemPointer = msg->ptrType;
	if (msg->ptrType == SYSPTR_NULL) backend->haveBackendPointer = FALSE;

	if (!backend->active) {
		WLog_ERR(TAG,
				"not treating set system pointer as backend is not active");
		return 0;
	}

	WLog_DBG(TAG,
			"(%ld) set_system_pointer message type %" PRIu32 " to %" PRIu32 "",
			connection->id, msg->ptrType, msg->clientId);

	LinkedList_Enumerator_Reset(connection->frontConnections);
	while (LinkedList_Enumerator_MoveNext(connection->frontConnections)) {
		auto frontConnection = static_cast<ogon_connection *>(
				LinkedList_Enumerator_Current(connection->frontConnections));

		if (!msg->clientId || (frontConnection->id == msg->clientId)) {
			POINTER_SYSTEM_UPDATE pointer_system = {0};
			rdpPointerUpdate *pointer =
					frontConnection->context.peer->context->update->pointer;

			pointer_system.type = msg->ptrType;
			IFCALL(pointer->PointerSystem, &frontConnection->context,
					&pointer_system);
		}
	}

	return 0;
}

static int ogon_server_beep(
		ogon_connection *connection, const ogon_message *rawmsg) {
	const ogon_msg_beep *msg = (const ogon_msg_beep *)rawmsg;
	ogon_backend_connection *backend = connection->backend;

	if (!backend->active) {
		WLog_ERR(TAG, "not treating server beep as backend is not active");
		return 0;
	}

	LinkedList_Enumerator_Reset(connection->frontConnections);
	while (LinkedList_Enumerator_MoveNext(connection->frontConnections)) {
		PLAY_SOUND_UPDATE playSound = {0};
		auto frontConnection = static_cast<ogon_connection *>(
				LinkedList_Enumerator_Current(connection->frontConnections));
		pPlaySound playSoundCall =
				frontConnection->context.peer->context->update->PlaySound;

		playSound.duration = msg->duration;
		playSound.frequency = msg->frequency;
		IFCALL(playSoundCall, &frontConnection->context, &playSound);
	}

	return 0;
}

/* @brief contextual data for an SBP request */
typedef struct _sbp_context {
	LONG connectionId;
	UINT32 originalTag;
	UINT32 originalType;
} sbp_context;

static sbp_context *SbpContext_new(
		ogon_connection *connection, UINT32 tag, UINT32 type) {
	sbp_context *ret = (sbp_context *)malloc(sizeof(sbp_context));
	if (!ret) {
		return nullptr;
	}
	ret->connectionId = connection->id;
	ret->originalTag = tag;
	ret->originalType = type;
	return ret;
}

static void sbpCallback(
		UINT32 reason, Ogon__Pbrpc__RPCBase *response, void *args) {
	sbp_context *context = (sbp_context *)args;

	auto event = static_cast<rds_notification_sbp *>(
			calloc(1, sizeof(rds_notification_sbp)));
	if (!event) {
		WLog_ERR(TAG, "sbp callback: unable to allocate message");
		goto cleanup_exit;
	}

	event->reply.tag = context->originalTag;
	event->reply.sbpType =
			context->originalType;	// TODO: check if it's really sbpType

	switch (reason) {
		case PBRPC_SUCCESS:
			if (!response) {
				WLog_ERR(TAG, "No response data");
				free(event);
				goto cleanup_exit;
			}

			event->reply.status = SBP_REPLY_SUCCESS;
			event->reply.sbpType = response->msgtype;
			event->reply.dataLen = response->payload.len;
			event->reply.data = (char *)response->payload.data;
			break;

		case PBRCP_TRANSPORT_ERROR:
		default:
			if (response) {
				event->reply.sbpType = response->msgtype;
			} else {
				event->reply.sbpType = context->originalType;
			}

			event->reply.status = SBP_REPLY_TRANSPORT_ERROR;
			break;
	}

	if (!app_context_post_message_connection(
				context->connectionId, NOTIFY_SBP_REPLY, event, nullptr)) {
		WLog_ERR(TAG, "sbp callback: error posting to connection %" PRId32 "",
				context->connectionId);
		pbrpc_message_free_response(response);
		free(event);
		goto cleanup_exit;
	}
	pbrpc_message_free(response, FALSE);

cleanup_exit:
	free(context);
}

static int ogon_server_sbp_request(
		ogon_connection *connection, const ogon_message *rawmsg) {
	const ogon_msg_sbp_request *msg = (const ogon_msg_sbp_request *)rawmsg;
	sbp_context *sbpContext;
	pbRPCContext *pbContext;
	pbRPCPayload payload;

	if (!(pbContext = (pbRPCContext *)ogon_icp_get_context())) {
		WLog_ERR(TAG, "failed to get pbrpc context");
		return 0;
	}

	if (!(sbpContext = SbpContext_new(connection, msg->tag, msg->sbpType))) {
		WLog_ERR(TAG, "failed to create sbp context");
		return 0;
	}

	payload.data = msg->data;
	payload.dataLen = msg->dataLen;
	payload.errorDescription = 0;

	pbrcp_call_method_async(
			pbContext, msg->sbpType, &payload, sbpCallback, (void *)sbpContext);
	return 0;
}

static int ogon_server_framebuffer_sync_reply(
		ogon_connection *connection, const ogon_message *rawmsg) {
	const ogon_msg_framebuffer_sync_reply *msg =
			(const ogon_msg_framebuffer_sync_reply *)rawmsg;
	ogon_backend_connection *backend = connection->shadowing->backend;

	if (!connection->backend->active) {
		WLog_ERR(TAG, "not treating sync reply as backend is not active");
		return 0;
	}

	if (msg->bufferId != ogon_dmgbuf_get_id(backend->damage)) {
		WLog_ERR(TAG,
				"sync reply for connection %ld: unknown(old) bufferId %" PRId32
				"",
				connection->id, msg->bufferId);
		return 0;
	}

	return frontend_handle_sync_reply(connection);
}

static int ogon_server_message_reply(
		ogon_connection *connection, const ogon_message *rawmsg) {
	const ogon_msg_message_reply *msg = (const ogon_msg_message_reply *)rawmsg;
	int error = 0;
	message_answer *answer_helper;
	ogon_backend_connection *backend = connection->backend;
	if (ListDictionary_Contains(backend->message_answer_list,
				(void *)(intptr_t)msg->message_id)) {
		answer_helper = (message_answer *)ListDictionary_GetItemValue(
				backend->message_answer_list,
				(void *)(intptr_t)msg->message_id);
		error = ogon_icp_sendResponse(answer_helper->icp_tag,
				answer_helper->icp_type, 0, TRUE,
				(void *)(intptr_t)msg->result);
		if (error != 0) {
			WLog_ERR(TAG, "error sending icp response");
			return 1;
		}
		return 0;
	}
	return 1;
}

static const backend_server_protocol_cb serverCallbacks[] = {
		ogon_server_set_pointer,		/*  0 - OGON_SERVER_SET_POINTER */
		ogon_server_framebuffer_info,	/*  1 - OGON_SERVER_FRAMEBUFFER_INFO */
		ogon_server_beep,				/*  2 - OGON_SERVER_BEEP */
		ogon_server_set_system_pointer, /*  3 - OGON_SERVER_SET_SYSTEM_POINTER
										 */
		ogon_server_sbp_request,		/*  4 - OGON_SERVER_SBP_REQUEST */
		ogon_server_framebuffer_sync_reply, /*  5 -
											   OGON_SERVER_FRAMEBUFFER_SYNC_REPLY
											 */
		ogon_server_message_reply,			/*  6 - OGON_SERVER_MESSAGE_REPLY */
		nullptr,							/*  7 - OGON_SERVER_VERSION_REPLY */
};

#define SERVER_CALLBACKS_NB \
	(sizeof(serverCallbacks) / sizeof(backend_server_protocol_cb))

static BOOL backend_treat_message(
		ogon_connection *connection, wStream *s, UINT16 type) {
	ogon_msg_version *version = nullptr;
	backend_server_protocol_cb cb;
	ogon_backend_connection *backend = connection->backend;
	BOOL ret = FALSE;

	if (!ogon_message_read(s, type, &backend->currentInMessage)) {
		WLog_ERR(TAG,
				"error treating message: failed to read server message type "
				"%" PRIu16 "",
				type);
		Stream_SetPosition(s, 0);
		winpr_HexDump(TAG, WLOG_ERROR, Stream_Buffer(s), Stream_Length(s));
		return FALSE;
	}

	if (type >= SERVER_CALLBACKS_NB) {
		WLog_ERR(TAG,
				"error treating message: invalid message type %" PRIu16 "",
				type);
		goto out;
	}

	if (type == OGON_SERVER_VERSION_REPLY) {
		version = &backend->currentInMessage.version;
		if (version->versionMajor != OGON_PROTOCOL_VERSION_MAJOR) {
			WLog_ERR(TAG,
					"error treating message: received protocol version info "
					"with %" PRIu32 ".%" PRIu32
					" but own protocol version is %" PRIu32 ".%" PRIu32 "",
					version->versionMajor, version->versionMinor,
					OGON_PROTOCOL_VERSION_MAJOR, OGON_PROTOCOL_VERSION_MINOR);
			goto out;
		}

		if (!version->cookie) {
			WLog_ERR(TAG, "no cookie to match %s",
					backend->properties.backendCookie);
			goto out;
		}

		if (strcmp(version->cookie, backend->properties.backendCookie)) {
			WLog_ERR(TAG, "backend's cookie doesn't match, closing connection");
			goto out;
		}

		backend->version_exchanged = TRUE;
		ret = TRUE;
		goto out;
	}

	if (!backend->version_exchanged) {
		WLog_ERR(TAG,
				"error treating message: no version exchanged, disconnecting "
				"backend");
		goto out;
	}

	cb = backend->server[type];
	if (!cb) {
		WLog_ERR(TAG,
				"error treating message: message %" PRIu16
				" (%s) not implemented yet",
				type, ogon_message_name(type));
		goto out;
	}

	if (cb(connection, &backend->currentInMessage) < 0) {
		WLog_ERR(TAG, "error treating message %" PRIu16 " (%s)", type,
				ogon_message_name(type));
	} else {
		ret = TRUE;
	}

out:
	ogon_message_free(type, &backend->currentInMessage, TRUE);
	return ret;
}

static BOOL backend_drain_output(ogon_backend_connection *backend) {
	int mask = OGON_EVENTLOOP_READ;

	if (backend->writeReady && ringbuffer_used(&backend->xmitBuffer)) {
		if (!drain_ringbuffer_to_pipe(&backend->xmitBuffer, backend->pipe,
					&backend->writeReady)) {
			return FALSE;
		}
	}

	if (!backend->writeReady) {
		mask |= OGON_EVENTLOOP_WRITE;
	}

	if ((eventsource_mask(backend->pipeEventSource) != mask) &&
			!eventsource_change_source(backend->pipeEventSource, mask)) {
		WLog_ERR(TAG, "drain output: unable to change eventSource mask");
		return FALSE;
	}

	return TRUE;
}

static BOOL backend_drain_input(ogon_connection *connection) {
	UINT32 packetLen;
	DWORD readBytes;
	ogon_backend_connection *backend = connection->backend;

	while (TRUE) {
		if (!ReadFile(backend->pipe, Stream_Pointer(backend->recvBuffer),
					backend->expectedReadBytes, &readBytes, nullptr) ||
				!readBytes) {
			if (GetLastError() == ERROR_NO_DATA) break;

			WLog_DBG(TAG, "error during ReadFile(handle=%p toRead=%" PRIu32 ")",
					backend->pipe, backend->expectedReadBytes);
			return FALSE;
		}

#if 0
		WLog_DBG(TAG, "wanted %"PRIu32" and had %"PRIu32"", backend->expectedReadBytes, readBytes);
		winpr_HexDump(TAG, WLOG_DEBUG, Stream_Pointer(backend->recvBuffer), readBytes);
#endif

		backend->expectedReadBytes -= readBytes;
		Stream_Seek(backend->recvBuffer, readBytes);

		if (backend->expectedReadBytes) {
			continue;
		}

		if (backend->stateWaitingHeader) {
			Stream_SetPosition(backend->recvBuffer, 0);
			ogon_read_message_header(backend->recvBuffer,
					&backend->currentInMessageType, &packetLen);
			backend->stateWaitingHeader = FALSE;
			backend->expectedReadBytes = packetLen;

			Stream_SetPosition(backend->recvBuffer, 0);

			if (backend->expectedReadBytes) {
				if (!Stream_EnsureRemainingCapacity(
							backend->recvBuffer, packetLen)) {
					WLog_ERR(TAG, "unable to grow incoming buffer");
					return FALSE;
				}
				continue;
			}
		}

		Stream_SealLength(backend->recvBuffer);
		Stream_SetPosition(backend->recvBuffer, 0);

		/* WLog_DBG(TAG, "drain input: treating message type %"PRIu16" ...",
		 * backend->currentInMessageType); */
		if (!backend_treat_message(connection, backend->recvBuffer,
					backend->currentInMessageType)) {
			WLog_ERR(TAG, "error treating message type %" PRIu16 "",
					backend->currentInMessageType);
			return FALSE;
		}

		Stream_SetPosition(backend->recvBuffer, 0);
		backend->expectedReadBytes = RDS_ORDER_HEADER_LENGTH;
		backend->stateWaitingHeader = TRUE;
	}

	return TRUE;
}

/* event loop callback for the content provider pipe */
static int handle_pipe_bytes(int mask, int fd, HANDLE handle, void *data) {
	OGON_UNUSED(fd);
	OGON_UNUSED(handle);
	ogon_connection *connection = (ogon_connection *)data;
	ogon_backend_connection *backend = connection->backend;

	if (mask & OGON_EVENTLOOP_WRITE) {
		backend->writeReady = TRUE;
		if (!backend_drain_output(backend)) {
			ogon_connection_close(connection);
		}
	}

	if (mask & OGON_EVENTLOOP_READ) {
		if (!backend_drain_input(connection)) {
			ogon_connection_close(connection);
		}
	}

	return 0;
}

ogon_backend_connection *backend_new(
		ogon_connection *conn, ogon_backend_props *props) {
	rdpSettings *settings = conn->context.settings;
	ogon_client_interface *client;
	DWORD pipeMode;

	auto ret = new (ogon_backend_connection);
	if (!ret) {
		goto fail;
	}

	client = &ret->client;
	client->SynchronizeKeyboardEvent = backend_synchronize_keyboard_event;
	client->ScancodeKeyboardEvent = backend_scancode_keyboard_event;
	client->UnicodeKeyboardEvent = backend_unicode_keyboard_event;
	client->MouseEvent = backend_mouse_event;
	client->ExtendedMouseEvent = backend_extended_mouse_event;
	client->FramebufferSyncRequest = backend_framebuffer_sync_request;
	client->Sbp = backend_sbp_reply;
	client->ImmediateSyncRequest = backend_immediate_sync_request;
	client->SeatNew = backend_seat_new;
	client->SeatRemoved = backend_seat_removed;
	client->Message = backend_message;

	ret->server = serverCallbacks;

	ret->writeReady = TRUE;
	ret->stateWaitingHeader = TRUE;
	ret->expectedReadBytes = RDS_ORDER_HEADER_LENGTH;
	ret->screenInfos.width = settings->DesktopWidth;
	ret->screenInfos.height = settings->DesktopHeight;
	ret->active = TRUE;
	ret->lastSetSystemPointer = SYSPTR_DEFAULT;
	ret->haveBackendPointer = FALSE;

	if (!ringbuffer_init(&ret->xmitBuffer, 0x10000)) {
		goto fail;
	}

	ret->recvBuffer = Stream_New(nullptr, 0x10000);
	if (!ret->recvBuffer) {
		goto fail;
	}

	ret->pipe = ogon_named_pipe_connect(props->serviceEndpoint, 20);
	if (ret->pipe == INVALID_HANDLE_VALUE) {
		WLog_ERR(TAG, "failed to connect to named pipe [%s]",
				props->serviceEndpoint);
		goto fail;
	}

	WLog_DBG(TAG, "connected to endpoint [%s]", props->serviceEndpoint);

	pipeMode = PIPE_NOWAIT;
	if (!SetNamedPipeHandleState(ret->pipe, &pipeMode, nullptr, nullptr)) {
		WLog_ERR(TAG, "unable to set [%s] pipe non-blocking",
				props->serviceEndpoint);
		goto fail;
	}

	ret->pipeEventSource = eventloop_add_handle(conn->runloop->evloop,
			OGON_EVENTLOOP_READ, ret->pipe, handle_pipe_bytes, conn);
	if (!ret->pipeEventSource) {
		WLog_ERR(TAG, "error adding endpoint pipe handle to event loop");
		goto fail;
	}

	if (!(ret->message_answer_list = ListDictionary_New(FALSE))) {
		WLog_ERR(TAG, "error creating message_answer_list");
		goto fail;
	}
	ret->message_answer_list->objectValue.fnObjectFree =
			list_dictionary_message_free;

	ret->next_message_id = 1;

	ret->properties = *props;
	/* the backend now owns the strings in properties */
	props->serviceEndpoint = nullptr;
	props->backendCookie = nullptr;
	props->ogonCookie = nullptr;

	if (!ogon_backend_send_version(ret)) {
		WLog_ERR(TAG, "error sending version packet over [%s]",
				props->serviceEndpoint);
		goto fail;
	}
	ret->version_exchanged = FALSE;

	return ret;

fail:

	backend_destroy(&ret);
	return nullptr;
}

void backend_destroy(ogon_backend_connection **backendP) {
	ogon_backend_connection *backend = *backendP;
	if (!backend) return;

	ogon_backend_props_free(&backend->properties);
	if (backend->pipeEventSource)
		eventloop_remove_source(&backend->pipeEventSource);
	CloseHandle(backend->pipe);
	backend->pipe = nullptr;

	Stream_Free(backend->recvBuffer, TRUE);
	ringbuffer_destroy(&backend->xmitBuffer);
	ogon_dmgbuf_free(backend->damage);
	ListDictionary_Clear(backend->message_answer_list);
	ListDictionary_Free(backend->message_answer_list);
	free(backend->lastSetPointer.andMaskData);
	free(backend->lastSetPointer.xorMaskData);
	free(backend);

	*backendP = nullptr;
}
