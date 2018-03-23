/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Connection class
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

#ifndef _OGON_SMGR_SESSION_CONNECTION_H_
#define _OGON_SMGR_SESSION_CONNECTION_H_

#include <string>
#include <list>

#include <winpr/synch.h>
#include <boost/shared_ptr.hpp>
#include <config/PropertyLevel.h>
#include <call/CallIn.h>

namespace ogon { namespace sessionmanager { namespace session {



	typedef struct _CLIENT_INFORMATION {
		long width;
		long height;
		long initialWidth;
		long initialHeight;
		long colordepth;
		std::string clientHostName;
		std::string winStationName;
		UINT32 clientBuildNumber;
		UINT16 clientProductId;
		UINT32 clientHardwareId;
		std::string clientAddress;
		UINT16 clientProtocolType;
		~_CLIENT_INFORMATION() {};
	} CLIENT_INFORMATION, *pCLIENT_INFORMATION;

	typedef enum _CONNECTION_STATE
	{
		Connection_Init,
		Connection_Has_Session,
		Connection_Session_failed
	} CONNECTION_STATE;

	class Connection {
	public:
		Connection(UINT32 connectionId);
		~Connection();

		std::string getDomain();
		std::string getUserName();

		void setSessionId(UINT32 sessionId);
		UINT32 getSessionId();

		pCLIENT_INFORMATION getClientInformation();

		UINT32 getConnectionId();

		/* the domain can be modified by the authmodule*/
		int authenticateUser(const std::string &username, std::string &domain,
				const std::string &password, UINT32 sessionId = 0);
		void resetAuthenticatedUser();
		bool getProperty(const std::string &path, PROPERTY_STORE_HELPER &helper);

		bool tryQueueCall(callNS::CallInPtr call);
		std::list<callNS::CallInPtr> setStatusGetList(CONNECTION_STATE state);

	private:
		UINT32 mConnectionId;
		UINT32 mSessionId;

		int mAuthStatus;

		CONNECTION_STATE mConnectionState;

		CLIENT_INFORMATION mClientInformation;

		std::string mUsername;
		std::string mDomain;
		CRITICAL_SECTION mCSection;
		CRITICAL_SECTION mCSectionQueue;
		std::list<callNS::CallInPtr> mQueuedCalls;
	};

	typedef boost::shared_ptr<Connection> ConnectionPtr;

} /*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* _OGON_SMGR_SESSION_CONNECTION_H_ */
