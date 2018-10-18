/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Server Graphics Pipeline Virtual Channel
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Norbert Federa <norbert.federa@thincast.com>
 * Vic Lee <llyzs.vic@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/stream.h>
#include <winpr/sysinfo.h>

#include "../common/global.h"

#include "rdpgfx.h"

#define TAG OGON_TAG("core.egfx")

BOOL WTSVirtualChannelWrite(HANDLE ch, PCHAR data, ULONG length, PULONG pWritten) {
	if (!virtual_manager_write_internal_virtual_channel((internal_virtual_channel *)ch, (BYTE*)data, length, pWritten)) {
		WLog_ERR("%s: failed to write to internal channel", __FUNCTION__);
		return FALSE;
	}
	return TRUE;
}

#define RDPGFX_MAX_SEGMENT_LENGTH 65535

#define RDPGFX_SINGLE 0xE0
#define RDPGFX_MULTIPART 0xE1

#define RDPGFX_WIRETOSURFACE_1_HEADER_SIZE 25
#define RDPGFX_WIRETOSURFACE_2_HEADER_SIZE 21

#ifndef RDPGFX_CAPVERSION_104
#define RDPGFX_CAPVERSION_104 0x000A0400
#endif

static BOOL rdpgfx_server_send_capabilities(rdpgfx_server_context* rdpgfx, wStream *s) {

	Stream_SetPosition(s, 0);

	if (!Stream_EnsureRemainingCapacity(s, 22)) {
		WLog_ERR(TAG, "unable to realloc stream");
		return FALSE;
	}

	/**
	 * [MS-RDPEGFX] 1.7 Versioning and Capability Negotiation
	 * The client advertises supported capability sets from section 2.2.3 in an
	 * RDPGFX_CAPS_ADVERTISE_PDU message.
	 * In response, the server selects _ONE_ of these sets and then sends an
	 * RDPGFX_CAPS_CONFIRM_PDU message to the client containing the selected set.
	 */

	Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
	Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

	Stream_Write_UINT16(s, RDPGFX_CMDID_CAPSCONFIRM); /* RDPGFX_HEADER.cmdId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
	Stream_Write_UINT32(s, 20); /* RDPGFX_HEADER.pduLength (4 bytes) */
	Stream_Write_UINT32(s, rdpgfx->version); /* RDPGFX_CAPSET.version (4 bytes) */
	Stream_Write_UINT32(s, 4); /* RDPGFX_CAPSET.capsDataLength (4 bytes) */
	Stream_Write_UINT32(s, rdpgfx->flags);

	return WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s),
		(ULONG) Stream_GetPosition(s), NULL);
}

