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

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <unistd.h>

#include <winpr/file.h>
#include <winpr/pipe.h>
#include <winpr/ssl.h>

#include <freerdp/freerdp.h>
#include <freerdp/peer.h>

#include <ogon/backend.h>
#include <ogon/dmgbuf.h>

#include "../common/global.h"
#include "icp/icp_client_stubs.h"
#include "icp/pbrpc/pbrpc.h"
#include "peer.h"
#include "eventloop.h"
#include "app_context.h"
#include "channels.h"
#include "backend.h"


#define TAG OGON_TAG("core.peer")

BOOL ogon_connection_init_front(ogon_connection *conn);
int frontend_handle_frame_sent(ogon_connection *conn);
void handle_wait_timer_state(ogon_connection *conn);
BOOL ogon_frontend_install_frame_timer(ogon_connection *conn);
int ogon_resize_frontend(ogon_connection *conn, ogon_backend_connection *backend);


/* event loop callback for the stop event */
static int handle_stop_event(int mask, int fd, HANDLE handle, void *data) {
	OGON_UNUSED(handle);
	OGON_UNUSED(fd);
	ogon_connection *connection = (ogon_connection *)data;

	if (!(mask & OGON_EVENTLOOP_READ)) {
		return -1;
	}
	ogon_connection_close(connection);
	return 0;
}


static BOOL process_switch_pipe(ogon_connection *conn, wMessage *msg) {
	BOOL ret;
	struct ogon_notification_switch *notification = (struct ogon_notification_switch *)msg->wParam;
	rdpSettings* settings;
	freerdp_peer *client;
	UINT32 width = 0;
	UINT32 height = 0;
	BOOL resizeClient = FALSE;
	ogon_front_connection *front = &conn->front;

	client = conn->context.peer;
	settings = client->settings;

	if (conn->backend) {
		backend_destroy(&conn->backend);
	}

	if ((notification->maxWidth != 0) && (notification->maxWidth != settings->DesktopWidth)) {
		if (notification->maxWidth < front->initialDesktopWidth) {
			width = notification->maxWidth;
		} else {
			width = front->initialDesktopWidth;
		}
		resizeClient = TRUE;
	} else {
		width = settings->DesktopWidth;
	}

	if ((notification->maxHeight != 0) && (notification->maxHeight != settings->DesktopHeight)) {
		if (notification->maxHeight < front->initialDesktopHeight) {
			height = notification->maxHeight;
		} else {
			height = front->initialDesktopHeight;
		}
		resizeClient = TRUE;
	} else {
		height = settings->DesktopHeight;
	}

	conn->backend = backend_new(conn, &notification->props);
	ret = (conn->backend != NULL);

	/* backend was created successfully send capabilities */
	if (ret) {
		if (!ogon_backend_initialize(conn, conn->backend, settings, width, height))	{
			WLog_ERR(TAG, "error sending initial packet during switch");
			backend_destroy(&conn->backend);
			ret = FALSE;
		}
	}

	if (ogon_icp_sendResponse(notification->tag, msg->id, 0, ret, NULL) != 0) {
		WLog_ERR(TAG, "problem occurred while sending switchPipe response");
		ret = FALSE;
	}

	/* note: if everything went well the backend took ownership of notification->props (and so all
	 * 	pointers in notification->props are NULL), otherwise we will free them here */
	ogon_backend_props_free(&notification->props);
	free(notification);

	/* If backend creation or responding to switchPipe failed back out */
	if (!ret) {
		return ret;
	}

	ogon_state_set_event(front->state, OGON_EVENT_BACKEND_SWITCH);
	if (resizeClient) {
		if (width != settings->DesktopWidth || height != settings->DesktopHeight) {
			/* make sure no re-activation is triggered when one is already in process */
			if (ogon_state_get(front->state) == OGON_STATE_WAITING_RESIZE) {
				front->pendingResizeWidth = width;
				front->pendingResizeHeight = height;
				return TRUE;
			} else {
				settings->DesktopWidth = width;
				settings->DesktopHeight = height;
				client->update->DesktopResize(client->update->context);
				ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_TRIGGER_RESIZE);
			}
		}
	}

	return ret;
}

static BOOL process_logoff(ogon_connection *conn, wMessage *msg) {
	int error;
	BOOL ret = TRUE;
	struct ogon_notification_logoff *notification = (struct ogon_notification_logoff *)msg->wParam;

	ogon_connection_close(conn);

	if (conn->front.vcm) {
		virtual_manager_close_all_channels(conn->front.vcm);
	}

	error = ogon_icp_sendResponse(notification->tag, msg->id, 0, TRUE, NULL);
	if (error != 0)	{
		WLog_ERR(TAG, "problem occured while sending logging off response");
		ret = FALSE;
		goto out;
	}
	conn->sendDisconnect = FALSE;

out:
	free(notification);
	return ret;
}


static BOOL process_sbp_reply(ogon_connection *conn, wMessage *msg) {
	rds_notification_sbp *notification = (rds_notification_sbp *)msg->wParam;
	BOOL ret = TRUE;

	if (!conn->backend) {
		WLog_ERR(TAG, "processing sbp reply: no backend");
		ret = FALSE;
		goto out_exit;
	}

	if (!conn->backend->client.Sbp(conn->backend, &notification->reply)) {
		WLog_ERR(TAG, "processing sbp reply: error when sending reply to backend");
		ret = FALSE;
	}

out_exit:
	free(notification->reply.data);
	free(notification);
	return ret;
}


static BOOL process_vc_connect(ogon_connection *conn, wMessage *msg) {
	struct ogon_notification_vc_connect *notification = (struct ogon_notification_vc_connect *)msg->wParam;
	BOOL ret = TRUE;
	ogon_vcm *vcm = conn->front.vcm;

	if (!vcm) {
		WLog_ERR(TAG, "processing vc connect: no virtual channel manager");
		int error = 0;
		error = ogon_icp_sendResponse(notification->tag, msg->id, 0, FALSE, NULL);
		if (error != 0) {
			WLog_ERR(TAG, "ogon_icp_sendResponse failed");
		}
		ret = FALSE;
		goto out_exit;
	}

	if (notification->flags & WTS_CHANNEL_OPTION_DYNAMIC) {
		// dynamic virtual channel
		ret = virtual_manager_open_dynamic_virtual_channel(conn, vcm, msg);
	} else {
		// static virtual channel
		ret = virtual_manager_open_static_virtual_channel(conn, vcm, msg);
	}

out_exit:
	free(notification->vcname);
	free(notification);
	return ret;
}



