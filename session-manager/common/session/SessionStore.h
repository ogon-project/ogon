/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Session store class
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

#ifndef _OGON_SMGR_SESSION_SESSIONSTORE_H_
#define _OGON_SMGR_SESSION_SESSIONSTORE_H_

#include "Session.h"

#include <string>
#include <list>
#include <winpr/synch.h>
#include <map>

namespace ogon { namespace sessionmanager { namespace session {

	typedef std::map<UINT32 , SessionPtr> TSessionMap;
	typedef std::pair<UINT32, SessionPtr> TSessionPair;

	/**
	 * @brief
	 */
	class SessionStore {
	public:
		SessionStore();
		~SessionStore();

		SessionPtr getSession(UINT32 sessionId);
		SessionPtr getFirstSession(const std::string &username, const std::string &domain);
		SessionPtr getFirstSession(const std::string &username, const std::string &domain,
				const std::string &clientHostName);
		SessionPtr getFirstDisconnectedSession(const std::string &username, const std::string &domain);
		SessionPtr getFirstDisconnectedSession(const std::string &username, const std::string &domain,
				const std::string &clientHostName);
		SessionPtr getFirstLoggedInSession(const std::string &username, const std::string &domain);
		SessionPtr getFirstLoggedInSession(const std::string &username, const std::string &domain,
										   const std::string &clientHostName);

		SessionPtr createSession();
		std::list<SessionPtr> getAllSessions();
		int removeSession(UINT32 sessionId);

	private:
		TSessionMap mSessionMap;
		UINT32 mNextSessionId;
		CRITICAL_SECTION mCSection;
	};

} /*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* _OGON_SMGR_SESSION_SESSIONSTORE_H_ */
