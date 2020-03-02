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

#ifndef _OGON_RDPSRV_RDPGFX_CHANNEL_H_
#define _OGON_RDPSRV_RDPGFX_CHANNEL_H_

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/rdpgfx.h>

#include "channels.h"

typedef enum _rdpgfx_server_open_result
{
	RDPGFX_SERVER_OPEN_RESULT_OK = 0,
	RDPGFX_SERVER_OPEN_RESULT_CLOSED = 1,
	RDPGFX_SERVER_OPEN_RESULT_NOTSUPPORTED = 2,
	RDPGFX_SERVER_OPEN_RESULT_ERROR = 3
} rdpgfx_server_open_result;

typedef struct _rdpgfx_server_context rdpgfx_server_context;

typedef void (*pfn_rdpgfx_server_open)(rdpgfx_server_context *context);
typedef void (*pfn_rdpgfx_server_close)(rdpgfx_server_context *context);
typedef BOOL (*pfn_rdpgfx_server_wire_to_surface1)(rdpgfx_server_context *context, RDPGFX_WIRE_TO_SURFACE_PDU_1 *wire_to_surface_1);
typedef BOOL (*pfn_rdpgfx_server_wire_to_surface2)(rdpgfx_server_context *context, RDPGFX_WIRE_TO_SURFACE_PDU_2 *wire_to_surface_2);
typedef BOOL (*pfn_rdpgfx_server_solid_fill)(rdpgfx_server_context *context, RDPGFX_SOLID_FILL_PDU *solid_fill);
typedef BOOL (*pfn_rdpgfx_server_create_surface)(rdpgfx_server_context *context, RDPGFX_CREATE_SURFACE_PDU *create_surface);
typedef BOOL (*pfn_rdpgfx_server_delete_surface)(rdpgfx_server_context *context, RDPGFX_DELETE_SURFACE_PDU *delete_surface);
typedef BOOL (*pfn_rdpgfx_server_start_frame)(rdpgfx_server_context *context, RDPGFX_START_FRAME_PDU *start_frame);
typedef BOOL (*pfn_rdpgfx_server_end_frame)(rdpgfx_server_context *context, RDPGFX_END_FRAME_PDU *end_frame);
typedef BOOL (*pfn_rdpgfx_server_reset_graphics)(rdpgfx_server_context *context, RDPGFX_RESET_GRAPHICS_PDU *reset_graphics);
typedef BOOL (*pfn_rdpgfx_server_map_surface_to_output)(rdpgfx_server_context *context, RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU *map_surface_to_output);
typedef BOOL (*pfn_rdpgfx_server_surface_to_cache)(rdpgfx_server_context *context, RDPGFX_SURFACE_TO_CACHE_PDU *surface_to_cache);
typedef BOOL (*pfn_rdpgfx_server_cache_to_surface)(rdpgfx_server_context *context, RDPGFX_CACHE_TO_SURFACE_PDU *cache_to_surface);

typedef void (*pfn_rdpgfx_server_open_result)(rdpgfx_server_context* context, rdpgfx_server_open_result result);
typedef void (*pfn_rdpgfx_server_frame_acknowledge)(rdpgfx_server_context* context, RDPGFX_FRAME_ACKNOWLEDGE_PDU* frame_acknowledge);
typedef void (*pfn_rdpgfx_server_qoe_frame_acknowledge)(rdpgfx_server_context* context, RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU* qoe_frame_acknowledge);

struct _rdpgfx_server_context
{
	HANDLE rdpgfx_channel;
	HANDLE vcm;
	wStream *s;
	UINT32 requiredBytes;
	BOOL capsReceived;

	/* Server self-defined pointer. */
	void* data;

	UINT32 version;
	UINT32 flags;

	BOOL h264Supported;
	BOOL avc444Supported;
	BOOL avc444v2Supported;
	BOOL avc444Restricted;

	/*** APIs called by the server. ***/
	/**
	 * Open the graphics channel. The server MUST wait until OpenResult callback is called
	 * before using the channel.
	 */
	pfn_rdpgfx_server_open Open;
	/**
	 * Close the graphics channel.
	 */
	pfn_rdpgfx_server_close Close;
	/**
	 * Transfer bitmap data to surface.
	 */
	pfn_rdpgfx_server_wire_to_surface1 WireToSurface1;
	/**
	 * Transfer bitmap data to surface.
	 */
	pfn_rdpgfx_server_wire_to_surface2 WireToSurface2;
	/**
	 * Fill solid color in surface.
	 */
	pfn_rdpgfx_server_solid_fill SolidFill;
	/**
	 * Create a surface.
	 */
	pfn_rdpgfx_server_create_surface CreateSurface;
	/**
	 * Delete a surface.
	 */
	pfn_rdpgfx_server_delete_surface DeleteSurface;
	/**
	 * Start a frame.
	 */
	pfn_rdpgfx_server_start_frame StartFrame;
	/**
	 * End a frame.
	 */
	pfn_rdpgfx_server_end_frame EndFrame;
	/**
	 * Change the width and height of the graphics output buffer, and update the monitor layout.
	 */
	pfn_rdpgfx_server_reset_graphics ResetGraphics;
	/**
	 * Map a surface to a rectangular area of the graphics output buffer.
	 */
	pfn_rdpgfx_server_map_surface_to_output MapSurfaceToOutput;
	/**
	 * Transfer surface data to cache slot.
	 */
	pfn_rdpgfx_server_surface_to_cache SurfaceToCache;
	/**
	 * Transfer cache data to surface.
	 */
	pfn_rdpgfx_server_cache_to_surface CacheToSurface;

	/*** Callbacks registered by the server. ***/
	/**
	 * Indicate whether the channel is opened successfully.
	 */
	pfn_rdpgfx_server_open_result OpenResult;
	/**
	 * A frame is acknowledged by the client.
	 */
	pfn_rdpgfx_server_frame_acknowledge FrameAcknowledge;
	/**
	 * Optional message to enable Quality of Exferience (QoE) metrics.
	 */
	pfn_rdpgfx_server_qoe_frame_acknowledge QoeFrameAcknowledge;
};

rdpgfx_server_context* rdpgfx_server_context_new(HANDLE vcm);
void rdpgfx_server_context_free(rdpgfx_server_context* context);

#endif /* _OGON_RDPSRV_RDPGFX_CHANNEL_H_ */