static BOOL process_vc_disconnect(ogon_connection *conn, wMessage *msg) {
	struct ogon_notification_vc_disconnect *notification = (struct ogon_notification_vc_disconnect *) msg->wParam;
	ogon_vcm *vcm = conn->front.vcm;
	BOOL ret = TRUE;

	if (!vcm) {
		WLog_ERR(TAG, "processing vc disconnect: no virtual channel manager");
		ret = FALSE;
		goto out_exit;
	}

	ret = virtual_manager_close_dynamic_virtual_channel(vcm,msg);

out_exit:
	free(notification->vcname);
	free(notification);
	return ret;
}

static BOOL process_start_remote_control(ogon_connection *conn, wMessage *msg) {
	struct ogon_notification_start_remote_control *notification = (struct ogon_notification_start_remote_control *) msg->wParam;
	struct rds_notification_new_shadowing_frontend *new_shadow_peer = NULL;
	ogon_front_connection *front = &conn->front;
	int mask, fd, error;
	ogon_source_state stateRdp;
	BOOL closeConn;

	if (conn->shadowing != conn) {
		WLog_ERR(TAG, "session %ld already shadowing session %ld", conn->id, conn->shadowing->id);
		goto out_error;
	}

	mask = eventsource_mask(front->rdpEventSource);
	fd = eventsource_fd(front->rdpEventSource);

	new_shadow_peer = (struct rds_notification_new_shadowing_frontend *)malloc(sizeof(*new_shadow_peer));
	if (!new_shadow_peer) {
		WLog_ERR(TAG, "unable to allocate memory for shadowing peer");
		goto out_error;
	}
	new_shadow_peer->startRemoteControl = *notification;
	new_shadow_peer->originalRdpMask = mask;
	new_shadow_peer->originalFd = fd;
	new_shadow_peer->srcConnection = conn;

	eventsource_store_state(front->rdpEventSource, &stateRdp);
	eventloop_remove_source(&front->rdpEventSource);
	eventloop_remove_source(&front->frameEventSource);
	conn->backend->active = FALSE;

	if (!app_context_post_message_connection(notification->targetId, NOTIFY_NEW_SHADOWING_FRONTEND, new_shadow_peer, NULL)) {
		WLog_ERR(TAG, "destination session %"PRIu32" not found", notification->targetId);
		goto out_post_error;
	}

	ogon_state_set_event(front->state, OGON_EVENT_BACKEND_TRIGGER_REWIRE);
	free(notification);
	return TRUE;

out_post_error:
	ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_REWIRE_ERROR);

	conn->backend->active = TRUE;
	closeConn = FALSE;
	if (!(front->rdpEventSource = eventloop_restore_source(conn->runloop->evloop, &stateRdp))) {
		WLog_ERR(TAG, "unable to restore RDP event source");
		closeConn = TRUE;
	}

	if (!ogon_frontend_install_frame_timer(conn)) {
		WLog_ERR(TAG, "unable to restore frame timer event source");
		closeConn = TRUE;
	}

	if (closeConn) {
		ogon_connection_close(conn);
	}

out_error:
	error = ogon_icp_sendResponse(notification->tag, NOTIFY_START_REMOTE_CONTROL, 0, FALSE, NULL);
	if (error != 0)	{
		WLog_ERR(TAG, "error sending startRemoteControl response");
	}
	free(new_shadow_peer);
	free(notification);
	return FALSE;
}


static int handle_front_rdp_event(int mask, int fd, HANDLE handle, void *data) {
	OGON_UNUSED(fd);
	OGON_UNUSED(handle);
	ogon_connection *connection = (ogon_connection *)data;
	ogon_front_connection *front = &connection->front;
	freerdp_peer *frontPeer = connection->context.peer;

	if (mask & OGON_EVENTLOOP_WRITE) {
		/* first drain pending bytes in the write queue */
		if (frontPeer->DrainOutputBuffer(frontPeer) < 0) {
			WLog_ERR(TAG, "connection close during drainOutputBuffer");
			goto error;
		}

		/* set read ready so if we're in a renegociation with TLS, the next bits will be treated */
		mask |= OGON_EVENTLOOP_READ;

		if (!frontPeer->IsWriteBlocked(frontPeer)) {
			if (!eventsource_change_source(front->rdpEventSource, OGON_EVENTLOOP_READ)) {
				WLog_ERR(TAG, "error changing mask on rdpEventSource");
				goto error;
			}

			/* WLog_DBG(TAG, "not scanning write anymore"); */
			if (frontend_handle_frame_sent(connection) < 0) {
				WLog_ERR(TAG, "error handling frame sent actions");
				goto error;
			}
		} /* else {
			WLog_DBG(TAG, "%ld write blocked", connection->id);
		} */
	}

	if (mask & OGON_EVENTLOOP_READ) {
		if (!frontPeer->CheckFileDescriptor(frontPeer))	{
			WLog_ERR(TAG, "error during CheckFileDescriptor for %ld", connection->id);
			goto error;
		}
	}

	front->writeReady = (mask & OGON_EVENTLOOP_WRITE);
	return 0;

error:
	ogon_connection_close(connection);
	return -1;
}

BOOL initiate_immediate_request(ogon_connection *conn, ogon_front_connection *front,
	BOOL setDamage)
{
	RECTANGLE_16 rect16;
	ogon_backend_connection *backend = conn->shadowing->backend;

	if (!backend) {
		WLog_DBG(TAG, "%s ignored since backend has vanished", __FUNCTION__);
		return TRUE;
	}

	if (setDamage && front->encoder) {
		/* set a big damage that is the whole screen */
		rect16.top = 0;
		rect16.left = 0;
		rect16.right = backend->screenInfos.width;
		rect16.bottom = backend->screenInfos.height;
		region16_clear(&front->encoder->accumulatedDamage);
		if (!region16_union_rect(&front->encoder->accumulatedDamage, &front->encoder->accumulatedDamage, &rect16)) {
			WLog_ERR(TAG, "unable to union_rect damage");
			return FALSE;
		}
	}

	switch (ogon_state_get(front->state)) {
	/* in these states we force the sync */
	case OGON_STATE_WAITING_ACTIVE_OUTPUT:
	case OGON_STATE_WAITING_ACK:
	case OGON_STATE_WAITING_FRAME_SENT:

	case OGON_STATE_WAITING_TIMER:
	case OGON_STATE_WAITING_SYNC_REPLY:
		/* forces the state */
		ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_IMMEDIATE_REQUEST);

		if (!backend->client.ImmediateSyncRequest(backend, ogon_dmgbuf_get_id(backend->damage))) {
			WLog_ERR(TAG, "error sending immediateSync request");
		}
		backend->waitingSyncReply = TRUE;
		break;

	case OGON_STATE_WAITING_RESIZE:
	case OGON_STATE_EVENTLOOP_MOVE:
		break;
	case OGON_STATE_WAITING_BACKEND:
	default:
		WLog_ERR(TAG, "immediate request initiation is not expecting to be called from state %d", ogon_state_get(front->state));
		break;
	}

	return TRUE;
}

