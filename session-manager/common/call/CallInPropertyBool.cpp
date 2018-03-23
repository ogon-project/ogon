/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call GetPropertyBool (ogon to session manager)
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

#include "CallInPropertyBool.h"
#include <appcontext/ApplicationContext.h>

using ogon::icp::PropertyBoolRequest;
using ogon::icp::PropertyBoolResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallInPropertyBool::CallInPropertyBool()
		:mConnectionId(0), mFound(false), mValue(false) {
	}

	CallInPropertyBool::~CallInPropertyBool() {
	}

	unsigned long CallInPropertyBool::getCallType() const {
		return ogon::icp::PropertyBool;
	}

	bool CallInPropertyBool::decodeRequest() {
		// decode protocol buffers
		PropertyBoolRequest req;

		if (!req.ParseFromString(mEncodedRequest)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}

		mPropertyPath = req.path();
		mConnectionId = req.connectionid();
		return true;
	}

	bool CallInPropertyBool::encodeResponse() {
		// encode protocol buffers
		PropertyBoolResponse resp;
		resp.set_success(mFound);
		resp.set_value(mValue);

		if (!resp.SerializeToString(&mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallInPropertyBool::doStuff() {
		UINT32 sessionId = 0;
		sessionNS::ConnectionPtr connection = APP_CONTEXT.getConnectionStore()->getConnection(mConnectionId);
		if (connection) {
			sessionId = connection->getSessionId();
		}
		mFound = APP_CONTEXT.getPropertyManager()->getPropertyBool(sessionId, mPropertyPath, mValue);
		return true;
	}

	bool CallInPropertyBool::prepare() {
		doStuff();
		APP_CONTEXT.getRpcOutgoingQueue()->addElement(CallIn::shared_from_this());
		return true;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/

