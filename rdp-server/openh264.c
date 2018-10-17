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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WITH_OPENH264

#include <dlfcn.h>

#include <winpr/environment.h>
#include <winpr/sysinfo.h>
#include <freerdp/primitives.h>

#include "../3rdparty/openh264/codec_api.h"
#include "../3rdparty/openh264/codec_ver.h"

#include "../common/global.h"
#include "openh264.h"

#define TAG OGON_TAG("core.openh264")

typedef int (*pfn_create_openh264_encoder)(ISVCEncoder **ppEncoder);
typedef void (*pfn_destroy_openh264_encoder)(ISVCEncoder *pEncoder);
typedef void (*pfn_get_openh264_version)(OpenH264Version *pVersion);

pfn_create_openh264_encoder create_openh264_encoder = NULL;
pfn_destroy_openh264_encoder destroy_open_h264_encoder = NULL;
pfn_get_openh264_version get_openh264_version = NULL;
primitives_t *freerdp_primitives = NULL;

static void *openh264lib = NULL;
static BOOL h264_init_success = FALSE;

struct _ogon_h264_context {
	ISVCEncoder *pEncoder;
	SSourcePicture pic1;
	SSourcePicture pic2;
	UINT32 scrWidth;
	UINT32 scrHeight;
	UINT32 scrStride;
	UINT32 maxBitRate;
	UINT32 frameRate;
	UINT32 bitRate;
	UINT32 nullCount;
	UINT32 nullValue;
};

BOOL ogon_openh264_compress(ogon_h264_context *h264, UINT32 newFrameRate,
	UINT32 targetFrameSizeInBits, BYTE *data, BYTE **ppDstData,
	UINT32 *pDstSize, BOOL avcMode, BOOL *pOptimizable)
{
	SFrameBSInfo info;
	prim_size_t screenSize;
	pstatus_t pstatus = PRIMITIVES_SUCCESS;
	int i, j;

	if (!h264 || !h264_init_success) {
		return FALSE;
	}

	screenSize.width = (INT32)h264->scrWidth;
	screenSize.height = (INT32)h264->scrHeight;

	/**
	 * avcMode parameter:
	 * 0: AVC420
	 * 1: AVC444 step 1/2: convert and encode
	 * 2: AVC444 step 2/2: just encode
	 */

	switch(avcMode) {
	case 0:
		pstatus = freerdp_primitives->RGBToYUV420_8u_P3AC4R(
				data, PIXEL_FORMAT_BGRA32, h264->scrStride,
				h264->pic1.pData, (UINT32 *) h264->pic1.iStride,
				&screenSize);
		break;
	case 1:
		// pstatus = ogon_RGBToAVC444YUV_BGRX(
		pstatus = freerdp_primitives->RGBToAVC444YUV(
				data, PIXEL_FORMAT_BGRA32, h264->scrStride,
				h264->pic1.pData, (UINT32 *) h264->pic1.iStride,
				h264->pic2.pData, (UINT32 *) h264->pic2.iStride,
				&screenSize);
		break;
	}

	if (pstatus != PRIMITIVES_SUCCESS) {
		WLog_ERR(TAG, "yuv conversion failed");
		return FALSE;
	}

	if (newFrameRate && newFrameRate <= 60 && h264->frameRate != newFrameRate) {
		float framerate = (float)newFrameRate;
		if ((*h264->pEncoder)->SetOption(h264->pEncoder, ENCODER_OPTION_FRAME_RATE, &framerate)) {
			WLog_ERR(TAG, "Failed to set encoder frame rate to %f", framerate);
			return FALSE;
		}
		WLog_DBG(TAG, "Changed openh264 frame rate from %"PRIu32" to %"PRIu32" (max is 60)", h264->frameRate, newFrameRate);
		h264->frameRate = newFrameRate;
	}

	if (targetFrameSizeInBits) {
		UINT32 newBitRate = targetFrameSizeInBits * h264->frameRate;

		if (newBitRate > h264->maxBitRate) {
			newBitRate = h264->maxBitRate;
		}

		if (newBitRate != h264->bitRate) {
			SBitrateInfo bitrate;
			bitrate.iLayer = SPATIAL_LAYER_ALL;
			bitrate.iBitrate = newBitRate;
			if ((*h264->pEncoder)->SetOption(h264->pEncoder, ENCODER_OPTION_BITRATE, &bitrate)) {
				WLog_ERR(TAG, "Failed to set encoder bitrate to %d", bitrate.iBitrate);
				return FALSE;
			}
			/* WLog_DBG(TAG, "Changed bitrate from %"PRIu32" to %d", h264->bitRate, bitrate.iBitrate); */
			h264->bitRate = newBitRate;
		}
	}

	memset(&info, 0, sizeof(SFrameBSInfo));

	if ((*h264->pEncoder)->EncodeFrame(h264->pEncoder, avcMode==2 ? &h264->pic2 : &h264->pic1, &info)) {
		WLog_ERR(TAG, "Failed to encode frame");
		return FALSE;
	}

	if (info.eFrameType == videoFrameTypeSkip) {
		WLog_WARN(TAG, "frame was skipped!");
		return FALSE;
	}

	*ppDstData = info.sLayerInfo[0].pBsBuf;
	*pDstSize = 0;

	for (i = 0; i < info.iLayerNum; i++) {
		for (j = 0; j < info.sLayerInfo[i].iNalCount; j++) {
			*pDstSize += info.sLayerInfo[i].pNalLengthInByte[j];
		}
	}
	/* WLog_DBG(TAG, "ENCODED SIZE (mode=%"PRIu32"): %"PRIu32" byte (%"PRIu32" bits", avcMode, *pDstSize, (*pDstSize) * 8); */

	/**
	 * TODO:
	 * Maybe there is a better way to detect if encoding the same
	 * buffer again will actually improve quality.
	 * For now we consider 10 encodings with size <= h264->nullValue in a row
	 * as final.
	 */

	if (*pDstSize > h264->nullValue) {
		h264->nullCount = 0;
	} else {
		h264->nullCount++;
	}

	if (pOptimizable) {
		*pOptimizable = (h264->nullCount >= 10) ? FALSE : TRUE;
	}

	return TRUE;
}