static BOOL post_rewire_original_backend(ogon_connection *conn, int fd, int mask) {
	struct ogon_notification_rewire_backend *rewireNotif;

	rewireNotif = (struct ogon_notification_rewire_backend *)malloc(sizeof(*rewireNotif) );
	if (!rewireNotif) {
		return FALSE;
	}

	ogon_state_set_event(conn->front.state, OGON_EVENT_BACKEND_TRIGGER_REWIRE);
	rewireNotif->rdpFd = fd;
	rewireNotif->rdpMask = mask;
	rewireNotif->rewire = TRUE;

	if (!app_context_post_message_connection(conn->id, NOTIFY_REWIRE_ORIGINAL_BACKEND, rewireNotif, NULL)) {
		WLog_ERR(TAG, "error posting notification to %ld", conn->id);
		free(rewireNotif);
		return FALSE;
	}
	return TRUE;
}


static BOOL process_new_shadowing_frontend(ogon_connection *conn, wMessage *msg) {
	struct rds_notification_new_shadowing_frontend *notif = (struct rds_notification_new_shadowing_frontend *) msg->wParam;
	ogon_connection *srcConn = notif->srcConnection;
	ogon_front_connection *front = &srcConn->front;
	ogon_backend_connection *backend = conn->backend;
	rdpSettings *settings = conn->context.settings;
	rdpSettings *spySettings = srcConn->context.settings;
	rdpPointerUpdate* pointer = srcConn->context.peer->update->pointer;

	int error;
	BOOL ret = FALSE;
	ogon_keyboard_modifiers_state *escapeModifiers;

	/* 'conn' is the spied connection, the spy is in srcConn */
	if (conn->shadowing != conn) {
		WLog_ERR(TAG, "%ld can't be shadowed as it is already shadowing session %ld", conn->id, conn->shadowing->id);
		if (!post_rewire_original_backend(srcConn, notif->originalFd, notif->originalRdpMask)) {
			WLog_ERR(TAG, "unable to send a send a 'rewire original backend' message to the original connection");
		}
		goto out;
	}

	front->rdpEventSource = eventloop_add_fd(conn->runloop->evloop, notif->originalRdpMask, notif->originalFd, handle_front_rdp_event, srcConn);
	if (!front->rdpEventSource) {
		WLog_ERR(TAG, "unable to install event handler for spying connection %ld", srcConn->id);
		if (!post_rewire_original_backend(srcConn, notif->originalFd, notif->originalRdpMask)) {
			WLog_ERR(TAG, "unable to send a send a 'rewire original backend' message to the original connection");
		}
		goto out;
	}
	ogon_state_prepare_shadowing(front->state, conn->front.state);

	/* store the shadowing escape sequence */
	escapeModifiers = &srcConn->shadowingEscapeModifiers;
	*escapeModifiers = 0;
	if (notif->startRemoteControl.hotKeyModifier & REMOTECONTROL_KBDCTRL_HOTKEY) {
		*escapeModifiers |= OGON_KEYBOARD_CTRL;
	}
	if (notif->startRemoteControl.hotKeyModifier & REMOTECONTROL_KBDALT_HOTKEY) {
		*escapeModifiers |= OGON_KEYBOARD_ALT;
	}
	if (notif->startRemoteControl.hotKeyModifier & REMOTECONTROL_KBDSHIFT_HOTKEY) {
		*escapeModifiers |= OGON_KEYBOARD_SHIFT;
	}

	srcConn->shadowingEscapeKey = notif->startRemoteControl.hotKeyVk;

	if (notif->startRemoteControl.flags & REMOTECONTROL_FLAG_DISABLE_KEYBOARD){
		front->inputFilter |= INPUT_FILTER_KEYBOARD;
	}

	if (notif->startRemoteControl.flags & REMOTECONTROL_FLAG_DISABLE_MOUSE){
		front->inputFilter |= INPUT_FILTER_MOUSE;
	}

	if (srcConn->front.indicators != conn->front.indicators && srcConn->context.update->SetKeyboardIndicators) {
		srcConn->front.indicators = conn->front.indicators;
		srcConn->context.update->SetKeyboardIndicators(&srcConn->context, srcConn->front.indicators);
	}

	srcConn->shadowing = conn;
	if (!LinkedList_AddLast(conn->frontConnections, srcConn)) {
		WLog_ERR(TAG, "failed to append src connnection to front connections");
		if (!eventloop_remove_source(&front->rdpEventSource)) {
			WLog_ERR(TAG, "unable to remove from the eventloop");
		}
		if (!post_rewire_original_backend(srcConn, notif->originalFd, notif->originalRdpMask)) {
			WLog_ERR(TAG, "unable to send a send a 'rewire original backend' message to the original connection");
		}
		goto out;
	}
	ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_NEW_SHADOWING);

	backend->seatNew.clientId = (UINT32) srcConn->id;
	backend->seatNew.keyboardLayout = spySettings->KeyboardLayout;
	backend->seatNew.keyboardType = spySettings->KeyboardType;
	backend->seatNew.keyboardSubType = spySettings->KeyboardSubType;
	if (!backend->client.SeatNew(conn->backend, &backend->seatNew)) {
		if (!LinkedList_Remove(conn->frontConnections, srcConn)) {
			WLog_ERR(TAG, "strange the spy to remove(%ld) is not in the frontConnections", srcConn->id);
		}
		if (!eventloop_remove_source(&front->rdpEventSource)) {
			WLog_ERR(TAG, "unable to remove from the eventloop");
		}
		if (!post_rewire_original_backend(srcConn, notif->originalFd, notif->originalRdpMask)) {
			WLog_ERR(TAG, "unable to send a send a 'rewire original backend' message to the original connection");
		}
		WLog_ERR(TAG, "an error occurred when notifying the seat arrival");
		goto out;
	}

	if ((spySettings->DesktopWidth != settings->DesktopWidth) || (spySettings->DesktopHeight != settings->DesktopHeight)) {
		/* resize the spy to the spied resolution */
		if (ogon_resize_frontend(srcConn, backend) < 0) {
			WLog_ERR(TAG, "failed to resize the spy");
			if (!conn->backend->client.SeatRemoved(conn->backend, (UINT32) srcConn->id)) {
				WLog_ERR(TAG, "error notifying seat removal for %ld", srcConn->id);
			}
			if (!LinkedList_Remove(conn->frontConnections, srcConn)) {
				WLog_ERR(TAG, "strange the spy to remove(%ld) is not in the frontConnections", srcConn->id);
			}
			if (!eventloop_remove_source(&front->rdpEventSource)) {
				WLog_ERR(TAG, "unable to remove from the eventloop");
			}
			if (!post_rewire_original_backend(srcConn, notif->originalFd, notif->originalRdpMask)) {
				WLog_ERR(TAG, "unable to send a send a 'rewire original backend' message to the original connection");
			}
			goto out;
		}
		ret = TRUE;
		goto out;
	}

	/* reset the last pointer */
	if (srcConn->backend->lastSetSystemPointer != backend->lastSetSystemPointer) {
		POINTER_SYSTEM_UPDATE pointer_system = { 0 };
		pointer_system.type = backend->lastSetSystemPointer;
		pointer->PointerSystem(&srcConn->context, &pointer_system);
	}

	/* force the spy with the last set pointer on the backend */
	if (backend->haveBackendPointer) {
		ogon_connection_set_pointer(srcConn, &backend->lastSetPointer);
	}

	if (initiate_immediate_request(conn, front, TRUE)) {
		handle_wait_timer_state(srcConn);
		ret = TRUE;
	}
