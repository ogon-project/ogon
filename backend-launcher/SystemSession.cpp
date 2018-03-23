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

#include "SystemSession.h"
#include <string.h>
#include <stdio.h>
#include <utmpx.h>
#include <winpr/environment.h>
#include "../common/global.h"

namespace ogon { namespace launcher {

static wLog *logger = WLog_Get("ogon.launcher");

	SystemSession::SystemSession() : mPamHandle(NULL), mSessionOpen(false), mUTMPwritten(false),
			mSessionPID(0), mSessionID(0)
	{
	}

	SystemSession::~SystemSession() {
		if (mSessionOpen) {
			stopSession();
		}
		if (mPamHandle) {
			uninitalizePAM();
		}
	}

	bool SystemSession::startSession() {

		if (mSessionOpen || !mPamHandle) {
			return false;
		}
#if 0
		pam_set_item(pam_handle, PAM_XDISPLAY, ":22");
		pam_set_item(pam_handle, PAM_TTY, ":22");
		pam_putenv(pam_handle, strdup("USER="USER));
		pam_putenv(pam_handle, strdup("DISPLAY=:22"));
#endif

		if (pam_putenv(mPamHandle, "XDG_SESSION_TYPE=unspecified") != PAM_SUCCESS ||
				pam_putenv(mPamHandle, "XDG_SESSION_CLASS=user") != PAM_SUCCESS ||
				pam_set_item(mPamHandle, PAM_RHOST, mRemoteHost.c_str()) != PAM_SUCCESS) {
			return false;
		}

		if (PAM_SUCCESS != pam_open_session(mPamHandle, 0)) {
			WLog_Print(logger, WLOG_ERROR, "unable to open a PAM session");
			return false;
		}

		mSessionOpen = true;

		if (!writeUTMPEntry(USER_PROCESS)) {
			return false;
		}
		mUTMPwritten = true;
		return true;
	}

	bool SystemSession::populateEnv(char **envBlockp) const {
		const char *toPopulate[] = {
			"XDG_RUNTIME_DIR",
			"XDG_SESSION_ID",

			/* these one should not be set in our case, but it doesn't hurt to
			 * populate them if pam_systemd has set them.
			 */
			"XDG_SEAT",
			"XDG_VTNR",
			NULL
		};

		for (int i = 0; toPopulate[i]; i++) {
			const char *val = pam_getenv(mPamHandle, toPopulate[i]);
			if (val) {
				if (!SetEnvironmentVariableEBA(envBlockp, toPopulate[i], val)) {
					return false;
				}
			}
		}

		return true;
	}

	bool SystemSession::stopSession() {
		if (!mPamHandle || !mSessionOpen) {
			return false;
		}
		if (mUTMPwritten) {
			writeUTMPEntry(DEAD_PROCESS);
			mUTMPwritten = false;
		}
		if (pam_close_session(mPamHandle, 0) != PAM_SUCCESS) {
			return false;
		}
		mSessionOpen = false;
		return true;
	}

	int SystemSession::pam_conv_cb(int msg_length, const struct pam_message **msg, struct pam_response **resp, void *app_data) {
		/*
		 * As authentication happens before, in the session-manager,
		 * nothing needs to be done here.
		 */
		OGON_UNUSED(msg_length);
		OGON_UNUSED(msg);
		OGON_UNUSED(resp);
		OGON_UNUSED(app_data);
		return PAM_SUCCESS;
	}

	bool SystemSession::init(const std::string &userName, const std::string &serviceName,
			const std::string &remoteHost, pid_t sessionPID, UINT32 sessionID) {
		pam_conv conv = {SystemSession::pam_conv_cb, NULL};

		if (mPamHandle) {
			return false;
		}
		mSessionPID = sessionPID;
		mUserName = userName;
		mRemoteHost = remoteHost;
		mSessionID = sessionID;

		if (!userName.length() || sessionPID == 0 || sessionID == 0) {
			return false;
		}

		return pam_start(serviceName.c_str(), userName.c_str(), &conv, &mPamHandle) == PAM_SUCCESS;
	}

	bool SystemSession::uninitalizePAM() {
		if (!mPamHandle) {
			return false;
		}
		return pam_end(mPamHandle, PAM_SUCCESS) == PAM_SUCCESS;
	}

	bool SystemSession::writeUTMPEntry(short type) {
		struct utmpx utx;
		struct timeval tv;

		memset(&utx, 0, sizeof(struct utmpx));

		utx.ut_type = type;
		utx.ut_pid = mSessionPID;
		strncpy(utx.ut_user, mUserName.c_str(), __UT_NAMESIZE - 1);
		strncpy(utx.ut_host, mRemoteHost.c_str(), __UT_HOSTSIZE - 1);
		snprintf(utx.ut_line, __UT_LINESIZE - 1, "rdp:%" PRIu32 "", mSessionID);

		gettimeofday (&tv, NULL);
		utx.ut_tv.tv_sec = tv.tv_sec;
		utx.ut_tv.tv_usec = tv.tv_usec;

		setutxent();
		if (!pututxline(&utx)) {
			return false;
		}
		endutxent();
		updwtmpx("/var/log/wtmp", &utx);

		return true;
	}

} /* launcher */ } /* ogon */
