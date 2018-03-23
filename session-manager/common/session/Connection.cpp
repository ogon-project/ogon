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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/wlog.h>
#include <winpr/synch.h>

#include <appcontext/ApplicationContext.h>
#include <module/AuthModule.h>
#include <utils/CSGuard.h>

#include <string.h>


#include "Connection.h"

namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_Connection = WLog_Get("ogon.sessionmanager.session.connection");

	Connection::Connection(DWORD connectionId) : mConnectionId(connectionId),
		mSessionId(0), mAuthStatus(-1),
		mConnectionState(Connection_Init)
	{
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_Connection, WLOG_FATAL,
				"Failed to initialize connection critical section (mCSection)");
			throw std::bad_alloc();
		}

		if (!InitializeCriticalSectionAndSpinCount(&mCSectionQueue, 0x00000400)) {
			WLog_Print(logger_Connection, WLOG_FATAL,
				"Failed to initialize connection critical section (mCSectionQueue)");
			throw std::bad_alloc();
		}

		mClientInformation.clientBuildNumber = 0;
		mClientInformation.clientHardwareId = 0;
		mClientInformation.clientProductId = 0;
		mClientInformation.clientProtocolType = 0;
		mClientInformation.colordepth = 0;
		mClientInformation.height = 0;
		mClientInformation.width = 0;
		mClientInformation.initialWidth = 0;
		mClientInformation.initialHeight = 0;
	}

	Connection::~Connection() {
		DeleteCriticalSection(&mCSection);
		DeleteCriticalSection(&mCSectionQueue);
	}

	std::string Connection::getDomain() {
		return mDomain;
	}

	std::string Connection::getUserName() {
		return mUsername;
	}

	void Connection::setSessionId(UINT32 sessionId) {
		mSessionId = sessionId;
		WLog_Print(logger_Connection, WLOG_DEBUG, "Session %" PRIu32 " bound to connection %" PRIu32 "\n", mSessionId, mConnectionId);
	}

	UINT32 Connection::getSessionId() {
		return mSessionId;
	}

	UINT32 Connection::getConnectionId() {
		return mConnectionId;
	}

	int Connection::authenticateUser(const std::string &username, std::string &domain,
		const std::string &password, UINT32 sessionId) {

		CSGuard guard(&mCSection);
		if (!APP_CONTEXT.getPermissionManager()->isLogonAllowedForUser(username)) {
			WLog_Print(logger_Connection, WLOG_DEBUG,
				"s %" PRIu32 ": Logon of user %s is not allowed!", sessionId, username.c_str());
			return 1;
		}
		if (mAuthStatus == 0) {
			// a Connection can only be authorized once
			return -1;
		}

		std::string authModule;
		if (!APP_CONTEXT.getPropertyManager()->getPropertyString(0, "auth.module", authModule, username)) {
			WLog_Print(logger_Connection, WLOG_INFO, "s %" PRIu32 ": could not find attribute 'auth.module', using 'PAM' instead!",
					   sessionId);
			authModule = "PAM";
		}

		moduleNS::AuthModule *auth = APP_CONTEXT.getModuleManager()->getAuthModule(authModule);

		if (!auth) {
			WLog_Print(logger_Connection, WLOG_DEBUG,
					   "s %" PRIu32 ": could not get authModule %s", sessionId, authModule.c_str());
			return 1;
		}

		rdsAuthModule *context = auth->newContext();
		if (!context) {
			WLog_Print(logger_Connection, WLOG_DEBUG,
					   "s %" PRIu32 ": could not get create authModule context", sessionId);
			return 1;
		}

		mAuthStatus = auth->logonUser(context, username, domain, password);
		auth->freeContext(context);

		if (mAuthStatus == 0) {
			std::string fullusername;
			if (domain.size()) {
				fullusername = username + "\\" + domain;
			} else {
				fullusername = username;
			}
			WLog_Print(logger_Connection, WLOG_DEBUG,
					   "s %" PRIu32 ": user '%s' authenticated successful ", sessionId, fullusername.c_str());
			mUsername = username;
			mDomain = domain;
		}
		return mAuthStatus;
	}

	void Connection::resetAuthenticatedUser() {
		mAuthStatus = -1;
		mUsername.clear();
		mDomain.clear();
	}

	pCLIENT_INFORMATION Connection::getClientInformation() {
		return &mClientInformation;
	}

	bool Connection::getProperty(const std::string &path, PROPERTY_STORE_HELPER& helper) {

		if (path.compare("XRES") == 0) {
			helper.type = NumberType;
			helper.numberValue = mClientInformation.width;
			return true;
		}

		if (path.compare("YRES") == 0) {
			helper.type = NumberType;
			helper.numberValue = mClientInformation.height;
			return true;
		}

		if (path.compare("COLORDEPTH") == 0) {
			helper.type = NumberType;
			helper.numberValue = mClientInformation.colordepth;
			return true;
		}

		if (path.compare("INITIALXRES") == 0) {
			helper.type = NumberType;
			helper.numberValue = mClientInformation.initialWidth;
			return true;
		}

		if (path.compare("INITIALYRES") == 0) {
			helper.type = NumberType;
			helper.numberValue = mClientInformation.initialHeight;
			return true;
		}
		return false;
	}

	bool Connection::tryQueueCall(callNS::CallInPtr call) {
		CSGuard guard(&mCSectionQueue);
		switch (mConnectionState) {
			case Connection_Init:
				mQueuedCalls.push_back(call);
				return true;
			case Connection_Session_failed:
				call->abort();
				APP_CONTEXT.getRpcOutgoingQueue()->addElement(call);
				return true;
			default:
				break;
		}
		return false;
	}

	std::list<callNS::CallInPtr> Connection::setStatusGetList(CONNECTION_STATE state) {
		CSGuard guard(&mCSectionQueue);
		std::list<callNS::CallInPtr> tempList;

		mConnectionState = state;

		switch (mConnectionState) {
			case Connection_Init:
				return tempList;
			case Connection_Session_failed: {
				std::list<callNS::CallInPtr>::iterator iterator;
				for (iterator = mQueuedCalls.begin(); iterator != mQueuedCalls.end(); ++iterator) {
					callNS::CallInPtr currentCall = (*iterator);
					currentCall->abort();
					APP_CONTEXT.getRpcOutgoingQueue()->addElement(currentCall);
				}
				return tempList;
			}
			case Connection_Has_Session: {
				tempList = mQueuedCalls;
				mQueuedCalls.clear();
				return tempList;
			}
		}
		return tempList;
	}

} /*session*/ } /*sessionmanager*/ } /*ogon*/
