/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for ping rpc call (session manager to ogon)
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

#include "CallOutPing.h"

using ogon::icp::PingRequest;
using ogon::icp::PingResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallOutPing::CallOutPing() {
		mPong = false;
	}

	CallOutPing::~CallOutPing(){
	}

	unsigned long CallOutPing::getCallType() const {
		return ogon::icp::Ping;
	}

	bool CallOutPing::encodeRequest() {
		// decode protocol buffers
		PingRequest req;

		if (!req.SerializeToString(&mEncodedRequest)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallOutPing::decodeResponse() {
		// encode protocol buffers
		PingResponse resp;

		if (!resp.ParseFromString(mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		mPong = resp.pong();
		return true;
	}

	bool CallOutPing::getPong() {
		return mPong;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
