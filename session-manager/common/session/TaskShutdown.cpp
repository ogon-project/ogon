/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task for shutting down a session (internally used by session)
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

#include "TaskShutdown.h"
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>


namespace ogon { namespace sessionmanager { namespace session {

	TaskShutdown::TaskShutdown(sessionNS::SessionPtr session) {
		setAccessorSession(session);
	}

	void TaskShutdown::run() {
		destroyAuthBackend();
		switch (getAccessorSession()->getConnectState()) {
			case WTSActive:
				setConnectState(WTSDisconnected);
				/* no break */
			case WTSConnected:
				setConnectState(WTSDown);
				break;
			case WTSDisconnected:
				setConnectState(WTSDown);
				break;
			default:
				break;
		}
		stopModule();

		unregisterSession();
		removeAuthToken();
		getAccessorSession()->stopExecutorThread(false);
	}

} /*session*/ } /*sessionmanager*/ } /*ogon*/
