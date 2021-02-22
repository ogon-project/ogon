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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/pipe.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/freerdp.h>

#include "../common/global.h"
#include "icp/icp_client_stubs.h"
#include "icp/pbrpc/pbrpc.h"

#include "peer.h"
#include "channels.h"


#define TAG OGON_TAG("core.channels")

#define VC_BYTES_LIMIT_PER_LOOP_TURN 0x10000

static void ringbuffer_reset(RingBuffer *rb) {
	ringbuffer_commit_read_bytes(rb, ringbuffer_used(rb));
}

static BOOL dvc_send_close(registered_virtual_channel *dvc);

registered_virtual_channel *VirtualChannelManagerGetChannelByNameAndType(ogon_vcm *vcm,
	const char *name, UINT16 type)
{
	registered_virtual_channel *currentChannel = NULL;
	wArrayList *registeredChannels;
	UINT32 index;
	int count;

	if ((vcm == NULL) || (vcm->registered_virtual_channels == NULL)) {
		return NULL;
	}
	registeredChannels = vcm->registered_virtual_channels;

	count = ArrayList_Count(registeredChannels);
	for (index = 0; index < (unsigned int)count; index++) {
		currentChannel = (registered_virtual_channel *)ArrayList_GetItem(registeredChannels, index);
		if (strncasecmp(currentChannel->vc_name, name, strlen(currentChannel->vc_name)) == 0 &&
			currentChannel->channel_type == type)
		{
			return currentChannel;
		}

	}
	return NULL;
}

registered_virtual_channel *VirtualChannelManagerGetChannelByIdAndType(ogon_vcm *vcm,
	UINT32 id, UINT16 type)
{
	registered_virtual_channel *currentChannel = NULL;
	wArrayList *registeredChannels;
	UINT32 index;
	int count;

	if ((vcm == NULL) || (vcm->registered_virtual_channels == NULL)) {
		return NULL;
	}
	registeredChannels = vcm->registered_virtual_channels;


	count = ArrayList_Count(registeredChannels);
	for (index = 0; index < (unsigned int)count; index++) {
		currentChannel = (registered_virtual_channel *)ArrayList_GetItem(registeredChannels, index);
		if ((currentChannel->channel_id == id) && (currentChannel->channel_type == type)) {
			return currentChannel;
		}
	}

	return NULL;
}

static registered_virtual_channel *vc_new(const char *vcname, const char *pipeName, HANDLE serverHandle,
	ogon_vcm *vcm, ogon_connection *conn)
{
	rdpSettings *settings;
	registered_virtual_channel *ret = (registered_virtual_channel *)calloc(1, sizeof(registered_virtual_channel));
	if (!ret) {
		WLog_ERR(TAG, "error allocating registered virtual channel");
		return NULL;
	}

	ret->channel_instance = 0;
	if (vcname) {
		ret->vc_name = _strdup(vcname);
		if (!ret->vc_name) {
			WLog_ERR(TAG, "error allocating registered virtual channel name");
			goto out_free;
		}
	}

	if (pipeName) {
		ret->pipe_name = _strdup(pipeName);
		if (!ret->pipe_name) {
			WLog_ERR(TAG, "error allocating registered virtual pipe name");
			goto out_free_name;
		}
	}
	ret->pipe_server = serverHandle;
	if (conn) {
		ret->event_loop = conn->runloop->evloop;
	}
	ret->client = vcm->client;
	ret->vcm = vcm;
	ret->channel_id = WTSChannelGetId(vcm->client, vcname);
	ret->pipe_client = INVALID_HANDLE_VALUE;

	settings = vcm->client->settings;
	ret->pipe_expected_bytes = 4;
	ret->pipe_waiting_length = TRUE;
	ret->pipe_target_buffer = ret->header_buffer;
	ret->pipe_input_buffer = Stream_New(NULL, settings->VirtualChannelChunkSize);
	if (!ret->pipe_input_buffer) {
		goto out_free_pipe_name;
	}

	ret->receive_data = Stream_New(NULL, settings->VirtualChannelChunkSize);
	if (!ret->receive_data) {
		goto out_free_input_buffer;
	}

	if (!ringbuffer_init(&ret->pipe_xmit_buffer, settings->VirtualChannelChunkSize)) {
		goto out_free_receive_data;
	}

	return ret;

out_free_receive_data:
	Stream_Free(ret->receive_data, TRUE);
out_free_input_buffer:
	Stream_Free(ret->pipe_input_buffer, TRUE);
out_free_pipe_name:
	free(ret->pipe_name);
out_free_name:
	free(ret->vc_name);
out_free:
	free(ret);
	WLog_ERR(TAG, "vc_new() failed");
	return NULL;
}

void vc_free(registered_virtual_channel *channel) {
	ringbuffer_destroy(&channel->pipe_xmit_buffer);

	if (channel->receive_data) {
		Stream_Free(channel->receive_data, TRUE);
	}
	if (channel->pipe_input_buffer) {
		Stream_Free(channel->pipe_input_buffer, TRUE);
	}
	if (channel->pipe_name) {
		ogon_named_pipe_clean(channel->pipe_name);
		free(channel->pipe_name);
	}
	free(channel->vc_name);
	free(channel->internalChannel);
	free(channel);
}

static BOOL vc_flush_xmit_buffer(registered_virtual_channel *channel, int sendLimit) {
	DWORD written;
	DataChunk chunks[2];
	int i, nbChunks;
	int commitedBytes = 0;

	if (channel->internalChannel) {
		if (!channel->internalChannel->receive_callback) {
			return FALSE;
		}
		while ((nbChunks = ringbuffer_peek(&channel->pipe_xmit_buffer, chunks, 16384))) {
			commitedBytes = 0;
			for (i = 0; i < nbChunks; i++) {
				if (!channel->internalChannel->receive_callback(channel->internalChannel->context, chunks[i].data, chunks[i].size)) {
					return FALSE;
				}
				commitedBytes += chunks[i].size;
			}
			ringbuffer_commit_read_bytes(&channel->pipe_xmit_buffer, commitedBytes);
		}
		return TRUE;
	}

	if (channel->writeBlocked) {
		return TRUE;
	}

	while ((nbChunks = ringbuffer_peek(&channel->pipe_xmit_buffer, chunks, 16384))) {
		commitedBytes = 0;

		/*WLog_DBG(TAG, "used=%"PRIuz" chunk0=%"PRIuz" chunk1=%"PRIuz"", ringbuffer_used(&channel->pipe_xmit_buffer),
				chunks[0].size, chunks[1].size);*/

		for (i = 0; i < nbChunks; i++) {
			while (chunks[i].size) {
				if (!WriteFile(channel->pipe_client, chunks[i].data, chunks[i].size, &written, NULL)) {
					WLog_ERR(TAG, "error writing to channel client pipe");
					return FALSE;
				}
				/* WLog_DBG(TAG, "wrote %"PRIu32"", written); */

				if (!written) {
					/*WLog_DBG(TAG, "activating write availability");*/
					channel->writeBlocked = TRUE;
					if (!eventsource_change_source(channel->event_source_client, OGON_EVENTLOOP_READ | OGON_EVENTLOOP_WRITE)) {
						WLog_ERR(TAG, "error activating write availability scan");
					}

					ringbuffer_commit_read_bytes(&channel->pipe_xmit_buffer, commitedBytes);
					return TRUE;
				}

				commitedBytes += written;
				chunks[i].data += written;
				chunks[i].size -= written;

				if (commitedBytes > sendLimit) {
					/* fake a writeBlocked */
					channel->writeBlocked = TRUE;
					return eventsource_reschedule_for_write(channel->event_source_client);
				}
			}
		}

		ringbuffer_commit_read_bytes(&channel->pipe_xmit_buffer, commitedBytes);
	}

	return TRUE;
}

