/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for the LogonUser call.
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

#include "CallInLogonUser.h"
#include <appcontext/ApplicationContext.h>
#include <call/TaskCallInLogonUser.h>
#include <session/TaskLogonUser.h>
#include <permission/permission.h>

using ogon::icp::LogonUserRequest;
using ogon::icp::LogonUserResponse;

namespace ogon { namespace sessionmanager { namespace call {

	static wLog *logger_CallInLogonUser = WLog_Get("ogon.sessionmanager.call.callinlogonuser");

	CallInLogonUser::CallInLogonUser() : mClientBuildNumber(0), mClientProductId(0),
		mClientHardwareId(0), mClientProtocolType(0), mWidth(0), mMaxWidth(0),
		mHeight(0), mMaxHeight(0), mColorDepth(0), mAuthStatus(0), mConnectionId(0) {
	}

	CallInLogonUser::~CallInLogonUser() {
	}

	unsigned long CallInLogonUser::getCallType() const {
		return ogon::icp::LogonUser;
	}

	bool CallInLogonUser::decodeRequest() {
		// decode protocol buffers
		LogonUserRequest req;

		if (!req.ParseFromString(mEncodedRequest)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}

		mUserName = req.username();
		mConnectionId = req.connectionid();
		mDomainName = req.domain();
		mPassword = req.password();
		mWidth = req.width();
		mHeight = req.height();
		mColorDepth = req.colordepth();
		mClientHostName = req.clienthostname();
		mClientAddress = req.clientaddress();
		mClientBuildNumber = req.clientbuildnumber();
		mClientProductId = req.clientproductid();
		mClientHardwareId = req.clienthardwareid();
		mClientProtocolType = req.clientprotocoltype();

		return true;
	}

	void CallInLogonUser::updateResult(uint32_t result, std::string pipeName, long maxHeight, long maxWidth, std::string backendCookie, std::string ogonCookie) {
		mResult = result;
		mPipeName = pipeName;
		mMaxHeight = maxHeight;
		mMaxWidth= maxWidth;
		mBackendCookie = backendCookie;
		mOgonCookie = ogonCookie;
	}


