/**
 * ogon - Free Remote Desktop Services
 * Session Manager
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

#include <iostream>
#include <stdio.h>

#include <sys/types.h>
#include <signal.h>
#include <appcontext/ApplicationContext.h>

#include <fcntl.h>
#include <winpr/cmdline.h>
#include <winpr/path.h>
#include <ogon/version.h>
#include <ogon/build-config.h>
#include "../../common/global.h"
#include "../../common/procutils.h"
#include "buildflags.h"

#define PIDFILE "ogon-session-manager.pid"

using namespace std;

static HANDLE gMainEvent;
char *g_configFileName = 0;
static wLog *logger_sessionManager = WLog_Get("ogon.sessionmanager");

int checkConfigFile(std::string checkConfigFileName);
void killProcess(char *pid_file)  __attribute__ ((noreturn));

static void reloadConfig(int /*signal*/) {
	std::string name;
	if (g_configFileName) {
		name.assign(g_configFileName);
	} else {
		name = APP_CONTEXT.getSystemConfigPath()  + "/config.ini";
	}
	APP_CONTEXT.loadConfig(name);
}

#ifndef WIN32

static struct {
	BOOL set;
	siginfo_t info;
} gSignals[NSIG];

static void signal_handler_native(int signal, siginfo_t *siginfo, void *context) {
	/**
	 * Warning: This is the native signal handler
	 * Only use async-signal-safe functions here!
	 */
	OGON_UNUSED(context);

	if (signal >= NSIG)
		return;

	gSignals[signal].set = TRUE;
	gSignals[signal].info = *siginfo;

	SetEvent(gMainEvent);
}

static void signal_handler_internal(int signal) {
	pid_t pid;
	uid_t uid;
	char *pname;

	if (signal >= NSIG)
		return;

	pid = gSignals[signal].info.si_pid;
	uid = gSignals[signal].info.si_uid;
	pname = get_process_name(pid);

	WLog_Print(logger_sessionManager, WLOG_INFO,
		"received signal %d from process %lu '%s' (uid %lu)",
		signal, (unsigned long) pid, pname ? pname : "*unknown*", (unsigned long) uid);

	free(pname);

	if ((DWORD)pid == GetCurrentProcessId()) {
		WLog_Print(logger_sessionManager, WLOG_INFO,
			"signal has been sent from my own process!");
	}

	switch (signal) {
		/* signals handled specially */
		case SIGHUP:
			reloadConfig(signal);
			return;
		/* these signals trigger a shutdown */
		case SIGINT:
		case SIGTERM:
			APP_CONTEXT.setShutdown();
			return;
		default:
			break;
	}

	/* some unexpected signal wants to be handled. shutdown as well in this case */
	WLog_Print(logger_sessionManager, WLOG_ERROR,
		"shutting down because of unexpected signal %d from process %lu (uid %lu) ...",
		signal, (unsigned long) pid, (unsigned long) uid);

	APP_CONTEXT.setShutdown();
}
#endif /* WIN32 not defined */

