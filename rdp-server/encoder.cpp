/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Bitmap Encoder Interface
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
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <ogon/dmgbuf.h>

#include "../common/global.h"

#include "encoder.h"

#define TAG OGON_TAG("core.encoder")

#define OGON_ENCODER_MIN_MULTIFRAG_REQUEST_SIZE 1024

static void ogon_delete_encoder_bmp_context(ogon_bitmap_encoder *e) {
	ogon_bmp_context *ctx;

	if (!e || !(ctx = e->bmpContext)) {
		return;
	}

	freerdp_bitmap_planar_context_free(ctx->planar);
	Stream_Free(ctx->bs, TRUE);
	Stream_Free(ctx->bts, TRUE);
	winpr_aligned_free(ctx->imageCopyBuffer);
	free(ctx->rects);
	delete (ctx);

	e->bmpContext = nullptr;
}

static BOOL ogon_create_encoder_bmp_context(ogon_bitmap_encoder *encoder) {
	ogon_bmp_context *ctx = nullptr;
	UINT32 bufferSize;
	UINT32 maxBitmapSize;

	if (!encoder || encoder->bmpContext) {
		return FALSE;
	}

	if (!(ctx = new (ogon_bmp_context))) {
		goto fail;
	}
	encoder->bmpContext = ctx;

	if (!(ctx->planar = freerdp_bitmap_planar_context_new(
				  PLANAR_FORMAT_HEADER_NA | PLANAR_FORMAT_HEADER_RLE, 64,
				  64))) {
		goto fail;
	}

	maxBitmapSize =
			64 * 64 * encoder->dstBytesPerPixel + 3; /* 3 == RLE header */

	if (!(ctx->imageCopyBuffer = static_cast<BYTE *>(
				  winpr_aligned_malloc(maxBitmapSize, 16)))) {
		goto fail;
	}

	bufferSize = encoder->desktopWidth * encoder->desktopHeight *
				 encoder->dstBytesPerPixel;
	bufferSize += bufferSize / 10;

	if (!(ctx->bs = Stream_New(nullptr, bufferSize))) {
		goto fail;
	}

	if (!(ctx->bts = Stream_New(nullptr, maxBitmapSize))) {
		goto fail;
	}

	return TRUE;

fail:
	ogon_delete_encoder_bmp_context(encoder);
	return FALSE;
}

#ifdef WITH_ENCODER_STATS
static void ogon_print_encoder_stopwatchxx(STOPWATCH *sw, const char *title) {
	double s = stopwatch_get_elapsed_time_in_seconds(sw);
	double avg = sw->count == 0 ? 0 : s / sw->count;
	WLog_DBG(TAG, "%-20s | %10u | %10.4fs | %8.6fs | %6.0f", title, sw->count,
			s, avg, sw->count / s);
}

static void ogon_print_encoder_stopwatches(ogon_bitmap_encoder *e) {
	WLog_DBG(TAG,
			"------------------------------------------------------------+-----"
			"--");
	WLog_DBG(TAG,
			"STOPWATCH            |      COUNT |       TOTAL |       AVG |    "
			"IPS");
	WLog_DBG(TAG,
			"---------------------+------------+-------------+-----------+-----"
			"--");
	ogon_print_encoder_stopwatchxx(e->swSendSurfaceBits, "sendSurfaceBits");
	ogon_print_encoder_stopwatchxx(e->swSimplifyDamage, "simplifyDamage");
	ogon_print_encoder_stopwatchxx(e->swSendGraphicsBits, "sendGraphicsBits");
	ogon_print_encoder_stopwatchxx(
			e->swFramebufferCompare, "framebufferCompare");
	ogon_print_encoder_stopwatchxx(e->swSendBitmapUpdate, "sendBitmapUpdate");
	ogon_print_encoder_stopwatchxx(e->swBitmapCompress, "bitmapCompress");
	ogon_print_encoder_stopwatchxx(e->swH264Compress, "h264Compress");
	ogon_print_encoder_stopwatchxx(e->swImageCopy, "imageCopy");
	WLog_DBG(TAG,
			"------------------------------------------------------------+-----"
			"--");
}

static void ogon_delete_encoder_stopwatches(ogon_bitmap_encoder *e) {
	STOPWATCH *sw;
	if ((sw = e->swSendSurfaceBits)) {
		stopwatch_free(sw);
	}
	if ((sw = e->swSimplifyDamage)) {
		stopwatch_free(sw);
	}
	if ((sw = e->swSendGraphicsBits)) {
		stopwatch_free(sw);
	}
	if ((sw = e->swFramebufferCompare)) {
		stopwatch_free(sw);
	}
	if ((sw = e->swSendBitmapUpdate)) {
		stopwatch_free(sw);
	}
	if ((sw = e->swBitmapCompress)) {
		stopwatch_free(sw);
	}
	if ((sw = e->swH264Compress)) {
		stopwatch_free(sw);
	}
	if ((sw = e->swImageCopy)) {
		stopwatch_free(sw);
	}
}