static void ogon_channels_drdynvc_failed(ogon_vcm *vcm) {
	ogon_connection *conn = vcm->session;
	ogon_front_connection *front = &conn->front;

	/* let internal dynamic virtual channels know that there is no transport */

	if (front->rdpgfxRequired) {
		front->rdpgfx->OpenResult(front->rdpgfx, RDPGFX_SERVER_OPEN_RESULT_NOTSUPPORTED);
	}
}

static void ogon_channels_drdynvc_ready(ogon_vcm *vcm) {
	ogon_connection *conn = vcm->session;
	ogon_front_connection *front = &conn->front;

	/* open required internal dynamic virtual channels */

	if (front->rdpgfxRequired) {
		WLog_DBG(TAG, "opening internal dynamic graphics channel");
		front->rdpgfx->Open(front->rdpgfx);
	}
}

static void ogon_channels_drdynvc_state_change(ogon_vcm *vcm) {
	switch (vcm->drdynvc_state)
	{
	case DRDYNVC_STATE_INITIALIZED:
		WLog_DBG(TAG, "new drdynvc state: INITIALIZED");
		return;

	case DRDYNVC_STATE_READY:
		WLog_DBG(TAG, "new drdynvc state: READY");
		ogon_channels_drdynvc_ready(vcm);
		return;

	case DRDYNVC_STATE_NONE:
		WLog_DBG(TAG, "new drdynvc state: NONE");
		break;

	case DRDYNVC_STATE_FAILED:
		WLog_DBG(TAG, "new drdynvc state: FAILED");
		break;

	default:
		WLog_ERR(TAG, "Invalid or unknown drdynvc state: %"PRIu8"", vcm->drdynvc_state);
		return;
	}

	ogon_channels_drdynvc_failed(vcm);
}

BOOL ogon_channels_post_connect(ogon_connection *connection) {
	UINT32 dynvc_caps;
	wArrayList *registeredChannels;
	registered_virtual_channel *dynChannel = NULL;
	ogon_vcm *vcm = connection->front.vcm;
	BOOL result = FALSE;

	if (vcm->drdynvc_state != DRDYNVC_STATE_NONE) {
		WLog_ERR(TAG, "unexpected drdynvc state in channels post connect: %"PRIu8"", vcm->drdynvc_state);
		goto out;
	}

	if (!WTSIsChannelJoinedByName(vcm->client, "drdynvc")) {
		WLog_DBG(TAG, "drdynvc channel is not joined");
		result = TRUE;
		goto out;
	}

	vcm->drdynvc_state = DRDYNVC_STATE_FAILED;

	registeredChannels = vcm->registered_virtual_channels;

	dynChannel = vc_new("drdynvc", NULL, NULL, vcm, NULL);
	if (!dynChannel) {
		WLog_ERR(TAG, "unable to allocate dynamic channel");
		goto out;
	}

	dynChannel->channel_type = RDP_PEER_CHANNEL_TYPE_SVC;

	if (!WTSChannelSetHandleByName(vcm->client, "drdynvc", dynChannel)) {
		vc_free(dynChannel);
		WLog_ERR(TAG, "WTSChannelSetHandleByName failed in post connect");
		goto out;
	}

	if (ArrayList_Add(registeredChannels, dynChannel) < 0) {
		WLog_ERR(TAG, "unable to add dynamic channel in the list of channels");
		goto out;
	}

	vcm->drdynvc_channel_id = dynChannel->channel_id;
	dynvc_caps = 0x00010050; /* DYNVC_CAPS_VERSION1 (4 bytes) */
	if (!dynChannel->client->SendChannelData(dynChannel->client, dynChannel->channel_id, (BYTE *)&dynvc_caps, sizeof(dynvc_caps))) {
		WLog_ERR(TAG, "SendChannelData failed failed in post connect");
		goto out;
	}

	result = TRUE;
	vcm->drdynvc_state = DRDYNVC_STATE_INITIALIZED;

out:
	ogon_channels_drdynvc_state_change(vcm);
	return result;
}

BOOL vc_disconnect_server_part(registered_virtual_channel *channel)
{
	if (channel->event_source_server) {
		eventloop_remove_source(&channel->event_source_server);
		if (channel->pipe_server != INVALID_HANDLE_VALUE) {
			CloseHandle(channel->pipe_server);
			channel->pipe_server = INVALID_HANDLE_VALUE;
		}
	}
	return TRUE;
}

static void vc_disconnect_client_part(registered_virtual_channel *channel) {
	/*WLog_DBG(TAG, "disconnecting channel %s", channel->vc_name);*/
	if (channel->event_source_client) {
		eventloop_remove_source(&channel->event_source_client);
	}

	if (channel->pipe_client != INVALID_HANDLE_VALUE) {
		DisconnectNamedPipe(channel->pipe_client);
		CloseHandle(channel->pipe_client);
		channel->pipe_client = INVALID_HANDLE_VALUE;
	}

	ringbuffer_reset(&channel->pipe_xmit_buffer);
	return;
}

static int wts_read_variable_uint(wStream *s, int cbLen, UINT32* val) {
	if ((cbLen < 0) || (cbLen > 3)) {
		return 0;
	}

	switch (cbLen)
	{
		case 0:
			if (Stream_GetRemainingLength(s) < 1)
				return 0;
			Stream_Read_UINT8(s, *val);
			return 1;

		case 1:
			if (Stream_GetRemainingLength(s) < 2)
				return 0;
			Stream_Read_UINT16(s, *val);
			return 2;

		default:
			if (Stream_GetRemainingLength(s) < 4)
				return 0;
			Stream_Read_UINT32(s, *val);
			return 4;
	}
}

static BOOL wts_read_drdynvc_capabilities_response(registered_virtual_channel *channel, UINT32 length)
{
	UINT16 Version;

	if (length < 3) {
		return FALSE;
	}

	Stream_Seek_UINT8(channel->receive_data); /* Pad (1 byte) */
	Stream_Read_UINT16(channel->receive_data, Version);

	WLog_DBG(TAG, "received drdynvc capabilities response version %"PRIu16"", Version);

	channel->vcm->drdynvc_state = DRDYNVC_STATE_READY;
	return TRUE;
}