void ogon_openh264_context_free(ogon_h264_context *h264) {
	if (!h264) {
		return;
	}

	if (h264->pEncoder) {
		destroy_open_h264_encoder(h264->pEncoder);
	}

	_aligned_free(h264->pic1.pData[0]);
	_aligned_free(h264->pic1.pData[1]);
	_aligned_free(h264->pic1.pData[2]);

	_aligned_free(h264->pic2.pData[0]);
	_aligned_free(h264->pic2.pData[1]);
	_aligned_free(h264->pic2.pData[2]);

	free(h264);

	return;
}

ogon_h264_context *ogon_openh264_context_new(UINT32 scrWidth, UINT32 scrHeight,
	UINT32 scrStride)
{
	ogon_h264_context *h264 = NULL;
	UINT32 h264Width;
	UINT32 h264Height;
	SEncParamExt encParamExt;
	SBitrateInfo bitrate;
	SYSTEM_INFO sysinfo;
	size_t ysize, usize, vsize;

	if (!h264_init_success) {
		WLog_ERR(TAG, "Cannot create OpenH264 context: library was not initialized");
		return NULL;
	}

	if (scrWidth < 16 || scrHeight < 16) {
		WLog_ERR(TAG, "Error: Minimum height and width for OpenH264 is 16 but we got %"PRIu32" x %"PRIu32"", scrWidth, scrHeight);
		return NULL;
	}

	if (scrWidth % 16) {
		WLog_WARN(TAG, "WARNING: screen width %"PRIu32" is not a multiple of 16. Expect degraded H.264 performance!", scrWidth);
	}

	if (!(h264 = (ogon_h264_context *)calloc(1, sizeof(ogon_h264_context)))) {
		WLog_ERR(TAG, "Failed to allocate OpenH264 context");
		return NULL;
	}

	ZeroMemory(&sysinfo, sizeof(sysinfo));
	GetNativeSystemInfo(&sysinfo);

	/**
	 * [MS-RDPEGFX 2.2.4.4 RFX_AVC420_BITMAP_STREAM]
	 *
	 * The width and height of the MPEG-4 AVC/H.264 codec bitstream MUST be aligned to a
	 * multiple of 16.
	 */

	h264Width = (scrWidth + 15) & ~15;    /* codec bitstream width must be a multiple of 16 */
	h264Height = (scrHeight + 15) & ~15;  /* codec bitstream height must be a multiple of 16 */

	h264->scrWidth = scrWidth;
	h264->scrHeight = scrHeight;
	h264->scrStride = scrStride;

	h264->pic1.iPicWidth = h264->pic2.iPicWidth = h264Width;
	h264->pic1.iPicHeight = h264->pic2.iPicHeight = h264Height;
	h264->pic1.iColorFormat = h264->pic2.iColorFormat = videoFormatI420;

	h264->pic1.iStride[0] = h264->pic2.iStride[0] = h264Width;
	h264->pic1.iStride[1] = h264->pic2.iStride[1] = h264Width / 2;
	h264->pic1.iStride[2] = h264->pic2.iStride[2] = h264Width / 2;

	h264->frameRate = 20;
	h264->bitRate = 1000000 * 2; /* 2 Mbit/s */

	ysize = h264Width * h264Height;
	usize = vsize = ysize >> 2;

	if (!(h264->pic1.pData[0] = (unsigned char*) _aligned_malloc(ysize, 256))) {
		goto err;
	}
	if (!(h264->pic1.pData[1] = (unsigned char*) _aligned_malloc(usize, 256))) {
		goto err;
	}
	if (!(h264->pic1.pData[2] = (unsigned char*) _aligned_malloc(vsize, 256))) {
		goto err;
	}

	if (!(h264->pic2.pData[0] = (unsigned char*) _aligned_malloc(ysize, 256))) {
		goto err;
	}
	if (!(h264->pic2.pData[1] = (unsigned char*) _aligned_malloc(usize, 256))) {
		goto err;
	}
	if (!(h264->pic2.pData[2] = (unsigned char*) _aligned_malloc(vsize, 256))) {
		goto err;
	}

	memset(h264->pic1.pData[0], 0, ysize);
	memset(h264->pic1.pData[1], 0, usize);
	memset(h264->pic1.pData[2], 0, vsize);

	memset(h264->pic2.pData[0], 0, ysize);
	memset(h264->pic2.pData[1], 0, usize);
	memset(h264->pic2.pData[2], 0, vsize);

	if ((create_openh264_encoder(&h264->pEncoder) != 0) || !h264->pEncoder) {
		WLog_ERR(TAG, "Failed to create H.264 encoder");
		goto err;
	}

	ZeroMemory(&encParamExt, sizeof(encParamExt));
	if ((*h264->pEncoder)->GetDefaultParams(h264->pEncoder, &encParamExt)) {
		WLog_ERR(TAG, "Failed to retrieve H.264 default ext params");
		goto err;
	}

	encParamExt.iUsageType = SCREEN_CONTENT_REAL_TIME;
	encParamExt.iPicWidth = h264Width;
	encParamExt.iPicHeight = h264Height;
	encParamExt.iRCMode = RC_BITRATE_MODE;
	encParamExt.fMaxFrameRate = (float)h264->frameRate;
	encParamExt.iTargetBitrate = h264->bitRate;
	encParamExt.iMaxBitrate = UNSPECIFIED_BIT_RATE;
	encParamExt.bEnableDenoise = 0;
	encParamExt.bEnableLongTermReference = 0;
	encParamExt.bEnableFrameSkip = 0;
	encParamExt.iSpatialLayerNum = 1;
	encParamExt.sSpatialLayers[0].fFrameRate = encParamExt.fMaxFrameRate;
	encParamExt.sSpatialLayers[0].iVideoWidth = encParamExt.iPicWidth;
	encParamExt.sSpatialLayers[0].iVideoHeight = encParamExt.iPicHeight;
	encParamExt.sSpatialLayers[0].iSpatialBitrate = encParamExt.iTargetBitrate;
	encParamExt.sSpatialLayers[0].iMaxSpatialBitrate = encParamExt.iMaxBitrate;

	encParamExt.iMultipleThreadIdc = 1;
	encParamExt.sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;
	encParamExt.sSpatialLayers[0].sSliceArgument.uiSliceNum = 1;

	/**
	 * FIXME:
	 * openh264 crashes on low screen sizes if multi-threading is enabled
	 * test case: xfreerdp /gfx-h264 /size:128x64
	 */
	if (h264Width >= 320 && h264Height >= 320) {
		encParamExt.iMultipleThreadIdc = (unsigned short) sysinfo.dwNumberOfProcessors;
	}

	if (encParamExt.iMultipleThreadIdc > 1) {
		encParamExt.sSpatialLayers[0].sSliceArgument.uiSliceMode = SM_FIXEDSLCNUM_SLICE;
		encParamExt.sSpatialLayers[0].sSliceArgument.uiSliceNum = encParamExt.iMultipleThreadIdc;
		h264->nullValue = 20 * encParamExt.iMultipleThreadIdc;
		WLog_DBG(TAG, "Using %hu threads for h.264 encoding (nullValue=%"PRIu32")", encParamExt.iMultipleThreadIdc, h264->nullValue);
	} else {
		h264->nullValue = 16;
	}

	if ((*h264->pEncoder)->InitializeExt(h264->pEncoder, &encParamExt)) {
		WLog_ERR(TAG, "Failed to initialize H.264 encoder");
		goto err;
	}

	bitrate.iLayer = SPATIAL_LAYER_ALL;
	bitrate.iBitrate = h264->bitRate;
	if ((*h264->pEncoder)->SetOption(h264->pEncoder, ENCODER_OPTION_BITRATE, &bitrate)) {
		WLog_ERR(TAG, "Failed to set encoder bitrate to %d", bitrate.iBitrate);
		goto err;
	}

	bitrate.iLayer = SPATIAL_LAYER_0;
	bitrate.iBitrate = 0;
	if ((*h264->pEncoder)->GetOption(h264->pEncoder, ENCODER_OPTION_MAX_BITRATE, &bitrate)) {
		WLog_ERR(TAG, "Failed to get encoder max bitrate");
		goto err;
	}

	h264->maxBitRate = bitrate.iBitrate;
	/* WLog_DBG(TAG, "maxBitRate: %"PRIu32"", h264->maxBitRate); */

	return h264;

err:
	if (h264) {
		if (h264->pEncoder) {
			destroy_open_h264_encoder(h264->pEncoder);
		}
		_aligned_free(h264->pic1.pData[0]);
		_aligned_free(h264->pic1.pData[1]);
		_aligned_free(h264->pic1.pData[2]);
		_aligned_free(h264->pic2.pData[0]);
		_aligned_free(h264->pic2.pData[1]);
		_aligned_free(h264->pic2.pData[2]);
		free (h264);
	}

	return NULL;
}

