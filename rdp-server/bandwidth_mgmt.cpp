/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Bandwidth Management
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
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

#include <winpr/sysinfo.h>

#include "../common/global.h"
#include "commondefs.h"

#include "bandwidth_mgmt.h"
#include "peer.h"

#define TAG OGON_TAG("core.bandwidthmgmt")

#define MIN_DATA_SIZE 7 * 1024
#define STD_USED_ENCODING_BITRATE 10 * 1024 * 1024

#define DEBUG_BUCKET 0
#define DEBUG_BANDWIDTH 0

void ogon_bwmgmt_init_buckets(ogon_connection *conn, UINT32 bitrate) {
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;
	UINT32 targetFrameSizeInBits;
	int run;

	if (bitrate == 0) {
		bitrate = STD_USED_ENCODING_BITRATE;
	}
	targetFrameSizeInBits = bitrate / conn->fps;

	for (run = 0; run < OGON_MAX_BUCKET; run++) {
		bwmgmt->bucket[run].size = targetFrameSizeInBits;
	}
}

UINT32 ogon_bwmgmt_update_bucket(ogon_connection *conn) {
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;
	UINT32 targetFrameSizeInBits;

	bwmgmt->current_bucket++;
	if (bwmgmt->current_bucket >= OGON_MAX_BUCKET) {
		bwmgmt->current_bucket = 0;
	}

	if (!bwmgmt->configured_bitrate && bwmgmt->autodetect_bitRateKBit > 20) {
		targetFrameSizeInBits =
				bwmgmt->autodetect_bitRateKBit * 1024 / conn->fps;
	} else if (bwmgmt->configured_bitrate) {
		/* use static value if fixed bitrate is enabled */
		targetFrameSizeInBits = bwmgmt->configured_bitrate / conn->fps;
		goto out;
	} else {
		targetFrameSizeInBits = (UINT32)STD_USED_ENCODING_BITRATE / conn->fps;
	}

	if (targetFrameSizeInBits < bwmgmt->future_data_size_used) {
		bwmgmt->future_data_size_used -= targetFrameSizeInBits;
		/* if the connection gets a lot of traffic in the virtual
		   channels then dont let the framerate drop under 1 */
		if (bwmgmt->suppressed_frames == (UINT32)conn->fps) {
			bwmgmt->suppressed_frames = 0;
		} else {
			targetFrameSizeInBits = 0;
#if DEBUG_BANDWIDTH
			WLog_DBG(TAG,
					"suppressing next frame, supressed frames so far  %" PRIu32
					"!",
					bwmgmt->suppressed_frames);
#endif
			bwmgmt->suppressed_frames++;
		}
	} else {
		targetFrameSizeInBits -= bwmgmt->future_data_size_used;
		bwmgmt->future_data_size_used = 0;
		bwmgmt->suppressed_frames = 0;
	}
out:
	bwmgmt->bucket[bwmgmt->current_bucket].size = targetFrameSizeInBits;
#if DEBUG_BUCKET
	WLog_DBG(TAG, "Updating bucket %" PRIu32 " with size %" PRIu32 "",
			bwmgmt->current_bucket, targetFrameSizeInBits);
#endif
	return targetFrameSizeInBits;
}

static UINT16 ogon_bwmgmt_calc_using_buckets(ogon_connection *conn) {
	UINT16 using_buckets = (UINT16)conn->front.frameAcknowledge;

	if (!using_buckets) {
		using_buckets =
				(UINT16)((conn->fps / 2 > OGON_MAX_BUCKET) ? OGON_MAX_BUCKET
														   : conn->fps / 2);
	}

	if (using_buckets > OGON_MAX_BUCKET) {
		using_buckets = OGON_MAX_BUCKET;
	}
	return using_buckets;
}

