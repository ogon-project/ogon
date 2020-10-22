/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Frontend
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <unistd.h>

#include <winpr/path.h>
#include <winpr/input.h>
#include <winpr/sysinfo.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/crypto/crypto.h>
#include <freerdp/pointer.h>
#include <freerdp/channels/rdpgfx.h>

#include <ogon/dmgbuf.h>

#include "icp/icp_client_stubs.h"
#include "icp/pbrpc/pbrpc.h"
#include "../common/global.h"

#include "peer.h"
#include "channels.h"
#include "encoder.h"
#include "eventloop.h"
#include "backend.h"
#include "app_context.h"
#include "bandwidth_mgmt.h"

#define TAG OGON_TAG("core.frontend")
#if OPENSSL_VERSION_NUMBER < 0x10100000L
void RSA_get0_key(const RSA *r, const BIGNUM **n, const BIGNUM **e, const BIGNUM **d)
{
	if (n != NULL)
		*n = r->n;
	if (e != NULL)
		*e = r->e;
	if (d != NULL)
		*d = r->d;
}
#endif


static rdpRsaKey* ogon_generate_weak_rsa_key() {
	BOOL success = FALSE;
	rdpRsaKey* key = NULL;
	RSA* rsa = NULL;
	BIGNUM *e = NULL;
	const BIGNUM *rsa_e = NULL;
	const BIGNUM *rsa_n = NULL;
	const BIGNUM *rsa_d = NULL;

	if (!(key = (rdpRsaKey *)calloc(1, sizeof(rdpRsaKey)))) {
		goto out;
	}
	if (!(e = BN_new())) {
		goto out;
	}
	if (!(rsa = RSA_new())) {
		goto out;
	}
	if (!BN_set_word(e, 0x10001) || !RSA_generate_key_ex(rsa, 512, e, NULL)) {
		goto out;
	}
	RSA_get0_key(rsa, &rsa_n, NULL, NULL);
	key->ModulusLength = BN_num_bytes(rsa_n);
	if (!(key->Modulus = (BYTE *)malloc(key->ModulusLength))) {
		goto out;
	}
	BN_bn2bin(rsa_n, key->Modulus);
	crypto_reverse(key->Modulus, key->ModulusLength);

	RSA_get0_key(rsa, NULL, NULL, &rsa_d);
	key->PrivateExponentLength = BN_num_bytes(rsa_d);
	if (!(key->PrivateExponent = (BYTE *)malloc(key->PrivateExponentLength))) {
		goto out;
	}
	BN_bn2bin(rsa_d, key->PrivateExponent);
	crypto_reverse(key->PrivateExponent, key->PrivateExponentLength);

	RSA_get0_key(rsa, NULL, &rsa_e, NULL);
	memset(key->exponent, 0, sizeof(key->exponent));
	BN_bn2bin(rsa_e, key->exponent + sizeof(key->exponent) - BN_num_bytes(rsa_e));
	crypto_reverse(key->exponent, sizeof(key->exponent));

	success = TRUE;

out:
	if (rsa) {
		RSA_free(rsa);
	}
	if (e) {
		BN_free(e);
	}
	if (!success) {
		if (key) {
			free(key->Modulus);
			free(key->PrivateExponent);
			free(key);
		}
		return NULL;
	}
	return key;
}

static int ogon_generate_certificate(ogon_connection *conn, const char *cert_file, const char *key_file) {
	rdpSettings *settings = conn->context.settings;

	settings->CertificateFile = strdup(cert_file);
	settings->PrivateKeyFile = strdup(key_file);
	settings->RdpKeyFile = strdup(settings->PrivateKeyFile);

	if (!settings->CertificateFile || !settings->PrivateKeyFile || !settings->RdpKeyFile) {
		goto out_fail;
	}

	return 0;

out_fail:
	free(settings->CertificateFile);
	settings->CertificateFile = NULL;
	free(settings->PrivateKeyFile);
	settings->PrivateKeyFile = NULL;
	return -1;
}

void handle_wait_timer_state(ogon_connection *conn) {
	ogon_front_connection *front = &conn->front;
	ogon_backend_connection *backend;

	if (!ogon_state_should_create_frame(front->state)){
		return;
	}

	backend = conn->shadowing->backend;
	if (!backend) {
		return;
	}

	ogon_state_set_event(front->state, OGON_EVENT_BACKEND_SYNC_REQUESTED);

	if (ogon_state_get(front->state) == OGON_STATE_WAITING_SYNC_REPLY) {
		/*
		 * In case of shadowing it can happen that a slower client was unable to handle
		 * one of the previous sync replies received (e.g. if sending takes longer of if
		 * it is waited for a frame ack). In these situations the accumulatedDamage are not empty and
		 * should be send out. In order to keep the state machine intact send out an immediate request
		 * and handle the case when the reply is received.
		 */
		if (!region16_is_empty(&front->encoder->accumulatedDamage)) {
			initiate_immediate_request(conn, &conn->front, FALSE);
			return;
		}

		if (!backend->waitingSyncReply) {
			if (!backend->client.FramebufferSyncRequest(backend, ogon_dmgbuf_get_id(backend->damage)))	{
				WLog_ERR(TAG, "error sending framebuffer sync request");
				ogon_connection_close(conn);
			}
			backend->waitingSyncReply = TRUE;
		}
	}
}


int frontend_handle_frame_sent(ogon_connection *conn) {
	ogon_front_connection *front = &conn->front;

	if (ogon_state_get(front->state) != OGON_STATE_WAITING_FRAME_SENT) {
		return -1;
	}

	if (front->frameAcknowledge) {
		if (front->lastAckFrame + front->frameAcknowledge + 1 < front->nextFrameId) {
			/* WLog_DBG(TAG, "waiting frame ack(frontLast=%"PRIu32" current=%"PRIu32")", front->lastAckFrame,
							front->nextFrameId); */
			ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_FRAME_ACK_SEND);
			return 0;
		}
	}

	ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_FRAME_SENT);
	handle_wait_timer_state(conn);
	return 0;
}

int ogon_backend_consume_damage(ogon_connection *conn);

int frontend_handle_sync_reply(ogon_connection *conn) {

	conn->shadowing->backend->waitingSyncReply = FALSE;

	LinkedList_Enumerator_Reset(conn->frontConnections);
	while (LinkedList_Enumerator_MoveNext(conn->frontConnections)) {
		ogon_connection *c = LinkedList_Enumerator_Current(conn->frontConnections);
		ogon_front_connection *front = &c->front;
		ogon_bitmap_encoder *encoder = front->encoder;

		if (ogon_backend_consume_damage(c) < 0) {
			WLog_ERR(TAG, "error when treating backend damage for connection %ld", c->id);
			return -1;
		}

		/* Don't handle the reply if we don't expecting one */
		if (ogon_state_get(front->state) != OGON_STATE_WAITING_SYNC_REPLY){
			continue;
		}

		ogon_state_set_event(front->state, OGON_EVENT_BACKEND_SYNC_REPLY_RECEIVED);

		if (!encoder) {
			WLog_ERR(TAG, "no encoder, perhaps i should die ?");
			return -1;
		}

		if (ogon_state_get(front->state) == OGON_STATE_WAITING_ACTIVE_OUTPUT){
			continue;
		}

		if (ogon_send_surface_bits(c) < 0) {
			WLog_ERR(TAG, "error sending surface bits");
			return -1;
		}
	}

	/**
	 * Note: it's intentional to split the treatment in 2 loops, because we want to keep
	 * 		backend's damage data coherent for all front connections.
	 * 		A call to frontend_handle_frame_sent() may send a SYNC_REQUEST that
	 * 		would modify the shared frame buffer and damage data in our back.
	 */
	LinkedList_Enumerator_Reset(conn->frontConnections);
	while (LinkedList_Enumerator_MoveNext(conn->frontConnections)) {
		ogon_connection *c = LinkedList_Enumerator_Current(conn->frontConnections);
		freerdp_peer *peer = c->context.peer;
		ogon_front_connection *front = &c->front;

		if (!peer->IsWriteBlocked(peer)) {
		   frontend_handle_frame_sent(c);
		   continue;
		}

		/* frame has been blocked in the output buffer, let's monitor write availability
		 * of the front socket */
		/* WLog_DBG(TAG, "scanning for write for %ld", c->id); */
		if (!eventsource_change_source(front->rdpEventSource, OGON_EVENTLOOP_READ | OGON_EVENTLOOP_WRITE)) {
			WLog_ERR(TAG, "error activating write select() on rdpEventSource for connection %ld", c->id);
			continue;
		}
	}

	return 0;
}

