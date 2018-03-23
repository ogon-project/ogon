/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Connection store class
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


#ifndef _OGON_SMGR_SESSION_CONNECTIONSTORE_H_
#define _OGON_SMGR_SESSION_CONNECTIONSTORE_H_

#include "Connection.h"

#include <string>
#include <winpr/synch.h>
#include <map>

namespace ogon { namespace sessionmanager { namespace session {

	typedef std::map<UINT32 , ConnectionPtr> TConnectionMap;
	typedef std::pair<UINT32, ConnectionPtr> TConnectionPair;


	/**
	 *	@brief
	 */
	class ConnectionStore {
	public:
		ConnectionStore();
		~ConnectionStore();

		ConnectionPtr getOrCreateConnection(UINT32 connectionID);
		ConnectionPtr getConnection(UINT32 connectionID);
		ConnectionPtr getConnectionForSessionId(UINT32 mSessionId);
		int removeConnection(UINT32 connectionID);

		UINT32 getConnectionIdForSessionId(UINT32 mSessionId);

		void reset();

	private:
		TConnectionMap mConnectionMap;
		CRITICAL_SECTION mCSection;
	};

} /*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* _OGON_SMGR_SESSION_CONNECTIONSTORE_H_ */
