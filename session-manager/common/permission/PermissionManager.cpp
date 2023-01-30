/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Permission Manager class
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

#include "PermissionManager.h"
#include <winpr/wlog.h>
#include <utils/CSGuard.h>
#include <dirent.h>
#include <utils/StringHelpers.h>
#include <appcontext/ApplicationContext.h>

#ifdef __linux__
#include <grp.h>
#include <pwd.h>
#endif


namespace ogon { namespace sessionmanager { namespace permission {

	static wLog *logger_PermissionManager = WLog_Get("ogon.sessionmanager.permission.permissionmanager");

	PermissionManager::PermissionManager() {
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_PermissionManager, WLOG_FATAL,
				"Failed to initialize permission manager critical section");
			throw std::bad_alloc();
		}
		mRandomBucket = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
		srand(time(nullptr));
		removeAuthTokens();
		mUnknownGroupsLogonAllowed = false;
	}

	PermissionManager::~PermissionManager() {
		DeleteCriticalSection(&mCSection);
	}


	std::string PermissionManager::genRandom(int length) {
		std::string randomString;
		for (int run = 0; run < length; run++ ) {
			randomString += mRandomBucket[rand() % mRandomBucket.size()];
		}
		return randomString;
	}


	std::string PermissionManager::registerSession(sessionNS::SessionPtr session) {
		std::string token = genRandom();

		CSGuard guard(&mCSection);
		while(mSessionMap.find(token) != mSessionMap.end()) {
			token = genRandom();
		}

		mSessionMap[token] = session;
		return token;
	}

	int PermissionManager::unregisterSession(sessionNS::SessionPtr session) {
		CSGuard guard(&mCSection);
		mSessionMap.erase(session->getToken());
		return 0;
	}

	void PermissionManager::removeAuthTokens() {
			struct dirent *next_file;
			DIR *theFolder;

			char filepath[256];

			theFolder = opendir("/tmp");

			while ( (next_file = readdir(theFolder)) ) {
				if (strncmp(next_file->d_name, "ogon.session.",
					strlen("ogon.session.")) == 0) {

					sprintf(filepath, "%s/%s", "/tmp", next_file->d_name);
					remove(filepath);
				}
			}
			closedir(theFolder);
	}

	sessionNS::SessionPtr PermissionManager::getSessionForToken(const std::string &token) {
		CSGuard guard(&mCSection);
		if (mSessionMap.find(token) != mSessionMap.end()) {
			return mSessionMap[token];
		}
		return sessionNS::SessionPtr();
	}

	sessionNS::SessionPtr PermissionManager::getSessionForTokenAndPermission(
		const std::string &token, DWORD permission)
	{

		CSGuard guard(&mCSection);
		if (mSessionMap.find(token) != mSessionMap.end()) {
			sessionNS::SessionPtr session = mSessionMap[token];
			if (session->checkPermission(permission)){
				return session;
			}
		}
		return sessionNS::SessionPtr();
	}


	std::string PermissionManager::registerLogon(const std::string &username,
		const std::string &domain, DWORD permission) {

		std::string token = genRandom();

		CSGuard guard(&mCSection);
		while(mLogonPermissionMap.find(token) != mLogonPermissionMap.end()) {
			token = genRandom();
		}
		LogonPermissionPtr perm(new LogonPermission(username, domain, permission));
		mLogonPermissionMap[token] = perm;
		return token;
	}

	int PermissionManager::unregisterLogon(const std::string &token) {
		CSGuard guard(&mCSection);
		mLogonPermissionMap.erase(token);
		return 0;
	}

	LogonPermissionPtr PermissionManager::getPermissionForLogon(const std::string &token) {
		CSGuard guard(&mCSection);
		if (mLogonPermissionMap.find(token) != mLogonPermissionMap.end()) {
			return mLogonPermissionMap[token];
		}
		return LogonPermissionPtr();
	}

#ifdef __linux__
	std::list<std::string> PermissionManager::getGroupList(const std::string &username) {
		int ngroups = 20;
		gid_t *groups;
		std::list<std::string> groupList;

		groups = (gid_t*)malloc(ngroups * sizeof (gid_t));
		if (groups == nullptr) {
			WLog_Print(logger_PermissionManager, WLOG_FATAL,
				"malloc for groups failed!");
			return groupList;
		}

		struct passwd *pw;
		pw = getpwnam(username.c_str());
		if (pw == nullptr) {
			WLog_Print(logger_PermissionManager, WLOG_FATAL,
				"getpwnam for groups failed!");
			free(groups);
			return groupList;
		}

		if (getgrouplist(username.c_str(), pw->pw_gid, groups, &ngroups) == -1) {
			WLog_Print(logger_PermissionManager, WLOG_FATAL,
				"getgrouplist() for groups failed!");
			free(groups);
			return groupList;
		}

		struct group *gr;
		for (int run = 0; run < ngroups; run++) {
			gr = getgrgid(groups[run]);
			std::string groupName(gr->gr_name);
			groupList.push_back(groupName);
		}

		free(groups);
		return groupList;
	}
#else
	std::list<std::string> PermissionManager::getGroupList(const std::string &username) {
		std::list<std::string> groupList;

		return groupList;
	}
#endif

	bool PermissionManager::isLogonAllowedForUser(const std::string &username) {

		std::list<std::string> groupList = getGroupList(username);

		std::list<std::string>::iterator iter;
		bool allowedLogin = false;

		for(iter=groupList.begin(); iter != groupList.end(); ++iter) {
			std::string groupName = *iter;
			TGroupsMap::iterator it = mGroupsMap.find(groupName);
			if(it != mGroupsMap.end()) {
				if (mGroupsMap[groupName] == false) {
					// deny login
					return false;
				} else {
					allowedLogin = true;
				}
			}
		}

		if (!allowedLogin) {
			return mUnknownGroupsLogonAllowed;
		}
		return true;
	}

	void PermissionManager::reloadAllowedUsers() {
		std::string value;
		mGroupsMap.clear();
		mUnknownGroupsLogonAllowed = false;

		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();
		if (propertyManager->getPropertyString(0, "permission.groups.whiteList", value)) {

			std::vector<std::string> userList = split<std::string>(value, ";");
			for (unsigned int index = 0; index < userList.size(); index++) {
				std::string userName = std::trim(userList[index]);
				if (userName.compare("*") == 0) {
					mUnknownGroupsLogonAllowed = true;
				} else {
					mGroupsMap[userName] = true;
				}
			}
		}

		if (propertyManager->getPropertyString(0, "permission.groups.blackList", value)) {

			std::vector<std::string> userList = split<std::string>(value, ";");
			for (unsigned int index = 0; index < userList.size(); index++) {
				std::string userName = std::trim(userList[index]);
				if (userName.compare("*") == 0) {
					mUnknownGroupsLogonAllowed = false;
				} else {
					mGroupsMap[userName] = false;
				}
			}
		}
	}


} /*permission*/ } /*sessionmanager*/ } /*ogon*/
