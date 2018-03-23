/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call CallOutOtsApiVirtualChannelClose (session manager to ogon)
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

#include <call/CallOutOtsApiVirtualChannelClose.h>
#include <appcontext/ApplicationContext.h>

using ogon::icp::OtsApiVirtualChannelCloseRequest;
using ogon::icp::OtsApiVirtualChannelCloseResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallOutOtsApiVirtualChannelClose::CallOutOtsApiVirtualChannelClose() {
		mConnectionID = 0;
		mInstance = 0;
		mSuccess = false;
	};

	CallOutOtsApiVirtualChannelClose::~CallOutOtsApiVirtualChannelClose() {
	};

	unsigned long CallOutOtsApiVirtualChannelClose::getCallType() const {
		return ogon::icp::OtsApiVirtualChannelClose;
	}


	bool CallOutOtsApiVirtualChannelClose::encodeRequest(){
		OtsApiVirtualChannelCloseRequest req;
		req.set_connectionid(mConnectionID);
		req.set_virtualname(mVirtualName);
		req.set_instance(mInstance);

		if (!req.SerializeToString(&mEncodedRequest)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallOutOtsApiVirtualChannelClose::decodeResponse() {
		OtsApiVirtualChannelCloseResponse resp;

		if (!resp.ParseFromString(mEncodedResponse)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}
		mSuccess = resp.success();
		return true;
	}


	void CallOutOtsApiVirtualChannelClose::setConnectionID(UINT32 connectionID) {
		mConnectionID = connectionID;
	}

	void CallOutOtsApiVirtualChannelClose::setVirtualName(const std::string &virtualName) {
		mVirtualName = virtualName;
	}

	bool CallOutOtsApiVirtualChannelClose::getSuccess() const {
		return mSuccess;
	}

	void CallOutOtsApiVirtualChannelClose::setInstance(DWORD instance){
		mInstance = instance;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