static BOOL ogon_create_encoder_stopwatches(ogon_bitmap_encoder *e) {
	if (!e) {
		return FALSE;
	}

	if (!(e->swSendSurfaceBits = stopwatch_create())) {
		goto fail;
	}
	if (!(e->swSimplifyDamage = stopwatch_create())) {
		goto fail;
	}
	if (!(e->swSendGraphicsBits = stopwatch_create())) {
		goto fail;
	}
	if (!(e->swFramebufferCompare = stopwatch_create())) {
		goto fail;
	}
	if (!(e->swSendBitmapUpdate = stopwatch_create())) {
		goto fail;
	}
	if (!(e->swBitmapCompress = stopwatch_create())) {
		goto fail;
	}
	if (!(e->swH264Compress = stopwatch_create())) {
		goto fail;
	}
	if (!(e->swImageCopy = stopwatch_create())) {
		goto fail;
	}

	return TRUE;

fail:
	ogon_delete_encoder_stopwatches(e);
	return FALSE;
}
#endif /* WITH_ENCODER_STATS */

ogon_bitmap_encoder *ogon_bitmap_encoder_new(int desktopWidth,
		int desktopHeight, int bitsPerPixel, int bytesPerPixel, int scanLine,
		int dstBitsPerPixel, unsigned int multifragMaxRequestSize) {
	ogon_bitmap_encoder *encoder;

	if ((desktopWidth < 1) || (desktopHeight < 1) ||
			(scanLine < desktopWidth * bytesPerPixel)) {
		WLog_ERR(TAG, "invalid encoder parameters");
		goto out;
	}

	if ((scanLine % 16) != 0) {
		WLog_ERR(TAG, "error, scanline %d is not a multiple of 16", scanLine);
		goto out;
	}

	if (multifragMaxRequestSize < OGON_ENCODER_MIN_MULTIFRAG_REQUEST_SIZE) {
		WLog_ERR(TAG, "error, multifragMaxRequestSize %u is too low",
				multifragMaxRequestSize);
		goto out;
	}

	encoder = (ogon_bitmap_encoder *)calloc(1, sizeof(ogon_bitmap_encoder));
	if (!encoder) {
		WLog_ERR(TAG, "error allocating encoder memory");
		goto out;
	}

	encoder->srcFormat = PIXEL_FORMAT_BGRX32;

	switch (dstBitsPerPixel) {
		case 32:
			encoder->dstFormat = PIXEL_FORMAT_RGBX32;
			break;
		case 24:
			encoder->dstFormat = PIXEL_FORMAT_RGB24;
			break;
		case 16:
			encoder->dstFormat = PIXEL_FORMAT_RGB16;
			break;
		case 15:
			encoder->dstFormat = PIXEL_FORMAT_RGB15;
			break;
#if 0
		case 8: /* not working yet */
			encoder->dstFormat = PIXEL_FORMAT_RGB8;
			break;
#endif
		default:
			WLog_ERR(TAG, "error, unsupported client color depth: %d",
					dstBitsPerPixel);
			goto fail;
	}

	encoder->dstBytesPerPixel = (dstBitsPerPixel + 7) / 8;
	encoder->dstBitsPerPixel = dstBitsPerPixel;
	encoder->multifragMaxRequestSize = multifragMaxRequestSize;

	encoder->desktopWidth = desktopWidth;
	encoder->desktopHeight = desktopHeight;

	encoder->bitsPerPixel = bitsPerPixel;
	encoder->bytesPerPixel = bytesPerPixel;
	encoder->scanLine = scanLine;

	if (!(encoder->clientView = (BYTE *)winpr_aligned_malloc(
				  desktopHeight * scanLine, 256))) {
		goto fail;
	}

	ogon_encoder_blank_client_view_area(encoder, nullptr);

	if (!(encoder->stream = Stream_New(nullptr, 4096))) {
		goto fail;
	}

	if (!ogon_create_encoder_bmp_context(encoder)) {
		goto fail;
	}

	if (!(encoder->rfx_context = rfx_context_new(TRUE))) {
		goto fail;
	}
	encoder->rfx_context->mode = RLGR3;
	encoder->rfx_context->width = desktopWidth;
	encoder->rfx_context->height = desktopHeight;

	if (encoder->bytesPerPixel == 4) {
		rfx_context_set_pixel_format(encoder->rfx_context, PIXEL_FORMAT_BGRA32);
	} else if (encoder->bytesPerPixel == 3) {
		rfx_context_set_pixel_format(encoder->rfx_context, PIXEL_FORMAT_BGR24);
	} else {
		WLog_ERR(TAG, "don't know how to handle bytesPerPixel=%" PRIu32 "",
				encoder->bytesPerPixel);
	}

	if (!(encoder->debug_context = freerdp_bitmap_planar_context_new(
				  PLANAR_FORMAT_HEADER_NA | PLANAR_FORMAT_HEADER_RLE,
				  desktopWidth, 8))) {
		goto fail;
	}

	if (!(encoder->debug_buffer =
						static_cast<BYTE *>(calloc(desktopWidth * 8, 4)))) {
		goto fail;
	}

	region16_init(&encoder->accumulatedDamage);

#if defined(USE_FREERDP_H264)
	encoder->fh264_context = h264_context_new(TRUE);
	if (!encoder->fh264_context) {
		WLog_DBG(TAG, "failed to initialize the freerdp H264 encoder");
		/* don't fail, the codec library might not be installed */
	}
#endif
#ifdef WITH_OPENH264
	if (!(encoder->h264_context =
						ogon_openh264_context_new(encoder->desktopWidth,
								encoder->desktopHeight, encoder->scanLine))) {
		WLog_DBG(TAG, "failed to initialize the OpenH264 encoder");
		/* don't fail, the codec library might not be installed */
	}
#endif

#ifdef WITH_ENCODER_STATS
	if (!ogon_create_encoder_stopwatches(encoder)) {
		goto fail;
	}
#endif

	return encoder;

fail:
	ogon_bitmap_encoder_free(encoder);
out:
	WLog_ERR(TAG, "encoder creation failed");
	return nullptr;
}