static COMMAND_LINE_ARGUMENT_A ogon_session_manager_args[] = {
	{ "help", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "prints help" },
	{ "h", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "prints help" },
	{ "kill", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "kill daemon" },
	{ "nodaemon", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "no daemon" },
	{ "config", COMMAND_LINE_VALUE_REQUIRED, "<configfile>", NULL, NULL, -1, NULL, "set config file" },
	{ "version", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "print version" },
	{ "checkconfig", COMMAND_LINE_VALUE_REQUIRED, "<configfile>", "", NULL, -1, NULL, "config file to check" },
	{ "log", COMMAND_LINE_VALUE_REQUIRED, "<syslog> or <journald>", "", NULL, -1, NULL, "Log type to use" },
	{ "loglevel", COMMAND_LINE_VALUE_REQUIRED, "<level>", "", NULL, -1, NULL, "logging level"},
	{ "buildconfig", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "print build configuration"},
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

void printhelprow(const char *kshort, const char *klong, const char *helptext) {
	if (kshort) {
		printf("    %s, %-25s %s\n", kshort,klong, helptext);
	} else {
		printf("        %-25s %s\n", klong, helptext);
	}
}

void printhelp(const char *bin) {
	printf("Usage: %s [options]\n", bin);
	printf("\noptions:\n\n");
	printhelprow("-h", "--help", "prints this help screen");
	printhelprow(NULL, "--kill", "kills a running daemon");
	printhelprow(NULL, "--config=<filename>", "path to the config file");
	printhelprow(NULL, "--checkconfig=<filename>", "checks the given config file");
	printhelprow(NULL, "--version","print the version");
	printhelprow(NULL, "--log=<system> syslog or journald", "log type to use");
	printhelprow(NULL, "--loglevel=<level>", "level for logging");
	printhelprow(NULL, "--buildconfig", "Print build configuration");
}

void parseCommandLine(int argc, char **argv, int &no_daemon, int &kill_process, std::string &checkConfigFileName,
		unsigned &wlog_appender_type, DWORD &log_level) {

	DWORD flags;
	int status = 0;
	COMMAND_LINE_ARGUMENT_A *arg;
	wlog_appender_type = WLOG_APPENDER_CONSOLE;

	no_daemon = kill_process = 0;

	flags = 0;
	flags |= COMMAND_LINE_SIGIL_DASH;
	flags |= COMMAND_LINE_SIGIL_DOUBLE_DASH;
	flags |= COMMAND_LINE_SIGIL_ENABLE_DISABLE;
	flags |= COMMAND_LINE_SEPARATOR_EQUAL;

	status = CommandLineParseArgumentsA(argc, (LPSTR *) argv, ogon_session_manager_args, flags, NULL, NULL, NULL);

	if (status != COMMAND_LINE_STATUS_PRINT_HELP && status != 0)
	{
		fprintf(stderr,"Failed to parse command line: %d\n", status);
		printhelp(argv[0]);
		exit(-1);
	}
	arg = ogon_session_manager_args;


	do {
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT)) {
			continue;
		}

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "version") {
			printf("ogon-session-manager %s (commit %s)\n", OGON_VERSION_FULL, GIT_REVISION);
			exit(0);
		}
		CommandLineSwitchCase(arg, "buildconfig") {
			printf("Build configuration: " BUILD_CONFIG "\n"
				"Build type:          " BUILD_TYPE "\n"
				"CFLAGS:             " CFLAGS "\n"
				"Compiler:            " COMPILER_ID ", " COMPILER_VERSION "\n");
			exit(0);
		}
		CommandLineSwitchCase(arg, "kill") {
			kill_process = 1;
		}
		CommandLineSwitchCase(arg, "nodaemon") {
			no_daemon = 1;
		}
		CommandLineSwitchCase(arg, "config") {
			g_configFileName = arg->Value;
		}
		CommandLineSwitchCase(arg, "checkconfig") {
			checkConfigFileName.assign(arg->Value);
		}
		CommandLineSwitchCase(arg, "log") {
			if (!strcmp(arg->Value, "syslog")) {
				wlog_appender_type = WLOG_APPENDER_SYSLOG;
			} else if (!strcmp(arg->Value, "journald")) {
				wlog_appender_type = WLOG_APPENDER_JOURNALD;
			} else {
				fprintf(stderr, "unknown log system '%s'\n", arg->Value);
				fprintf(stderr, "valid options are 'syslog' or 'journald'\n");
				exit(1);
			}
		}
		CommandLineSwitchCase(arg, "loglevel") {
			if (!strcasecmp(arg->Value, "trace")) {
				log_level = WLOG_TRACE;
			} else if (!strcasecmp(arg->Value, "debug")) {
				log_level = WLOG_DEBUG;
			} else if (!strcasecmp(arg->Value, "info")) {
				log_level = WLOG_INFO;
			} else if (!strcasecmp(arg->Value, "warn")) {
				log_level = WLOG_WARN;
			} else if (!strcasecmp(arg->Value, "error")) {
				log_level = WLOG_ERROR;
			} else {
				fprintf(stderr, "unknown logging level '%s'\n", arg->Value);
				exit(1);
			}
		}
		CommandLineSwitchCase(arg, "h") {
			printhelp(argv[0]);
			exit(0);
		}
		CommandLineSwitchCase(arg, "help") {
			printhelp(argv[0]);
			exit(0);
		}
		CommandLineSwitchEnd(arg)
	} while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

