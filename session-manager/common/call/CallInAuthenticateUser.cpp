/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call AuthenticateUser (backend to session manager over SBP)
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

#include "CallInAuthenticateUser.h"
#include "TaskAuthenticateUser.h"

using ogon::sbp::AuthenticateUserRequest;
using ogon::sbp::AuthenticateUserResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallInAuthenticateUser::CallInAuthenticateUser() : mAuthStatus(0),
		mSessionId(0) {
	}

	CallInAuthenticateUser::~CallInAuthenticateUser() {
	}

	unsigned long CallInAuthenticateUser::getCallType() const {
		return ogon::sbp::AuthenticateUser;
	}

	bool CallInAuthenticateUser::decodeRequest() {
		// decode protocol buffers
		AuthenticateUserRequest req;

		if (!req.ParseFromString(mEncodedRequest)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}

		mUserName = req.username();
		mSessionId = req.sessionid();
		mDomainName = req.domain();
		mPassword = req.password();

		return true;
	}

	bool CallInAuthenticateUser::encodeResponse() {
		// encode protocol buffers
		AuthenticateUserResponse resp;
		// stup do stuff here

		switch (mAuthStatus) {
			case 0:
				resp.set_authstatus(ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_SUCCESSFUL);
				break;
			case 1:
				resp.set_authstatus(ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_BAD_CREDENTIALS);
				break;
			case 2:
				resp.set_authstatus(ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_WRONG_SESSION_STATE);
				break;
			default:
				resp.set_authstatus(ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_UNKNOWN_ERROR);
		}

		if (!resp.SerializeToString(&mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallInAuthenticateUser::doStuff() {
		TaskAuthenticateUser authenticateTask(mUserName, mDomainName, mPassword, mSessionId);
		authenticateTask.run();
		mResult = authenticateTask.getResult(mAuthStatus);
		return true;
	}

	bool CallInAuthenticateUser::prepare() {
		if (!putInSessionExecutor_sesId(mSessionId)) {
			mAuthStatus = 1;
			return false;
		}
		return true;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
