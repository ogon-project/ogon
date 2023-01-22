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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "PropertyManager.h"

#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>
//#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>
//#include <boost/property_tree/xml_parser.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <utils/StringHelpers.h>

namespace ogon { namespace sessionmanager { namespace config {

	static wLog *logger_PropertyManager = WLog_Get("ogon.sessionmanager.config.propertymanager");

	std::string gConnectionPrefix = "CURRENT.CONNECTION.";

	PropertyManager::PropertyManager() {
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_PropertyManager, WLOG_FATAL,
					"Failed to initialize property manager critical section");
			throw std::bad_alloc();
		}
	}

	PropertyManager::~PropertyManager() {
		clearMaps();
		DeleteCriticalSection(&mCSection);
		mPropertyGlobalMap.clear();
	}

	void PropertyManager::clearMaps() {
		TPropertyPropertyMap::iterator iterPPMap;

		for(iterPPMap = mPropertyUserMap.begin();
				iterPPMap != mPropertyUserMap.end(); iterPPMap++) {
			TPropertyMap *uPropMap = (TPropertyMap *)iterPPMap->second;
			delete uPropMap;
		}
		mPropertyUserMap.clear();
		mPropertyGlobalMap.clear();
	}

	bool PropertyManager::getPropertyInternal(UINT32 sessionID, const std::string &path,
		PROPERTY_STORE_HELPER &helper, const std::string &username) const {

		// first try to resolve the sessionID
		CSGuard guard(&mCSection);
		std::string localPath = path;
		boost::algorithm::to_upper(localPath);

		std::string currentUserName;
		if (sessionID == 0) {
			// for no session, use username if it's present
			if (username.size() == 0) {
				TPropertyMap::const_iterator it = mPropertyGlobalMap.find(localPath);
				if (it != mPropertyGlobalMap.end()) {
					helper = it->second;
					return true;
				}
				return false;
			}
			currentUserName = username;
		} else {
			// for a given sessionID we try to get the username from the sessionstore
			sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(sessionID);
			if (!session) {
				return false;
			}
			currentUserName = session->getUserName();
		}

		if (localPath.substr(0, gConnectionPrefix.size()) == gConnectionPrefix) {
			// requesting session values
			std::string actualPath = localPath.substr(gConnectionPrefix.size());
			sessionNS::SessionPtr currentSession = APP_CONTEXT.getSessionStore()->getSession(sessionID);
			if (nullptr == currentSession) {
				WLog_Print(logger_PropertyManager, WLOG_ERROR,
					"Cannot get Session for sessionID %" PRIu32 "", sessionID);
				return false;
			}

			sessionNS::ConnectionStore *connectionStore = APP_CONTEXT.getConnectionStore();
			UINT32 connectionID = connectionStore->getConnectionIdForSessionId(currentSession->getSessionID());
			if (connectionID == 0) {
				WLog_Print(logger_PropertyManager, WLOG_ERROR,
					"Cannot get ConnectionId for sessionID %" PRIu32 "", sessionID);
				return false;
			}

			sessionNS::ConnectionPtr currentConnection = connectionStore->getConnection(connectionID);
			if (!currentConnection) {
				WLog_Print(logger_PropertyManager, WLOG_ERROR,
					"Cannot get Connection for connectionId %" PRIu32 "", connectionID);
				return false;
			}
			return currentConnection->getProperty(actualPath, helper);
		}

		TPropertyPropertyMap::const_iterator propUserIt = mPropertyUserMap.find(currentUserName);
		if (propUserIt != mPropertyUserMap.end()) {
			TPropertyMap *uPropMap = propUserIt->second;
			if (uPropMap->find(localPath) != uPropMap->end()) {
				// we found the setting ...
				helper = (*uPropMap)[localPath];
				return true;
			}
		}

		TPropertyMap::const_iterator globalPropIt = mPropertyGlobalMap.find(localPath);
		if (globalPropIt != mPropertyGlobalMap.end()) {
			helper = globalPropIt->second;
			return true;
		}
		return false;
	}

	bool PropertyManager::getPropertyBool(UINT32 sessionID, const std::string &path,
		bool &value, const std::string &username) const {

		PROPERTY_STORE_HELPER internStore;
		if (!getPropertyInternal(sessionID, path, internStore, username) || (internStore.type != BoolType)) {
			return false;
		}

		value = internStore.boolValue;
		return true;
	}

	bool PropertyManager::getPropertyNumber(UINT32 sessionID, const std::string &path,
		long &value, const std::string &username) const {

		PROPERTY_STORE_HELPER internStore;
		if (!getPropertyInternal(sessionID, path, internStore, username) || (internStore.type != NumberType)) {
			return false;
		}

		value = internStore.numberValue;
		return true;
	}

