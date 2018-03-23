/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call SwitchTo (session manager to ogon)
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

#include "CallOutSwitchTo.h"
#include <appcontext/ApplicationContext.h>

using ogon::icp::SwitchToRequest;
using ogon::icp::SwitchToResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallOutSwitchTo::CallOutSwitchTo() {
		mConnectionId = 0;
		mSuccess = false;
		mMaxWidth = 1920;
		mMaxHeight = 1200;
	}

	CallOutSwitchTo::~CallOutSwitchTo() {
	}

	unsigned long CallOutSwitchTo::getCallType() const{
		return ogon::icp::SwitchTo;
	}


	bool CallOutSwitchTo::encodeRequest(){
		SwitchToRequest req;
		req.set_connectionid(mConnectionId);
		req.set_serviceendpoint(mServiceEndpoint);
		req.set_maxwidth(mMaxWidth);
		req.set_maxheight(mMaxHeight);
		req.set_ogoncookie(mOgonCookie);
		req.set_backendcookie(mBackendCookie);

		if (!req.SerializeToString(&mEncodedRequest)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallOutSwitchTo::decodeResponse() {
		SwitchToResponse resp;

		if (!resp.ParseFromString(mEncodedResponse)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}
		mSuccess = resp.success();
		return true;
	}


	void CallOutSwitchTo::setConnectionId(UINT32 connectionId) {
		mConnectionId = connectionId;
	}

	void CallOutSwitchTo::setServiceEndpoint(const std::string &serviceEndpoint,
			const std::string &ogonCookie, const std::string &backendCookie)
	{
		mServiceEndpoint = serviceEndpoint;
		mOgonCookie = ogonCookie;
		mBackendCookie = backendCookie;
	}

	bool CallOutSwitchTo::isSuccess() {
		return mSuccess;
	}

	void CallOutSwitchTo::setMaxWidth(long width) {
		mMaxWidth = width;
	}

	void CallOutSwitchTo::setMaxHeight(long height) {
		mMaxHeight = height;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
