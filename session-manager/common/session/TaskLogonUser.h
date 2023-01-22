/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task for logging on a user
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

#ifndef OGON_SMGR_SESSION_TASKLOGONUSER_H_
#define OGON_SMGR_SESSION_TASKLOGONUSER_H_

#include <task/InformableTask.h>
#include <session/Connection.h>
#include <session/SessionAccessor.h>
#include <call/CallInLogonUser.h>

namespace ogon { namespace sessionmanager { namespace session {

	class TaskLogonUser: public taskNS::InformableTask, sessionNS::SessionAccessor {
	public:
		TaskLogonUser(UINT32 connectionId, UINT32 sessionID, const std::string &userName, const std::string &domainName,
			const std::string &password, const std::string &clientHostName,
			const std::string &clientAddress, UINT32 clientBuildNumber,
			UINT16 clientProductId, UINT32 clientHardwareId,
			UINT16 mClientProtocolType, long width, long height, long colorDepth,
			bool authSession, bool disconnectFirst, UINT32 logoffSession, ogon::sessionmanager::call::CallInLogonUserPtr call);
		virtual ~TaskLogonUser(){};
		virtual void run();
		UINT32 getResults(std::string &pipeName, long &maxWith, long &maxHeight,
			std::string &ogonCookie, std::string &backendCookie) const;
		virtual void abortTask();

	private:
		sessionNS::SessionPtr getAuthSession();
		sessionNS::SessionPtr getSession();
		void disconnectSession(sessionNS::SessionPtr session);
		void fetchQueuedCalls(sessionNS::ConnectionPtr currentConnection);

		UINT32 mConnectionId;
		UINT32 mSessionId;
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

		bool mCreateAuthSession;

		std::string mPipeName;
		std::string mOgonCookie;
		std::string mBackendCookie;
		UINT32 mResult;

		bool mDisconnectFirst;
		UINT32 mLogoffSession;
		ogon::sessionmanager::call::CallInLogonUserPtr mCurrentCall;

	};

	typedef std::shared_ptr<TaskLogonUser> TaskLogonUserPtr;

} /*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* OGON_SMGR_SESSION_TASKLOGONUSER_H_ */
