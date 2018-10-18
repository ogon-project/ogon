/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Framebuffer Encoders
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <freerdp/codec/region.h>
#include <freerdp/codec/rfx.h>

#include <ogon/dmgbuf.h>

#include "../common/global.h"
#include "commondefs.h"
#include "bandwidth_mgmt.h"
#include "peer.h"
#include "backend.h"
#include "encoder.h"
#include "font8x8.h"

#define TAG OGON_TAG("core.graphics")

typedef int (*pfn_send_graphics_bits)(ogon_connection *conn, BYTE *data, RDP_RECT *rects, UINT32 numRects);

BOOL ogon_rfx_write_message_progressive_simple(RFX_CONTEXT *context, wStream *s,
	RFX_MESSAGE *msg)
{
	UINT32 blockLen;
	UINT32 i;
	UINT32 *qv;
	RFX_TILE *tile;
	UINT32 tilesDataSize;

	if (context->mode != RLGR1) {
		WLog_ERR(TAG, "%s: error, RLGR1 mode is required!", __FUNCTION__);
		return FALSE;
	}

	/* RFX_PROGRESSIVE_SYNC */
	blockLen = 12;
	if (!Stream_EnsureRemainingCapacity(s, blockLen)) {
		return FALSE;
	}
	Stream_Write_UINT16(s, 0xCCC0); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen); /* blockLen (4 bytes) */
	Stream_Write_UINT32(s, 0xCACCACCA); /* magic (4 bytes) */
	Stream_Write_UINT16(s, 0x0100); /* version (2 bytes) */

	/* RFX_PROGRESSIVE_CONTEXT */
	blockLen = 10;
	if (!Stream_EnsureRemainingCapacity(s, blockLen)) {
		return FALSE;
	}
	Stream_Write_UINT16(s, 0xCCC3); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, 10); /* blockLen (4 bytes) */
	Stream_Write_UINT8(s, 0); /* ctxId (1 byte) */
	Stream_Write_UINT16(s, 64); /* tileSize (2 bytes) */
	Stream_Write_UINT8(s, 0); /* flags (1 byte) */

	/* RFX_PROGRESSIVE_FRAME_BEGIN */
	blockLen = 12;
	if (!Stream_EnsureRemainingCapacity(s, blockLen)) {
		return FALSE;
	}
	Stream_Write_UINT16(s, 0xCCC1); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen); /* blockLen (4 bytes) */
	Stream_Write_UINT32(s, msg->frameIdx); /* frameIndex (4 bytes) */
	Stream_Write_UINT16(s, 1); /* regionCount (2 bytes) */

	/* RFX_PROGRESSIVE_REGION */
	blockLen  = 18;
	blockLen += msg->numRects * 8;
	blockLen += msg->numQuant * 5;
	tilesDataSize = msg->numTiles * 22;
	for (i = 0; i < msg->numTiles; i++) {
		tile = msg->tiles[i];
		tilesDataSize += tile->YLen + tile->CbLen + tile->CrLen;
	}
	blockLen += tilesDataSize;

	if (!Stream_EnsureRemainingCapacity(s, blockLen)) {
		return FALSE;
	}
	Stream_Write_UINT16(s, 0xCCC4); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen); /* blockLen (4 bytes) */
	Stream_Write_UINT8(s, 64); /* tileSize (1 byte) */
	Stream_Write_UINT16(s, msg->numRects); /* numRects (2 bytes) */
	Stream_Write_UINT8(s, msg->numQuant); /* numQuant (1 byte) */
	Stream_Write_UINT8(s, 0); /* numProgQuant (1 byte) */
	Stream_Write_UINT8(s, 0); /* flags (1 byte) */
	Stream_Write_UINT16(s, msg->numTiles); /* numTiles (2 bytes) */
	Stream_Write_UINT32(s, tilesDataSize); /* tilesDataSize (4 bytes) */

	for (i = 0; i < msg->numRects; i++)	{
		/* TS_RFX_RECT */
		Stream_Write_UINT16(s, msg->rects[i].x); /* x (2 bytes) */
		Stream_Write_UINT16(s, msg->rects[i].y); /* y (2 bytes) */
		Stream_Write_UINT16(s, msg->rects[i].width); /* width (2 bytes) */
		Stream_Write_UINT16(s, msg->rects[i].height); /* height (2 bytes) */
	}

	/**
	 * Note: The RFX_COMPONENT_CODEC_QUANT structure differs from the
	 * TS_RFX_CODEC_QUANT ([MS-RDPRFX] section 2.2.2.1.5) structure with respect
	 * to the order of the bands.
	 *             0    1    2   3     4    5    6    7    8    9
	 * RDPRFX:   LL3, LH3, HL3, HH3, LH2, HL2, HH2, LH1, HL1, HH1
	 * RDPEGFX:  LL3, HL3, LH3, HH3, HL2, LH2, HH2, HL1, LH1, HH1
	 */
	for (i = 0, qv = msg->quantVals; i < msg->numQuant; i++, qv += 10) {
		/* RFX_COMPONENT_CODEC_QUANT */
		Stream_Write_UINT8(s, qv[0] + (qv[2] << 4)); /* LL3 (4-bit), HL3 (4-bit) */
		Stream_Write_UINT8(s, qv[1] + (qv[3] << 4)); /* LH3 (4-bit), HH3 (4-bit) */
		Stream_Write_UINT8(s, qv[5] + (qv[4] << 4)); /* HL2 (4-bit), LH2 (4-bit) */
		Stream_Write_UINT8(s, qv[6] + (qv[8] << 4)); /* HH2 (4-bit), HL1 (4-bit) */
		Stream_Write_UINT8(s, qv[7] + (qv[9] << 4)); /* LH1 (4-bit), HH1 (4-bit) */
	}

	for (i = 0; i < msg->numTiles; i++) {
		/* RFX_PROGRESSIVE_TILE_SIMPLE */
		tile = msg->tiles[i];
		blockLen = 22 + tile->YLen + tile->CbLen + tile->CrLen;
		Stream_Write_UINT16(s, 0xCCC5); /* blockType (2 bytes) */
		Stream_Write_UINT32(s, blockLen); /* blockLen (4 bytes) */
		Stream_Write_UINT8(s, tile->quantIdxY); /* quantIdxY (1 byte) */
		Stream_Write_UINT8(s, tile->quantIdxCb); /* quantIdxCb (1 byte) */
		Stream_Write_UINT8(s, tile->quantIdxCr); /* quantIdxCr (1 byte) */
		Stream_Write_UINT16(s, tile->xIdx); /* xIdx (2 bytes) */
		Stream_Write_UINT16(s, tile->yIdx); /* yIdx (2 bytes) */
		Stream_Write_UINT8(s, 0); /* flags (1 byte) */
		Stream_Write_UINT16(s, tile->YLen); /* YLen (2 bytes) */
		Stream_Write_UINT16(s, tile->CbLen); /* CbLen (2 bytes) */
		Stream_Write_UINT16(s, tile->CrLen); /* CrLen (2 bytes) */
		Stream_Write_UINT16(s, 0); /* tailLen (2 bytes) */
		Stream_Write(s, tile->YData, tile->YLen); /* YData */
		Stream_Write(s, tile->CbData, tile->CbLen); /* CbData */
		Stream_Write(s, tile->CrData, tile->CrLen); /* CrData */
	}

	/* RFX_PROGRESSIVE_FRAME_END */
	blockLen = 6;
	if (!Stream_EnsureRemainingCapacity(s, blockLen)) {
		return FALSE;
	}
	Stream_Write_UINT16(s, 0xCCC2); /* blockType (2 bytes) */
	Stream_Write_UINT32(s, blockLen); /* blockLen (4 bytes) */

	return TRUE;
}