static BOOL rdpgfx_server_recv_capabilities(rdpgfx_server_context *rdpgfx, wStream *s,
	UINT32 length)
{
	UINT16 i;
	UINT32 flags;
	UINT32 version;
	UINT16 capsSetCount;
	UINT32 capsDataLength;

	if (length < 2) {
		return FALSE;
	}
	Stream_Read_UINT16(s, capsSetCount);
	length -= 2;

	for (i = 0; i < capsSetCount; i++) {
		if (length < 8) {
			return FALSE;
		}
		Stream_Read_UINT32(s, version);
		Stream_Read_UINT32(s, capsDataLength);
		length -= 8;
		if (length < capsDataLength) {
			return FALSE;
		}
		if (capsDataLength >= 4) {
			Stream_Read_UINT32(s, flags);
		} else {
			flags = 0;
		}
		length -= capsDataLength;

#if 0
		switch (version) {
			case RDPGFX_CAPVERSION_8:
				WLog_DBG(TAG, "rdpgfx: RDPGFX_CAPSET_VERSION8   (0x%08"PRIX32") flags=0x%08"PRIX32"", version, flags);
				WLog_DBG(TAG, "rdpgfx: - THIN-CLIENT    = %s", flags & RDPGFX_CAPS_FLAG_THINCLIENT  ? "yes" : "no");
				WLog_DBG(TAG, "rdpgfx: - SMALL_CACHE    = %s", flags & RDPGFX_CAPS_FLAG_SMALL_CACHE  ? "yes" : "no");
				break;
			case RDPGFX_CAPVERSION_81:
				WLog_DBG(TAG, "rdpgfx: RDPGFX_CAPSET_VERSION81  (0x%08"PRIX32") flags=0x%08"PRIX32"", version, flags);
				WLog_DBG(TAG, "rdpgfx: - THIN-CLIENT    = %s", flags & RDPGFX_CAPS_FLAG_THINCLIENT  ? "yes" : "no");
				WLog_DBG(TAG, "rdpgfx: - SMALL_CACHE    = %s", flags & RDPGFX_CAPS_FLAG_SMALL_CACHE  ? "yes" : "no");
				WLog_DBG(TAG, "rdpgfx: - AVC420_ENABLED = %s", flags & RDPGFX_CAPS_FLAG_AVC420_ENABLED  ? "yes" : "no");
				/**
				 * The flag RDPGFX_CAPS_FLAG_AVC420_ENABLED indicates that the usage of the MPEG-4 AVC/H.264 Codec in
				 * YUV420p mode is supported in the RDPGFX_WIRE_TO_SURFACE_PDU_1 message.
				 */
				break;
			case RDPGFX_CAPVERSION_10:
				WLog_DBG(TAG, "rdpgfx: RDPGFX_CAPSET_VERSION10  (0x%08"PRIX32") flags=0x%08"PRIX32"", version, flags);
				WLog_DBG(TAG, "rdpgfx: - SMALL_CACHE    = %s", flags & RDPGFX_CAPS_FLAG_SMALL_CACHE  ? "yes" : "no");
				WLog_DBG(TAG, "rdpgfx: - AVC_DISABLED   = %s", flags & RDPGFX_CAPS_FLAG_AVC_DISABLED  ? "yes" : "no");
				/**
				 * If the RDPGFX_CAPS_FLAG_AVC_DISABLED flag is not set, the client MUST be capable of processing the
				 * MPEG-4 AVC/H.264 Codec in YUV444 mode in the RDPGFX_WIRE_TO_SURFACE_PDU_1 message.
				 */
				break;
			case RDPGFX_CAPVERSION_101:
				WLog_DBG(TAG, "rdpgfx: RDPGFX_CAPSET_VERSION101 (0x%08"PRIX32") flags=0x%08"PRIX32"", version, flags);
				/**
				 * No flags. However, according to [MS-RDPEGFX 1.7], usage of the MPEG-4 AVC/H.264 Codec in YUV444v2
				 * mode is implied by the RDPGFX_CAPSET_VERSION101 structure.
				 */
				break;
			case RDPGFX_CAPVERSION_102:
				WLog_DBG(TAG, "rdpgfx: RDPGFX_CAPSET_VERSION102 (0x%08"PRIX32") flags=0x%08"PRIX32"", version, flags);
				WLog_DBG(TAG, "rdpgfx: - SMALL_CACHE    = %s", flags & RDPGFX_CAPS_FLAG_SMALL_CACHE  ? "yes" : "no");
				WLog_DBG(TAG, "rdpgfx: - AVC_DISABLED   = %s", flags & RDPGFX_CAPS_FLAG_AVC_DISABLED  ? "yes" : "no");
				/**
				 * If the RDPGFX_CAPS_FLAG_AVC_DISABLED flag is not set, the client MUST be capable of processing the
				 * MPEG-4 AVC/H.264 Codec in YUV444 mode in the RDPGFX_WIRE_TO_SURFACE_PDU_1 message.
				 */
				break;
			case RDPGFX_CAPVERSION_103:
				WLog_DBG(TAG, "rdpgfx: RDPGFX_CAPSET_VERSION103 (0x%08"PRIX32") flags=0x%08"PRIX32"", version, flags);
				WLog_DBG(TAG, "rdpgfx: - AVC_DISABLED   = %s", flags & RDPGFX_CAPS_FLAG_AVC_DISABLED  ? "yes" : "no");
				/**
				 * If the RDPGFX_CAPS_FLAG_AVC_DISABLED flag is not set, the client MUST be capable of processing the
				 * MPEG-4 AVC/H.264 Codec in YUV444 mode in the RDPGFX_WIRE_TO_SURFACE_PDU_1 message.
				 */
				break;
			case RDPGFX_CAPVERSION_104:
				WLog_DBG(TAG, "rdpgfx: RDPGFX_CAPSET_VERSION104 (0x%08"PRIX32") flags=0x%08"PRIX32"", version, flags);
				WLog_DBG(TAG, "rdpgfx: - SMALL_CACHE    = %s", flags & RDPGFX_CAPS_FLAG_SMALL_CACHE  ? "yes" : "no");
				WLog_DBG(TAG, "rdpgfx: - AVC_DISABLED   = %s", flags & RDPGFX_CAPS_FLAG_AVC_DISABLED  ? "yes" : "no");
				/**
				 * If the RDPGFX_CAPS_FLAG_AVC_DISABLED flag is not set, the client MUST be capable of processing:
				 * 1. The MPEG-4 AVC/H.264 Codec in YUV444 mode in the RDPGFX_WIRE_TO_SURFACE_PDU_1 message.
				 * 2. The MPEG-4 AVC/H.264 Codec in YUV420 mode in the RDPGFX_WIRE_TO_SURFACE_PDU_1 message in the
				 *    same frame as other codecs.
				 */
				break;
		}
#endif

		if (version >= RDPGFX_CAPVERSION_8 && version <= RDPGFX_CAPVERSION_104  && version > rdpgfx->version) {
			switch (version) {
				case RDPGFX_CAPVERSION_8:
					break;
				case RDPGFX_CAPVERSION_81:
					rdpgfx->h264Supported = (flags & RDPGFX_CAPS_FLAG_AVC420_ENABLED);
					rdpgfx->avc444Supported = FALSE;
					break;
				case RDPGFX_CAPVERSION_10:
					rdpgfx->avc444Supported = !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
					rdpgfx->h264Supported = rdpgfx->avc444Supported;
					rdpgfx->avc444v2Supported  = FALSE;
					break;
				case RDPGFX_CAPVERSION_102:
				case RDPGFX_CAPVERSION_103:
				case RDPGFX_CAPVERSION_104:
					rdpgfx->avc444Supported = !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
					rdpgfx->h264Supported = rdpgfx->avc444Supported;
					rdpgfx->avc444v2Supported  = rdpgfx->avc444Supported;
					break;

				default:
					WLog_DBG(TAG, "rdpgfx: ignoring unsupported capset version 0x%08"PRIX32" flags=0x%08"PRIX32"", version, flags);
			}
			rdpgfx->version = version;
			rdpgfx->flags = flags;
		}
	}

	if (rdpgfx->version == 0) {
		return FALSE;
	}

	return TRUE;
}

