/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call StopRemoteControl (session manager to ogon)
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

#include <call/CallOutOtsApiStopRemoteControl.h>
#include <appcontext/ApplicationContext.h>

using ogon::icp::OtsApiStopRemoteControlRequest;
using ogon::icp::OtsApiStopRemoteControlResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallOutOtsApiStopRemoteControl::CallOutOtsApiStopRemoteControl() {
		mConnectionId = 0;
		mSuccess = false;
	}

	CallOutOtsApiStopRemoteControl::~CallOutOtsApiStopRemoteControl() {
	}

	unsigned long CallOutOtsApiStopRemoteControl::getCallType() const {
		return ogon::icp::OtsApiStopRemoteControl;
	}


	bool CallOutOtsApiStopRemoteControl::encodeRequest(){
		OtsApiStopRemoteControlRequest req;
		req.set_connectionid(mConnectionId);

		if (!req.SerializeToString(&mEncodedRequest)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallOutOtsApiStopRemoteControl::decodeResponse() {
		OtsApiStopRemoteControlResponse resp;

		if (!resp.ParseFromString(mEncodedResponse)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}
		mSuccess = resp.success();
		return true;
	}

	void CallOutOtsApiStopRemoteControl::setConnectionId(UINT32 connectionId) {
		mConnectionId = connectionId;
	}

	bool CallOutOtsApiStopRemoteControl::isSuccess() {
		return mSuccess;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