static BOOL wts_read_drdynvc_create_response(registered_virtual_channel *channel, wStream *s, UINT32 length)
{
	wArrayList *registeredChannels;
	HRESULT CreationStatus;
	int error;

	if (length < 4) {
		return FALSE;
	}

	/**
	 * MS-RDPEDYC 2.2.2.2:
	 * CreationStatus (4 bytes): A 32-bit, signed integer that specifies the HRESULT code that indicates
	 * success or failure of the client DVC creation.
	 * A zero or positive value indicates success; a negative value indicates failure.
	 */

	Stream_Read_INT32(s, CreationStatus);

	if (FAILED(CreationStatus))
	{
		WLog_ERR(TAG, "creation failed for channel %s(id=%"PRIu32"): 0x%08"PRIX32"",
						channel->vc_name, channel->channel_id, (UINT32)CreationStatus);
		channel->dvc_open_state = DVC_OPEN_STATE_FAILED;
		if (!channel->internalChannel) {
			error = ogon_icp_sendResponse(channel->dvc_open_tag, channel->dvc_open_id, 0, TRUE, NULL);
			if (error != 0) {
				WLog_ERR(TAG, "ogon_icp_sendResponse failed");
				return FALSE;
			}
		} else {
			IFCALL(channel->internalChannel->deleted_callback, channel->internalChannel->context, CreationStatus);
		}

		registeredChannels = channel->vcm->registered_virtual_channels;
		ArrayList_Remove(registeredChannels, channel);

		vc_disconnect_server_part(channel);
		vc_free(channel);
		return TRUE;
	}


	WLog_DBG(TAG, "created dynamic channel id %"PRIu32" (%s)", channel->channel_id, channel->vc_name);
	channel->dvc_open_state = DVC_OPEN_STATE_SUCCEEDED;
	if (!channel->internalChannel) {
		error = ogon_icp_sendResponse(channel->dvc_open_tag, channel->dvc_open_id, 0, TRUE,channel);
		if (error != 0) {
			WLog_ERR(TAG, "ogon_icp_sendResponse failed");
			return FALSE;
		}
	} else {
		internal_virtual_channel *intVchannel = channel->internalChannel;
		if (intVchannel->created_callback && !intVchannel->created_callback(intVchannel->context)) {
			WLog_ERR(TAG, "error when calling the created_callback of the internal channel");
			return FALSE;
		}
	}

	return TRUE;
}

static BOOL wts_read_drdynvc_data_first(registered_virtual_channel *channel, wStream *s, int cbLen, UINT32 length)
{
	int value;

	if (channel->dvc_total_length) {
		/* If we haven't seen all the bytes of the previous packet there's good chances
		 * that the traffic is corrupted
		 */
		WLog_ERR(TAG, "attempt to send incomplete fragmented packets");
		return FALSE;
	}

	value = wts_read_variable_uint(s, cbLen, &channel->dvc_total_length);
	if (value == 0) {
		return FALSE;
	}

	length -= value;

	if (length >= channel->dvc_total_length) {
		/* Note: here we force the fact that a data packet mustn't be sent with the
		 * 		FIRST_PACKET flag when fragmentation wouldn't be needed (>= instead of >).
		 */
		WLog_ERR(TAG, "invalid packet stream length(%"PRIu32") >= announced length (%"PRIu32")",
				length, channel->dvc_total_length);
		return FALSE;
	}

	channel->dvc_total_length -= length;

	return ringbuffer_write(&channel->pipe_xmit_buffer, Stream_Pointer(s), length) &&
			vc_flush_xmit_buffer(channel, VC_BYTES_LIMIT_PER_LOOP_TURN);
}

static BOOL wts_read_drdynvc_data(registered_virtual_channel *channel, wStream *s, UINT32 length)
{
	if (channel->dvc_total_length > 0)
	{
		if (length > channel->dvc_total_length)
		{
			WLog_ERR(TAG, "incorrect fragment data, length=%"PRIu32", while it only remains %"PRIu32" of fragmented data",
					length, channel->dvc_total_length);
			channel->dvc_total_length = 0;
			return FALSE;
		}

		channel->dvc_total_length -= length;
	}

	return ringbuffer_write(&channel->pipe_xmit_buffer, Stream_Pointer(s), length) &&
		vc_flush_xmit_buffer(channel, VC_BYTES_LIMIT_PER_LOOP_TURN);
}

static BOOL wts_read_drdynvc_close_response(registered_virtual_channel *channel)
{
	BOOL ret = TRUE;
	WLog_DBG(TAG, "channel id %"PRIu32" close response", channel->channel_id);
	channel->dvc_open_state = DVC_OPEN_STATE_CLOSED;

	if (channel->internalChannel) {
		internal_virtual_channel *internalChannel = channel->internalChannel;
		if (internalChannel->deleted_callback && !internalChannel->deleted_callback(channel->internalChannel->context, STATUS_FILE_CLOSED)) {
			WLog_ERR(TAG, "error when calling the deleted_callback of the internal channel");
			ret = FALSE;
		}

		virtual_manager_close_internal_virtual_channel(channel->internalChannel);
	} else {
		virtual_manager_close_dynamic_virtual_channel_common(channel);
		vc_free(channel);
	}

	return ret;
}

static BOOL wts_read_drdynvc_pdu(registered_virtual_channel *channel, wStream *s)
{
	UINT32 length;
	int value;
	int cmd;
	int Sp;
	int cbChId;
	UINT32 channelId;
	registered_virtual_channel *targetChannel;

	length = Stream_GetPosition(s);

	if (length < 2) {
		return FALSE;
	}

	Stream_SetPosition(s, 0);
	Stream_Read_UINT8(s, value);

	length--;
	cmd = (value & 0xf0) >> 4;
	Sp = (value & 0x0c) >> 2;
	cbChId = (value & 0x03) /*>> 0*/;

	if (cmd == CAPABILITY_REQUEST_PDU) {
		int laststate = channel->vcm->drdynvc_state;
		if (!wts_read_drdynvc_capabilities_response(channel, length)) {
			return FALSE;
		}

		if (channel->vcm->drdynvc_state != laststate) {
			ogon_channels_drdynvc_state_change(channel->vcm);
		}
		return TRUE;
	}

	if (channel->vcm->drdynvc_state != DRDYNVC_STATE_READY)	{
		WLog_ERR(TAG, "received cmd %d but channel is not ready", cmd);
		return FALSE;
	}

	value = wts_read_variable_uint(s, cbChId, &channelId);
	if (value == 0) {
		return FALSE;
	}

	length -= value;

	targetChannel = VirtualChannelManagerGetChannelByIdAndType(channel->vcm, channelId, RDP_PEER_CHANNEL_TYPE_DVC);
	if (!targetChannel) {
		if (cmd != CLOSE_REQUEST_PDU) {
			WLog_ERR(TAG, "channel with id=%"PRIu32" does not exist, not executing request with type %d", channelId, cmd);
			return FALSE;
		}

		return TRUE;
	}

	switch (cmd) {
		case CREATE_REQUEST_PDU:
			return wts_read_drdynvc_create_response(targetChannel, s, length);
		case DATA_FIRST_PDU:
			return wts_read_drdynvc_data_first(targetChannel, s, Sp, length);
		case DATA_PDU:
			return wts_read_drdynvc_data(targetChannel, s, length);
		case CLOSE_REQUEST_PDU:
			return wts_read_drdynvc_close_response(targetChannel);
		default:
			WLog_ERR(TAG, "drdynvc pdu cmd %d not recognized", cmd);
			return FALSE;
	}

	return TRUE;
}