/**
 * Note: Analysis of gfx/rfx data produced by Server 2012R2 (RDVH)
 * shows that there is never more than one single rectangle inside
 * one wire-to-surface pdu!
 * In addition one pdu only contains multiple tiles if those tiles
 * are either horizontally or vertically adjacent.
 * This strongly indicates that their encoder first calculates the
 * damaged region as a "y-x-banded" array of rectangles and then
 * encodes each rectangle in a single wire-to-surface pdu.
 *
 * Now the Microsoft clients seem to expect this simplified rfx
 * payload (they fail/crash horribly otherwise).
 *
 * When the ogon_send_gfx_xxx functions are called the rectangles
 * passed in "rects" are already expected to be y-x-banded and mapped
 * to virtual grid with 64x64 grid tile size.
 */


int ogon_send_gfx_rfx_progressive_bits(ogon_connection *conn, BYTE *data,
	RDP_RECT *rects, UINT32 numRects)
{
	wStream *s;
	UINT32 i;
	RFX_MESSAGE *message;
	RDPGFX_WIRE_TO_SURFACE_PDU_2 pdu;
	RFX_RECT r;
	ogon_backend_connection *backend = conn->backend;
	ogon_front_connection *frontend = &conn->front;
	ogon_bitmap_encoder *encoder = frontend->encoder;
	int ret = 0;

	if (!backend || !data) {
		return 0;
	}

	s = encoder->stream;

	pdu.surfaceId = frontend->rdpgfxOutputSurface;
	pdu.pixelFormat = GFX_PIXEL_FORMAT_ARGB_8888;
	pdu.codecId = RDPGFX_CODECID_CAPROGRESSIVE;
	pdu.codecContextId = 0;

	encoder->rfx_context->mode = RLGR1;

	for (i = 0; i < numRects; i++) {
		r.x = rects[i].x;
		r.y = rects[i].y;
		r.width = rects[i].width;
		r.height = rects[i].height;

		if (rects[i].x % 64 || rects[i].y % 64) {
			WLog_ERR(TAG, "%s: invalid rectangle: x=%"PRId16" y=%"PRId16"", __FUNCTION__, rects[i].x, rects[i].y);
			return 0;
		}
		if (!(message = rfx_encode_message(encoder->rfx_context, &r, 1,
			data, encoder->desktopWidth, encoder->desktopHeight, encoder->scanLine)))
		{
			WLog_ERR(TAG, "failed to encode rfx message");
			return 0;
		}

		message->freeRects = TRUE;

		Stream_SetPosition(s, 0);
		ret = ogon_rfx_write_message_progressive_simple(encoder->rfx_context, s, message) ? 0 : -1;
		rfx_message_free(encoder->rfx_context, message);

		pdu.bitmapDataLength = Stream_GetPosition(s);
		pdu.bitmapData = Stream_Buffer(s);

		if (!ogon_bwmgmt_detect_bandwidth_start(conn)) {
			return -1;
		}

		frontend->rdpgfx->WireToSurface2(frontend->rdpgfx, &pdu);

		if (!ogon_bwmgmt_detect_bandwidth_stop(conn)) {
			return -1;
		}
	}

	return ret;
}

int ogon_send_gfx_rfx_bits(ogon_connection *conn, BYTE *data, RDP_RECT *rects,
	UINT32 numRects)
{
	wStream *s;
	UINT32 i;
	RFX_MESSAGE *message;
	BYTE *buf;
	RDPGFX_WIRE_TO_SURFACE_PDU_1 pdu;
	RFX_RECT r;
	ogon_backend_connection *backend = conn->backend;
	ogon_front_connection *frontend = &conn->front;
	ogon_bitmap_encoder *encoder = frontend->encoder;

	if (!backend || !data) {
		return 0;
	}

	s = encoder->stream;

	pdu.surfaceId = frontend->rdpgfxOutputSurface;
	pdu.pixelFormat = GFX_PIXEL_FORMAT_ARGB_8888;
	pdu.codecId = RDPGFX_CODECID_CAVIDEO;

	r.x = 0;
	r.y = 0;

	encoder->rfx_context->mode = RLGR3;

	for (i = 0; i < numRects; i++) {
		r.width = rects[i].width;
		r.height = rects[i].height;

		if (rects[i].x % 64 || rects[i].y % 64) {
			WLog_ERR(TAG, "%s: invalid rectangle: x=%"PRId16" y=%"PRId16"", __FUNCTION__, rects[i].x, rects[i].y);
			return 0;
		}
		buf = data + rects[i].y * encoder->scanLine + rects[i].x * encoder->bytesPerPixel;
		if (!(message = rfx_encode_message(encoder->rfx_context, &r, 1,
			buf, r.width, r.height, encoder->scanLine)))
		{
			WLog_ERR(TAG, "failed to encode rfx message");
			return 0;
		}

		message->freeRects = TRUE;

		Stream_SetPosition(s, 0);

		if (!rfx_write_message(encoder->rfx_context, s, message)) {
			WLog_ERR(TAG, "failed to write rfx message");
			rfx_message_free(encoder->rfx_context, message);
			return 0;
		}

		rfx_message_free(encoder->rfx_context, message);

		pdu.destRect.left = rects[i].x;
		pdu.destRect.top = rects[i].y;
		pdu.destRect.right = pdu.destRect.left + rects[i].width;
		pdu.destRect.bottom = pdu.destRect.top + rects[i].height;
		pdu.bitmapDataLength = Stream_GetPosition(s);
		pdu.bitmapData = Stream_Buffer(s);

		if (!ogon_bwmgmt_detect_bandwidth_start(conn)) {
			return -1;
		}

		frontend->rdpgfx->WireToSurface1(frontend->rdpgfx, &pdu);

		if (!ogon_bwmgmt_detect_bandwidth_stop(conn)) {
			return -1;
		}
	}

	return 0;
}

