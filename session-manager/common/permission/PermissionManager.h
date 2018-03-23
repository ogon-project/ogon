/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Permission Manager class
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

#ifndef _OGON_SMGR_PERMISSIONMANAGER_H_
#define _OGON_SMGR_PERMISSIONMANAGER_H_

#include <string>
#include <map>

#include <winpr/wlog.h>
#include <utils/CSGuard.h>
#include <session/Session.h>
#include "LogonPermission.h"

namespace ogon { namespace sessionmanager { namespace permission {

	typedef std::map<std::string , bool> TGroupsMap;

	typedef std::map<std::string , sessionNS::SessionPtr> TSessionMap;
	typedef std::pair<std::string, sessionNS::SessionPtr> TSessionPair;

	typedef std::map<std::string , LogonPermissionPtr> TLogonPermissionMap;
	typedef std::pair<std::string, LogonPermissionPtr> TLogonPermissionPair;

	/**
	 * @brief
	 */
	class PermissionManager {
	public:
		PermissionManager();
		~PermissionManager();

		std::string registerSession(sessionNS::SessionPtr session);
		int unregisterSession(sessionNS::SessionPtr session);

		std::string registerLogon(const std::string &username, const std::string &domain, DWORD permission);
		int unregisterLogon(const std::string &token);
		LogonPermissionPtr getPermissionForLogon(const std::string &token);

		sessionNS::SessionPtr getSessionForToken(const std::string &token);
		sessionNS::SessionPtr getSessionForTokenAndPermission(const std::string &token, DWORD permission);

		bool isLogonAllowedForUser(const std::string &username);
		void reloadAllowedUsers();

		/** generates a random string that has the given length
		 * @param length the length to generate
		 * @return a random string
		 */
		std::string genRandom(int length = 10);

	private:
		void removeAuthTokens();
		std::list<std::string> getGroupList(const std::string &username);
		TSessionMap mSessionMap;
		TLogonPermissionMap mLogonPermissionMap;
		CRITICAL_SECTION mCSection;
		TGroupsMap mGroupsMap;
		bool mUnknownGroupsLogonAllowed;

		std::string mRandomBucket;
	};

} /*permission*/ } /*sessionmanager*/ } /*ogon*/

namespace permissionNS = ogon::sessionmanager::permission;

#endif /* _OGON_SMGR_PERMISSIONMANAGER_H_ */
