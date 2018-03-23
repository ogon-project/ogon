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

#ifndef _OGON_SMGR_LOCALMODULE_H_
#define _OGON_SMGR_LOCALMODULE_H_

#include "Module.h"

namespace ogon { namespace sessionmanager { namespace module {

	/**
	 * @brief
	 */
	class LocalModule: public Module {
	public:
		LocalModule();
		virtual int initModule(const std::string &moduleFileName, RDS_MODULE_ENTRY_POINTS *entrypoints);
		virtual ~LocalModule();

		virtual RDS_MODULE_COMMON *newContext();
		virtual void freeContext(RDS_MODULE_COMMON *context);

		virtual std::string start(RDS_MODULE_COMMON *context);
		virtual int stop(RDS_MODULE_COMMON *context);

		virtual int connect(RDS_MODULE_COMMON *context);
		virtual int disconnect(RDS_MODULE_COMMON *context);

		virtual std::string getWinstationName(RDS_MODULE_COMMON *context);

	private:
		pRdsModuleInit mfpInit;
		pRdsModuleNew mfpNew;
		pRdsModuleFree mfpFree;

		pRdsModuleStart mfpStart;
		pRdsModuleStop mfpStop;
		pRdsModuleDestroy mfpDestroy;
		pRdsModuleGetCustomInfo mfpGetCustomInfo;
		pRdsModuleConnect mfpConnect;
		pRdsModuleDisconnect mfpDisconnect;
		HMODULE mLoadedLib;

	};

} /*module*/ } /*sessionmanager*/ } /*ogon*/

namespace moduleNS = ogon::sessionmanager::module;

#endif /* _OGON_SMGR_LOCALMODULE_H_ */
