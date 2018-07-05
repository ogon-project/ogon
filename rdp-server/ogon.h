/**
 * ogon - Free Remote Desktop Services
 * RDP Server
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

#ifndef _OGON_RDPSRV_OGON_H_
#define _OGON_RDPSRV_OGON_H_

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>
#include <freerdp/codec/region.h>
#include <freerdp/channels/channels.h>

#include "commondefs.h"
#include <ogon/backend.h>


int ogon_send_surface_bits(ogon_connection *conn);

void ogon_connection_set_pointer(ogon_connection *connection, ogon_msg_set_pointer* msg);

void ogon_connection_clear_pointer_cache(ogon_connection *connection);


struct ogon_notification_switch {
	UINT32 tag;
	ogon_backend_props props;
	UINT32 maxWidth;
	UINT32 maxHeight;
};

struct ogon_notification_logoff {
	UINT32 tag;
};

struct ogon_notification_vc_connect {
	UINT32 tag;
	char *vcname;
	BOOL isDynamic;
	DWORD flags;
};

struct ogon_notification_vc_disconnect {
	UINT32 tag;
	char *vcname;
	DWORD instance;
};

struct ogon_notification_start_remote_control {
	UINT32 tag;
	UINT32 connectionId;
	UINT32 targetId;
	BYTE hotKeyVk;
	DWORD hotKeyModifier;
	DWORD flags;
};

struct rds_notification_stop_remote_control {
	UINT32 tag;
};

struct rds_notification_new_shadowing_frontend {
	struct ogon_notification_start_remote_control startRemoteControl;
	ogon_connection *srcConnection;
	int originalRdpMask;
	int originalFd;
};

struct ogon_notification_msg_message {
	UINT32 tag;
	UINT32 type;
	UINT32 style;
	UINT32 timeout;
	UINT32 parameter_num;
	char* parameter1;
	char* parameter2;
	char* parameter3;
	char* parameter4;
	char* parameter5;
};

struct ogon_notification_rewire_backend {
	int rdpMask;
	int rdpFd;
	BOOL rewire;
};

struct ogon_notification_logon_info {
	UINT32 sessionId;
	char* user;
	char* domain;
	BOOL haveCookie;
	BYTE cookie[16];
};

/** @brief sent to the eventloop */
enum {
	NOTIFY_SWITCHTO = 0,
	NOTIFY_LOGOFF,
	NOTIFY_SBP_REPLY,
	NOTIFY_VC_CONNECT,
	NOTIFY_VC_DISCONNECT,
	NOTIFY_START_REMOTE_CONTROL,
	NOTIFY_NEW_SHADOWING_FRONTEND,
	NOTIFY_REWIRE_ORIGINAL_BACKEND,
	NOTIFY_UNWIRE_SPY,
	NOTIFY_STOP_SHADOWING,
	NOTIFY_USER_MESSAGE,
	NOTIFY_LOGON_INFO,
};


#endif /* _OGON_RDPSRV_OGON_H_ */
