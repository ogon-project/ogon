/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call DisconnectUserSession (ogon to session manager)
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

#include "CallInDisconnectUserSession.h"
#include <appcontext/ApplicationContext.h>
#include <session/TaskDisconnect.h>

using ogon::icp::DisconnectUserSessionRequest;
using ogon::icp::DisconnectUserSessionResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallInDisconnectUserSession::CallInDisconnectUserSession() {
		mConnectionId = 0;
		mDisconnected = false;
	}

	CallInDisconnectUserSession::~CallInDisconnectUserSession() {
	}

	unsigned long CallInDisconnectUserSession::getCallType() const {
		return ogon::icp::DisconnectUserSession;
	}

	bool CallInDisconnectUserSession::decodeRequest() {
		// decode protocol buffers
		DisconnectUserSessionRequest req;
		if (!req.ParseFromString(mEncodedRequest)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}
		mConnectionId = req.connectionid();
		return true;
	}

	bool CallInDisconnectUserSession::encodeResponse() {
		// encode protocol buffers
		DisconnectUserSessionResponse resp;
		// stup do stuff here

		resp.set_disconnected(mDisconnected);

		if (!resp.SerializeToString(&mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}

		return true;
	}

	bool CallInDisconnectUserSession::doStuff() {
		sessionNS::TaskDisconnect disconnectTask(mConnectionId);
		disconnectTask.run();
		mResult = disconnectTask.getResults(mDisconnected);
		return true;
	}

	bool CallInDisconnectUserSession::prepare() {
		if (!putInSessionExecutor_conId(mConnectionId)) {
			mDisconnected = false;
			return false;
		}
		return true;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
