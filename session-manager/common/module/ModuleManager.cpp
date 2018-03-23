/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Module Manager
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

#include "ModuleManager.h"
#include "LocalModule.h"

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/library.h>

#include <config/PropertyCWrapper.h>
#include <utils/StringHelpers.h>

#include <appcontext/ApplicationContext.h>

#include "CallBacks.h"
#include "RemoteModule.h"

namespace ogon { namespace sessionmanager { namespace module {

	static wLog *logger_ModuleManager = WLog_Get("ogon.sessionmanager.module.modulemanager");

	ModuleManager::ModuleManager() {
		this->pathSeparator = PathGetSeparatorA(PATH_STYLE_NATIVE);
	}

	ModuleManager::~ModuleManager() {
		unloadAll();
	}

	int ModuleManager::loadModulesFromPathAndEnv(const std::string &path,
		const std::string &pattern)
	{
		char* mods = getenv(MODULE_ENV_VAR);

		if (mods) {
			WLog_Print(logger_ModuleManager, WLOG_TRACE,
				"found env variable %s with content '%s'", MODULE_ENV_VAR, mods);

			std::string envpath(mods);
			std::vector<std::string> pathList = split<std::string>(envpath, ":");

			for (std::vector<std::string>::size_type run = 0; run < pathList.size(); run++) {
				loadModulesFromPath(pathList[run], pattern);
			}
		} else {
			WLog_Print(logger_ModuleManager, WLOG_TRACE,
				"did not find env variable '%s'", MODULE_ENV_VAR);
		}
		loadModulesFromPath(path, pattern);
		return 0;
	}

	int ModuleManager::loadModulesFromPath(const std::string &path, const std::string &pattern) {

		HANDLE hFind;
		WIN32_FIND_DATA FindFileData;
		std::string fullsearch = path + pathSeparator + pattern;

		hFind = FindFirstFile(fullsearch.c_str(), &FindFileData);
		WLog_Print(logger_ModuleManager, WLOG_TRACE,
			"scanning with in directory %s for modules", fullsearch.c_str());

		if (hFind == INVALID_HANDLE_VALUE) {
			WLog_Print(logger_ModuleManager, WLOG_ERROR,
				"FindFirstFile (path = %s) failed", fullsearch.c_str());
			return -1;
		}

		do {
			if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
				// try to add this module ...
				addModule(path, std::string(FindFileData.cFileName));
			}
		} while (FindNextFile(hFind, &FindFileData) != 0);
		FindClose(hFind);
		return 0;
	}

	int ModuleManager::addAuthModuleInternal(pRdsAuthModuleEntry entry, const std::string &fullFileName) {

		int result = 0;
		RDS_AUTH_MODULE_ENTRY_POINTS entrypoints;

		ZeroMemory(&entrypoints, sizeof(RDS_AUTH_MODULE_ENTRY_POINTS));

		result = entry(&entrypoints);

		if (result != 0) {
			WLog_Print(logger_ModuleManager, WLOG_ERROR,
				"library %s function %s reported error %d", fullFileName.c_str(),
				RDS_AUTH_MODULE_ENTRY_POINT_NAME, result);
			return -1;
		}

		// no error occurred
		AuthModule *module = new AuthModule();
		if (module->initModule(fullFileName, &entrypoints) != 0) {
			WLog_Print(logger_ModuleManager, WLOG_ERROR,
				"library %s not loaded", fullFileName.c_str());
			delete module;
			return -1;
		}

		// check if module with same name is registered.
		if (mAuthModulesMap.count(module->getName())) {
			WLog_Print(logger_ModuleManager, WLOG_INFO,
				"library %s loaded, but another library has already registred authmodulename %s",
				fullFileName.c_str(), module->getName().c_str());
			delete module;
			return -1;
		} else {
			WLog_Print(logger_ModuleManager, WLOG_INFO,
				"library (AuthModel) %s loaded properly", fullFileName.c_str());
			// add this module to the map
			mAuthModulesMap.insert(std::pair<std::string,AuthModule *>(module->getName(), module));
		}
		return 0;
	}

	int ModuleManager::addStdModuleLocalInternal(pRdsModuleEntry entry, const std::string &fullFileName) {

		int result = 0;

		RDS_MODULE_ENTRY_POINTS entrypoints;
		// found entrypoint
		ZeroMemory(&entrypoints, sizeof(RDS_MODULE_ENTRY_POINTS));
		// setting the property callbacks
		entrypoints.config.getPropertyBool = getPropertyBool;
		entrypoints.config.getPropertyNumber = getPropertyNumber;
		entrypoints.config.getPropertyString = getPropertyString;
		entrypoints.status.addMonitoringProcess = CallBacks::addMonitoringProcess;
		entrypoints.status.removeMonitoringProcess = CallBacks::removeMonitoringProcess;
		result = entry(&entrypoints);

		if (result != 0) {
			WLog_Print(logger_ModuleManager, WLOG_ERROR,
				"library %s function %s reported error %d", fullFileName.c_str(),
				RDS_MODULE_ENTRY_POINT_NAME, result);
			return -1;
		}

		Module *module = new LocalModule();

		if (module->initModule(fullFileName, &entrypoints) != 0) {
			WLog_Print(logger_ModuleManager, WLOG_ERROR,
				"library %s not loaded", fullFileName.c_str());
			delete module;
			return -1;
		}

		// check if module with same name is registered.
		if (mModulesLocalMap.count(module->getName())) {
			WLog_Print(logger_ModuleManager, WLOG_INFO,
				"library %s loaded, but another library has already registred modulename %s",
				fullFileName.c_str(), module->getName().c_str());
			delete module;
			return -1;
		} else {
			WLog_Print(logger_ModuleManager, WLOG_INFO,
				"library (Module) %s loaded properly", fullFileName.c_str());
			// add this module to the map
			mModulesLocalMap.insert(std::pair<std::string,Module *>(module->getName(), module));
		}
		return 0;
	}