static inline void handle_progressive_updates(ogon_connection *conn) {
	ogon_front_connection *front = &conn->front;

	if (front->codecMode != CODEC_MODE_H264) {
		return;
	}
	if (front->rdpgfxProgressiveTicks == 0) {
		return;
	}
	/* start improving quality after 0.25 seconds */
	if (front->rdpgfxProgressiveTicks++ < (UINT32)(conn->fps / 4)) {
		return;
	}
	/* upper limit is 10 seconds */
	if (front->rdpgfxProgressiveTicks > (UINT32)(10 * conn->fps)) {
		front->rdpgfxProgressiveTicks = 0;
		return;
	}
	if (ogon_state_get(front->state) != OGON_STATE_WAITING_SYNC_REPLY) {
		return;
	}
	/*WLog_DBG(TAG, "initiating immediate request (rdpgfxProgressiveTicks = %"PRIu32")", front->rdpgfxProgressiveTicks);*/
	initiate_immediate_request(conn, front, FALSE);
}

static int handle_frame_timer_event(int mask, int fd, HANDLE handle, void *data) {
	OGON_UNUSED(handle);
	int ret;
	UINT64 expirations;
	ogon_connection *conn = (ogon_connection *)data;

	/*WLog_DBG(TAG, "(%p)", conn);*/
	if (!(mask & OGON_EVENTLOOP_READ))
		return 0;

	/* drain timerfd */
	do {
		ret = read(fd, &expirations, sizeof(expirations));
	} while (ret < 0 && errno == EINTR);

	if (ret < 0) {
		WLog_ERR(TAG, "error draining timerfd, error=%s(%d)", strerror(errno), errno);
		return 0;
	}
	LinkedList_Enumerator_Reset(conn->frontConnections);
	while (LinkedList_Enumerator_MoveNext(conn->frontConnections)) {
		ogon_connection *c = (ogon_connection *)LinkedList_Enumerator_Current(conn->frontConnections);
		ogon_front_connection *front = &c->front;
		ogon_statistics *stats = &conn->front.statistics;

		UINT32 current_time = GetTickCount();
		BOOL bandwidthExceeded = FALSE;

		if (stats->fps_measure_timestamp + 1000 < current_time) {
			stats->fps_measure_timestamp = current_time;
			stats->fps_measured = stats->fps_measure_currentfps;
			stats->fps_measure_currentfps = 0;
		}

		if (stats->bytes_sent_timestamp + 1000 < current_time) {
			stats->bytes_sent_timestamp = current_time;
			stats->bytes_sent = stats->bytes_sent_current;
			stats->bytes_sent_current = 0;
		}

		ogon_state_set_event(front->state, OGON_EVENT_FRAME_TIMER);

		if (front->codecMode == CODEC_MODE_H264) {
			ogon_bwmgmt_update_data_usage(c);

			if (ogon_bwmgmt_update_bucket(c) == 0) {
				/* no space in the current bucket */
				bandwidthExceeded = TRUE;
			}
		}

		if (!bandwidthExceeded) {
			ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_BANDWIDTH_GOOD);
			handle_progressive_updates(c);
			ogon_bwmgmt_client_detect_rtt(c);
			handle_wait_timer_state(c);
		} else {
			ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_BANDWIDTH_FAIL);
		}
	}

	return 0;
}

BOOL ogon_frontend_install_frame_timer(ogon_connection *conn) {
	ogon_front_connection *front = &conn->front;

	front->frameEventSource = eventloop_add_handle(conn->runloop->evloop, OGON_EVENTLOOP_READ, front->frameTimer,
												handle_frame_timer_event, conn);
	if (!front->frameEventSource) {
		WLog_ERR(TAG, "unable to add frame timer in eventloop");
		return FALSE;
	}
	return TRUE;
}

static BOOL ogon_peer_capabilities(freerdp_peer *client) {
	OGON_UNUSED(client);
	return TRUE;
}

static BOOL ogon_peer_post_connect(freerdp_peer *client)
{
	rdpSettings* settings = client->settings;
	ogon_connection *conn = (ogon_connection *)client->context;
	ogon_front_connection *front = &conn->front;
	int error_code;

	WLog_DBG(TAG, "connection id %ld client hostname=[%s]", conn->id, client->hostname);

	if (front->backendProps.serviceEndpoint) {
		WLog_ERR(TAG, "error, service endpoint MUST be NULL in post connect");
		return FALSE;
	}

	if (front->activationCount) {
		WLog_ERR(TAG, "error, activation count must be 0 in post connect");
		return FALSE;
	}

	if (client->settings->AutoLogonEnabled)	{
		WLog_DBG(TAG, "autologon enabled, user=[%s] domain=[%s]",
			client->settings->Username, client->settings->Domain);
	}

	WLog_DBG(TAG, "requested desktop: %"PRIu32"x%"PRIu32"@%"PRIu32"bpp", settings->DesktopWidth,
			 settings->DesktopHeight, settings->ColorDepth);

	/**
	 * Note regarding some ogon_icp_LogonUser parameters:
	 * clientProductId:
	 * This is the WTSClientProductId value the session manager will return in
	 * in WTS_INFO_CLASS enumerations. This should be set to the clientProductId
	 * value receiced in the GCC client core data (see MS-RDPBCGR 2.2.1.3.2).
	 * However, FreeRDP's settings->clientProductId currently incorrectly stores the
	 * char[64] clientDigProductId string and does not parse clientProductId from
	 * the wire. Thus we corrently hardcode this value to 1 because the docs say
	 * that this value SHOULD be initialized to 1.
	 * hardwareID:
	 * This is the WTSClientHardwareId value the session manager will return in
	 * in WTS_INFO_CLASS enumerations. Microsoft's WTS API states that this value
	 * is reserved for future use and that it will always return a value of 0.
	 */

	error_code = ogon_icp_LogonUser((UINT32)(conn->id),
			settings->Username, settings->Domain, settings->Password,
			settings->ClientHostname, settings->ClientAddress,
			settings->ClientBuild,
			1, /* clientProductId not parsed by FreeRDP currently */
			0, /* WTSClientHardwareId: always 0, reserved for future use */
			WTS_PROTOCOL_TYPE_RDP,
			settings->DesktopWidth, settings->DesktopHeight, settings->ColorDepth,
			&front->backendProps,
			&front->maxWidth, &front->maxHeight);

	if (error_code != PBRPC_SUCCESS) {
		WLog_ERR(TAG, "logon user call failed with error %d", error_code);
		return FALSE;
	}

	WLog_DBG(TAG, "logon user call successful, service endpoint = [%s]", front->backendProps.serviceEndpoint);

	return TRUE;
}