static BOOL ogon_processFrontendChannelData(registered_virtual_channel *channel,
		const BYTE *data, size_t size, UINT32 flags, size_t totalSize) {
	UINT32 buffer[2];
	wStream *buffer_stream;
	BOOL ret = TRUE;
	BOOL flush = FALSE;

	/**
	 * The RDPDR channel does not set CHANNEL_FLAG_SHOW_PROTOCOL and the
	 * internal protocol has no information whatsoever regarding the total PDU
	 * length but includes lots of *optional* paddings in the spec.
	 * In addition there are lots of clients that violate the protocol by
	 * adding some arbitrary paddings (e.g. Microsoft's Mac client)
	 * A RDPDR server that is only using only the WTS API can therefore not be
	 * implemented without access to the channel PDU headers.
	 * Therefore we always enforce the CHANNEL_FLAG_SHOW_PROTOCOL for the
	 * RDPDR channel.
	 */
	if (!_strnicmp(channel->vc_name, "RDPDR", 5)) {
		flags |= CHANNEL_FLAG_SHOW_PROTOCOL;
	}

	if ((flags & CHANNEL_FLAG_SHOW_PROTOCOL) && (channel->channel_id != channel->vcm->drdynvc_channel_id)) {
		/**
		 * if CHANNEL_FLAG_SHOW_PROTOCOL is specified write each PDU with headers directly to the pipe
		 */
		if (!(buffer_stream = Stream_New((BYTE *)buffer, sizeof(buffer)))) {
			return FALSE;
		}
		Stream_Write_UINT32(buffer_stream, (UINT32) totalSize);
		Stream_Write_UINT32(buffer_stream, (UINT32) flags);

		if (!ringbuffer_write(&channel->pipe_xmit_buffer, Stream_Buffer(buffer_stream), 8)) {
			WLog_ERR(TAG, "unable to write packet informations in xmit buffer");
			Stream_Free(buffer_stream, FALSE);
			return FALSE;
		}

		Stream_Free(buffer_stream, FALSE);

		if (!ringbuffer_write(&channel->pipe_xmit_buffer, data, size)) {
			WLog_ERR(TAG, "unable to write packet payload in xmit buffer");
			return FALSE;
		}

		flush = TRUE;
		goto out_flush;
	}

	if (flags & CHANNEL_FLAG_FIRST) {
		Stream_SetPosition(channel->receive_data, 0);
	}

	if (!Stream_EnsureRemainingCapacity(channel->receive_data, size)) {
		WLog_ERR(TAG, "Stream re-allocation failed");
		return FALSE;
	}
	Stream_Write(channel->receive_data, data, size);

	if (flags & CHANNEL_FLAG_LAST) {
		if (Stream_GetPosition(channel->receive_data) != totalSize) {
			WLog_ERR(TAG, "packet badly fragmented");
			return FALSE;
		}

		if (channel->channel_id == channel->vcm->drdynvc_channel_id) {
			ret = wts_read_drdynvc_pdu(channel, channel->receive_data);
		} else {
			flush = TRUE;
			if (!ringbuffer_write(&channel->pipe_xmit_buffer,
						Stream_Buffer(channel->receive_data), totalSize)) {
				WLog_ERR(TAG, "unable to write packet payload in xmit buffer");
				return FALSE;
			}
		}
		Stream_SetPosition(channel->receive_data, 0);
	}

out_flush:
	if (ret && flush)
		return vc_flush_xmit_buffer(channel, VC_BYTES_LIMIT_PER_LOOP_TURN);

	return ret;
}

static int ogon_receiveFrontendChannelData(freerdp_peer *client,
		UINT16 channelId, const BYTE *data, size_t size, UINT32 flags,
		size_t totalSize) {
	registered_virtual_channel *channel;

	channel = (registered_virtual_channel *)WTSChannelGetHandleById(client, channelId);
	if (!channel) {
		WLog_WARN(TAG, "failed to get handle for channel with id (%"PRIu16")", channelId);
		return TRUE;
	}
	return ogon_processFrontendChannelData(channel, data, size, flags, totalSize);
}

ogon_vcm *openVirtualChannelManager(ogon_connection *conn)
{
	freerdp_peer *client;
	ogon_vcm *vcm;

	if (!conn->context.peer) {
		return NULL;
	}

	client = conn->context.peer;

	vcm = (ogon_vcm *)calloc(1, sizeof(ogon_vcm));
	if (!vcm) {
		return NULL;
	}

	vcm->client = client;
	vcm->rdp = conn->context.rdp;
	vcm->drdynvc_channel_id = 0;
	vcm->drdynvc_state = DRDYNVC_STATE_NONE;
	vcm->dvc_channel_id_seq = 1;
	vcm->session = conn;

	vcm->registered_virtual_channels = ArrayList_New(TRUE);
	if (!vcm->registered_virtual_channels) {
		free(vcm);
		return NULL;
	}

	client->ReceiveChannelData = ogon_receiveFrontendChannelData;

	return vcm;
}

void virtual_manager_close_all_channels(ogon_vcm *vcm)
{
	int count, index;
	registered_virtual_channel *currentChannel;
	wArrayList *registeredChannels = vcm->registered_virtual_channels;

	count = ArrayList_Count(registeredChannels);

	for (index = 0; index < count; index++)
	{
		currentChannel = (registered_virtual_channel *) ArrayList_GetItem(registeredChannels, index);
		vc_disconnect_server_part(currentChannel);
		vc_disconnect_client_part(currentChannel);
		if (currentChannel->internalChannel)
			IFCALL(currentChannel->internalChannel->deleted_callback,
				currentChannel->internalChannel->context, STATUS_REMOTE_DISCONNECT);
		vc_free(currentChannel);
	}

	ArrayList_Clear(registeredChannels);
}

void closeVirtualChannelManager(ogon_vcm *vcm)
{
	wArrayList *registeredChannels;

	if (!vcm) {
		return;
	}

	registeredChannels = vcm->registered_virtual_channels;

	virtual_manager_close_all_channels(vcm);

	ArrayList_Free(registeredChannels);

	free(vcm);
}


static int wts_write_variable_uint(wStream *stream, UINT32 val)
{
	int cb;

	if (val <= 0xFF)
	{
		cb = 0;
		Stream_Write_UINT8(stream, val);
	}
	else if (val <= 0xFFFF)
	{
		cb = 1;
		Stream_Write_UINT16(stream, val);
	}
	else
	{
		cb = 2;
		Stream_Write_UINT32(stream, val);
	}

	return cb;
}

