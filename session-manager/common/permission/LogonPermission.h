/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Logon permission class
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

#ifndef _OGON_SMGR_LOGONPERMISSION_H_
#define _OGON_SMGR_LOGONPERMISSION_H_

#include <string>

#include <boost/shared_ptr.hpp>

#include <winpr/wtypes.h>

namespace ogon { namespace sessionmanager { namespace permission {

	/**
	 * @brief
	 */
	class LogonPermission {
	public:
		LogonPermission(const std::string &username, const std::string &domain, DWORD permission);

		std::string getUsername() const;
		std::string getDomain() const;
		DWORD getPermission() const;

	private:
		std::string	mUsername;
		std::string mDomain;
		DWORD	mPermission;
	};

	typedef boost::shared_ptr<LogonPermission> LogonPermissionPtr;

} /*permission*/ } /*sessionmanager*/ } /*ogon*/

namespace permissionNS = ogon::sessionmanager::permission;

#endif /* _OGON_SMGR_LOGONPERMISSION_H_ */
