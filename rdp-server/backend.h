/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Backend
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
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

#ifndef _OGON_RDPSRV_BACKEND_H_
#define _OGON_RDPSRV_BACKEND_H_

#include <winpr/collections.h>
#include <freerdp/codec/region.h>
#include <freerdp/utils/ringbuffer.h>

#include <ogon/backend.h>

#include "eventloop.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef int(*backend_server_protocol_cb)(ogon_connection *conn, ogon_message *msg);

/** @brief holds data related to the backend connection, the content provider */
struct _ogon_backend_connection {
	HANDLE pipe;
	ogon_event_source *pipeEventSource;
	void* damage;
	unsigned int damageUserId;
	BOOL writeReady;
	RingBuffer xmitBuffer;
	UINT32 backendVersion;
	BOOL waitingSyncReply;
	ogon_screen_infos screenInfos;
	UINT32 lastSetSystemPointer;
	BOOL haveBackendPointer;
	ogon_msg_set_pointer lastSetPointer;

	BOOL active;
	BOOL stateWaitingHeader;
	DWORD expectedReadBytes;
	wStream *recvBuffer;
	ogon_message currentInMessage;
	UINT16 currentInMessageType;

	ogon_msg_capabilities capabilities;
	ogon_msg_framebuffer_sync_request framebufferSyncRequest;
	ogon_msg_synchronize_keyboard_event synchronizeKeyboard;
	ogon_msg_framebuffer_sync_request immediateSyncRequest;
	ogon_msg_seat_new seatNew;
	ogon_msg_seat_removed seatRemoved;
	wListDictionary *message_answer_list;
	UINT32 next_message_id;

	ogon_client_interface client;
	backend_server_protocol_cb *server;
	ogon_backend_props properties;
	BOOL version_exchanged;
	BOOL multiseatCapable;
};


/**
 * Creates a backend for the connection from given backend properties. Note that this
 * function also connects the named pipe and sends the first version packet.
 *
 * @param conn
 * @param props
 * @return
 */
ogon_backend_connection *backend_new(ogon_connection *conn, ogon_backend_props *props);

/**
 * Sends the capabilities message and also the synchronize keyboard packet to the
 * backend
 *
 * @param conn a ogon_connection
 * @param backend the corresponding backend
 * @param settings
 * @param width front width
 * @param height front height
 * @return
 */
BOOL ogon_backend_initialize(ogon_connection *conn, ogon_backend_connection *backend,
	rdpSettings *settings, UINT32 width, UINT32 height);

/**
 *	deletes the given backend. The backend pointer is nullified after the destruction.
 *
 * @param backendP the backend to destroy
 */
void backend_destroy(ogon_backend_connection **backendP);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _OGON_RDPSRV_BACKEND_H_ */
