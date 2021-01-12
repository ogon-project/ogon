/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * X11 Backend Module
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifndef WIN32
#include <signal.h>
#include <errno.h>
#endif

#include <winpr/pipe.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/wlog.h>
#include <winpr/environment.h>
#include <winpr/sysinfo.h>
#include <ogon/backend.h>
#include <list>
#include <algorithm>
#include <grp.h>
#include <pwd.h>
#include <sys/poll.h>

#include "x11_module.h"
#include "../common/module_helper.h"
#include "../../common/global.h"
#include <ogon/api.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <X11/Xauth.h>
#include <sys/socket.h>
#include <linux/un.h>

static RDS_MODULE_CONFIG_CALLBACKS gConfig;
static RDS_MODULE_STATUS_CALLBACKS gStatus;

#define X11_BACKEND_NAME "ogon-backend-x"

#define X11_DISPLAY_OFFSET 10
#define BUF_SIZE 4096
#define X11_STARTUP_TIMEOUT 5000
#define X11SOCKET "/tmp/.X11-unix/X"

static wLog *gModuleLog;

static CRITICAL_SECTION gStartCS;
static std::list<unsigned int> gUsedDisplays;

struct rds_module_x11 {
	RDS_MODULE_COMMON commonModule;

	pid_t x11pid;
	STARTUPINFO WMStartupInfo;
	PROCESS_INFORMATION WMProcessInformation;
	STARTUPINFO VCStartupInfo;
	PROCESS_INFORMATION VCProcessInformation;
	char *wmStop;

	unsigned int displayNum;
};

typedef struct rds_module_x11 rdsModuleX11;

