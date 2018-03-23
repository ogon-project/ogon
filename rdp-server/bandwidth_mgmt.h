/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Bandwidth Management
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef _OGON_RDPSRV_BANDWIDTH_MGMT_H_
#define _OGON_RDPSRV_BANDWIDTH_MGMT_H_

#include <freerdp/freerdp.h>
#include "commondefs.h"


void ogon_bwmgmt_init_buckets(ogon_connection *conn, UINT32 bitrate);
UINT32 ogon_bwmgmt_update_bucket(ogon_connection *conn);
BOOL ogon_bwmgmt_update_data_usage(ogon_connection *conn);
UINT32 ogon_bwmgtm_calc_max_target_frame_size(ogon_connection *conn);

BOOL ogon_bwmgmt_client_detect_rtt(ogon_connection *conn);
BOOL ogon_bwmgmt_client_rtt_measure_response(rdpContext *context, UINT16 sequenceNumber);

BOOL ogon_bwmgmt_detect_bandwidth_start(ogon_connection *conn);
BOOL ogon_bwmgmt_detect_bandwidth_stop(ogon_connection *conn);
BOOL ogon_bwmgmt_client_bandwidth_measure_results(rdpContext *context, UINT16 sequenceNumber);

#endif /* _OGON_RDPSRV_BANDWIDTH_MGMT_H_ */
