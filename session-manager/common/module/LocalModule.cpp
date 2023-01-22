/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * LocalModule Class
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

#include "LocalModule.h"
#include <winpr/wlog.h>
#include <winpr/library.h>

namespace ogon { namespace sessionmanager { namespace module {

	static wLog *logger_LocalModule = WLog_Get("ogon.sessionmanager.module.localmodule");

	LocalModule::LocalModule()
		: mfpInit(nullptr),
		  mfpNew(nullptr),
		  mfpFree(nullptr),
		  mfpStart(nullptr),
		  mfpStop(nullptr),
		  mfpDestroy(nullptr),
		  mfpGetCustomInfo(nullptr),
		  mfpConnect(nullptr),
		  mfpDisconnect(nullptr),
		  mLoadedLib(nullptr) {}

	int LocalModule::initModule(const std::string &moduleFileName,
		RDS_MODULE_ENTRY_POINTS *entrypoints) {

		HANDLE hLib;

		hLib = LoadLibrary(moduleFileName.c_str());

		if (!hLib) {
			WLog_Print(logger_LocalModule, WLOG_ERROR,
					   "loading library %s failed", moduleFileName.c_str());
			return -1;
		}

		// get the exports
		pRdsModuleEntry entry = (pRdsModuleEntry) GetProcAddress(hLib, RDS_MODULE_ENTRY_POINT_NAME);

		if (!entry) {
			WLog_Print(logger_LocalModule, WLOG_ERROR,
					   "library %s is not a ogon module", moduleFileName.c_str());
			return -1;
		}


		mLoadedLib = hLib;
		if (!entrypoints){
			return -1;
		}

		setFileName(moduleFileName);

		if ((!entrypoints->Free) || (!entrypoints->New)|| (!entrypoints->Start)|| (!entrypoints->Stop)) {
			WLog_Print(logger_LocalModule, WLOG_ERROR,
				"not all passed function pointers are set for module %s",
					   moduleFileName.c_str());
			return -1;
		}

		if (!entrypoints->Name) {
			 WLog_Print(logger_LocalModule, WLOG_ERROR,
				"no ModuleName is set for module %s", moduleFileName.c_str());
			 return -1;
		}

		mfpInit = entrypoints->Init;
		mfpFree = entrypoints->Free;
		mfpNew = entrypoints->New;
		mfpStart = entrypoints->Start;
		mfpStop = entrypoints->Stop;
		mfpGetCustomInfo = entrypoints->getCustomInfo;
		mfpDestroy = entrypoints->Destroy;
		mfpConnect = entrypoints->Connect;
		mfpDisconnect = entrypoints->Disconnect;

		setName(std::string(entrypoints->Name));
		setVersion(entrypoints->Version);

		return mfpInit();
	}

	LocalModule::~LocalModule() {
		if (mLoadedLib) {
			mfpDestroy();
			FreeLibrary(mLoadedLib);
		}
	}

	RDS_MODULE_COMMON* LocalModule::newContext() {
		if (mfpNew)
			return mfpNew();
		return nullptr;
	}

	void LocalModule::freeContext(RDS_MODULE_COMMON *context) {
		if (mfpNew)
			mfpFree(context);
	}

	std::string LocalModule::start(RDS_MODULE_COMMON *context) {
		char *pipeName;
		std::string pipeNameStr;
		if (!mfpStart)
			return "";
		pipeName = mfpStart(context);

		if (pipeName) {
			pipeNameStr.assign(pipeName);
		}
		free(pipeName);
		return pipeNameStr;
	}

	int LocalModule::stop(RDS_MODULE_COMMON *context) {
		if (mfpStop) {
			return mfpStop(context);
		}
		return -1;
	}

	int LocalModule::connect(RDS_MODULE_COMMON *context) {
		if (mfpConnect) {
			return mfpConnect(context);
		}
		return -1;
	}

	int LocalModule::disconnect(RDS_MODULE_COMMON *context) {
		if (mfpDisconnect) {
			return mfpDisconnect(context);
		}
		return -1;
	}

	std::string LocalModule::getWinstationName(RDS_MODULE_COMMON *context) {
		char *customInfo;
		std::string customInfoStr;
		if (!mfpGetCustomInfo)
			return "";
		customInfo = mfpGetCustomInfo(context);
		if (customInfo == nullptr) {
			return getName() + ":";
		}
		customInfoStr.assign(customInfo);
		free(customInfo);
		return getName() + ":" + customInfoStr;
	}

} /*module*/ } /*sessionmanager*/ } /*ogon*/