/* user, groups, environment, cwd, pipe , sessionId*/
BOOL start_x_backend(rdsModuleX11 *x11_module, passwd *pwd, char *lpCurrentDirectory, char *xauthorityfile) {
	BOOL ret = FALSE;
	char buf[BUF_SIZE];
	pid_t pid = 0;
	int numArgs = 0;
	LPSTR *pArgs = NULL;
	char **envp = NULL;
	LPTCH lpszEnvironmentBlock = NULL;
	sigset_t oldSigMask;
	sigset_t newSigMask;
	BOOL restoreSigMask = FALSE;
	int comm_fds[2];
	struct pollfd pfd;
	char lpCommandLine[BUF_SIZE];
	long tmp;
	unsigned int display_offset;
	long xres, yres, colordepth;
#ifndef __sun
	int maxfd;
#endif
	int fd;
	int sig;
	sigset_t set;
	struct sigaction act;
	unsigned int dpi;
	char fontPath[BUF_SIZE];
	bool nokpcursor = false;
	bool haveFontPath;
	DWORD ticks_end;
	DWORD ticks_current;
	RDS_MODULE_COMMON *commonModule;
	int status;
	int bytes_read;
	char *buf_ptr;

	commonModule = &x11_module->commonModule;
	if (!getPropertyNumberWrapper(commonModule->baseConfigPath, &gConfig,
								  commonModule->sessionId, "displayOffset", &tmp)) {
		display_offset = X11_DISPLAY_OFFSET;
	} else {
		display_offset = (unsigned int) tmp;
	}

	if (!getPropertyNumberWrapper(commonModule->baseConfigPath, &gConfig,
								  commonModule->sessionId, "dpi", &tmp)) {
		dpi = 96;
	} else {
		dpi = (unsigned int) tmp;
	}

	haveFontPath = getPropertyStringWrapper(commonModule->baseConfigPath,
											&gConfig,
											commonModule->sessionId, "fontPath", fontPath, BUF_SIZE);

	getPropertyBoolWrapper(commonModule->baseConfigPath, &gConfig, commonModule->sessionId,
						   "noKPCursor", &nokpcursor);

	initResolutions(&gConfig, commonModule->sessionId, &xres, &yres, &colordepth);


	if (pipe(comm_fds)) {
		WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": failed to create communication pipe",
				commonModule->sessionId);
		return FALSE;
	}

	if (haveFontPath) {
		sprintf_s(lpCommandLine, BUF_SIZE,
				  "%s -displayfd %d:%u -geometry %ldx%ld -depth %d -dpi %u -fp %s",
				  X11_BACKEND_NAME, comm_fds[1], display_offset, xres, yres, 24, dpi,
				  fontPath);
	} else {
		sprintf_s(lpCommandLine, BUF_SIZE, "%s -displayfd %d:%u -geometry %ldx%ld -depth %d -dpi %u",
				  X11_BACKEND_NAME, comm_fds[1], display_offset, xres, yres, 24, dpi);
	}

	if (xauthorityfile) {
		strncat(lpCommandLine, " -auth ", BUF_SIZE - strlen(lpCommandLine) - 1);
		strncat(lpCommandLine, xauthorityfile, BUF_SIZE - strlen(lpCommandLine) - 1);
	}

	if (nokpcursor) {
		strncat(lpCommandLine, " -nkc", BUF_SIZE - strlen(lpCommandLine) - 1);
	}

	pArgs = CommandLineToArgvA(lpCommandLine, &numArgs);
	if (!pArgs) {
		goto close_fds;
	}

	if (commonModule->envBlock) {
		envp = EnvironmentBlockToEnvpA(commonModule->envBlock);
	} else {
		lpszEnvironmentBlock = GetEnvironmentStrings();
		if (!lpszEnvironmentBlock) {
			goto finish;
		}
		envp = EnvironmentBlockToEnvpA(lpszEnvironmentBlock);
	}
	if (!envp) {
		goto finish;
	}

	sigfillset(&newSigMask);
	restoreSigMask = !pthread_sigmask(SIG_SETMASK, &newSigMask, &oldSigMask);

	/* fork and exec */
	pid = fork();

	if (pid < 0) {
		/* fork failure */
		goto finish;
	}

	if (pid == 0) {
		/* child process */

		/* set default signal handlers */
		memset(&act, 0, sizeof(act));
		act.sa_handler = SIG_DFL;
		act.sa_flags = 0;
		sigemptyset(&act.sa_mask);
		for (sig = 1; sig < NSIG; sig++) {
			sigaction(sig, &act, NULL);
		}
		/* unblock all signals */
		sigfillset(&set);
		pthread_sigmask(SIG_UNBLOCK, &set, NULL);

#ifdef __sun	/* block all signals so that the child can safely reset the caller's handlers */
		closefrom(3);
#else /* __sun */
#ifdef F_MAXFD /* available on some BSD derivates */
		maxfd = fcntl(0, F_MAXFD);
#else
		maxfd = sysconf(_SC_OPEN_MAX);
#endif /* F_MAXFD */
		for (fd = 3; fd < maxfd; fd++) {
			if (fd != comm_fds[1]) {
				close(fd);
			}
		}
#endif /* __sun */


		if (setgid((gid_t) pwd->pw_gid)) {
			WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": setgid to %d failed (%s)",
					   x11_module->commonModule.sessionId, pwd->pw_gid, strerror(errno));
			_exit(1);
		}

		if (pwd->pw_name) {
			if (initgroups(pwd->pw_name, pwd->pw_gid)) {
				WLog_Print(gModuleLog, WLOG_INFO, "s %" PRIu32 ": initgroups for %s failed (%s)",
						   x11_module->commonModule.sessionId, pwd->pw_name, strerror(errno));
			}
		}

		if (setuid(pwd->pw_uid)) {
			WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": setuid to %d failed (%s)",
					   x11_module->commonModule.sessionId, pwd->pw_uid, strerror(errno));
			_exit(1);
		}

		if (lpCurrentDirectory && strlen(lpCurrentDirectory) > 0) {
			chdir(lpCurrentDirectory);
		}

		if (execvpe(pArgs[0], pArgs, envp) < 0) {
			/* execve failed - end the process */
			_exit(1);
		}
	}

	/* parent code - get the display id from xorg */
	pfd.fd = comm_fds[0];
	pfd.events = POLLIN;
	WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": x backend started (pid %lu, cmd %s)",
			   x11_module->commonModule.sessionId, (unsigned long) pid, lpCommandLine);

	ZeroMemory(buf, BUF_SIZE);
	ticks_current = GetTickCount();
	ticks_end = ticks_current + X11_STARTUP_TIMEOUT;

	buf_ptr = buf;
	bytes_read = 0;
	while (TRUE) {
		if ((status = poll(&pfd, 1, ticks_end - ticks_current)) < 1) {
			WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": could not get display id (poll error: %s)",
					commonModule->sessionId, strerror(errno));
			TerminateChildProcess(pid, 1, NULL, commonModule->sessionId);
			goto finish;
		}
		if ((status = read(comm_fds[0], buf_ptr, BUF_SIZE - bytes_read)) <= 0) {
			WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": problem while reading the display id",
					commonModule->sessionId);
			TerminateChildProcess(pid, 1, NULL, commonModule->sessionId);
			goto finish;
		}
		bytes_read += status;
		buf_ptr += status;
		if (*(buf_ptr - 1) == '\n') {
			if (sscanf(buf, "%u", &x11_module->displayNum) != 1) {
				WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": problem parsing the received displayid data",
						commonModule->sessionId);
				TerminateChildProcess(pid, 1, NULL, commonModule->sessionId);
				goto finish;
			} else {
				break;
			}
		}
		ticks_current =  GetTickCount();
		if (ticks_current >= ticks_end) {
			WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": reading display id timed out", commonModule->sessionId);
			TerminateChildProcess(pid, 1 ,NULL, commonModule->sessionId);
			goto finish;
		}
	}

	x11_module->x11pid = pid;
	ret = TRUE;