static BOOL vc_handle_read(registered_virtual_channel *regVC, int readLimit) {
	HANDLE handle = regVC->pipe_client;
	DWORD bytesRead;
	UINT32 payloadLen;
	BOOL first;
	BYTE *writeBuffer, *buffer;
	wStream *s;
	int cbLen, cbChId, sendlength, totalRead;
	ULONG toWrite;

	totalRead = 0;
	while (totalRead < readLimit) {
		if (!ReadFile(handle, regVC->pipe_target_buffer, regVC->pipe_expected_bytes, &bytesRead, NULL))
		{
			return (GetLastError() == ERROR_NO_DATA);
		}

		totalRead += bytesRead;
		regVC->pipe_expected_bytes -= bytesRead;
		regVC->pipe_target_buffer += bytesRead;
		if (regVC->pipe_expected_bytes)
			continue;

		/* when here we have read expected bytes and we're doing a state change
		 * (header --> payload, or payload --> headers)
		 */

		if (regVC->pipe_waiting_length) {
			/* got headers read switch to payload reading */
			regVC->pipe_expected_bytes = regVC->pipe_current_packet_length = regVC->header_buffer[0] |
					(regVC->header_buffer[1] << 8) |
					(regVC->header_buffer[2] << 16) |
					(regVC->header_buffer[3] << 24);

			if (!Stream_EnsureCapacity(regVC->pipe_input_buffer, regVC->pipe_expected_bytes)) {
				WLog_ERR(TAG, "Stream re-allocation failed");
				return FALSE;
			}
			regVC->pipe_target_buffer = Stream_Buffer(regVC->pipe_input_buffer);
			regVC->pipe_waiting_length = FALSE;
			continue;
		}

		/* got the payload, forward the packet to the RDP peer and go back to
		 * header reading.
		 */
		/* WLog_DBG(TAG, "packet with size %"PRIu32"", regVC->pipe_current_packet_length); */
		payloadLen = regVC->pipe_current_packet_length;
		writeBuffer = Stream_Buffer(regVC->pipe_input_buffer);

		if (regVC->channel_type == RDP_PEER_CHANNEL_TYPE_DVC) {
			if (regVC->vcm->drdynvc_state != DRDYNVC_STATE_READY)
			{
				WLog_ERR(TAG, "error: dynamic virtual channel is not ready");
				return FALSE;
			}

			first = TRUE;
			while (payloadLen > 0)
			{
				s = Stream_New(NULL, regVC->client->settings->VirtualChannelChunkSize);
				if (!s) {
					WLog_ERR(TAG, "failed to create Stream of size %"PRIu32"", regVC->client->settings->VirtualChannelChunkSize);
					return FALSE;
				}
				buffer = Stream_Buffer(s);

				Stream_Seek_UINT8(s);
				cbChId = wts_write_variable_uint(s, regVC->channel_id);

				if (first && (payloadLen > (UINT32) Stream_GetRemainingLength(s)))
				{
					cbLen = wts_write_variable_uint(s, payloadLen);
					buffer[0] = (DATA_FIRST_PDU << 4) | (cbLen << 2) | cbChId;
				}
				else
				{
					buffer[0] = (DATA_PDU << 4) | cbChId;
				}

				first = FALSE;
				toWrite = Stream_GetRemainingLength(s);

				if (toWrite > payloadLen) {
					toWrite = payloadLen;
				}

				Stream_Write(s, writeBuffer, toWrite);
				sendlength = Stream_GetPosition(s);

				if (!regVC->client->SendChannelData(regVC->client, regVC->vcm->drdynvc_channel_id, Stream_Buffer(s), sendlength))
				{
					WLog_ERR(TAG, "SendChannelData failed for dynamic virtual channel id %"PRIu32"", regVC->vcm->drdynvc_channel_id);
					return FALSE;
				}

				payloadLen -= toWrite;
				writeBuffer += toWrite;

				Stream_Free(s, TRUE);
			}

		} else {
			if (!regVC->client->SendChannelData(regVC->client, regVC->channel_id, Stream_Buffer(regVC->pipe_input_buffer),
					regVC->pipe_current_packet_length))
			{
				WLog_ERR(TAG, "SendChannelData failed for static virtual channel id %"PRIu32"", regVC->channel_id);
				return FALSE;
			}
		}

		/* reset to initial state */
		regVC->pipe_expected_bytes = 4;
		regVC->pipe_waiting_length = TRUE;
		regVC->pipe_target_buffer = regVC->header_buffer;
	}

	if (totalRead > readLimit) {
		return eventsource_reschedule_for_read(regVC->event_source_client);
	}

	return TRUE;
}

/* event loop callback for a channel pipe event */
static int handle_vc_named_pipe_event(int mask, int fd, HANDLE handle, void *data)
{
	OGON_UNUSED(fd);
	OGON_UNUSED(handle);

	registered_virtual_channel *channel = (registered_virtual_channel *)data;

	if (mask & OGON_EVENTLOOP_WRITE) {
		channel->writeBlocked = FALSE;
		if (!vc_flush_xmit_buffer(channel, VC_BYTES_LIMIT_PER_LOOP_TURN)) {
			/* close detected during flush */
			goto out_shutdown;
		}

		if (!channel->writeBlocked) {
			/* everything has been flushed remove the write availability scanning */
			WLog_DBG(TAG, "removing write availability scanning");
			if (!eventsource_change_source(channel->event_source_client, OGON_EVENTLOOP_READ)) {
				WLog_ERR(TAG, "error removing write scanning");
				goto out_shutdown;
			}
		}
	}

	if (mask & OGON_EVENTLOOP_READ) {
		if (!vc_handle_read(channel, VC_BYTES_LIMIT_PER_LOOP_TURN))
			goto out_shutdown;
	}

	return 1;


out_shutdown:
	virtual_manager_close_dynamic_virtual_channel_common(channel);
	if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_DVC) {
		if (!dvc_send_close(channel)) {
			WLog_ERR(TAG, "error while sending close in DVC");
		}
	}
	vc_free(channel);
	return -1;
}


static int handle_vc_named_pipe_connect_event(int mask, int fd, HANDLE handle, void *data)
{
	OGON_UNUSED(fd);
	registered_virtual_channel *channel = (registered_virtual_channel *)data;
	BOOL fConnected;
	DWORD dwPipeMode;
	HANDLE createdPipe;

	if (!(mask & OGON_EVENTLOOP_READ))
		return TRUE;

	fConnected = ConnectNamedPipe(handle, NULL);

	if (!fConnected)
		fConnected = (GetLastError() == ERROR_PIPE_CONNECTED);

	if (!fConnected)
	{
		WLog_ERR(TAG, "error connecting named pipe");
		return FALSE;
	}

	// removing and closing old client
	vc_disconnect_client_part(channel);

	// adding new client
	dwPipeMode = PIPE_NOWAIT;
	if (!SetNamedPipeHandleState(handle, &dwPipeMode, NULL, NULL)) {
		WLog_ERR(TAG, "SetNamedPipeHandleState failed");
		return FALSE;
	}

	channel->pipe_client = handle;
	channel->pipe_alive = TRUE;
	channel->channel_instance++;

	channel->event_source_client = eventloop_add_handle(channel->event_loop, OGON_EVENTLOOP_READ,
			handle, handle_vc_named_pipe_event, channel);
	if (!channel->event_source_client) {
		WLog_ERR(TAG, "error adding vc client pipe handle to event loop");
		vc_disconnect_client_part(channel);
		return FALSE;
	}

	/**
	 * Note: accepting a pipe always recreates the binding socket so we have to
	 * rewire the associated event source
	 */
	if (!eventloop_remove_source(&channel->event_source_server)) {
		WLog_ERR(TAG, "error removing server event source from event loop");
		return FALSE;
	}

	createdPipe = ogon_named_pipe_create(channel->pipe_name);
	if (createdPipe == INVALID_HANDLE_VALUE) {
		WLog_ERR(TAG, "error creating channel named pipe (%s)", channel->pipe_name);
		return FALSE;
	}

	channel->pipe_server = createdPipe;
	channel->event_source_server = eventloop_add_handle(channel->event_loop, OGON_EVENTLOOP_READ,
			createdPipe, handle_vc_named_pipe_connect_event, channel);
	if (!channel->event_source_server) {
		WLog_ERR(TAG, "error adding vc server pipe handle to event loop");
		return FALSE;
	}

	return TRUE;
}

