/**
 * ogon - Free Remote Desktop Services
 * Backend Process Launcher
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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


/**
 * The process launcher handles out of session-manager
 * process launching for sessions.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ModuleCommunication.h"
#include "../common/global.h"
#include <winpr/synch.h>
#include <signal.h>
#include <list>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <winpr/file.h>
#include <winpr/wlog.h>
#include <algorithm>
#include <sys/prctl.h>
#include <errno.h>

#ifdef WITH_LIBSYSTEMD
#include <systemd/sd-journal.h>
#endif


using namespace ogon::launcher;

enum return_values {
	EXIT_OK,
	EXIT_FAIL_NO_SESSION_ID,
	EXIT_FAIL_SETSID,
	EXIT_FAIL_CREATEEVENT,
	EXIT_FAIL_SIGACTION,
	EXIT_FAIL_SIGMASK,
	EXIT_TIMEOUT,
	EXIT_FAIL_PROCESS,
};

static struct {
	BOOL set;
	siginfo_t info;
} gSignals[NSIG];

typedef struct {
	DWORD processId;
	bool terminateSessionOnExit;
} RDS_PROCESS_INFO;

static HANDLE gSignalEvent = NULL;
static HANDLE gSignalStop = NULL;
static std::list<RDS_PROCESS_INFO> gProcessInfos;
static int run = true;

CRITICAL_SECTION gProcessInfoLock;

static void add_monitoring_process(DWORD process_id, UINT32 session_id, bool terminate_session, RDS_MODULE_COMMON *context);
static bool remove_monitoring_process(DWORD process_id);
static bool get_property_bool (UINT32 sessionID, const char* path, bool* value);
static bool get_property_number(UINT32 sessionID, const char* path, long* value);
static bool get_property_string(UINT32 sessionID, const char* path, char* value, unsigned int valueLength);
static void signal_stop();

ModuleCommunication gModule(get_property_bool, get_property_number, get_property_string, add_monitoring_process, remove_monitoring_process, signal_stop);

static wLog *logger_ModuleCommunication = WLog_Get("ogon.launcher");

static void signal_handler(int signal, siginfo_t *siginfo, void *context) {
	/**
	 * Warning: This is the native signal handler
	 * Only use async-signal-safe functions here!
	 */
	OGON_UNUSED(context);

	if (signal >= NSIG)
		return;

	gSignals[signal].set = TRUE;
	gSignals[signal].info = *siginfo;

	SetEvent(gSignalEvent);
}

static BOOL forwardSignal(int signal) {
	kill(0, signal);
	return TRUE;
}

static void add_monitoring_process(DWORD process_id, UINT32 session_id, bool terminate_session, RDS_MODULE_COMMON *context) {
	OGON_UNUSED(context);
	OGON_UNUSED(session_id);
	RDS_PROCESS_INFO info;

	/* WLog_Print(logger_ModuleCommunication, WLOG_TRACE , "adding monitoring process %" PRIu32 " terminate=%d", process_id, terminate_session); */

	info.processId = process_id;
	info.terminateSessionOnExit = terminate_session;

	EnterCriticalSection(&gProcessInfoLock);

	gProcessInfos.push_back(info);

	LeaveCriticalSection(&gProcessInfoLock);
}

static bool remove_monitoring_process(DWORD process_id) {
	std::list<RDS_PROCESS_INFO>::iterator it;
	bool removed = false;

	/* WLog_Print(logger_ModuleCommunication, WLOG_TRACE , "removing monitoring process %" PRIu32 "", process_id); */

	EnterCriticalSection(&gProcessInfoLock);

	for (it = gProcessInfos.begin(); it != gProcessInfos.end(); it++) {
		if ((*it).processId == process_id) {
			gProcessInfos.erase(it);
			removed = true;
			break;
		}
	}

	LeaveCriticalSection(&gProcessInfoLock);

	return removed;
}