finish:
	/* restore caller's original signal mask */
	if (restoreSigMask) {
		pthread_sigmask(SIG_SETMASK, &oldSigMask, NULL);
	}

	if (pArgs) {
		HeapFree(GetProcessHeap(), 0, pArgs);
	}

	if (lpszEnvironmentBlock) {
		FreeEnvironmentStrings(lpszEnvironmentBlock);
	}

	if (envp) {
		int i = 0;

		while (envp[i]) {
			free(envp[i]);
			i++;
		}

		free(envp);
	}

close_fds:
	close(comm_fds[0]);
	close(comm_fds[1]);

	return ret;
}

static void x11_rds_module_reset_process_informations(STARTUPINFO *si,
        PROCESS_INFORMATION *pi) {
	ZeroMemory(si, sizeof(STARTUPINFO));
	si->cb = sizeof(STARTUPINFO);
	ZeroMemory(pi, sizeof(PROCESS_INFORMATION));
}

static DWORD clean_up_process(PROCESS_INFORMATION *pi) {
	DWORD ret = 0;
	if (pi->hProcess) {
		GetExitCodeProcess(pi->hProcess, &ret);
		CloseHandle(pi->hProcess);
		pi->hProcess = NULL;
		pi->dwProcessId = 0;
	}
	if (pi->hThread) {
		CloseHandle(pi->hThread);
		pi->hThread = NULL;
		pi->dwThreadId = 0;
	}

	return ret;
}

static RDS_MODULE_COMMON *x11_rds_module_new(void) {
	rdsModuleX11 *module = (rdsModuleX11 *) calloc(1, sizeof(rdsModuleX11));
	if (!module) {
		fprintf(stderr, "%s: error allocating x11 module memory\n", __FUNCTION__);
		return NULL;
	}
	return (RDS_MODULE_COMMON *) module;
}

static void x11_rds_module_free(RDS_MODULE_COMMON *module) {
	rdsModuleX11 *x11 = (rdsModuleX11 *)module;
	free(x11->wmStop);
	free(module);
}

static int x11_rds_stop_process(pid_t pid, unsigned int timeout_sec,
								UINT32 sessionID) {
	int exit_code = 0;

	if (gStatus.removeMonitoringProcess(pid)) {
		TerminateChildProcess(pid, timeout_sec * 1000, &exit_code, sessionID);
	}

	return exit_code;
}