static void ogon_select_codec_mode(ogon_connection *conn)
{
	rdpSettings *settings = conn->context.settings;
	ogon_front_connection *front = &conn->front;

	WLog_DBG(TAG, "choosing codec mode for connection %ld: %"PRIu32"x%"PRIu32" bpp=%"PRIu32" ConnectionType=%"PRIu32" FrameAcknowledge=%"PRIu32" SurfaceFrameMarkerEnabled=%"PRId32" SupportGraphicsPipeline=%"PRId32" RemoteFX=%"PRId32"",
			conn->id, settings->DesktopWidth, settings->DesktopHeight, settings->ColorDepth,
			settings->ConnectionType, settings->FrameAcknowledge, settings->SurfaceFrameMarkerEnabled,
			settings->SupportGraphicsPipeline, settings->RemoteFxCodec);

	front->codecMode = CODEC_MODE_BMP;
	front->rdpgfxRequired = FALSE;
	front->rdpgfxConnected = FALSE;
	front->rdpgfxH264Supported = FALSE;
	front->rdpgfxProgressiveTicks = 0;
	front->frameAcknowledge = 0;

	/* Currently we only allow the more sophisticated codecs if the client's network
	 * connection type is set to LAN or higher (this also includes the auto detection
	 * type which is not implemented yet)
	 */

	if (settings->ConnectionType >= CONNECTION_TYPE_LAN && settings->ColorDepth == 32) {
		BOOL supportGraphicsPipeline = FALSE;
		BOOL supportRemoteFxSurfaceCommand = FALSE;

		if (!front->rdpgfxForbidden && settings->SupportGraphicsPipeline &&
			WTSIsChannelJoinedByName(front->vcm->client, "drdynvc"))
		{
			supportGraphicsPipeline = TRUE;
		}

		if (settings->RemoteFxCodec && settings->SurfaceCommandsEnabled &&
			settings->SurfaceFrameMarkerEnabled)
		{
			/* FreeRDP stores the maxUnacknowledgedFrameCount value of the client's
			 * transmitted TS_FRAME_ACKNOWLEDGE_CAPABILITYSET in settings->FrameAcknowledge
			 * According to the spec: "... the client MAY set this field to 0, but this
			 * behaviour should be avoided because it provides very little information
			 * to the server other than that the client acknowledges frames."
			 * In order to know if the client acknowledges frames we have to check if the
			 * CAPSET_TYPE_FRAME_ACKNOWLEDGE (0x001E) was transmitted!
			 */
			if (settings->ReceivedCapabilities[0x001E]) {
				supportRemoteFxSurfaceCommand = TRUE;
			}
		}

		/* Note:
		 * If settings->RemoteFX is true this means that the client accepts bitmap data
		 * compressed using the legacy rfx codec ([MS-RDPRFX] sections 2.2.1 and 3.1.8)
		 * However, this does not necessarily mean that it can be sent within a surface
		 * command. If settings->SupportGraphicsPipeline is true the clients may refuse
		 * to display rfx sent in the surface command, thus we always must send it over
		 * the graphics virtual channel in this case.
		 * If settings->SupportGraphicsPipeline is true but settings->RemoteFX is false
		 * we must use the the RemoteFX progressive codec instead.
		 */

		if (settings->RemoteFxCodec) {
			if (supportGraphicsPipeline) {
				front->codecMode = CODEC_MODE_RFX2; /* remotefx in gfx */
				front->rdpgfxRequired = TRUE;
			} else if (supportRemoteFxSurfaceCommand) {
				front->codecMode = CODEC_MODE_RFX1; /* remotefx surface cmd */
			} else {
				WLog_ERR(TAG, "weird: RemoteFX is enabled but no suitable transport was found");
			}
		} else if (supportGraphicsPipeline) {
			front->codecMode = CODEC_MODE_RFX3; /* remotefx progressive support is mandatory in gfx */
			front->rdpgfxRequired = TRUE;
		}
	}

	if (front->codecMode == CODEC_MODE_RFX1 || front->rdpgfxRequired) {
		front->frameAcknowledge = settings->FrameAcknowledge;
		if (front->frameAcknowledge == 0) {
			front->frameAcknowledge = 5;
		}
	}

	WLog_DBG(TAG, "will use codec mode %d with frameAcknowledge=%"PRIu32"", front->codecMode, front->frameAcknowledge);
}

static void ogon_init_output(ogon_connection *conn) {
	ogon_front_connection *front = &conn->front;

	WLog_DBG(TAG, "%s: rdpgfxRequired=%"PRId32" drdynvc_state=%"PRIu8"", __FUNCTION__,
			 front->rdpgfxRequired, front->vcm->drdynvc_state);

	/*
	 * Some clients (including mstsc) that have sent a suppress output enable
	 * forget to disable suppress output after a reactivation if they were e.g
	 * minimized when a resize happened. Thus we always enable outout on
	 * reactivation, otherwise the buggy client would end up with a blank screen
	 * until they do another suppress/unsuppress sequence
	 */

	ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_ENABLE_OUTPUT);

	if (!front->rdpgfxRequired) {
		ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_STOP_WAITING_GFX);

		if (ogon_state_get(conn->front.state) == OGON_STATE_WAITING_TIMER) {
			initiate_immediate_request(conn->shadowing, front, TRUE);
			handle_wait_timer_state(conn);
		}
		return;
	}

	ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_WAITING_GFX);

	/*
	 * The graphics pipeline extension is required.
	 * Depending on the current channel manager dynamic channel state we have
	 * to open the channel now unless we know that this job will be done by the
	 * channel manager's drdynvc state callback.
	 */

	if (front->vcm->drdynvc_state == DRDYNVC_STATE_READY) {
		WLog_DBG(TAG, "opening internal dynamic graphics channel");
		IFCALL(front->rdpgfx->Open, front->rdpgfx);
	}
}