void ogon_openh264_library_close(void) {
	if (openh264lib) {
		dlclose(openh264lib);
		openh264lib = NULL;
	}
}

BOOL ogon_openh264_library_open(void) {
	char* libh264;
	OpenH264Version cver;

	if (h264_init_success) {
		WLog_WARN(TAG, "OpenH264 was already successfully initialized");
		return TRUE;
	}

	if (!(freerdp_primitives = primitives_get())) {
		WLog_ERR(TAG, "Failed to get FreeRDP primitives");
		goto fail;
	}

	libh264 = getenv("LIBOPENH264");
	if (libh264) {
		WLog_DBG(TAG, "Loading OpenH264 library specified in environment: %s", libh264);
		if (!(openh264lib = dlopen(libh264, RTLD_NOW))) {
			WLog_ERR(TAG, "Failed to load OpenH264 library: %s", dlerror());
			/* don't fail yet, we'll try to load the default library below ! */
		}
	}

	if (!openh264lib) {
		WLog_DBG(TAG, "Loading default OpenH264 library: %s", OGON_OPENH264_LIBRARY);
		if (!(openh264lib = dlopen(OGON_OPENH264_LIBRARY, RTLD_NOW))) {
			WLog_WARN(TAG, "Failed to load OpenH264 library: %s", dlerror());
			goto fail;
		}
	}

	if (!(create_openh264_encoder = (pfn_create_openh264_encoder)dlsym(openh264lib, "WelsCreateSVCEncoder"))) {
		WLog_ERR(TAG, "Failed to get OpenH264 encoder creation function: %s", dlerror());
		goto fail;
	}

	if (!(destroy_open_h264_encoder = (pfn_destroy_openh264_encoder)dlsym(openh264lib, "WelsDestroySVCEncoder"))) {
		WLog_ERR(TAG, "Failed to get OpenH264 encoder destroy function: %s", dlerror());
		goto fail;
	}

	if (!(get_openh264_version = (pfn_get_openh264_version)dlsym(openh264lib, "WelsGetCodecVersionEx"))) {
		WLog_ERR(TAG, "Failed to get OpenH264 version function: %s", dlerror());
		goto fail;
	}

	ZeroMemory(&cver, sizeof(cver));

	get_openh264_version(&cver);

	WLog_DBG(TAG, "OpenH264 codec version: %u.%u.%u.%u",
			cver.uMajor, cver.uMinor, cver.uRevision, cver.uReserved);

	if (cver.uMajor != OPENH264_MAJOR || cver.uMinor != OPENH264_MINOR) {
		WLog_ERR(TAG, "The loaded OpenH264 library is incompatible with this build (%d.%d.%d.%d)",
			OPENH264_MAJOR, OPENH264_MINOR, OPENH264_REVISION, OPENH264_RESERVED);
		goto fail;
	}

	WLog_DBG(TAG, "Successfully initialized OpenH264 library");

	h264_init_success = TRUE;
	return TRUE;

fail:
	create_openh264_encoder = NULL;
	destroy_open_h264_encoder = NULL;
	get_openh264_version = NULL;
	ogon_openh264_library_close();
	h264_init_success = FALSE;
	return FALSE;
}

#endif /* WITH_OPENH264 defined */
