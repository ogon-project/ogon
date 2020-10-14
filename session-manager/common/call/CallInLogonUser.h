/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for the LogonUser call.
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

#ifndef _OGON_SMGR_CALL_CALLINLOGONUSER_H_
#define _OGON_SMGR_CALL_CALLINLOGONUSER_H_

#include <winpr/crt.h>

#include "CallFactory.h"
#include <string>
#include "CallIn.h"
#include <ICP.pb.h>
#include <session/SessionAccessor.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallInLogonUser: public CallIn, sessionNS::SessionAccessor {
	public:
		CallInLogonUser();
		virtual ~CallInLogonUser();

		virtual unsigned long getCallType() const;
		virtual bool decodeRequest();
		virtual bool encodeResponse();
		virtual bool prepare();
		virtual bool doStuff();
		std::shared_ptr<CallInLogonUser> shared_from_this();
		void updateResult(uint32_t result, std::string pipeName, long maxHeight, long maxWidth, std::string backendCookie,
						  std::string ogonCookie);

	private:
		int authenticateUser();
		int getAuthSession();
		int getUserSession();
		sessionNS::SessionPtr createNewUserSession();

		std::string mUserName;
		std::string mDomainName;
		std::string mPassword;
		std::string mClientHostName;
		std::string mClientAddress;
		UINT32 mClientBuildNumber;
		UINT16 mClientProductId;
		UINT32 mClientHardwareId;
		UINT16 mClientProtocolType;

		long mWidth;
		long mMaxWidth;
		long mHeight;
		long mMaxHeight;
		long mColorDepth;

		int mAuthStatus;
		UINT32 mConnectionId;
		std::string mPipeName;
		std::string mOgonCookie;
		std::string mBackendCookie;
	};

	typedef std::shared_ptr<CallInLogonUser> CallInLogonUserPtr;

	FACTORY_REGISTER_DWORD(CallFactory, CallInLogonUser, ogon::icp::LogonUser);

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_CALLINLOGONUSER_H_ */
