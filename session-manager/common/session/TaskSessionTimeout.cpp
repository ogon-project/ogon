/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task for Session Timout callback.
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

#include "TaskSessionTimeout.h"
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <session/TaskEnd.h>


namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_TaskSessionTimeout = WLog_Get("ogon.sessionmanager.session.tasksessiontimeout");

	bool TaskSessionTimeout::isThreaded() {
		return true;
	};

	void TaskSessionTimeout::run() {
		long timeout;
		DWORD status;
		for (;;) {
			status = WaitForSingleObject(mhStop, 10 * 1000);
			if (status == WAIT_OBJECT_0) {
				// shutdown
				return;
			}
			if (status == WAIT_TIMEOUT) {
				// check all session if they need to be disconnected.
				std::list<SessionPtr> allSessions = APP_CONTEXT.getSessionStore()->getAllSessions();
				boost::posix_time::ptime currentTime = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
				std::list<sessionNS::SessionPtr>::iterator iterator;
				for (iterator = allSessions.begin(); iterator != allSessions.end(); ++iterator) {
					sessionNS::SessionPtr currentSession = (*iterator);
					if (currentSession->getConnectState() == WTSDisconnected) {
						if (!APP_CONTEXT.getPropertyManager()->getPropertyNumber(currentSession->getSessionID(), "session.timeout", timeout)) {
							WLog_Print(logger_TaskSessionTimeout, WLOG_INFO,
								"session.timeout was not found for session %" PRIu32 ", using value of 0",
								currentSession->getSessionID());
							timeout = 0;
						}

						if ((timeout >= 0) && (((currentTime - currentSession->getConnectStateChangeTime()).ticks()) / ((boost::posix_time::time_duration::ticks_per_second() * 60)) >= timeout)) {
							// shutdown current Session
							WLog_Print(logger_TaskSessionTimeout, WLOG_INFO,
								"s %" PRIu32 ": Session for user %s is stopped after %ld minutes after the disconnect.",
								currentSession->getSessionID(),
								currentSession->getUserName().c_str(),
								timeout);
							sessionNS::TaskEndPtr task = sessionNS::TaskEndPtr(new sessionNS::TaskEnd());
							task->setSessionId(currentSession->getSessionID());
							currentSession->addTask(task);
						}
					}
				} // for loop of all sessions
			}// WAIT_TIMEOUT
		} // for loop
	}

} /*session*/ } /*sessionmanager*/ } /*ogon*/