BOOL virtual_manager_open_virtual_channel_send_error(struct ogon_notification_vc_connect *notification, wMessage *msg)
{
	int error = ogon_icp_sendResponse(notification->tag, msg->id, 0, FALSE, NULL);
	if (error != 0) {
		WLog_ERR(TAG, "ogon_icp_sendResponse failed while sending virtual channel open error");
		return FALSE;
	}
	return TRUE;
}

BOOL virtual_manager_open_static_virtual_channel(ogon_connection *conn, ogon_vcm *vcm, wMessage *msg)
{
	char buffer[1024] = {0};
	wArrayList *registeredChannels;
	registered_virtual_channel *currentChannel;
	struct ogon_notification_vc_connect *notification = (struct ogon_notification_vc_connect *) msg->wParam;
	int error = 0;
	HANDLE createdPipe = INVALID_HANDLE_VALUE;
	int length = 0;

	registeredChannels = vcm->registered_virtual_channels;
	length = strlen(notification->vcname);
	if (length > 8)	{
		WLog_ERR(TAG, "open static channel: name too long (%s)", notification->vcname);
		virtual_manager_open_virtual_channel_send_error(notification, msg);
		return TRUE;
	}

	if (!WTSIsChannelJoinedByName(vcm->client, notification->vcname)) {
		WLog_ERR(TAG, "open static channel: channel (%s) is not registered", notification->vcname);
		virtual_manager_open_virtual_channel_send_error(notification, msg);
		return TRUE;
	}

	// first check if this pipe is already opened
	currentChannel = VirtualChannelManagerGetChannelByNameAndType(vcm, notification->vcname, RDP_PEER_CHANNEL_TYPE_SVC);
	if (currentChannel) {
		error = ogon_icp_sendResponse(notification->tag, msg->id, 0, TRUE, currentChannel);
		if (error != 0) {
			WLog_ERR(TAG, "open static channel: ogon_icp_sendResponse failed");
			return FALSE;
		}
		return TRUE;
	}

	snprintf(buffer, sizeof(buffer), "\\\\.\\pipe\\%s_%ld", notification->vcname, conn->id);

	createdPipe = ogon_named_pipe_create(buffer);
	if (createdPipe == INVALID_HANDLE_VALUE) {
		WLog_ERR(TAG, "open static channel: error creating named pipe (%s)", buffer);
		virtual_manager_open_virtual_channel_send_error(notification, msg);
		return FALSE;
	}

	// register named pipe and handle
	currentChannel = vc_new(notification->vcname, buffer, createdPipe, vcm, conn);
	if (!currentChannel)
		return FALSE;

	currentChannel->channel_type = RDP_PEER_CHANNEL_TYPE_SVC;
	currentChannel->event_source_server = eventloop_add_handle(conn->runloop->evloop, OGON_EVENTLOOP_READ,
			createdPipe, handle_vc_named_pipe_connect_event, currentChannel);
	if (!currentChannel->event_source_server) {
		WLog_ERR(TAG, "open static channel: error adding pipe handle to rds connection event loop");
		goto out_error;
	}

	if (!WTSChannelSetHandleByName(vcm->client, notification->vcname, currentChannel)) {
		WLog_ERR(TAG, "open static channel: WTSChannelSetHandleByName failed");
		goto out_error;
	}

	if (ArrayList_Add(registeredChannels, currentChannel) < 0) {
		WLog_ERR(TAG, "open static channel: unable to add channel in the list");
		goto out_error;
	}

	WLog_DBG(TAG, "static channel id %"PRIu32" created successfully", currentChannel->channel_id);

	error = ogon_icp_sendResponse(notification->tag, msg->id, 0, TRUE, currentChannel);
	if (error != 0) {
		WLog_ERR(TAG, "open static channel: ogon_icp_sendResponse failed");
		return FALSE;
	}
	return TRUE;

out_error:
	vc_free(currentChannel);

	CloseHandle(createdPipe);
	virtual_manager_open_virtual_channel_send_error(notification, msg);
	return FALSE;
}

static void wts_write_drdynvc_header(wStream *s, BYTE Cmd, UINT32 ChannelId)
{
	BYTE* bm;
	int cbChId;

	Stream_GetPointer(s, bm);
	Stream_Seek_UINT8(s);
	cbChId = wts_write_variable_uint(s, ChannelId);
	*bm = ((Cmd & 0x0F) << 4) | cbChId;
}


static BOOL wts_write_drdynvc_create_request(wStream *s, UINT32 ChannelId, const char *ChannelName)
{
	size_t len;

	wts_write_drdynvc_header(s, CREATE_REQUEST_PDU, ChannelId);
	len = strlen(ChannelName) + 1;
	if (!Stream_EnsureRemainingCapacity(s, len)) {
		return FALSE;
	}
	Stream_Write(s, ChannelName, len);
	return TRUE;
}