static BOOL rdpgfx_server_recv_frameack(rdpgfx_server_context *rdpgfx, wStream *s, UINT32 length) {
	RDPGFX_FRAME_ACKNOWLEDGE_PDU frame_acknowledge;

	if (length < 12) {
		return FALSE;
	}

	Stream_Read_UINT32(s, frame_acknowledge.queueDepth);
	Stream_Read_UINT32(s, frame_acknowledge.frameId);
	Stream_Read_UINT32(s, frame_acknowledge.totalFramesDecoded);

	IFCALL(rdpgfx->FrameAcknowledge, rdpgfx, &frame_acknowledge);

	return TRUE;
}

static BOOL rdpgfx_server_ogon_deleted_callback(void *context, HRESULT creationStatus) {
	rdpgfx_server_context *rdpgfx = context;

	/* The ogon VCM deleted the channel, possible reasons:
	 *   - the client reported a negative dynamic channel creation status
	 *   - the client closed the channel
	 *   - the client disconnected
	 */

	rdpgfx->rdpgfx_channel = NULL;

	switch (creationStatus)
	{
	case STATUS_REMOTE_DISCONNECT:
		WLog_DBG(TAG, "rdpgfx dynamic channel deleted (remote disconnect)");
		IFCALL(rdpgfx->OpenResult, rdpgfx, RDPGFX_SERVER_OPEN_RESULT_CLOSED);
		break;

	case STATUS_FILE_CLOSED:
		WLog_DBG(TAG, "rdpgfx dynamic channel deleted (closed by client)");
		IFCALL(rdpgfx->OpenResult, rdpgfx, RDPGFX_SERVER_OPEN_RESULT_CLOSED);
		break;

	default:
		WLog_DBG(TAG, "rdpgfx dynamic channel deleted (status = 0x%08"PRIX32")", creationStatus);
		IFCALL(rdpgfx->OpenResult, rdpgfx, RDPGFX_SERVER_OPEN_RESULT_ERROR);
		break;
	}

	return TRUE;
}

static BOOL rdpgfx_server_ogon_created_callback(void *context) {
	(void)(context); /* nothing to do, the client starts the chatter */
	WLog_DBG(TAG, "rdpgfx dynamic channel is open");
	return TRUE;
}

