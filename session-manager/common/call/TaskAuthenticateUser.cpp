/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * call task for authenticating a user
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

#include "TaskAuthenticateUser.h"
#include <winpr/wlog.h>
#include <permission/permission.h>
#include <appcontext/ApplicationContext.h>
#include <call/CallOutSwitchTo.h>
#include <session/Session.h>
#include <otsapi/TaskDisconnect.h>
#include <session/TaskSwitchTo.h>
#include <session/TaskEnd.h>
#include <SBP.pb.h>

namespace ogon { namespace sessionmanager { namespace call {

	static wLog *logger_TaskAuthenticateUser = WLog_Get("ogon.sessionmanager.call.taskauthenticateuser");

	TaskAuthenticateUser::TaskAuthenticateUser(const std::string &username,	const std::string &domainName,
			const std::string &password, UINT32 sessionid) :
		mUserName(username), mDomainName(domainName), mPassword(password),
		mSessionId(sessionid), mAuthStatus(ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_UNKNOWN_ERROR), mResult(0)
	{
	}

	void TaskAuthenticateUser::run() {
		if (authenticateUser() == ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_SUCCESSFUL) {
			getUserSession();
		}
	}

	int TaskAuthenticateUser::getResult(int &authStatus) {
		authStatus = mAuthStatus;
		return mResult;
	}

	int TaskAuthenticateUser::authenticateUser() {
		sessionNS::ConnectionStore *connectionStore = APP_CONTEXT.getConnectionStore();

		sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(mSessionId);

		if (!session) {
			WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR,
					   "s %" PRIu32 ": Cannot get session object", mSessionId);
			mAuthStatus = ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_UNKNOWN_ERROR;
			return -1;
		}

		if (session-> getConnectState() != WTSConnected) {
			WLog_Print(logger_TaskAuthenticateUser, WLOG_WARN,
					   "s %" PRIu32 ": session is in wrong session state, or its not a greeter session ", mSessionId);
			mAuthStatus = ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_WRONG_SESSION_STATE;
			return -1;
		}

		UINT32 connectionId = connectionStore->getConnectionIdForSessionId(mSessionId);
		sessionNS::ConnectionPtr currentConnection = connectionStore->getConnection(connectionId);
		if (currentConnection == nullptr) {
			WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR,
				"s %" PRIu32 ": Cannot get connection object (%" PRIu32 ")",
				mSessionId, connectionId);
			mAuthStatus = ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_UNKNOWN_ERROR;
			return -1;
		}

		mAuthStatus = currentConnection->authenticateUser(mUserName, mDomainName, mPassword);
		if (mAuthStatus == -1) mAuthStatus = ogon::sbp::AuthenticateUserResponse_AUTH_STATUS_AUTH_BAD_CREDENTIALS;
		return mAuthStatus;
	}

	int TaskAuthenticateUser::switchToInSameSession(sessionNS::SessionPtr session) {
		UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mSessionId);

		std::string ogonCookie;
		std::string backendCookie;
		session->storeCookies(ogonCookie, backendCookie);

		CallOutSwitchToPtr switchToCall(new CallOutSwitchTo());
		switchToCall->setServiceEndpoint(session->getPipeName(), ogonCookie, backendCookie);
		switchToCall->setConnectionId(connectionId);
		switchToCall->setMaxHeight(session->getMaxYRes());
		switchToCall->setMaxWidth(session->getMaxXRes());

		APP_CONTEXT.getRpcOutgoingQueue()->addElement(switchToCall);
		WaitForSingleObject(switchToCall->getAnswerHandle(), INFINITE);

