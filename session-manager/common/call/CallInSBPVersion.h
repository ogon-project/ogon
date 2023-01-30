/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call SBP Version (ogon to session manager)
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

#ifndef OGON_SMGR_CALL_CALLINSBPVERSION_H_
#define OGON_SMGR_CALL_CALLINSBPVERSION_H_

#include "CallFactory.h"
#include <string>
#include "CallIn.h"
#include <SBP.pb.h>
#include <session/SessionAccessor.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallInSBPVersion: public CallIn, public ogon::sessionmanager::session::SessionAccessor {
	public:
		CallInSBPVersion();
		virtual ~CallInSBPVersion();

		virtual unsigned long getCallType() const;
		virtual bool decodeRequest();
		virtual bool encodeResponse();
		virtual bool prepare();
		virtual bool doStuff();

	private:
		UINT32 mSessionId;
		UINT32 mVersionMajor;
		UINT32 mVersionMinor;
	};

	FACTORY_REGISTER_DWORD(CallFactory, CallInSBPVersion, ogon::sbp::VersionInfo);

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* OGON_SMGR_CALL_CALLINSBPVERSION_H_ */
