/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Session accessor class
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

#ifndef _OGON_SMGR_SESSION_SESSIONACCESSOR_H_
#define _OGON_SMGR_SESSION_SESSIONACCESSOR_H_

#include <string>
#include <session/Session.h>
#include <winpr/wtsapi.h>

namespace ogon { namespace sessionmanager { namespace session {


	class SessionAccessor {

	public:
		SessionAccessor() {
		}

		~SessionAccessor() {
		}

		void setAccessorSession(SessionPtr session) {
			mAccessorSession = session;
		}

		void resetAccessorSession() {
			mAccessorSession.reset();
		}

		SessionPtr getAccessorSession() {
			return mAccessorSession;
		}

		void init() {
			mAccessorSession->init();
		}

		void shutdown() {
			mAccessorSession->shutdown();
		}

		void setDomain(const std::string &domainName) {
			mAccessorSession->setDomain(domainName);
		}

		void setUserName(const std::string &username) {
			mAccessorSession->setUserName(username);
		}

		void setAuthUserName(const std::string &username) {
			mAccessorSession->setAuthUserName(username);
		}

		void setAuthDomain(const std::string &domainName) {
			mAccessorSession->setAuthDomain(domainName);
		}

		void setClientHostName(const std::string &clientHostName) {
			mAccessorSession->setClientHostName(clientHostName);
		}

		bool generateUserToken() {
			return mAccessorSession->generateUserToken();
		}

		bool generateEnvBlockAndModify(const std::string &clientName, const std::string &clientAddress) {
			return mAccessorSession->generateEnvBlockAndModify(clientName, clientAddress);
		}

		bool generateAuthEnvBlockAndModify(const std::string &clientName, const std::string &clientAddress) {
			return mAccessorSession->generateAuthEnvBlockAndModify(clientName, clientAddress);
		}

		void initPermissions() {
			mAccessorSession->initPermissions();
		}

		void setModuleConfigName(const std::string &configName) {
			mAccessorSession->setModuleConfigName(configName);
		}

		bool startModule(std::string &pipeName) {
			return mAccessorSession->startModule(pipeName);
		}

		bool connectModule() {
			return mAccessorSession->connectModule();
		}

		bool disconnectModule() {
			return mAccessorSession->disconnectModule();
		}

		bool stopModule() {
			return mAccessorSession->stopModule();
		}

		void setConnectState(WTS_CONNECTSTATE_CLASS state) {
			mAccessorSession->setConnectState(state);
		}

		bool markBackendAsAuth() {
			return mAccessorSession->markBackendAsAuth();
		}

		void destroyAuthBackend() {
			mAccessorSession->destroyAuthBackend();
		}

		void restoreBackendFromAuth() {
			mAccessorSession->restoreBackendFromAuth();
		}

		void applyAuthTokenPermissions() {
			mAccessorSession->applyAuthTokenPermissions();
		}

		void disconnect() {
			mAccessorSession->disconnect();
		}

		void unregisterSession() {
			mAccessorSession->unregisterSession();
		}

		void removeAuthToken() {
			mAccessorSession->removeAuthToken();
		}

		void startRemoteControl() {
			mAccessorSession->startRemoteControl();
		}

		void stopRemoteControl() {
			mAccessorSession->stopRemoteControl();
		}

		void setSBPVersion(bool compatible) {
			mAccessorSession->setSBPVersion(compatible);
		}



private:
		SessionPtr mAccessorSession;

	};

}/*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* _OGON_SMGR_SESSION_SESSIONACCESSOR_H_ */