static BOOL rdpgfx_server_ogon_receive_callback(void *context, const BYTE *data, UINT32 length) {
	rdpgfx_server_context* rdpgfx = context;
	wStream *s = rdpgfx->s;
	UINT16 cmdId;
	UINT32 pduLength;
	UINT32 position;
	UINT32 consume;

	while (Stream_GetPosition(s) + length >= rdpgfx->requiredBytes) {
		consume = rdpgfx->requiredBytes - Stream_GetPosition(s);
		if (!Stream_EnsureRemainingCapacity(s, consume)) {
			WLog_ERR(TAG, "rdpgfx receive callback: unable to realloc stream");
			return FALSE;
		}
		Stream_Write(s, data, consume);
		data += consume;
		length -= consume;

		position = Stream_GetPosition(s);

		Stream_SetPosition(s, 0);
		Stream_Read_UINT16(s, cmdId);
		Stream_Seek_UINT16(s); /* flags, 2 bytes, unused */
		Stream_Read_UINT32(s, pduLength);

		if (pduLength < 8) {
			goto err;
		}

		if (position < pduLength) {
			rdpgfx->requiredBytes = pduLength;
			Stream_SetPosition(s, position);
			continue;
		}

		pduLength -= 8;

		switch (cmdId) {
			case RDPGFX_CMDID_CAPSADVERTISE:
				if (rdpgfx->capsReceived) {
					goto err;
				}

				if (!rdpgfx_server_recv_capabilities(rdpgfx, s, pduLength)) {
					goto err;
				}

				rdpgfx->capsReceived = TRUE;

				if (!rdpgfx_server_send_capabilities(rdpgfx, s)) {
					goto err;
				}

				IFCALL(rdpgfx->OpenResult, rdpgfx, RDPGFX_SERVER_OPEN_RESULT_OK);
				break;

			case RDPGFX_CMDID_FRAMEACKNOWLEDGE:
				if (!rdpgfx_server_recv_frameack(rdpgfx, s, pduLength)) {
					goto err;
				}
				break;

			case 0x0016:
				/* undocumented command id (as of March 2015) which is always sent by Microsoft's
				 * iOS Client directly after it sends the RDPGFX_FRAME_ACKNOWLEDGE_PDU
				 */
				break;

			default:
				WLog_DBG(TAG, "rdpgfx: unknown cmdId %"PRIu16"", cmdId);
				break;
		}

		/* a complete pdu was consumed, get ready for the next one */
		rdpgfx->requiredBytes = 8;
		Stream_SetPosition(s, 0);
	}

	if (length) {
		/* data still has a few bytes of the next pdu - is this possible ? */
		if (!Stream_EnsureRemainingCapacity(rdpgfx->s, length)) {
			WLog_ERR(TAG, "rdpgfx: unable to resize stream for %"PRIu32" more bytes", length);
			goto err;
		}
		Stream_Write(s, data, length);
	}

	if (Stream_GetPosition(s) >= rdpgfx->requiredBytes) {
		WLog_ERR(TAG, "rdpgfx: internal parsing error");
		goto err;
	}

	return TRUE;

err:
	IFCALL(rdpgfx->Close, rdpgfx);
	return FALSE;
}

static void rdpgfx_server_open(rdpgfx_server_context *rdpgfx) {
	ogon_vcm *vcm = (ogon_vcm *) rdpgfx->vcm;
	internal_virtual_channel *intVC;

	if (rdpgfx->rdpgfx_channel) {
		WLog_ERR(TAG, "rdpgfx: error, channel is already opened");
		return;
	}

	if (!(intVC = virtual_manager_open_internal_virtual_channel(
			vcm, RDPGFX_DVC_CHANNEL_NAME, TRUE)))
	{
		IFCALL(rdpgfx->OpenResult, rdpgfx, RDPGFX_SERVER_OPEN_RESULT_NOTSUPPORTED);
		return;
	}

	intVC->context = (void*)rdpgfx;
	intVC->receive_callback = rdpgfx_server_ogon_receive_callback;
	intVC->created_callback = rdpgfx_server_ogon_created_callback;
	intVC->deleted_callback = rdpgfx_server_ogon_deleted_callback;

	rdpgfx->rdpgfx_channel = (void*)intVC;
	rdpgfx->requiredBytes = 8;
	rdpgfx->version = 0;
	rdpgfx->flags = 0;
	rdpgfx->capsReceived = FALSE;
}

