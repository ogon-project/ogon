/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Property Manager to store and receive the ogon config
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

#ifndef OGON_SMGR_PROPERTYMANAGER_H_
#define OGON_SMGR_PROPERTYMANAGER_H_

#include <string>
#include <map>

#include <boost/property_tree/ptree.hpp>

#include <winpr/synch.h>

#include "PropertyLevel.h"

namespace ogon { namespace sessionmanager { namespace config {

	typedef std::map<std::string, PROPERTY_STORE_HELPER> TPropertyMap;
	typedef std::pair<std::string, PROPERTY_STORE_HELPER> TPropertyPair;

	typedef std::map<std::string, TPropertyMap *> TPropertyPropertyMap;
	typedef std::pair<std::string, TPropertyMap *> TPropertyPropertyPair;


	/**
	 * @brief
	 */
	class PropertyManager {
	public:
		PropertyManager();
		~PropertyManager();

		bool getPropertyBool(UINT32 sessionID, const std::string &path, bool &value,
				const std::string &username = "") const;
		bool getPropertyNumber(UINT32 sessionID, const std::string &path, long &value,
				const std::string &username = "") const;
		bool getPropertyString(UINT32 sessionID, const std::string &path, std::string &value,
				const std::string &username = "") const;

		bool setPropertyBool(PROPERTY_LEVEL level, UINT32 sessionID, const std::string &path,
				bool value, const std::string &username = "");
		bool setPropertyNumber(PROPERTY_LEVEL level, UINT32 sessionID, const std::string &path,
				long value, const std::string &username = "");
		bool setPropertyString(PROPERTY_LEVEL level, UINT32 sessionID, const std::string &path,
				const std::string &value, const std::string &username = "");

		bool saveProperties(const std::string &filename);
		bool loadProperties(const std::string &filename);
		static bool checkConfigFile(const std::string &filename);

	private:
		bool parsePropertyGlobal(const std::string &parentPath, const boost::property_tree::ptree &tree,
				PROPERTY_LEVEL level);
		bool setPropertyInternal(PROPERTY_LEVEL level, UINT32 sessionID, const std::string &path,
				PROPERTY_STORE_HELPER helper, const std::string &username);
		bool getPropertyInternal(UINT32 sessionID, const std::string &path, PROPERTY_STORE_HELPER &helper,
				const std::string &username) const;

		void clearMaps();

		TPropertyMap mPropertyGlobalMap;
		TPropertyPropertyMap mPropertyUserMap;
		mutable CRITICAL_SECTION mCSection;
	};
} /*config*/ } /*sessionmanager*/ } /*ogon*/

namespace configNS = ogon::sessionmanager::config;

#endif /* OGON_SMGR_PROPERTYMANAGER_H_ */
