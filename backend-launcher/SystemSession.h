/**
 * ogon - Free Remote Desktop Services
 * Backend Process Launcher
 * Abstraction of system session handling
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
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

#ifndef OGON_BACKENDLAUNCHER_SYSTEMSESSION_H_
#define OGON_BACKENDLAUNCHER_SYSTEMSESSION_H_

#include <security/pam_appl.h>
#include <string>
#include <winpr/wtypes.h>

namespace ogon { namespace launcher  {

class SystemSession {
	public:
		SystemSession();
		~SystemSession();
		bool init(const std::string &userName, const std::string &serviceName,
				const std::string &remoteHost, pid_t sessionPID, UINT32 sessionID);
		bool startSession();
		bool populateEnv(char **envBlockp) const;
		bool stopSession();
		static int pam_conv_cb(int msg_length, const struct pam_message **msg, struct pam_response **resp, void *app_data);
	private:
		bool uninitalizePAM();
		bool writeUTMPEntry(short type);
		bool mSessionOpen;
		bool mUTMPwritten;
		pam_handle_t *mPamHandle;
		std::string mUserName;
		std::string mRemoteHost;
		pid_t mSessionPID;
		UINT32 mSessionID;
};

} /* launcher */ } /* ogon */
#endif			   /* OGON_BACKENDLAUNCHER_SYSTEMSESSION_H_ */
