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

#ifndef OGON_SMGR_APPLICATIONCONTEXT_H_
#define OGON_SMGR_APPLICATIONCONTEXT_H_

#include <utils/SingletonBase.h>
#include <utils/SignalingQueue.h>
#include <session/SessionStore.h>
#include <session/ConnectionStore.h>
#include <pbRPC/RpcEngine.h>
#include <winpr/wlog.h>
#include <call/CallOut.h>
#include <module/ModuleManager.h>
#include <config/PropertyManager.h>
#include <otsapi/OTSApiServer.h>
#include <task/Executor.h>
#include <permission/PermissionManager.h>
#include <session/SessionNotifier.h>
#include <process/ProcessMonitor.h>

#define APP_CONTEXT ogon::sessionmanager::ApplicationContext::instance()

namespace ogon { namespace sessionmanager {

	/**
	 * @brief
	 */
	class ApplicationContext: public SingletonBase<ApplicationContext> {
	public:
		/** @return the session store */
		sessionNS::SessionStore *getSessionStore();

		/** @return the session notifier */
		sessionNS::SessionNotifier *getSessionNotifier();

		/** @return the connection store */
		sessionNS::ConnectionStore *getConnectionStore();

		/** @return the property manager */
		configNS::PropertyManager *getPropertyManager();

		/** @return the module manager */
		moduleNS::ModuleManager *getModuleManager();

		/** @return the permission manager */
		permissionNS::PermissionManager *getPermissionManager();

		/** @return the processs monitor */
		processNS::ProcessMonitor *getProcessMonitor();

		/** @return the ICP RPC engine */
		pbRPC::RpcEngine *getIcpEngine();

		/**
		 * starts the ICP and thrift engines
		 * @return if the operation was successful
		 */
		bool startRPCEngines();

		/**
		 * stops the RPC engines (ICP and thrift)
		 * @return if the operation completed successfully
		 */
		bool stopRPCEngines();

		bool startTaskExecutor();
		bool stopTaskExecutor();
		void startSessionTimoutMonitor();
		bool addTask(taskNS::TaskPtr task);

		bool startSessionNotifier();
		bool stopSessionNotifier();

		bool startProcessMonitor();
		bool stopProcessMonitor();

		std::string getLibraryPath() const;
		std::string getExecutablePath() const;
		std::string getSystemConfigPath() const;
		std::string getModulePath() const;
		std::string getAuthModulePath() const;

		SignalingQueue<callNS::CallPtr> *getRpcOutgoingQueue();

		int loadModulesFromPath(const std::string &path);
		int loadAuthModulesFromPath(const std::string &path);
		void setupDefaultValues();

		void rpcDisconnected();

		void shutdown();

		bool isShutdown();
		void setShutdown();
		void init();

		bool loadConfig(const std::string &name);

	private:

		void initPaths();
		void configureExecutableSearchPath();

		std::string mLibraryPath;
		std::string mExecutablePath;
		std::string mSystemConfigPath;
		std::string mModulePath;
		std::string mAuthModulePath;

		bool mShutdown;

		taskNS::Executor mTaskExecutor;
		sessionNS::SessionStore mSessionStore;
		sessionNS::SessionNotifier mSessionNotifier;
		sessionNS::ConnectionStore mConnectionStore;

		configNS::PropertyManager mPropertyManager;
		pbRPC::RpcEngine mRpcEngine;
		SignalingQueue<callNS::CallPtr> mRpcOutgoingCalls;
		moduleNS::ModuleManager mModuleManager;
		permissionNS::PermissionManager mPermissionManager;
		otsapiNS::OTSApiServer mOTSApiServer;
		processNS::ProcessMonitor mProcessMonitor;
		SINGLETON_ADD_INITIALISATION(ApplicationContext)
	};

} /*sessionmanager*/ } /*ogon*/

namespace appNS = ogon::sessionmanager;

#endif /* OGON_SMGR_APPLICATIONCONTEXT_H_ */
