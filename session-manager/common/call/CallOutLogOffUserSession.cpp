/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call LogOffUserSession (session manager to ogon)
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

#include "CallOutLogOffUserSession.h"
#include <appcontext/ApplicationContext.h>

using ogon::icp::LogoffUserSessionRequest;
using ogon::icp::LogoffUserSessionResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallOutLogOffUserSession::CallOutLogOffUserSession() {
		mConnectionId = 0;
		mLoggedOff = false;
	}

	CallOutLogOffUserSession::~CallOutLogOffUserSession() {
	}

	unsigned long CallOutLogOffUserSession::getCallType() const {
		return ogon::icp::LogoffUserSession;
	}

	bool CallOutLogOffUserSession::encodeRequest() {
		// decode protocol buffers
		LogoffUserSessionRequest req;
		req.set_connectionid(mConnectionId);

		if (!req.SerializeToString(&mEncodedRequest)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallOutLogOffUserSession::decodeResponse() {
		// encode protocol buffers
		LogoffUserSessionResponse resp;
		// stup do stuff here

		if (!resp.ParseFromString(mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}

		mLoggedOff = resp.loggedoff();
		return true;
	}

	void CallOutLogOffUserSession::setConnectionId(UINT32 connectionId) {
		mConnectionId = connectionId;
	}

	bool CallOutLogOffUserSession::isLoggedOff() {
		return mLoggedOff;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
