/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task to disconnect a session.
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

#include "TaskDisconnect.h"
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>
#include <call/CallOutLogOffUserSession.h>

namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_TaskDisconnect = WLog_Get("ogon.sessionmanager.session.taskdisconnect");

	TaskDisconnect::TaskDisconnect(UINT32 connectionId, UINT32 sessionId):
		mConnectionId(connectionId), mSessionId(sessionId), mDisconnected(false),
		mResult(0) {
	}

	UINT32 TaskDisconnect::getResults(bool &disconnected) {
		disconnected = mDisconnected;
		return mResult;
	}

	UINT32 TaskDisconnect::disconnect_Connection() {
		sessionNS::ConnectionPtr currentConnection = APP_CONTEXT.getConnectionStore()->getConnection(mConnectionId);
		if (currentConnection == nullptr) {
			WLog_Print(logger_TaskDisconnect, WLOG_DEBUG,
				"No connection found for connectionId %" PRIu32 "!",
				mConnectionId);
			mDisconnected = false;
			return -1;
		}

		UINT32 sessionId = currentConnection->getSessionId();
		if (sessionId == 0) {
			WLog_Print(logger_TaskDisconnect, WLOG_DEBUG,
				"connection %" PRIu32 " has no session set!",
				mConnectionId);
			mDisconnected = false;
			return -1;
		}

		sessionNS::SessionPtr currentSession = APP_CONTEXT.getSessionStore()->getSession(sessionId);

		if (!currentSession) {
			WLog_Print(logger_TaskDisconnect, WLOG_DEBUG,
				"s %" PRIu32 ": session object not found for connection with id %" PRIu32 "!",
				sessionId,
				mConnectionId);
			mDisconnected = false;
			return -1;
		}

		APP_CONTEXT.getConnectionStore()->removeConnection(mConnectionId);

		if (currentSession->getConnectState() == WTSConnected) {
			// this session has to be terminated,
			// if WTSConnected: is an authsession, shutdown

			APP_CONTEXT.getSessionStore()->removeSession(currentSession->getSessionID());
			mDisconnected = true;
			return 0;
		}

		if (currentSession->getConnectState() != WTSActive
			&& currentSession->getConnectState() != WTSShadow) {
			mDisconnected = false;
			WLog_Print(logger_TaskDisconnect, WLOG_DEBUG,
				"s %" PRIu32 ": Session was not in WTSActive state! State was %u",
				sessionId, currentSession->getConnectState());
			return 0;
		}

		WLog_Print(logger_TaskDisconnect, WLOG_TRACE,
			"s %" PRIu32 ": Disconnecting session", sessionId);
		setAccessorSession(currentSession);
		disconnect();

		mDisconnected = true;
		return 0;
	}

	UINT32 TaskDisconnect::disconnect_Session() {

		sessionNS::SessionPtr currentSession = APP_CONTEXT.getSessionStore()->getSession(mSessionId);

		if (!currentSession) {
			WLog_Print(logger_TaskDisconnect, WLOG_DEBUG,
				"s %" PRIu32 ": Session not found!",
				mSessionId);
			mDisconnected = false;
			return -1;
		}

		UINT32 connectionID = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mSessionId);
		if (connectionID) {
			APP_CONTEXT.getConnectionStore()->removeConnection(connectionID);
		}

		if (currentSession->getConnectState() == WTSConnected) {
			// this session has to be terminated,
			// if WTSConnected: is an authsession, shutdown

			APP_CONTEXT.getSessionStore()->removeSession(mSessionId);
			mDisconnected = true;
			return 0;
		}

		if (currentSession->getConnectState() != WTSActive
			&& currentSession->getConnectState() != WTSShadow) {
			WLog_Print(logger_TaskDisconnect, WLOG_DEBUG,
				"s %" PRIu32 ": Session was not in WTSActive state! State was %u",
				mSessionId, currentSession->getConnectState());
			return 0;
		}

		WLog_Print(logger_TaskDisconnect, WLOG_TRACE,
			"s %" PRIu32 ": Disconnecting session", mSessionId);

		setAccessorSession(currentSession);
		disconnect();

		mDisconnected = true;
		return 0;
	}

	void TaskDisconnect::run() {
		if (mSessionId) {
			mResult = disconnect_Session();
		} else {
			mResult = disconnect_Connection();
		}

	}

	void TaskDisconnect::abortTask() {
		mResult = -1;
		InformableTask::abortTask();
	}
} /*session*/ } /*sessionmanager*/ } /*ogon*/