int ogon_send_gfx_debug_bitmap(ogon_connection *conn) {
	ogon_front_connection *frontend = &conn->front;
	ogon_bitmap_encoder *encoder = frontend->encoder;
	BYTE *encodedData = NULL;
	UINT32 encodedSize;
	RDPGFX_WIRE_TO_SURFACE_PDU_1 pdu;
	UINT32 scanLine = encoder->desktopWidth * 4;

	if (!(encodedData = freerdp_bitmap_compress_planar(encoder->debug_context,
		encoder->debug_buffer, PIXEL_FORMAT_BGRX32, encoder->desktopWidth, 8,
		scanLine, NULL, &encodedSize)))
	{
		WLog_ERR(TAG, "%s: planar compression failed", __FUNCTION__);
		return 0;
	}

	/* WLog_DBG(TAG, "%s: encodedSize: %"PRIu32"", __FUNCTION__, encodedSize); */

	pdu.surfaceId = frontend->rdpgfxOutputSurface;
	pdu.pixelFormat = GFX_PIXEL_FORMAT_ARGB_8888;
	pdu.codecId = RDPGFX_CODECID_PLANAR;
	pdu.destRect.left = 0;
	pdu.destRect.top = 0;
	pdu.destRect.right = encoder->desktopWidth;
	pdu.destRect.bottom = 8;
	pdu.bitmapDataLength = encodedSize;
	pdu.bitmapData = encodedData;

	frontend->rdpgfx->WireToSurface1(frontend->rdpgfx, &pdu);

	free(encodedData);

	return 0;
}

#ifdef WITH_OPENH264

static inline BOOL ogon_write_avc420_bitmap_stream(wStream *s, RDP_RECT *rects, UINT32 numRects, BYTE *encodedData, UINT32 encodedSize)
{
	UINT32 i;
	UINT32 avc420MetablockSize = 4 + numRects * 10;

	if (!Stream_EnsureRemainingCapacity(s, avc420MetablockSize + encodedSize)) {
		WLog_ERR(TAG, "%s: stream capacity failure", __FUNCTION__);
		return FALSE;
	}

	Stream_Write_UINT32(s, numRects); /* numRegionRects (4 bytes) */

	for (i = 0; i < numRects; i++) {
		Stream_Write_UINT16(s, rects[i].x); /* regionRects.left (2 bytes) */
		Stream_Write_UINT16(s, rects[i].y); /* regionRects.top (2 bytes) */
		Stream_Write_UINT16(s, rects[i].x + rects[i].width); /* regionRects.right (2 bytes) */
		Stream_Write_UINT16(s, rects[i].y + rects[i].height); /* regionRects.bottom (2 bytes) */
	}

	for (i = 0; i < numRects; i++) {
		Stream_Write_UINT8(s, 18); /* qpVal (1 byte) */
		Stream_Write_UINT8(s, 100); /* qualityVal (1 byte) */
	}

	Stream_Write(s, encodedData, encodedSize);

	return TRUE;
}