	int ModuleManager::addStdModuleRemoteInternal(pRdsModuleEntry entry, const std::string &fullFileName) {

		int result = 0;

		RDS_MODULE_ENTRY_POINTS entrypoints;
		// found entrypoint
		ZeroMemory(&entrypoints, sizeof(RDS_MODULE_ENTRY_POINTS));
		// setting the property callbacks
		entrypoints.config.getPropertyBool = getPropertyBool;
		entrypoints.config.getPropertyNumber = getPropertyNumber;
		entrypoints.config.getPropertyString = getPropertyString;
		entrypoints.status.addMonitoringProcess = CallBacks::addMonitoringProcess;
		entrypoints.status.removeMonitoringProcess = CallBacks::removeMonitoringProcess;
		result = entry(&entrypoints);

		if (result != 0) {
			WLog_Print(logger_ModuleManager, WLOG_ERROR,
					   "library %s function %s reported error %d", fullFileName.c_str(),
					   RDS_MODULE_ENTRY_POINT_NAME, result);
			return -1;
		}

		Module *module = new RemoteModule();

		if (module->initModule(fullFileName, &entrypoints) != 0) {
			WLog_Print(logger_ModuleManager, WLOG_ERROR,
					   "library %s not loaded", fullFileName.c_str());
			delete module;
			return -1;
		}

		// check if module with same name is registered.
		if (mModulesRemoteMap.count(module->getName())) {
			WLog_Print(logger_ModuleManager, WLOG_INFO,
					   "library %s loaded, but another library has already registred modulename %s",
					   fullFileName.c_str(), module->getName().c_str());
			delete module;
			return -1;
		} else {
			WLog_Print(logger_ModuleManager, WLOG_INFO,
					   "library (Module) %s loaded properly", fullFileName.c_str());
			// add this module to the map
			mModulesRemoteMap.insert(std::pair<std::string,Module *>(module->getName(), module));
		}
		return 0;
	}


	int ModuleManager::addModule(const std::string &path, const std::string &modulename) {
		HMODULE hLib;
		pRdsModuleEntry entry;
		pRdsAuthModuleEntry authEntry;
		int returnValue = 0;

		std::string fullFileName = path + pathSeparator + modulename;

		WLog_Print(logger_ModuleManager, WLOG_TRACE, "loading library %s",
			fullFileName.c_str());

		hLib = LoadLibrary(fullFileName.c_str());

		if (!hLib) {
			WLog_Print(logger_ModuleManager, WLOG_ERROR,
				"loading library %s failed", fullFileName.c_str());
			return -1;
		}

		// get the exports
		entry = (pRdsModuleEntry) GetProcAddress(hLib, RDS_MODULE_ENTRY_POINT_NAME);

		if (entry) {
			if ((returnValue = addStdModuleLocalInternal(entry, fullFileName))) {
				WLog_Print(logger_ModuleManager, WLOG_ERROR,
						   "addStdModuleLocalInternal for library %s failed!", fullFileName.c_str());
				FreeLibrary(hLib);
				return returnValue;
			}
			if ((returnValue = addStdModuleRemoteInternal(entry, fullFileName)))
				WLog_Print(logger_ModuleManager, WLOG_ERROR,
						   "addStdModuleRemoteInternal for library %s failed!", fullFileName.c_str());
			FreeLibrary(hLib);
			return returnValue;

		}

		authEntry = (pRdsAuthModuleEntry) GetProcAddress(hLib, RDS_AUTH_MODULE_ENTRY_POINT_NAME);

		if (authEntry) {
			if ((returnValue = addAuthModuleInternal(authEntry, fullFileName)))
				WLog_Print(logger_ModuleManager, WLOG_ERROR,
						   "addAuthModuleInternal for library %s failed!", fullFileName.c_str());
			FreeLibrary(hLib);
			return returnValue;
		}

		WLog_Print(logger_ModuleManager, WLOG_ERROR,
			"library %s does not export function %s or %s",
			fullFileName.c_str(), RDS_MODULE_ENTRY_POINT_NAME,
			RDS_AUTH_MODULE_ENTRY_POINT_NAME);
		return -1;
	}

	Module *ModuleManager::getModule(const std::string &moduleName, bool useLauncher) {

		if (useLauncher) {
			if (mModulesRemoteMap.count(moduleName)) {
				return mModulesRemoteMap[moduleName];
			} else {
				return NULL;
			}
		}

		if (mModulesLocalMap.count(moduleName)) {
			return mModulesLocalMap[moduleName];
		} else {
			return NULL;
		}
	}

	AuthModule *ModuleManager::getAuthModule(const std::string &moduleName) {
		if (mAuthModulesMap.count(moduleName)) {
			return mAuthModulesMap[moduleName];
		}
		return NULL;
	}


	void ModuleManager::unloadAll() {
		std::map<std::string,Module *>::iterator iter;

		for (iter = mModulesLocalMap.begin(); iter != mModulesLocalMap.end(); iter++) {
			delete iter->second;
		}

		for (iter = mModulesRemoteMap.begin(); iter != mModulesRemoteMap.end(); iter++) {
			delete iter->second;
		}

		std::map<std::string,AuthModule *>::iterator iterAuth;

		for (iterAuth = mAuthModulesMap.begin(); iterAuth != mAuthModulesMap.end(); iterAuth++) {
			delete iterAuth->second;
		}
	}

} /*module*/ } /*sessionmanager*/ } /*ogon*/
