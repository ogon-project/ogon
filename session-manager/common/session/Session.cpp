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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <string.h>
#include <pwd.h>
#include <fcntl.h>
#include <unistd.h>

#include <winpr/wlog.h>
#include <winpr/sspicli.h>
#include <winpr/environment.h>
#include <winpr/tchar.h>

#include <appcontext/ApplicationContext.h>
#include <utils/StringHelpers.h>
#include <permission/permission.h>

#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>
#include <session/TaskEnd.h>
#include <session/TaskShutdown.h>

#include "Session.h"

#define STD_PATH  "/usr/local/bin:/usr/bin:/bin"

namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_Session = WLog_Get("ogon.sessionmanager.session.session");

	Session::Session(UINT32 sessionID) : mSessionID(sessionID),
		mSessionStarted(false), mPermissions(0), mMaxXRes(0), mMaxYRes(0),
		mUserToken(NULL), mpEnvBlock(NULL), mCurrentModuleContext(NULL),
		mAuthModuleContext(NULL), mCurrentState(WTSInit), mUserUID(0),
		mGroupUID(0), mAllowedChannelsParsed(false),
		mhExecutorStopEvent(INVALID_HANDLE_VALUE),
		mhExecutorThreadStartedEvent(INVALID_HANDLE_VALUE),
		mhExecutorThread(NULL), mExecutorRunning(false),
		mSBPVersionCompatible(false), mCurrentModule(NULL), mCurrentAuthModule(NULL)
	{
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_Session, WLOG_FATAL,
				"Failed to initialize session critical section (mCSection)");
			throw std::bad_alloc();
		}

		if (!InitializeCriticalSectionAndSpinCount(&mCSectionState, 0x00000400)) {
			WLog_Print(logger_Session, WLOG_FATAL,
				"Failed to initialize session critical section (mCSectionState)");
			throw std::bad_alloc();
		}

		if (!(mhExecutorStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL))) {
			WLog_Print(logger_Session, WLOG_FATAL,
				"Failed to create session executor stop event");
			throw std::bad_alloc();
		}

		if (!(mhExecutorThreadStartedEvent = CreateEvent(NULL, TRUE, FALSE, NULL))) {
			WLog_Print(logger_Session, WLOG_FATAL,
				"Failed to create session executor thread started event");
			throw std::bad_alloc();
		}

		mLogonTime = boost::date_time::not_a_date_time;
		mConnectTime = boost::date_time::not_a_date_time;
		mDisconnectTime = boost::date_time::not_a_date_time;

		permissionNS::PermissionManager *permManager = APP_CONTEXT.getPermissionManager();
		mOgonCookie = permManager->genRandom(50);
		mBackendCookie = permManager->genRandom(50);
	}

	Session::~Session() {
		if (!stopExecutorThread()) {
			 WLog_Print(logger_Session, WLOG_FATAL,
				"stop of Executor Thread failed!");
		}

		if (mhExecutorThread) {
			CloseHandle(mhExecutorThread);
			mhExecutorThread = NULL;
		}
		CloseHandle(mhExecutorStopEvent);
		CloseHandle(mhExecutorThreadStartedEvent);
		DeleteCriticalSection(&mCSection);
		DeleteCriticalSection(&mCSectionState);

		if (mUserToken) {
			CloseHandle(mUserToken);
		}

		if (mpEnvBlock) {
			free(mpEnvBlock);
		}
	}

	void Session::initMaxResolutions(char **envBlock, UINT32 sessionID) {
		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();

		if (!propertyManager->getPropertyNumber(sessionID, "session.maxXRes", mMaxXRes)) {
			WLog_Print(logger_Session, WLOG_TRACE, "s %" PRIu32 ": Property session.maxXRes was not found, no maximum set!", sessionID);
		}

		if (!propertyManager->getPropertyNumber(sessionID, "session.maxYRes", mMaxYRes)) {
			WLog_Print(logger_Session, WLOG_TRACE, "s %" PRIu32 ": Property session.maxYRes was not found, no maximum set!", sessionID);
		}

		if ((0 != mMaxXRes) && (0 != mMaxYRes)) {
			std::string smax = boost::lexical_cast<std::string>(mMaxXRes);
			smax += "x";
			smax += boost::lexical_cast<std::string>(mMaxYRes);
			if (!SetEnvironmentVariableEBA(envBlock, "OGON_SMAX", smax.c_str())) {
				WLog_Print(logger_Session, WLOG_TRACE, "s %" PRIu32 ": Could not set OGON_SMAX in the envBlock", sessionID);
			}
		} else {
			WLog_Print(logger_Session, WLOG_TRACE, "s %" PRIu32 ": No OGON_SMAX set because of missing config!", sessionID);
		}
	}

	long Session::getMaxXRes() const {
		return mMaxXRes;
	}

	long Session::getMaxYRes() const {
		return mMaxYRes;
	}

	std::string Session::getDomain() const {
		return mDomain;
	}

	void Session::setDomain(const std::string &domainName) {
		mDomain = domainName;
	}

	std::string Session::getUserName() const {
		return mUsername;
	}

	void Session::setUserName(const std::string &username) {
		mUsername = username;
	}

	UINT32 Session::getSessionID() const {
		return mSessionID;
	}

	void Session::storeCookies(std::string &ogonCookie, std::string &backendCookie) const {
		ogonCookie = mOgonCookie;
		backendCookie = mBackendCookie;
	}

	std::string Session::getClientHostName() const {
		return mClientHostName;
	}

	void Session::setClientHostName(const std::string &clientHostName) {
		mClientHostName = clientHostName;
	}

	bool Session::generateUserToken() {
		if (mUsername.length() == 0) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed, no username!", mSessionID);
			return false;
		}

		if (mUserToken) {
			CloseHandle(mUserToken);
			mUserToken = NULL;
		}

		return LogonUserA(mUsername.c_str(), mDomain.c_str(), NULL, LOGON32_LOGON_INTERACTIVE,
				LOGON32_PROVIDER_DEFAULT, &mUserToken);
	}

	void Session::applyEnvironmentFilterAndAdd(char **envBlock) {
		std::string envFilterString;
		std::string envAddString;
		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();

		if (!propertyManager->getPropertyString(mSessionID, "environment.filter", envFilterString)) {
			WLog_Print(logger_Session, WLOG_TRACE, "s %" PRIu32 ": Property environment.filter was not found ... no filtering will apply!", mSessionID);
		}
		if (!propertyManager->getPropertyString(mSessionID, "environment.add", envAddString)) {
			WLog_Print(logger_Session, WLOG_TRACE, "s %" PRIu32 ": Property environment.add was not found ... no additional environment strings will be set.", mSessionID);
		}
		envFilterString = std::trim(envFilterString);
		if (envFilterString.compare("*") == 0) {
			*envBlock = GetEnvironmentStrings();
		} else if (envFilterString.size() > 0) {
			// we need to apply filter
			std::vector<std::string> envList = split<std::string>(envFilterString, ";");
			for (unsigned int index = 0; index < envList.size(); index++) {
				std::string envVarName = envList[index];
				if (envVarName.size() > 0) {
					char* env = getenv(envVarName.c_str());
					if (env) {
						// we found env variable ... use it
						if (!SetEnvironmentVariableEBA(envBlock, envVarName.c_str(), env)) {
							WLog_Print(logger_Session, WLOG_DEBUG,
									   "s %" PRIu32 ": Unable to set the additional environment string '%s' in the envBlock",
									   mSessionID, envVarName.c_str());

						}
					}
				}
			}
		}

		if (envAddString.size() > 0) {
			std::vector<std::string> envList = split<std::string>(envAddString, ";");
			for (unsigned int index = 0; index < envList.size(); index++) {
				std::string envVarName = envList[index];
				// find the first ":"
				std::size_t found = envVarName.find(':');
				if (found != std::string::npos) {
					if (!SetEnvironmentVariableEBA(envBlock, envVarName.substr(0, found).c_str(),
						envVarName.substr(found + 1).c_str()))
					{
						WLog_Print(logger_Session, WLOG_DEBUG,
							"s %" PRIu32 ": Could not set the aditional variable '%s' to the envBlock", mSessionID, envVarName.substr(0, found).c_str());
					}
				} else {
					WLog_Print(logger_Session, WLOG_DEBUG, "s %" PRIu32 ": Add environment variable should have the following form: <VariableName>:<Content>;", mSessionID);
				}
			}
		}
	}

	bool Session::checkEnvironmentPath(char **envBlock) {
		if (!GetEnvironmentVariableEBA(*envBlock, "PATH", NULL, 0)) {
			// did not find PATH, so set default
			if (!SetEnvironmentVariableEBA(envBlock, "PATH", STD_PATH)) {
				return false;
			}
		}
		return true;
	}

	bool Session::generateEnvBlockAndModify(const std::string &clientName, const std::string &clientAddress) {

		struct passwd* pwnam;
		char envstr[256];

		if (mUsername.length() == 0) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": no username!", mSessionID);
			return false;
		}

		CSGuard guard(&mCSection);

		if (mpEnvBlock) {
			free(mpEnvBlock);
			mpEnvBlock = NULL;
		}

		applyEnvironmentFilterAndAdd(&mpEnvBlock);
		if (!checkEnvironmentPath(&mpEnvBlock)) {
			return false;
		}
		pwnam = getpwnam(mUsername.c_str());

		if (!pwnam) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to get user information (getpwnam) for username %s!",
						mSessionID, mUsername.c_str());
			return false;
		}

		sprintf_s(envstr, sizeof(envstr), "%lu", (unsigned long) pwnam->pw_uid);
		if (!SetEnvironmentVariableEBA(&mpEnvBlock, "UID", envstr) ||
			!SetEnvironmentVariableEBA(&mpEnvBlock, "SHELL", pwnam->pw_shell) ||
			!SetEnvironmentVariableEBA(&mpEnvBlock, "USER", pwnam->pw_name) ||
			!SetEnvironmentVariableEBA(&mpEnvBlock, "HOME", pwnam->pw_dir) ||
			!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_COOKIE", mOgonCookie.c_str()) ||
			!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_BACKEND_COOKIE", mBackendCookie.c_str()))
		{
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set basic variables into the environment block", mSessionID);
			return false;
		}

		sprintf_s(envstr, sizeof(envstr), "%" PRIu32 "", mSessionID);
		if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_SID", envstr)) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_SID in the environment block", mSessionID);
			return false;
		}

		if (clientName.size()) {
			if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_SESSION_CLIENT_NAME", clientName.c_str())) {
				WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_SESSION_CLIENT_NAME in the environment block", mSessionID);
				return false;
			}
		}

		if (clientAddress.size()) {
			if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_SESSION_CLIENT_ADDRESS", clientAddress.c_str())) {
				WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_SESSION_CLIENT_ADDRESS in the environment block", mSessionID);
				return false;
			}
		}

		mUserUID = pwnam->pw_uid;
		mGroupUID = pwnam->pw_gid;

		initMaxResolutions(&mpEnvBlock, mSessionID);

		pbRPC::PeerCredentials creds;
		pbRPC::RpcEngine *icpEngine = APP_CONTEXT.getIcpEngine();

		if (icpEngine->getOgonCredentials(&creds)) {
			if (creds.haveUid) {
				sprintf_s(envstr, sizeof(envstr), "%lu", (unsigned long) creds.uid);
				if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_UID", envstr)) {
					WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_UID in the environment block", mSessionID);
					return false;
				}
			}

			/* OGON_PID is not set for sessions to support reconnection
			 * after a ogon restart (the pid of ogon would change).
			 * Perhaps the pid could be set when the session is not supposed to
			 * be reconnected...
			 */
		}

		return true;
	}

	bool Session::generateAuthEnvBlockAndModify(const std::string &clientName, const std::string &clientAddress) {
		CSGuard guard(&mCSection);

		char envstr[256];
		if (mpEnvBlock) {
			free(mpEnvBlock);
			mpEnvBlock = NULL;
		}

		applyEnvironmentFilterAndAdd(&mpEnvBlock);
		if (!checkEnvironmentPath(&mpEnvBlock)) {
			return false;
		}

		if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_COOKIE", mOgonCookie.c_str()) ||
				!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_BACKEND_COOKIE", mBackendCookie.c_str()))
		{
            WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_BACKEND_COOKIE in the environment block", mSessionID);
            return false;
		}

		if (mAuthUserName.length() != 0) {
			if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_USER", mAuthUserName.c_str())) {
                WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_USER in the environment block", mSessionID);
				return false;
			}
		}

		if (mAuthDomain.length() != 0) {
			if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_DOMAIN", mAuthDomain.c_str())) {
                WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_DOMAIN in the environment block", mSessionID);
				return false;
			}
		}

		sprintf_s(envstr, sizeof(envstr), "%" PRIu32 "", mSessionID);
		if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_SID", envstr)) {
            WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_SID in the environment block", mSessionID);
			return false;
		}

		if (clientName.size()) {
			if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_SESSION_CLIENT_NAME", clientName.c_str())) {
                WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_SESSION_CLIENT_NAME in the environment block", mSessionID);
				return false;
			}
		}

		if (clientAddress.size()) {
			if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_SESSION_CLIENT_ADDRESS", clientAddress.c_str())) {
                WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_SESSION_CLIENT_ADDRESS in the environment block", mSessionID);
				return false;
			}
		}

		initMaxResolutions(&mpEnvBlock, 0);

		pbRPC::PeerCredentials creds;
		pbRPC::RpcEngine *icpEngine = APP_CONTEXT.getIcpEngine();

		if (icpEngine->getOgonCredentials(&creds)) {
			if (creds.haveUid) {
				sprintf_s(envstr, sizeof(envstr), "%lu", (unsigned long) creds.uid);
				if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_UID", envstr)) {
                    WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_UID in the environment block", mSessionID);
                    return false;
				}
			}

			if (creds.havePid) {
				sprintf_s(envstr, sizeof(envstr), "%lu", (unsigned long) creds.pid);
				if (!SetEnvironmentVariableEBA(&mpEnvBlock, "OGON_PID", envstr)) {
                    WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": failed to set OGON_PID in the environment block", mSessionID);
					return false;
				}
			}
		}

		return true;
	}

	bool Session::setClientBPP(char **envBlock) {
		long colordepth;
		char colorstr[256];
		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();

		if (!propertyManager->getPropertyNumber(mSessionID, "current.connection.colordepth", colordepth)) {
			WLog_Print(logger_Session, WLOG_TRACE, "s %" PRIu32 ": Property current.connection.colordepth could not be received!", mSessionID);
			return false;
		}

		sprintf_s(colorstr, sizeof(colorstr), "%ld", colordepth);
		if (!SetEnvironmentVariableEBA(envBlock, "OGON_CONNECTION_BPP", colorstr)) {
			WLog_Print(logger_Session, WLOG_FATAL, "s %" PRIu32 ": Could not set OGON_CONNECTION_BPP in the environment block", mSessionID);
			return false;
		}

		return true;
	}

	void Session::setModuleConfigName(const std::string &configName) {
		mModuleConfigName = configName;
	}

	std::string Session::getModuleConfigName() const {
		return mModuleConfigName;
	}

	char *Session::dupEnv(char *orgBlock) {
		char *penvb = orgBlock;
		int length;
		int currentLength;
		char *lpszEnvironmentBlock;
		if (orgBlock == NULL) {
			return NULL;
		}

		length = 0;
		while (*penvb && *(penvb + 1)) {
			currentLength = strlen(penvb) + 1;
			length += currentLength;
			penvb += (currentLength);
		}

		lpszEnvironmentBlock = (char *)malloc((length + 2) * sizeof(char));
		if (!lpszEnvironmentBlock) {
            WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": malloc failed!", mSessionID);
            return NULL;
		}
		memcpy(lpszEnvironmentBlock, orgBlock, length + 1);

		return lpszEnvironmentBlock;
	}

	bool Session::startModule(std::string &pipeName) {
		CSGuard guard(&mCSection);

		ConnectionPtr connection;
		std::string pName;
		bool useLauncher;

		if (mSessionStarted) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": session has already be started, stop first.", mSessionID);
			return false;

		}

		if (!setClientBPP(&mpEnvBlock)) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": setClientBPP failed!", mSessionID);
			return false;
		}

		std::string configBaseName = std::string("module.") + mModuleConfigName;
		std::string queryString = configBaseName + std::string(".modulename");

		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();
		if (!propertyManager->getPropertyString(mSessionID, queryString, mModuleName)) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": Property %s not found.", mSessionID, queryString.c_str());
			return false;
		}

		queryString = configBaseName + std::string(".uselauncher");

		if (!propertyManager->getPropertyBool(0, queryString, useLauncher)) {
			WLog_Print(logger_Session, WLOG_TRACE, "s %" PRIu32 ": Property use_launcher not found, dont using launcher for now.", mSessionID);
			useLauncher = true;
		}

		mCurrentModule = APP_CONTEXT.getModuleManager()->getModule(mModuleName, useLauncher);

		if (!mCurrentModule) {
			WLog_Print(logger_Session, WLOG_ERROR,
				"s %" PRIu32 ": no module found for name %s", mSessionID, mModuleName.c_str());
			return false;
		}

		mCurrentModuleContext = mCurrentModule->newContext();
		if (!mCurrentModuleContext) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": could not create a new module context for name %s",
                       mSessionID, mModuleName.c_str());
			return false;
		}
		mCurrentModuleContext->sessionId = mSessionID;

		mCurrentModuleContext->userName = _strdup(mUsername.c_str());
		if (!mCurrentModuleContext->userName) {
			WLog_Print(logger_Session, WLOG_ERROR,
				"s %" PRIu32 ": _stdup failed for name %s", mSessionID, mModuleName.c_str());
			goto freeContext;
		}

		mCurrentModuleContext->userToken = mUserToken;
		mCurrentModuleContext->envBlock = dupEnv(mpEnvBlock);
		mCurrentModuleContext->baseConfigPath = _strdup(configBaseName.c_str());
		if (!mCurrentModuleContext->baseConfigPath) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": _stdup failed for name %s", mSessionID, configBaseName.c_str());
			goto freeUserName;
		}

		mCurrentModuleContext->domain = _strdup(mDomain.c_str());
		if (!mCurrentModuleContext->domain) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": _stdup failed for name %s", mSessionID, mDomain.c_str());
			goto freeDomainName;
		}

		connection = APP_CONTEXT.getConnectionStore()->getConnectionForSessionId(mSessionID);
		if (connection) {
			mCurrentModuleContext->remoteIp = _strdup(connection->getClientInformation()->clientAddress.c_str());
			if (!mCurrentModuleContext->remoteIp) {
				WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": _stdup failed for name %s", mSessionID,
						   connection->getClientInformation()->clientAddress.c_str());
				goto freeRemoteIp;
			}
		}

		pName = mCurrentModule->start(mCurrentModuleContext);

		if (pName.length() == 0) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": no pipeName was returned", mSessionID);
			mCurrentModule->stop(mCurrentModuleContext);
			goto freeAll;
		}

		pipeName = pName;
		mPipeName = pName;
		mSessionStarted = true;
		mWinStationName = mCurrentModule->getWinstationName(mCurrentModuleContext);
		return true;

	freeAll:
		free(mCurrentModuleContext->baseConfigPath);
	freeRemoteIp:
		free(mCurrentModuleContext->remoteIp);
	freeDomainName:
		free(mCurrentModuleContext->domain);
	freeUserName:
		free(mCurrentModuleContext->userName);
		if(mCurrentModuleContext->envBlock) {
			free(mCurrentModuleContext->envBlock);
		}
	freeContext:
		mCurrentModule->freeContext(mCurrentModuleContext);
		mCurrentModuleContext = NULL;
		return false;
	}

	bool Session::connectModule() {
		if (!mCurrentModule) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": no module loaded, call start first!", mSessionID);
			return false;
		}

		if (!mCurrentModuleContext) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": no modulecontext set for session, start module first!", mSessionID);
			return false;
		}

		mCurrentModule->connect(mCurrentModuleContext);
		return true;
	}

	bool Session::disconnectModule() {

		if (!mCurrentModule) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": no module loaded, call start first!", mSessionID);
			return false;
		}

		if (!mCurrentModuleContext) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": no modulecontext set for session, start module first!", mSessionID);
			return false;
		}

		mCurrentModule->disconnect(mCurrentModuleContext);
		return true;
	}


	bool Session::stopModule() {
		CSGuard guard(&mCSection);

		if (!mSessionStarted) {
			WLog_Print(logger_Session, WLOG_TRACE, "s %" PRIu32 ": session has not been started yet.", mSessionID);
			return false;
		}

		if (!mCurrentModule) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": no module loaded, call start first!", mSessionID);
			return false;
		}

		if (mCurrentModuleContext) {
			mCurrentModule->stop(mCurrentModuleContext);
			freeModuleContext(mCurrentModuleContext);
			mCurrentModule->freeContext(mCurrentModuleContext);
			mCurrentModuleContext = NULL;
		}
		mPipeName.clear();
		mSessionStarted = false;
		return true;
	}

	std::string Session::getPipeName() const {
		return mPipeName;
	}

	WTS_CONNECTSTATE_CLASS Session::getConnectState() const {
		CSGuard guard(&mCSectionState);
		return mCurrentState;
	}

	const char *Session::sessionStateToString(WTS_CONNECTSTATE_CLASS state) {
		switch (state) {
			case WTSActive:
				return "WTSActive";
			case WTSConnected:
				return "WTSConnected";
			case WTSConnectQuery:
				return "WTSConnectQuery";
			case WTSShadow:
				return "WTSShadow";
			case WTSDisconnected:
				return "WTSDisconnected";
			case WTSIdle:
				return "WTSIdle";
			case WTSListen:
				return "WTSListen";
			case WTSReset:
				return "WTSReset";
			case WTSDown:
				return "WTSDown";
			case WTSInit:
				return "WTSInit";
			default:
				break;
		}
		return "Unkown State";
	}



	/**
	 *
	 * 				WTSInit (session is created)
	 * 					|
	 * 					V
	 * 			----WTSConnected  (State if auth is not done, greeter)
	 * 			|		|
	 * 			|		V  <-----------------------------> WTSShadow
	 * 			|	WTSActive <----------------------
	 * 			|		A							|
	 * 			|		|					WTSConnectQuery
	 * 			|		V							A
	 * 			|	WTSDisconnected------------------
	 * 			|		|
	 * 			|		V
	 * 			--->WTSDown
	 *
	 */

	void Session::setConnectState(WTS_CONNECTSTATE_CLASS state) {

		if (state == mCurrentState) {
			return;
		}

		mCurrentStateChangeTime = boost::date_time::second_clock<boost::posix_time::ptime>::universal_time();
		sessionNS::SessionNotifier *sessionNotifier = APP_CONTEXT.getSessionNotifier();

		CSGuard guard(&mCSectionState);

		switch (state) {
			case WTSConnected:
				if (mCurrentState != WTSInit) {
					WLog_Print(logger_Session, WLOG_ERROR,
						"s %" PRIu32 ": wrong state transition, state WTSConnected should only be set if state was WTSInit, but was %s",
						mSessionID, sessionStateToString(mCurrentState));
				}
				mCurrentState = state;
				mConnectTime = mCurrentStateChangeTime;
				sessionNotifier->notify(WTS_REMOTE_CONNECT, mSessionID);
				guard.leaveGuard();
				connectModule();
				break;
			case WTSActive:
				if ((mCurrentState != WTSConnected) && (mCurrentState != WTSDisconnected)
						&& (mCurrentState != WTSConnectQuery) && (mCurrentState != WTSShadow)) {
					WLog_Print(logger_Session, WLOG_ERROR,
						"s %" PRIu32 ": wrong state transition from %s to WTSActive",
						mSessionID, sessionStateToString(mCurrentState));
				}
				{
					WTS_CONNECTSTATE_CLASS saveState = mCurrentState;
					mCurrentState = state;
					if (mLogonTime.is_not_a_date_time()) {
						mLogonTime = mCurrentStateChangeTime;
					}

					if (saveState == WTSConnected) {
						sessionNotifier->notify(WTS_SESSION_LOGON, mSessionID);
					} else if (saveState == WTSDisconnected) {
						sessionNotifier->notify(WTS_REMOTE_CONNECT, mSessionID);
						guard.leaveGuard();
						mConnectTime = mCurrentStateChangeTime;
						connectModule();
					} else if (saveState == WTSConnectQuery) {
						sessionNotifier->notify(WTS_REMOTE_CONNECT, mSessionID);
						guard.leaveGuard();
						mConnectTime = mCurrentStateChangeTime;
						connectModule();
					} else if (saveState == WTSShadow) {
						sessionNotifier->notify(WTS_SESSION_REMOTE_CONTROL, mSessionID);
					}
				}

				break;
			case WTSDisconnected:
				if ((mCurrentState != WTSActive)) {
					WLog_Print(logger_Session, WLOG_ERROR,
						"s %" PRIu32 ": wrong state transition, state WTSDisconnected should only be set if state was WTSActive but was %s",
						mSessionID, sessionStateToString(mCurrentState));
				}
				sessionNotifier->notify(WTS_REMOTE_DISCONNECT, mSessionID);
				mCurrentState = state;
				mDisconnectTime = mCurrentStateChangeTime;
				guard.leaveGuard();
				disconnectModule();
				break;
			case WTSDown:
				if ((mCurrentState != WTSDisconnected) && (mCurrentState != WTSConnected)) {
					WLog_Print(logger_Session, WLOG_ERROR,
						"s %" PRIu32 ": wrong state transition, state WTSDown should only be set if state was WTSDisconnected or WTSConnected but was %s",
						mSessionID, sessionStateToString(mCurrentState));
				}

				if (mCurrentState == WTSConnected) {
					sessionNotifier->notify(WTS_REMOTE_DISCONNECT, mSessionID);
				} else {
					sessionNotifier->notify(WTS_SESSION_LOGOFF, mSessionID);
				}

				mCurrentState = state;
				break;
			case WTSConnectQuery:
				mCurrentState = state;
				break;
			case WTSShadow:
				if ((mCurrentState != WTSActive)) {
					WLog_Print(logger_Session, WLOG_ERROR,
						"s %" PRIu32 ": wrong state transition, state WTSShadow should only be set if state was WTSActive but was %s",
						mSessionID, sessionStateToString(mCurrentState));
				}
				sessionNotifier->notify(WTS_SESSION_REMOTE_CONTROL, mSessionID);
				mCurrentState = state;
				break;
			default:
				WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": wrong state %d", mSessionID, mCurrentState);
				break;
		}
	}

	boost::posix_time::ptime Session::getConnectStateChangeTime() const {
		return mCurrentStateChangeTime;
	}

	void Session::registerSessionAndGetToken() {
		mAuthToken = APP_CONTEXT.getPermissionManager()->registerSession(shared_from_this());
	}

	void Session::unregisterSession() {
		APP_CONTEXT.getPermissionManager()->unregisterSession(shared_from_this());
	}

	std::string Session::getToken() const {
		return mAuthToken;
	}

	void Session::applyAuthToken() {
		FILE *f;
		int fd;

		std::string filename = getAuthTokenFileName();

		if ((fd = open(filename.c_str(), O_RDWR | O_CREAT, S_IRUSR)) >= 0) {
			f = fdopen(fd, "w");
			if(f != NULL) {
				fprintf(f, "%s", mAuthToken.c_str());
				fclose(f);
			} else {
				 WLog_Print(logger_Session, WLOG_ERROR,
					"s %" PRIu32 ": fdopen: cannot open tokenfile %s", mSessionID, filename.c_str());
			}
		} else {
			 WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": open: cannot create tokenfile %s",
				mSessionID, filename.c_str());
		}
	}

	void Session::applyAuthTokenPermissions(){
		std::string filename = getAuthTokenFileName();
		if (chown(filename.c_str(), mUserUID, mGroupUID)) {
			WLog_Print(logger_Session, WLOG_ERROR,	"s %" PRIu32 ": chown failed for file %s and userid = %ld and groupid = %ld",
				mSessionID, filename.c_str(), (long) mUserUID, (long) mGroupUID);
		}
	}

	std::string Session::getAuthTokenFileName() const {
		std::stringstream sstm;
		sstm << TOKEN_DIR_PREFIX << getSessionID();
		return sstm.str();
	}

	void Session::removeAuthToken() {
		std::remove(getAuthTokenFileName().c_str());
	}

	void Session::init() {
		registerSessionAndGetToken();
		applyAuthToken();
		if(!startExecutorThread()) {
			 WLog_Print(logger_Session, WLOG_FATAL,	"s %" PRIu32 ": start of Executor Thread failed!", mSessionID);
		}
	}

	void Session::shutdown() {
		TaskShutdownPtr shutdown(new TaskShutdown(shared_from_this()));
		addTask(shutdown);
	}


	bool Session::checkPermission(DWORD requestedPermissions) {
		return (mPermissions & requestedPermissions) == requestedPermissions ? true : false;
	}

	void Session::initPermissions() {
		std::string value;
		if (!APP_CONTEXT.getPropertyManager()->getPropertyString(mSessionID, "permission.level", value)) {
			mPermissions = WTS_PERM_FLAGS_GUEST;
		} else {
			if (boost::iequals(value,"FULL")) {
				mPermissions = WTS_PERM_FLAGS_FULL;
			} else if (boost::iequals(value,"USER")) {
				mPermissions = WTS_PERM_FLAGS_USER;
			} else {
				mPermissions = WTS_PERM_FLAGS_GUEST;
			}
		}
	}

	bool Session::markBackendAsAuth() {
		CSGuard guard(&mCSection);

		if (mCurrentState != WTSConnected) {
			WLog_Print(logger_Session, WLOG_FATAL,
				"s %" PRIu32 ": Awaited state is WTSConnected but state was %d", mSessionID, mCurrentState);
			return false;
		}
		mAuthModuleContext = mCurrentModuleContext;
		mAuthModelName = mModuleName;
		mCurrentAuthModule = mCurrentModule;
		mCurrentModuleContext = NULL;
		mPipeName.clear();
		mSessionStarted = false;
		return true;
	}

	void Session::restoreBackendFromAuth() {
		CSGuard guard(&mCSection);

		mCurrentModuleContext = mAuthModuleContext;
		mModuleName = mAuthModelName;
		mCurrentModule = mCurrentAuthModule;
		mAuthModuleContext = NULL;
		mCurrentAuthModule = NULL;
		mAuthModelName.clear();
		mSessionStarted = true;
		return;
	}

	void Session::destroyAuthBackend() {
		CSGuard guard(&mCSection);

		if (!mAuthModuleContext){
			return;
		}

		if (!mCurrentAuthModule) {
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": no module loaded, call start first!", mSessionID);
			return;
		}

		mCurrentAuthModule->disconnect(mAuthModuleContext);
		mCurrentAuthModule->stop(mAuthModuleContext);
		freeModuleContext(mAuthModuleContext);
		mCurrentAuthModule->freeContext(mAuthModuleContext);
		mAuthModuleContext = NULL;
	}

	bool Session::isCurrentModule(RDS_MODULE_COMMON *context) const {
		return mCurrentModuleContext == context;
	}

	void Session::freeModuleContext(RDS_MODULE_COMMON *context) {
		free(context->userName);
		free(context->envBlock);
		free(context->baseConfigPath);
		free(context->remoteIp);
		free(context->domain);
	}

	bool Session::isVirtualChannelAllowed(const std::string &channelName) {

		if (!mAllowedChannelsParsed) {
			parseAllowedChannels();
		}
		if (mAllowedChannels.size() < 1) {
			return false;
		}

		if (mAllowedChannels.front().compare("*") == 0) {
			return true;
		}

		std::list<std::string>::const_iterator iter;
		for (iter = mAllowedChannels.begin(); iter != mAllowedChannels.end();++iter ) {
			if (boost::iequals(channelName, *iter)) {
				return true;
			}
		}
		return false;
	}

	void Session::parseAllowedChannels() {

		mAllowedChannels.clear();
		std::string value;
		if (!APP_CONTEXT.getPropertyManager()->getPropertyString(mSessionID, "virtualChannel.whiteList", value)) {
			mAllowedChannels.push_front("*");
			mAllowedChannelsParsed = true;
			return;
		}

		std::vector<std::string> channelList = split<std::string>(value, ";");
		for (unsigned int index = 0; index < channelList.size(); index++) {
			std::string channelName = channelList[index];
			if (channelName.compare("*") == 0) {
				// if * is found remove all other values and push * to the front
				mAllowedChannels.clear();
				mAllowedChannels.push_front("*");
				mAllowedChannelsParsed = true;
				return;
			}
			mAllowedChannels.push_front(std::trim(channelName));
		}
		mAllowedChannelsParsed = true;
	}


	void Session::disconnect() {
		long timeout;
		bool reconnect;
		CSGuard guard(&mCSection);
		if (getConnectState() == WTSShadow) {
			setConnectState(WTSActive);
		}

		setConnectState(WTSDisconnected);
		disconnectModule();

		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();
		if (!propertyManager->getPropertyNumber(0, "session.timeout", timeout, mUsername)) {
			timeout = 0;
		}

		if (!propertyManager->getPropertyBool(0, "session.reconnect", reconnect, mUsername)) {
			reconnect = true;
		}

		if ((timeout == 0) || (!reconnect))  {
			sessionNS::TaskEnd task;
			task.setSessionId(mSessionID);
			task.run();
			setConnectState(WTSDown);
		}
	}

	std::string Session::getWinStationName() const {
		return mWinStationName;
	}

	boost::posix_time::ptime Session::getConnectTime() const {
		return mConnectTime;
	}

	boost::posix_time::ptime Session::getDisconnectTime() const {
		return mDisconnectTime;
	}

	boost::posix_time::ptime Session::getLogonTime() const {
		return mLogonTime;
	}

	bool Session::startExecutorThread() {

		CSGuard guard(&mCSection);

		if (mExecutorRunning) {
			WLog_Print(logger_Session, WLOG_ERROR,
				"s %" PRIu32 ": Executor Thread already running!", mSessionID);
			return false;
		}

		SessionPtr currentSession = shared_from_this();

		if (!(mhExecutorThread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) Session::executorThread, (void*) &currentSession,
				0, NULL)))
		{
			WLog_Print(logger_Session, WLOG_ERROR, "s %" PRIu32 ": faile to create thread", mSessionID);
			return false;
		}

		WaitForSingleObject(mhExecutorThreadStartedEvent, INFINITE);
		ResetEvent(mhExecutorThreadStartedEvent);
		mExecutorRunning = true;
		return true;
	}

	bool Session::stopExecutorThread(bool wait) {
		if (!mExecutorRunning) {
			return true;
		}

		if (mhExecutorThread) {
			SetEvent(mhExecutorStopEvent);
			if (wait) {
				WaitForSingleObject(mhExecutorThread, INFINITE);
			}
		}
		return true;
	}

	void Session::endExecutorThread() {
		CSGuard guard(&mCSection);

		mExecutorRunning = false;
		// Abort all outstanding tasks
		std::list<taskNS::TaskPtr> currentTasks = mTasks.getAllElements();
		std::list<taskNS::TaskPtr>::const_iterator iter;
		for(iter = currentTasks.begin(); iter != currentTasks.end(); ++iter) {
			taskNS::TaskPtr currentTask = *iter;
			currentTask->abortTask();
			currentTask.reset();
		}
		return;
	}


	DWORD Session::executorThread(void* arg) {
		SessionPtr session = (*(SessionPtr *) arg);
		WLog_Print(logger_Session, WLOG_INFO, "s %" PRIu32 ": started Executor thread", session->getSessionID());

		session->runExecutor();
		session->endExecutorThread();

		WLog_Print(logger_Session, WLOG_INFO, "s %" PRIu32 ": stopped Executor thread", session->getSessionID());
		return 0;
	}

	void Session::runExecutor() {

		SetEvent(mhExecutorThreadStartedEvent);

		DWORD nCount;
		HANDLE queueHandle = mTasks.getSignalHandle();
		HANDLE events[2];

		nCount = 0;
		events[nCount++] = mhExecutorStopEvent;
		events[nCount++] = queueHandle;

		while (1) {

			WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

			if (WaitForSingleObject(mhExecutorStopEvent, 0) == WAIT_OBJECT_0) {
				break;
			}

			if (WaitForSingleObject(queueHandle, 0) == WAIT_OBJECT_0) {
				std::list<taskNS::TaskPtr> currentTasks = mTasks.getAllElements();
				std::list<taskNS::TaskPtr>::const_iterator iter;
				for(iter = currentTasks.begin(); iter != currentTasks.end(); ++iter) {
					taskNS::TaskPtr currentTask = *iter;
					currentTask->preProcess();
					currentTask->run();
					currentTask->postProcess();
					currentTask.reset();
				}
			}
		}
	}

	bool Session::addTask(taskNS::TaskPtr task) {
		CSGuard guard(&mCSection);
		if (mExecutorRunning) {
			mTasks.addElement(task);
			return true;
		} else {
			task->abortTask();
			return false;
		}
	}

	void Session::startRemoteControl() {
		CSGuard guard(&mCSection);
		if (getConnectState() == WTSActive) {
			setConnectState(WTSShadow);
		}
	}

	void Session::stopRemoteControl() {
		CSGuard guard(&mCSection);
		if (getConnectState() == WTSShadow) {
			setConnectState(WTSActive);
		}
	}

	std::list<UINT32> Session::getShadowedByList() const {
		CSGuard guard(&mCSection);
		return mShadowedBy;

	}

	void Session::addShadowedBy(UINT32 session) {
		CSGuard guard(&mCSection);
		mShadowedBy.push_back(session);
	}

	void Session::removeShadowedBy(UINT32 session) {
		CSGuard guard(&mCSection);
		mShadowedBy.remove(session);

	}

	void Session::clearShadowedBy() {
		CSGuard guard(&mCSection);
		mShadowedBy.clear();
	}

	bool Session::isSBPVersionCompatible() const {
		return mSBPVersionCompatible;
	}

	void Session::setSBPVersion(bool compatible) {
		mSBPVersionCompatible = compatible;
	}

	void Session::setAuthUserName(const std::string &username) {
		mAuthUserName = username;
	}

	std::string Session::getAuthUserName() const {
		return mAuthUserName;
	}

	void Session::setAuthDomain(const std::string &domainName) {
		mAuthDomain = domainName;
	}

	std::string Session::getAuthDomain() const {
		return mAuthDomain;
	}



} /*session*/ } /*sessionmanager*/ } /*ogon*/