static UINT32 ogon_bwmgmt_update_bucket_usage_rec(ogon_connection *conn,
		UINT32 size_used, UINT16 depth, INT32 current_index) {
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;
	UINT32 currentBucketSize;

	if (current_index < 0) {
		current_index += OGON_MAX_BUCKET;
	} else if (current_index >= OGON_MAX_BUCKET) {
		current_index -= OGON_MAX_BUCKET;
	}
	currentBucketSize = bwmgmt->bucket[current_index].size;

	if (size_used > currentBucketSize) {
		size_used -= currentBucketSize;
#if DEBUG_BUCKET
		WLog_DBG(TAG,
				"%s: Updateing bucket %" PRIu32 " with size %" PRIu32
				" to 0 (size_used: %" PRIu32 ")",
				__FUNCTION__, current_index, bwmgmt->bucket[current_index].size,
				size_used);
#endif
		bwmgmt->bucket[current_index].size = 0;
		if (--depth == 0) {
			return size_used;
		}
		return ogon_bwmgmt_update_bucket_usage_rec(
				conn, size_used, depth, ++current_index);
	} else {
		bwmgmt->bucket[current_index].size -= size_used;
#if DEBUG_BUCKET
		WLog_DBG(TAG,
				"%s: Updateing bucket %" PRIu32 " to %" PRIu32
				" (size_used: %" PRIu32 ")",
				__FUNCTION__, current_index, bwmgmt->bucket[current_index].size,
				size_used);
#endif
		return 0;
	}
}

static UINT32 ogon_bwmgmt_update_bucket_usage(
		ogon_connection *conn, UINT32 size_used) {
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;
	UINT32 currentBucketIndex = bwmgmt->current_bucket;
	UINT16 usingBuckets = ogon_bwmgmt_calc_using_buckets(conn);
	UINT32 currentBucketSize = bwmgmt->bucket[currentBucketIndex].size;

	if (size_used > currentBucketSize) {
		size_used -= currentBucketSize;
#if DEBUG_BUCKET
		WLog_DBG(TAG,
				"%s: Updateing bucket %" PRIu32 " with size %" PRIu32
				" to 0 (size_used: %" PRIu32 ")",
				__FUNCTION__, currentBucketIndex,
				bwmgmt->bucket[currentBucketIndex].size,
				size_used + currentBucketSize);
#endif
		bwmgmt->bucket[currentBucketIndex].size = 0;
		return ogon_bwmgmt_update_bucket_usage_rec(conn, size_used,
				usingBuckets, currentBucketIndex - usingBuckets);
	} else {
#if DEBUG_BUCKET
		WLog_DBG(TAG,
				"%s: Updateing bucket  %" PRIu32 " with size %" PRIu32
				" to %" PRIu32 " (size_used: %" PRIu32 ")",
				__FUNCTION__, currentBucketIndex,
				bwmgmt->bucket[currentBucketIndex].size,
				bwmgmt->bucket[currentBucketIndex].size - size_used, size_used);
#endif
		bwmgmt->bucket[currentBucketIndex].size -= size_used;
		return 0;
	}
}

BOOL ogon_bwmgmt_update_data_usage(ogon_connection *conn) {
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;
	ogon_statistics *stats = &conn->front.statistics;

	UINT32 dataWritten = freerdp_get_transport_sent(&conn->context, TRUE);
	stats->bytes_sent_current += dataWritten;

	if (bwmgmt->configured_bitrate) {
		/* dont update buckets if we are using a fixed bit rate*/
		return TRUE;
	}

	dataWritten = ogon_bwmgmt_update_bucket_usage(conn, dataWritten * 8);
	if (dataWritten) {
		bwmgmt->future_data_size_used += dataWritten;
	}

	return TRUE;
}

UINT32 ogon_bwmgtm_calc_max_target_frame_size(ogon_connection *conn) {
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;

	UINT32 max_target_frame_size = 0;
	INT32 run;
	UINT16 using_buckets = ogon_bwmgmt_calc_using_buckets(conn);
	INT32 current_index;

	if (bwmgmt->configured_bitrate) {
		/* return static value for a fixed bitrate*/
		return bwmgmt->bucket[bwmgmt->current_bucket].size;
	}

	for (run = using_buckets - 1; run >= 0; run--) {
		current_index = bwmgmt->current_bucket - run;
		if (current_index < 0) {
			current_index += OGON_MAX_BUCKET;
		}
		max_target_frame_size += bwmgmt->bucket[current_index].size;
	}

	return max_target_frame_size;
}

