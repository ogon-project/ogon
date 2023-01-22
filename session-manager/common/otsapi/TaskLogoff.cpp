/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * otsapi task for logging off a user
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

#include <otsapi/TaskLogoff.h>
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>
#include <call/CallOutLogOffUserSession.h>
#include <otsapi/TaskStopRemoteControl.h>

namespace ogon { namespace sessionmanager { namespace otsapi {

	static wLog *logger_TaskLogoff = WLog_Get("ogon.sessionmanager.otsapi.tasklogoff");

	TaskLogoff::TaskLogoff(UINT32 sessionId, BOOL wait, DWORD timeout):
		mSessionId(sessionId), mWait(wait), mTimeout(timeout),
		mLoggedoff(false) {
	}

	BOOL TaskLogoff::getResults() {
		return mLoggedoff;
	}

	BOOL TaskLogoff::logoff() {
		BOOL result = FALSE;

		sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(mSessionId);

		if (session == nullptr) {
			WLog_Print(logger_TaskLogoff, WLOG_ERROR, "s %" PRIu32 ": Session not found!", mSessionId);
			return false;
		}

		if (session->getConnectState() == WTSShadow ) {
			// end shadowing first
			TaskStopRemoteControl stop(mSessionId, mTimeout);
			stop.run();
			if (!stop.getResults()) {
				WLog_Print(logger_TaskLogoff, WLOG_ERROR,
					"s %" PRIu32 ": Shadowing could not be stopped! Continuing any way.",
					mSessionId);
			}
		}

		UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mSessionId);
		if (connectionId != 0)  {

			callNS::CallOutLogOffUserSessionPtr logoffCall(new callNS::CallOutLogOffUserSession());
			logoffCall->setConnectionId(connectionId);
			APP_CONTEXT.getRpcOutgoingQueue()->addElement(logoffCall);

			if (mWait) {
				DWORD status = WaitForSingleObject(logoffCall->getAnswerHandle(), mTimeout);
				if (status == WAIT_TIMEOUT) {
					WLog_Print(logger_TaskLogoff, WLOG_TRACE, "s %" PRIu32 ": LogOffUserSession timed out", mSessionId);
					result = FALSE;
				} else {
					result = logoffCall->isLoggedOff();
				}
			} else {
				result = TRUE;
			}
		} else {
			result = TRUE;
		}

		APP_CONTEXT.getSessionStore()->removeSession(mSessionId);

		return result;
	}

	void TaskLogoff::run() {
		mLoggedoff = logoff();
	}

	void TaskLogoff::abortTask() {
		mLoggedoff = false;
		InformableTask::abortTask();
	}

} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/