out:
	error = ogon_icp_sendResponse(notif->startRemoteControl.tag, NOTIFY_START_REMOTE_CONTROL, 0, ret, NULL);
	if (error != 0)	{
		WLog_ERR(TAG, "problem occured while sending startRemoteControl response");
	}
	free(notif);
	return ret;
}

static BOOL process_rewire_original_backend(ogon_connection *conn, wMessage *msg) {
	struct ogon_notification_rewire_backend *notif = (struct ogon_notification_rewire_backend *) msg->wParam;
	ogon_front_connection *front = &conn->front;
	ogon_backend_connection *backend = conn->backend;
	rdpSettings *settings = conn->context.settings;
	POINTER_SYSTEM_UPDATE pointer_system = { 0 };
	rdpPointerUpdate* pointer = conn->context.peer->update->pointer;

	BOOL ret = FALSE;

	/*WLog_DBG(TAG, "%ld exiting shadowing", conn->id);
	if (conn->shadowing == conn) {
		WLog_ERR(TAG, "%ld wasn't shadowing", conn->id);
		goto out;
	}*/

	conn->shadowing = conn;
	if (!notif->rewire) {
		ret = TRUE;
		conn->runThread = FALSE;
		goto out;
	}
	/* reset the input filter */
	front->inputFilter = 0;
	/* rewire front RDP connection */
	front->rdpEventSource = eventloop_add_fd(conn->runloop->evloop, notif->rdpMask, notif->rdpFd, handle_front_rdp_event, conn);
	if (!front->rdpEventSource) {
		WLog_ERR(TAG, "unable to reinstall event handler for connection %ld", conn->id);
		goto out;
	}

	/* rewire timer event */
	if (!ogon_frontend_install_frame_timer(conn)) {
		WLog_ERR(TAG, "unable to reinstall timer handler for connection %ld", conn->id);
		goto out;
	}

	ogon_state_set_event(front->state, OGON_EVENT_BACKEND_REWIRE_ORIGINAL);

	backend->active = TRUE;

	/* Sync the indicators with the client state */
	if (backend->client.SynchronizeKeyboardEvent &&
			!backend->client.SynchronizeKeyboardEvent(backend, front->indicators, conn->id)) {
		 ogon_connection_close(conn);
		 ret = FALSE;
		 goto out;
	 }

	if ((settings->DesktopWidth != backend->screenInfos.width) || (settings->DesktopHeight != backend->screenInfos.height)) {
		/* resize the peer to the backend's size */
		if (ogon_resize_frontend(conn, backend) < 0) {
			WLog_ERR(TAG, "failed to resize the peer");
		}
		ret = TRUE;
		goto out;
	}

	/* sets back the pointer of the backend */
	pointer_system.type = backend->lastSetSystemPointer;
	pointer->PointerSystem(&conn->context, &pointer_system);

	if (backend->haveBackendPointer) {
		ogon_connection_set_pointer(conn, &backend->lastSetPointer);
	}

	if (initiate_immediate_request(conn, front, TRUE)) {
		ret = TRUE;
	}

out:
	free(notif);
	return ret;
}

static BOOL process_unwire_spy(ogon_connection *conn, wMessage *msg) {
	OGON_UNUSED(conn);
	ogon_connection *spy = (ogon_connection *)msg->wParam;
	struct rds_notification_stop_remote_control *notification;

	if (!LinkedList_Remove(conn->frontConnections, spy)) {
		WLog_ERR(TAG, "strange the spy to remove(%ld) is not in the frontConnections", spy->id);
		if (msg != 0 && msg->lParam != 0) {
			notification = (struct rds_notification_stop_remote_control *)msg->lParam;
			ogon_icp_sendResponse(notification->tag, NOTIFY_STOP_SHADOWING, 0, FALSE, NULL);
			free(notification);
		}
		return FALSE;
	}

	if (!conn->backend->client.SeatRemoved(conn->backend, (UINT32) spy->id)) {
		WLog_ERR(TAG, "error notifying seat removal for %ld", spy->id);
	}

	return ogon_post_exit_shadow_notification(spy, msg, TRUE);
}

static BOOL process_stop_shadowing(ogon_connection *conn, wMessage *msg) {
	BOOL ret = FALSE;
	struct rds_notification_stop_remote_control *notification;

	if (conn->shadowing == conn) {
		WLog_ERR(TAG, "%ld wasn't shadowing", conn->id);
		if (msg != 0 && msg->wParam != 0) {
			notification = (struct rds_notification_stop_remote_control *)msg->wParam;
			ogon_icp_sendResponse(notification->tag, NOTIFY_STOP_SHADOWING, 0, FALSE, NULL);
			free(notification);
		}
		ret = FALSE;
		goto out;
	}

	if (!app_context_post_message_connection(conn->shadowing->id, NOTIFY_UNWIRE_SPY, conn, msg->wParam)) {
		WLog_ERR(TAG, "error while posting the unwire spy message");
	}
	ret = TRUE;
out:
	return ret;
}

static void fillParameter(char *source, UINT32 *dest_len, char **dest) {
	if (source == NULL) {
		*dest_len = 0;
		*dest = NULL;
	} else {
		*dest_len = strlen(source) + 1;
		*dest = source;
	}
}