int ogon_send_gfx_h264_bits(ogon_connection *conn, BYTE *data, RDP_RECT *rects,
	UINT32 numRects)
{
	ogon_front_connection *frontend = &conn->front;
	ogon_bitmap_encoder *encoder = frontend->encoder;
	BYTE *encodedData;
	UINT32 encodedSize;
	RDP_RECT desktopRect;
	RDPGFX_WIRE_TO_SURFACE_PDU_1 pdu;
	wStream *s;
	BOOL optimizable = FALSE;
	UINT32 maxFrameRate = (UINT32)conn->fps;
	UINT32 targetFrameSizeInBits;
	BOOL useAVC444 = frontend->rdpgfx->avc444Supported;
	BOOL useAVC444v2 = frontend->rdpgfx->avc444v2Supported;
	BOOL enableFullAVC444 = FALSE;
	ogon_openh264_compress_mode openh264CompressMode;
	BOOL rv;

	s = encoder->stream;
	Stream_SetPosition(s, 0);

	if (useAVC444 && frontend->rdpgfxH264EnableFullAVC444) {
		enableFullAVC444 = TRUE;
	}

#if 0
	/**
	 * Note: with the current implementation there are currently
	 * only disadvantages involved if the update region is limited
	 */

	if (!rects || !numRects)
#endif
	{
		desktopRect.x = 0;
		desktopRect.y = 0;
		desktopRect.width = encoder->desktopWidth;
		desktopRect.height = encoder->desktopHeight;
		numRects = 1;
		rects = &desktopRect;
	}

	targetFrameSizeInBits = ogon_bwmgtm_calc_max_target_frame_size(conn);

	openh264CompressMode = COMPRESS_MODE_AVC420; /* avc420 frame only */

	if (enableFullAVC444) {
		if (useAVC444v2) {
			openh264CompressMode = COMPRESS_MODE_AVC444V2_A; /* avc444v2 step 1/2 */
		} else {
			openh264CompressMode = COMPRESS_MODE_AVC444V1_A; /* avc444v1 step 1/2 */
		}

		/* since we'll encoding this frame twice: */
		maxFrameRate *= 2;
		targetFrameSizeInBits /= 2;
	}

	STOPWATCH_START(encoder->swH264Compress);
	rv = ogon_openh264_compress(encoder->h264_context, maxFrameRate,
		                    targetFrameSizeInBits, data, &encodedData, &encodedSize,
		                    openh264CompressMode, &optimizable);
	if (!rv || encodedSize < 1)
	{
		STOPWATCH_STOP(encoder->swH264Compress);
		WLog_ERR(TAG, "h264 compression failed. mode=%"PRIu32" encodedSize=%"PRIu32" targetFrameSizeInBits=%"PRIu32"",
		         openh264CompressMode, encodedSize, targetFrameSizeInBits);
		goto out;
	}
	STOPWATCH_STOP(encoder->swH264Compress);
#if 0
	WLog_DBG(TAG, "h264 compression ok. mode=%"PRIu32" encodedSize=%"PRIu32" targetFrameSizeInBits=%"PRIu32" optimizable=%"PRIu32"",
	         openh264CompressMode, encodedSize, targetFrameSizeInBits, optimizable);
#endif
	if (useAVC444) {
		UINT32 avc420EncodedBitstreamInfo = encodedSize + 4 + numRects * 10;

		if (!enableFullAVC444) {
			avc420EncodedBitstreamInfo |= (1 << 30); /* LC = 0x1: YUV420 frame only */
		}
		if (!Stream_EnsureRemainingCapacity(s, sizeof(avc420EncodedBitstreamInfo))) {
			WLog_ERR(TAG, "%s: stream capacity failure", __FUNCTION__);
			goto out;
		}
		Stream_Write_UINT32(s, avc420EncodedBitstreamInfo); /* avc420EncodedBitstreamInfo (4 bytes) */
	}

	if (!ogon_write_avc420_bitmap_stream(s, rects, numRects, encodedData, encodedSize)) {
		goto out;
	}

	if (enableFullAVC444) {
		/* generate avc444 avc420EncodedBitstream2 */
		openh264CompressMode = COMPRESS_MODE_AVC444VX_B; /* avc444 step 2/2 */

		STOPWATCH_START(encoder->swH264Compress);
		rv = ogon_openh264_compress(encoder->h264_context, maxFrameRate,
		                            targetFrameSizeInBits, data, &encodedData, &encodedSize,
		                            openh264CompressMode, &optimizable);
		if (!rv || encodedSize < 1)
		{
			STOPWATCH_STOP(encoder->swH264Compress);
			WLog_ERR(TAG, "h264 compression failed. mode=%"PRIu32" encodedSize=%"PRIu32" targetFrameSizeInBits=%"PRIu32"",
			         openh264CompressMode, encodedSize, targetFrameSizeInBits);
			goto out;
		}
		STOPWATCH_STOP(encoder->swH264Compress);
#if 0
		WLog_DBG(TAG, "h264 compression ok. mode=%"PRIu32" encodedSize=%"PRIu32" targetFrameSizeInBits=%"PRIu32" optimizable=%"PRIu32"",
		         openh264CompressMode, encodedSize, targetFrameSizeInBits, optimizable);
#endif
		if (!ogon_write_avc420_bitmap_stream(s, rects, numRects, encodedData, encodedSize)) {
			goto out;
		}

		optimizable = FALSE; /* no post rendering possible in full avc444 mode */
	}

	pdu.surfaceId = frontend->rdpgfxOutputSurface;
	pdu.pixelFormat = GFX_PIXEL_FORMAT_ARGB_8888;
	if (useAVC444) {
		pdu.codecId = useAVC444v2 ? RDPGFX_CODECID_AVC444v2 : RDPGFX_CODECID_AVC444;
	} else  {
		pdu.codecId = RDPGFX_CODECID_AVC420;
	}
	pdu.destRect.left = 0;
	pdu.destRect.top = 0;
	pdu.destRect.right = encoder->desktopWidth;
	pdu.destRect.bottom = encoder->desktopHeight;
	pdu.bitmapDataLength = Stream_GetPosition(s);
	pdu.bitmapData = Stream_Buffer(s);

	if (!ogon_bwmgmt_detect_bandwidth_start(conn)) {
		return -1;
	}

	frontend->rdpgfx->WireToSurface1(frontend->rdpgfx, &pdu);

	if (!ogon_bwmgmt_detect_bandwidth_stop(conn)) {
		return -1;
	}

out:
	if (optimizable) {
		if (frontend->rdpgfxProgressiveTicks == 0) {
			frontend->rdpgfxProgressiveTicks = 1;
		}
	} else {
		frontend->rdpgfxProgressiveTicks = 0;
	}

	return 0;
}
#endif /* WITH_OPENH264 defined */

int ogon_send_rdp_rfx_bits(ogon_connection *conn, BYTE *data, RDP_RECT *rects,
	UINT32 numRects)
{
	/**
	 * Note: we use rfx_encode_message() instead of rfx_encode_messages().
	 * With current freerdp the server advertises a max request size that
	 * is large enough to hold a full screen update for any bitmap codec.
	 * In case of RemoteFX the client MUST accept this value otherwise
	 * RemoteFX gets disabled. To nonetheless enable the multi message
	 * encoder enable the following definition:
	 * #define OGON_USE_MULTIMESSAGE_RFX_ENCODER
	 */

	wStream *s;
	SURFACE_BITS_COMMAND cmd;
	RFX_MESSAGE* message;
	UINT32 messageSize;

	freerdp_peer *peer = conn->context.peer;
	rdpSettings *settings = conn->context.settings;
	rdpUpdate* update = peer->update;

	ogon_backend_connection *backend = conn->backend;
	ogon_front_connection *frontend = &conn->front;
	ogon_bitmap_encoder *encoder = frontend->encoder;

	assert(data);

	if (!backend) {
		return 0;
	}

	if (!update->SurfaceBits) {
		WLog_ERR(TAG, "SurfaceBits callback is not set");
		return -1;
	}

	cmd.destLeft = 0;
	cmd.destTop = 0;
	cmd.destRight = encoder->desktopWidth;
	cmd.destBottom = encoder->desktopHeight;
	cmd.bmp.codecID = settings->RemoteFxCodecId;
	cmd.bmp.bpp = 32;
	cmd.bmp.width = encoder->desktopWidth;
	cmd.bmp.height = encoder->desktopHeight;
	cmd.skipCompression = TRUE;

	s = encoder->stream;

	if (!(message = rfx_encode_message(encoder->rfx_context,
		(RFX_RECT*)rects, numRects, data, settings->DesktopWidth,
		settings->DesktopHeight, encoder->scanLine)))
	{
		WLog_ERR(TAG, "failed to encode rfx message (numRects = %"PRIu32")", numRects);
		return 0;
	}

	message->freeRects = TRUE;

	Stream_SetPosition(s, 0);

	if (!rfx_write_message(encoder->rfx_context, s, message)) {
		WLog_ERR(TAG, "failed to write rfx message");
		rfx_message_free(encoder->rfx_context, message);
		return 0;
	}

	rfx_message_free(encoder->rfx_context, message);

	cmd.bmp.bitmapDataLength = Stream_GetPosition(s);
	cmd.bmp.bitmapData = Stream_Buffer(s);
	messageSize = cmd.bmp.bitmapDataLength;

	messageSize += 22; /* the size of the surface bits command header */

	if (messageSize > settings->MultifragMaxRequestSize) {
		WLog_ERR(TAG, "internal error: rfx message size (%"PRIu32") exceeds max request size (%"PRIu32")",
			messageSize, settings->MultifragMaxRequestSize);
		return 0;
	}

	if (!ogon_bwmgmt_detect_bandwidth_start(conn)) {
		return -1;
	}

	update->SurfaceBits(update->context, &cmd);

	if (!ogon_bwmgmt_detect_bandwidth_stop(conn)) {
		return -1;
	}

	return 0;
}

