/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * AuthModule
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

#include "AuthModule.h"
#include <winpr/wlog.h>
#include <winpr/library.h>

namespace ogon { namespace sessionmanager { namespace module {

	static wLog *logger_AuthModule = WLog_Get("ogon.sessionmanager.module.authmodule");

	AuthModule::AuthModule()
		: mfpNew(nullptr),
		  mfpFree(nullptr),
		  mfpLogonUser(nullptr),
		  mLoadedLib(nullptr) {}

	int AuthModule::initModule(const std::string &moduleFileName, RDS_AUTH_MODULE_ENTRY_POINTS *entrypoints) {

		if (!entrypoints) {
			return -1;
		}

		mLoadedLib = LoadLibrary(moduleFileName.c_str());

		if (!mLoadedLib) {
			WLog_Print(logger_AuthModule, WLOG_ERROR,
					   "loading library %s failed", moduleFileName.c_str());
			return -1;
		}

		mModuleFile = moduleFileName;

		if ((!entrypoints->Free) || (!entrypoints->New)|| (!entrypoints->LogonUser)) {
			WLog_Print(logger_AuthModule, WLOG_ERROR,
				"not all passed function pointers are set for module %s",
				mModuleFile.c_str());
			return -1;
		}

		if (!entrypoints->Name) {
			 WLog_Print(logger_AuthModule, WLOG_ERROR,
				"no ModuleName is set for module %s",
				mModuleFile.c_str());
			 return -1;
		}

		mfpFree = entrypoints->Free;
		mfpNew = entrypoints->New;
		mfpLogonUser = entrypoints->LogonUser;
		mModuleName = std::string(entrypoints->Name);

		return 0;
	}

	AuthModule::~AuthModule() {
		if (mLoadedLib) {
			FreeLibrary(mLoadedLib);
		}
	}

	std::string AuthModule::getName() {
		return mModuleName;
	}

	rdsAuthModule * AuthModule::newContext() {
		return mfpNew();
	}

	void AuthModule::freeContext(rdsAuthModule *context) {
		return mfpFree(context);
	}

	/* the domain can be modified by an auth module
	 * this is the case if the authmodule does not support domains
	 * then the authmodule should set the domain to the same value
	 * or "". */

	int AuthModule::logonUser(rdsAuthModule *context, const std::string &username,
		std::string &domain, const std::string &password) {
		char * domainName = const_cast<char*>(domain.c_str());
		int error =  mfpLogonUser(context,username.c_str(), &domainName, password.c_str());
		if (domainName != domain.c_str())
		{
			domain = std::string(domainName);
			free(domainName);
		}
		return error;
	}

} /*module*/ } /*sessionmanager*/ } /*ogon*/