static BOOL process_user_message(ogon_connection *conn, wMessage *msg) {
	struct ogon_notification_msg_message *notification = (struct ogon_notification_msg_message *) msg->wParam;
	BOOL ret = TRUE;

	if (!conn->backend) {
		WLog_ERR(TAG, "process message: no backend");
		ret = FALSE;
		goto out_exit;
	}

	ogon_msg_message message;
	message.icp_tag = notification->tag;
	message.icp_type = msg->id;
	message.message_type = notification->type;
	message.style = notification->style;
	message.timeout = notification->timeout;
	message.parameter_num = notification->parameter_num;
	fillParameter(notification->parameter1, &message.parameter1_len, &message.parameter1);
	fillParameter(notification->parameter2, &message.parameter2_len, &message.parameter2);
	fillParameter(notification->parameter3, &message.parameter3_len, &message.parameter3);
	fillParameter(notification->parameter4, &message.parameter4_len, &message.parameter4);
	fillParameter(notification->parameter5, &message.parameter5_len, &message.parameter5);

	conn->backend->client.Message(conn->backend, &message);

out_exit:
	free(notification->parameter1);
	free(notification->parameter2);
	free(notification->parameter3);
	free(notification->parameter4);
	free(notification->parameter5);
	free(notification);
	return ret;
}

/* event loop callback for the commands event */
static int handle_command_queue_event(int mask, int fd, HANDLE handle, void *data) {
	OGON_UNUSED(fd);
	OGON_UNUSED(handle);
	ogon_connection *connection = (ogon_connection *) data;
	wMessage msg;

	if (!(mask & OGON_EVENTLOOP_READ)) {
		return -1;
	}

	while (MessageQueue_Peek(connection->commandQueue, &msg, TRUE)) {
		switch (msg.id) {
		case WMQ_QUIT:
			ogon_connection_close(connection);
			break;

		case NOTIFY_SWITCHTO:
			if (!process_switch_pipe(connection, &msg))	{
				WLog_ERR(TAG, "error processing pipe switch");
				ogon_connection_close(connection);
			}
			break;

		case NOTIFY_LOGOFF:
			if (!process_logoff(connection, &msg)) {
				WLog_ERR(TAG, "error processing logoff");
			}
			break;

		case NOTIFY_SBP_REPLY:
			if (!process_sbp_reply(connection, &msg)) {
				WLog_ERR(TAG, "error processing sbp reply");
				ogon_connection_close(connection);
			}
			break;

		case NOTIFY_VC_CONNECT:
			if (!process_vc_connect(connection, &msg)) {
				WLog_ERR(TAG, "error processing VC connect");
			}
			break;

		case NOTIFY_VC_DISCONNECT:
			if (!process_vc_disconnect(connection, &msg)) {
				WLog_ERR(TAG, "error processing VC disconnect");
			}
			break;

		case NOTIFY_USER_MESSAGE:
			if (!process_user_message(connection, &msg)) {
				WLog_ERR(TAG, "error processing message");
			}
			break;

		case NOTIFY_START_REMOTE_CONTROL:
			if (!process_start_remote_control(connection, &msg)) {
				WLog_ERR(TAG, "error processing remote control start");
			}
			break;

		case NOTIFY_NEW_SHADOWING_FRONTEND:
			if (!process_new_shadowing_frontend(connection, &msg)) {
				WLog_ERR(TAG, "error processing new shadowing frontend");
			}
			break;

		case NOTIFY_REWIRE_ORIGINAL_BACKEND:
			if (!process_rewire_original_backend(connection, &msg)) {
				WLog_ERR(TAG, "error rewiring original backend");
			}
			break;

		case NOTIFY_UNWIRE_SPY:
			if (!process_unwire_spy(connection, &msg)) {
				WLog_ERR(TAG, "error treating the unwire spy message");
			}
			break;

		case NOTIFY_STOP_SHADOWING:
			if (!process_stop_shadowing(connection, &msg)) {
				WLog_ERR(TAG, "error stopping shadowing");
			}
			break;

		default:
			WLog_ERR(TAG, "unhandled message type %"PRIu32"", msg.id);
			break;
		}
	}

	return 0;
}


/* RDPEPS related variables */
#define PRE_BLOB_TIMEOUT 10
static const unsigned char pre_blob_magic[] = {0xc0, 0xff, 0x33};

typedef enum {
	PRECONNECT_WAITING_MAGIC = 0,
	PRECONNECT_WAITING_BLOB_LENGTH,
	PRECONNECT_WAITING_BLOB_DATA,

	PRECONNECT_LAUNCH_CONNECTION = 100,
	PRECONNECT_ERROR = 101,
	PRECONNECT_TIMEOUT = 102,
	PRECONNECT_CLOSE = 103,
} ogon_preconnect_state;

typedef struct {
	volatile ogon_preconnect_state state;
	ogon_connection_runloop *runloop;
	wStream *inStream;
	int expectedBytes;
} ogon_preconnect_context;

static int ogon_read_bytes(int fd, unsigned char *data, unsigned int bytes) {
	int status = 0;
	do {
		status = read(fd, data, bytes);
	} while ((status < 0) && (errno == EINTR));
	return status;
};

static void ogon_handle_rdpeps(ogon_preconnect_context *context) {
	int status;
	DWORD flags, version, id;
	wStream *s;

	s = context->inStream;
	Stream_Read_UINT32(s, flags);
	Stream_Read_UINT32(s, version);
	Stream_Read_UINT32(s, id);

	WLog_DBG(TAG, "session information: flags %"PRIu32", version %"PRIu32", id %"PRIu32"", flags, version, id);

	context->state = PRECONNECT_LAUNCH_CONNECTION;
	switch (version) {
	case 0x01: {
		/* TYPE 1 RDP_PRECONNECTION_PDU */
		/* no extra data */
		break;
	}
	case 0x02: {
		/* TYPE 2 RDP_PRECONNECTION_PDU */
		DWORD cchPCB;
		/* also read the cchPCP size */
		if (Stream_GetRemainingLength(s) < 2) {
			WLog_ERR(TAG, "not enough bytes for type 2 preconnection PDU");
			context->state = PRECONNECT_ERROR;
			return;
		}
		Stream_Read_UINT16(s, cchPCB);
		WLog_DBG(TAG, "session information v2: size %"PRIu32"", cchPCB);
		break;
	}
	default:
		/* Unknown RDP_PRECONNECTION_PDU type */
		status = write(context->runloop->peer->sockfd, pre_blob_magic, sizeof(pre_blob_magic));
		if (status < 0) {
			WLog_ERR(TAG, "error writing back magic blob");
			context->state = PRECONNECT_ERROR;
		}
		break;
	}
}

static void pre_connect_timeout_handler(void *data) {
	ogon_preconnect_context *context = (ogon_preconnect_context *)data;
	context->state = PRECONNECT_TIMEOUT;
}