int ogon_send_bitmap_bits(ogon_connection *conn, BYTE *data, RDP_RECT *rects,
	UINT32 numRects)
{
	rdpUpdate *update = conn->context.peer->update;
	ogon_bitmap_encoder *encoder = conn->front.encoder;
	UINT32 bytePerPixel = encoder->dstBytesPerPixel;
	UINT32 bitsPerPixel = encoder->dstBitsPerPixel;
	ogon_bmp_context *bmp = encoder->bmpContext;
	UINT32 updatePduSize, maxPduSize, maxRectSize, maxDataSize;
	UINT32 i, x, y, nx, ny, numBitmaps, newMaxSize, nextSize;
	BITMAP_UPDATE bitmapUpdate;
	RDP_RECT *pr;
	RDP_RECT r;
	BYTE *src;
	int w, h, mod, e, sp;
	BITMAP_DATA *bitmapData;
	void *tmp;
	UINT32 bitmapUpdateHeaderSize = 2; /* see FreeRDP update_write_bitmap_update() */
	UINT32 bitmapUpdateDataHeaderSize = 26; /* see FreeRDP update_write_bitmap_data() */

	if (!numRects || !data || !rects) {
		return -1;
	}

	maxPduSize = encoder->multifragMaxRequestSize;
	maxRectSize = maxPduSize - bitmapUpdateHeaderSize - bitmapUpdateDataHeaderSize;
	maxRectSize -= 3; /* leave room for the rle header for uncompressable bitmaps */
	numBitmaps = 0;

	Stream_SetPosition(bmp->bs, 0);

	for (i = 0, pr = rects; i < numRects; i++, pr++)
	{
		 /* WLog_DBG(TAG, "input rectangle #%"PRIu32": %"PRId16" %"PRId16" %"PRId16" %"PRId16" (mrsz=%"PRIu32" bpp=%"PRIu32")",
			i, pr->x, pr->y, pr->width, pr->height, maxRectSize, bitsPerPixel); */

		/* horizontal 4 pixel alignment is required */

		if ((mod = pr->x % 4)) {
			pr->x -= mod;
			pr->width += mod;
		}
		if ((mod = pr->width % 4)) {
			pr->width += 4 - mod;
		}

		/* the maximum supported bitmap size is 64 x 64 */

		w = pr->width > 64 ? 64 : pr->width;
		h = pr->height > 64 ? 64 : pr->height;

		/* if this exceeds the client's MaxRequestSize limit we reduce the fragment size
		   but try to keep the average fragments' width as high as possible */

		while (w * bytePerPixel > maxRectSize) {
			w /= 2;
		}
		while (w * h * bytePerPixel > maxRectSize) {
			h = (h == 3) ? 2 : h/2;
		}

		assert(h && w >= 4);

		nx = (pr->width + w - 1) / w;
		ny = (pr->height + h - 1) / h;

		/* generate and process w x h sized fragments of each input rectangle */

		for (y = 0, r.height = h, r.y = pr->y; y < ny; y++, r.y += h) {
			if (r.y + h > pr->y + pr->height) {
				r.height = pr->y + pr->height - r.y;
			}
			for (x = 0, r.width = w, r.x = pr->x; x < nx; x++, r.x += w) {
				if (r.x + w > pr->x + pr->width) {
					r.width = pr->x + pr->width - r.x;
				}
				/* WLog_DBG(TAG, "  encoding rectangle: %"PRId16" %"PRId16" %"PRId16" %"PRId16"", r.x, r.y, r.width, r.height); */
				if (bmp->rectsAllocated < numBitmaps + 1) {
					if (!(tmp = realloc((void*)bmp->rects, sizeof(BITMAP_DATA) * (numBitmaps + 1)))) {
						WLog_ERR(TAG, "failed to (re)allocate %"PRIu32" BITMAP_DATA structs", numBitmaps + 1);
						goto fail;
					}
					bmp->rects = (BITMAP_DATA*)tmp;
					bmp->rectsAllocated = numBitmaps + 1;
				}

				newMaxSize = r.height * r.width * bytePerPixel + 3; /* 3 == RLE header */

				if (Stream_GetRemainingLength(bmp->bs) < newMaxSize) {
					WLog_ERR(TAG, "internal error: unsufficient bmp encoder buffer size: remaining: %"PRIuz" required: %"PRIu32"",
						Stream_GetRemainingLength(bmp->bs), newMaxSize);
					goto fail;
				}

				bitmapData = &bmp->rects[numBitmaps];

				bitmapData->bitmapDataStream = Stream_Pointer(bmp->bs);
				bitmapData->bitsPerPixel = bitsPerPixel;
				bitmapData->compressed = TRUE;
				bitmapData->destLeft = r.x;
				bitmapData->destTop = r.y;
				bitmapData->destRight = r.x + r.width - 1;
				bitmapData->destBottom = r.y + r.height - 1;
				bitmapData->width = r.width;
				bitmapData->height = r.height;
				bitmapData->cbUncompressedSize = r.width * r.height * bytePerPixel;
				bitmapData->cbScanWidth = r.width * bytePerPixel;
				bitmapData->cbCompFirstRowSize = 0;
				bitmapData->flags = 0;

				src = data + r.y * encoder->scanLine + r.x * encoder->bytesPerPixel;

				if (bitsPerPixel > 24) {
					STOPWATCH_START(encoder->swBitmapCompress);
					if (!freerdp_bitmap_compress_planar(bmp->planar, src,
						encoder->srcFormat, r.width, r.height, encoder->scanLine,
						bitmapData->bitmapDataStream, &bitmapData->bitmapLength))
					{
						STOPWATCH_STOP(encoder->swBitmapCompress);
						WLog_ERR(TAG, "planar bitmap compression failed");
						goto fail;
					}
					STOPWATCH_STOP(encoder->swBitmapCompress);
					Stream_Seek(bmp->bs, bitmapData->bitmapLength);
				} else {
					STOPWATCH_START(encoder->swImageCopy);
					if (!freerdp_image_copy(bmp->imageCopyBuffer, encoder->dstFormat,
						0, 0, 0, r.width, r.height, src, encoder->srcFormat,
						encoder->scanLine, 0, 0, NULL,FREERDP_FLIP_NONE ))
					{
						STOPWATCH_STOP(encoder->swImageCopy);
						WLog_ERR(TAG, "image copy failed");
						goto fail;
					}
					STOPWATCH_STOP(encoder->swImageCopy);

					if ((e = r.width % 4)) {
						e = 4 - e;
					}

					sp = Stream_GetPosition(bmp->bs);

					STOPWATCH_START(encoder->swBitmapCompress);
					if (freerdp_bitmap_compress((char*)bmp->imageCopyBuffer, r.width, r.height,
						bmp->bs, bitsPerPixel, sp + newMaxSize, r.height - 1, bmp->bts, e) < 0)
					{
						STOPWATCH_STOP(encoder->swBitmapCompress);
						WLog_ERR(TAG, "interleaved bitmap compression failed");
						goto fail;
					}
					STOPWATCH_STOP(encoder->swBitmapCompress);

					bitmapData->bitmapLength = Stream_GetPosition(bmp->bs) - sp;

					if (bitmapData->bitmapLength > newMaxSize) {
						WLog_ERR(TAG, "interleaved bitmap compression exceeded byte limit: %"PRIu32" > %"PRIu32"",
							 bitmapData->bitmapLength, newMaxSize);
						goto fail;
					}
				}

				bitmapData->cbCompMainBodySize = bitmapData->bitmapLength;
				numBitmaps++;
			}
		}
	}

	bitmapUpdate.skipCompression = FALSE;
	bitmapUpdate.rectangles = bmp->rects;
	bitmapUpdate.count = 0;
	bitmapUpdate.number = 0;

	updatePduSize = 0;

	maxDataSize = maxPduSize - bitmapUpdateHeaderSize;

	for (i = 0; i < numBitmaps; i++) {
		updatePduSize += bmp->rects[i].bitmapLength + bitmapUpdateDataHeaderSize;

		if (updatePduSize > maxDataSize) {
			WLog_ERR(TAG, "Internal error: %"PRIu32" is exceeding maxDataSize (%"PRIu32") in bmp encoder",
				updatePduSize, maxDataSize);
			goto fail;
		}

		bitmapUpdate.number++;
		bitmapUpdate.count++;

		if (i + 1 < numBitmaps) {
			nextSize = bmp->rects[i + 1].bitmapLength + bitmapUpdateDataHeaderSize;
		} else {
			nextSize = 0;
		}

		if (!nextSize || updatePduSize + nextSize > maxDataSize) {
			STOPWATCH_START(encoder->swSendBitmapUpdate);

			if (!update->BitmapUpdate(&conn->context, &bitmapUpdate)) {
				STOPWATCH_STOP(encoder->swSendBitmapUpdate);
				WLog_ERR(TAG, "BitmapUpdate call failed");
				goto fail;
			}
			STOPWATCH_STOP(encoder->swSendBitmapUpdate);
			bitmapUpdate.rectangles += bitmapUpdate.count;
			bitmapUpdate.count = 0;
			bitmapUpdate.number = 0;
			updatePduSize = 0;
		}
	}

	return 0;

fail:
	return -1;
}


