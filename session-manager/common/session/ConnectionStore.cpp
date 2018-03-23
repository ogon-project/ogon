/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Connection store class
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

#include "ConnectionStore.h"
#include <winpr/wlog.h>
#include <utils/CSGuard.h>

namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_ConnectionStore = WLog_Get("ogon.sessionmanager.session.connectionstore");

	ConnectionStore::ConnectionStore() {
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_ConnectionStore, WLOG_FATAL,
				"Failed to initialize connection store critical section");
			throw std::bad_alloc();
		}
	}

	ConnectionStore::~ConnectionStore() {
		DeleteCriticalSection(&mCSection);
	}

	ConnectionPtr ConnectionStore::getOrCreateConnection(UINT32 connectionID) {
		CSGuard guard(&mCSection);
		if (mConnectionMap.find(connectionID) != mConnectionMap.end()) {
			return mConnectionMap[connectionID];
		}

		ConnectionPtr connection = ConnectionPtr(new Connection(connectionID));
		mConnectionMap[connectionID] = connection;
		return connection;
	}

	ConnectionPtr ConnectionStore::getConnection(UINT32 connectionId) {
		CSGuard guard(&mCSection);
		TConnectionMap::const_iterator iter = mConnectionMap.find(connectionId);
		if (iter != mConnectionMap.end()) {
			return iter->second;
		}
		return ConnectionPtr();
	}

	ConnectionPtr ConnectionStore::getConnectionForSessionId(UINT32 mSessionId) {

		CSGuard guard(&mCSection);
		ConnectionPtr connection;
		TConnectionMap::iterator iter;

		for (iter = mConnectionMap.begin(); iter != mConnectionMap.end(); iter++) {
			if((iter->second->getSessionId() == mSessionId)) {
				connection = iter->second;
				break;
			}
		}
		return connection;
	}



	int ConnectionStore::removeConnection(UINT32 connectionID) {
		CSGuard guard(&mCSection);
		mConnectionMap.erase(connectionID);
		return 0;
	}

	UINT32 ConnectionStore::getConnectionIdForSessionId(UINT32 mSessionId) {
		CSGuard guard(&mCSection);
		UINT32 connectionId = 0;

		TConnectionMap::iterator iter;

		for (iter = mConnectionMap.begin(); iter != mConnectionMap.end(); iter++) {
			if((iter->second->getSessionId() == mSessionId)) {
				connectionId = iter->first;
				break;
			}
		}
		return connectionId;
	}

	void ConnectionStore::reset() {
		CSGuard guard(&mCSection);
		mConnectionMap.clear();
	}

} /*session*/ } /*sessionmanager*/ } /*ogon*/
