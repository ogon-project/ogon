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

#ifndef OGON_SMGR_AUTHMODULE_H_
#define OGON_SMGR_AUTHMODULE_H_

#include "../../auth.h"
#include "Module.h"
#include <winpr/wtypes.h>
#include <string>

namespace ogon { namespace sessionmanager { namespace module {

	/**
	 * @brief
	 */
	class AuthModule {
	public:
		AuthModule();
		int initModule(const std::string &moduleFileName, RDS_AUTH_MODULE_ENTRY_POINTS *entrypoints);
		virtual ~AuthModule();
		std::string getName();

		rdsAuthModule * newContext();
		void freeContext(rdsAuthModule *context);

		/* the domain can be modified by an auth module
		 * this is the case if the authmodule does not support domains
		 * then the authmodule should set the domain to the same value
		 * or "". */

		int logonUser(rdsAuthModule *context, const std::string &username, std::string &domain,
				const std::string &password);

	private:
		pRdsAuthModuleNew mfpNew;
		pRdsAuthModuleFree mfpFree;

		pRdsAuthLogonUser mfpLogonUser;

		std::string mModuleFile;
		std::string mModuleName;
		HMODULE mLoadedLib;
	};

} /*module*/ } /*sessionmanager*/ } /*ogon*/

namespace moduleNS = ogon::sessionmanager::module;

#endif /* OGON_SMGR_AUTHMODULE_H_ */