static BOOL ogon_peer_activate(freerdp_peer *client) {
	rdpSettings* settings = client->settings;
	ogon_connection *conn = (ogon_connection *)client->context;
	ogon_front_connection *front = &conn->front;
	LARGE_INTEGER due;
	BOOL resizeClient = FALSE;

	/**
	 * Note:
	 * Don't change any settings->xxxx values unless you know what you're doing.
	 * Such modifications might change the client behaviour on reavtivation!
	 */

	WLog_DBG(TAG, "------------------------------------------------------------");
	WLog_DBG(TAG, "Connection id %ld performing activation #%"PRIu32" (%s backend)",
			 conn->id, front->activationCount,
			 conn->backend ? "existing" : "no");

	front->activationCount++;

	/*
	 * In theory the graphics pipeline channel could stay open all the time and
	 * we could use the egfx reset graphics pdu for resizes insted of the damn
	 * deactivate/reactivate crutch but the Microsoft clients react extremely
	 * buggy with that and xfreerdp ignores that pdu completely.
	 * To test if it gets better in the future connect with all available clients
	 * to a Win 8.1 RHDVH hosted on 2012/R2 HyperV with 800x600 and start the
	 * xmoto game with a config file set to 1024x768 fullscreen in order to
	 * force the server to do a session resize using the gfx reset graphics pdu.
	 * Because the Microsoft clients always terminate with a protocol error if
	 * the gfx channel is not recreated after a reactivation we always have to
	 * close the channel here and we'll open it again in ogon_init_output()
	 * if front->rdpgfxRequired is set by ogon_select_codec_mode().
	 */

	ogon_rdpgfx_shutdown(conn);

	ogon_select_codec_mode(conn);

	if (conn->backend) {
		/*
		 * a reactivation sequence, most probably a resize
		 */

		ogon_bitmap_encoder* encoder = front->encoder;
		ogon_backend_connection *backend;
		rdpPointerUpdate *pointer = client->update->pointer;
		POINTER_SYSTEM_UPDATE systemPointer = { 0 };

		backend = conn->shadowing->backend;

		/* We don't support (yet) changes of color depth in reactivations if the encoder
		 * was already created. If that is the case we have to bail out currently.
		 */
		if (encoder) {
			if (encoder->dstBitsPerPixel != settings->ColorDepth) {
				WLog_ERR(TAG, "reactivation with new color depth is not supported");
				return FALSE;
			}

			if (encoder->multifragMaxRequestSize != settings->MultifragMaxRequestSize) {
				if (!ogon_bitmap_encoder_update_maxrequest_size(front->encoder, settings->MultifragMaxRequestSize)) {
					WLog_ERR(TAG, "failed to update encoder multifragMaxRequestSize");
					return FALSE;
				}
			}
		}

		/* If a resize was finished check if there is another resize pending */
		if (ogon_state_get(front->state) == OGON_STATE_WAITING_RESIZE) {
			if (front->pendingResizeWidth || front->pendingResizeHeight) {
				if ((front->pendingResizeWidth != settings->DesktopWidth)
				                 || (front->pendingResizeHeight != settings->DesktopHeight)) {
					/* we still need to do a reactivation sequence to the new size */
					settings->DesktopWidth = front->pendingResizeWidth;
					settings->DesktopHeight = front->pendingResizeHeight;
					front->pendingResizeHeight = 0;
					front->pendingResizeWidth = 0;
					client->update->DesktopResize(client->context);
					return TRUE;
				}
			}
			ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_RESIZED);
		}

		/* restore the kind and shape of pointer */
		systemPointer.type = backend->lastSetSystemPointer;
		IFCALL(pointer->PointerSystem, client->context, &systemPointer);

		ogon_connection_clear_pointer_cache(conn);
		if (backend->haveBackendPointer)
			ogon_connection_set_pointer(conn, &backend->lastSetPointer);

		ogon_init_output(conn);

		return TRUE;
	}


	/**
	 * Note: the code below is only executed for the first activation or if we
	 * have initiated a DesktopResize during the first activation.
	 * However, a client might repeatedly ignore the size restrictions which
	 * we can detect by checking front->activationCount
	 */

	if (front->activationCount > 2) {
		WLog_ERR(TAG, "re-activation loop detected, bailing out");
		return FALSE;
	}

	if (front->activationCount == 1) {
		front->initialDesktopWidth = settings->DesktopWidth;
		front->initialDesktopHeight = settings->DesktopHeight;
	}

	if (settings->ColorDepth == 24 && front->codecMode == CODEC_MODE_BMP) {
		/* hack: we don't support 24 bpp in planar codec mode (would require
		 * interleaved RLE compression), so we fallback to 16 bpp.
		 */
		WLog_INFO(TAG, "color depth 24 not supported in planar codec mode, switching connection %ld to 16bpp", conn->id);
		settings->ColorDepth = 16;
		resizeClient = TRUE;
	}

	if (front->maxWidth && (settings->DesktopWidth > front->maxWidth)) {
		WLog_INFO(TAG, "client width %"PRIu32" exceeds limit of %"PRIu32"", settings->DesktopWidth, front->maxWidth);
		settings->DesktopWidth = front->maxWidth;
		resizeClient = TRUE;
	}

	if (front->maxHeight && (settings->DesktopHeight > front->maxHeight)) {
		WLog_INFO(TAG, "client height %"PRIu32" exceeds limit of %"PRIu32"", settings->DesktopHeight, front->maxHeight);
		settings->DesktopHeight = front->maxHeight;
		resizeClient = TRUE;
	}

	/*
	 * Trigger a re-size if required - this is only hit on the initial activation
	 * before the backend was created. Therefore no need to check if an other
	 * request is already pending.
	 */
	if (resizeClient) {
		ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_TRIGGER_RESIZE);
		client->update->DesktopResize(client->update->context);
		return TRUE;
	}

	/*
	 * If a re-size happened inform the state machine that it is finished now.
	 */
	if (ogon_state_get(front->state) == OGON_STATE_WAITING_RESIZE) {
		ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_RESIZED);
	}

	if (conn->backend) {
		WLog_ERR(TAG, "internal error: backend initially expected to be null");
		return FALSE;
	}

	conn->backend = backend_new(conn, &front->backendProps);
	if (!conn->backend) {
		WLog_ERR(TAG, "error creating backend");
		return FALSE;
	}

	if (!ogon_backend_initialize(conn, conn->backend, settings, settings->DesktopWidth, settings->DesktopHeight))	{
		WLog_ERR(TAG, "error sending capabilities to backend [%s]", front->backendProps.serviceEndpoint);
		goto out_fail;
	}

	front->frameTimer = CreateWaitableTimer(NULL, TRUE, NULL);
	if (!front->frameTimer) {
		WLog_ERR(TAG, "unable to create frame timer");
		goto out_fail;
	}

	due.QuadPart = 0;
	if (!SetWaitableTimer(front->frameTimer, &due, 1000 / conn->fps, NULL, NULL, 0)) {
		WLog_ERR(TAG, "unable to program frame timer");
		goto out_fail;
	}

	if (!ogon_frontend_install_frame_timer(conn)) {
		WLog_ERR(TAG, "unable to add frame timer in eventloop");
		goto out_fail;
	}

	if (settings->PointerCacheSize) {
		WLog_DBG(TAG, "creating pointer cache table for %"PRIu32" entries", settings->PointerCacheSize);
		front->pointerCache = calloc(1, sizeof(ogon_pointer_cache_entry) * settings->PointerCacheSize);
		if (!front->pointerCache){
			WLog_ERR(TAG, "Error creating pointer cache");
			goto out_fail;
		}
	}

	if (!ogon_channels_post_connect(conn)) {
		goto out_fail;
	}

	ogon_init_output(conn);
	return TRUE;

out_fail:
	backend_destroy(&conn->backend);
	conn->backend = NULL;
	return FALSE;
}

static BOOL ogon_input_synchronize_event(rdpInput *input, UINT32 flags) {
	ogon_connection *conn = (ogon_connection *)input->context;
	ogon_backend_connection* backend = conn->shadowing->backend;
	ogon_keyboard_indicator_state indicator_state = conn->front.indicators;

	/* synchronize keyboard packet means all keys up (including modifiers) */
	conn->front.modifiers = 0;
	conn->front.indicators = flags;

	if ((conn->front.inputFilter & INPUT_FILTER_KEYBOARD) || !backend ||
		!backend->client.SynchronizeKeyboardEvent)
	{
		return TRUE;
	}

	if (!backend->client.SynchronizeKeyboardEvent(backend, flags, conn->id)) {
		ogon_connection_close(conn);
	}

	/* if the backend doesn't handle multi-seat, synchronize all other connections
	 * that are bound to the same backend */
	if (!backend->multiseatCapable && (indicator_state != conn->front.indicators)) {
		ogon_connection *connection = conn->shadowing;
		LinkedList_Enumerator_Reset(connection->frontConnections);

		while (LinkedList_Enumerator_MoveNext(connection->frontConnections)) {
			ogon_connection *frontConnection = LinkedList_Enumerator_Current(connection->frontConnections);
			if ((frontConnection != conn) && frontConnection->context.update->SetKeyboardIndicators) {
				frontConnection->front.indicators = conn->front.indicators;
				frontConnection->context.update->SetKeyboardIndicators(&frontConnection->context, frontConnection->front.indicators);
			}
		}
	}
	return TRUE;
}

static void toggle_indicator_flag(ogon_connection *conn, UINT16 flag) {
	conn->front.indicators ^= flag;
}

static void ogon_update_keyboard_indicator(ogon_connection *conn, UINT16 flags, UINT16 code) {
	if (flags != KBD_FLAGS_DOWN) {
		return;
	}

	switch (code) {
		case VK_CAPITAL:
			toggle_indicator_flag(conn, KBD_SYNC_CAPS_LOCK);
			break;
		case VK_SCROLL:
			toggle_indicator_flag(conn, KBD_SYNC_SCROLL_LOCK);
			break;
		case VK_NUMLOCK:
			toggle_indicator_flag(conn, KBD_SYNC_NUM_LOCK);
			break;
		default:
			break;
	}
}

