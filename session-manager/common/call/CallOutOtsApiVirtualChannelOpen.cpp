/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call OtsApiVirtualChannelOpen (session manager to ogon)
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

#include <call/CallOutOtsApiVirtualChannelOpen.h>
#include <appcontext/ApplicationContext.h>

using ogon::icp::OtsApiVirtualChannelOpenRequest;
using ogon::icp::OtsApiVirtualChannelOpenResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallOutOtsApiVirtualChannelOpen::CallOutOtsApiVirtualChannelOpen() {
		mConnectionID = 0;
		mInstance = 0;
		mDynamicChannel = false;
		mFlags = 0;
	}

	CallOutOtsApiVirtualChannelOpen::~CallOutOtsApiVirtualChannelOpen() {
	}

	unsigned long CallOutOtsApiVirtualChannelOpen::getCallType() const {
		return ogon::icp::OtsApiVirtualChannelOpen;
	}


	bool CallOutOtsApiVirtualChannelOpen::encodeRequest() {
		OtsApiVirtualChannelOpenRequest req;
		req.set_connectionid(mConnectionID);
		req.set_virtualname(mVirtualName);
		req.set_flags(mFlags);
		req.set_dynamicchannel(mDynamicChannel);

		if (!req.SerializeToString(&mEncodedRequest)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallOutOtsApiVirtualChannelOpen::decodeResponse() {
		OtsApiVirtualChannelOpenResponse resp;

		if (!resp.ParseFromString(mEncodedResponse)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}
		mConnectionString = resp.connectionstring();
		mInstance = resp.instance();
		return true;
	}

	void CallOutOtsApiVirtualChannelOpen::setConnectionID(UINT32 connectionID) {
		mConnectionID = connectionID;
	}

	void CallOutOtsApiVirtualChannelOpen::setVirtualName(const std::string &virtualName) {
		mVirtualName = virtualName;
	}

	std::string CallOutOtsApiVirtualChannelOpen::getConnectionString() const {
		return mConnectionString;
	}

	DWORD CallOutOtsApiVirtualChannelOpen::getInstance() const {
		return mInstance;
	}

	void CallOutOtsApiVirtualChannelOpen::setDynamicChannel(bool dynamic) {
		mDynamicChannel = dynamic;
	}

	void CallOutOtsApiVirtualChannelOpen::setFlags(DWORD flags){
		mFlags = flags;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
