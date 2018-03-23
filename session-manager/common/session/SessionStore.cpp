/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Session store class
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

#include "SessionStore.h"
#include <winpr/wlog.h>
#include <utils/CSGuard.h>

namespace ogon { namespace sessionmanager { namespace session {

	static wLog *logger_SessionStore = WLog_Get("ogon.sessionmanager.session.sessionstore");

	SessionStore::SessionStore() {
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_SessionStore, WLOG_FATAL,
				"Failed to initialize session store critical section");
			throw std::bad_alloc();
		}
		mNextSessionId = 1;
	}

	SessionStore::~SessionStore() {
		DeleteCriticalSection(&mCSection);
	}

	SessionPtr SessionStore::getSession(UINT32 sessionId) {
		CSGuard guard(&mCSection);
		if (mSessionMap.find(sessionId) != mSessionMap.end()) {
			return mSessionMap[sessionId];
		}
		return SessionPtr();
	}

	SessionPtr SessionStore::createSession() {
		CSGuard guard(&mCSection);

		do {
			mNextSessionId++;
			if (mNextSessionId == 0) {
				mNextSessionId++;
			}
		} while (mSessionMap.find(mNextSessionId) != mSessionMap.end());
		SessionPtr session(new Session(mNextSessionId));
		session->init();
		mSessionMap[session->getSessionID()] = session;
		return session;
	}

	SessionPtr SessionStore::getFirstSession(const std::string &username, const std::string &domain) {
		CSGuard guard(&mCSection);

		SessionPtr session;
		TSessionMap::iterator iter;

		for (iter = mSessionMap.begin(); iter != mSessionMap.end(); iter++) {
			if((iter->second->getUserName().compare(username) == 0) &&
					(iter->second->getDomain().compare(domain) == 0)) {
				session = iter->second;
				break;
			}
		}
		return session;
	}

	SessionPtr SessionStore::getFirstSession(const std::string &username,
		const std::string &domain, const std::string &clientHostName) {

		CSGuard guard(&mCSection);

		SessionPtr session;
		TSessionMap::iterator iter;

		for (iter = mSessionMap.begin(); iter != mSessionMap.end(); iter++) {
			if((iter->second->getUserName().compare(username) == 0) &&
					(iter->second->getDomain().compare(domain) == 0)) {
				if (clientHostName.compare(iter->second->getClientHostName()) == 0) {
					session = iter->second;
					break;
				}
			}
		}
		return session;
	}

	SessionPtr SessionStore::getFirstDisconnectedSession(const std::string &username,
		const std::string &domain) {

		CSGuard guard(&mCSection);

		SessionPtr session;
		TSessionMap::iterator iter;

		for (iter = mSessionMap.begin(); iter != mSessionMap.end(); iter++) {
			if((iter->second->getUserName().compare(username) == 0) &&
					(iter->second->getDomain().compare(domain) == 0)) {
				if (iter->second->getConnectState() == WTSDisconnected) {
					iter->second->setConnectState(WTSConnectQuery);
					session = iter->second;
					break;
				}
			}
		}
		return session;
	}

	SessionPtr SessionStore::getFirstDisconnectedSession(const std::string &username,
		const std::string &domain, const std::string &clientHostName) {

		CSGuard guard(&mCSection);

		SessionPtr session;
		TSessionMap::iterator iter;

		for (iter = mSessionMap.begin(); iter != mSessionMap.end(); iter++) {
			if((iter->second->getUserName().compare(username) == 0) &&
					(iter->second->getDomain().compare(domain) == 0)) {
				if ((iter->second->getConnectState() == WTSDisconnected)
						&& (clientHostName.compare(iter->second->getClientHostName())==0)) {
					iter->second->setConnectState(WTSConnectQuery);
					session = iter->second;
					break;
				}
			}
		}
		return session;
	}

	SessionPtr SessionStore::getFirstLoggedInSession(const std::string &username,
													 const std::string &domain) {

		CSGuard guard(&mCSection);

		SessionPtr session;
		TSessionMap::iterator iter;
		WTS_CONNECTSTATE_CLASS state;

		for (iter = mSessionMap.begin(); iter != mSessionMap.end(); iter++) {
			if((iter->second->getUserName().compare(username) == 0) &&
					(iter->second->getDomain().compare(domain) == 0)) {
				state = iter->second->getConnectState();
				if ((state == WTSDisconnected) ||
					(state == WTSActive) ||
					(state == WTSInit) ||
					(state == WTSShadow)) {
					session = iter->second;
					break;
				}
			}
		}
		return session;
	}

	SessionPtr SessionStore::getFirstLoggedInSession(const std::string &username,
													 const std::string &domain, const std::string &clientHostName){

		CSGuard guard(&mCSection);

		SessionPtr session;
		TSessionMap::iterator iter;
		WTS_CONNECTSTATE_CLASS state;

		for (iter = mSessionMap.begin(); iter != mSessionMap.end(); iter++) {
			if((iter->second->getUserName().compare(username) == 0) &&
					(iter->second->getDomain().compare(domain) == 0)) {
				state = iter->second->getConnectState();
				if ((state == WTSDisconnected) ||
					(state == WTSActive) ||
					(state == WTSInit) ||
					(state == WTSShadow)) {
					if ((clientHostName.compare(iter->second->getClientHostName()) == 0)) {
						session = iter->second;
						break;
					}
				}
			}
		}
		return session;
	}

	int SessionStore::removeSession(UINT32 sessionId) {
		SessionPtr session;
		EnterCriticalSection(&mCSection);
		TSessionMap::const_iterator iter = mSessionMap.find(sessionId);
		if (iter != mSessionMap.end()) {
			session = iter->second;
		}
		mSessionMap.erase(sessionId);
		LeaveCriticalSection(&mCSection);
		if (session) {
			session->shutdown();
		}
		return 0;
	}

	std::list<SessionPtr> SessionStore::getAllSessions() {
		CSGuard guard(&mCSection);
		std::list<SessionPtr> list;
		for (TSessionMap::const_iterator it = mSessionMap.begin(); it != mSessionMap.end(); ++it) {
			list.push_back( it->second );
		}
		return list;
	}

} /*session*/ } /*sessionmanager*/ } /*ogon*/