static void ogon_update_keyboard_modifiers(ogon_connection *conn, UINT16 flags, UINT16 code) {
	UINT16 andMask, orMask;

	andMask = 0xff;
	orMask = 0;

	switch (code) {
	case VK_CONTROL:
	case VK_RCONTROL:
	case VK_LCONTROL:
		if (flags & KBD_FLAGS_DOWN) {
			orMask |= OGON_KEYBOARD_CTRL;
		}
		if (flags & KBD_FLAGS_RELEASE) {
			andMask &= ~OGON_KEYBOARD_CTRL;
		}
		break;

	case VK_MENU:
	case VK_LMENU:
	case VK_RMENU:
		if (flags & KBD_FLAGS_DOWN) {
			orMask |= OGON_KEYBOARD_ALT;
		}
		if (flags & KBD_FLAGS_RELEASE) {
			andMask &= ~OGON_KEYBOARD_ALT;
		}
		break;

	case VK_LSHIFT:
	case VK_RSHIFT:
	case VK_SHIFT:
		if (flags & KBD_FLAGS_DOWN) {
			orMask |= OGON_KEYBOARD_SHIFT;
		}
		if (flags & KBD_FLAGS_RELEASE) {
			andMask &= ~OGON_KEYBOARD_SHIFT;
		}
		break;
	default:
		return;
	}

	conn->front.modifiers &= andMask;
	conn->front.modifiers |= orMask;
}

static BOOL ogon_input_keyboard_event(rdpInput *input, UINT16 flags, UINT16 code) {
	ogon_connection *conn = (ogon_connection *)input->context;
	ogon_backend_connection* backend = conn->shadowing->backend;
	rdpSettings *settings = input->context->settings;
	UINT16 vkcode, scancode;
	ogon_keyboard_indicator_state indicator_state = conn->front.indicators;

	scancode = code;
	if (flags & KBD_FLAGS_EXTENDED) {
		scancode |= KBD_FLAGS_EXTENDED;
	}
	vkcode = GetVirtualKeyCodeFromVirtualScanCode(scancode, settings->KeyboardType);

	ogon_update_keyboard_modifiers(conn, flags, vkcode);
	ogon_update_keyboard_indicator(conn, flags, vkcode);

	if (!backend || !backend->client.ScancodeKeyboardEvent) {
		return TRUE;
	}

	if ((conn->shadowing != conn) && (flags & KBD_FLAGS_DOWN)) {
		/* we're shadowing, let's see if the escape sequence has been pressed */
		if ((vkcode == conn->shadowingEscapeKey) && ((conn->front.modifiers & conn->shadowingEscapeModifiers) == conn->shadowingEscapeModifiers)) {
			if (!app_context_post_message_connection(conn->shadowing->id, NOTIFY_UNWIRE_SPY, conn, NULL)) {
				WLog_ERR(TAG, "error posting a NOTIFY_UNWIRE_SPY to self(%ld), frontend is %ld", conn->shadowing->id, conn->id);
			}

			/*
			 * After unwiring a spy the control key(s) used might still be pressed so they need to be released.
			 * Since we can't be sure which modifier key was exactly pressed send key up of all possible keys.
			 * If a modifier isn't available GetVirtualScanCodeFromVirtualKeyCode returns 0 and they key isn't sent.
			 */
			if (conn->front.modifiers & OGON_KEYBOARD_ALT) {
				backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE,
						GetVirtualScanCodeFromVirtualKeyCode(VK_MENU, settings->KeyboardType),
						settings->KeyboardType, conn->id
				);
				backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE,
						GetVirtualScanCodeFromVirtualKeyCode(VK_LMENU, settings->KeyboardType),
						settings->KeyboardType, conn->id
				);
				backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE,
						GetVirtualScanCodeFromVirtualKeyCode(VK_RMENU, settings->KeyboardType),
						settings->KeyboardType, conn->id
				);

				ogon_update_keyboard_modifiers(conn, KBD_FLAGS_RELEASE, VK_MENU);
			}

			if (conn->front.modifiers & OGON_KEYBOARD_SHIFT) {
				backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE,
						GetVirtualScanCodeFromVirtualKeyCode(VK_SHIFT, settings->KeyboardType),
						settings->KeyboardType, conn->id
				);
				backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE,
						GetVirtualScanCodeFromVirtualKeyCode(VK_LSHIFT, settings->KeyboardType),
						settings->KeyboardType, conn->id
				);
				backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE,
						GetVirtualScanCodeFromVirtualKeyCode(VK_RSHIFT, settings->KeyboardType),
						settings->KeyboardType, conn->id
				);

				ogon_update_keyboard_modifiers(conn, KBD_FLAGS_RELEASE, VK_SHIFT);
			}

			if (conn->front.modifiers & OGON_KEYBOARD_CTRL) {
				backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE,
						GetVirtualScanCodeFromVirtualKeyCode(VK_CONTROL, settings->KeyboardType),
						settings->KeyboardType, conn->id
				);
				backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE,
						GetVirtualScanCodeFromVirtualKeyCode(VK_RCONTROL, settings->KeyboardType),
						settings->KeyboardType, conn->id
				);
				backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE,
						GetVirtualScanCodeFromVirtualKeyCode(VK_LCONTROL, settings->KeyboardType),
						settings->KeyboardType, conn->id
				);

				ogon_update_keyboard_modifiers(conn, KBD_FLAGS_RELEASE, VK_CONTROL);
			}
			return TRUE;
		}
	}

	if (conn->front.inputFilter & INPUT_FILTER_KEYBOARD) {
		return TRUE;
	}

	/* Fix for sick mstsc behaviour sending CONTROL_L together with ALTGR */
	if ((code == 56) && (flags == (KBD_FLAGS_DOWN|KBD_FLAGS_EXTENDED))) {
		if (!backend->client.ScancodeKeyboardEvent(backend, KBD_FLAGS_RELEASE, 29, settings->KeyboardType, conn->id)) {
			ogon_connection_close(conn);
			return TRUE;
		}
	}

	if (!backend->client.ScancodeKeyboardEvent(backend, flags, code, settings->KeyboardType, conn->id)) {
		ogon_connection_close(conn);
		return TRUE;
	}

	if (!backend->multiseatCapable && (indicator_state != conn->front.indicators)) {
		ogon_connection *connection = conn->shadowing;
		LinkedList_Enumerator_Reset(connection->frontConnections);
		while (LinkedList_Enumerator_MoveNext(connection->frontConnections)) {
			ogon_connection *frontConnection = LinkedList_Enumerator_Current(connection->frontConnections);
			if ((frontConnection != conn) && frontConnection->context.update->SetKeyboardIndicators) {
				frontConnection->front.indicators = conn->front.indicators;
				frontConnection->context.update->SetKeyboardIndicators(&frontConnection->context, frontConnection->front.indicators);
			}
		}
	}

	return TRUE;
}

static BOOL ogon_input_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code) {
	ogon_connection *conn = (ogon_connection *)input->context;
	ogon_backend_connection* backend = conn->shadowing->backend;

	if ((conn->front.inputFilter & INPUT_FILTER_KEYBOARD) || !backend || !backend->client.UnicodeKeyboardEvent) {
		return TRUE;
	}

	if (!backend->client.UnicodeKeyboardEvent(backend, flags, code, conn->id)) {
		ogon_connection_close(conn);
	}

	return TRUE;
}

static BOOL ogon_input_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y) {
	ogon_connection *conn = (ogon_connection*) input->context;
	ogon_backend_connection* backend = (ogon_backend_connection *)conn->shadowing->backend;
	ogon_connection *connection = conn->shadowing;
	POINTER_POSITION_UPDATE pointerUpdate = { 0 };

	if ((conn->front.inputFilter & INPUT_FILTER_MOUSE) || !backend || !backend->client.MouseEvent) {
		return TRUE;
	}

	if (!backend->client.MouseEvent(backend, flags, x, y, conn->id)) {
		ogon_connection_close(conn);
	}

	pointerUpdate.xPos = x;
	pointerUpdate.yPos = y;

	if (!backend->multiseatCapable) {
		LinkedList_Enumerator_Reset(connection->frontConnections);
		while (LinkedList_Enumerator_MoveNext(connection->frontConnections)) {
			ogon_connection *frontConnection = LinkedList_Enumerator_Current(connection->frontConnections);
			if ((frontConnection != conn) && frontConnection->context.update->pointer->PointerPosition) {
				frontConnection->context.update->pointer->PointerPosition(&frontConnection->context, &pointerUpdate);
			}
		}
	}
	return TRUE;
}