static BOOL handle_signal(int signum) {
	/* SIGCHLD: parent dead or control pipe closed */
	if (signum == SIGPIPE) {
		gModule.stopModule();
		run = false;
		return TRUE;
	}

	/* Forward any signal except SIGCHLD to the process group */
	if (signum != SIGCHLD) {
		return forwardSignal(signum);
	}

	EnterCriticalSection(&gProcessInfoLock);

	bool stop = false;
	std::list<RDS_PROCESS_INFO>::iterator it = gProcessInfos.begin();

	while (it != gProcessInfos.end()) {
		int status;
		bool erase = false;
		RDS_PROCESS_INFO info = *it;
		pid_t w = waitpid((pid_t) info.processId, &status, WNOHANG);

		if (w > 0) {
			WLog_Print(logger_ModuleCommunication, WLOG_TRACE, "Registered process %" PRIu32 " exited (status: %d)", info.processId, status);
			erase = true;
			if (info.terminateSessionOnExit) {
				stop = true;
			}
		} else if (w == -1 && errno == ECHILD) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "waitpid(%" PRIu32 ") failed (status: %d, errno=ECHILD) ", info.processId, status);
			erase = true;
		}

		if (erase) {
			gProcessInfos.erase(it++);
		} else {
			++it;
		}
	}

	LeaveCriticalSection(&gProcessInfoLock);

	if (stop) {
		gModule.stopModule();
		run = false;
	}

	return TRUE;
}

/* Module interface */
static bool get_property_bool (UINT32 sessionID, const char* path, bool* value) {
	return gModule.getPropertyBool(sessionID, path, value);
}

static bool get_property_number(UINT32 sessionID, const char* path, long* value) {
	return gModule.getPropertyNumber(sessionID, path, value);
}

static bool get_property_string(UINT32 sessionID, const char* path, char* value, unsigned int valueLength) {
	return gModule.getPropertyString(sessionID, path, value, valueLength);
}

static void signal_stop() {
	SetEvent(gSignalStop);
}

static void SplitFilename(
		std::string filename, std::string &folder, std::string &file) {
	size_t found;

	filename.erase(
			remove( filename.begin(), filename.end(), '\'' ),
			filename.end()
	);

	found = filename.find_last_of("/\\");
	if (found == std::string::npos)
	{
		file = filename;
		folder = "";
	}
	else {
		folder = filename.substr(0,found);
		file = filename.substr(found+1);
	}
}