static int pre_connect_handler(int mask, int fd, HANDLE handle, void *data) {
	ogon_preconnect_context *context = (ogon_preconnect_context *)data;
	freerdp_peer *peer = context->runloop->peer;
	unsigned long bytes_available = 0;
	char pre_blob[sizeof(pre_blob_magic)];
	int status;

	OGON_UNUSED(mask);
	OGON_UNUSED(handle);
	OGON_UNUSED(fd);

	if (context->state == PRECONNECT_WAITING_MAGIC) {
		if (ioctl(peer->sockfd, FIONREAD, &bytes_available) < 0) {
			WLog_ERR(TAG, "error reading available bytes on the client socket");
			context->state = PRECONNECT_ERROR;
			return 0;
		}

		if (bytes_available < sizeof(pre_blob_magic)) {
			Sleep(100); /* to prevent busy waiting */
			return 0;
		}

		status = recv(peer->sockfd, pre_blob, sizeof(pre_blob_magic), MSG_PEEK);
		if ((status < 0) || (status != sizeof(pre_blob_magic))) {
			WLog_ERR(TAG, "error reading pre blob");
			context->state = PRECONNECT_ERROR;
			return 0;
		}

		if (memcmp(pre_blob, pre_blob_magic, sizeof(pre_blob_magic))) {
			/* not the preblob magic, we can go and proceed normally */
			context->state = PRECONNECT_LAUNCH_CONNECTION;
			return 0;
		}

		/* we have the magic bytes, let's read the RDPEPS blob */
		read(peer->sockfd, pre_blob, sizeof(pre_blob_magic)); /* skip the pre blob magic */

		context->inStream = Stream_New(NULL, 128);
		if (!context->inStream) {
			WLog_ERR(TAG, "error creating stream to read RDPEPS blob");
			context->state = PRECONNECT_ERROR;
			return 0;
		}

		context->expectedBytes = 4;
		context->state = PRECONNECT_WAITING_BLOB_LENGTH;
	} else if (context->state == PRECONNECT_WAITING_BLOB_LENGTH || context->state == PRECONNECT_WAITING_BLOB_DATA) {
		status = ogon_read_bytes(peer->sockfd, Stream_Pointer(context->inStream), context->expectedBytes);
		if (status < 0) {
			WLog_ERR(TAG, "error reading blob bytes");
			context->state = PRECONNECT_ERROR;
			return 0;
		}

		context->expectedBytes -= status;
		Stream_Seek(context->inStream, status);
		if (context->expectedBytes) {
			/* all bytes aren't here yet */
			return 0;
		}

		Stream_SealLength(context->inStream);
		Stream_SetPosition(context->inStream, 0);

		if (context->state == PRECONNECT_WAITING_BLOB_LENGTH) {
			Stream_Read_UINT32(context->inStream, context->expectedBytes);
			if (context->expectedBytes <= 4) {
				WLog_ERR(TAG, "announced blob length is too small (%d)", context->expectedBytes);
				context->state = PRECONNECT_ERROR;
				return 0;
			}

			context->expectedBytes -= 4;

			if (context->expectedBytes < 12) {
				/* we need at least 12 bytes */
				context->state = PRECONNECT_CLOSE;

				status = write(peer->sockfd, pre_blob_magic, sizeof(pre_blob_magic));
				if (status < 0) {
					WLog_ERR(TAG, "error when writing back the pre blob magic");
					context->state = PRECONNECT_ERROR;
				}

				return 0;
			}

			/* we check for a sane limit, to not crash by allocating too much memory */
			if (context->expectedBytes > 0x10000) {
				WLog_ERR(TAG, "announced blob length looks too big (%d)", context->expectedBytes);
				context->state = PRECONNECT_ERROR;
				return 0;
			}

			Stream_SetPosition(context->inStream, 0);
			if (!Stream_EnsureRemainingCapacity(context->inStream, context->expectedBytes)) {
				WLog_ERR(TAG, "unable to grow the instream to %d", context->expectedBytes);
				context->state = PRECONNECT_ERROR;
				return 0;
			}
			context->state = PRECONNECT_WAITING_BLOB_DATA;
		} else {
			/* got the PRECONNECT_WAITING_BLOB_DATA */
			ogon_handle_rdpeps(context);
		}
	}

	return 0;
}

void frontend_destroy(ogon_front_connection *front);

static void ogon_connection_free(freerdp_peer *peer, rdpContext *context) {
	OGON_UNUSED(peer);
	ogon_connection *conn;

	if (!context) {
		return;
	}
	conn = (ogon_connection*)context;

	frontend_destroy(&conn->front);

	if (conn->commandQueue) {
		wMessage msg;
		UINT32 tag = 0;
		int error = 0;
		while (MessageQueue_Peek(conn->commandQueue, &msg, TRUE)) {
			switch(msg.id) {
				case NOTIFY_SWITCHTO:
				{
					struct ogon_notification_switch *notification = (struct ogon_notification_switch *)msg.wParam;
					ogon_backend_props_free(&notification->props);
					tag = notification->tag;
				}
					break;
				case NOTIFY_LOGOFF:
				{
					struct ogon_notification_logoff *notification = (struct ogon_notification_logoff *)msg.wParam;
					tag = notification->tag;
				}
					break;
				case NOTIFY_SBP_REPLY:
					free(msg.wParam);
					continue;
				case NOTIFY_VC_CONNECT:
				{
					struct ogon_notification_vc_connect *notification = (struct ogon_notification_vc_connect *)msg.wParam;
					tag = notification->tag;
					free(notification->vcname);
				}
					break;
				case NOTIFY_VC_DISCONNECT:
				{
					struct ogon_notification_vc_disconnect *notification = (struct ogon_notification_vc_disconnect *)msg.wParam;
					tag = notification->tag;
					free(notification->vcname);
				}
					break;
				case NOTIFY_START_REMOTE_CONTROL:
				{
					struct ogon_notification_start_remote_control *notification = (struct ogon_notification_start_remote_control *) msg.wParam;
					tag = notification->tag;
				}
					break;

				case NOTIFY_UNWIRE_SPY:
				{
					struct rds_notification_stop_remote_control *notification = (struct rds_notification_stop_remote_control *) msg.lParam;
					if (!notification) {
						continue;
					}
					tag = notification->tag;
				}
					break;

				case NOTIFY_STOP_SHADOWING:
				{
					struct rds_notification_stop_remote_control *notification = (struct rds_notification_stop_remote_control *) msg.wParam;
					tag = notification->tag;
				}
					break;

				default:
					WLog_ERR(TAG, "unhandled type %"PRIu32"", msg.id);
			}
			free(msg.wParam);
			/* send back a failed (1) to session-manager */
			error = ogon_icp_sendResponse(tag, msg.id, 1, FALSE, NULL);
			if (error != 0)	{
				WLog_ERR(TAG, "error sending logging off response");
			}
		}
		MessageQueue_Free(conn->commandQueue);
		conn->commandQueue = NULL;
	}

	if (conn->stopEvent) {
		CloseHandle(conn->stopEvent);
		conn->stopEvent = NULL;
	}

	if (conn->runloop->evloop) {
		eventloop_destroy(&conn->runloop->evloop);
	}

	if (!conn->externalStop) {
		CloseHandle(conn->runloop->workThread);
		conn->runloop->workThread = INVALID_HANDLE_VALUE;
	}

	free(conn->runloop);
	conn->runloop = NULL;

	LinkedList_Free(conn->frontConnections);
	conn->frontConnections = NULL;
}