void initWLog(unsigned wlog_appender_type, DWORD logLevel) {
	wLog *wlog_root;
	wLogLayout *layout;
	wLogAppender *appender;

	WLog_Init();

	wlog_root = WLog_GetRoot();
	if (wlog_root) {
		const char *prefixFormat;

		if (!WLog_SetLogAppenderType(wlog_root, wlog_appender_type)) {
			fprintf(stderr, "Failed to initialize the wlog appender\n");
			exit(1);
		}

		if (!(appender = WLog_GetLogAppender(wlog_root))) {
			fprintf(stderr, "Failed to get the wlog appender\n");
			exit(1);
		}

		switch (wlog_appender_type) {
		case WLOG_APPENDER_CONSOLE:
			prefixFormat = "[%yr.%mo.%dy %hr:%mi:%se:%ml] [%pid:%tid] [%lv:%mn] [%fl|%fn|%ln] - ";
			if (!WLog_ConfigureAppender(appender, "outputstream", const_cast<char *>("stderr"))) {
				fprintf(stderr, "Failed to set the wlog output stream\n");
				exit(1);
			}
			break;
		case WLOG_APPENDER_JOURNALD:
			if (!WLog_ConfigureAppender(appender, "identifier", const_cast<char *>("ogon-sm"))) {
				fprintf(stderr, "Failed to set the log identifier for journald\n");
				exit(1);
			}
			/* fall through */
		default:
			prefixFormat = "%tid-[%lv:%fl@%fn:%ln] - ";
			break;
		}

		if (!(layout = WLog_GetLogLayout(wlog_root))) {
			fprintf(stderr, "Failed to get the wlog layout\n");
			exit(1);
		}

		if (!WLog_Layout_SetPrefixFormat(wlog_root, layout, prefixFormat)) {
			fprintf(stderr, "Failed to set the wlog output format\n");
			exit(1);
		}

		if (!WLog_SetLogLevel(wlog_root, logLevel)) {
			fprintf(stderr, "Failed to set the logger level\n");
			exit(1);
		}
	} else {
		fprintf(stderr, "Could not get root logger!\n");
		exit(1);
	}
}

void killProcess(char *pid_file) {

	FILE* fp;
	char text[256];
	pid_t pid;

	WLog_Print(logger_sessionManager, WLOG_INFO, "stopping ogon");

	fp = NULL;

	if (PathFileExistsA(pid_file)) {
		fp = fopen(pid_file, "r");
	}

	if (!fp) {
		WLog_Print(logger_sessionManager, WLOG_FATAL, "problem opening ogon-sessionmanager.pid [%s]", pid_file);
		WLog_Print(logger_sessionManager, WLOG_FATAL, "maybe its not running");
	} else {
		int status;
		ZeroMemory(text, 32);
		status = fread((void *)text, 1, 31, fp);
		fclose(fp);
		if (status <= 0) {
			WLog_Print(logger_sessionManager, WLOG_FATAL, "Unable to read the process id");
			exit(1);
		}
		pid = atoi(text);

		if (pid > 0) {
			WLog_Print(logger_sessionManager, WLOG_INFO, "stopping process id %lu", (unsigned long) pid);
			kill(pid, SIGTERM);
		} else {
			WLog_Print(logger_sessionManager, WLOG_INFO, "refusing to kill pid 0");
		}

	}
	exit(0);
}


