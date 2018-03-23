/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call SendMessage (session manager to ogon)
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

#include "CallOutMessage.h"
#include <appcontext/ApplicationContext.h>

using ogon::icp::MessageRequest;
using ogon::icp::MessageResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallOutMessage::CallOutMessage() {
		mConnectionId = 0;
		mType = 1;
		mStyle = 0;
		mResult = 0;
		mParameterNumber = 0;
		mTimeout = 0;
	};

	CallOutMessage::~CallOutMessage() {
	};

	unsigned long CallOutMessage::getCallType() const {
		return ogon::icp::Message;
	}

	bool CallOutMessage::encodeRequest() {
		// decode protocol buffers
		MessageRequest req;
		req.set_connectionid(mConnectionId);
		req.set_type(mType);
		req.set_parameternum(mParameterNumber);
		req.set_style(mStyle);
		req.set_timeout(mTimeout);
		switch (mParameterNumber) {
			case 5:
				req.set_parameter5(mParameter5);
				/*no break*/
			case 4:
				req.set_parameter4(mParameter4);
				/*no break*/
			case 3:
				req.set_parameter3(mParameter3);
				/*no break*/
			case 2:
				req.set_parameter2(mParameter2);
				/*no break*/
			case 1:
				req.set_parameter1(mParameter1);
				/*no break*/
			default:
				break;
		}

		if (!req.SerializeToString(&mEncodedRequest)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	};

	bool CallOutMessage::decodeResponse() {
		// encode protocol buffers
		MessageResponse resp;

		if (!resp.ParseFromString(mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}

		mResult = resp.result();
		return true;
	};

	void CallOutMessage::setConnectionId(UINT32 connectionId) {
		mConnectionId = connectionId;
	}

	void CallOutMessage::setType(UINT32 type) {
		mType = type;
	}

	void CallOutMessage::setParameterNumber(UINT32 number) {
		mParameterNumber = number;
	}

	void CallOutMessage::setParameter1(const std::string &param) {
		mParameter1 = param;
	}

	void CallOutMessage::setParameter2(const std::string &param) {
		mParameter2 = param;
	}

	void CallOutMessage::setParameter3(const std::string &param) {
		mParameter3 = param;
	}

	void CallOutMessage::setParameter4(const std::string &param) {
		mParameter4 = param;
	}

	void CallOutMessage::setParameter5(const std::string &param) {
		mParameter5 = param;
	}

	void CallOutMessage::setTimeout(UINT32 timeout) {
		mTimeout = timeout;
	}

	void CallOutMessage::setStyle(UINT32 style) {
		mStyle = style;
	}

	UINT32 CallOutMessage::getResult() const {
		return mResult;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
