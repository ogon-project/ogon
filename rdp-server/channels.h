/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Virtual Channel Support
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
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

#ifndef OGON_RDPSRV_CHANNELS_H_
#define OGON_RDPSRV_CHANNELS_H_

#include <freerdp/utils/ringbuffer.h>
#include <freerdp/channels/wtsvc.h>

#include "ogon.h"
#include "eventloop.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

enum {
	CREATE_REQUEST_PDU = 0x01,
	DATA_FIRST_PDU = 0x02,
	DATA_PDU = 0x03,
	CLOSE_REQUEST_PDU = 0x04,
	CAPABILITY_REQUEST_PDU = 0x05
};

enum {
	RDP_PEER_CHANNEL_TYPE_SVC = 0,
	RDP_PEER_CHANNEL_TYPE_DVC = 1
};

enum {
	DVC_OPEN_STATE_NONE = 0,
	DVC_OPEN_STATE_SUCCEEDED = 1,
	DVC_OPEN_STATE_FAILED = 2,
	DVC_OPEN_STATE_CLOSED = 3
};


typedef struct _ogon_virtual_channel_manager ogon_vcm;

typedef struct _registered_virtual_channel registered_virtual_channel;

typedef struct _internal_virtual_channel internal_virtual_channel;
typedef BOOL (*internal_channel_created_cb)(void *context);
typedef BOOL (*internal_channel_deleted_cb)(void *context, HRESULT status);
typedef BOOL (*internal_channel_receive_cb)(void *context, const BYTE *data, UINT32 length);

/** @brief a channel that is handled by ogon itself */
struct _internal_virtual_channel
{
	registered_virtual_channel *channel;
	void *context;
	internal_channel_created_cb created_callback;
	internal_channel_deleted_cb deleted_callback;
	internal_channel_receive_cb receive_callback;
};

/** @brief a VC in ogon */
struct _registered_virtual_channel
{
	ogon_vcm *vcm;
	freerdp_peer *client;

	UINT32 channel_id;
	UINT16 channel_type;
	UINT16 index;

	BYTE dvc_open_state;
	UINT32 dvc_total_length;
	UINT32 dvc_open_tag;
	UINT32 dvc_open_id;

	HANDLE pipe_server;
	HANDLE pipe_client;
	BOOL pipe_alive;
	char *pipe_name;
	char *vc_name;
	ogon_event_source *event_source_server;
	ogon_event_source *event_source_client;
	ogon_event_loop *event_loop;
	BOOL writeBlocked;

	DWORD channel_instance;

	wStream *receive_data;
	RingBuffer pipe_xmit_buffer;

	UINT32 pipe_expected_bytes;
	BOOL pipe_waiting_length;
	BYTE *pipe_target_buffer;
	UINT32 pipe_current_packet_length;
	wStream *pipe_input_buffer;

	internal_virtual_channel *internalChannel;

	BYTE header_buffer[4];
};

struct _ogon_virtual_channel_manager
{
	rdpRdp* rdp;
	freerdp_peer *client;

	BYTE drdynvc_state;
	UINT32 drdynvc_channel_id;
	UINT32 dvc_channel_id_seq;

	wArrayList *registered_virtual_channels;
	ogon_connection *session;
};

BOOL ogon_channels_post_connect(ogon_connection *session);
ogon_vcm *openVirtualChannelManager(ogon_connection *context);
void closeVirtualChannelManager(ogon_vcm *hServer);

BOOL virtual_manager_open_dynamic_virtual_channel(ogon_connection *conn, ogon_vcm *vcm, wMessage *msg);
BOOL virtual_manager_open_static_virtual_channel(ogon_connection *conn, ogon_vcm *vcm, wMessage *msg);
BOOL virtual_manager_close_dynamic_virtual_channel(ogon_vcm *vcm, wMessage *msg);
BOOL virtual_manager_close_dynamic_virtual_channel_common(registered_virtual_channel *channel);
void virtual_manager_close_all_channels(ogon_vcm *vcm);

BOOL virtual_manager_close_internal_virtual_channel(internal_virtual_channel *intVC);
internal_virtual_channel *virtual_manager_open_internal_virtual_channel(ogon_vcm *vcm, const char *name, BOOL isDynamic);
BOOL virtual_manager_write_internal_virtual_channel(internal_virtual_channel *intVC, BYTE *data, UINT32 length, UINT32 *pWritten);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OGON_RDPSRV_CHANNELS_H_ */