		if (switchToCall->getResult() != 0) {
			WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR, "s %" PRIu32 ": answer: RPC error %" PRIu32 "!",
				mSessionId, switchToCall->getResult());
			mResult = 1;
			return 1;
		}

		if (!switchToCall->isSuccess()) {
			WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR, "s %" PRIu32 ": switching in ogon failed!", mSessionId);
			mResult = 1;
			return 1;
		}

		destroyAuthBackend();
		return 0;
	}

	int TaskAuthenticateUser::getUserSession() {

		sessionNS::SessionPtr currentSession;
		std::string clientHostName;
		bool reconnectAllowed = true;
		bool reconnectClientHostName = false;
		bool singleSession = false;
		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();
		sessionNS::SessionStore *sessionStore = APP_CONTEXT.getSessionStore();
		sessionNS::ConnectionStore *connectionStore = APP_CONTEXT.getConnectionStore();
		sessionNS::ConnectionPtr currentConnection = connectionStore->getConnectionForSessionId(mSessionId);

		propertyManager->getPropertyBool(0, "session.reconnect", reconnectAllowed, mUserName);
		propertyManager->getPropertyBool(0, "session.reconnect.fromSameClient", reconnectClientHostName, mUserName);
		propertyManager->getPropertyBool(0, "session.singleSession", singleSession, mUserName);

		sessionNS::SessionPtr oldSession = sessionStore->getSession(mSessionId);

		if (oldSession == nullptr) {
			WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR, "s %" PRIu32 ": Cannot get session", mSessionId);
			mResult = 1;// will report error with answer
			return 1;
		}

		clientHostName = oldSession->getClientHostName();

		if (reconnectAllowed) {
			if (singleSession) {
				if (reconnectClientHostName) {
					currentSession = sessionStore->getFirstLoggedInSession(mUserName, mDomainName, clientHostName);
				} else {
					currentSession = sessionStore->getFirstLoggedInSession(mUserName, mDomainName);
				}

				if (currentSession) {
					// check if someone is connected and disconnect.
					UINT32 connectionID = connectionStore->getConnectionIdForSessionId(currentSession->getSessionID());
					if (connectionID != 0) {
						WLog_Print(logger_TaskAuthenticateUser, WLOG_INFO,
								   "s %" PRIu32 ": Single session detected, disconnecting connection %" PRIu32 "!",
								   mSessionId, connectionID);
						otsapiNS::TaskDisconnectPtr disconnectTask(new otsapiNS::TaskDisconnect(currentSession->getSessionID(), TRUE, INFINITE));
						currentSession->addTask(disconnectTask);
						WaitForSingleObject(disconnectTask->getHandle(), INFINITE);
					}
				}
			} else {
				if (reconnectClientHostName){
					currentSession = sessionStore->getFirstDisconnectedSession(mUserName, mDomainName,clientHostName);
				} else {
					currentSession = sessionStore->getFirstDisconnectedSession(mUserName, mDomainName);
				}
			}
		} else if (singleSession) {
			// reconnect not allowed and single session
			// search previous session and disconnect
			if (reconnectClientHostName) {
				currentSession = sessionStore->getFirstLoggedInSession(mUserName, mDomainName, clientHostName);
			} else {
				currentSession = sessionStore->getFirstLoggedInSession(mUserName, mDomainName);
			}

			if (currentSession) {
				// shut down this session.
				sessionNS::TaskEndPtr shutdown(new sessionNS::TaskEnd());
				shutdown->setSessionId(currentSession->getSessionID());
				currentSession->addTask(shutdown);
				WaitForSingleObject(shutdown->getHandle(), INFINITE);
				currentSession.reset();
			}
		}

		if (!currentSession) {
			WLog_Print(logger_TaskAuthenticateUser, WLOG_INFO,
				"s %" PRIu32 ": Replacing authsession with new session for user: %s in domain: %s with following active settings: reconnectAllowed(%s), fromSameClient(%s), singleSession(%s)",
				mSessionId, mUserName.c_str(), mDomainName.c_str(),
				reconnectAllowed ? "true" : "false",
				reconnectClientHostName ? "true" : "false",
				singleSession ? "true" : "false");

			currentSession = oldSession;
			setAccessorSession(currentSession);
			// reuse current session, but start new module
			setUserName(mUserName);
#ifdef _WIN32
			setDomain(mDomainName);
#else
			setDomain("");
#endif
			setClientHostName(clientHostName);
			initPermissions();

			if (!currentSession->checkPermission(WTS_PERM_FLAGS_LOGON)) {
				sessionStore->removeSession(currentSession->getSessionID());
				WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR,	"s %" PRIu32 ": user %s with domain %s has no permission to logon!",
							mSessionId, mUserName.c_str(), mDomainName.c_str());
				currentConnection->resetAuthenticatedUser();
				mResult = 1;// will report error with answer
				return 1;
			}

			if (!generateUserToken()) {
				sessionStore->removeSession(currentSession->getSessionID());
				WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR,	"s %" PRIu32 ": generateUserToken failed for user %s with domain %s",
							mSessionId, mUserName.c_str(), mDomainName.c_str());
				currentConnection->resetAuthenticatedUser();
				mResult = 1;// will report error with answer
				return 1;
			}

			sessionNS::ConnectionPtr connection = connectionStore->getConnectionForSessionId(mSessionId);
			std::string clientname;
			std::string clientAddress;
			if (connection) {
				clientHostName = connection->getClientInformation()->clientHostName;
				clientAddress = connection->getClientInformation()->clientAddress;
			}

			if (!generateEnvBlockAndModify(clientHostName, clientAddress)) {
				sessionStore->removeSession(currentSession->getSessionID());
				WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR,	"s %" PRIu32 ": generateEnvBlockAndModify failed for user %s with domain %s",
							mSessionId, mUserName.c_str(), mDomainName.c_str());
				currentConnection->resetAuthenticatedUser();
				mResult = 1;// will report error with answer
				return 1;
			}

			markBackendAsAuth();
			applyAuthTokenPermissions();

			std::string moduleConfigName;

			if (!propertyManager->getPropertyString(currentSession->getSessionID(), "module", moduleConfigName)) {
				WLog_Print(logger_TaskAuthenticateUser, WLOG_INFO, "s %" PRIu32 ": Could not get attribute 'module', using 'X11' instead", mSessionId);
				moduleConfigName = "X11";
			}
			setModuleConfigName(moduleConfigName);

			long mMaxWidth =  currentSession->getMaxXRes();
			if ((mMaxWidth != 0) && (mMaxWidth < currentConnection->getClientInformation()->width)) {
				currentConnection->getClientInformation()->width = mMaxWidth;
			}

			long mMaxHeight = currentSession->getMaxYRes();
			if ((mMaxHeight != 0) && (mMaxHeight < currentConnection->getClientInformation()->height)) {
				currentConnection->getClientInformation()->height = mMaxHeight;
			}

			std::string pipeName;
			if (!startModule(pipeName)) {
				WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR,	"s %" PRIu32 ": ModuleConfig %s does not start properly for user %s in domain %s",
							mSessionId, currentSession->getModuleConfigName().c_str(),
							mUserName.c_str(), mDomainName.c_str());
				restoreBackendFromAuth();
				currentConnection->resetAuthenticatedUser();
				mResult = 1;// will report error with answer
				return 1;
			}

			// in that case this session can do the switch ...
			UINT32 switch2Result = switchToInSameSession(currentSession);
			connectModule();
			setConnectState(WTSActive);
			resetAccessorSession();
			return switch2Result;
		}

		// session found
		WLog_Print(logger_TaskAuthenticateUser, WLOG_INFO,
			"s %" PRIu32 ": Using session for user: %s in domain: %s with following active settings: reconnectAllowed(%s), fromSameClient(%s), singleSession(%s)"
			,currentSession->getSessionID(), mUserName.c_str(), mDomainName.c_str()
			,reconnectAllowed ? "true" : "false"
			,reconnectClientHostName ? "true" : "false"
			,singleSession ? "true" : "false");

		// in that case the other session should do the switch
		UINT32 connectionId = connectionStore->getConnectionIdForSessionId(mSessionId);
		sessionNS::TaskSwitchToPtr switchTo( new sessionNS::TaskSwitchTo(connectionId, currentSession->getSessionID()));
		currentSession->addTask(switchTo);
		WaitForSingleObject(switchTo->getHandle(), INFINITE);

		bool success;
		UINT32 result = switchTo->getResults(success);
		if (result != 0) {
			WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR, "s %" PRIu32 ": answer: RPC error %" PRIu32 "!", mSessionId, result);
			mResult = result;
			return 1;
		}

		if (!success) {
			WLog_Print(logger_TaskAuthenticateUser, WLOG_ERROR, "s %" PRIu32 ": switching in ogon failed!", mSessionId);
			mResult = 1;
			return 1;
		}

		// remove auth session
		sessionStore->removeSession(oldSession->getSessionID());
		return 0;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