BOOL ogon_bwmgmt_client_detect_rtt(ogon_connection *conn) {
	freerdp_peer *peer = conn->context.peer;
	ogon_front_connection *frontend = &conn->front;
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;
	UINT32 starttime;

	if (!peer->activated || !peer->context->settings->NetworkAutoDetect ||
			!frontend->frameAcknowledge) {
		return TRUE;
	}

	/* ensure there are no inflight frames */
	if (frontend->lastAckFrame + frontend->frameAcknowledge + 1 <
			frontend->nextFrameId) {
		return TRUE;
	}

	if (bwmgmt->autodetect_rtt_sent) {
		return TRUE;
	}

	/* Ensure the minimum time interval between test batches*/
	starttime = GetTickCount();
	if (starttime - bwmgmt->autodetect_rtt_starttime < 1000) {
		return TRUE;
	}
	bwmgmt->autodetect_rtt_starttime = starttime;
	bwmgmt->autodetect_rtt_sent = TRUE;
	peer->context->autodetect->RTTMeasureRequest(peer->context->autodetect,
			RDP_TRANSPORT_TCP, bwmgmt->autodetect_rtt_seq);

	return TRUE;
}

BOOL ogon_bwmgmt_client_rtt_measure_response(rdpAutoDetect *autodetect,
		RDP_TRANSPORT_TYPE transport, UINT16 sequenceNumber) {
	OGON_UNUSED(sequenceNumber);
	ogon_connection *connection = (ogon_connection *)autodetect->context;
	ogon_front_connection *frontend = &connection->front;
	freerdp_peer *peer = connection->context.peer;
	ogon_bandwidth_mgmt *bwmgmt = &connection->front.bandwidthMgmt;
	UINT32 frameack;

	if (frontend->frameAcknowledge) {
		/* Calculate frameack based on RTT*/
		frameack = MIN(MAX(peer->context->autodetect->netCharBaseRTT *
									   connection->fps / 1000,
							   2),
				(unsigned int)connection->fps);
		if (frameack != frontend->frameAcknowledge) {
			frontend->frameAcknowledge = frameack;
			WLog_VRB(TAG,
					"measured delay : %" PRIu32 " adjusted frameack to %" PRIu32
					"",
					peer->context->autodetect->netCharBaseRTT, frameack);
		}
	}

	/* Reset counter for next test batch*/
	bwmgmt->autodetect_rtt_sent = FALSE;
	bwmgmt->autodetect_rtt_seq++;
	peer->context->autodetect->netCharAverageRTT = 0;
	peer->context->autodetect->netCharBaseRTT = 0;

	return TRUE;
}

static UINT32 ogon_bwmgmt_client_bandwidth_meassure_average(
		ogon_bandwidth_mgmt *bwmgmt) {
	UINT32 totalBitrates = 0;
	UINT32 totaltimedelta = 0;
	UINT32 run;

	for (run = 0; run < OGON_MAX_STATISTIC; ++run) {
		totalBitrates += bwmgmt->statistics[run].bits;
		totaltimedelta += bwmgmt->statistics[run].timedelta;
	}

	return totaltimedelta ? totalBitrates / totaltimedelta : 0;
}