static BOOL ogon_compare_and_update_framebuffer_copy(ogon_bitmap_encoder *dstEncoder,
	BYTE *dst, const BYTE *src, int x, int y, int w, int h, int pixelSize, int lineSize)
{
#ifndef WITH_ENCODER_STATS
	OGON_UNUSED(dstEncoder);
#endif
	int i, offset, widthBytes;
	BOOL ret = TRUE;

	STOPWATCH_START(dstEncoder->swFramebufferCompare);

	widthBytes = w * pixelSize;
	offset = (y * lineSize) + (x * pixelSize);
	src += offset;
	dst += offset;

	/* skip lines that are the same */
	for (i = 0; i < h; i++, src += lineSize, dst += lineSize) {
		if (memcmp(dst, src, widthBytes) != 0) {
			ret = FALSE;
			break;
		}
	}

	/* if (!ret)
		WLog_DBG(TAG, "dirty dst=%p src=%p x=%d y=%d w=%d h=%d i=%d",
				 (const void*) dst, (const void*) src, x, y, w, h, i); */

	/* then blindly copy remaining ones */
	for (; i < h; i++, src += lineSize, dst += lineSize) {
		memcpy(dst, src, widthBytes);
	}

	STOPWATCH_STOP(dstEncoder->swFramebufferCompare);

	return ret;
}

static BOOL simplify_damagedRegion(REGION16 *damage, ogon_backend_connection *backend,
	ogon_bitmap_encoder *dstEncoder, REGION16 *input,
	int tileWidth, int tileHeight, BOOL damageFullTiles, int *damageSize)
{
	int x, y, w, h;
	int minTileX, maxTileX, minTileY, maxTileY;
	const RECTANGLE_16 *extents, *rects;
	RECTANGLE_16 tile;
	REGION16 tileIntersection;
	BYTE *fbCopy = dstEncoder->clientView;
	const BYTE* fbData = ogon_dmgbuf_get_data(backend->damage);
	BOOL ret = TRUE;
	UINT32 dmgcount, nrects, i;

	STOPWATCH_START(dstEncoder->swSimplifyDamage);

	region16_init(&tileIntersection);

	extents = region16_extents(input);
	minTileX = extents->left / tileWidth;
	minTileY = extents->top / tileHeight;
	maxTileX = (extents->right - 1) / tileWidth;
	maxTileY = (extents->bottom - 1) / tileHeight;

	*damageSize = 0;
	for (y = minTileY, h = tileHeight; y <= maxTileY; y++)
	{
		if (y == maxTileY)
			h = extents->bottom - (y * tileHeight);

		tile.top = y * tileHeight;
		tile.bottom = tile.top + h;

		for (x = minTileX, w = tileWidth; x <= maxTileX; x++)
		{
			if (x == maxTileX)
				w = extents->right - (x * tileWidth);

			tile.left = x * tileWidth;
			tile.right = tile.left + w;

			if (!region16_intersect_rect(&tileIntersection, input, &tile)) {
				WLog_ERR(TAG, "error computing tile intersection");
				ret = FALSE;
				goto out_cleanup;
			}

			rects = region16_rects(&tileIntersection, &nrects);
			for (i = 0, dmgcount=0; i < nrects; i++, rects++) {
				if (ogon_compare_and_update_framebuffer_copy(dstEncoder,
					fbCopy, fbData, rects->left, rects->top,
					(rects->right - rects->left), (rects->bottom - rects->top),
					4, dstEncoder->scanLine))
				{
					continue;
				}
				dmgcount++;
				if (!damageFullTiles && !region16_union_rect(damage, damage, rects)) {
					WLog_ERR(TAG, "error adding rect to damage region");
					goto out_cleanup;
				}

				*damageSize += (rects->right - rects->left) * (rects->bottom - rects->top);
			}

			if (damageFullTiles && dmgcount && !region16_union_rect(damage, damage, &tile)) {
				WLog_ERR(TAG, "error adding tile to damage region");
				goto out_cleanup;
			}

			region16_clear(&tileIntersection);
		}
	}

out_cleanup:
	region16_uninit(&tileIntersection);
	STOPWATCH_STOP(dstEncoder->swSimplifyDamage);
	return ret;

}

