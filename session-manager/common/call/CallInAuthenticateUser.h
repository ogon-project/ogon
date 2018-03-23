/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call AuthenticateUser (backend to session manager over SBP)
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

#ifndef _OGON_SMGR_CALL_CALLINAUTHENTICATEUSER_H_
#define _OGON_SMGR_CALL_CALLINAUTHENTICATEUSER_H_

#include <winpr/crt.h>

#include "CallFactory.h"
#include <string>
#include "CallIn.h"
#include <SBP.pb.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallInAuthenticateUser: public CallIn {
	public:
		CallInAuthenticateUser();
		virtual ~CallInAuthenticateUser();

		virtual unsigned long getCallType() const;
		virtual bool decodeRequest();
		virtual bool encodeResponse();
		virtual bool prepare();
		virtual bool doStuff();

	private:

		std::string mUserName;
		std::string mDomainName;
		std::string mPassword;

		int mAuthStatus;
		UINT32 mSessionId;
		std::string mPipeName;
	};

	FACTORY_REGISTER_DWORD(CallFactory, CallInAuthenticateUser, ogon::sbp::AuthenticateUser);

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_CALLINAUTHENTICATEUSER_H_ */
