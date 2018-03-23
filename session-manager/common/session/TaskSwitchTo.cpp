/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task for switching to another session
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

#include "TaskSwitchTo.h"
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>
#include <session/TaskDisconnect.h>
#include <call/CallOutSwitchTo.h>

namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_TaskSwitchTo = WLog_Get("ogon.sessionmanager.session.taskswitchto");

	TaskSwitchTo::TaskSwitchTo(UINT32 connectionId, UINT32 sessionId) : mConnectionId(connectionId),
		mSessionId(sessionId), mSuccess(false), mResult(0) {
	}

	UINT32 TaskSwitchTo::getResults(bool &success) {
		success = mSuccess;
		return mResult;
	}

	void TaskSwitchTo::run() {
		std::string ogonCookie;
		std::string backendCookie;

		sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(mSessionId);
		if (!session) {
			WLog_Print(logger_TaskSwitchTo, WLOG_DEBUG, "s %" PRIu32 ": Session not found!", mSessionId);
			mResult = 1;
			return;
		}
		UINT32 connectionID = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mSessionId);
		if (connectionID != 0) {
			sessionNS::TaskDisconnect disconnectTask(connectionID);
			disconnectTask.run();
		}

		session->storeCookies(ogonCookie, backendCookie);

		callNS::CallOutSwitchToPtr switchToCall(new callNS::CallOutSwitchTo());
		switchToCall->setServiceEndpoint(session->getPipeName(), ogonCookie, backendCookie);
		switchToCall->setConnectionId(mConnectionId);
		switchToCall->setMaxHeight(session->getMaxYRes());
		switchToCall->setMaxWidth(session->getMaxXRes());

		APP_CONTEXT.getRpcOutgoingQueue()->addElement(switchToCall);
		WaitForSingleObject(switchToCall->getAnswerHandle(), INFINITE);
		setAccessorSession(session);

		if (switchToCall->getResult() != 0) {
			WLog_Print(logger_TaskSwitchTo, WLOG_ERROR,
				"s %" PRIu32 ": answer: RPC error %" PRIu32 "!",
				mSessionId, switchToCall->getResult());
			setConnectState(WTSDisconnected);
			return;
		}

		mSuccess = switchToCall->isSuccess();
		if (!mSuccess) {
			WLog_Print(logger_TaskSwitchTo, WLOG_ERROR,
				"s %" PRIu32 ": switching in ogon failed!", mSessionId);
			setConnectState(WTSDisconnected);
			return;
		}

		sessionNS::ConnectionPtr connection = APP_CONTEXT.getConnectionStore()->getConnection(mConnectionId);
		if (connection != NULL) {
			connection->setSessionId(mSessionId);
		}
		setConnectState(WTSActive);
	}

	void TaskSwitchTo::abortTask() {
		mResult = -1;
		InformableTask::abortTask();
	}


} /*session*/ } /*sessionmanager*/ } /*ogon*/