static inline UINT32 ogon_update_encoder_rects(ogon_bitmap_encoder *encoder,
	REGION16 *region)
{
	UINT32 nrects, i;
	const RECTANGLE_16 *src;
	RDP_RECT *dst;

	src = region16_rects(region, &nrects);

	if (encoder->rdpRectsAllocated < nrects) {
		if (!(dst = realloc(encoder->rdpRects, nrects * sizeof(RDP_RECT)))) {
			WLog_ERR(TAG, "error (re)allocating %"PRIu32" rdpRects", nrects);
			return 0;
		}
		encoder->rdpRects = dst;
		encoder->rdpRectsAllocated = nrects;
	} else {
		dst = encoder->rdpRects;
	}

	for (i = 0; i < nrects; i++, src++, dst++) {
		dst->x = src->left;
		dst->y = src->top;
		dst->width = src->right - src->left;
		dst->height = src->bottom - src->top;
	}

	return nrects;
}

int ogon_backend_consume_damage(ogon_connection *conn) {
	ogon_front_connection *front = &conn->front;
	const RDP_RECT *rects, *r;
	BYTE *data = NULL;
	UINT32 i, numRects = 0;
	RECTANGLE_16 rect16;
	ogon_bitmap_encoder *dstEncoder = front->encoder;
	ogon_backend_connection *backend = conn->shadowing->backend;

	rects = ogon_dmgbuf_get_rects(backend->damage, &numRects);
	data = (BYTE *)ogon_dmgbuf_get_data(backend->damage);

	if (!rects || !numRects || !data) {
		return 0;
	}

	for (i = 0, r = rects; i < numRects; i++, r++) {
		if (r->x < 0 || r->y < 0 || r->width < 1 || r->height < 1 ||
		    (UINT32)(r->x + r->width) > dstEncoder->desktopWidth ||
		    (UINT32)(r->y + r->height) > dstEncoder->desktopHeight)
		{
			WLog_WARN(TAG, "invalid rectangle in damage list");
			continue;
		}

		rect16.left = r->x;
		rect16.right = r->x + r->width;
		rect16.top = r->y;
		rect16.bottom = r->y + r->height;

		if (!region16_union_rect(&dstEncoder->accumulatedDamage, &dstEncoder->accumulatedDamage, &rect16)) {
			WLog_ERR(TAG, "error during region16_union_rect()");
			return -1;
		}
	}

	return 0;
}

static inline void ogon_send_frame_marker(ogon_connection *conn, BOOL begin) {
	rdpSettings *settings = conn->context.settings;
	rdpUpdate *update = conn->context.peer->update;
	ogon_front_connection *front = &conn->front;

	/* WLog_DBG(TAG, "%s: send frame %s frameId=%"PRIu32" lastAckFrame=%"PRIu32"", __FUNCTION__,
		 begin ? "BEGIN" : "END", front->nextFrameId, front->lastAckFrame); */

	if (front->rdpgfxConnected) {
		/* under gfx frame markers must be supported */
		if (begin) {
			RDPGFX_START_FRAME_PDU pdu;
			pdu.frameId = front->nextFrameId;
			pdu.timestamp = 0;
			front->rdpgfx->StartFrame(front->rdpgfx, &pdu);
		} else {
			RDPGFX_END_FRAME_PDU pdu;
			pdu.frameId = front->nextFrameId;
			front->rdpgfx->EndFrame(front->rdpgfx, &pdu);
		}
	} else if (settings->SurfaceFrameMarkerEnabled && front->codecMode != CODEC_MODE_BMP) {
		SURFACE_FRAME_MARKER sfm;
		sfm.frameId = front->nextFrameId;
		if (begin) {
			sfm.frameAction = SURFACECMD_FRAMEACTION_BEGIN;
		} else {
			sfm.frameAction = SURFACECMD_FRAMEACTION_END;
			front->nextFrameId++;
		}
		update->SurfaceFrameMarker(&conn->context, &sfm);
	}

	if (!begin) {
		front->nextFrameId++;
	}
}

#if 0
static void dumpExtents(REGION16 *r) {
	const RECTANGLE_16 *rect16 = region16_extents(r);
	WLog_DBG(TAG, "(%"PRIu16",%"PRIu16"  %"PRIu16",%"PRIu16")", rect16->left, rect16->top, rect16->right, rect16->bottom);
}
#endif

void ogon_render_string(char *str, UINT32 fg, UINT32 bg, BYTE *buf, UINT32 scanLine, BOOL vFlip) {
	UINT32 *p;
	UINT32 x, y, z;
	size_t i, len;
	char *glyph;
	unsigned char o;

	if ((len = strlen(str)) * 4 > scanLine) {
		len = scanLine / 4;
	}

	/* clear line */
	for (y = 0; y < 8; y++) {
		p = (UINT32*)(buf + scanLine * y);
		for (x = 0; x < scanLine/4; x++, p++) {
			*p = bg;
		}
	}

	/* draw characters with 1 pixel distance */
	for (i = 0; i < len; i++) {
		o = (unsigned char) str[i];
		if (o < 32 || o > 126) {
			o = '.';
		}
		glyph = font8x8[o];
		for (y = 0; y < 8; y++) {
			z = vFlip ? 7 - y : y;
			p = ((UINT32*)(buf + scanLine * z)) + i * 9;
			for (x = 0; x < 8; x++, p++) {
				if (glyph[y] & 1 << x) {
					*p = fg;
				}
			}
		}
	}
}