static BOOL rdpgfx_server_wire_to_surface_1(rdpgfx_server_context *rdpgfx,
	RDPGFX_WIRE_TO_SURFACE_PDU_1 *wire_to_surface_1)
{
	wStream *s;
	BOOL result;
	UINT32 pduLength = RDPGFX_WIRETOSURFACE_1_HEADER_SIZE + wire_to_surface_1->bitmapDataLength;

	if (pduLength <= RDPGFX_MAX_SEGMENT_LENGTH)
	{
		if (!(s = Stream_New(NULL, pduLength + 2))) {
			return FALSE;
		}

		Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
		Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

		Stream_Write_UINT16(s, RDPGFX_CMDID_WIRETOSURFACE_1); /* RDPGFX_HEADER.cmdId (2 bytes) */
		Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
		Stream_Write_UINT32(s, pduLength); /* RDPGFX_HEADER.pduLength (4 bytes) */
		Stream_Write_UINT16(s, wire_to_surface_1->surfaceId);
		Stream_Write_UINT16(s, wire_to_surface_1->codecId);
		Stream_Write_UINT8(s, wire_to_surface_1->pixelFormat);
		Stream_Write_UINT16(s, wire_to_surface_1->destRect.left);
		Stream_Write_UINT16(s, wire_to_surface_1->destRect.top);
		Stream_Write_UINT16(s, wire_to_surface_1->destRect.right);
		Stream_Write_UINT16(s, wire_to_surface_1->destRect.bottom);
		Stream_Write_UINT32(s, wire_to_surface_1->bitmapDataLength);
		Stream_Write(s, wire_to_surface_1->bitmapData, wire_to_surface_1->bitmapDataLength);
	}
	else
	{
		UINT32 segmentCount;
		UINT32 segmentLength;
		BYTE* bitmapData = wire_to_surface_1->bitmapData;
		UINT32 bitmapDataLength = wire_to_surface_1->bitmapDataLength;

		segmentCount = (pduLength + (RDPGFX_MAX_SEGMENT_LENGTH - 1)) / RDPGFX_MAX_SEGMENT_LENGTH;
		if (!(s = Stream_New(NULL, 7 + (5 + RDPGFX_MAX_SEGMENT_LENGTH) * segmentCount))) {
			return FALSE;
		}

		Stream_Write_UINT8(s, RDPGFX_MULTIPART); /* descriptor (1 byte) */
		Stream_Write_UINT16(s, segmentCount);
		Stream_Write_UINT32(s, pduLength); /* uncompressedSize (4 bytes) */

		segmentLength = RDPGFX_MAX_SEGMENT_LENGTH - RDPGFX_WIRETOSURFACE_1_HEADER_SIZE;

		Stream_Write_UINT32(s, 1 + RDPGFX_MAX_SEGMENT_LENGTH);
		Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

		Stream_Write_UINT16(s, RDPGFX_CMDID_WIRETOSURFACE_1); /* RDPGFX_HEADER.cmdId (2 bytes) */
		Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
		Stream_Write_UINT32(s, pduLength); /* RDPGFX_HEADER.pduLength (4 bytes) */
		Stream_Write_UINT16(s, wire_to_surface_1->surfaceId);
		Stream_Write_UINT16(s, wire_to_surface_1->codecId);
		Stream_Write_UINT8(s, wire_to_surface_1->pixelFormat);
		Stream_Write_UINT16(s, wire_to_surface_1->destRect.left);
		Stream_Write_UINT16(s, wire_to_surface_1->destRect.top);
		Stream_Write_UINT16(s, wire_to_surface_1->destRect.right);
		Stream_Write_UINT16(s, wire_to_surface_1->destRect.bottom);
		Stream_Write_UINT32(s, wire_to_surface_1->bitmapDataLength);
		Stream_Write(s, bitmapData, segmentLength);

		bitmapData += segmentLength;
		bitmapDataLength -= segmentLength;

		while (bitmapDataLength > 0) {
			segmentLength = bitmapDataLength;
			if (segmentLength > RDPGFX_MAX_SEGMENT_LENGTH) {
				segmentLength = RDPGFX_MAX_SEGMENT_LENGTH;
			}

			Stream_Write_UINT32(s, 1 + segmentLength);
			Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

			Stream_Write(s, bitmapData, segmentLength);

			bitmapData += segmentLength;
			bitmapDataLength -= segmentLength;
		}
	}

	result = WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s), (ULONG) Stream_GetPosition(s), NULL);
	Stream_Free(s, TRUE);

	return result;
}

