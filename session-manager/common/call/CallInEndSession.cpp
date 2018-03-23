/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call CallInEndSession (service to session manager)
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

#include "CallInEndSession.h"
#include <session/TaskEnd.h>
#include <appcontext/ApplicationContext.h>

using ogon::sbp::EndSessionRequest;
using ogon::sbp::EndSessionResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallInEndSession::CallInEndSession() {
		mSessionId = 0;
		mSuccess = false;
	}

	CallInEndSession::~CallInEndSession() {
	}

	unsigned long CallInEndSession::getCallType() const {
		return ogon::sbp::EndSession;
	}

	bool CallInEndSession::decodeRequest() {
		// decode protocol buffers
		EndSessionRequest req;
		if (!req.ParseFromString(mEncodedRequest)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}
		mSessionId = req.sessionid();
		return true;
	}

	bool CallInEndSession::encodeResponse() {
		// encode protocol buffers
		EndSessionResponse resp;

		resp.set_success(mSuccess);

		if (!resp.SerializeToString(&mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallInEndSession::doStuff() {
		sessionNS::TaskEnd shutdown;

		shutdown.setSessionId(mSessionId);
		shutdown.run();

		mSuccess = shutdown.getResults();
		return true;
	}

	bool CallInEndSession::prepare() {
		if (!putInSessionExecutor_sesId(mSessionId)) {
			mSuccess = false;
			return false;
		}
		return true;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