static BOOL ogon_input_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y) {
	ogon_connection *conn = (ogon_connection*) input->context;
	ogon_backend_connection* backend = (ogon_backend_connection *)conn->shadowing->backend;

	if ((conn->front.inputFilter & INPUT_FILTER_MOUSE) || !backend || !backend->client.ExtendedMouseEvent) {
		return TRUE;
	}

	if (!backend->client.ExtendedMouseEvent(backend, flags, x, y, conn->id)) {
		ogon_connection_close(conn);
	}

	return TRUE;
}


static BOOL ogon_refresh_rect(rdpContext *context, BYTE count, const RECTANGLE_16* areas) {
	ogon_connection *conn = (ogon_connection*)context;
	ogon_backend_connection* backend = conn->backend;
	ogon_front_connection *frontend = &conn->front;
	ogon_bitmap_encoder *encoder = frontend->encoder;

	RECTANGLE_16 r;
	int i;

	if (!encoder || !backend || !count) {
		return TRUE;
	}

	for (i = 0; i < count; i++) {
		/* areas are actually TS_RECTANGLE_16 structures which describe
		 * a rectangle expressed in inclusive coordinates (the right and
		 * bottom coordinates are included in the rectangle bounds).
		 */
		r = areas[i];

		if (r.right < r.left) {
			continue;
		}

		if (r.bottom < r.top) {
			continue;
		}

		r.right++;
		r.bottom++;

		if (r.right >= encoder->desktopWidth) {
			r.right = encoder->desktopWidth;
		}
		if (r.bottom >= encoder->desktopHeight) {
			r.bottom = encoder->desktopHeight;
		}
		if (!region16_union_rect(&encoder->accumulatedDamage, &encoder->accumulatedDamage, &r)) {
			WLog_ERR(TAG, "error when computing union_rect");
			return TRUE;
		}

		ogon_encoder_blank_client_view_area(frontend->encoder, &r);
	}

	/* Sending an immediate sync message allows to have the backend reply directly even
	 * if it was waiting for some real damage to occur.
	 */
	initiate_immediate_request(conn, frontend, FALSE);

	return TRUE;
}

static BOOL ogon_suppress_output(rdpContext *context, BYTE allow, const RECTANGLE_16* area)
{
	ogon_connection *connection = (ogon_connection*)context;
	ogon_front_connection *front = (ogon_front_connection *)&connection->front;

	/* WLog_DBG(TAG, "conn=%ld allow=%"PRIu8"", connection->id, allow); */
	if (allow) {
		if (area == NULL) {
			WLog_ERR(TAG, "protocol error, area must _not_ be null");
			return TRUE;
		}
		ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_ENABLE_OUTPUT);
		handle_wait_timer_state(connection);
	}
	else
	{
		if (area != NULL) {
			WLog_ERR(TAG, "protocol error. area _must_ be null.");
			return TRUE;
		}
		/* WLog_DBG(TAG, "output suppressed."); */
		ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_DISABLE_OUTPUT);
	}

	return TRUE;
}

static BOOL ogon_update_frame_acknowledge(rdpContext *context, UINT32 frameId)
{
	ogon_connection *connection = (ogon_connection*) context;
	ogon_front_connection *frontend = (ogon_front_connection *)&connection->front;

	/* WLog_DBG(TAG, "%s: frameId=%"PRIu32"", __FUNCTION__, frameId); */

	frontend->lastAckFrame = frameId;

	if (ogon_state_get(frontend->state) != OGON_STATE_WAITING_ACK)
		return TRUE;

	if (frontend->frameAcknowledge)
		if (frontend->lastAckFrame + frontend->frameAcknowledge + 1 < frontend->nextFrameId)
			return TRUE;

	ogon_state_set_event(frontend->state, OGON_EVENT_FRONTEND_FRAME_ACK_RECEIVED);
	handle_wait_timer_state(connection);

	return TRUE;
}

BOOL ogon_rdpgfx_shutdown(ogon_connection *conn)
{
	ogon_front_connection *front = &conn->front;
	RDPGFX_DELETE_SURFACE_PDU delete_surface = { 0 };
	if (front->rdpgfxConnected) {
		WLog_DBG(TAG, "shutting down graphics pipeline channel");
		if (front->rdpgfxOutputSurface) {
			delete_surface.surfaceId = front->rdpgfxOutputSurface;
			IFCALL(front->rdpgfx->DeleteSurface, front->rdpgfx, &delete_surface);
			front->rdpgfxOutputSurface = 0;
		}
		front->rdpgfx->Close(front->rdpgfx);
	}
	return TRUE;
}

BOOL ogon_rdpgfx_init_output(ogon_connection *conn)
{
	ogon_front_connection *front = &conn->front;
	ogon_bitmap_encoder *encoder = front->encoder;
	UINT32 width, height;

	RDPGFX_RESET_GRAPHICS_PDU reset_graphics = { 0 };
	RDPGFX_CREATE_SURFACE_PDU create_surface = { 0 };
	RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU map_surface_to_output = { 0 };

	if (!front->rdpgfxConnected || !encoder) {
		return TRUE;
	}

	width = encoder->desktopWidth;
	height = encoder->desktopHeight;

	WLog_DBG(TAG, "initializing rdpgfx output %"PRIu32"x%"PRIu32" framestate = %d", width, height, ogon_state_get(front->state));

	reset_graphics.width = width;
	reset_graphics.height = height;
	reset_graphics.monitorCount = 1;
	if (!front->rdpgfx->ResetGraphics(front->rdpgfx, &reset_graphics)) {
		WLog_ERR(TAG, "%s: ResetGraphics FAILED", __FUNCTION__);
		goto err;
	}

	front->rdpgfxOutputSurface = 1;
	create_surface.surfaceId = front->rdpgfxOutputSurface;
	create_surface.width = width;
	create_surface.height = height;
	create_surface.pixelFormat = GFX_PIXEL_FORMAT_ARGB_8888;

	if (!front->rdpgfx->CreateSurface(front->rdpgfx, &create_surface)) {
		WLog_ERR(TAG, "%s: CreateSurface FAILED", __FUNCTION__);
		goto err;
	}

#if 0
	{
		RDPGFX_SOLID_FILL_PDU solidfill = { 0 };
		RECTANGLE_16 fillRect;
		solidfill.surfaceId = front->rdpgfxOutputSurface;
		solidfill.fillPixel.B = 0xFF;
		solidfill.fillPixel.G = 0xFF;
		solidfill.fillPixel.R = 0;
		solidfill.fillPixel.XA = 0xFF;
		solidfill.fillRectCount = 1;
		solidfill.fillRects = &fillRect;
		fillRect.left = 0;
		fillRect.top = 0;
		fillRect.right = width;
		fillRect.bottom = height;

		if (!front->rdpgfx->SolidFill(front->rdpgfx, &solidfill)) {
			WLog_ERR(TAG, "%s: SolidFill FAILED", __FUNCTION__);
			goto err;
		}
	}
#endif

	map_surface_to_output.surfaceId = front->rdpgfxOutputSurface;
	map_surface_to_output.reserved = 0;
	map_surface_to_output.outputOriginX = 0;
	map_surface_to_output.outputOriginY = 0;
	if (!front->rdpgfx->MapSurfaceToOutput(front->rdpgfx, &map_surface_to_output)) {
		WLog_ERR(TAG, "%s: MapSurfaceToOutput FAILED", __FUNCTION__);
		goto err;
	}

	return TRUE;

err:
	IFCALL(front->rdpgfx->Close, front->rdpgfx);
	return FALSE;
}