	bool CallInLogonUser::encodeResponse() {
		// encode protocol buffers
		LogonUserResponse resp;

		resp.set_serviceendpoint(mPipeName);
		resp.set_maxheight(mMaxHeight);
		resp.set_maxwidth(mMaxWidth);
		resp.set_backendcookie(mBackendCookie.c_str());
		resp.set_ogoncookie(mOgonCookie.c_str());

		if (!resp.SerializeToString(&mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	int CallInLogonUser::authenticateUser() {
		sessionNS::ConnectionPtr currentConnection = APP_CONTEXT.getConnectionStore()->getOrCreateConnection(mConnectionId);
		mAuthStatus = currentConnection->authenticateUser(mUserName, mDomainName, mPassword);
		return mAuthStatus;
	}

	sessionNS::SessionPtr CallInLogonUser::createNewUserSession() {
		sessionNS::SessionStore *sessionStore = APP_CONTEXT.getSessionStore();
		sessionNS::SessionPtr currentSession = sessionStore->createSession();
		UINT32 sessionId = currentSession->getSessionID();

		setAccessorSession(currentSession);
		setUserName(mUserName);
#ifdef _WIN32
		setDomain(mDomainName);
#else
		setDomain("");
#endif
		setClientHostName(mClientHostName);

		initPermissions();

		if (!currentSession->checkPermission(WTS_PERM_FLAGS_LOGON)) {
			sessionStore->removeSession(currentSession->getSessionID());
			WLog_Print(logger_CallInLogonUser, WLOG_ERROR, "s %" PRIu32 " user %s with domain %s has no permission to logon!",
					   sessionId, mUserName.c_str(), mDomainName.c_str());
			mResult = 1;// will report error with answer
			return sessionNS::SessionPtr();
		}

		if (!generateUserToken()) {
			sessionStore->removeSession(currentSession->getSessionID());
			WLog_Print(logger_CallInLogonUser, WLOG_ERROR, "s %" PRIu32 ": generateUserToken failed for user %s with domain %s",
					   sessionId, mUserName.c_str(), mDomainName.c_str());
			mResult = 1;// will report error with answer
			return sessionNS::SessionPtr();
		}

		if (!generateEnvBlockAndModify(mClientHostName, mClientAddress)) {
			sessionStore->removeSession(currentSession->getSessionID());
			WLog_Print(logger_CallInLogonUser, WLOG_ERROR, "s %" PRIu32 ": generateEnvBlockAndModify failed for user %s with domain %s",
					   sessionId, mUserName.c_str(), mDomainName.c_str());
			mResult = 1;// will report error with answer
			return sessionNS::SessionPtr();
		}

		applyAuthTokenPermissions();

		std::string moduleConfigName;

		if (!APP_CONTEXT.getPropertyManager()->getPropertyString(currentSession->getSessionID(), "module", moduleConfigName)) {
			WLog_Print(logger_CallInLogonUser, WLOG_INFO, "s %" PRIu32 ": could not find attribute module, using X11 instead!",
					   sessionId);
			moduleConfigName = "X11";
		}

		WLog_Print(logger_CallInLogonUser, WLOG_TRACE, "s %" PRIu32 ": created session for user '%s' with domain '%s'",
				   sessionId,
			mUserName.c_str(), mDomainName.c_str());
		setModuleConfigName(moduleConfigName);
		resetAccessorSession();
		return currentSession;
	}

	int CallInLogonUser::getUserSession() {
		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();
		sessionNS::ConnectionPtr currentConnection = APP_CONTEXT.getConnectionStore()->getOrCreateConnection(mConnectionId);
		sessionNS::SessionPtr currentSession;
		bool reconnectAllowed = true;
		bool reconnectClientHostName = false;
		bool singleSession = false;

		bool disconnectFirst = false;
		UINT32 logoffSession = 0;

		propertyManager->getPropertyBool(0, "session.reconnect", reconnectAllowed, mUserName);
		propertyManager->getPropertyBool(0, "session.reconnect.fromSameClient", reconnectClientHostName, mUserName);
		propertyManager->getPropertyBool(0, "session.singleSession", singleSession, mUserName);

		sessionNS::SessionStore *store = APP_CONTEXT.getSessionStore();

		if (reconnectAllowed) {
			if (singleSession) {
				if (reconnectClientHostName) {
					currentSession = store->getFirstLoggedInSession(mUserName, mDomainName, mClientHostName);
				} else {
					currentSession = store->getFirstLoggedInSession(mUserName, mDomainName);
				}

				if (currentSession) {
					disconnectFirst = true;
				}
			} else {
				// no single session
				if (reconnectClientHostName ){
					currentSession = store->getFirstDisconnectedSession(mUserName, mDomainName, mClientHostName);
				} else  {
					currentSession = store->getFirstDisconnectedSession(mUserName, mDomainName);
				}
			}

		} else if (singleSession) {
			// reconnect not allowed and single session
			// search previous session and disconnect
			if (reconnectClientHostName) {
				currentSession = store->getFirstLoggedInSession(mUserName, mDomainName, mClientHostName);
			} else {
				currentSession = store->getFirstLoggedInSession(mUserName, mDomainName);
			}
			if (currentSession) {
				// shut down this session.
				logoffSession = currentSession->getSessionID();
				currentSession.reset();
			}
		}
		if (!currentSession) {
			currentSession = createNewUserSession();
		}

		sessionNS::TaskLogonUserPtr logontask(new sessionNS::TaskLogonUser(
			currentConnection->getConnectionId(), currentSession->getSessionID(),
			mUserName, mDomainName, mPassword, mClientHostName, mClientAddress,
			mClientBuildNumber, mClientProductId, mClientHardwareId, mClientProtocolType,
			mWidth, mHeight, mColorDepth, false, disconnectFirst, logoffSession, shared_from_this()));

		currentSession->addTask(logontask);

		return 0;
	}

	int CallInLogonUser::getAuthSession() {
		// authentication failed, start up greeter module
		sessionNS::ConnectionPtr currentConnection = APP_CONTEXT.getConnectionStore()->getOrCreateConnection(mConnectionId);
		sessionNS::SessionPtr currentSession = APP_CONTEXT.getSessionStore()->createSession();

		sessionNS::TaskLogonUserPtr logontask(new sessionNS::TaskLogonUser(
			currentConnection->getConnectionId(), currentSession->getSessionID(),
			mUserName, mDomainName, mPassword, mClientHostName, mClientAddress,
			mClientBuildNumber, mClientProductId, mClientHardwareId, mClientProtocolType,
			mWidth, mHeight, mColorDepth, true, false, 0, shared_from_this()));

		currentSession->addTask(logontask);

		return 0;
	}

	bool CallInLogonUser::doStuff() {
		if (APP_CONTEXT.isShutdown()){
			mResult = 1;
			return true;
		}

		authenticateUser();
		if (mAuthStatus != 0) {
			getAuthSession();
		} else {
#ifndef _WIN32
			// ignore domains on non windows platforms for now
			mDomainName = "";
#endif
			getUserSession();
		}
		return true;
	}

	bool CallInLogonUser::prepare() {
		// create a new connection object, if it does not exist
		sessionNS::ConnectionPtr currentConnection = APP_CONTEXT.getConnectionStore()->getOrCreateConnection(mConnectionId);
		return doStuff();
	}

	std::shared_ptr<CallInLogonUser> CallInLogonUser::shared_from_this() {
		CallPtr call = Call::shared_from_this();
		return std::dynamic_pointer_cast<CallInLogonUser>(call);
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
