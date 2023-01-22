/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * OpenH264 Encoder
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
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

#ifndef OGON_RDPSRV_OPENH264_H_
#define OGON_RDPSRV_OPENH264_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WITH_OPENH264

#include <freerdp/freerdp.h>

typedef struct _ogon_h264_context ogon_h264_context;

typedef enum _ogon_openh264_compress_mode {
        COMPRESS_MODE_AVC420,      /* AVC420 frame only */
        COMPRESS_MODE_AVC444V1_A,  /* AVC444v1 step 1/2: yuv conversion and compress pic1 */
        COMPRESS_MODE_AVC444V2_A,  /* AVC444v2 step 1/2: yuv conversion and compress pic1 */
        COMPRESS_MODE_AVC444VX_B,  /* AVC444vX step 2/2: compress pic2 only */
} ogon_openh264_compress_mode;


BOOL ogon_openh264_library_open(void);
void ogon_openh264_library_close(void);
BOOL ogon_openh264_compress(ogon_h264_context *h264, UINT32 newFrameRate,
		UINT32 targetFrameSizeInBits, const BYTE *data, BYTE **ppDstData,
		UINT32 *pDstSize, ogon_openh264_compress_mode avcMode,
		BOOL *pOptimizable);
void ogon_openh264_context_free(ogon_h264_context *h264);
ogon_h264_context *ogon_openh264_context_new(UINT32 scrWidth, UINT32 scrHeight, UINT32 scrStride);


#endif /* WITH_OPENH264 defined   */
#endif /* OGON_RDPSRV_OPENH264_H_ */