int main(int argc, char *argv[]) {
	int read_fd;
	int write_fd;
	pid_t session_id;
	struct sigaction act;
	sigset_t set;
	HANDLE wait_handles[3];
	int fd;
	int ret = EXIT_OK;
	UINT error;
	bool gotData = false;

	HANDLE hPipe_in, hPipe_out;

	read_fd = dup(STDIN_FILENO);
	write_fd = dup(STDOUT_FILENO);

	wLog *mWLogRoot = WLog_GetRoot();

	if (argc < 2) {
		return  EXIT_FAIL_NO_SESSION_ID;
	}

	fd = open("/dev/zero", O_RDONLY);
	dup2(fd, STDIN_FILENO);
	close(fd);


	if (argc > 2) {
		WLog_SetLogAppenderType(mWLogRoot, WLOG_APPENDER_FILE);
		wLogAppender* appender = WLog_GetLogAppender(mWLogRoot);

		std::string filepath, folder, file;
		filepath.assign(argv[2]);

		SplitFilename(filepath, folder, file);

		if (folder.size()) {
			WLog_ConfigureAppender(appender, "outputfilepath", const_cast<char *>(folder.c_str()) );
		}
		WLog_ConfigureAppender(appender, "outputfilename", const_cast<char *>(file.c_str()) );
		WLog_SetLogLevel(mWLogRoot, WLOG_TRACE);
	}

#ifdef WITH_LIBSYSTEMD
	char journal_label[255];
	UINT32 ogon_session_id = atol(argv[1]);

	snprintf(journal_label, 255, "ogon-launcher-%" PRIu32 "", ogon_session_id);
	fd = sd_journal_stream_fd(journal_label, LOG_INFO, 1);
	if (fd < 0) {
		fd = open("/dev/null", O_WRONLY);
	}
#else
	fd = open("/dev/null", O_WRONLY);
#endif
	dup2(fd, STDERR_FILENO);
	dup2(fd, STDOUT_FILENO);
	close(fd);

	hPipe_in = GetFileHandleForFileDescriptor(read_fd);
	hPipe_out = GetFileHandleForFileDescriptor(write_fd);

	WLog_Print(logger_ModuleCommunication, WLOG_INFO , "started");
	/*
	 * If setsid fails the launcher does either have a controlling tty,
	 * not enough rights or is already a process group leader.
	 */
	if ((session_id = setsid()) < 0) {
		WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "setsid failed!");
		return EXIT_FAIL_SETSID;
	}

	/* If the parent dies this proces get's a SIGPIPE */
	prctl(PR_SET_PDEATHSIG, SIGPIPE);

	gModule.setSessionPID(session_id);

	if (!(gSignalEvent = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "CreateEvent failed!");
		return EXIT_FAIL_CREATEEVENT;
	}

	if (!(gSignalStop = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "CreateEvent failed!");
		return EXIT_FAIL_CREATEEVENT;
	}

	InitializeCriticalSection(&gProcessInfoLock);

	memset(gSignals, 0, sizeof(gSignals));
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = &signal_handler;
	act.sa_flags = SA_SIGINFO;
	/*
	 * SIGTERM is forwarded to all child processes
	 * SIGCHLD handles process death(s)
	 * SIGPIPE might happen on the communication pipe
	 */
	if (sigaction(SIGTERM, &act, NULL) ||
			sigaction(SIGCHLD, &act, NULL) ||
			sigaction(SIGPIPE, &act, NULL)) {
		WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "sigaction failed!");
		return EXIT_FAIL_SIGACTION;
	}

	if (sigemptyset(&set) || sigaddset(&set, SIGTERM) ||
			sigaddset(&set, SIGCHLD) ||
			sigaddset(&set, SIGPIPE) ||
			pthread_sigmask(SIG_UNBLOCK, &set, NULL)) {
		WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "sigmask failed!");
		return EXIT_FAIL_SIGMASK;
	}

	gModule.initHandles(hPipe_in, hPipe_out);

	wait_handles[0] = gSignalEvent;
	wait_handles[1] = hPipe_in;
	wait_handles[2] = gSignalStop;

	while (run) {
		DWORD status;
		DWORD timeout = 50000;


		status = WaitForMultipleObjects(3, wait_handles, FALSE, gotData ? INFINITE : timeout);
		if (status == WAIT_TIMEOUT) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "timeout for first communication failed!");
			ret = EXIT_TIMEOUT;
			break;
		}

		if ((status == (WAIT_OBJECT_0)) || (WaitForSingleObject(gSignalEvent, 0) == WAIT_OBJECT_0)) {
			int signum;
			ResetEvent(gSignalEvent);
			/* check if some unix signal has been flagged */
			for (signum = 0; signum < NSIG; signum++) {
				if (!gSignals[signum].set) {
					continue;
				}
				gSignals[signum].set = FALSE;
				handle_signal(signum);
			}
		}

		if ((status == (WAIT_OBJECT_0 + 1)) || (WaitForSingleObject(wait_handles[1], 0) == WAIT_OBJECT_0)) {
			/* Handle communication with session-manager */
			error = gModule.doRead();

			if (error == REMOTE_CLIENT_EXIT) {
				WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "requested shutdown!");
				ret = 0;
				break;
			} else if (error) {
				WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "communication failed!");
				ret = EXIT_FAIL_PROCESS;
				break;
			}

			gotData = true;
		}

		if ((status == (WAIT_OBJECT_0 + 2)) || (WaitForSingleObject(gSignalStop, 0) == WAIT_OBJECT_0)) {
			ret = 0;
			WLog_Print(logger_ModuleCommunication, WLOG_INFO , "requested shutdown!");
			break;
		}
	}
	CloseHandle(gSignalEvent);
	CloseHandle(gSignalStop);
	CloseHandle(hPipe_in);
	CloseHandle(hPipe_out);
	return ret;
}