static int x11_rds_stop_process(PROCESS_INFORMATION *pi,
                                unsigned int timeout_sec, UINT32 sessionID) {
	int ret = 0;

#ifdef WIN32
	if (!pi->hProcess) {
		return 0;
	}
	TerminateProcess(pi->hProcess, 0);

	// Wait until child process exits.
	WaitForSingleObject(pi->hProcess, 5);
#else
	if (gStatus.removeMonitoringProcess(pi->dwProcessId)) {
		TerminateChildProcess(pi->dwProcessId, timeout_sec * 1000, NULL, sessionID);
	}
#endif
	clean_up_process(pi);
	return ret;
}

static BOOL x11_rds_module_init_xauth(const char *xauthFileName, uid_t uid, gid_t gid) {
	FILE *fp = NULL;
	int fd;
	Xauth *pxau = NULL;
	Xauth xauth;
	char localhost[256];
	BOOL found = FALSE;
	BOOL result = FALSE;
	char cookie[16];

	memset(cookie, 0, sizeof(cookie));
	memset(localhost, 0, sizeof(localhost));
	memset(&xauth, 0, sizeof(xauth));

	if (gethostname(localhost, sizeof(localhost) - 1) < 0) {
                strncpy(localhost, "localhost", sizeof(localhost) - 1);
        }

	xauth.name = const_cast<char *>("MIT-MAGIC-COOKIE-1");
	xauth.name_length = strlen(xauth.name);
	xauth.family = FamilyLocal;
	xauth.address = localhost;
	xauth.address_length = strlen(localhost);
	xauth.data = cookie;
	xauth.data_length = sizeof(cookie);

	if ((fp = fopen(xauthFileName, "r")) != NULL) {
		while (!found && (pxau = XauReadAuth(fp))) {
			if (	pxau->family == xauth.family &&
				pxau->name_length == xauth.name_length &&
				!memcmp(pxau->name, xauth.name, xauth.name_length) &&
				pxau->address_length == xauth.address_length &&
				!memcmp(pxau->address, xauth.address, xauth.address_length))
			{
				found = TRUE;
			}
			XauDisposeAuth(pxau);
		}
		fclose(fp);

		if (found) {
			return TRUE;
		}
	}

	if ((fd = open("/dev/urandom", O_RDONLY)) < 0) {
		WLog_Print(gModuleLog, WLOG_ERROR, "error opening urandom device");
		return FALSE;
	}

	if (read(fd, cookie, sizeof(cookie)) != sizeof(cookie)) {
		WLog_Print(gModuleLog, WLOG_ERROR, "error reading data from urandom device");
		close(fd);
		return FALSE;
	}
	close(fd);

	if ((fd = open(xauthFileName, O_RDWR | O_CREAT, 0600)) < 0) {
		WLog_Print(gModuleLog, WLOG_ERROR, "error opening [%s]", xauthFileName);
		memset(cookie, 0, sizeof(cookie));
		return FALSE;
	}

	fchown(fd, uid, gid);

	if ((fp = fdopen(fd, "w+")) == NULL) {
		WLog_Print(gModuleLog, WLOG_ERROR, "error opening xauth file descriptor");
		memset(cookie, 0, sizeof(cookie));
		close(fd);
		return FALSE;
	}

	if (XauWriteAuth(fp, &xauth) != 1) {
		WLog_Print(gModuleLog, WLOG_ERROR, "error writing xauth entry to %s", xauthFileName);
	} else {
		result = TRUE;
	}

	memset(cookie, 0, sizeof(cookie));
	fclose(fp);
	return result;
}