int checkConfigFile(std::string checkConfigFileName) {
	if (ogon::sessionmanager::config::PropertyManager::checkConfigFile(checkConfigFileName)) {
		std::cout << "No syntax error detected in config file " << checkConfigFileName << std::endl;
		return 0;
	}
	return 1;
}

void checkPidFile(char *pid_file) {

	FILE* fp;

	if (!PathFileExistsA(OGON_VAR_PATH)) {
		WLog_Print(logger_sessionManager, WLOG_FATAL, "Directory '%s' doesn't exist\n", OGON_VAR_PATH);
		exit(1);
	}

	if (!PathFileExistsA(OGON_PID_PATH)) {
		WLog_Print(logger_sessionManager, WLOG_FATAL, "Directory '%s' doesn't exist\n", OGON_PID_PATH);
		exit(1);
	}

	/* make sure we can write to pid file */
	fp = fopen(pid_file, "w+");

	if (!fp) {
		WLog_Print(logger_sessionManager, WLOG_FATAL, "running in daemon mode with no access to pid files, quitting!");
		exit(0);
	}

	if (fwrite("0", 1, 1, fp) == 0) {
		WLog_Print(logger_sessionManager, WLOG_FATAL, "running in daemon mode with no access to pid files, quitting!");
		exit(0);
	}

	fclose(fp);
	DeleteFileA(pid_file);
}

void daemonizeCode(char *pid_file) {

	int fd;
	FILE* fp;
	pid_t pid;
	char text[256];

#ifndef __sun
	int maxfd;
#endif
	/* start of daemonizing code */
	pid = fork();

	if (pid == -1) {
		fprintf(stderr, "problem forking!\n");
		exit(1);
	}

	if (0 != pid) {
		fprintf(stderr, "process %lu started\n", (unsigned long) pid);
		exit(0);
	}

	/* write the pid to file */
	pid = GetCurrentProcessId();
	fp = fopen(pid_file, "w+");

	if (!fp) {
		fprintf(stderr, "problem with writing process id to ogon-sessionmanager.pid\n");
	} else {
		sprintf_s(text, sizeof(text), "%lu", (unsigned long) pid);
		if (fwrite((void *)text, strlen(text), 1, fp) != 1) {
			fprintf(stderr, "problem writing to pid file ..\n");
		}
		fclose(fp);
	}

	/* Close all file descriptors */
#ifdef __sun
	closefrom(3);
#else
#ifdef F_MAXFD // on some BSD derivates
	maxfd = fcntl(0, F_MAXFD);
#else
	maxfd = sysconf(_SC_OPEN_MAX);
#endif
	for(fd=0; fd < maxfd; fd++) {
		close(fd);
	}
#endif // __sun
	fd = open("/dev/null", O_RDWR);
	if (fd != STDIN_FILENO) {
		fprintf(stderr, "/dev/null is not STDIN_FILENO!\n");
		exit(-1);
	}
	if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO) {
		fprintf(stderr, "dup2 failed, result is not STDOUT_FILENO!\n");
		exit(-1);
	}
	if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO) {
		fprintf(stderr, "dup2 failed, result is not STDERR_FILENO!\n");
		exit(-1);
	}
	/* end of daemonizing code */
}

void setupSignalHandler() {

#ifndef WIN32
	sigset_t set;
	struct sigaction act;

	memset(gSignals, 0, sizeof(gSignals));

	/* setup signal handler */
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = &signal_handler_native;
	/* our signal handler uses the sa_sigaction prototype */
	act.sa_flags = SA_SIGINFO;
	/* block all signals during execution of signal handler */
	sigfillset(&act.sa_mask);
	/* handle the following signals */
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGHUP, &act, NULL);
	/* and unblock them as well */
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGHUP);
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);
#endif
}

