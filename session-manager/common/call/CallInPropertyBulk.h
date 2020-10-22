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

#ifndef _OGON_SMGR_CALL_CALLINPROPERTYBULK_H_
#define _OGON_SMGR_CALL_CALLINPROPERTYBULK_H_

#include "CallFactory.h"
#include <string>
#include <list>
#include "CallIn.h"
#include <ICP.pb.h>

#include <winpr/wtypes.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallInPropertyBulk: public CallIn {
	public:
		CallInPropertyBulk();
		virtual ~CallInPropertyBulk();

		virtual unsigned long getCallType() const;
		virtual bool decodeRequest();
		virtual bool encodeResponse();
		virtual bool prepare();
		virtual bool doStuff();


	private:
		struct BulkPropertyReq {
			BulkPropertyReq(const std::string &ppath, ::ogon::icp::EnumPropertyType t)
			: path(ppath), ptype(t), success(false), boolValue(false), intValue(0)
			{
			}

			std::string path;
			::ogon::icp::EnumPropertyType ptype;
			bool success;

			bool boolValue;
			int32_t intValue;
			std::string stringValue;

		};
		typedef std::list<BulkPropertyReq> RequestedProps;

		UINT32		mConnectionId;
		RequestedProps	mProps;
	};

	FACTORY_REGISTER_DWORD(CallFactory, CallInPropertyBulk, ogon::icp::PropertyBulk);

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_CALLINPROPERTYBULK_H_ */
