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

#ifndef OGON_SMGR_MODULEMANAGER_H_
#define OGON_SMGR_MODULEMANAGER_H_

#include "Module.h"
#include "AuthModule.h"
#include <string>
#include <map>

#define MODULE_ENV_VAR "OGON_ADDITIONAL_MODULES"

namespace ogon { namespace sessionmanager { namespace module {

	class ModuleManager {
	public:
		ModuleManager();
		virtual ~ModuleManager();

		int loadModulesFromPath(const std::string &path, const std::string &pattern);
		int loadModulesFromPathAndEnv(const std::string &path, const std::string &pattern);

		Module *getModule(const std::string &moduleName, bool useLauncher);
		AuthModule *getAuthModule(const std::string &moduleName);

	private:
		char pathSeparator;
		int addModule(const std::string &path, const std::string &modulename);
		std::map<std::string,Module *> mModulesLocalMap;
		std::map<std::string,Module *> mModulesRemoteMap;
		std::map<std::string,AuthModule *> mAuthModulesMap;
		void unloadAll();
		int addAuthModuleInternal(pRdsAuthModuleEntry entry, const std::string &fullFileName);
		int addStdModuleLocalInternal(pRdsModuleEntry entry, const std::string &fullFileName);
		int addStdModuleRemoteInternal(pRdsModuleEntry entry, const std::string &fullFileName);
	};

} /*module*/ } /*sessionmanager*/ } /*ogon*/

namespace moduleNS = ogon::sessionmanager::module;

#endif /* OGON_SMGR_MODULEMANAGER_H_ */
