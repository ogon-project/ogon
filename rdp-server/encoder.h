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

#ifndef _OGON_RDPSRV_ENCODER_H_
#define _OGON_RDPSRV_ENCODER_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/codec/region.h>

#include "openh264.h"

#ifdef WITH_ENCODER_STATS
#include <freerdp/utils/stopwatch.h>
#define STOPWATCH_START(sw)	stopwatch_start(sw)
#define STOPWATCH_STOP(sw)	stopwatch_stop(sw)
#else
#define STOPWATCH_START(sw)	do { } while (0)
#define STOPWATCH_STOP(sw)	do { } while (0)
#endif

typedef struct _ogon_bmp_context {
	BITMAP_PLANAR_CONTEXT *planar;
	wStream *bs;
	wStream *bts;
	BYTE *imageCopyBuffer;
	BITMAP_DATA *rects;
	UINT32 rectsAllocated;
} ogon_bmp_context;


/** @brief encoder state */
typedef struct _ogon_bitmap_encoder {
	UINT32 desktopWidth;
	UINT32 desktopHeight;

	UINT32 bitsPerPixel;
	UINT32 bytesPerPixel;
	UINT32 scanLine;

	UINT32 dstBitsPerPixel;
	UINT32 dstBytesPerPixel;
	UINT32 multifragMaxRequestSize;

	UINT32 srcFormat;
	UINT32 dstFormat;

	BYTE *clientView;

	REGION16 accumulatedDamage;
	RDP_RECT *rdpRects;
	UINT32 rdpRectsAllocated;

	wStream *stream;

	ogon_bmp_context *bmpContext;

	RFX_CONTEXT *rfx_context;

	BITMAP_PLANAR_CONTEXT *debug_context;
	BYTE* debug_buffer;

#ifdef WITH_OPENH264
	ogon_h264_context *h264_context;
#endif

#ifdef WITH_ENCODER_STATS
	STOPWATCH *swSendSurfaceBits;
	STOPWATCH *swSimplifyDamage;
	STOPWATCH *swSendGraphicsBits;
	STOPWATCH *swFramebufferCompare;
	STOPWATCH *swSendBitmapUpdate;
	STOPWATCH *swBitmapCompress;
	STOPWATCH *swH264Compress;
	STOPWATCH *swImageCopy;
#endif

} ogon_bitmap_encoder;


ogon_bitmap_encoder *ogon_bitmap_encoder_new( int desktopWidth,
	int desktopHeight, int bitsPerPixel, int bytesPerPixel,	int scanLine,
	int dstBitsPerPixel, unsigned int multifragMaxRequestSize);

void ogon_bitmap_encoder_free(ogon_bitmap_encoder *encoder);

BOOL ogon_bitmap_encoder_update_maxrequest_size(ogon_bitmap_encoder *encoder,
	unsigned int multifragMaxRequestSize
);

void ogon_encoder_blank_client_view_area(ogon_bitmap_encoder *encoder, RECTANGLE_16 *r);

#endif /* _OGON_RDPSRV_ENCODER_H_ */
