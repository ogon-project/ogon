/**
 * ogon - Free Remote Desktop Services
 * Session Notification Monitor
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

#include <stdio.h>
#include <stdlib.h>
#include <dbus/dbus.h>
#include <unistd.h>
#include <winpr/wtypes.h>


#define WTS_CONSOLE_CONNECT 		0x1
#define WTS_CONSOLE_DISCONNECT	 	0x2
#define WTS_REMOTE_CONNECT 			0x3
#define WTS_REMOTE_DISCONNECT 		0x4
#define WTS_SESSION_LOGON 			0x5
#define WTS_SESSION_LOGOFF 			0x6
#define WTS_SESSION_LOCK 			0x7
#define WTS_SESSION_UNLOCK 			0x8
#define WTS_SESSION_REMOTE_CONTROL 	0x9
#define WTS_SESSION_CREATE 			0xA
#define WTS_SESSION_TERMINATE 		0xB

static const char *wtsNotificationToString(int signal) {
	switch (signal) {
		case WTS_CONSOLE_CONNECT:
			return "WTS_CONSOLE_CONNECT";
		case WTS_CONSOLE_DISCONNECT:
			return "WTS_CONSOLE_DISCONNECT";
		case WTS_REMOTE_CONNECT:
			return "WTS_REMOTE_CONNECT";
		case WTS_REMOTE_DISCONNECT:
			return "WTS_REMOTE_DISCONNECT";
		case WTS_SESSION_LOGON:
			return "WTS_SESSION_LOGON";
		case WTS_SESSION_LOGOFF:
			return "WTS_SESSION_LOGOFF";
		case WTS_SESSION_LOCK:
			return "WTS_SESSION_LOCK";
		case WTS_SESSION_UNLOCK:
			return "WTS_SESSION_UNLOCK";
		case WTS_SESSION_REMOTE_CONTROL:
			return "WTS_SESSION_REMOTE_CONTROL";
		case WTS_SESSION_CREATE:
			return "WTS_SESSION_CREATE";
		case WTS_SESSION_TERMINATE:
			return "WTS_SESSION_TERMINATE";
		default:
			return "UNKNOW_MESSAGE";
	}
}

int main( int /*argc*/, const char** /*argv[]*/ ) {

	DBusError err;
	DBusConnection *conn;
	DBusMessage *msg;
	DBusMessageIter args;
	unsigned int notificationType;
	UINT32 sessionid;

	dbus_error_init(&err);
	conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Connection error (%s)\n", err.message);
		dbus_error_free(&err);
	}
	if (nullptr == conn) {
		exit(1);
	}

	dbus_bus_add_match(conn, "type='signal',interface='ogon.SessionManager.session.notification'", &err);
	dbus_connection_flush(conn);
	if (dbus_error_is_set(&err)) {
		fprintf(stderr, "Match error (%s)\n", err.message);
		exit(1);
	}
	printf("Started listening for ogon session notifications\n");

	while (true) {
		dbus_connection_read_write(conn, 0);
		msg = dbus_connection_pop_message(conn);
		sessionid = 0;

		if (nullptr == msg) {
			usleep(100);
			continue;
		}

		if (dbus_message_is_signal(msg, "ogon.SessionManager.session.notification", "SessionNotification")) {
			if (!dbus_message_iter_init(msg, &args)) {
				fprintf(stderr, "Message has no parameters\n");
				dbus_message_unref(msg);
				continue;
			} else if (DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type(&args)) {
				fprintf(stderr, "Argument1 is not UINT32!\n");
				dbus_message_unref(msg);
				continue;
			} else {
				dbus_message_iter_get_basic(&args, &notificationType);
			}


			dbus_message_iter_next(&args);

			if (DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type(&args)) {
				fprintf(stderr, "Argument is not UINT32!\n");
				dbus_message_unref(msg);
				continue;
			} else {
				dbus_message_iter_get_basic(&args, &sessionid);
			}

			printf("Got notification %s for session %" PRIu32 "\n",
				wtsNotificationToString(notificationType), sessionid);
		}
		dbus_message_unref(msg);
	}
}