BOOL ogon_bwmgmt_client_bandwidth_measure_results(rdpAutoDetect *autodetect,
		RDP_TRANSPORT_TYPE transport, UINT16 responseType,
		UINT16 sequenceNumber) {
	OGON_UNUSED(sequenceNumber);
	ogon_connection *connection = (ogon_connection *)autodetect->context;
	ogon_bandwidth_mgmt *bwmgmt = &connection->front.bandwidthMgmt;
	UINT32 average_bit_rate;
	UINT32 byte_count = autodetect->bandwidthMeasureByteCount;
	UINT32 time_delta = autodetect->bandwidthMeasureTimeDelta;

	if (byte_count < MIN_DATA_SIZE) {
		return TRUE;
	}

	if (time_delta < 2) {
		/**
		 * Note:
		 * Since measurement results below 3ms are far from accurate
		 * we can only say for sure that we could have written > 500fps
		 * with that amount of data.
		 * Therefore we inject a fake result with twice the current
		 * average bandwidth in this case.
		 * This will constantly increase our bandwith until the accurate
		 * results kick in.
		 */
		average_bit_rate = bwmgmt->autodetect_bitRateKBit;
		if (average_bit_rate > 100 * 1024) {
			return TRUE;
		}
#if 0
		/* Option 1: faking just a single statistic value */
		time_delta = 1;
		byte_count = average_bit_rate / 8 * 2;
		if (byte_count == 0) {
			byte_count = MIN_DATA_SIZE ;
		}
#if DEBUG_BANDWIDTH
		WLog_DBG(TAG, "faking measurement result: %"PRIu32" ms %"PRIu32" bytes", time_delta, byte_count);
#endif
#else
		/* Option 2: setting all statistic values to the same average value */
		UINT32 run = 0;
		if (average_bit_rate == 0) {
			average_bit_rate = STD_USED_ENCODING_BITRATE / 1024;
		} else {
			average_bit_rate *= 2;
		}

		for (run = 0; run < OGON_MAX_STATISTIC; run++) {
			bwmgmt->statistics[run].bits = average_bit_rate;
			bwmgmt->statistics[run].timedelta = 1;
		}
		bwmgmt->autodetect_bitRateKBit = average_bit_rate;
#if DEBUG_BANDWIDTH
		WLog_DBG(TAG, "faking average bit rate: %.2f",
				average_bit_rate / 1024.0);
#endif
		return TRUE;
#endif
	}

	bwmgmt->current_statistic++;
	if (bwmgmt->current_statistic >= OGON_MAX_STATISTIC) {
		bwmgmt->current_statistic = 0;
	}

	bwmgmt->statistics[bwmgmt->current_statistic].timedelta = time_delta;
	bwmgmt->statistics[bwmgmt->current_statistic].bits = byte_count * 8;

	average_bit_rate = ogon_bwmgmt_client_bandwidth_meassure_average(bwmgmt);
#if DEBUG_BANDWIDTH
	WLog_DBG(TAG,
			"using bandwidth measure results: byte_count=%" PRIu32
			" time_delta=%" PRIu32 " (bucket mbps: %.2f avrate=%.2f mbps)",
			byte_count, time_delta,
			byte_count * 8.0 / time_delta * 1000 / 1024 / 1024,
			average_bit_rate / 1024.0);
#endif

	if (average_bit_rate) {
		bwmgmt->autodetect_bitRateKBit = average_bit_rate;
	} else {
		bwmgmt->autodetect_bitRateKBit = autodetect->netCharBandwidth;
	}

	return TRUE;
}

BOOL ogon_bwmgmt_detect_bandwidth_start(ogon_connection *conn) {
	freerdp_peer *peer = conn->context.peer;
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;

	if (!peer->activated || !peer->context->settings->NetworkAutoDetect) {
		return TRUE;
	}

	if (!peer->context->autodetect->BandwidthMeasureStart(
				peer->context->autodetect, RDP_TRANSPORT_TCP,
				bwmgmt->autodetect_bandwidth_seq)) {
		WLog_ERR(TAG, "BandwidthMeasureStart failed");
		return FALSE;
	}

	return TRUE;
}

BOOL ogon_bwmgmt_detect_bandwidth_stop(ogon_connection *conn) {
	freerdp_peer *peer = conn->context.peer;
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;

	if (!peer->activated || !peer->context->settings->NetworkAutoDetect) {
		return TRUE;
	}

	if (!peer->context->autodetect->BandwidthMeasureStop(
				peer->context->autodetect, RDP_TRANSPORT_TCP,
				bwmgmt->autodetect_bandwidth_seq)) {
		WLog_ERR(TAG, "BandwidthMeasureStop failed");
		return FALSE;
	}

	return TRUE;
}
