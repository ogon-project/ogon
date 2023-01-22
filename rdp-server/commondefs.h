/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Common Definitions
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * David Fort <contact@hardening-consulting.com>
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

#ifndef OGON_RDPSRV_COMMONDEFS_H_
#define OGON_RDPSRV_COMMONDEFS_H_

#include <winpr/wtypes.h>

/** @brief holds data describing a screen */
typedef struct _ogon_screen_infos {
	UINT32 width;
	UINT32 height;
	UINT32 bpp;
	UINT32 bytesPerPixel;
	UINT32 scanline;
} ogon_screen_infos;


/** @brief the kind of codec to use when sending bitmap content */
typedef enum _ogon_codec_mode {
	CODEC_MODE_BMP,  /* rdp planar */
	CODEC_MODE_NSC,  /* nscodec */
	CODEC_MODE_RFX1, /* rfx/core */
	CODEC_MODE_RFX2, /* rfx/gfx */
	CODEC_MODE_RFX3, /* rfx-progressive/gfx */
	CODEC_MODE_H264, /* h264/gfx */
} ogon_codec_mode;

/** @brief filters that can be applied on a spying/shadowing connection */
enum {
	INPUT_FILTER_MOUSE = 		0x1,
	INPUT_FILTER_KEYBOARD = 	0x2,
};

/** @brief keyboard modifiers */
enum {
	OGON_KEYBOARD_CTRL = 0x01,
	OGON_KEYBOARD_ALT = 	0x02,
	OGON_KEYBOARD_SHIFT = 0x04,
};
/** @brief used to track the state of modifiers keys */
typedef UINT16 ogon_keyboard_modifiers_state;

/** @brief used to track the state of identifier keys */
typedef UINT16 ogon_keyboard_indicator_state;


/** @brief properties of a backend (name and cookies) */
typedef struct _ogon_backend_props {
	char *serviceEndpoint;
	char *ogonCookie;
	char *backendCookie;
} ogon_backend_props;


typedef struct _ogon_connection ogon_connection;
typedef struct _ogon_front_connection ogon_front_connection;
typedef struct _ogon_backend_connection ogon_backend_connection;

#endif /* OGON_RDPSRV_COMMONDEFS_H_ */
