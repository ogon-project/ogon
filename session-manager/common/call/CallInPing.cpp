/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for ping rpc call (ogon to session manager)
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#include "CallInPing.h"

using ogon::icp::PingRequest;
using ogon::icp::PingResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallInPing::CallInPing() {
	}

	CallInPing::~CallInPing() {
	}

	unsigned long CallInPing::getCallType() const {
		return ogon::icp::Ping;
	}

	bool CallInPing::decodeRequest() {
		// decode protocol buffers
		PingRequest req;

		if (!req.ParseFromString(mEncodedRequest)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}
		return true;
	}

	bool CallInPing::encodeResponse() {
		// encode protocol buffers
		PingResponse resp;
		resp.set_pong(true);

		if (!resp.SerializeToString(&mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallInPing::doStuff() {
		return true;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
