/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * SessionNotifier class
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "SessionNotifier.h"
#include <utils/CSGuard.h>
#include <winpr/wlog.h>

namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_SessionNotifier = WLog_Get("ogon.sessionmanager.session.sessionnotifier");

	SessionNotifier::SessionNotifier() : mDBusConn(NULL) {
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_SessionNotifier, WLOG_FATAL,
				"Failed to initialize session notifier critical section");
			throw std::bad_alloc();
		}
	}

	bool SessionNotifier::init() {
		DBusError err;
		int ret;

		dbus_error_init(&err);

		mDBusConn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
		if (dbus_error_is_set(&err)) {
			WLog_Print(logger_SessionNotifier, WLOG_FATAL,
				"Could not connect to dbus (%s)", err.message);
			dbus_error_free(&err);
			return false;
		}
		if (NULL == mDBusConn) {
			WLog_Print(logger_SessionNotifier, WLOG_FATAL,
				"Could not connect to dbus.");
			return false;
		}

		ret = dbus_bus_request_name(mDBusConn, "ogon.SessionManager.session.notification",
			DBUS_NAME_FLAG_REPLACE_EXISTING , &err);
		if (dbus_error_is_set(&err)) {
			WLog_Print(logger_SessionNotifier, WLOG_FATAL,
				"Could not register dbus name (%s)", err.message);
			dbus_error_free(&err);
			return false;
		}
		if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != ret) {
			WLog_Print(logger_SessionNotifier, WLOG_FATAL,
				"Could not get ownership of dbus name");
			return false;
		}
		return true;
	}

	bool SessionNotifier::shutdown() {
		return true;
	}

	SessionNotifier::~SessionNotifier() {
		shutdown();
		DeleteCriticalSection(&mCSection);
	}

	bool SessionNotifier::notify(DWORD reason, UINT32 sessionId) {
		if (!mDBusConn) {
			return false;
		}
		DBusMessage *msg;
		DBusMessageIter args;
		dbus_uint32_t serial = 0;

		CSGuard guard(&mCSection);

		msg = dbus_message_new_signal("/ogon/SessionManager/session/notification", // object name of the signal
									 "ogon.SessionManager.session.notification", // interface name of the signal
									 "SessionNotification"); // name of the signal
		if (NULL == msg) {
			WLog_Print(logger_SessionNotifier, WLOG_ERROR,
				"could not create new dbus message");
			return false;
		}

		dbus_message_iter_init_append(msg, &args);
		if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &reason)) {
			WLog_Print(logger_SessionNotifier, WLOG_ERROR, "out of Memory");
			dbus_message_unref(msg);
			return false;
		}

		if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &sessionId)) {
			WLog_Print(logger_SessionNotifier, WLOG_ERROR, "out of Memory");
			dbus_message_unref(msg);
			return false;
		}

		if (!dbus_connection_send(mDBusConn, msg, &serial)) {
			WLog_Print(logger_SessionNotifier, WLOG_ERROR, "out of Memory");
			dbus_message_unref(msg);
			return false;
		}
		dbus_connection_flush(mDBusConn);

		WLog_Print(logger_SessionNotifier, WLOG_TRACE,
			"s %" PRIu32 ": sent session notification %s",
			sessionId, wtsNotificationToString(reason));

		dbus_message_unref(msg);
		return true;
	}

	const char * SessionNotifier::wtsNotificationToString(int signal) {
		switch (signal) {
			case 0x1:
				return "WTS_CONSOLE_CONNECT";
			case 0x2:
				return "WTS_CONSOLE_DISCONNECT";
			case 0x3:
				return "WTS_REMOTE_CONNECT";
			case 0x4:
				return "WTS_REMOTE_DISCONNECT";
			case 0x5:
				return "WTS_SESSION_LOGON";
			case 0x6:
				return "WTS_SESSION_LOGOFF";
			case 0x7:
				return "WTS_SESSION_LOCK";
			case 0x8:
				return "WTS_SESSION_UNLOCK";
			case 0x9:
				return "WTS_SESSION_REMOTE_CONTROL";
			case 0xA:
				return "WTS_SESSION_CREATE";
			case 0xB:
				return "WTS_SESSION_TERMINATE";
			default:
				return "UNKNOW_MESSAGE";
		}
	}

} /*session*/ } /*sessionmanager*/ } /*ogon*/
