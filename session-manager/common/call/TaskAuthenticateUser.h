/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * call task for authenticating a user
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

#ifndef _OGON_SMGR_CALL_TASKCALLINAUTHENTICATEUSER_H_
#define _OGON_SMGR_CALL_TASKCALLINAUTHENTICATEUSER_H_

#include <task/Task.h>
#include <session/SessionAccessor.h>
#include <string>

namespace ogon { namespace sessionmanager { namespace call {

	enum AUTH_RESULT{
		AUTH_SUCCESSFUL = 0,
		AUTH_BAD_CREDENTIALS = 1,
		AUTH_WRONG_SESSION_STATE = 2,
		AUTH_UNKNOWN_ERROR = 3,
	};

	class TaskAuthenticateUser: public taskNS::Task, sessionNS::SessionAccessor {
	public:
		TaskAuthenticateUser(const std::string &username, const std::string &domainName,
			const std::string &password, UINT32 sessionid);
		virtual void run();
		int getResult(int &authStatus);

	private:

		int authenticateUser();
		int getUserSession();
		int switchToInSameSession(sessionNS::SessionPtr session);

		std::string mUserName;
		std::string mDomainName;
		std::string mPassword;
		UINT32 mSessionId;

		int mAuthStatus;
		uint32_t mResult;
	};

	typedef boost::shared_ptr<TaskAuthenticateUser> TaskAuthenticateUserPtr;

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_TASKCALLINAUTHENTICATEUSER_H_ */
