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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "LogonPermission.h"

namespace ogon { namespace sessionmanager { namespace permission {

	LogonPermission::LogonPermission(const std::string &username, const std::string &domain,
			DWORD permission)
	{
		mUsername = username;
		mDomain = domain;
		mPermission = permission;
	}

	std::string LogonPermission::getUsername() const {
		return mUsername;
	}

	std::string LogonPermission::getDomain() const {
		return mDomain;
	}

	DWORD LogonPermission::getPermission() const {
		return mPermission;
	}
} /*permission*/ } /*sessionmanager*/ } /*ogon*/