	bool PropertyManager::getPropertyString(UINT32 sessionID, const std::string &path,
		std::string &value, const std::string &username) const {

		PROPERTY_STORE_HELPER internStore;
		if (!getPropertyInternal(sessionID, path, internStore, username) || (internStore.type != StringType)) {
			return false;
		}

		value = internStore.stringValue;
		return true;

	}

	bool PropertyManager::setPropertyInternal(PROPERTY_LEVEL level, UINT32 sessionID,
		const std::string &path, PROPERTY_STORE_HELPER helper, const std::string &username) {

		CSGuard guard(&mCSection);
		std::string localPath = path;
		boost::algorithm::to_upper(localPath);

		if (level == User) {
			std::string currentUserName;
			if (sessionID == 0) {
				// for no session, use username if it's present
				if (username.size() == 0) {
					return false;
				}
				currentUserName = username;
			} else {
				// for a given sessionID we try to get the username from the sessionstore
				sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(sessionID);
				if (nullptr == session) {
					return false;
				}
				currentUserName = session->getUserName();
			}

			// we have the username now
			if (mPropertyUserMap.find(currentUserName) != mPropertyUserMap.end()) {
				TPropertyMap *uPropMap = mPropertyUserMap[currentUserName];
				(*uPropMap)[localPath] = helper;
			} else {
				TPropertyMap *uPropMap = new TPropertyMap();
				(*uPropMap)[localPath] = helper;
				mPropertyUserMap[currentUserName] = uPropMap;
			}
			return true;
		} else if (level == Global) {
			mPropertyGlobalMap[localPath]= helper;
			return true;
		}
		return false;
	}

	bool PropertyManager::setPropertyBool(PROPERTY_LEVEL level, UINT32 sessionID,
		const std::string &path, bool value, const std::string &username) {

		PROPERTY_STORE_HELPER helper;
		helper.type = BoolType;
		helper.boolValue = value;

		if (username.size() == 0) {
			WLog_Print(logger_PropertyManager, WLOG_TRACE,
				"Adding property %s with value %s in global scope",
				path.c_str(), value ? "true" : "false");
		} else {
			WLog_Print(logger_PropertyManager, WLOG_TRACE,
				"Adding property %s with value %s for user %s",
				path.c_str(), value ? "true" : "false", username.c_str());
		}

		return setPropertyInternal(level, sessionID, path, helper, username);
	}

	bool PropertyManager::setPropertyNumber(PROPERTY_LEVEL level, UINT32 sessionID,
		const std::string &path, long value, const std::string &username) {

		PROPERTY_STORE_HELPER helper;
		helper.type = NumberType;
		helper.numberValue = value;

		if (username.size() == 0) {
			WLog_Print(logger_PropertyManager, WLOG_TRACE,
				"Adding property %s with value %ld in global scope",
				path.c_str(), value);
		} else {
			WLog_Print(logger_PropertyManager, WLOG_TRACE,
				"Adding property %s with value %ld for user %s",
				path.c_str(), value, username.c_str());
		}

		return setPropertyInternal(level, sessionID, path, helper, username);
	}

	bool PropertyManager::setPropertyString(PROPERTY_LEVEL level, UINT32 sessionID,
		const std::string &path, const std::string &value, const std::string &username) {

		PROPERTY_STORE_HELPER helper;
		helper.type = StringType;
		helper.stringValue = value;

		if (username.size() == 0) {
			WLog_Print(logger_PropertyManager, WLOG_TRACE,
				"Adding property %s with value %s in global scope",
				path.c_str(), value.c_str());
		} else {
			WLog_Print(logger_PropertyManager, WLOG_TRACE,
				"Adding property %s with value %s for user %s",
				path.c_str(), value.c_str(), username.c_str());
		}

		return setPropertyInternal(level, sessionID, path, helper, username);
	}

