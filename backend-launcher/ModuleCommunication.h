/**
 * ogon - Free Remote Desktop Services
 * Backend Process Launcher
 * Module Class for handling communication with session-manager
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#ifndef _OGON_BACKENDLAUNCHER_MODULECOMMUNICATION_H_
#define _OGON_BACKENDLAUNCHER_MODULECOMMUNICATION_H_

#include <security/pam_appl.h>
#include <string>
#include "../session-manager/common/module/RemoteModuleTransport.h"
#include "SystemSession.h"

using namespace ogon::sessionmanager::module;

namespace ogon { namespace launcher  {

#define REMOTE_CLIENT_EXIT 1007

	typedef void (*pSignalStop)();

	class ModuleCommunication: public RemoteModuleTransport {

	public:
		ModuleCommunication(pgetPropertyBool getPropBool, pgetPropertyNumber getPropNumber, pgetPropertyString getPropString,
							pAddMonitoringProcess addMonitoring, pRemoveMonitoringProcess removeMonitoring, pSignalStop signalStop);
		~ModuleCommunication();
		UINT doRead();
		void initHandles(HANDLE readHandle, HANDLE writeHandle);

		bool getPropertyBool(UINT32 sessionID, const char* path, bool* value);
		bool getPropertyNumber(UINT32 sessionID, const char* path, long* value);
		bool getPropertyString(UINT32 sessionID, const char* path, char* value, unsigned int valueLength);
		void setSessionPID(pid_t sessionPID);
		bool stopModule();


	protected:

		virtual UINT processCall(RemoteModuleTransportContext &context, const UINT32 callID, const UINT32 callType,
								 const bool isResponse, const std::string &payload, void* customData);

	private:
		pgetPropertyBool mGetPropertyBool;
		pgetPropertyNumber mGetPropertyNumber;
		pgetPropertyString mGetPropertyString;
		pAddMonitoringProcess mAddMonitoringProcess;
		pRemoveMonitoringProcess mRemoveMonitoringProcess;
		pSignalStop mStop;

		SystemSession session;
		RemoteModuleTransportContext mContext;
		UINT processModuleStart(const std::string &payload, const UINT32 callID);
		UINT processModuleStop(const std::string &payload, const UINT32 callID);
		UINT processModuleGetCustomInfo(const std::string &payload, const UINT32 callID);
		UINT processModuleConnect(const std::string &payload, const UINT32 callID);
		UINT processModuleDisconnect(const std::string &payload, const UINT32 callID);
		UINT processModuleExit(const std::string &payload, const UINT32 callID);
		UINT generateUserToken();
		UINT loadModule();
		UINT processGetPropertyBool(const std::string &payload, void *customData);
		UINT processGetPropertyNumber(const std::string &payload, void *customData);
		UINT processGetPropertyString(const std::string &payload, void *customData);

		void ogonModule(RDS_MODULE_COMMON *module);

		UINT32 mSessionId;
		pid_t mSessionPID;
		std::string mUserName;
		std::string mDomain;
		std::string mBaseConfigPath;
		std::string mEnv;
		std::string mModuleFileName;
		std::string mRemoteIp;
		HANDLE mUserToken;
		bool mSessionStarted;
		bool mStartSystemSession;

		HMODULE mModuleLib;
		RDS_MODULE_ENTRY_POINTS mEntrypoints;
		RDS_MODULE_COMMON *mModuleContext;
	};

} /* launcher */ } /* ogon */

#endif /* _OGON_BACKENDLAUNCHER_MODULECOMMUNICATION_H_ */
