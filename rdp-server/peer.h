/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Peer
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

#ifndef OGON_RDPSRV_PEER_H_
#define OGON_RDPSRV_PEER_H_

#include <winpr/collections.h>
#include <winpr/synch.h>

#include <freerdp/client.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/codec/region.h>
#include <freerdp/utils/ringbuffer.h>

#include "commondefs.h"

#include <ogon/backend.h>


#include "encoder.h"
#include "eventloop.h"
#include "channels.h"
#include "state.h"
#include "rdpgfx.h"
#include "../backend/protocol.h"

#define OGON_MAX_STATISTIC 30

#define OGON_MAX_BUCKET 15

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/** @brief holds data related to a new pointer cache entry */
typedef struct _ogon_pointer_cache_entry {
	UINT32 hash;
	UINT32 hits;
} ogon_pointer_cache_entry;


typedef struct _ogon_bucket_data {
	UINT32 size;
} ogon_bucket_data;

typedef struct _ogon_bandwidth_statistic {
	UINT32 timedelta;
	UINT32 bits;
} ogon_bandwidth_statistic;

typedef struct _ogon_bandwidth_mgmt {
	UINT32 configured_bitrate; /* 0 means auto otherwise the encoder bitrate*/

	UINT32 autodetect_bitRateKBit; /* the autodetected bitrate in KBit */
	UINT16 autodetect_bandwidth_seq; /* autodetect sequence */

	/* values used for round trip time detection */
	BOOL autodetect_rtt_sent;
	UINT32 autodetect_rtt_starttime;
	UINT16 autodetect_rtt_seq;

	/* statistics used for getting a better (cleaner) autodetect_bitRateKBit */
	ogon_bandwidth_statistic statistics[OGON_MAX_STATISTIC];
	UINT16 current_statistic;

	/* Buckets used to get a better usage of the available bandwidth */
	ogon_bucket_data bucket[OGON_MAX_BUCKET];
	UINT32 current_bucket;
	UINT32 future_data_size_used;
	UINT32 suppressed_frames; /* frames suppressed because of to little size in bucket */

}ogon_bandwidth_mgmt;

typedef struct _ogon_statistics {
	/* rendered fps in the last sec */
	UINT32 fps_measure_timestamp;
	UINT16 fps_measured;
	UINT16 fps_measure_currentfps;

	/* bytes sent in the last sec */
	UINT32 bytes_sent_timestamp;
	UINT32 bytes_sent;
	UINT32 bytes_sent_current;
}  ogon_statistics;

/** @brief holds data related to the front RDP connection */
struct _ogon_front_connection {
	ogon_event_source *rdpEventSource;
	ogon_event_source *channelEventSource;

	ogon_vcm *vcm;

	UINT32 lastAckFrame;
	UINT32 nextFrameId;
	UINT32 frameAcknowledge;
	ogon_pointer_cache_entry *pointerCache;
	ogon_state_machine *state;
	BOOL writeReady;

	ogon_keyboard_modifiers_state modifiers;
	ogon_keyboard_indicator_state indicators;
	int inputFilter;
	ogon_codec_mode codecMode;
	ogon_bitmap_encoder *encoder;

	rdpgfx_server_context* rdpgfx;
	UINT32 rdpgfxOutputSurface;
	BOOL rdpgfxRequired;
	BOOL rdpgfxConnected;
	BOOL rdpgfxForbidden;
	BOOL rdpgfxH264Forbidden;
	BOOL rdpgfxH264Supported;
	BOOL rdpgfxH264EnableFullAVC444;

	UINT32 rdpgfxProgressiveTicks;

	BOOL showDebugInfo;

	ogon_event_source *frameEventSource;

	UINT32 pendingResizeWidth;
	UINT32 pendingResizeHeight;

	ogon_backend_props backendProps;
	UINT32 activationCount;
	UINT32 maxWidth;
	UINT32 maxHeight;
	UINT32 initialDesktopWidth;
	UINT32 initialDesktopHeight;

	ogon_bandwidth_mgmt bandwidthMgmt;

	ogon_statistics statistics;

};


/** @brief */
typedef struct _ogon_connection_runloop {
	ogon_event_loop *evloop;
	HANDLE workThread;
	freerdp_peer *peer;
} ogon_connection_runloop;

/** @brief the RDS context associated with a single peer */
struct _ogon_connection{
	rdpContext context;

	long id;
	ogon_connection_runloop *runloop;
	wMessageQueue *commandQueue;
	volatile BOOL runThread;
	HANDLE stopEvent;

	ogon_front_connection front;
	ogon_connection *shadowing;
	wLinkedList *frontConnections;
	ogon_backend_connection *backend;

	int fps;
	BOOL sendDisconnect;
	ogon_keyboard_modifiers_state shadowingEscapeModifiers;
	UINT16 shadowingEscapeKey;
	BOOL externalStop;
};


/** @brief */
typedef struct _rds_notification_sbp {
	ogon_msg_sbp_reply reply;
} rds_notification_sbp;

void ogon_backend_props_free(ogon_backend_props *props);
BOOL ogon_rdpgfx_init_output(ogon_connection *conn);
BOOL ogon_rdpgfx_shutdown(ogon_connection *conn);
void ogon_connection_close(ogon_connection *conn);
BOOL ogon_post_exit_shadow_notification(ogon_connection *conn, wMessage *msg, BOOL rewire);
BOOL initiate_immediate_request(ogon_connection *conn, ogon_front_connection *front, BOOL setDamage);
ogon_connection_runloop *ogon_runloop_new(freerdp_peer *peer);
ogon_connection *ogon_connection_create(ogon_connection_runloop *runloop);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OGON_RDPSRV_PEER_H_ */