	bool PropertyManager::parsePropertyGlobal(const std::string &parentPath,
		const boost::property_tree::ptree &tree, PROPERTY_LEVEL level) {

		bool useParentPath = false;
		std::string username;

		if (parentPath.size() != 0) {
			// check if it is global
			if (stringStartsWith(parentPath, "user")) {
				username = parentPath;
				username.erase(0,5);
				level = User;
			} else if (!stringStartsWith(parentPath, "global")) {
				useParentPath = true;
				level = Global;
			}
		}

		BOOST_FOREACH(boost::property_tree::ptree::value_type const &v, tree) {
			boost::property_tree::ptree subtree = v.second;
			if (v.second.data().size() != 0) {
				std::string fullPath;
				if (useParentPath) {
					fullPath = parentPath + "." + v.first;
				} else {
					fullPath = v.first;
				}

				if (std::stringEndsWith(fullPath, "_number")) {
					std::string propertyName = fullPath.substr(0, fullPath.size() - strlen("_number"));
					std::replace(propertyName.begin(), propertyName.end(), '_', '.');
					try {
						long number = boost::lexical_cast<long>(v.second.data());
						setPropertyNumber(level, 0, propertyName ,number, username);
					} catch (boost::bad_lexical_cast &) {
						WLog_Print(logger_PropertyManager, WLOG_ERROR,
							"Could not cast %s to a number, property %s ignored!",
							v.second.data().c_str(), propertyName.c_str());
					}
				} else if (std::stringEndsWith(fullPath, "_string")) {
					std::string propertyName = fullPath.substr(0, fullPath.size() - strlen("_string"));
					std::replace(propertyName.begin(), propertyName.end(), '_', '.');
					setPropertyString(level, 0, propertyName, v.second.data(), username);
				} else if (std::stringEndsWith(fullPath, "_bool")) {
					std::string propertyName = fullPath.substr(0, fullPath.size() - strlen("_bool"));
					std::replace(propertyName.begin(), propertyName.end(), '_', '.');
					try {
						setPropertyBool(level, 0, propertyName,
							boost::lexical_cast<bool>(v.second.data()), username);
					} catch (boost::bad_lexical_cast &) {
						WLog_Print(logger_PropertyManager, WLOG_ERROR,
							"Could not cast %s to a bool, property %s ignored!",
							v.second.data().c_str(), propertyName.c_str());
					}
				}
			}
			if (parentPath.size() == 0) {
				parsePropertyGlobal(v.first, subtree, level);
			} else {
				parsePropertyGlobal(parentPath + "." + v.first, subtree, level);
			}
		}
		return true;
	}

	bool PropertyManager::loadProperties(const std::string &filename) {
		boost::property_tree::ptree pt;
		CSGuard guard(&mCSection);
		clearMaps();
		APP_CONTEXT.setupDefaultValues();

		try {
			//boost::property_tree::read_json(filename, pt);
			//boost::property_tree::read_xml(filename, pt);
			boost::property_tree::read_ini(filename, pt);
			parsePropertyGlobal("", pt, Global);
		} catch (boost::property_tree::file_parser_error &e) {
			WLog_Print(logger_PropertyManager, WLOG_ERROR,
					"Error while parsing config file: %s", e.what());
		}
		return true;
	}

	bool PropertyManager::checkConfigFile(const std::string &filename) {
		boost::property_tree::ptree pt;
		try {
			boost::property_tree::read_ini(filename, pt);
		} catch (boost::property_tree::file_parser_error &e) {
			WLog_Print(logger_PropertyManager, WLOG_ERROR,
					"Error while parsing: %s", e.what());
			return false;
		}
		return true;
	}

	bool PropertyManager::saveProperties(const std::string &filename) {
		using boost::property_tree::ptree;
		ptree pt;

		ptree &gnode = pt.add("global", "");

		CSGuard guard(&mCSection);

		TPropertyMap::iterator iter;

		for(iter = mPropertyGlobalMap.begin(); iter != mPropertyGlobalMap.end(); iter++) {
			std::string corrected = iter->first;
			std::replace(corrected.begin(), corrected.end(), '.', '_');

			if (iter->second.type == BoolType) {
				gnode.add(corrected + "_bool", iter->second.boolValue);
			} else if (iter->second.type == StringType) {
				gnode.add(corrected + "_string", iter->second.stringValue);
			} else if (iter->second.type == NumberType) {
				gnode.add(corrected + "_number", iter->second.numberValue);
			}
		}

		TPropertyPropertyMap::iterator iterPPMap;

		for(iterPPMap = mPropertyUserMap.begin(); iterPPMap != mPropertyUserMap.end(); iterPPMap++) {
			TPropertyMap *uPropMap = (TPropertyMap *)iterPPMap->second;
			ptree &unode = pt.add("user_" + iterPPMap->first, "");

			for(iter = uPropMap->begin(); iter != uPropMap->end(); iter++) {
				std::string corrected = iter->first;
				std::replace(corrected.begin(), corrected.end(), '.', '_');

				if (iter->second.type == BoolType) {
					unode.add(corrected + "_bool", iter->second.boolValue);
				} else if (iter->second.type == StringType) {
					unode.add(corrected + "_string", iter->second.stringValue);
				} else if (iter->second.type == NumberType) {
					unode.add(corrected + "_number", iter->second.numberValue);
				}
			}
		}

		//write_json(filename + ".json", pt);
		//boost::property_tree::xml_writer_settings<char> settings('\t', 1);
		//write_xml(filename + ".xml", pt,std::locale(), settings);
		write_ini(filename, pt);
		return true;
	}
} /*config*/ } /*sessionmanager*/ } /*ogon*/
