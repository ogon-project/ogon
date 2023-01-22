/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * otsapi task for disconnecting a user
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

#include <otsapi/TaskDisconnect.h>
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>
#include <call/CallOutLogOffUserSession.h>
#include <otsapi/TaskStopRemoteControl.h>


namespace ogon { namespace sessionmanager { namespace otsapi {

	static wLog *logger_TaskDisconnect = WLog_Get("ogon.sessionmanager.otsapi.taskdisconnect");

	TaskDisconnect::TaskDisconnect(UINT32 sessionId, BOOL wait, DWORD timeout):
		mSessionId(sessionId), mWait(wait), mTimeout(timeout),
		mDisconnected(false) {
	}

	BOOL TaskDisconnect::getResults() {
		return mDisconnected;
	}

	BOOL TaskDisconnect::disconnectSession() {

		sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(mSessionId);

		if (session == nullptr) {
			WLog_Print(logger_TaskDisconnect, WLOG_ERROR, "s %" PRIu32 ": Session not found!", mSessionId);
			return false;
		}

		if (session->getConnectState() == WTSShadow ) {
			// end shadowing first
			TaskStopRemoteControl stop(mSessionId, mTimeout);
			stop.run();
			if (!stop.getResults()) {
				WLog_Print(logger_TaskDisconnect, WLOG_ERROR,
					"s %" PRIu32 ": Shadowing could not be stopped! Continuing any way.",
					mSessionId);
			}
		}

		UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mSessionId);
		if (connectionId == 0)  {
			WLog_Print(logger_TaskDisconnect, WLOG_INFO,
				"s %" PRIu32 ": Cannot notify ogon (connection for session missing)!",
				session->getSessionID());
			return true;
		}

		callNS::CallOutLogOffUserSessionPtr disconnectCall(new callNS::CallOutLogOffUserSession());
		disconnectCall->setConnectionId(connectionId);
		APP_CONTEXT.getRpcOutgoingQueue()->addElement(disconnectCall);

		if (mWait) {
			DWORD status = WaitForSingleObject(disconnectCall->getAnswerHandle(), mTimeout);
			if (status == WAIT_TIMEOUT) {
				WLog_Print(logger_TaskDisconnect, WLOG_DEBUG, "s %" PRIu32 ": timed out", mSessionId);
				return false;
			}
		}
		setAccessorSession(session);
		disconnect();

		APP_CONTEXT.getConnectionStore()->removeConnection(connectionId);

		return true;
	}

	void TaskDisconnect::run() {
		mDisconnected = disconnectSession();
	}

	void TaskDisconnect::abortTask() {
		mDisconnected = false;
		InformableTask::abortTask();
	}

} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/