static BOOL socket_activate_keepalive(int sock, int idle, int maxpkt) {
	int yes = 1;

	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(int)) < 0) {
		return FALSE;
	}

	if (idle > 0) {
		if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &idle, sizeof(int)) < 0) {
			   return FALSE;
		}
	}

	if (maxpkt > 0) {
		if (setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &maxpkt, sizeof(int)) < 0) {
			   return FALSE;
		}
	}
	return TRUE;
}

static int *connection_thread(void *args) {
	ogon_connection *connection;
	ogon_event_source *pre_eventsource, *timeout_eventsource;
	ogon_preconnect_context preconnect_context;
	ogon_connection_runloop *runloop = (ogon_connection_runloop *)args;
	freerdp_peer *peer = runloop->peer;
	char *keepaliveParams = NULL;
	int idle, maxPkt;

	/* ================================================================================
	 *
	 * first we're gonna deploy a minimal env to read the preconnection packet (if any),
	 * we just deploy a timer and some event handler that scan the first bytes for
	 * the magic packet.
	 */
	pre_eventsource = eventloop_add_fd(runloop->evloop, OGON_EVENTLOOP_READ,
			peer->sockfd, pre_connect_handler, &preconnect_context);
	if (!pre_eventsource) {
		WLog_ERR(TAG, "unable to add the peer in the eventloop");
		close(peer->sockfd);
		goto error_event_source;
	}

	timeout_eventsource = eventloop_add_timer(runloop->evloop, PRECONNECT_TIMEOUT * 1000,
			pre_connect_timeout_handler, &preconnect_context);
	if (!timeout_eventsource) {
		WLog_ERR(TAG, "unable to create timeout timer");
		close(peer->sockfd);
		goto error_set_timer;
	}

	ZeroMemory(&preconnect_context, sizeof(preconnect_context));
	preconnect_context.state = PRECONNECT_WAITING_MAGIC;
	preconnect_context.runloop = runloop;

	while (preconnect_context.state < PRECONNECT_LAUNCH_CONNECTION) {
		eventloop_dispatch_loop(runloop->evloop, 1 * 1000);
	}

	/* cleanup items from the preconnect phase */
	eventloop_remove_source(&timeout_eventsource);
	eventloop_remove_source(&pre_eventsource);
	if (preconnect_context.inStream) {
		Stream_Free(preconnect_context.inStream, TRUE);
	}

	if (preconnect_context.state != PRECONNECT_LAUNCH_CONNECTION) {
		close(peer->sockfd);
		goto error_event_source;
	}


	/**
	 *  When here we have passed the first step, we can start the connection, we deploy
	 *  all FreeRDP related items and do the traditional event loop
	 */

	if (!(connection = ogon_connection_create(runloop))) {
		WLog_ERR(TAG, "unable to create the peer connection");
		close(peer->sockfd);
		goto error_event_source;
	}

	idle = -1;
	maxPkt = -1;
	do {
		char *comaPos;
		if ((ogon_icp_get_property_string(connection->id, "tcp.keepalive.params", &keepaliveParams) != PBRPC_SUCCESS) ||
				!keepaliveParams) {
			break;
		}

		comaPos = strchr(keepaliveParams, ',');
		if (comaPos) {
			*comaPos = '\0';
			comaPos++;
		}
		idle = atoi(keepaliveParams);
		if (!idle) {
			idle = -1;
		}

		if (!comaPos) {
			break;
		}

		maxPkt = atoi(comaPos);
		if(!maxPkt) {
			maxPkt = -1;
		}
	} while(0);
	free(keepaliveParams);

	if (!socket_activate_keepalive(peer->sockfd, idle, maxPkt)) {
		WLog_ERR(TAG, "unable to activate TCP keepalive on the socket");
	}


	connection->runThread = TRUE;
	while (connection->runThread) {
		eventloop_dispatch_loop(connection->runloop->evloop, 10 * 1000);
	}

	if (connection->backend) {
		backend_destroy(&connection->backend);
	}

	peer = connection->context.peer;
	if (peer) {
		peer->Close(peer);
	}

	if (connection->sendDisconnect) {
		ogon_icp_DisconnectUserSession_async(connection->id);
	}
	app_context_remove_connection(connection->id);

	if (peer) {
		/**
		 * As we need to wait for some time (see below) but the connection
		 * is already closed, from the rdp-server point of view, free the command queue
		 * and do some clean up. To make sure this isn't done twice when the
		 * freerdp context is finally freed set the callback to NULL.
		 */
		peer->ContextFree = NULL;
		ogon_connection_free(peer, peer->context);

		/**
		 * Fix for Microsoft ios and andriod based clients
		 * this gives the time, that the client can close the connection
		 * otherwise the andriod and ios client will show an error message
		 */
		Sleep(1000);
		peer->Disconnect(peer);
		freerdp_peer_context_free(peer);
		freerdp_peer_free(peer);
	}
	winpr_CleanupSSL(WINPR_SSL_CLEANUP_THREAD);

	return 0;

error_set_timer:
	eventloop_remove_source(&pre_eventsource);
error_event_source:
	eventloop_destroy(&runloop->evloop);
	CloseHandle(runloop->workThread);
	freerdp_peer_context_free(peer);
	freerdp_peer_free(peer);
	free(runloop);
	return 0;
}

ogon_connection_runloop *ogon_runloop_new(freerdp_peer *peer) {
	ogon_connection_runloop *ret;

	ret = (ogon_connection_runloop *)calloc(1, sizeof(*ret));
	if (!ret) {
		return NULL;
	}

	if (!(ret->evloop = eventloop_create())) {
		WLog_ERR(TAG, "unable to create the client eventloop");
		goto error_eventloop;
	}

	ret->peer = peer;
	ret->workThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)connection_thread, ret, 0, NULL);
	if (!ret->workThread) {
		WLog_ERR(TAG, "unable to create client connection thread");
		goto error_thread;
	}

	return ret;

error_thread:
	eventloop_destroy(&ret->evloop);
error_eventloop:
	free(ret);
	return NULL;
}