BOOL virtual_manager_open_dynamic_virtual_channel(ogon_connection *conn, ogon_vcm *vcm, wMessage *msg)
{
	char buffer[MAX_PATH +1]  = {0};
	wArrayList *registeredChannels;
	registered_virtual_channel *currentChannel = NULL;
	struct ogon_notification_vc_connect *notification = (struct ogon_notification_vc_connect *) msg->wParam;
	int error = 0;
	HANDLE createdPipe = INVALID_HANDLE_VALUE;
	int length= 0;
	char str[26] = {0};
	int templength = 0;
	wStream *s = NULL;

	length = strlen(notification->vcname);
	sprintf(str, "_DYN_%ld", conn->id);
	templength = strlen(str);

	if (length > (MAX_PATH - 9 - templength) ) {
		WLog_ERR(TAG, "open dynamic channel: name too long (%s)", notification->vcname);
		virtual_manager_open_virtual_channel_send_error(notification, msg);
		return TRUE;
	}

	registeredChannels = vcm->registered_virtual_channels;
	if (!WTSIsChannelJoinedByName(vcm->client, "drdynvc")) {
		WLog_ERR(TAG, "open dynamic channel: channel (%s) is not registered", notification->vcname);
		virtual_manager_open_virtual_channel_send_error(notification, msg);
		return TRUE;
	}

	// first check if this pipe is opened
	currentChannel = VirtualChannelManagerGetChannelByNameAndType(vcm, notification->vcname, RDP_PEER_CHANNEL_TYPE_DVC);
	if (currentChannel) {
		error = ogon_icp_sendResponse(notification->tag, msg->id, 0, TRUE, currentChannel);
		if (error != 0) {
			WLog_ERR(TAG, "open dynamic channel: ogon_icp_sendResponse failed");
			return FALSE;
		}
		return TRUE;
	}

	strcpy(buffer, "\\\\.\\pipe\\");
	strcat(buffer, notification->vcname);
	strcat(buffer, str);

	createdPipe = ogon_named_pipe_create(buffer);
	if (createdPipe == INVALID_HANDLE_VALUE) {
		virtual_manager_open_virtual_channel_send_error(notification, msg);
		return FALSE;
	}

	// register named pipe and handle
	currentChannel = vc_new(notification->vcname, buffer, createdPipe, vcm, conn);
	if (!currentChannel) {
		WLog_ERR(TAG, "open dynamic channel: vc_new failed");
		goto closePipeError;
	}

	currentChannel->channel_id = vcm->dvc_channel_id_seq++;
	currentChannel->channel_type = RDP_PEER_CHANNEL_TYPE_DVC;

	currentChannel->event_source_server = eventloop_add_handle(conn->runloop->evloop , OGON_EVENTLOOP_READ,
			createdPipe, handle_vc_named_pipe_connect_event, currentChannel);
	if (!currentChannel->event_source_server) {
		WLog_ERR(TAG, "open dynamic channel: error adding pipe handle to rds connection event loop");
		goto pipeError;
	}

	s = Stream_New(NULL, 64);
	if (!s) {
		WLog_ERR(TAG, "open dynamic channel: Stream_New failed");
		goto eventLoopRemoveError;
	}

	if (!wts_write_drdynvc_create_request(s, currentChannel->channel_id, currentChannel->vc_name)) {
		WLog_ERR(TAG, "Unable to create drdynvc request");
		goto StreamFreeError;
	}
	if (!vcm->client->SendChannelData(vcm->client, currentChannel->vcm->drdynvc_channel_id, Stream_Buffer(s), Stream_GetPosition(s))) {
		WLog_ERR(TAG, "open dynamic channel: SendChannelData failed");
		goto StreamFreeError;
	}

	WLog_DBG(TAG, "Sent dynamic channel creation request for channel id %"PRIu32" (%s)", currentChannel->channel_id, currentChannel->vc_name);

	if (ArrayList_Add(registeredChannels, currentChannel) < 0) {
		WLog_ERR(TAG, "open dynamic channel: unable to add channel in the list");
		goto StreamFreeError;
	}

	Stream_Free(s, TRUE);
	currentChannel->dvc_open_tag = notification->tag;
	currentChannel->dvc_open_id = msg->id;
	return TRUE;

StreamFreeError:
	Stream_Free(s, TRUE);
eventLoopRemoveError:
	eventloop_remove_source(&currentChannel->event_source_server);
pipeError:
	vc_free(currentChannel);
closePipeError:
	CloseHandle(createdPipe);
	virtual_manager_open_virtual_channel_send_error(notification, msg);
	return FALSE;

}

static BOOL dvc_send_close(registered_virtual_channel *dvc) {
	ogon_vcm *vcm;
	wStream *s;
	BOOL ret = TRUE;

	if (dvc->dvc_open_state != DVC_OPEN_STATE_SUCCEEDED)
		return TRUE;

	vcm = dvc->vcm;
	s = Stream_New(NULL, 8);
	if (!s) {
		WLog_ERR(TAG, "dvc send close: Stream_New failed");
		return FALSE;
	}

	wts_write_drdynvc_header(s, CLOSE_REQUEST_PDU, dvc->channel_id);
	if (!vcm->client->SendChannelData(vcm->client, vcm->drdynvc_channel_id, Stream_Buffer(s), Stream_GetPosition(s))) {
		WLog_ERR(TAG, "dvc send close: SendChannelData failed");
		ret = FALSE;
	}

	Stream_Free(s, TRUE);
	return ret;
}

BOOL virtual_manager_close_dynamic_virtual_channel_common(registered_virtual_channel *channel) {
	ogon_vcm *vcm = channel->vcm;
	wArrayList *registeredChannels = vcm->registered_virtual_channels;

	if (vcm == NULL) {
		WLog_ERR(TAG, "OgonVirtualChannelManager was not set!");
		return FALSE;
	}

	vc_disconnect_server_part(channel);
	vc_disconnect_client_part(channel);

	ArrayList_Remove(registeredChannels, channel);

	WLog_ERR(TAG, "%s channel id %"PRIu32" closed",
			 (channel->channel_type == RDP_PEER_CHANNEL_TYPE_DVC) ? "Dynamic" : "Static",
			 channel->channel_id);
	WTSChannelSetHandleById(vcm->client, channel->channel_id, NULL);
	return TRUE;
}


BOOL virtual_manager_close_dynamic_virtual_channel(ogon_vcm *vcm, wMessage *msg) {
	int count;
	int error;
	int index;
	struct ogon_notification_vc_disconnect *notification = (struct ogon_notification_vc_disconnect *) msg->wParam;
	wArrayList *registeredChannels = vcm->registered_virtual_channels;
	registered_virtual_channel *channel = NULL;

	if (vcm == NULL) {
		return FALSE;
	}

	count = ArrayList_Count(registeredChannels);
	for (index = 0; index < count; index++)
	{
		channel = (registered_virtual_channel *) ArrayList_GetItem(registeredChannels, index);
		if (strncasecmp(channel->vc_name,notification->vcname,strlen(channel->vc_name)) != 0)
			continue;

		if (notification->instance == channel->channel_instance) {
			// shut down server and client

			virtual_manager_close_dynamic_virtual_channel_common(channel);

			if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_DVC) {
				if (!dvc_send_close(channel)) {
					WLog_ERR(TAG, "error while sending close in DVC");
				}
			}

			vc_free(channel);

			error = ogon_icp_sendResponse(notification->tag, msg->id, 0, TRUE, NULL);
			if (error != 0) {
				WLog_ERR(TAG, "close dynamic channel: ogon_icp_sendResponse failed");
				return FALSE;
			}
			return TRUE;
		}

		/* it was an old instance, which was replaced by a newer and the client socket is already closed */
		error = ogon_icp_sendResponse(notification->tag, msg->id, 0, TRUE, NULL);
		if (error != 0) {
			WLog_ERR(TAG, "close dynamic channel: ogon_icp_sendResponse failed (old instance)");
			return FALSE;
		}
		return TRUE;
	}

	error = ogon_icp_sendResponse(notification->tag, msg->id, 0, FALSE, NULL);
	if (error != 0) {
		WLog_ERR(TAG, "close dynamic channel: ogon_icp_sendResponse failed (not found)");
		return FALSE;
	}
	return TRUE;
}

