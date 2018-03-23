/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Application Context
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

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/library.h>
#include <winpr/environment.h>

#include <list>
#include <utils/StringHelpers.h>

#include "ApplicationContext.h"

#include <session/TaskSessionTimeout.h>
#include <session/TaskDisconnect.h>
#include <session/TaskEnd.h>

#include <ogon/build-config.h>

namespace ogon { namespace sessionmanager {

	static wLog *logger_APPCONTEXT = WLog_Get("ogon.sessionmanager.appcontext");

	ApplicationContext::ApplicationContext() {
		mShutdown = false;

		initPaths();
		configureExecutableSearchPath();

	}

	ApplicationContext::~ApplicationContext() {
		WLog_Uninit();
	}

	void ApplicationContext::init(void) {
	}

	std::string ApplicationContext::getLibraryPath() const {
		return mLibraryPath;
	}

	std::string ApplicationContext::getExecutablePath() const {
		return mExecutablePath;
	}

	std::string ApplicationContext::getSystemConfigPath() const {
		return mSystemConfigPath;
	}

	std::string ApplicationContext::getModulePath() const {
		return mModulePath;
	}

	std::string ApplicationContext::getAuthModulePath() const {
		return mAuthModulePath;
	}

	void ApplicationContext::initPaths() {
		mLibraryPath = OGON_LIB_PATH;
		mExecutablePath = OGON_BIN_PATH;
		mSystemConfigPath = OGON_CFG_PATH;
		mModulePath = OGON_MODULE_LIB_PATH;
		mAuthModulePath = OGON_AUTH_MODULE_LIB_PATH;
	}

	void ApplicationContext::configureExecutableSearchPath() {
		char* path;

		if (!(path = getenv("PATH")))
			return;

		std::string pathExtra = "";
		bool executablePathPresent = false;
		bool systemConfigPathPresent = false;

		std::string pathEnv(path);
		std::vector<std::string> pathList = split<std::string>(pathEnv, ":");
		std::vector<std::string>::size_type index;

		for (index = 0; index < pathList.size(); index++) {
			if (mExecutablePath == pathList[index]) {
				executablePathPresent = true;
			} else if (mSystemConfigPath == pathList[index]) {
				systemConfigPathPresent = true;
			}
		}

		if (!executablePathPresent) {
			pathExtra += mExecutablePath;
			pathExtra += ":";
		}

		if (!systemConfigPathPresent) {
			pathExtra += mSystemConfigPath;
			pathExtra += ":";
		}

		if (pathExtra.length() > 0) {
			pathEnv = pathExtra + pathEnv;
			SetEnvironmentVariableA("PATH", pathEnv.c_str());
		}
	}

	sessionNS::SessionStore *ApplicationContext::getSessionStore() {
		return &mSessionStore;
	}

	sessionNS::SessionNotifier * ApplicationContext::getSessionNotifier(){
		return &mSessionNotifier;
	}

	sessionNS::ConnectionStore * ApplicationContext::getConnectionStore() {
		return &mConnectionStore;
	}

	configNS::PropertyManager * ApplicationContext::getPropertyManager() {
		return &mPropertyManager;
	}

	permissionNS::PermissionManager * ApplicationContext::getPermissionManager() {
		return &mPermissionManager;
	}

	processNS::ProcessMonitor *ApplicationContext::getProcessMonitor() {
		return &mProcessMonitor;
	}

	pbRPC::RpcEngine *ApplicationContext::getIcpEngine() {
		return &mRpcEngine;
	}

	bool ApplicationContext::startRPCEngines() {
		if (!mOTSApiServer.startOTSApi()) {
			WLog_Print(logger_APPCONTEXT, WLOG_FATAL, "OTSAPI thrift server startup FAILED!");
			return false;
		}
		if (!mRpcEngine.startEngine()) {
			WLog_Print(logger_APPCONTEXT, WLOG_FATAL, "ICP pbrpc server startup FAILED!");
			return false;
		}
		return true;
	}

