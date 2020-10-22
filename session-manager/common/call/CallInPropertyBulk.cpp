/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call GetPropertyBulk (ogon to session manager)
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * David Fort <contact@hardening-consulting.com>
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

#include "CallInPropertyBulk.h"
#include <appcontext/ApplicationContext.h>

using ogon::icp::PropertyBulkRequest;
using ogon::icp::PropertyBulkResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallInPropertyBulk::CallInPropertyBulk()
		: mConnectionId(0) {
	}

	CallInPropertyBulk::~CallInPropertyBulk() {
	}

	unsigned long CallInPropertyBulk::getCallType() const {
		return ogon::icp::PropertyBulk;
	}

	bool CallInPropertyBulk::decodeRequest() {
		// decode protocol buffers
		PropertyBulkRequest reqs;

		if (!reqs.ParseFromString(mEncodedRequest)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}

		mConnectionId = reqs.connectionid();

		for (int i = 0; i < reqs.properties_size(); i++) {
			const ::ogon::icp::PropertyReq & req = reqs.properties(i);
			mProps.push_back( BulkPropertyReq(req.propertypath(), req.propertytype()) );
		}

		return true;
	}

	bool CallInPropertyBulk::encodeResponse() {
		// encode protocol buffers
		PropertyBulkResponse resp;
		int i = 0;

		for (RequestedProps::const_iterator it = mProps.begin(); it != mProps.end(); ++it, i++) {
			::ogon::icp::PropertyValue *value = resp.add_results();

			value->set_success(it->success);
			if (it->success) {
				switch (it->ptype) {
				case ::ogon::icp::PROP_BOOL:
					value->set_boolvalue(it->boolValue);
					break;
				case ::ogon::icp::PROP_NUMBER:
					value->set_intvalue(it->intValue);
					break;
				case ::ogon::icp::PROP_STRING:
					value->set_stringvalue(it->stringValue);
					break;
				}
			}
		}

		if (!resp.SerializeToString(&mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallInPropertyBulk::doStuff() {
		UINT32 sessionId = 0;
		sessionNS::ConnectionPtr connection = APP_CONTEXT.getConnectionStore()->getConnection(mConnectionId);
		if (connection) {
			sessionId = connection->getSessionId();
		}

		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();
		for (RequestedProps::iterator it = mProps.begin(); it != mProps.end(); ++it) {

			switch (it->ptype) {
			case ::ogon::icp::PROP_BOOL:
				it->success = propertyManager->getPropertyBool(sessionId, it->path, it->boolValue);
				break;
			case ::ogon::icp::PROP_NUMBER: {
				long int longInt;
				it->success = propertyManager->getPropertyNumber(sessionId, it->path, longInt);
				if (it->success) {
					it->intValue = longInt;
				}
				break;
			}
			case ::ogon::icp::PROP_STRING:
				it->success = propertyManager->getPropertyString(sessionId, it->path, it->stringValue);
				break;
			}
		}
		return true;
	}

	bool CallInPropertyBulk::prepare() {
		doStuff();
		APP_CONTEXT.getRpcOutgoingQueue()->addElement(CallIn::shared_from_this());
		return true;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/

