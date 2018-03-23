/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * OTSApiHandler
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

#include <session/SessionStore.h>
#include <appcontext/ApplicationContext.h>

#include <winpr/thread.h>
#include <winpr/synch.h>
#include <winpr/wlog.h>
#include <winpr/user.h>

#include <call/CallOutLogOffUserSession.h>
#include <call/CallOutDisconnectUserSession.h>
#include <call/CallOutMessage.h>
#include <permission/permission.h>
#include <boost/algorithm/string/predicate.hpp>
#include <call/CallOutOtsApiStartRemoteControl.h>
#include <call/CallOutOtsApiVirtualChannelClose.h>
#include <call/CallOutOtsApiVirtualChannelOpen.h>
#include <session/Session.h>
#include <utils/TimeHelpers.h>
#include <winpr/sysinfo.h>
#include "../../common/global.h"
#include <ogon/version.h>
#include <otsapi/OTSApiHandler.h>
#include <otsapi/TaskDisconnect.h>
#include <otsapi/TaskLogoff.h>
#include <otsapi/TaskStartRemoteControl.h>
#include <otsapi/TaskStopRemoteControl.h>

#define OTSAPI_TIMEOUT 10*1000

namespace ogon{ namespace sessionmanager{ namespace otsapi {

	static wLog *logger_OTSApiHandler = WLog_Get("ogon.sessionmanager.otsapihandler");

	OTSApiHandler::OTSApiHandler() {
	}

	OTSApiHandler::~OTSApiHandler() {
	}

	void OTSApiHandler::getVersionInfo(TVersion& _return, const TVersion& versionInfo) {
		OGON_UNUSED(versionInfo);
		_return.__set_VersionMajor(OGON_PROTOCOL_VERSION_MAJOR);
		_return.__set_VersionMinor(OGON_PROTOCOL_VERSION_MINOR);
	}

	TDWORD OTSApiHandler::ping(const TDWORD input) {
		return input;
	}

	void OTSApiHandler::virtualChannelOpen(TReturnVirtualChannelOpen &_return,
			const TSTRING &authToken, const TDWORD sessionId,
			const TSTRING &virtualName,const bool isDynChannel, const TDWORD flags) {

		DWORD status;
		bool dynamicChannel = false;
		if (isDynChannel && (flags & WTS_CHANNEL_OPTION_DYNAMIC)) {
			dynamicChannel = true;
		}

		sessionNS::SessionPtr session = getSessionAndCheckForPerm(authToken,
				sessionId, WTS_PERM_FLAGS_VIRTUAL_CHANNEL);

		if (session == NULL) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "s %" PRIu32 ": Session not found or no permission!", sessionId);
			_return.__set_pipeName("");
			_return.__set_instance(0);
			return;
		}

