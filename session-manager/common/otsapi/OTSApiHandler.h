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

#ifndef _OGON_SMGR_OTSAPI_HANDLER_H_
#define _OGON_SMGR_OTSAPI_HANDLER_H_

#include <otsapi/otsapi.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <thrift/server/TServer.h>
#include <session/Session.h>

namespace ogon{ namespace sessionmanager{ namespace otsapi {

	class OTSApiHandler : virtual public otsapiIf {
	 public:
		OTSApiHandler();
		virtual ~OTSApiHandler();
		virtual void getVersionInfo(TVersion& _return, const TVersion& versionInfo);
		virtual void logonConnection(TReturnLogonConnection &_return,
			const TSTRING &username, const TSTRING &password, const TSTRING &domain);
		virtual TDWORD getPermissionForToken(const TSTRING &authToken);
		virtual bool logoffConnection(const TSTRING &authToken);
		virtual TDWORD ping(const TDWORD input);
		virtual void virtualChannelOpen(TReturnVirtualChannelOpen &_return,
			const TSTRING &authToken, const TDWORD sessionId,
			const TSTRING &virtualName, const TBOOL isDynChannel, const TDWORD flags);
		virtual bool virtualChannelClose(const TSTRING &authToken,
			const TDWORD sessionId, const TSTRING &virtualName, const TDWORD instance);
		virtual bool disconnectSession(const TSTRING &authToken,
			const TDWORD sessionId, const TBOOL wait);
		virtual bool logoffSession(const TSTRING &authToken,
			const TDWORD sessionId, const TBOOL wait);
		virtual void enumerateSessions(TReturnEnumerateSession &_return,
			const TSTRING &authToken, const TDWORD Version);
		virtual void querySessionInformation(TReturnQuerySessionInformation &_return,
			const TSTRING &authToken, const TDWORD sessionId, const TINT32 infoClass);
		virtual bool startRemoteControlSession(const TSTRING& authToken, const TDWORD sourceLogonId,
			const TDWORD targetLogonId, const TBYTE HotkeyVk,
			const TINT16 HotkeyModifiers, TDWORD flags);
		virtual bool stopRemoteControlSession(const TSTRING& authToken, const TDWORD sourceLogonId,
			const TDWORD targetLogonId);
		virtual TDWORD sendMessage(const TSTRING &authToken, const TDWORD sessionId,
			const TSTRING &title, const TSTRING &message, const TDWORD style,
			const TDWORD timeout, const TBOOL wait);

	 private:
		sessionNS::SessionPtr getSessionAndCheckForPerm(const TSTRING &authToken,
			UINT32 sessionId, DWORD requestedPermission);
	};
} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/

namespace otsapiNS = ogon::sessionmanager::otsapi;

#endif /* _OGON_SMGR_OTSAPI_HANDLER_H_ */
