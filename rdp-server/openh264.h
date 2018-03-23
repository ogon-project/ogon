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

#ifndef _OGON_RDPSRV_OPENH264_H_
#define _OGON_RDPSRV_OPENH264_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WITH_OPENH264

#include <freerdp/freerdp.h>

typedef struct _ogon_h264_context ogon_h264_context;

BOOL ogon_openh264_library_open(void);
void ogon_openh264_library_close(void);
BOOL ogon_openh264_compress(ogon_h264_context *h264, UINT32 newFrameRate,
                            UINT32 targetFrameSizeInBits, BYTE *data, BYTE **ppDstData,
                            UINT32 *pDstSize, BOOL avcMode, BOOL *pOptimizable);
void ogon_openh264_context_free(ogon_h264_context *h264);
ogon_h264_context *ogon_openh264_context_new(UINT32 scrWidth, UINT32 scrHeight, UINT32 scrStride);


#endif /* WITH_OPENH264 defined   */
#endif /* _OGON_RDPSRV_OPENH264_H_ */