static char *x11_rds_module_start(RDS_MODULE_COMMON *module) {
	BOOL status = TRUE;
	DWORD SessionId;
	rdsModuleX11 *x11;
	char buf[BUF_SIZE];
	char xauthFileName[BUF_SIZE];
	char *pipeName = NULL;
	char *cwd = NULL;
	unsigned long tmpLen = 0;
	passwd pw;
	passwd *result_pwd = NULL;

	x11 = (rdsModuleX11 *) module;
	x11_rds_module_reset_process_informations(&(x11->VCStartupInfo),
	        &(x11->VCProcessInformation));
	x11_rds_module_reset_process_informations(&(x11->WMStartupInfo),
	        &(x11->WMProcessInformation));

	SessionId = x11->commonModule.sessionId;


	if (getpwnam_r(module->userName, &pw, buf, BUF_SIZE, &result_pwd) != 0 || result_pwd == NULL) {
		WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": getpwnam failed", SessionId);
		return NULL;
	}

	if (getPropertyStringWrapper(module->baseConfigPath, &gConfig,
	                             module->sessionId, "xauthoritypath", buf, sizeof(buf))) {
		WLog_Print(gModuleLog, WLOG_DEBUG, "config: xAuthorityPath = %s", buf);
		sprintf_s(xauthFileName, sizeof(xauthFileName), "%s/.Xauthority.ogon.%" PRIu32 "", buf, SessionId);
	} else {
		tmpLen = GetEnvironmentVariableEBA(module->envBlock, "HOME", NULL, 0);
		if (tmpLen) {
			if (!(cwd = (char *) malloc(tmpLen))) {
				WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": couldn't allocate cwd", SessionId);
				return NULL;
			}
			GetEnvironmentVariableEBA(module->envBlock, "HOME", cwd, tmpLen);
		} else {
			cwd = strdup(result_pwd->pw_dir);
		}
		sprintf_s(xauthFileName, sizeof(xauthFileName), "%s/.Xauthority.ogon", cwd);
	}


	if (!SetEnvironmentVariableEBA(&module->envBlock, "XAUTHORITY", xauthFileName)) {
		goto out_fail;
	}

	if (!x11_rds_module_init_xauth(xauthFileName, pw.pw_uid, pw.pw_gid)) {
		goto out_fail;
	}

	if (!start_x_backend(x11, result_pwd, cwd, xauthFileName)) {
		goto out_fail;
	}

	pipeName = (char *) malloc(256);
	if (!pipeName) {
		WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": couldn't allocate pipeName", SessionId);
		goto out_fail;
	}
	ogon_named_pipe_get_endpoint_name(x11->displayNum, "X11", pipeName, 256);

	sprintf_s(buf, BUF_SIZE, ":%u", x11->displayNum);
	if (!SetEnvironmentVariableEBA(&module->envBlock, "DISPLAY", buf)) {
		goto out_fail;
	}

	if (!WaitNamedPipeA(pipeName, 5 * 1000)) {
		WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": WaitNamedPipe failure: %s", SessionId,
		           pipeName);
		x11_rds_stop_process(x11->x11pid, 5, SessionId);
		goto out_fail;
	}

	gStatus.addMonitoringProcess(x11->x11pid, SessionId, TRUE, module);

	WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": x backend started on display %u", SessionId, x11->displayNum);

	if (getPropertyStringWrapper(module->baseConfigPath, &gConfig,
	                             module->sessionId, "stopwm", buf, sizeof(buf))) {
		if (!(x11->wmStop = (char *) malloc(strlen(buf) + 1))) {
			WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": couldn't allocate wmStop", SessionId);
			goto out_fail;
		}
		strncpy(x11->wmStop, buf, strlen(buf) + 1);
	}

	if (!getPropertyStringWrapper(module->baseConfigPath, &gConfig,
	                              module->sessionId, "startwm", buf, sizeof(buf))) {
		strncpy(buf, "ogonXsession", BUF_SIZE);
	}

	status = CreateProcessAsUserA(module->userToken, NULL, buf, NULL, NULL, FALSE,
				0, module->envBlock, cwd, &(x11->WMStartupInfo), &(x11->WMProcessInformation));

	if (!status) {
		WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": problem starting %s (status %" PRId32 ")",
		           SessionId, buf, status);
		x11_rds_stop_process(x11->x11pid, 5, SessionId);
		if (x11->VCProcessInformation.dwProcessId) {
			x11_rds_stop_process(&(x11->VCProcessInformation), 5, SessionId);
		}
		goto out_fail;
	}

	gStatus.addMonitoringProcess(x11->WMProcessInformation.dwProcessId,
	                             x11->commonModule.sessionId, TRUE, module);

	free(cwd);
	WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": WM process (%s) started: %" PRId32 " (pid %" PRIu32 ")",
	           SessionId, buf, status, x11->WMProcessInformation.dwProcessId);
	return pipeName;

