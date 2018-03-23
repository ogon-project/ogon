/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call DisconnectUserSession (session manager to ogon)
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

#include "CallOutDisconnectUserSession.h"
#include <appcontext/ApplicationContext.h>

using ogon::icp::DisconnectUserSessionRequest;
using ogon::icp::DisconnectUserSessionResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallOutDisconnectUserSession::CallOutDisconnectUserSession() {
		mConnectionId = 0;
		mDisconnected = false;
	};

	CallOutDisconnectUserSession::~CallOutDisconnectUserSession() {

	};

	unsigned long CallOutDisconnectUserSession::getCallType() const {
		return ogon::icp::DisconnectUserSession;
	};

	bool CallOutDisconnectUserSession::encodeRequest() {
		// decode protocol buffers
		DisconnectUserSessionRequest req;
		req.set_connectionid(mConnectionId);

		if (!req.SerializeToString(&mEncodedRequest)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	};

	bool CallOutDisconnectUserSession::decodeResponse() {
		// encode protocol buffers
		DisconnectUserSessionResponse resp;
		// stup do stuff here

		if (!resp.ParseFromString(mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}

		mDisconnected = resp.disconnected();
		return true;
	};

	void CallOutDisconnectUserSession::setConnectionId(UINT32 connectionId) {
		mConnectionId = connectionId;
	}

	bool CallOutDisconnectUserSession::isDisconnected() {
		return mDisconnected;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