void ogon_bitmap_encoder_free(ogon_bitmap_encoder *encoder) {
	if (!encoder) {
		return;
	}

	winpr_aligned_free(encoder->clientView);
	region16_uninit(&encoder->accumulatedDamage);
	free(encoder->rdpRects);

	Stream_Free(encoder->stream, TRUE);

	ogon_delete_encoder_bmp_context(encoder);
	rfx_context_free(encoder->rfx_context);

	free(encoder->debug_buffer);
	freerdp_bitmap_planar_context_free(encoder->debug_context);

#if defined(USE_FREERDP_H264)
	h264_context_free(encoder->fh264_context);
#endif
#ifdef WITH_OPENH264
	ogon_openh264_context_free(encoder->h264_context);
#endif

#ifdef WITH_ENCODER_STATS
	ogon_print_encoder_stopwatches(encoder);
	ogon_delete_encoder_stopwatches(encoder);
#endif

	free(encoder);
}

BOOL ogon_bitmap_encoder_update_maxrequest_size(
		ogon_bitmap_encoder *encoder, unsigned int multifragMaxRequestSize) {
	if (!encoder) {
		return FALSE;
	}

	if (multifragMaxRequestSize < OGON_ENCODER_MIN_MULTIFRAG_REQUEST_SIZE) {
		return FALSE;
	}

	encoder->multifragMaxRequestSize = multifragMaxRequestSize;

	return TRUE;
}

void ogon_encoder_blank_client_view_area(
		ogon_bitmap_encoder *encoder, RECTANGLE_16 *r) {
	BYTE *dst = nullptr;
	int i;

	/**
	 * Set the requested rectangle of the encoder client view to 1 so that the
	 * alpha channel will be set: In the backend the pixels' alpha values are
	 * most likely set to 0x00 or 0xFF. This prevents the damage region
	 * simplification from optimizing out this region for cases where we must
	 * assume that the client view buffer is not in sync with the rdp client's
	 * view.
	 */

	if (!r) {
		memset(encoder->clientView, 1,
				encoder->desktopHeight * encoder->scanLine);
		return;
	}

	if (r->top >= encoder->desktopHeight) {
		return;
	}
	if (r->left >= encoder->desktopWidth) {
		return;
	}
	if (r->bottom >= encoder->desktopHeight) {
		r->bottom = encoder->desktopHeight;
	}
	if (r->right >= encoder->desktopWidth) {
		r->right = encoder->desktopWidth;
	}
	if (r->top >= r->bottom) {
		return;
	}
	if (r->left >= r->right) {
		return;
	}

	dst = encoder->clientView + (r->top * encoder->scanLine) +
		  (r->left * encoder->bytesPerPixel);

	for (i = r->top; i < r->bottom; i++) {
		memset(dst, 1, (r->right - r->left) * encoder->bytesPerPixel);
		dst += encoder->scanLine;
	}
}