void handleSignal() {

#ifndef WIN32
	int signum;

	/* check if some unix signal has been flagged */
	for (signum = 0; signum < NSIG; signum++) {
		if (!gSignals[signum].set) {
			continue;
		}
		gSignals[signum].set = FALSE;
		signal_handler_internal(signum);
	}
#endif
}

int main(int argc, char **argv) {
	pid_t pid;

	int no_daemon;
	int kill_process;
	char pid_file[256];
	std::string checkConfigFileName;
	unsigned wlog_appender_type;
	DWORD logLevel = WLOG_ERROR;
#ifndef WIN32
	sigset_t set;
#endif

	parseCommandLine(argc, argv, no_daemon, kill_process, checkConfigFileName, wlog_appender_type, logLevel);

	if (checkConfigFileName.size()) {
		exit(checkConfigFile(checkConfigFileName));
	}


	sprintf_s(pid_file, 255, "%s/%s", OGON_PID_PATH, PIDFILE);

	if (!kill_process && PathFileExistsA(pid_file)) {
		fprintf(stderr, "It looks like ogon Session Manager is already running,\n");
		fprintf(stderr, "if not delete the %s file and try again!\n", pid_file);
		return 1;
	}

#ifndef WIN32
	/* block all signals per default */
	sigfillset(&set);
	if (pthread_sigmask(SIG_BLOCK, &set, NULL)) {
		fprintf(stderr, "failed to block all signals\n");
		return 1;
	}
#endif

	if (!no_daemon && !kill_process) {
		checkPidFile(pid_file);
		daemonizeCode(pid_file);
	}
	
	APP_CONTEXT.init();
	initWLog(wlog_appender_type, logLevel);

	if (kill_process) {
		killProcess(pid_file);
	}

	if (!(gMainEvent = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		WLog_Print(logger_sessionManager, WLOG_FATAL, "Failed to create main event");
		return 1;
	}

	reloadConfig(0);

	pid = GetCurrentProcessId();

	if (!APP_CONTEXT.startProcessMonitor()) {
		goto stop;
	}

	if (!APP_CONTEXT.startRPCEngines()) {
		goto stop;
	}
	if (APP_CONTEXT.loadModulesFromPath(APP_CONTEXT.getModulePath()) != 0) {
		goto stop;
	}

	if (APP_CONTEXT.loadAuthModulesFromPath(APP_CONTEXT.getAuthModulePath()) != 0) {
		goto stop;
	}

	if (!APP_CONTEXT.startSessionNotifier()){
		goto stop;
	}

	APP_CONTEXT.startTaskExecutor();
	APP_CONTEXT.startSessionTimoutMonitor();

	setupSignalHandler();

	WLog_Print(logger_sessionManager, WLOG_INFO, "ready to serve");
	WLog_Print(logger_sessionManager, WLOG_INFO, "protocol version %d.%d",	OGON_PROTOCOL_VERSION_MAJOR, OGON_PROTOCOL_VERSION_MINOR);

	for (;;) {
		if (WaitForSingleObject(gMainEvent, INFINITE) != WAIT_OBJECT_0) {
			WLog_Print(logger_sessionManager, WLOG_ERROR,
				"WaitForSingleObject (gMainEvent) failed. aborting ...");
			break;
		}
		ResetEvent(gMainEvent);
		WLog_Print(logger_sessionManager, WLOG_DEBUG, "main event signalled");
		handleSignal();
		if (APP_CONTEXT.isShutdown()) {
			break;
		}
	}

	CloseHandle(gMainEvent);
	WLog_Print(logger_sessionManager, WLOG_INFO, "stopping ...");

stop:
	APP_CONTEXT.shutdown();
	APP_CONTEXT.stopProcessMonitor();
	APP_CONTEXT.stopTaskExecutor();
	APP_CONTEXT.stopRPCEngines();
	APP_CONTEXT.stopSessionNotifier();

	/* only main process should delete pid file */
	if ((!no_daemon) && (pid == (pid_t)GetCurrentProcessId())) {
		DeleteFileA(pid_file);
	}

	return 0;
}
