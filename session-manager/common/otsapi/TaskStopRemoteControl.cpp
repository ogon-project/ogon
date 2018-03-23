/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * otsapi task for stopping remote control (shadowing)
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

#include <otsapi/TaskStopRemoteControl.h>
#include <appcontext/ApplicationContext.h>
#include <call/CallOutOtsApiStopRemoteControl.h>
#include <session/Session.h>


namespace ogon { namespace sessionmanager { namespace otsapi {

	static wLog *logger_taskStopRemoteControl = WLog_Get("ogon.sessionmanager.otsapi.taskstopremotecontrol");


	TaskStopRemoteControl::TaskStopRemoteControl(UINT32 sessionId, UINT32 timeout):
		mSessionId(sessionId), mResult(false), mTimeOut(timeout) {
	}

	BOOL TaskStopRemoteControl::getResults() {
		return mResult;
	}

	void TaskStopRemoteControl::run() {

		mResult = false;

		sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(mSessionId);
		if (!session) {
			WLog_Print(logger_taskStopRemoteControl, WLOG_ERROR, "s %" PRIu32 ": No session found!", mSessionId);
			return;
		}

		if (session->getConnectState() != WTSShadow) {
			WLog_Print(logger_taskStopRemoteControl, WLOG_TRACE, "s %" PRIu32 ": Session was not in WTSShadow state!", mSessionId);
			return;
		}


		UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mSessionId);

		if (connectionId == 0) {
			WLog_Print(logger_taskStopRemoteControl, WLOG_TRACE, "s %" PRIu32 ": No connectionId found for session!", mSessionId);

			return;

		}

		callNS::CallOutOtsApiStopRemoteControlPtr stopRemoteControlCall(new callNS::CallOutOtsApiStopRemoteControl());
		stopRemoteControlCall->setConnectionId(connectionId);
		APP_CONTEXT.getRpcOutgoingQueue()->addElement(stopRemoteControlCall);
		DWORD status = WaitForSingleObject(stopRemoteControlCall->getAnswerHandle(), mTimeOut);
		if (status == WAIT_TIMEOUT) {
			WLog_Print(logger_taskStopRemoteControl, WLOG_TRACE, "s %" PRIu32 ": OtsApiStopRemoteContro timed out", mSessionId);
		} else {
			setAccessorSession(session);
			stopRemoteControl();
			mResult = true;
		}
	}

	void TaskStopRemoteControl::abortTask() {
		mResult = false;
		InformableTask::abortTask();
	}

} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/