out_fail:
	free(cwd);
	free(pipeName);
	return NULL;
}

static void x11_execute_stop_script(rdsModuleX11 *x11) {
	PROCESS_INFORMATION StopProcessInformation;
	STARTUPINFO StopInfo;
	BOOL status;
	int tmpLen;
	char *cwd = NULL;
	RDS_MODULE_COMMON *commonModule = &x11->commonModule;
	UINT32 sessionID = commonModule->sessionId;

	if (!x11->wmStop) {
		return;
	}

	x11_rds_module_reset_process_informations(&StopInfo, &StopProcessInformation);

	tmpLen = GetEnvironmentVariableEBA(commonModule->envBlock, "HOME", NULL, 0);
	if (tmpLen) {
		if (!(cwd = (char *) malloc(tmpLen))) {
			WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": problem allocating cwd buffer",
			           sessionID);
			return;
		}
		GetEnvironmentVariableEBA(commonModule->envBlock, "HOME", cwd, tmpLen);
	}

	status = CreateProcessAsUserA(commonModule->userToken,
	                              NULL, x11->wmStop,
	                              NULL, NULL, FALSE, 0, commonModule->envBlock, cwd,
	                              &StopInfo, &StopProcessInformation);

	free(cwd);

	if (!status) {
		WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": problem starting [%s]", sessionID,
		           x11->wmStop);
		return;
	}

	WLog_Print(gModuleLog, WLOG_DEBUG,
	           "s %" PRIu32 ": stopwm script [%s] started with pid %" PRIu32 ")",
	           sessionID, x11->wmStop, StopProcessInformation.dwProcessId);

	TerminateChildProcessAfterTimeout(StopProcessInformation.dwProcessId, 5000, NULL, sessionID);
}