static BOOL rdpgfx_server_wire_to_surface_2(rdpgfx_server_context *rdpgfx,
	RDPGFX_WIRE_TO_SURFACE_PDU_2* wire_to_surface_2)
{
	wStream *s;
	BOOL result;
	UINT32 pduLength = RDPGFX_WIRETOSURFACE_2_HEADER_SIZE + wire_to_surface_2->bitmapDataLength;

	if (pduLength <= RDPGFX_MAX_SEGMENT_LENGTH)
	{
		if (!(s = Stream_New(NULL, pduLength + 2))) {
			return FALSE;
		}

		Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
		Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

		Stream_Write_UINT16(s, RDPGFX_CMDID_WIRETOSURFACE_2); /* RDPGFX_HEADER.cmdId (2 bytes) */
		Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
		Stream_Write_UINT32(s, pduLength); /* RDPGFX_HEADER.pduLength (4 bytes) */
		Stream_Write_UINT16(s, wire_to_surface_2->surfaceId); /* surfaceId (2 bytes) */
		Stream_Write_UINT16(s, wire_to_surface_2->codecId); /* codecId (2 bytes) */
		Stream_Write_UINT32(s, wire_to_surface_2->codecContextId); /* codecContextId (4 byte) */
		Stream_Write_UINT8(s, wire_to_surface_2->pixelFormat); /* pixelFormat (1 byte) */
		Stream_Write_UINT32(s, wire_to_surface_2->bitmapDataLength); /* bitmapDataLength (4 bytes) */
		Stream_Write(s, wire_to_surface_2->bitmapData, wire_to_surface_2->bitmapDataLength);
	} else {
		UINT32 segmentCount;
		UINT32 segmentLength;
		BYTE* bitmapData = wire_to_surface_2->bitmapData;
		UINT32 bitmapDataLength = wire_to_surface_2->bitmapDataLength;

		segmentCount = (pduLength + (RDPGFX_MAX_SEGMENT_LENGTH - 1)) / RDPGFX_MAX_SEGMENT_LENGTH;
		if (!(s = Stream_New(NULL, 7 + (5 + RDPGFX_MAX_SEGMENT_LENGTH) * segmentCount))) {
			return FALSE;
		}

		Stream_Write_UINT8(s, RDPGFX_MULTIPART); /* descriptor (1 byte) */
		Stream_Write_UINT16(s, segmentCount);
		Stream_Write_UINT32(s, pduLength); /* uncompressedSize (4 bytes) */

		segmentLength = RDPGFX_MAX_SEGMENT_LENGTH - RDPGFX_WIRETOSURFACE_2_HEADER_SIZE;

		Stream_Write_UINT32(s, 1 + RDPGFX_MAX_SEGMENT_LENGTH);
		Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

		Stream_Write_UINT16(s, RDPGFX_CMDID_WIRETOSURFACE_2); /* RDPGFX_HEADER.cmdId (2 bytes) */
		Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
		Stream_Write_UINT32(s, pduLength); /* RDPGFX_HEADER.pduLength (4 bytes) */
		Stream_Write_UINT16(s, wire_to_surface_2->surfaceId); /* surfaceId (2 bytes) */
		Stream_Write_UINT16(s, wire_to_surface_2->codecId); /* codecId (2 bytes) */
		Stream_Write_UINT32(s, wire_to_surface_2->codecContextId); /* codecContextId (4 byte) */
		Stream_Write_UINT8(s, wire_to_surface_2->pixelFormat); /* pixelFormat (1 byte) */
		Stream_Write_UINT32(s, wire_to_surface_2->bitmapDataLength); /* bitmapDataLength (4 bytes) */
		Stream_Write(s, bitmapData, segmentLength);

		bitmapData += segmentLength;
		bitmapDataLength -= segmentLength;

		while (bitmapDataLength > 0) {
			segmentLength = bitmapDataLength;
			if (segmentLength > RDPGFX_MAX_SEGMENT_LENGTH) {
				segmentLength = RDPGFX_MAX_SEGMENT_LENGTH;
			}

			Stream_Write_UINT32(s, 1 + segmentLength);
			Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

			Stream_Write(s, bitmapData, segmentLength);

			bitmapData += segmentLength;
			bitmapDataLength -= segmentLength;
		}
	}

	result = WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s), (ULONG) Stream_GetPosition(s), NULL);
	Stream_Free(s, TRUE);

	return result;
}

static BOOL rdpgfx_server_solidfill(rdpgfx_server_context* rdpgfx, RDPGFX_SOLID_FILL_PDU* solidfill)
{
	UINT16 i;
	wStream *s;
	BOOL result;

	if (!(s = Stream_New(NULL, 18 + solidfill->fillRectCount * 8))) {
		return FALSE;
	}

	Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
	Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

	Stream_Write_UINT16(s, RDPGFX_CMDID_SOLIDFILL); /* RDPGFX_HEADER.cmdId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
	Stream_Write_UINT32(s, 16 + solidfill->fillRectCount * 8); /* RDPGFX_HEADER.pduLength (4 bytes) */
	Stream_Write_UINT16(s, solidfill->surfaceId);
	Stream_Write_UINT8(s, solidfill->fillPixel.B);
	Stream_Write_UINT8(s, solidfill->fillPixel.G);
	Stream_Write_UINT8(s, solidfill->fillPixel.R);
	Stream_Write_UINT8(s, solidfill->fillPixel.XA);
	Stream_Write_UINT16(s, solidfill->fillRectCount);

	for (i = 0; i < solidfill->fillRectCount; i++) {
		Stream_Write_UINT16(s, solidfill->fillRects[i].left);
		Stream_Write_UINT16(s, solidfill->fillRects[i].top);
		Stream_Write_UINT16(s, solidfill->fillRects[i].right);
		Stream_Write_UINT16(s, solidfill->fillRects[i].bottom);
	}

	result = WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s), (ULONG) Stream_GetPosition(s), NULL);
	Stream_Free(s, TRUE);

	return result;
}

