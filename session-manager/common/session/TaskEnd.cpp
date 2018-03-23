/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task to end a session
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

#include "TaskEnd.h"
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>
#include <call/CallOutLogOffUserSession.h>


namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_TaskEnd = WLog_Get("ogon.sessionmanager.session.taskend");

	TaskEnd::TaskEnd() {
		mSessionId = 0;
		mSuccess = false;
	}
	void TaskEnd::run() {
		UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mSessionId);
		if (connectionId == 0) {
			// no connection found for this session ... just shut down!
			WLog_Print(logger_TaskEnd, WLOG_DEBUG,
				"s %" PRIu32 ": no connection found for session, just stopping session!",
				mSessionId);
			stopSession();
		} else {
			callNS::CallOutLogOffUserSessionPtr logoffSession(new callNS::CallOutLogOffUserSession());
			logoffSession->setConnectionId(connectionId);
			APP_CONTEXT.getRpcOutgoingQueue()->addElement(logoffSession);

			DWORD result = WaitForSingleObject(logoffSession->getAnswerHandle(), SHUTDOWN_TIME_OUT);

			switch (result) {
			case WAIT_OBJECT_0:
				if (logoffSession->getResult() == 0) {
					// no error
					if (logoffSession->isLoggedOff()) {
						WLog_Print(logger_TaskEnd, WLOG_DEBUG,
							"s %" PRIu32 ": logoff in ogon was successful!", mSessionId);
						mSuccess = true;
					} else {
						WLog_Print(logger_TaskEnd, WLOG_DEBUG,
							"s %" PRIu32 ": logoff in ogon was NOT successful!", mSessionId);
					}
				} else {
					// report error
					WLog_Print(logger_TaskEnd, WLOG_ERROR,
						"s %" PRIu32 ": ogon reported error %" PRIu32 "!",
						mSessionId, logoffSession->getResult());
				}

				break;
			case WAIT_TIMEOUT:
			case WAIT_FAILED:
				WLog_Print(logger_TaskEnd, WLOG_DEBUG,
					"s %" PRIu32 ": No answer in time, stop session any way!", mSessionId);
				break;
			}
			stopSession();
			APP_CONTEXT.getConnectionStore()->removeConnection(connectionId);
		}
	}

	void TaskEnd::setSessionId(UINT32 sessionId) {
		mSessionId = sessionId;
	}

	void TaskEnd::stopSession() {
		sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(mSessionId);
		if (session) {
			APP_CONTEXT.getSessionStore()->removeSession(mSessionId);
			mSuccess = true;
		} else {
			WLog_Print(logger_TaskEnd, WLOG_WARN,
				"s %" PRIu32 ": session not found!", mSessionId);
			mSuccess = false;
		}
	}

	bool TaskEnd::getResults() {
		return mSuccess;
	}

	void TaskEnd::abortTask() {
		mSuccess = false;
		InformableTask::abortTask();
	}


} /*session*/ } /*sessionmanager*/ } /*ogon*/
