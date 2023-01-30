/**
 * ogon - Free Remote Desktop Services
 * Internal Communication Protocol (ICP)
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#include "../../common/icp.h"
#include "ICP.pb-c.h"
#include "icp_server_stubs.h"
#include "pbrpc.h"
#include "pipe_transport.h"

struct icp_context {
	pbRPCContext *pbcontext;
	pbRPCTransportContext *tpcontext;
};

static struct icp_context *icpContext = nullptr;

static pbRPCMethod icpMethods[] = {{OGON__ICP__MSGTYPE__Ping, ping},
		{OGON__ICP__MSGTYPE__SwitchTo, switchTo},
		{OGON__ICP__MSGTYPE__LogoffUserSession, logoffUserSession},
		{OGON__ICP__MSGTYPE__OtsApiVirtualChannelOpen,
				otsapiVirtualChannelOpen},
		{OGON__ICP__MSGTYPE__OtsApiVirtualChannelClose,
				otsapiVirtualChannelClose},
		{OGON__ICP__MSGTYPE__OtsApiStartRemoteControl,
				otsapiStartRemoteControl},
		{OGON__ICP__MSGTYPE__OtsApiStopRemoteControl, otsapiStopRemoteControl},
		{OGON__ICP__MSGTYPE__Message, message}, {0, nullptr}};

int ogon_icp_start(HANDLE shutdown, UINT32 vmajor, UINT32 vminor) {
	icpContext = new (icp_context);
	if (!icpContext) return -1;

	icpContext->tpcontext = tp_npipe_new();
	if (!icpContext->tpcontext) goto out_free;

	icpContext->pbcontext = pbrpc_server_new(icpContext->tpcontext, shutdown);
	if (!icpContext->pbcontext) goto out_free_tpContext;

	pbrpc_register_methods(icpContext->pbcontext, icpMethods);
	pbrpc_server_start(icpContext->pbcontext, vmajor, vminor);
	return 0;

out_free_tpContext:
	tp_npipe_free(icpContext->tpcontext);
out_free:
	free(icpContext);
	return -1;
}

int ogon_icp_shutdown() {
	pbrpc_server_stop(icpContext->pbcontext);
	pbrpc_server_free(icpContext->pbcontext);
	tp_npipe_free(icpContext->tpcontext);
	delete (icpContext);
	return 0;
}

void *ogon_icp_get_context() { return icpContext->pbcontext; }

BOOL ogon_icp_get_protocol_version(
		void *context, UINT32 *vmajor, UINT32 *vminor) {
	pbRPCContext *pbrpc = (pbRPCContext *)context;

	if (!context || !vmajor || !vminor || !pbrpc->isConnected) {
		return FALSE;
	}

	*vmajor = pbrpc->remoteVersionMajor;
	*vminor = pbrpc->remoteVersionMinor;
	return TRUE;
}

void ogon_icp_set_disconnect_cb(disconnected_callback cb) {
	icpContext->pbcontext->disconnectedCb = cb;
}
