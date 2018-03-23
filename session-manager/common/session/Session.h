/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Session class
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

#ifndef _OGON_SMGR_SESSION_SESSION_H_
#define _OGON_SMGR_SESSION_SESSION_H_

#include <string>
#include <list>

#include <utils/SignalingQueue.h>
#include <task/Task.h>

#include <winpr/handle.h>
#include <winpr/wtsapi.h>
#include <winpr/wnd.h>
#include <boost/shared_ptr.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <ogon/module.h>


#include "Connection.h"
#include <module/Module.h>

#define TOKEN_DIR_PREFIX "/tmp/ogon.session."

namespace ogon { namespace sessionmanager { namespace session {

	class SessionAccessor;
	class SessionStore;

	/**
	 *	@brief
	 */
	class Session : public boost::enable_shared_from_this<Session> {
		friend class SessionAccessor;
		friend class SessionStore;

	public:
		/**
		 *
		 * @param sessionID
		 */
		Session(UINT32 sessionID);
		~Session();

		std::string getDomain() const;
		std::string getUserName() const;
		std::string getAuthUserName() const;
		std::string getAuthDomain() const;
		UINT32 getSessionID() const;
		std::string getPipeName() const;
		std::string getClientHostName() const;
		long getMaxXRes() const;
		long getMaxYRes() const;

		bool checkPermission(DWORD requestedPermissions);
		std::string getModuleConfigName() const;

		WTS_CONNECTSTATE_CLASS getConnectState() const;
		boost::posix_time::ptime getConnectStateChangeTime() const;

		std::string getToken() const;
		void storeCookies(std::string &ogonCookie, std::string &backendCookie) const;
		bool isCurrentModule(RDS_MODULE_COMMON *context) const;
		bool isVirtualChannelAllowed(const std::string &channelName);
		std::string getWinStationName() const;
		bool addTask(taskNS::TaskPtr task);

		bool stopExecutorThread(bool wait = false);

		boost::posix_time::ptime getConnectTime() const;
		boost::posix_time::ptime getDisconnectTime() const;
		boost::posix_time::ptime getLogonTime() const;

		std::list<UINT32> getShadowedByList() const;
		void addShadowedBy(UINT32 session);
		void removeShadowedBy(UINT32 session);
		void clearShadowedBy();

		bool isSBPVersionCompatible() const;

	private:

		/*private accessible through accessor*/
		void init();
		void shutdown();

		void setDomain(const std::string &domainName);
		void setUserName(const std::string &username);
		void setAuthUserName(const std::string &username);
		void setAuthDomain(const std::string &domainName);
		void setClientHostName(const std::string &clientHostName);
		bool generateUserToken();
		bool generateEnvBlockAndModify(const std::string &clientName, const std::string &clientAddress);
		bool generateAuthEnvBlockAndModify(const std::string &clientName, const std::string &clientAddress);

		void initPermissions();

		void setModuleConfigName(const std::string &configName);
		bool startModule(std::string &pipeName);
		bool connectModule();
		bool disconnectModule();
		bool stopModule();
		void setConnectState(WTS_CONNECTSTATE_CLASS state);

		bool markBackendAsAuth();
		void destroyAuthBackend();
		void restoreBackendFromAuth();
		void applyAuthTokenPermissions();
		void disconnect();
		void unregisterSession();
		void removeAuthToken();
		void startRemoteControl();
		void stopRemoteControl();
		void setSBPVersion(bool compatible);

	private:

		/* function not accessible through accessor*/
		char *dupEnv(char *orgBlock);
		void initMaxResolutions(char **envBlock,UINT32 sessionID);
		void registerSessionAndGetToken();
		bool setClientBPP(char **envBlock);

		void applyAuthToken();
		std::string getAuthTokenFileName() const;

		void applyEnvironmentFilterAndAdd(char **envBlock);
		bool checkEnvironmentPath(char **envBlock);
		static const char* sessionStateToString(WTS_CONNECTSTATE_CLASS state);
		void freeModuleContext(RDS_MODULE_COMMON *context);

		void parseAllowedChannels();

		bool startExecutorThread();
		static DWORD executorThread(void* arg);
		void endExecutorThread();
		void runExecutor();


		UINT32	mSessionID;
		bool	mSessionStarted;
		DWORD	mPermissions;
		long	mMaxXRes;
		long	mMaxYRes;
		boost::posix_time::ptime mConnectTime;
		boost::posix_time::ptime mDisconnectTime;
		boost::posix_time::ptime mLogonTime;

		std::string	mUsername;
		std::string	mAuthUserName;
		std::string	mDomain;
		std::string	mAuthDomain;

		std::string	mPipeName;
		std::string mBackendCookie;
		std::string mOgonCookie;

		HANDLE	mUserToken;
		char	*mpEnvBlock;

		std::string	mModuleConfigName;
		std::string	mModuleName;
		std::string	mAuthModelName;
		RDS_MODULE_COMMON	*mCurrentModuleContext;
		RDS_MODULE_COMMON	*mAuthModuleContext;
		WTS_CONNECTSTATE_CLASS	mCurrentState;
		boost::posix_time::ptime	mCurrentStateChangeTime;
		mutable CRITICAL_SECTION mCSection;
		mutable CRITICAL_SECTION mCSectionState;
		std::string	mAuthToken;
		uid_t	mUserUID;
		gid_t	mGroupUID;

		std::string	mClientHostName;

		std::list<std::string> mAllowedChannels;
		bool mAllowedChannelsParsed;
		std::string mWinStationName;

		// task executer
		HANDLE mhExecutorStopEvent;
		HANDLE mhExecutorThreadStartedEvent;
		HANDLE mhExecutorThread;
		SignalingQueue<taskNS::TaskPtr> mTasks;
		bool mExecutorRunning;

		std::list<UINT32> mShadowedBy;
		bool mSBPVersionCompatible;

		moduleNS::Module *mCurrentModule;
		moduleNS::Module *mCurrentAuthModule;
	};

	typedef boost::shared_ptr<Session> SessionPtr;

}/*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* _OGON_SMGR_SESSION_SESSION_H_ */
