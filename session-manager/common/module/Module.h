/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Base Class for Modules
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

#ifndef OGON_SMGR_MODULE_H_
#define OGON_SMGR_MODULE_H_

#include <ogon/module.h>
#include <winpr/wtypes.h>
#include <string>

namespace ogon { namespace sessionmanager { namespace module {

	/**
	 * @brief
	 */
	class Module {
	public:
		Module() {};
		virtual ~Module() {};

		virtual int initModule(const std::string &moduleFileName, RDS_MODULE_ENTRY_POINTS *entrypoints) = 0;
		std::string getName() const {
			return mModuleName;
		};

		std::string getModuleFile() const {
			return mModuleFile;
		}

		DWORD getVersion() const {
			return mVersion;
		}

		virtual RDS_MODULE_COMMON *newContext() = 0;
		virtual void freeContext(RDS_MODULE_COMMON *context) = 0;

		virtual std::string start(RDS_MODULE_COMMON *context) = 0;
		virtual int stop(RDS_MODULE_COMMON *context) = 0;

		virtual int connect(RDS_MODULE_COMMON *context) = 0;
		virtual int disconnect(RDS_MODULE_COMMON *context) = 0;

		virtual std::string getWinstationName(RDS_MODULE_COMMON *context) = 0;

	protected:
		void setName(std::string name) {
			mModuleName = name;
		}

		void setFileName(std::string fileName) {
			mModuleFile = fileName;
		}

		void setVersion(DWORD version) {
			mVersion = version;
		}

	private:
		DWORD mVersion;
		std::string mModuleFile;
		std::string mModuleName;
	};

} /*module*/ } /*sessionmanager*/ } /*ogon*/

namespace moduleNS = ogon::sessionmanager::module;

#endif /* OGON_SMGR_MODULE_H_ */