static void ogon_rdpgfx_open_result(rdpgfx_server_context *rdpgfx, rdpgfx_server_open_result result)
{
	ogon_connection *conn = (ogon_connection*) rdpgfx->data;
	ogon_front_connection *front = &conn->front;

	switch (result)
	{
	case RDPGFX_SERVER_OPEN_RESULT_OK:
		WLog_DBG(TAG, "%s: OK", __FUNCTION__);
		if (rdpgfx->h264Supported) {
			WLog_DBG(TAG, "%s: client supports H.264 codec (AVC444 = %"PRIu32")", __FUNCTION__, rdpgfx->avc444Supported);
			front->rdpgfxH264Supported = !front->rdpgfxH264Forbidden;
		}
		front->rdpgfxConnected = TRUE;
		goto out;

	case RDPGFX_SERVER_OPEN_RESULT_CLOSED:
		WLog_DBG(TAG, "%s: CLOSED", __FUNCTION__);
		break;
	case RDPGFX_SERVER_OPEN_RESULT_ERROR:
		WLog_DBG(TAG, "%s: ERROR", __FUNCTION__);
		break;
	case RDPGFX_SERVER_OPEN_RESULT_NOTSUPPORTED:
		WLog_DBG(TAG, "%s: NOT SUPPORTED", __FUNCTION__);
		break;
	default:
		WLog_DBG(TAG, "%s: UNKNOWN (%d)", __FUNCTION__, result);
		break;
	}

	WLog_DBG(TAG, "graphics pipeline disabled");

	front->rdpgfxH264Supported = FALSE;
	front->rdpgfxConnected = FALSE;
	front->rdpgfxRequired = FALSE;
	front->frameAcknowledge = 0;
	front->codecMode = CODEC_MODE_BMP;

out:
	ogon_state_set_event(front->state, OGON_EVENT_FRONTEND_STOP_WAITING_GFX);
	initiate_immediate_request(conn->shadowing, front, TRUE);
}

static void ogon_rdpgfx_frame_acknowledge(rdpgfx_server_context *rdpgfx, RDPGFX_FRAME_ACKNOWLEDGE_PDU *frame_acknowledge)
{
	ogon_connection *conn = (ogon_connection*) rdpgfx->data;
	ogon_front_connection *front = &conn->front;

	/* WLog_DBG(TAG, "%s: frameId=%"PRIu32"", __FUNCTION__, frame_acknowledge->frameId); */

	/* Note:
	 * If frame_acknowledge->queueDepth is 0xFFFFFFFF (SUSPEND_FRAME_ACKNOWLEDGEMENT) the
	 * client is telling us that it will no longer be transmitting RDPGFX_FRAME_ACKNOWLEDGE_PDU messages
	 * It can can opt back into sending these messages by sending an RDPGFX_FRAME_ACKNOWLEDGE_PDU
	 * with the queueDepth field set to a value in the range 0x00000000 to 0xFFFFFFFE (inclusive)
	 * in response to an RDPGFX_END_FRAME_PDU message.
	 */

	if (frame_acknowledge->queueDepth == SUSPEND_FRAME_ACKNOWLEDGEMENT) {
		WLog_DBG(TAG, "connection %ld suspended gfx frame acknowledgement", conn->id);
		front->frameAcknowledge = 0;
	}
	else if (!front->frameAcknowledge) {
		WLog_DBG(TAG, "connection %ld reenabled gfx frame acknowledgement", conn->id);
		front->frameAcknowledge = conn->context.settings->FrameAcknowledge;
		if (!front->frameAcknowledge)
			front->frameAcknowledge = 5;
	}

	if (front->lastAckFrame < frame_acknowledge->frameId)
		ogon_update_frame_acknowledge(&conn->context, frame_acknowledge->frameId);
}

static void ogon_rdpgfx_qoe_frame_acknowledge(rdpgfx_server_context *rdpgfx, RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU *qoe_frame_acknowledge)
{
	/* Note:
	 * Ogon currently makes no use of this message.
	 * Here is the info form MS-RDPEGFX:
	 *
	 * The optional RDPGFX_QOE_FRAME_ACKNOWLEDGE_PDU message is sent by the client
	 * to enable the calculation of Quality of Experience (QoE) metrics.
	 * This message is sent solely for informational and debugging purposes.
	 *
	 * timestamp (4 bytes): A 32-bit unsigned integer that specifies the timestamp
	 * (in milliseconds) when the client started decoding the RDPGFX_START_FRAME_PDU message.
	 * The value of the first timestamp sent by the client implicitly defines the origin
	 * for all subsequent timestamps. The server is responsible for handling roll-over of the timestamp.
	 *
	 * timeDiffSE (2 bytes): A 16-bit unsigned integer that specifies the time, in
	 * milliseconds, that elapsed between the decoding of the RDPGFX_START_FRAME_PDU and
	 * RDPGFX_END_FRAME_PDU messages. If the elapsed time is greater than 65 seconds,
	 * then this field SHOULD be set to 0x0000.
	 *
	 * timeDiffEDR (2 bytes): A 16-bit unsigned integer that specifies the time, in milliseconds,
	 * that elapsed between the decoding of the RDPGFX_END_FRAME_PDU message and the completion of the
	 * rendering operation for the commands contained in the logical graphics frame. If the elapsed
	 * time is greater than 65 seconds, then this field SHOULD be set to 0x0000.
	 */
#if 1
        OGON_UNUSED(rdpgfx);
        OGON_UNUSED(qoe_frame_acknowledge);
#else
	ogon_connection *conn = (ogon_connection*) rdpgfx->data;
	ogon_front_connection *front = &conn->front;

	WLog_DBG(TAG, "%s: frameId=%"PRIu32" timestamp=%"PRIu32" timeDiffSE=%"PRIu16" timeDiffEDR=%"PRIu16"",
	               __FUNCTION__, qoe_frame_acknowledge->frameId, qoe_frame_acknowledge->timestamp,
	               qoe_frame_acknowledge->timeDiffSE, qoe_frame_acknowledge->timeDiffEDR);
#endif
}

static void ogon_rdpgfx_cache_import_offer(rdpgfx_server_context *rdpgfx, RDPGFX_CACHE_IMPORT_OFFER_PDU *cache_import_offer)
{
	ogon_connection *conn = (ogon_connection*) rdpgfx->data;
	ogon_front_connection *front = &conn->front;
	RDPGFX_CACHE_IMPORT_REPLY_PDU cache_import_reply = { 0 };
#if 1
	OGON_UNUSED(cache_import_offer);
#else
	int i;

	WLog_DBG(TAG, "%s: cacheEntriesCount=%"PRIu16"", __FUNCTION__, cache_import_offer->cacheEntriesCount);
	for (i = 0; i < cache_import_offer->cacheEntriesCount; i++) {
		WLog_DBG(TAG, "%s: cacheEntry[%"PRIu16"].cacheKey: 0x%"PRIx64"",
			 __FUNCTION__, i, cache_import_offer->cacheEntries[i].cacheKey);
		WLog_DBG(TAG, "%s: cacheEntry[%"PRIu16"].bitmapLength: %"PRIu32"",
			 __FUNCTION__, i, cache_import_offer->cacheEntries[i].bitmapLength);
	}
#endif

	/* Note:
	 * Ogon currently isn't interested in this data, but we must tell the client
	 * that we haven't imported anything
	 */

	cache_import_reply.importedEntriesCount = 0;

	if (!front->rdpgfx->CacheImportReply(front->rdpgfx, &cache_import_reply)) {
		WLog_ERR(TAG, "%s: CacheImportReply FAILED", __FUNCTION__);
	}
}