static int x11_rds_module_stop(RDS_MODULE_COMMON *module) {
	rdsModuleX11 *x11 = (rdsModuleX11 *)module;
	int ret = 0;
	UINT32 sessionID = x11->commonModule.sessionId;
	char pipeName[256];
	char *pipePath;
	struct sockaddr_un address;
	int socket_fd;

	WLog_Print(gModuleLog, WLOG_TRACE, "s %" PRIu32 ": Stop called", sessionID);

	x11_execute_stop_script(x11);

	x11_rds_stop_process(&(x11->WMProcessInformation), 10, sessionID);
	WLog_Print(gModuleLog, WLOG_TRACE, "s %" PRIu32 ": WMProcess stopped", sessionID);
	x11_rds_stop_process(x11->x11pid, 2, sessionID);
	WLog_Print(gModuleLog, WLOG_TRACE, "s %" PRIu32 ": X11Process stopped", sessionID);
	if (x11->VCProcessInformation.dwProcessId) {
		ret = x11_rds_stop_process(&(x11->VCProcessInformation), 5, sessionID);
		WLog_Print(gModuleLog, WLOG_TRACE, "s %" PRIu32 ": VCProcess stopped", sessionID);
	}

	/* handle the case where the x server crashed and left sockets around */
	ogon_named_pipe_get_endpoint_name(x11->displayNum, "X11", pipeName, 256);
	pipePath = GetNamedPipeUnixDomainSocketFilePathA(pipeName);
	if (pipePath) {
		DeleteFile(pipePath);
		WLog_Print(gModuleLog, WLOG_TRACE, "s %" PRIu32 ": deleted service pipe %s", sessionID, pipePath);
		free(pipePath);
	}
	socket_fd = socket(PF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (socket_fd < 0) {
		return ret;
	}
	memset(&address, 0, sizeof(struct sockaddr_un));
	address.sun_family = AF_UNIX;
	snprintf(address.sun_path, UNIX_PATH_MAX,  X11SOCKET "%" PRIu32,x11->displayNum);
	while (connect(socket_fd, (struct sockaddr *) &address, sizeof(struct sockaddr_un)) == -1 && errno != EISCONN) {
		if (errno == EINTR) {
			continue;
		} else {
			if (errno == ECONNREFUSED) {
				DeleteFile(address.sun_path);
				WLog_Print(gModuleLog, WLOG_TRACE, "s %" PRIu32 ": Deleted X11 pipe %s", sessionID, address.sun_path);
			}
			break;
		}

	}
	close(socket_fd);

	return ret;
}

static char *x11_get_custom_info(RDS_MODULE_COMMON *module) {
	rdsModuleX11 *x11 = (rdsModuleX11 *)module;
	char *customInfo = (char *) malloc(11);
	if (customInfo) {
		snprintf(customInfo, 11, "%u", x11->displayNum);
	}
	return customInfo;
}

int x11_module_init() {
	WLog_Init();
	gModuleLog = WLog_Get("com.ogon.module.x11");

	if (!InitializeCriticalSectionAndSpinCount(&gStartCS, 0x00000400)) {
		WLog_Print(gModuleLog, WLOG_FATAL,
		           "Failed to initialize x11 module start critical section");
		return -1;
	}
	return 0;
}

int x11_module_destroy() {
	DeleteCriticalSection(&gStartCS);
	return 0;
}

static int x11_rds_module_connect(RDS_MODULE_COMMON *module) {
	rdsModuleX11 *x11 = (rdsModuleX11 *) module;

	char buf[BUF_SIZE];
	BOOL status = TRUE;
	UINT32 sessionID = x11->commonModule.sessionId;
	int tmpLen;
	char *cwd = NULL;

	if (x11->VCProcessInformation.dwProcessId) {
		clean_up_process(&x11->VCProcessInformation);
	}

	// starting the virtual channel script
	if (getPropertyStringWrapper(x11->commonModule.baseConfigPath, &gConfig,
	                             x11->commonModule.sessionId, "startvc", buf, BUF_SIZE)) {
		tmpLen = GetEnvironmentVariableEBA(x11->commonModule.envBlock, "HOME", NULL, 0);
		if (tmpLen) {
			if (!(cwd = (char *) malloc(tmpLen))) {
				WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": error allocating cwd", sessionID);
				return -1;
			}
			GetEnvironmentVariableEBA(x11->commonModule.envBlock, "HOME", cwd, tmpLen);
		}

		status = CreateProcessAsUserA(x11->commonModule.userToken,
		                              NULL, buf,
		                              NULL, NULL, FALSE, 0, x11->commonModule.envBlock, cwd,
		                              &(x11->VCStartupInfo), &(x11->VCProcessInformation));

		free(cwd);

		if (!status) {
			WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": problem starting %s (status %" PRId32 ")",
			           sessionID, buf, status);
			return -1;
		}

		gStatus.addMonitoringProcess(x11->VCProcessInformation.dwProcessId, sessionID,
		                             FALSE, module);

		WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": startvc process (%s) started: %" PRId32 " (pid %" PRIu32 ")",
			sessionID, buf, status, x11->VCProcessInformation.dwProcessId);
	}

	return 0;
}

static int x11_rds_module_disconnect(RDS_MODULE_COMMON *module) {
	OGON_UNUSED(module);
	return 0;
}


OGON_API int RdsModuleEntry(RDS_MODULE_ENTRY_POINTS *pEntryPoints) {
	pEntryPoints->Version = 1;

	pEntryPoints->Init = x11_module_init;
	pEntryPoints->New = x11_rds_module_new;
	pEntryPoints->Free = x11_rds_module_free;

	pEntryPoints->Start = x11_rds_module_start;
	pEntryPoints->Stop = x11_rds_module_stop;
	pEntryPoints->getCustomInfo = x11_get_custom_info;
	pEntryPoints->Destroy = x11_module_destroy;
	pEntryPoints->Connect = x11_rds_module_connect;
	pEntryPoints->Disconnect = x11_rds_module_disconnect;

	pEntryPoints->Name = "X11";

	gStatus = pEntryPoints->status;
	gConfig = pEntryPoints->config;

	return 0;
}
