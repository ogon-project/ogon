/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Module Helper
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * Martin Haimberger <martin.haimberger@thincast.com>
 * Norbert Federa <norbert.federa@thincast.com>
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

#include "module_helper.h"

#include <winpr/wlog.h>
#include <winpr/environment.h>
#include <winpr/wtypes.h>
#include <winpr/string.h>
#include <stdbool.h>

#ifndef WIN32
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#endif

#include "../../common/global.h"

static wLog *logger_ModuleHelper = WLog_Get(OGON_TAG("sessionmanager.module.helper"));

static bool combinePaths(char *buffer, unsigned buffersize, const char *basePath, const char *prop) {

	size_t basePathLength = strlen(basePath);
	size_t propLength = strlen(prop);
	size_t fullLength = basePathLength + propLength + 2;
	if (fullLength > buffersize) {
		return false;
	}
	memcpy(buffer,basePath,basePathLength);
	buffer[basePathLength] = '.';
	memcpy(buffer + basePathLength + 1, prop, propLength);
	buffer[fullLength-1] = 0;
	return true;
}

bool getPropertyBoolWrapper(const char *basePath, RDS_MODULE_CONFIG_CALLBACKS *config, UINT32 sessionID, const char *path, bool *value) {
	char tempBuffer[1024];
	if (!combinePaths(tempBuffer,1024,basePath,path)) {
		WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": getPropertyBoolWrapper: combinePaths failed", sessionID);
		return false;
	}
	return config->getPropertyBool(sessionID,tempBuffer,value);
}

bool getPropertyNumberWrapper(const char *basePath, RDS_MODULE_CONFIG_CALLBACKS *config, UINT32 sessionID, const char *path, long *value) {
	char tempBuffer[1024];
	if (!combinePaths(tempBuffer,1024,basePath,path)) {
		WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": getPropertyNumberWrapper: combinePaths failed", sessionID);
		return false;
	}
	return config->getPropertyNumber(sessionID,tempBuffer,value);
}

bool getPropertyStringWrapper(const char *basePath, RDS_MODULE_CONFIG_CALLBACKS *config, UINT32 sessionID, const char *path, char *value, unsigned int valueLength) {
	char tempBuffer[1024];
	if (!combinePaths(tempBuffer,1024,basePath,path)) {
		WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": getPropertyStringWrapper: combinePaths failed", sessionID);
		return false;
	}
	return config->getPropertyString(sessionID,tempBuffer,value,valueLength);

}

void initResolutions(RDS_MODULE_CONFIG_CALLBACKS *config, UINT32 sessionId, long *xres, long *yres, long *colordepth) {
	*xres = 0;
	*yres = 0;
	*colordepth = 0;
	long initialXRes = 0;
	long connectionXRes = 0;
	long sessionXRes = 0;
	long initialYRes = 0;
	long moduleYRes = 0;
	long sessionYRes = 0;

	config->getPropertyNumber(sessionId, "current.connection.xres", &connectionXRes);
	config->getPropertyNumber(sessionId, "current.connection.initialxres", &initialXRes);
	config->getPropertyNumber(sessionId, "session.maxxres", &sessionXRes );
	config->getPropertyNumber(sessionId, "current.connection.yres", &sessionYRes);
	config->getPropertyNumber(sessionId, "current.connection.initialyres", &initialYRes);
	config->getPropertyNumber(sessionId, "session.maxyres", &moduleYRes);

	WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "s %" PRIu32 ": initial %ldx%ld, current %ldx%ld, session limit %ldx%ld",
			   sessionId, initialXRes, initialYRes, connectionXRes, sessionYRes, sessionXRes , moduleYRes);

	if (initialXRes > sessionXRes) {
		*xres = sessionXRes ;
	} else {
		*xres = initialXRes;
	}
	if (initialYRes > moduleYRes) {
		*yres = moduleYRes;
	} else {
		*yres = initialYRes;
	}

	config->getPropertyNumber(sessionId, "current.connection.colordepth", colordepth);

	if ((*xres == 0) || (*yres == 0)) {
		WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": got no XRes or YRes from client, using config values", sessionId);

		if (!config->getPropertyNumber(sessionId, "session.xres", xres)) {
			*xres = 1024;
		}
		if (!config->getPropertyNumber(sessionId, "session.yres", yres)) {
			*yres = 768;
		}
	}

	if (*colordepth == 0) {
		if (!config->getPropertyNumber(sessionId, "session.colordepth", colordepth)) {
			*colordepth = 24;
		}
	}
}

