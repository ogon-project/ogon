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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "TaskLogonUser.h"
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>
#include <call/CallOutLogOffUserSession.h>
#include <session/TaskEnd.h>


namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_TaskLogonUser = WLog_Get("ogon.sessionmanager.session.tasklogonuser");

	TaskLogonUser::TaskLogonUser(UINT32 connectionId, UINT32 sessionId, const std::string &userName,
			const std::string &domainName, const std::string &password, const std::string &clientHostName,
			const std::string &clientAddress, UINT32 clientBuildNumber,
		UINT16 clientProductId, UINT32 clientHardwareId,
		UINT16 clientProtocolType, long width, long height, long colorDepth,
		bool authSession, bool disconnectFirst, UINT32 logoffSession,
	 	ogon::sessionmanager::call::CallInLogonUserPtr call) : taskNS::InformableTask()
	{
		mConnectionId = connectionId;
		mSessionId = sessionId;
		mUserName = userName;
		mDomainName = domainName;
		mPassword = password;
		mClientHostName = clientHostName;
		mClientAddress = clientAddress;
		mClientBuildNumber = clientBuildNumber;
		mClientProductId = clientProductId;
		mClientHardwareId = clientHardwareId;
		mClientProtocolType = clientProtocolType;
		mWidth = width;
		mHeight = height;
		mMaxHeight = 0;
		mMaxWidth = 0;
		mColorDepth = colorDepth;
		mCreateAuthSession = authSession;
		mResult = 0;
		mDisconnectFirst = disconnectFirst;
		mLogoffSession = logoffSession;
		mCurrentCall = call;
	}

	UINT32 TaskLogonUser::getResults(std::string &pipeName, long &maxWith, long &maxHeight,
			std::string &ogonCookie, std::string &backendCookie) const
	{
		pipeName = mPipeName;
		maxWith = mMaxWidth;
		maxHeight = mMaxHeight;
		ogonCookie = mOgonCookie;
		backendCookie = mBackendCookie;
		return mResult;
	}

	void TaskLogonUser::run() {
		sessionNS::SessionPtr session;
		if (mCreateAuthSession) {
			session = getAuthSession();
		} else {
			session = getSession();
		}

		if (session) {
			session->storeCookies(mOgonCookie, mBackendCookie);
			mResult = 0;
		} else {
			mResult = 1;
		}
		// send result
		if (mCurrentCall) {
			mCurrentCall->updateResult(mResult, mPipeName, mMaxHeight, mMaxWidth, mBackendCookie, mOgonCookie);
			APP_CONTEXT.getRpcOutgoingQueue()->addElement(mCurrentCall);
		}
	}

	void TaskLogonUser::fetchQueuedCalls(sessionNS::ConnectionPtr currentConnection) {
		// fetch queued tasks from the connection object
		std::list<callNS::CallInPtr> list = currentConnection->setStatusGetList(
				Connection_Has_Session);
		std::list<callNS::CallInPtr>::iterator iterator;
		for (iterator = list.begin(); iterator != list.end(); ++iterator) {
			callNS::CallInPtr currentCall = (*iterator);
			currentCall->prepare();
		}
	}

	void TaskLogonUser::disconnectSession(sessionNS::SessionPtr session) {
		callNS::CallOutLogOffUserSessionPtr logoffSession(new callNS::CallOutLogOffUserSession());
		UINT32 sessionId = session->getSessionID();
		UINT32 connectionID = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(sessionId);
		logoffSession->setConnectionId(connectionID);

		APP_CONTEXT.getRpcOutgoingQueue()->addElement(logoffSession);

		DWORD result = WaitForSingleObject(logoffSession->getAnswerHandle(), SHUTDOWN_TIME_OUT);

		switch (result) {
		case WAIT_OBJECT_0:
			if (logoffSession->getResult() == 0) {
				// no error
				if (logoffSession->isLoggedOff()) {
					WLog_Print(logger_TaskLogonUser, WLOG_DEBUG,
						"s %" PRIu32 ": disconnect in ogon was successful!", sessionId);
				} else {
					WLog_Print(logger_TaskLogonUser, WLOG_ERROR,
						"s %" PRIu32 ": disconnect in ogon was not successful, but still removing connection!", sessionId);
				}
			} else {
				// report error
				WLog_Print(logger_TaskLogonUser, WLOG_ERROR,
					"s %" PRIu32 ": ogon reported error %" PRIu32 ", but still removing connection!",
					sessionId, logoffSession->getResult());
			}
			break;
		case WAIT_TIMEOUT:
		case WAIT_FAILED:
			WLog_Print(logger_TaskLogonUser, WLOG_ERROR,
				"s %" PRIu32 ": No answer in time, but still removing connection!", sessionId);
			break;
		}
		APP_CONTEXT.getConnectionStore()->removeConnection(connectionID);
		setAccessorSession(session);
		setConnectState(WTSDisconnected);
		resetAccessorSession();
	}

	sessionNS::SessionPtr TaskLogonUser::getSession() {

		pCLIENT_INFORMATION clientInformation;
		sessionNS::SessionStore *sessionStore = APP_CONTEXT.getSessionStore();
		sessionNS::ConnectionStore *connectionStore = APP_CONTEXT.getConnectionStore();

		sessionNS::ConnectionPtr currentConnection = connectionStore->getConnection(mConnectionId);
		if (!currentConnection) {
			WLog_Print(logger_TaskLogonUser, WLOG_ERROR,
				"s %" PRIu32 ": Connection with connectionId %" PRIu32 " was not found! Aborting logon!",
				mSessionId, mConnectionId);
			return sessionNS::SessionPtr();
		}

		if (mLogoffSession) {
			sessionNS::SessionPtr logoffSession = sessionStore->getSession(mLogoffSession);
			if (logoffSession) {
				sessionNS::TaskEndPtr shutdown(new sessionNS::TaskEnd());
				shutdown->setSessionId(mLogoffSession);
				logoffSession->addTask(shutdown);
				WaitForSingleObject(shutdown->getHandle(), INFINITE);
			}
		}

		sessionNS::SessionPtr currentSession;
		if (mSessionId) {
			currentSession = sessionStore->getSession(mSessionId);
			if (!currentSession) {
				WLog_Print(logger_TaskLogonUser, WLOG_ERROR,
					"s %" PRIu32 ": Session with sessionId %" PRIu32 " was not found! Aborting logon!", mSessionId, mSessionId);
				goto errorOut;
			}
			if (mDisconnectFirst) {
				UINT32 connectionID = connectionStore->getConnectionIdForSessionId(mSessionId);
				if (connectionID != 0) {
					WLog_Print(logger_TaskLogonUser, WLOG_INFO,
						"s %" PRIu32 ": Single session detected, disconnecting connection %" PRIu32 "!",
						mSessionId, connectionID);
					disconnectSession(currentSession);
				}
			}
		} else {
			WLog_Print(logger_TaskLogonUser, WLOG_ERROR,
				"For a Logon to a session a sessionId is needed, but was called with sessionId 0!");
			goto errorOut;
		}
		setAccessorSession(currentSession);

		currentConnection->setSessionId(currentSession->getSessionID());

		clientInformation = currentConnection->getClientInformation();
		mMaxWidth =  currentSession->getMaxXRes();
		if ((mMaxWidth != 0) && (mMaxWidth < mWidth)) {
			clientInformation->width = mMaxWidth;
			WLog_Print(logger_TaskLogonUser, WLOG_INFO,
					   "s %" PRIu32 ": width %ld exceeded maximum of %ld, setting %ld",
					   mSessionId, mWidth, mMaxWidth, mMaxWidth);

		} else {
			clientInformation->width = mWidth;
		}

		mMaxHeight = currentSession->getMaxYRes();
		if ((mMaxHeight != 0) && (mMaxHeight < mHeight)) {
			clientInformation->height = mMaxHeight;
			WLog_Print(logger_TaskLogonUser, WLOG_INFO,
					   "s %" PRIu32 ": height %ld exceeded maximum of %ld, setting %ld",
					   mSessionId, mHeight, mMaxHeight, mMaxHeight);
		} else {
			clientInformation->height = mHeight;
		}

		WLog_Print(logger_TaskLogonUser, WLOG_INFO,
				   "s %" PRIu32 ": Using resolution %ldx%ld",
				   mSessionId, clientInformation->width, clientInformation->height);

		clientInformation->colordepth = mColorDepth;
		clientInformation->clientHostName = mClientHostName;
		clientInformation->clientAddress = mClientAddress;
		clientInformation->clientBuildNumber = mClientBuildNumber;
		clientInformation->clientProductId = mClientProductId;
		clientInformation->clientHardwareId = mClientHardwareId;
		clientInformation->clientProtocolType = mClientProtocolType;
		clientInformation->initialWidth = mWidth;
		clientInformation->initialHeight = mHeight;

		if (currentSession->getConnectState() == WTSInit) {
			std::string pipeName;
			if (!startModule(pipeName)) {
				WLog_Print(logger_TaskLogonUser, WLOG_ERROR,
					"s %" PRIu32 ": ModuleConfig %s does not start properly for user %s in domain %s",
					mSessionId, currentSession->getModuleConfigName().c_str(),
					mUserName.c_str(), mDomainName.c_str());
				sessionStore->removeSession(currentSession->getSessionID());
				goto errorOut;
			}
			setConnectState(WTSConnected);
		}
		fetchQueuedCalls(currentConnection);
		setConnectState(WTSActive);

		mPipeName = currentSession->getPipeName();
		resetAccessorSession();
		return currentSession;

	errorOut:
		resetAccessorSession();
		currentConnection->setStatusGetList(Connection_Session_failed);
		return sessionNS::SessionPtr();
	}

	sessionNS::SessionPtr TaskLogonUser::getAuthSession() {
		// authentication failed, start up greeter module
		sessionNS::ConnectionPtr currentConnection = APP_CONTEXT.getConnectionStore()->getOrCreateConnection(mConnectionId);
		sessionNS::SessionStore *sessionStore = APP_CONTEXT.getSessionStore();
		sessionNS::SessionPtr currentSession = sessionStore->getSession(mSessionId);
		std::string greeter;
		pCLIENT_INFORMATION clientInformation;

		if (currentSession == nullptr) {
			WLog_Print(logger_TaskLogonUser, WLOG_ERROR, "s %" PRIu32 ": Could not get session with sessionID %" PRIu32 "", mSessionId, mSessionId);
			goto errorOut;
		}
		setAccessorSession(currentSession);

		if (!APP_CONTEXT.getPropertyManager()->getPropertyString(0, "auth.greeter", greeter, mUserName)) {
			WLog_Print(logger_TaskLogonUser, WLOG_INFO, "s %" PRIu32 ": Could not get attribute 'auth.greeter', using 'Qt' instead", mSessionId);
			greeter = "Qt";
		}

		WLog_Print(logger_TaskLogonUser, WLOG_INFO, "s %" PRIu32 ": Creating new auth session for client %s",
				   mSessionId, mClientHostName.c_str());

		setModuleConfigName(greeter);
		setAuthUserName(mUserName);
		setAuthDomain(mDomainName);
		setClientHostName(mClientHostName);

		initPermissions();

		if (!generateAuthEnvBlockAndModify(mClientHostName, mClientAddress)) {
			WLog_Print(logger_TaskLogonUser, WLOG_ERROR, "s %" PRIu32 ": generateEnvBlockAndModify failed for user %s with domain %s",
						mSessionId, mUserName.c_str(), mDomainName.c_str());
			goto errorOut;
		}

		currentConnection->setSessionId(currentSession->getSessionID());

		clientInformation = currentConnection->getClientInformation();
		mMaxWidth =  currentSession->getMaxXRes();
		if ((mMaxWidth != 0) && (mMaxWidth < mWidth)) {
			clientInformation->width = mMaxWidth;
		} else {
			clientInformation->width = mWidth;
		}

		mMaxHeight = currentSession->getMaxYRes();
		if ((mMaxHeight != 0) && (mMaxHeight < mHeight)) {
			clientInformation->height = mMaxHeight;
		} else {
			clientInformation->height = mHeight;
		}

		clientInformation->colordepth = mColorDepth;
		clientInformation->clientHostName = mClientHostName;
		clientInformation->clientAddress = mClientAddress;
		clientInformation->clientBuildNumber = mClientBuildNumber;
		clientInformation->clientProductId = mClientProductId;
		clientInformation->clientHardwareId = mClientHardwareId;
		clientInformation->clientProtocolType = mClientProtocolType;
		clientInformation->initialWidth = mWidth;
		clientInformation->initialHeight = mHeight;

		if (!startModule(greeter)) {
			WLog_Print(logger_TaskLogonUser, WLOG_ERROR,
					   "s %" PRIu32 ": ModuleConfig %s does not start properly for user %s in domain %s",
					   mSessionId, currentSession->getModuleConfigName().c_str(),
					   mUserName.c_str(), mDomainName.c_str());
			goto errorOut;
		}
		fetchQueuedCalls(currentConnection);
		// fetch queued tasks from the connection object
		setConnectState(WTSConnected);

		mPipeName = currentSession->getPipeName();
		resetAccessorSession();
		return currentSession;

	errorOut:
		sessionStore->removeSession(mSessionId);
		resetAccessorSession();
		currentConnection->setStatusGetList(Connection_Session_failed);
		return sessionNS::SessionPtr();

	}

	void TaskLogonUser::abortTask() {
		mResult = -1;
		if (mCurrentCall) {
			mCurrentCall->updateResult(1, "", 1024, 800, "", "");
			APP_CONTEXT.getRpcOutgoingQueue()->addElement(mCurrentCall);
		}
		InformableTask::abortTask();
	}

} /*session*/ } /*sessionmanager*/ } /*ogon*/