BOOL virtual_manager_write_internal_virtual_channel(internal_virtual_channel *intVC, BYTE *data, UINT32 length, UINT32 *pWritten)
{
	int cbLen;
	int cbChId;
	BOOL first = TRUE;
	UINT32 toWrite;
	UINT32 written = 0;
	BYTE *streamBuffer;
	BOOL result = FALSE;
	wStream *s = NULL;
	registered_virtual_channel *regVC;

	if (!intVC || !intVC->channel) {
		return FALSE;
	}

	regVC = intVC->channel;

	/* static channel data */
	if (regVC->channel_type == RDP_PEER_CHANNEL_TYPE_SVC) {
		if (!length)
			goto out_success;
		if (!data)
			goto out_error;
		if (!regVC->client->SendChannelData(regVC->client, regVC->channel_id, data, length)) {
			WLog_ERR(TAG, "SendChannelData failed for internal static virtual channel id %"PRIu32"", regVC->channel_id);
			goto out_error;
		}

		goto out_success;
	}

	/* dynamic channel data */
	if (regVC->channel_type != RDP_PEER_CHANNEL_TYPE_DVC) {
		WLog_ERR(TAG, "unknown virtual channel type %"PRIu16"", regVC->channel_type);
		goto out_error;
	}

	if (regVC->vcm->drdynvc_state != DRDYNVC_STATE_READY) {
		WLog_ERR(TAG, "error: dynamic virtual channel is not ready");
		goto out_error;
	}

	if (!length)
		goto out_success;
	if (!data)
		goto out_error;

	if (!(s = Stream_New(NULL, regVC->client->settings->VirtualChannelChunkSize))) {
		WLog_ERR(TAG, "%s: error creating stream", __FUNCTION__);
		goto out_error;
	}
	streamBuffer = Stream_Buffer(s);

	while (length > 0) {
		Stream_SetPosition(s, 1);

		cbChId = wts_write_variable_uint(s, regVC->channel_id);

		if (first && (length > (UINT32) Stream_GetRemainingLength(s))) {
			cbLen = wts_write_variable_uint(s, length);
			streamBuffer[0] = (DATA_FIRST_PDU << 4) | (cbLen << 2) | cbChId;
		} else {
			streamBuffer[0] = (DATA_PDU << 4) | cbChId;
		}

		first = FALSE;
		toWrite = Stream_GetRemainingLength(s);

		if (toWrite > length) {
			toWrite = length;
		}

		Stream_Write(s, data, toWrite);

		if (!regVC->client->SendChannelData(regVC->client, regVC->vcm->drdynvc_channel_id, Stream_Buffer(s), Stream_GetPosition(s))) {
			WLog_ERR(TAG, "SendChannelData failed for internal dynamic virtual channel id %"PRIu32"", regVC->vcm->drdynvc_channel_id);
			goto out_error;
		}
		written += toWrite;
		length -= toWrite;
		data += toWrite;
	}

out_success:
	result = TRUE;

out_error:
	if (s)
		Stream_Free(s, TRUE);
	if (pWritten)
		*pWritten = written;
	return result;
}

internal_virtual_channel *virtual_manager_open_internal_virtual_channel(
		ogon_vcm *vcm, const char *name, BOOL isDynamic)
{
	registered_virtual_channel *regVC = NULL;
	internal_virtual_channel *intVC = NULL;
	wStream *s = NULL;
	UINT16 channelType = isDynamic ? RDP_PEER_CHANNEL_TYPE_DVC : RDP_PEER_CHANNEL_TYPE_SVC;

	if (isDynamic) {
		if (!WTSIsChannelJoinedByName(vcm->client, "drdynvc")) {
			WLog_ERR(TAG, "open internal channel: drdynvc is not registered");
			return NULL;
		}
	} else {
		if (strlen(name) > 8) {
			WLog_ERR(TAG, "open internal channel: name too long (%s)", name);
			return NULL;
		}
		if (!WTSIsChannelJoinedByName(vcm->client, name)) {
			WLog_ERR(TAG, "open internal channel: channel (%s) is not registered", name);
			return NULL;
		}
	}

	if (VirtualChannelManagerGetChannelByNameAndType(vcm, name, channelType)) {
		WLog_ERR(TAG, "open internal channel: channel (%s) already exists", name);
		return NULL;
	}

	if (!(intVC = calloc(1, sizeof(internal_virtual_channel)))) {
		WLog_ERR(TAG, "open internal channel: failed to allocate internal channel (%s)", name);
		goto err;
	}

	if (!(regVC = vc_new(name, NULL, NULL, vcm, NULL))) {
		WLog_ERR(TAG, "open internal channel: vc_new failed (%s)", name);
		goto err;
	}

	intVC->channel = regVC;
	regVC->channel_type = channelType;
	regVC->internalChannel = intVC;

	if (!(s = Stream_New(NULL, 64))) {
		WLog_ERR(TAG, "open internal channel: Stream_New failed");
		goto err;
	}

	if (isDynamic) {
		regVC->channel_id = vcm->dvc_channel_id_seq++;
		wts_write_drdynvc_create_request(s, regVC->channel_id, regVC->vc_name);
		if (!vcm->client->SendChannelData(vcm->client, vcm->drdynvc_channel_id, Stream_Buffer(s), Stream_GetPosition(s))) {
			WLog_ERR(TAG, "open internal channel: SendChannelData failed");
			goto err;
		}
	} else {
		if (!WTSChannelSetHandleByName(vcm->client, name, regVC)) {
			WLog_ERR(TAG, "open internal channel: WTSChannelSetHandleByName failed");
			goto err;
		}
	}

	if (ArrayList_Add(vcm->registered_virtual_channels, regVC) < 0) {
		WLog_ERR(TAG, "open internal channel: error adding channel in the list");
		goto err;
	}

	if (isDynamic) {
		WLog_DBG(TAG, "internal dynamic channel with id %"PRIu32" created successfully (%s), waiting for creation response",
				 regVC->channel_id, regVC->vc_name);
	} else {
		WLog_DBG(TAG, "internal static channel with id %"PRIu32" created successfully (%s)", regVC->channel_id, regVC->vc_name);
	}

	Stream_Free(s, TRUE);
	return intVC;

err:
	if (s) {
		Stream_Free(s, TRUE);
	}

	free(intVC);

	if (regVC) {
		vc_free(regVC);
	}

	return NULL;
}

BOOL virtual_manager_close_internal_virtual_channel(internal_virtual_channel *intVC) {
	int count;
	int index;
	wArrayList *registeredChannels;
	registered_virtual_channel *channel = NULL;
	ogon_vcm *vcm = NULL;

	if (!intVC || !intVC->channel || !intVC->channel->vcm) {
		return FALSE;
	}

	vcm = intVC->channel->vcm;
	registeredChannels = vcm->registered_virtual_channels;

	count = ArrayList_Count(registeredChannels);
	for (index = 0; index < count; index++)
	{
		channel = (registered_virtual_channel *) ArrayList_GetItem(registeredChannels, index);
		if (channel != intVC->channel)
			continue;

		if (channel->channel_type == RDP_PEER_CHANNEL_TYPE_DVC) {
			if (!dvc_send_close(channel)) {
				WLog_ERR(TAG, "error while sending close in DVC");
			}
		}

		ArrayList_Remove(registeredChannels, channel);
		WLog_DBG(TAG, "internal %s channel id %"PRIu32" closed",
				(channel->channel_type == RDP_PEER_CHANNEL_TYPE_DVC) ? "Dynamic" : "Static",
				channel->channel_id);

		WTSChannelSetHandleByName(vcm->client, channel->vc_name, NULL);
		vc_free(channel);
		return TRUE;
	}

	WLog_ERR(TAG, "%s: internal channel not found", __FUNCTION__);
	return FALSE;
}