bool TerminateChildProcessAfterTimeout(DWORD dwProcessId, DWORD dwMilliseconds, int *pExitCode, UINT32 sessionID)
{
#ifdef WIN32
	return false;
#else
	pid_t pid;
	pid_t rv;
	unsigned kills = 0;
	unsigned ms;
	int status;
	int exitCode = 255;

	if (pExitCode) {
		*pExitCode = exitCode;
	}

	/* WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "called with dwProcessId=%" PRIu32 " and dwMilliseconds=%" PRIu32 "", dwProcessId, dwMilliseconds); */

	if (!dwProcessId) {
		WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": called with zero process id", sessionID);
		return false;
	}

	if (dwProcessId == GetCurrentProcessId()) {
		WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": called with my own process id %" PRIu32 "", sessionID, dwProcessId);
		return false;
	}

	pid = (pid_t)dwProcessId;

	if (dwMilliseconds == INFINITE) {
		while (pid != (rv = waitpid(pid, &status, 0))) {
			if (rv == -1) {
				if (errno == EINTR) {
					continue;
				}
				if (errno == ECHILD) {
					/* no such child processes */
					WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": blocking waitpid(%lu) failed (errno=ECHILD)",
						sessionID, (unsigned long) pid);
					return true;
				}
			}
			/* some error: either -1 or an unexpected pid was returned */
			WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": blocking waitpid(%lu) failed with return value %ld (errno=%d)",
					   sessionID, (unsigned long) pid, (long) rv, errno);
			return false;
		}
		goto out;
	}

	while (pid != (rv = waitpid(pid, &status, WNOHANG))) {
		if (rv == -1 && errno == ECHILD) {
			/* no such child processes */
			WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": nonblocking waitpid(%lu) failed (errno=ECHILD)",
				sessionID, (unsigned long) pid);
			return true;
		}
		if (rv != 0) {
			WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": nonblocking waitpid(%lu) failed with return value %ld (errno=%d)",
				sessionID, (unsigned long) pid, (long) rv, errno);
			return false;
		}
		/* child is still running */
		if (!(ms = dwMilliseconds < 250 ? dwMilliseconds : 250)) {
			/* grace time expired */
			switch (kills++) {
				case 0:
					WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "s %" PRIu32 ": sending SIGTERM to process %lu", sessionID, (unsigned long) pid);
					kill(pid, SIGTERM);
					break;
				case 1:
					WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "s %" PRIu32 ": sending SIGKILL to process %lu", sessionID, (unsigned long) pid);
					kill(pid, SIGKILL);
					break;
				default:
					WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": process %lu still active after assassination attempt", sessionID, (unsigned long) pid);
					return false;
			}
			/* wait another 500ms after sending term/kill signal */
			dwMilliseconds = 500;
			continue;
		}
		Sleep(ms);
		dwMilliseconds -= ms;
	}

out:
	if (WIFEXITED(status)) {
		exitCode = WEXITSTATUS(status);
		WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "s %" PRIu32 ": process %lu exited normally with code %d", sessionID, (unsigned long) pid, exitCode);
	} else if (WIFSIGNALED(status)) {
		WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "s %" PRIu32 ": process %lu was terminated by signal %d", sessionID, (unsigned long) pid, WTERMSIG(status));
	} else if (WIFSTOPPED(status)) {
		WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "s %" PRIu32 ": process %lu was stopped by delivery of signal %d", sessionID, (unsigned long) pid, WSTOPSIG(status));
	}

	if (pExitCode) {
		*pExitCode = exitCode;
	}

	return true;

#endif /* WIN32 not defined */
}

bool TerminateChildProcess(DWORD dwProcessId, DWORD dwTimeout, int *pExitCode, UINT32 sessionID)
{
#ifdef WIN32
	return false;
#else
#define CYCLE_TIME 100
	pid_t pid;
	pid_t rv;
	unsigned kills = 0;
	unsigned ms;
	int status;
	int exitCode = 255;

	if (pExitCode) {
		*pExitCode = exitCode;
	}

	if (!dwProcessId) {
		WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": called with zero process id", sessionID);
		return false;
	}

	if (dwProcessId == GetCurrentProcessId()) {
		WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": called with my own process id %" PRIu32 "", sessionID, dwProcessId);
		return false;
	}

	pid = (pid_t)dwProcessId;

	/* Immediately send SIGTERM then wait until the timeout is reached */
	kill(pid, SIGTERM);

	/* Clamp the timeout to a multiple of CYCLE_TIME */
	ms = (dwTimeout / CYCLE_TIME) * CYCLE_TIME + CYCLE_TIME;
	while (pid != (rv = waitpid(pid, &status, WNOHANG))) {
		if (rv == -1 && errno == ECHILD) {
			/* no such child processes */
			WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": nonblocking waitpid(%lu) failed with return value %ld (errno=ECHILD)",
				sessionID, (unsigned long) pid, (long) rv);
			return true;
		}
		if (rv != 0) {
			WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": nonblocking waitpid(%lu) failed with return value %ld (errno=%d)",
				sessionID, (unsigned long) pid, (long) rv, errno);
			return false;
		}
		if (!ms) {
			/* SIGKILL already sent but no response after 10 * CYCLE_TIME */
			if (kills > 1) {
				WLog_Print(logger_ModuleHelper, WLOG_ERROR, "s %" PRIu32 ": failed to terminate process %lu via SIGKILL",
					sessionID, (unsigned long) pid);
				return false;
			}
			kill(pid, SIGKILL);
			kills++;
			ms = CYCLE_TIME * 10;
		}
		Sleep(CYCLE_TIME);
		ms -= CYCLE_TIME;
	}

	if (WIFEXITED(status)) {
		exitCode = WEXITSTATUS(status);
		WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "s %" PRIu32 ": process %lu exited normally with code %d", sessionID, (unsigned long) pid, exitCode);
	} else if (WIFSIGNALED(status)) {
		WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "s %" PRIu32 ": process %lu was terminated by signal %d", sessionID, (unsigned long) pid, WTERMSIG(status));
	} else if (WIFSTOPPED(status)) {
		WLog_Print(logger_ModuleHelper, WLOG_DEBUG, "s %" PRIu32 ": process %lu was stopped by delivery of signal %d", sessionID, (unsigned long) pid, WSTOPSIG(status));
	}

	if (pExitCode) {
		*pExitCode = exitCode;
	}

	return true;

#endif /* WIN32 not defined */
}