BOOL ogon_render_debug_info(ogon_connection *conn, BOOL embed) {
	ogon_front_connection *front = &conn->front;
	ogon_bitmap_encoder *encoder = front->encoder;
	rdpSettings *settings = conn->context.settings;
	BYTE *data = encoder->debug_buffer;
	UINT32 scanLine = encoder->desktopWidth * 4;

	char *codec = "???";
	char *security = "???";
	char *msg;
	int len;

	if (embed) {
		if (!(data = ogon_dmgbuf_get_data(conn->backend->damage))) {
			return FALSE;
		}
		scanLine = encoder->scanLine;
	}

	switch (front->codecMode) {
		case CODEC_MODE_RFX1:
			codec = "RFX1";
			break;
		case CODEC_MODE_RFX2:
			codec = "RFX2";
			break;
		case CODEC_MODE_RFX3:
			codec = "RFX3";
			break;
		case CODEC_MODE_BMP:
			if (encoder->dstBitsPerPixel > 24) {
				codec = "BMPP";
			} else {
				codec = "BMPI";
			}
			break;
		case CODEC_MODE_NSC:
			codec = "NSC";
			break;
		case CODEC_MODE_H264:
			if (front->rdpgfx->avc444Supported) {
				if (front->rdpgfxH264EnableFullAVC444) {
					codec = front->rdpgfx->avc444v2Supported ? "AVC444f2" : "AVC444f1";
				} else {
					codec = front->rdpgfx->avc444v2Supported ? "AVC444p2" : "AVC444p1";
				}
			} else {
				codec = "AVC420";
			}
			break;
	}

	switch (settings->SelectedProtocol) {
		case 0x00000000:
			security = "RDP";
			break;
		case 0x00000001:
			security = "TLS";
			break;
		case 0x00000002:
			security = "NLA";
			break;
	}

	len = front->encoder->desktopWidth / 9 + 1;
	if (!(msg = malloc(len))) {
		return FALSE;
	}

	snprintf(msg, len, "#%ld | act #%"PRIu32" | %s | frame #%06"PRIu32" (ack=%"PRIu32" last=%"PRIu32") | %"PRIu32"x%"PRIu32" | sec=%s | %"PRIu32" kbps | %"PRIu16" fps | bps %"PRIu32"",
		conn->id, front->activationCount, codec, front->nextFrameId, front->frameAcknowledge,
		front->lastAckFrame, encoder->desktopWidth, encoder->desktopHeight,
		security, front->bandwidthMgmt.autodetect_bitRateKBit, front->statistics.fps_measured, front->statistics.bytes_sent * 8);

	ogon_render_string(msg, 0x00FF00, 0x000000, data, scanLine, !embed);

	free(msg);

	return TRUE;
}

int ogon_send_surface_bits(ogon_connection *conn) {
	BYTE *data;
	int ret, nrects, damagedSize;
	REGION16 damagedRegion;
	UINT32 tileSize = 32;
	BOOL damageFullTiles = FALSE;
	BOOL debugInfoEmbedded = TRUE;

	ogon_backend_connection *backend = conn->shadowing->backend;
	ogon_front_connection *front = &conn->front;
	ogon_bitmap_encoder *dstEncoder = front->encoder;

	pfn_send_graphics_bits sendGraphicsBits = NULL;

	STOPWATCH_START(dstEncoder->swSendSurfaceBits);

	if (front->rdpgfxRequired) {
		if (!front->rdpgfxConnected) {
			WLog_ERR(TAG, "gfx required but pipeline is not connected");
			ret = -1;
			goto out;
		}
		if (!front->rdpgfxOutputSurface) {
			if (!ogon_rdpgfx_init_output(conn)) {
				WLog_ERR(TAG, "rdpgfx_init_output failed");
				ret = -1;
				goto out;
			}
		}
#ifdef WITH_OPENH264
		if (front->rdpgfxH264Supported && front->codecMode != CODEC_MODE_H264) {
			if (dstEncoder->h264_context) {
				WLog_DBG(TAG, "%s: switching to H264 codec mode", __FUNCTION__);
				front->codecMode = CODEC_MODE_H264;
			}
		}
#endif
	}

	switch (front->codecMode) {
		case CODEC_MODE_RFX1:
			sendGraphicsBits = ogon_send_rdp_rfx_bits;
			break;
		case CODEC_MODE_RFX2:
			sendGraphicsBits = ogon_send_gfx_rfx_bits;
			break;
		case CODEC_MODE_RFX3:
			sendGraphicsBits = ogon_send_gfx_rfx_progressive_bits;
			break;
		case CODEC_MODE_BMP:
			sendGraphicsBits = ogon_send_bitmap_bits;
			break;
#ifdef WITH_OPENH264
		case CODEC_MODE_H264:
			sendGraphicsBits = ogon_send_gfx_h264_bits;
			debugInfoEmbedded = FALSE;
			break;
#endif
		case CODEC_MODE_NSC:
			WLog_ERR(TAG, "NS codec not implemented");
		default:
			WLog_ERR(TAG, "invalid codec mode %d", front->codecMode);
			ret = -1;
			goto out;
	}

	ret = 0;
	region16_init(&damagedRegion);

	data = ogon_dmgbuf_get_data(backend->damage);

	if (front->rdpgfxConnected) {
		/* We always have to do this if front->rdpgfxConnected ! */
		damageFullTiles = TRUE;
		tileSize = 64;
	}

	if (!simplify_damagedRegion(&damagedRegion, backend, dstEncoder,
		&dstEncoder->accumulatedDamage, tileSize, tileSize,
		damageFullTiles, &damagedSize))
	{
		WLog_ERR(TAG, "error during input simplification");
		ret = -1;
		goto out_release_damaged;
	}

	if (region16_is_empty(&damagedRegion)) {
		if (front->rdpgfxProgressiveTicks == 0) {
			/*WLog_DBG(TAG, "id=%ld, no damage accumulated=", conn->id);
			dumpExtents(&dstEncoder->accumulatedDamage);*/
			goto out_release_damaged;
		}
	}
	else {
		front->rdpgfxProgressiveTicks = 0;
	}

	if (front->showDebugInfo) {
		if (ogon_render_debug_info(conn, debugInfoEmbedded) && debugInfoEmbedded) {
			RECTANGLE_16 rect16;
			rect16.left = 0;
			rect16.top = 0;
			rect16.right = dstEncoder->desktopWidth;
			rect16.bottom = tileSize;
			region16_union_rect(&damagedRegion, &damagedRegion, &rect16);
		}
	}

	nrects = ogon_update_encoder_rects(dstEncoder, &damagedRegion);

	ogon_send_frame_marker(conn, TRUE);

	STOPWATCH_START(dstEncoder->swSendGraphicsBits);
	ret = sendGraphicsBits(conn, data, dstEncoder->rdpRects, nrects);
	STOPWATCH_STOP(dstEncoder->swSendGraphicsBits);

	front->statistics.fps_measure_currentfps++;

	if (front->showDebugInfo && !debugInfoEmbedded) {
		ogon_send_gfx_debug_bitmap(conn);
	}

	ogon_send_frame_marker(conn, FALSE);

out_release_damaged:
	region16_clear(&dstEncoder->accumulatedDamage);
	region16_uninit(&damagedRegion);
out:
	STOPWATCH_STOP(dstEncoder->swSendSurfaceBits);
	return ret;
}