	bool ApplicationContext::stopRPCEngines() {
		bool retval = true;
		if (!mRpcEngine.stopEngine()) {
			WLog_Print(logger_APPCONTEXT, WLOG_FATAL,
				"ICP pbrpc server server stop FAILED!");
			retval = false;
		}
		if (!mOTSApiServer.stopOTSApi()) {
			WLog_Print(logger_APPCONTEXT, WLOG_FATAL,
				"OTSAPI thrift server stop FAILED!");
			retval = false;
		}
		return retval;
	}

	SignalingQueue<callNS::CallPtr> * ApplicationContext::getRpcOutgoingQueue() {
		return &mRpcOutgoingCalls;
	}

	int ApplicationContext::loadModulesFromPath(const std::string &path) {
		LPCSTR pszExt;
		char pattern[256];

		pszExt = PathGetSharedLibraryExtensionA(0);
		sprintf_s(pattern, 256, "libogon-mod-*.%s", pszExt);

		return mModuleManager.loadModulesFromPathAndEnv(path, pattern);
	}

	int ApplicationContext::loadAuthModulesFromPath(const std::string &path) {
		LPCSTR pszExt;
		char pattern[256];

		pszExt = PathGetSharedLibraryExtensionA(0);
		sprintf_s(pattern, 256, "libogon-auth-*.%s", pszExt);
		return mModuleManager.loadModulesFromPathAndEnv(path, pattern);
	}

	moduleNS::ModuleManager* ApplicationContext::getModuleManager() {
		return &mModuleManager;
	}

	void ApplicationContext::setupDefaultValues() {
		mPropertyManager.setPropertyString(Global, 0, "ssl.certificate", "/etc/ssl/certs/ogon.crt");
		mPropertyManager.setPropertyString(Global, 0, "ssl.key", "/etc/ssl/private/ogon.key");
		mPropertyManager.setPropertyString(Global, 0, "tcp.keepalive.params", "-1,-1");

		mPropertyManager.setPropertyString(Global, 0, "module", "xsession");
		mPropertyManager.setPropertyString(Global, 0, "auth.module", "PAM");
		mPropertyManager.setPropertyString(Global, 0, "auth.greeter", "greeter");
		mPropertyManager.setPropertyBool(Global, 0, "session.reconnect", true);
		mPropertyManager.setPropertyBool(Global, 0, "session.reconnect.fromSameClient", false);
		mPropertyManager.setPropertyNumber(Global, 0, "session.timeout", 1);
		mPropertyManager.setPropertyBool(Global, 0, "session.singleSession",true);
		//mPropertyManager.setPropertyString(Global, 0, "environment.filter", "TEST;PATH");
		mPropertyManager.setPropertyString(Global, 0, "environment.filter", "");
		//mPropertyManager.setPropertyString(Global, 0, "environment.add", "TEST:value;TEST1:value1");
		mPropertyManager.setPropertyString(Global, 0, "environment.add", "");
		mPropertyManager.setPropertyString(Global, 0, "permission.level", "USER");
		mPropertyManager.setPropertyString(Global, 0, "permission.groups.whiteList", "*");
		mPropertyManager.setPropertyString(Global, 0, "permission.groups.blackList", "");

		mPropertyManager.setPropertyString(Global, 0, "virtualChannel.whiteList", "*");

		mPropertyManager.setPropertyNumber(Global, 0, "session.maxXRes", 1920);
		mPropertyManager.setPropertyNumber(Global, 0, "session.maxYRes", 1200);
		mPropertyManager.setPropertyNumber(Global, 0, "session.xres", 1024);
		mPropertyManager.setPropertyNumber(Global, 0, "session.yres", 768);
		mPropertyManager.setPropertyNumber(Global, 0, "session.colordepth", 24);

		// Module Config
		mPropertyManager.setPropertyString(Global, 0, "module.xsession.modulename", "X11");
		mPropertyManager.setPropertyString(Global, 0, "module.xsession.startwm", "ogonXsession");
		mPropertyManager.setPropertyBool  (Global, 0, "module.xsession.uselauncher", true);
		std::string fullpath = OGON_SBIN_PATH;
		fullpath += "/ogon-backend-launcher";
		mPropertyManager.setPropertyBool  (Global, 0, "module.xsession.launcherexecutable", fullpath.c_str());
		//mPropertyManager.setPropertyString(Global, 0, "module.xsession.launcherdebugfile", "/tmp/launcherlog");


		mPropertyManager.setPropertyString(Global, 0, "module.greeter.modulename", "Qt");
		fullpath = OGON_BIN_PATH;
		fullpath += "/ogon-qt-greeter";
		mPropertyManager.setPropertyString(Global, 0, "module.greeter.cmd", fullpath.c_str());
		mPropertyManager.setPropertyBool  (Global, 0, "module.greeter.uselauncher", false);
		//mPropertyManager.setPropertyString(Global, 0, "module.greeter.launcherdebugfile", "/tmp/launcherlog");
	}