static BOOL rdpgfx_server_create_surface(rdpgfx_server_context* rdpgfx,
	RDPGFX_CREATE_SURFACE_PDU* create_surface)
{
	wStream *s;
	BOOL result;

	if (!(s = Stream_New(NULL, 17))) {
		return FALSE;
	}

	Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
	Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

	Stream_Write_UINT16(s, RDPGFX_CMDID_CREATESURFACE); /* RDPGFX_HEADER.cmdId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
	Stream_Write_UINT32(s, 15); /* RDPGFX_HEADER.pduLength (4 bytes) */
	Stream_Write_UINT16(s, create_surface->surfaceId);
	Stream_Write_UINT16(s, create_surface->width);
	Stream_Write_UINT16(s, create_surface->height);
	Stream_Write_UINT8(s, create_surface->pixelFormat);

	result = WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s), (ULONG) Stream_GetPosition(s), NULL);
	Stream_Free(s, TRUE);

	return result;
}

static BOOL rdpgfx_server_delete_surface(rdpgfx_server_context* rdpgfx, RDPGFX_DELETE_SURFACE_PDU* delete_surface)
{
	wStream *s;
	BOOL result;

	if (!(s = Stream_New(NULL, 12)))
		return FALSE;

	Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
	Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

	Stream_Write_UINT16(s, RDPGFX_CMDID_DELETESURFACE); /* RDPGFX_HEADER.cmdId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
	Stream_Write_UINT32(s, 10); /* RDPGFX_HEADER.pduLength (4 bytes) */
	Stream_Write_UINT16(s, delete_surface->surfaceId);

	result = WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s), (ULONG) Stream_GetPosition(s), NULL);
	Stream_Free(s, TRUE);

	return result;
}

static BOOL rdpgfx_server_start_frame(rdpgfx_server_context* rdpgfx,
	RDPGFX_START_FRAME_PDU* start_frame)
{
	wStream *s;
	BOOL result;

	if (!(s = Stream_New(NULL, 18))) {
		return FALSE;
	}

	Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
	Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

	Stream_Write_UINT16(s, RDPGFX_CMDID_STARTFRAME); /* RDPGFX_HEADER.cmdId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
	Stream_Write_UINT32(s, 16); /* RDPGFX_HEADER.pduLength (4 bytes) */
	Stream_Write_UINT32(s, start_frame->timestamp);
	Stream_Write_UINT32(s, start_frame->frameId);

	result = WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s), (ULONG) Stream_GetPosition(s), NULL);
	Stream_Free(s, TRUE);

	return result;
}

static BOOL rdpgfx_server_end_frame(rdpgfx_server_context* rdpgfx, RDPGFX_END_FRAME_PDU* end_frame) {
	wStream *s;
	BOOL result;

	if (!(s = Stream_New(NULL, 14))) {
		return FALSE;
	}

	Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
	Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

	Stream_Write_UINT16(s, RDPGFX_CMDID_ENDFRAME); /* RDPGFX_HEADER.cmdId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
	Stream_Write_UINT32(s, 12); /* RDPGFX_HEADER.pduLength (4 bytes) */
	Stream_Write_UINT32(s, end_frame->frameId);

	result = WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s), (ULONG) Stream_GetPosition(s), NULL);
	Stream_Free(s, TRUE);

	return result;
}