static BOOL ogon_connection_new(freerdp_peer *peer, rdpContext *context) {
	ogon_connection *conn = (ogon_connection*) context;
	ogon_front_connection *front;

	conn->id = app_context_get_connectionid();
	conn->stopEvent = NULL;
	conn->fps = 20;
	conn->sendDisconnect = TRUE;
	conn->shadowing = conn;
	conn->runloop = (ogon_connection_runloop *)peer->ContextExtra;

	/* stop event */
	if (!(conn->stopEvent = CreateEventA(NULL, TRUE, FALSE, NULL))) {
		WLog_ERR(TAG, "error creating create stop event");
		goto out_evloop;
	}
	if (!eventloop_add_handle(conn->runloop->evloop, OGON_EVENTLOOP_READ, conn->stopEvent,
		handle_stop_event, conn))
	{
		WLog_ERR(TAG, "error adding stop event handle to event loop");
		goto out_evloop;
	}

	/* command queue */
	if (!(conn->commandQueue = MessageQueue_New(NULL))) {
		WLog_ERR(TAG, "error creating command message queue");
		goto out_stopEvent;
	}
	if (!eventloop_add_handle(conn->runloop->evloop, OGON_EVENTLOOP_READ, MessageQueue_Event(conn->commandQueue),
		handle_command_queue_event, conn))
	{
		WLog_ERR(TAG, "error adding command queue handle to event loop");
		goto out_commandQueue;
	}

	/* wire the front RDP client */
	if (!ogon_connection_init_front(conn)) {
		WLog_ERR(TAG, "error initializing front connection");
		goto out_commandQueue;
	}
	front = &conn->front;
	front->rdpEventSource = eventloop_add_handle(conn->runloop->evloop, OGON_EVENTLOOP_READ,
			peer->GetEventHandle(peer),	handle_front_rdp_event, conn);
	if (!front->rdpEventSource) {
		WLog_ERR(TAG, "error adding peer event handle to event loop");
		goto out_commandQueue;
	}

	if (!app_context_add_connection(conn)) {
		WLog_ERR(TAG, "error adding connection to app context list");
		goto out_eventSource;
	}

	return TRUE;

out_eventSource:
	eventloop_remove_source(&front->rdpEventSource);
out_commandQueue:
	MessageQueue_Free(conn->commandQueue);
	conn->commandQueue = 0;
out_stopEvent:
	CloseHandle(conn->stopEvent);
	conn->stopEvent = NULL;
out_evloop:
	eventloop_destroy(&conn->runloop->evloop);
//out_error:
	peer->Close(peer);
	WLog_ERR(TAG, "%s() failed", __FUNCTION__);
	return FALSE;
}


void ogon_backend_props_free(ogon_backend_props *props) {
	free(props->serviceEndpoint);
	free(props->ogonCookie);
	free(props->backendCookie);

	ZeroMemory(props, sizeof(*props));
}

ogon_connection *ogon_connection_create(ogon_connection_runloop *runloop) {
	freerdp_peer *peer = runloop->peer;

	peer->ContextSize = sizeof(ogon_connection);
	peer->ContextNew = ogon_connection_new;
	peer->ContextFree = ogon_connection_free;
	peer->ContextExtra = runloop;

	if (!freerdp_peer_context_new(peer)) {
		WLog_ERR(TAG, "freerdp_peer_context_new failed");
		return NULL;
	}

	return (ogon_connection *)peer->context;
}

BOOL ogon_post_exit_shadow_notification(ogon_connection *conn, wMessage *msg, BOOL rewire) {
	struct ogon_notification_rewire_backend *notif;
	UINT32 spiedId = conn->shadowing->id;
	struct rds_notification_stop_remote_control *notification;
	BOOL returnValue = FALSE;

	notif = (struct ogon_notification_rewire_backend *)malloc(sizeof(*notif) );
	if (!notif) {
		goto out;
	}

	ogon_state_set_event(conn->front.state, OGON_EVENT_BACKEND_TRIGGER_REWIRE);
	notif->rdpFd = eventsource_fd(conn->front.rdpEventSource);
	notif->rdpMask = eventsource_mask(conn->front.rdpEventSource);
	notif->rewire = rewire;
	eventloop_remove_source(&conn->front.rdpEventSource);

	if (!app_context_post_message_connection(conn->id, NOTIFY_REWIRE_ORIGINAL_BACKEND, notif, NULL)) {
		WLog_ERR(TAG, "error posting notification to %ld", conn->id);
		free(notif);
		goto out;
	}

	returnValue = TRUE;
out:

	if (msg != 0 && msg->lParam != 0) {
		notification = (struct rds_notification_stop_remote_control *)msg->lParam;
		ogon_icp_sendResponse(notification->tag, NOTIFY_STOP_SHADOWING, 0, returnValue, NULL);
		free(notification);
	} else {
		if (returnValue && ogon_icp_RemoteControlEnded(conn->id, spiedId) != PBRPC_SUCCESS) {
			WLog_ERR(TAG, "error notifying the end of shadowing");
		}
	}

	return returnValue;
}

void ogon_connection_close(ogon_connection *conn) {
	ogon_front_connection *front = &conn->front;

	/*WLog_DBG(TAG, "treating connection close for %ld", conn->id);*/
	if (conn->shadowing != conn) {
		/* if the connection is actually shadowing we want it to die in its own eventloop.
		 * So let's post an event that will re implant the connection in its eventloop, and
		 * will kill it there.
		 */
		if (!LinkedList_Remove(conn->shadowing->frontConnections, conn))
			WLog_ERR(TAG, "conn %p not found in frontConnections", conn);

		conn->shadowing->backend->client.SeatRemoved(conn->shadowing->backend, conn->id); /* we don't care about the result */

		conn->shadowing = conn;
		if (!ogon_post_exit_shadow_notification(conn, NULL, FALSE)) {
			WLog_ERR(TAG, "unable to post a 'rewire backend notification' to connection %ld", conn->id);
		}
	} else {
		/* this connection is not shadowing, but may be spied. So we still have to rewire
		 * front connections (spies) to their initial eventloop. This is just like if
		 *  WTSStopRemoteControlSession() was called for all spying sessions. */
		conn->runThread = FALSE;

		LinkedList_Enumerator_Reset(conn->frontConnections);
		while (LinkedList_Enumerator_MoveNext(conn->frontConnections)) {
			ogon_connection *c = (ogon_connection *)LinkedList_Enumerator_Current(conn->frontConnections);

			if (c == conn) /* don't post to ourself */
				continue;

			if (!ogon_post_exit_shadow_notification(c, NULL, TRUE)) {
				WLog_ERR(TAG, "unable to post a 'rewire backend notification' to spying connection %ld", c->id);
			}
		}

		/* drop all front connections (including self) */
		LinkedList_Clear(conn->frontConnections);
	}

	if (front->rdpEventSource) {
		eventloop_remove_source(&front->rdpEventSource);
	}

	if (front->frameEventSource) {
		eventloop_remove_source(&front->frameEventSource);
	}
}