BOOL ogon_connection_init_front(ogon_connection *conn)
{
	rdpInput *input;
	rdpUpdate *update;
	rdpSettings *settings = conn->context.settings;
	ogon_front_connection *front = &conn->front;
	ogon_bandwidth_mgmt *bwmgmt = &conn->front.bandwidthMgmt;

	freerdp_peer *peer = conn->context.peer;
	int res;

	PropertyItem reqs[] = {
	/*0*/	PROPERTY_ITEM_INIT_STRING("ssl.certificate"),
	/*1*/	PROPERTY_ITEM_INIT_STRING("ssl.key"),
	/*2*/	PROPERTY_ITEM_INIT_BOOL("ogon.forceWeakRdpKey", FALSE),
	/*3*/	PROPERTY_ITEM_INIT_BOOL("ogon.showDebugInfo", FALSE),
	/*4*/	PROPERTY_ITEM_INIT_BOOL("ogon.disableGraphicsPipeline", FALSE),
	/*5*/	PROPERTY_ITEM_INIT_INT("ogon.bitrate", 0),
	/*6*/	PROPERTY_ITEM_INIT_BOOL("ogon.disableGraphicsPipelineH264", FALSE),
	/*7*/	PROPERTY_ITEM_INIT_BOOL("ogon.enableFullAVC444", FALSE),
	/*8*/   PROPERTY_ITEM_INIT_BOOL("ogon.restrictAVC444", FALSE),
		PROPERTY_ITEM_INIT_INT(NULL, 0), /* last one */
	};

	enum {
		INDEX_CERT = 0,
		INDEX_KEY,
		INDEX_FORCE_WEAK,
		INDEX_SHOW_DEBUG,
		INDEX_NO_EGFX,
		INDEX_BITRATE,
		INDEX_NO_H264,
		INDEX_AVC444,
		INDEX_RESTRICT_AVC444
	};

	res = ogon_icp_get_property_bulk(conn->id, reqs);
	if (res != PBRPC_SUCCESS) {
		WLog_ERR(TAG, "error retrieving properties by the bulk method (res=%d)", res);
		ogon_PropertyItem_free(reqs);
		return FALSE;
	}

	if (!reqs[INDEX_CERT].success) {
		WLog_ERR(TAG, "unable to retrieve certificate file path (ssl.certificate path)");
		ogon_PropertyItem_free(reqs);
		return FALSE;
	}

	if (!reqs[INDEX_KEY].success) {
		WLog_ERR(TAG, "unable to retrieve key file path (ssl.key)");
		ogon_PropertyItem_free(reqs);
		return FALSE;
	}

	if (ogon_generate_certificate(conn, reqs[INDEX_CERT].v.stringValue, reqs[INDEX_KEY].v.stringValue) < 0) {
		ogon_PropertyItem_free(reqs);
		return FALSE;
	}

	if (reqs[INDEX_FORCE_WEAK].success && reqs[INDEX_FORCE_WEAK].v.boolValue) {
			free(settings->RdpKeyFile);
			settings->RdpKeyFile = NULL;
			settings->RdpServerRsaKey = ogon_generate_weak_rsa_key();
	}

	front->showDebugInfo = reqs[INDEX_SHOW_DEBUG].v.boolValue;
	front->rdpgfxForbidden = reqs[INDEX_NO_EGFX].v.boolValue;


	peer->settings->NetworkAutoDetect = TRUE;
	peer->autodetect->BandwidthMeasureResults = ogon_bwmgmt_client_bandwidth_measure_results;
	peer->autodetect->RTTMeasureResponse = ogon_bwmgmt_client_rtt_measure_response;

#ifdef WITH_OPENH264
	if (!front->rdpgfxForbidden) {
		if (reqs[INDEX_NO_H264].success) {
			front->rdpgfxH264Forbidden = reqs[INDEX_NO_H264].v.boolValue;
		}
		if (reqs[INDEX_AVC444].success) {
			front->rdpgfxH264EnableFullAVC444 = reqs[INDEX_AVC444].v.boolValue;
		}
	}
#else
	front->rdpgfxH264Forbidden = TRUE;
#endif

	if (reqs[INDEX_BITRATE].success) { /* "ogon.bitrate" */
		bwmgmt->configured_bitrate = (UINT32)reqs[INDEX_BITRATE].v.intValue;
	}
	if (bwmgmt->configured_bitrate) {
		WLog_INFO(TAG, "Using fixed encoder bitrate (applies only for h264 for now) of %"PRIu32"", bwmgmt->configured_bitrate);
	} else {
		WLog_INFO(TAG, "Using bandwidth management to adjust encoder bitrate (applies only for h264 for now)");
	}

	ogon_PropertyItem_free(reqs);

	peer->Initialize(peer);

	settings->OsMajorType = OSMAJORTYPE_UNIX;
	settings->OsMinorType = OSMINORTYPE_PSEUDO_XSERVER;
	settings->ColorDepth = 32;
	settings->RefreshRect = TRUE;
	settings->RemoteFxCodec = TRUE;
	settings->BitmapCacheV3Enabled = TRUE;
	settings->FrameMarkerCommandEnabled = TRUE;
	settings->SurfaceFrameMarkerEnabled = TRUE;
	settings->SupportGraphicsPipeline = TRUE;
	settings->WaitForOutputBufferFlush = FALSE;

	settings->RdpSecurity = TRUE;
	settings->TlsSecurity = TRUE;
	settings->NlaSecurity = FALSE;

	settings->UnicodeInput = TRUE;
	settings->HasHorizontalWheel = TRUE;
	settings->HasExtendedMouseEvent = TRUE;

	peer->Capabilities = ogon_peer_capabilities;
	peer->PostConnect = ogon_peer_post_connect;
	peer->Activate = ogon_peer_activate;

	input = peer->input;
	input->SynchronizeEvent = ogon_input_synchronize_event;
	input->KeyboardEvent = ogon_input_keyboard_event;
	input->UnicodeKeyboardEvent = ogon_input_unicode_keyboard_event;
	input->MouseEvent = ogon_input_mouse_event;
	input->ExtendedMouseEvent = ogon_input_extended_mouse_event;

	update = peer->update;
	update->SurfaceFrameAcknowledge = ogon_update_frame_acknowledge;
	update->SuppressOutput = ogon_suppress_output;
	update->RefreshRect = ogon_refresh_rect;

	front->state = ogon_state_new();
	if (!front->state) {
		return FALSE;
	}
	front->vcm = openVirtualChannelManager(conn);
	if (!front->vcm) {
		return FALSE;
	}

	if (!(front->rdpgfx = rdpgfx_server_context_new(front->vcm))) {
		return FALSE;
	}

	front->rdpgfx->data = conn;
	front->rdpgfx->OpenResult = ogon_rdpgfx_open_result;
	front->rdpgfx->FrameAcknowledge = ogon_rdpgfx_frame_acknowledge;
	front->rdpgfx->QoeFrameAcknowledge = ogon_rdpgfx_qoe_frame_acknowledge;
	front->rdpgfx->CacheImportOffer = ogon_rdpgfx_cache_import_offer;

	if (!front->rdpgfxForbidden) {
		if (reqs[INDEX_RESTRICT_AVC444].success) {
			front->rdpgfx->avc444Restricted = reqs[INDEX_RESTRICT_AVC444].v.boolValue;
		}
	}

	conn->frontConnections = LinkedList_New();
	if (!conn->frontConnections) {
		return FALSE;
	}

	ogon_bwmgmt_init_buckets(conn, bwmgmt->configured_bitrate ? bwmgmt->configured_bitrate : 0);
	return LinkedList_AddFirst(conn->frontConnections, conn);
}

void frontend_destroy(ogon_front_connection *front)
{
	if (!front) {
		return;
	}

	if (front->vcm)	{
		closeVirtualChannelManager(front->vcm);
		front->vcm = NULL;
	}

	if (front->frameEventSource)
		eventloop_remove_source(&front->frameEventSource);

	if (front->frameTimer) {
		CloseHandle(front->frameTimer);
		front->frameTimer = NULL;
	}

	if (front->encoder) {
		ogon_bitmap_encoder_free(front->encoder);
		front->encoder = NULL;
	}

	ogon_state_free(front->state);

	if (front->pointerCache) {
		WLog_DBG(TAG, "freeing pointer cache");
		free(front->pointerCache);
		front->pointerCache = NULL;
	}

	ogon_backend_props_free(&front->backendProps);

	if (front->rdpgfx) {
		front->rdpgfx->OpenResult = NULL;
		front->rdpgfx->FrameAcknowledge = NULL;
		rdpgfx_server_context_free(front->rdpgfx);
		front->rdpgfx = NULL;
	}
}