static BOOL rdpgfx_server_reset_graphics(rdpgfx_server_context* rdpgfx,
	RDPGFX_RESET_GRAPHICS_PDU* reset_graphics)
{
	wStream *s;
	BOOL result;

	if (!(s = Stream_New(NULL, 342))) {
		return FALSE;
	}

	Stream_Clear(s);

	Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
	Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

	Stream_Write_UINT16(s, RDPGFX_CMDID_RESETGRAPHICS); /* RDPGFX_HEADER.cmdId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
	Stream_Write_UINT32(s, 340); /* RDPGFX_HEADER.pduLength (4 bytes) - 340 is required according to the spec*/
	Stream_Write_UINT32(s, reset_graphics->width);
	Stream_Write_UINT32(s, reset_graphics->height);
	Stream_Write_UINT32(s, 1); /* monitorCount (4 bytes) */
	Stream_Write_UINT32(s, 0); /* TS_MONITOR_DEF.left (4 bytes) */
	Stream_Write_UINT32(s, 0); /* TS_MONITOR_DEF.top (4 bytes) */
	/* Note: TS_MONITOR_DEF's right and bottom members are inclusive coordinates ! */
	Stream_Write_UINT32(s, reset_graphics->width - 1); /* TS_MONITOR_DEF.right (4 bytes) */
	Stream_Write_UINT32(s, reset_graphics->height - 1); /* TS_MONITOR_DEF.bottom (4 bytes) */
	Stream_Write_UINT32(s, MONITOR_PRIMARY); /* TS_MONITOR_DEF.flags (4 bytes) */

	result = WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s), 342, NULL);
	Stream_Free(s, TRUE);

	return result;
}

static BOOL rdpgfx_server_map_surface_to_output(rdpgfx_server_context* rdpgfx,
	RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* map_surface_to_output)
{
	wStream *s;
	BOOL result;

	if (!(s = Stream_New(NULL, 22))) {
		return FALSE;
	}

	Stream_Write_UINT8(s, RDPGFX_SINGLE); /* descriptor (1 byte) */
	Stream_Write_UINT8(s, PACKET_COMPR_TYPE_RDP8); /* RDP8_BULK_ENCODED_DATA.header (1 byte) */

	Stream_Write_UINT16(s, RDPGFX_CMDID_MAPSURFACETOOUTPUT); /* RDPGFX_HEADER.cmdId (2 bytes) */
	Stream_Write_UINT16(s, 0); /* RDPGFX_HEADER.flags (2 bytes) */
	Stream_Write_UINT32(s, 20); /* RDPGFX_HEADER.pduLength (4 bytes) */
	Stream_Write_UINT16(s, map_surface_to_output->surfaceId);
	Stream_Write_UINT16(s, 0); /* reserved (2 bytes) */
	Stream_Write_UINT32(s, map_surface_to_output->outputOriginX);
	Stream_Write_UINT32(s, map_surface_to_output->outputOriginY);

	result = WTSVirtualChannelWrite(rdpgfx->rdpgfx_channel, (PCHAR) Stream_Buffer(s), (ULONG) Stream_GetPosition(s), NULL);
	Stream_Free(s, TRUE);

	return result;
}

static void rdpgfx_server_close(rdpgfx_server_context* rdpgfx) {
	if (rdpgfx->rdpgfx_channel) {
		virtual_manager_close_internal_virtual_channel(rdpgfx->rdpgfx_channel);
		rdpgfx->rdpgfx_channel = NULL;
		IFCALL(rdpgfx->OpenResult, rdpgfx, RDPGFX_SERVER_OPEN_RESULT_CLOSED);
	}
}

rdpgfx_server_context* rdpgfx_server_context_new(HANDLE vcm) {
	rdpgfx_server_context* rdpgfx;

	if (!(rdpgfx = (rdpgfx_server_context*) calloc(1, sizeof(rdpgfx_server_context)))) {
		return NULL;
	}

	if (!(rdpgfx->s = Stream_New(NULL, 4096))) {
		free(rdpgfx);
		return NULL;
	}

	rdpgfx->vcm = vcm;
	rdpgfx->Open = rdpgfx_server_open;
	rdpgfx->Close = rdpgfx_server_close;
	rdpgfx->WireToSurface1 = rdpgfx_server_wire_to_surface_1;
	rdpgfx->WireToSurface2 = rdpgfx_server_wire_to_surface_2;
	rdpgfx->SolidFill = rdpgfx_server_solidfill;
	rdpgfx->CreateSurface = rdpgfx_server_create_surface;
	rdpgfx->DeleteSurface = rdpgfx_server_delete_surface;
	rdpgfx->StartFrame = rdpgfx_server_start_frame;
	rdpgfx->EndFrame = rdpgfx_server_end_frame;
	rdpgfx->ResetGraphics = rdpgfx_server_reset_graphics;
	rdpgfx->MapSurfaceToOutput = rdpgfx_server_map_surface_to_output;

	return rdpgfx;
}

void rdpgfx_server_context_free(rdpgfx_server_context* rdpgfx) {
	Stream_Free(rdpgfx->s, TRUE);
	rdpgfx->Close(rdpgfx);
	free(rdpgfx);
}