		if (!session->isVirtualChannelAllowed(virtualName)) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "s %" PRIu32 ": Virtual channel %s is forbidden per config!",
				sessionId, virtualName.c_str());
			_return.__set_pipeName("");
			_return.__set_instance(0);
			return;
		}

		UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(session->getSessionID());
		if (connectionId == 0)  {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR,
				"s %" PRIu32 ": cannot notify ogon (connection missing)!", sessionId);
			_return.__set_pipeName("");
			_return.__set_instance(0);
			return;
		}

		callNS::CallOutOtsApiVirtualChannelOpenPtr openCall(new callNS::CallOutOtsApiVirtualChannelOpen());
		openCall->setConnectionID(connectionId);
		openCall->setVirtualName(virtualName);
		openCall->setDynamicChannel(dynamicChannel);
		openCall->setFlags(dynamicChannel ? flags : 0);

		APP_CONTEXT.getRpcOutgoingQueue()->addElement(openCall);
		status = WaitForSingleObject(openCall->getAnswerHandle(), OTSAPI_TIMEOUT);
		if (status == WAIT_TIMEOUT) {
			WLog_Print(logger_OTSApiHandler, WLOG_WARN, "s %" PRIu32 ": virtualChannelOpen timed out", sessionId);
			_return.__set_pipeName("");
			_return.__set_instance(0);
			return;
		}
		if (openCall->getResult() == 0) {
			// no error
			WLog_Print(logger_OTSApiHandler, WLOG_TRACE, "s %" PRIu32 ": got result: %s",
				sessionId, openCall->getConnectionString().c_str());
			_return.__set_pipeName(openCall->getConnectionString());
			_return.__set_instance(openCall->getInstance());
			return;
		}

		// report error
		WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "s %" PRIu32 ": rpc reported error %" PRIu32 "", sessionId, openCall->getResult());
		_return.__set_pipeName("");
		_return.__set_instance(0);
	}

	bool OTSApiHandler::virtualChannelClose( const TSTRING &authToken,
		const TDWORD sessionId, const TSTRING &virtualName, const TDWORD instance) {

		DWORD status;
		sessionNS::SessionPtr session = getSessionAndCheckForPerm(authToken,
			sessionId, WTS_PERM_FLAGS_VIRTUAL_CHANNEL);

		if (session == NULL) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR,
				"s %" PRIu32 ": Session not found or no permission!", sessionId);
			return false;
		}

		UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(session->getSessionID());
		if (connectionId == 0)  {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR,
				"s %" PRIu32 ": cannot notify ogon (connection missing)!", sessionId);
			return false;
		}

		callNS::CallOutOtsApiVirtualChannelClosePtr closeCall(new callNS::CallOutOtsApiVirtualChannelClose());
		closeCall->setConnectionID(connectionId);
		closeCall->setVirtualName(virtualName);
		closeCall->setInstance(instance);

		APP_CONTEXT.getRpcOutgoingQueue()->addElement(closeCall);
		status = WaitForSingleObject(closeCall->getAnswerHandle(), OTSAPI_TIMEOUT);
		if (status == WAIT_TIMEOUT) {
			WLog_Print(logger_OTSApiHandler, WLOG_WARN, "s %" PRIu32 ": virtualChannelClose timed out", sessionId);
			return false;
		}
		if (closeCall->getResult() == 0) {
			// no error
			WLog_Print(logger_OTSApiHandler, WLOG_TRACE, "s %" PRIu32 ": got result: %s",
				sessionId, closeCall->getSuccess() ? "true" : "false");
			return closeCall->getSuccess();
		}
		// report error
		WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "s %" PRIu32 ": rpc reported error %" PRIu32 "", sessionId, closeCall->getResult());
		return false;
	}

	bool OTSApiHandler::disconnectSession( const TSTRING &authToken,
		const TDWORD sessionId, const TBOOL wait) {

		sessionNS::SessionPtr session = getSessionAndCheckForPerm(authToken,
			sessionId, WTS_PERM_FLAGS_DISCONNECT);

		if (session == NULL) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR,
				"s %" PRIu32 ": Session not found or no permission!", sessionId);
			return false;
		}

		TaskDisconnectPtr taskDisconnect(new TaskDisconnect(session->getSessionID(), wait, OTSAPI_TIMEOUT));
		session->addTask(taskDisconnect);
		WaitForSingleObject(taskDisconnect->getHandle(), INFINITE);
		return taskDisconnect->getResults();
	}

	bool OTSApiHandler::logoffSession( const TSTRING &authToken,
		const TDWORD sessionId, const TBOOL wait) {

		sessionNS::SessionPtr session = getSessionAndCheckForPerm(authToken,
			sessionId, WTS_PERM_FLAGS_LOGOFF);

		if (session == NULL) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR,
				"s %" PRIu32 ": Session not found or no permission!", sessionId);
			return false;
		}

		TaskLogoffPtr taskLogoff(new TaskLogoff(session->getSessionID(), wait, OTSAPI_TIMEOUT));
		session->addTask(taskLogoff);
		WaitForSingleObject(taskLogoff->getHandle(), INFINITE);
		return taskLogoff->getResults();
	}

	void OTSApiHandler::enumerateSessions( TReturnEnumerateSession &_return,
		const TSTRING &authToken, const TDWORD Version) {

		OGON_UNUSED(Version);

		DWORD count;
		DWORD index;

		sessionNS::SessionPtr current = APP_CONTEXT.getPermissionManager()->getSessionForToken(authToken);
		if (current == NULL) {
			permissionNS::LogonPermissionPtr permission = APP_CONTEXT.getPermissionManager()->getPermissionForLogon(authToken);
			if (permission == NULL) {
				WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "Logon Permission not found!");
				_return.__set_returnValue(false);
				return;
			}
			if (!((permission->getPermission() & WTS_PERM_FLAGS_QUERY_INFORMATION) == WTS_PERM_FLAGS_QUERY_INFORMATION)) {
				WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "No permission for user (%s) to query information!", permission->getUsername().c_str());
				_return.__set_returnValue(false);
				return;
			}
		} else {
			if (!current->checkPermission(WTS_PERM_FLAGS_QUERY_INFORMATION)) {
				WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "s %" PRIu32 ": No permission to query information!", current->getSessionID());
				_return.__set_returnValue(false);
				return;
			}
		}

		std::list<sessionNS::SessionPtr> sessions = APP_CONTEXT.getSessionStore()->getAllSessions();

		count = sessions.size();
		TSessionList list(count);

		std::list<sessionNS::SessionPtr>::iterator session = sessions.begin();

		for (index = 0; index < count; index++) {
			list.at(index).sessionId = (*session)->getSessionID();
			list.at(index).connectState = (*session)->getConnectState();
			session++;
		}

		_return.__set_sessionInfoList(list);
		_return.__set_returnValue(true);
	}

	void OTSApiHandler::querySessionInformation(TReturnQuerySessionInformation &_return,
		const TSTRING &authToken, const TDWORD sessionId, const TINT32 infoClass){

		sessionNS::SessionPtr session = getSessionAndCheckForPerm(authToken,
			sessionId, WTS_PERM_FLAGS_QUERY_INFORMATION);

		_return.__set_returnValue(false);

		if (session == NULL) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR,
				"s %" PRIu32 ": no session found or no permission!", sessionId);
			return;
		}

		if (infoClass == WTSSessionInfo) {
			TWTSINFO wtsinfo;
			wtsinfo.__set_State(session->getConnectState());
			wtsinfo.__set_SessionId(session->getSessionID());
			wtsinfo.__set_WinStationName(session->getWinStationName());
			if ((session->getConnectState() != WTSConnected) &&
				(session->getConnectState() != WTSInit)) {
				wtsinfo.__set_Domain(session->getDomain());
				wtsinfo.__set_UserName(session->getUserName());
			}
			boost::posix_time::ptime connectTime = session->getConnectTime();
			boost::posix_time::ptime disconnectTime = session->getDisconnectTime();;
			boost::posix_time::ptime mLogonTime = session->getLogonTime();

			if (connectTime.is_not_a_date_time()) {
				wtsinfo.__set_ConnectTime(0);
			} else {
				FILETIME connectTimeFT;
				GetUnixTimeAsFileTime(::to_time_t(connectTime), &connectTimeFT);
				wtsinfo.__set_ConnectTime(convertFileTimeToint64(connectTimeFT));
			}

			if (disconnectTime.is_not_a_date_time()) {
				wtsinfo.__set_DisconnectTime(0);
			} else {
				FILETIME disconnectTimeFT;
				GetUnixTimeAsFileTime(::to_time_t(disconnectTime), &disconnectTimeFT);
				wtsinfo.__set_DisconnectTime(convertFileTimeToint64(disconnectTimeFT));
			}

			if (mLogonTime.is_not_a_date_time()) {
				wtsinfo.__set_LogonTime(0);
			} else {
				FILETIME logonTimeFT;
				GetUnixTimeAsFileTime(::to_time_t(mLogonTime), &logonTimeFT);
				wtsinfo.__set_LogonTime(convertFileTimeToint64(logonTimeFT));
			}
			FILETIME currentTimeFT;
			GetSystemTimeAsFileTime(&currentTimeFT);
			wtsinfo.__set_CurrentTime(convertFileTimeToint64(currentTimeFT));

			_return.__set_returnValue(true);
			_return.infoValue.__set_WTSINFO(wtsinfo);
			return;
		}


		if ((infoClass == WTSUserName) || (infoClass == WTSDomainName)) {
			_return.__set_returnValue(true);
			if ((session->getConnectState() == WTSConnected) ||
				(session->getConnectState() == WTSInit)) {
				_return.infoValue.__set_stringValue("");
			} else if (infoClass == WTSUserName) {
				_return.infoValue.__set_stringValue(session->getUserName());
			} else {
				_return.infoValue.__set_stringValue(session->getDomain());
			}
			return;
		}

		if ((infoClass == WTSSessionId) || (infoClass == WTSConnectState) ||
				(infoClass == WTSWinStationName) || (infoClass == WTSLogonTime)) {
			_return.__set_returnValue(true);
			switch (infoClass) {
				case WTSSessionId:
					_return.infoValue.__set_int32Value(session->getSessionID());
					break;
				case WTSConnectState:
					_return.infoValue.__set_int32Value(session->getConnectState());
					break;
				case WTSWinStationName:
					_return.infoValue.__set_stringValue(session->getWinStationName());
					break;
				case WTSLogonTime: {
					boost::posix_time::ptime mLogonTime = session->getLogonTime();
					if (!mLogonTime.is_not_a_date_time()) {
						_return.infoValue.__set_int64Value(0);
					} else {
						FILETIME logonTimeFT;
						GetUnixTimeAsFileTime(::to_time_t(mLogonTime), &logonTimeFT);
						_return.infoValue.__set_int64Value(convertFileTimeToint64(logonTimeFT));
					}
					break;
				}
				default:
					break;
			}
			return;
		}

		sessionNS::ConnectionPtr connection = APP_CONTEXT.getConnectionStore()->getConnectionForSessionId(session->getSessionID());

		if (!connection) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "s %" PRIu32 ": no client connection found to query values!", sessionId);
			return;
		}

		_return.__set_returnValue(true);
		TClientDisplay display;
		switch (infoClass) {
			case WTSClientBuildNumber:
				_return.infoValue.__set_int32Value(connection->getClientInformation()->clientBuildNumber);
				break;
			case WTSClientName:
				_return.infoValue.__set_stringValue(connection->getClientInformation()->clientHostName);
				break;
			case WTSClientProductId:
				_return.infoValue.__set_int16Value(connection->getClientInformation()->clientProductId);
				break;
			case WTSClientHardwareId:
				_return.infoValue.__set_int32Value(connection->getClientInformation()->clientHardwareId);
				break;
			case WTSClientAddress:
				_return.infoValue.__set_stringValue(connection->getClientInformation()->clientAddress);
				break;
			case WTSClientDisplay:
				display.__set_displayWidth(connection->getClientInformation()->width);
				display.__set_displayHeight(connection->getClientInformation()->height);
				display.__set_colorDepth(connection->getClientInformation()->colordepth);
				_return.infoValue.__set_displayValue(display);
				break;
			case WTSClientProtocolType:
				_return.infoValue.__set_int16Value(connection->getClientInformation()->clientProtocolType);
				break;
			default:
				_return.__set_returnValue(false);
			break;
		}
	}

	sessionNS::SessionPtr OTSApiHandler::getSessionAndCheckForPerm(
		const TSTRING &authToken, UINT32 sessionId, DWORD requestedPermission) {

		sessionNS::SessionPtr session = APP_CONTEXT.getPermissionManager()->getSessionForToken(authToken);
		if (session && session->getSessionID() != sessionId) {
			// query Session information for another session
			if (session->checkPermission(requestedPermission)) {
				session = APP_CONTEXT.getSessionStore()->getSession(sessionId);
			} else {
				return sessionNS::SessionPtr();
			}
		}
		if (session == NULL) {
			// check auth tokens
			permissionNS::LogonPermissionPtr permission = APP_CONTEXT.getPermissionManager()->getPermissionForLogon(authToken);
			if (permission == NULL ) {
				// no authtoken found
				return sessionNS::SessionPtr();
			}
			sessionNS::SessionPtr ownSession = APP_CONTEXT.getSessionStore()->getSession(sessionId);
			if ((ownSession != NULL ) && (ownSession->getDomain() == permission->getDomain()) &&
					(ownSession->getUserName() == permission->getUsername())) {
				// same user can do anything
				return ownSession;
			}

			if ((permission->getPermission() & requestedPermission) == requestedPermission) {
				// user has permission
				session = APP_CONTEXT.getSessionStore()->getSession(sessionId);
			}
		}
		return session;
	}

	void OTSApiHandler::logonConnection(TReturnLogonConnection &_return,
		const TSTRING &username, const TSTRING &password, const TSTRING &domain) {

		sessionNS::ConnectionPtr con(new sessionNS::Connection(0));
		std::string domainName = domain;
		int status = con->authenticateUser(username, domainName, password);
		if (status == 0) {
			DWORD permission;
			std::string value;
			if (!APP_CONTEXT.getPropertyManager()->getPropertyString(0, "permission.level" ,value, username)) {
				permission = WTS_PERM_FLAGS_GUEST;
			} else {
				if (boost::iequals(value, "FULL")) {
					permission = WTS_PERM_FLAGS_FULL;
				} else if (boost::iequals(value, "USER")) {
					permission = WTS_PERM_FLAGS_USER;
				} else {
					permission = WTS_PERM_FLAGS_GUEST;
				}
			}
			std::string authToken = APP_CONTEXT.getPermissionManager()->registerLogon(username, domainName, permission);
			_return.__set_success(true);
			_return.__set_authToken(authToken);
		} else {
			_return.__set_success(false);
		}
	}

	bool OTSApiHandler::logoffConnection(const TSTRING &authToken){
		APP_CONTEXT.getPermissionManager()->unregisterLogon(authToken);
		return true;
	}

	TDWORD OTSApiHandler::getPermissionForToken(const TSTRING &authToken) {
		permissionNS::LogonPermissionPtr permission = APP_CONTEXT.getPermissionManager()->getPermissionForLogon(authToken);
		if (permission == NULL) {
			return 0;
		} else {
			return permission->getPermission();
		}
	}

	bool OTSApiHandler::startRemoteControlSession(const TSTRING& authToken, const TDWORD sourceLogonId,
		const TDWORD targetLogonId, const TBYTE HotkeyVk,
		const TINT16 HotkeyModifiers, TDWORD flags) {

		sessionNS::SessionPtr session = APP_CONTEXT.getPermissionManager()->getSessionForToken(authToken);
		if (!session) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "s %" PRIu32 ": No session found for authToken !", sourceLogonId);
			return false;
		}

		if (session->getSessionID() != (UINT32)sourceLogonId) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "s %" PRIu32 ": Can't start a shadowing session from a different session for now !", session->getSessionID());
			return false;
		}

		/* "self-shadowing" is not possible */
		if (session->getSessionID() == (UINT32)targetLogonId) {
			return false;
		}

		if (!session->checkPermission(WTS_PERM_FLAGS_REMOTE_CONTROL)) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR,
				"s %" PRIu32 ": User does not have the WTS_PERM_FLAGS_REMOTE_CONTROL right!", session->getSessionID());
			return false;
		}

		TaskStartRemoteControlPtr startRemoteControl(new TaskStartRemoteControl(sourceLogonId, targetLogonId, HotkeyVk, HotkeyModifiers, flags, OTSAPI_TIMEOUT));
		session->addTask(startRemoteControl);

		WaitForSingleObject(startRemoteControl->getHandle(), INFINITE);

		return startRemoteControl->getResult();
	}

	bool OTSApiHandler::stopRemoteControlSession(const TSTRING& authToken, const TDWORD sourceLogonId,
				const TDWORD /*targetLogonId*/) {

		sessionNS::SessionPtr session = getSessionAndCheckForPerm(authToken,
				sourceLogonId, WTS_PERM_FLAGS_REMOTE_CONTROL);

		if (session == NULL) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR, "s %" PRIu32 ": Session not found or no permission!", sourceLogonId);
			return false;
		}

		std::list<UINT32>::const_iterator iter;

		std::list<UINT32> shadowdBy = session->getShadowedByList();

		for (iter = shadowdBy.begin(); iter != shadowdBy.end(); ++iter) {
			sessionNS::SessionPtr sessionDisconnect = APP_CONTEXT.getSessionStore()->getSession(*iter);
			if (!sessionDisconnect) {
				continue;
			}
			WLog_Print(logger_OTSApiHandler, WLOG_INFO, "s %" PRIu32 ": sending shadow stop for sessionid %" PRIu32 "",
					   session->getSessionID(), sessionDisconnect->getSessionID());
			TaskStopRemoteControlPtr task(new TaskStopRemoteControl(sessionDisconnect->getSessionID(), OTSAPI_TIMEOUT));
			sessionDisconnect->addTask(task);
			WaitForSingleObject(task->getHandle(), INFINITE);
		}
		session->clearShadowedBy();
		return true;
	}

	TDWORD OTSApiHandler::sendMessage(const TSTRING &authToken, const TDWORD sessionId,
		const TSTRING &title, const TSTRING &message, const TDWORD style,
		const TDWORD timeout, const TBOOL wait) {

		DWORD status;
		sessionNS::SessionPtr session = getSessionAndCheckForPerm(authToken,
			sessionId, WTS_PERM_FLAGS_MESSAGE);

		if (session == NULL) {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR,
				"s %" PRIu32 ": Session not found or no permission!", sessionId);
			return 0;
		}

		UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(session->getSessionID());
		if (connectionId == 0)  {
			WLog_Print(logger_OTSApiHandler, WLOG_ERROR,
				"s %" PRIu32 ": cannot notify ogon (connection missing)!", sessionId);
			return 0;
		}

		callNS::CallOutMessagePtr messageCall(new callNS::CallOutMessage());
		messageCall->setConnectionId(connectionId);
		messageCall->setType(MESSAGE_CUSTOM_TYPE);
		messageCall->setParameterNumber(2);
		messageCall->setParameter1(title);
		messageCall->setParameter2(message);
		messageCall->setTimeout(timeout);
		messageCall->setStyle(style);


		APP_CONTEXT.getRpcOutgoingQueue()->addElement(messageCall);
		if (!wait) {
			return IDASYNC;
		}

		status = WaitForSingleObject(messageCall->getAnswerHandle(), timeout == 0 ? INFINITE : timeout * 1000);
		if (status == WAIT_TIMEOUT) {
			WLog_Print(logger_OTSApiHandler, WLOG_WARN, "s %" PRIu32 ": sendMessage timed out", sessionId);
			return IDTIMEOUT;
		}

		if (messageCall->getResult() == 0) {
			// result 0 is only sent if timeout has appeared too early
			return IDTIMEOUT;
		}
		return messageCall->getResult();
	}

} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/
