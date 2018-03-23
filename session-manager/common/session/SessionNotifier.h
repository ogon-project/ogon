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


#ifndef _OGON_SMGR_SESSION_SESSIONNOTIFIER_H_
#define _OGON_SMGR_SESSION_SESSIONNOTIFIER_H_

#include "Connection.h"

#include <string>
#include <winpr/synch.h>

#include <dbus/dbus.h>

namespace ogon { namespace sessionmanager { namespace session {

	class SessionNotifier {
	public:
		SessionNotifier();
		~SessionNotifier();

		bool init();
		bool notify(DWORD reason, UINT32 sessionId);
		bool shutdown();

	private:
		const char *wtsNotificationToString(int signal);
		DBusConnection *mDBusConn;
		CRITICAL_SECTION mCSection;
	};

} /*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* _OGON_SMGR_SESSION_SESSIONNOTIFIER_H_ */
