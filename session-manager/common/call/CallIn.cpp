/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Baseclass for incoming rpc calls (ogon to session manager)
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

#include "CallIn.h"
#include <SBP.pb.h>
#include <string>
#include <memory>
#include <winpr/handle.h>
#include <appcontext/ApplicationContext.h>
#include <session/TaskCallIn.h>


namespace ogon { namespace sessionmanager { namespace call {

	static wLog *logger_CallIn = WLog_Get("ogon.sessionmanager.call.callin");

	CallIn::CallIn() {
	}

	CallIn::~CallIn() {
	}

	void CallIn::setEncodedRequest(const std::string &encodedRequest) {
		mEncodedRequest = encodedRequest;
	}

	std::string CallIn::getEncodedResponse() const {
		return mEncodedResponse;
	}

	std::shared_ptr<CallIn> CallIn::shared_from_this() {
		CallPtr call = Call::shared_from_this();
		return std::dynamic_pointer_cast<CallIn>(call);
	}

	bool CallIn::putInSessionExecutor_conId(UINT32 connectionId) {
		sessionNS::ConnectionPtr currentConnection = APP_CONTEXT.getConnectionStore()->getConnection(connectionId);
		if (currentConnection == NULL) {
			WLog_Print(logger_CallIn, WLOG_ERROR,
				"Cannot get Connection %" PRIu32 "", connectionId);
			goto abort;
		}

		if (!currentConnection->tryQueueCall(CallIn::shared_from_this())) {

			sessionNS::TaskCallInPtr asyncCallIn(new sessionNS::TaskCallIn());
			asyncCallIn->setCallIn(CallIn::shared_from_this());

			sessionNS::SessionPtr currentSession = APP_CONTEXT.getSessionStore()->getSession(currentConnection->getSessionId());

			// add it to the session
			if (currentSession) {
				if(!currentSession->addTask(asyncCallIn)) {
					WLog_Print(logger_CallIn, WLOG_ERROR,
						"Could not add call to session %" PRIu32 ", addTask failed!",
						currentSession->getSessionID());
				}
			} else {
				WLog_Print(logger_CallIn, WLOG_ERROR,
					"Could not add call for session with connection %" PRIu32 ", session not found!",
					connectionId);
				goto abort;
			}
		} else {
			WLog_Print(logger_CallIn, WLOG_TRACE,
				"Call queued on connection %" PRIu32 "", connectionId);
		}
		return true;
	abort:
		abort();
		return false;

	}

	bool CallIn::putInSessionExecutor_sesId(UINT32 sessionId) {

		sessionNS::SessionPtr currentSession = APP_CONTEXT.getSessionStore()->getSession(sessionId);
		sessionNS::TaskCallInPtr asyncCallIn;
		unsigned long calltype = -1;
		if (!currentSession) {
			WLog_Print(logger_CallIn, WLOG_TRACE,
				"Could not add call session with id %" PRIu32 ": session not found!",
				sessionId);
			goto abort;
		}

		calltype = getCallType();
		if (calltype >= 200 && calltype != ogon::sbp::VersionInfo &&
			!currentSession->isSBPVersionCompatible()) {
			WLog_Print(logger_CallIn, WLOG_TRACE,
				"SBP Versions for session %" PRIu32 " are either not compatible or where not sent!",
				sessionId);
			goto abort;
		}

		asyncCallIn = sessionNS::TaskCallInPtr(new sessionNS::TaskCallIn());
		asyncCallIn->setCallIn(CallIn::shared_from_this());
		if(!currentSession->addTask(asyncCallIn)) {
			WLog_Print(logger_CallIn, WLOG_ERROR,
				"Could not add call to session %" PRIu32 ", addTask failed!",
				currentSession->getSessionID());
		}
		return true;
	abort:
		abort();
		return false;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