	bool ApplicationContext::startTaskExecutor() {
		return mTaskExecutor.start();
	}

	bool ApplicationContext::stopTaskExecutor() {
		return mTaskExecutor.stop();
	}

	void ApplicationContext::startSessionTimoutMonitor() {
		sessionNS::TaskSessionTimeoutPtr task(new sessionNS::TaskSessionTimeout());
		addTask(task);
	}

	bool ApplicationContext::addTask(taskNS::TaskPtr task) {
		return mTaskExecutor.addTask(task);
	}

	void ApplicationContext::rpcDisconnected() {
		// remove all connections
		getConnectionStore()->reset();
		// iterate over the session and disconnect them if they are auth sessions.
		std::list<sessionNS::SessionPtr> allSessions = getSessionStore()->getAllSessions();

		std::list<sessionNS::SessionPtr>::iterator iterator;
		for (iterator = allSessions.begin(); iterator != allSessions.end(); ++iterator) {
			sessionNS::SessionPtr currentSession = (*iterator);
			sessionNS::TaskDisconnectPtr taskDisconnect(new sessionNS::TaskDisconnect(0, currentSession->getSessionID()));
			currentSession->addTask(taskDisconnect);
		}
	}

	void ApplicationContext::shutdown() {
		std::list<sessionNS::SessionPtr> allSessions = getSessionStore()->getAllSessions();
		std::list<sessionNS::SessionPtr>::iterator iterator;
		for (iterator = allSessions.begin(); iterator != allSessions.end(); ++iterator) {
			sessionNS::TaskEndPtr endTask(new sessionNS::TaskEnd());
			sessionNS::SessionPtr currentSession = (*iterator);
			endTask->setSessionId(currentSession->getSessionID());
			currentSession->addTask(endTask);
			WaitForSingleObject(endTask->getHandle(), INFINITE);
			currentSession->stopExecutorThread(true);
		}
	}

	bool ApplicationContext::isShutdown() {
		return mShutdown;
	}

	void ApplicationContext::setShutdown() {
		mShutdown = true;
	}

	bool ApplicationContext::startSessionNotifier() {
		return mSessionNotifier.init();
	}

	bool ApplicationContext::stopSessionNotifier() {
		return mSessionNotifier.shutdown();
	}

	bool ApplicationContext::startProcessMonitor() {
		return mProcessMonitor.start();
	}

	bool ApplicationContext::stopProcessMonitor() {
		return mProcessMonitor.stop();

	}

	bool ApplicationContext::loadConfig(const std::string &name) {
		bool result;

		WLog_Print(logger_APPCONTEXT, WLOG_INFO, "Loading / reloading config %s!", name.c_str());
		result = mPropertyManager.loadProperties(name);
		mPermissionManager.reloadAllowedUsers();
		return result;
	}

} /*sessionmanager*/ } /*ogon*/

