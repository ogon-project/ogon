/**
 * ogon - Free Remote Desktop Services
 * RDP Server
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#ifndef WIN32
#include <signal.h>
#endif

#include <winpr/crt.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/cmdline.h>
#include <winpr/library.h>
#include <winpr/file.h>
#include <winpr/ssl.h>
#include <winpr/wlog.h>

#include <ogon/version.h>
#include <ogon/build-config.h>

#include <winpr/sysinfo.h>
#include <openssl/ssl.h>

#include "icp/icp_client_stubs.h"
#include "../common/global.h"
#include "../common/icp.h"
#include "../common/procutils.h"
#include "ogon.h"
#include "app_context.h"
#include "openh264.h"
#include "peer.h"
#include "eventloop.h"
#include "buildflags.h"

#define TAG OGON_TAG("core.main")
#define PIDFILE "ogon-rdp-server.pid"

static HANDLE g_term_event = NULL;
static HANDLE g_signal_event = NULL;
static UINT16 g_listen_port = 3389;

COMMAND_LINE_ARGUMENT_A ogon_args[] = {
	{ "help", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "show help screen" },
	{ "kill", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "kill a running daemon" },
	{ "nodaemon", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "run in foreground" },
	{ "version", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "print the version" },
	{ "port", COMMAND_LINE_VALUE_REQUIRED, "<number>", "3389", NULL, -1, NULL, "listening port" },
	{ "log", COMMAND_LINE_VALUE_REQUIRED, "<backend>", "", NULL, -1, NULL, "logging backend (syslog or journald)" },
	{ "loglevel", COMMAND_LINE_VALUE_REQUIRED, "<level>", "", NULL, -1, NULL, "logging level"},
	{ "buildconfig", COMMAND_LINE_VALUE_FLAG, "", NULL, NULL, -1, NULL, "print build configuration"},
	{ NULL, 0, NULL, NULL, NULL, -1, NULL, NULL }
};

static void printhelprow(const char *kshort, const char *klong, const char *helptext) {
	if (kshort) {
		printf("    %s, %-20s %s\n", kshort,klong, helptext);
	} else {
		printf("        %-20s %s\n", klong, helptext);
	}

}

static void printhelp(const char *bin) {
	printf("Usage: %s [options]\n", bin);
	printf("\noptions:\n\n");
	printhelprow(NULL, "--help", "print this help screen");
	printhelprow(NULL, "--kill", "kill a running daemon");
	printhelprow(NULL, "--version", "print the version");
	printhelprow(NULL, "--nodaemon", "run in foreground");
	printhelprow(NULL, "--port=<number>", "listening port (default: 3389)");
	printhelprow(NULL, "--log=<backend>", "logging backend (syslog or journald)");
	printhelprow(NULL, "--loglevel=<level>", "level for logging");
	printhelprow(NULL, "--buildconfig", "Print build configuration");
}


#ifndef WIN32
static struct {
	BOOL set;
	siginfo_t info;
} g_signals[NSIG];


static void signal_handler_native(int signal, siginfo_t *siginfo, void *context) {
	/**
	 * Warning: This is the native signal handler
	 * Only use async-signal-safe functions here!
	 */
	OGON_UNUSED(context);

	if (signal >= NSIG) {
		return;
	}

	g_signals[signal].set = TRUE;
	g_signals[signal].info = *siginfo;

	SetEvent(g_signal_event);
}

static void signal_handler_internal(int signal) {
	pid_t pid;
	uid_t uid;
	char *pname;

	if (signal >= NSIG) {
		return;
	}

	pid = g_signals[signal].info.si_pid;
	uid = g_signals[signal].info.si_uid;
	pname = get_process_name(pid);

	WLog_INFO(TAG, "received signal %d from process %lu '%s' (uid %lu)",
		signal, (unsigned long) pid, pname ? pname : "*unknown*", (unsigned long) uid);

	free(pname);

	if ((DWORD)pid == GetCurrentProcessId()) {
		WLog_INFO(TAG, "signal has been sent from my own process!");
	}

	switch (signal) {
		/* these signals trigger a shutdown */
		case SIGINT:
		case SIGTERM:
			SetEvent(g_term_event);
			return;
		default:
			break;
	}

	/* some unexpected signal wants to be handled. shutdown as well in this case */
	WLog_ERR(TAG, "shutting down because of unexpected signal %d from process %lu (uid %lu) ...",
		signal, (unsigned long) pid, (unsigned long) uid);

	SetEvent(g_term_event);
}
#endif /* WIN32 not defined */


static BOOL ogon_peer_accepted(freerdp_listener* instance, freerdp_peer *peer) {
	ogon_connection_runloop *runloop;

	OGON_UNUSED(instance);

	runloop = ogon_runloop_new(peer);
	return runloop != NULL;
}

typedef struct {
	BOOL doRun;
	freerdp_listener *listener;
	BOOL returnValue;
} main_loop_context;

static int handle_term_event(int mask, int fd, HANDLE handle, void *data) {
	OGON_UNUSED(mask);
	OGON_UNUSED(fd);
	OGON_UNUSED(handle);

	main_loop_context *context = data;
	context->doRun = FALSE;
	return 0;
}

static int handle_signal_event(int mask, int fd, HANDLE handle, void *data) {
	OGON_UNUSED(mask);
	OGON_UNUSED(fd);
	OGON_UNUSED(data);

	int signum;
	WLog_DBG(TAG, "g_signal_event triggered");
	ResetEvent(handle);

	/* check if some unix signal has been flagged */
	for (signum = 0; signum < NSIG; signum++) {
		if (!g_signals[signum].set) {
			continue;
		}
		g_signals[signum].set = FALSE;
		signal_handler_internal(signum);
	}

	return 0;
}

static int handle_incoming_connection(int mask, int fd, HANDLE handle, void *data) {
	OGON_UNUSED(mask);
	OGON_UNUSED(fd);
	OGON_UNUSED(handle);

	main_loop_context *context = data;
	freerdp_listener *listener = context->listener;

	if (!listener->CheckFileDescriptor(listener)) {
		WLog_ERR(TAG, "Failed to check FreeRDP file descriptor %p", handle);
		context->returnValue = FALSE;
		context->doRun = FALSE;
	}

	return 0;
}

#define MAX_EVENT_HANDLES 32
static BOOL ogon_main_loop(UINT16 port) {
	DWORD nCount, i;
	HANDLE events[MAX_EVENT_HANDLES];
	main_loop_context context;
	ogon_event_loop *loop;
	ogon_event_source *term_event_source;
	ogon_event_source *handle_sources[MAX_EVENT_HANDLES];
#ifndef WIN32
	ogon_event_source *signal_event_source;
#endif

	context.doRun = TRUE;
	context.returnValue = FALSE;

	if (!(context.listener = freerdp_listener_new())) {
		WLog_ERR(TAG, "error creating listener");
		return FALSE;
	}
	context.listener->PeerAccepted = ogon_peer_accepted;

	if (!context.listener->Open(context.listener, NULL, port)) {
		WLog_ERR(TAG, "error opening listener on port %"PRIu16"", port);
		freerdp_listener_free(context.listener);
		return FALSE;
	}

	loop = eventloop_create();
	if (!loop) {
		WLog_ERR(TAG, "unable to create the eventloop");
		goto error_eventloop;
	}

	term_event_source = eventloop_add_handle(loop, OGON_EVENTLOOP_READ, g_term_event,
			handle_term_event, &context);
	if (!term_event_source) {
		WLog_ERR(TAG, "unable to create to add term event in the event loop");
		goto error_term_event;
	}

#ifndef WIN32
	signal_event_source = eventloop_add_handle(loop, OGON_EVENTLOOP_READ, g_signal_event,
			handle_signal_event, &context);
	if (!signal_event_source) {
		WLog_ERR(TAG, "unable to create to add signal event in the event loop");
		goto error_signal_event;
	}
#endif

	ZeroMemory(handle_sources, sizeof(handle_sources));
	nCount = context.listener->GetEventHandles(context.listener, events, MAX_EVENT_HANDLES);
	if (!nCount) {
		WLog_ERR(TAG, "Failed to get FreeRDP file descriptor");
		goto error_listener_handles;
	}

	for (i = 0; i < nCount; i++) {
		handle_sources[i] = eventloop_add_handle(loop, OGON_EVENTLOOP_READ, events[i],
				handle_incoming_connection, &context);
		if (!handle_sources[i]) {
			WLog_ERR(TAG, "Failed to create an event source for handle %p", events[i]);
			goto error_handle_sources;
		}
	}

	context.returnValue = TRUE;
	while (context.doRun) {
		eventloop_dispatch_loop(loop, 1 * 1000);
	}


error_handle_sources:
	for (i = 0; i < MAX_EVENT_HANDLES && handle_sources[i]; i++) {
		eventloop_remove_source(&handle_sources[i]);
	}
error_listener_handles:
#ifndef WIN32
	eventloop_remove_source(&signal_event_source);
error_signal_event:
#endif
	eventloop_remove_source(&term_event_source);

error_term_event:
	eventloop_destroy(&loop);
error_eventloop:
	context.listener->Close(context.listener);
	freerdp_listener_free(context.listener);
	return context.returnValue;
}

void session_manager_connection_lost() {
	WLog_INFO(TAG, "session manager connection lost, killing all sessions");
	app_context_stop_all_connections();
}

static void initializeWLog(unsigned wlog_appender_type, DWORD logLevel) {
	wLog *wlog_root;
	wLogLayout *layout;
	wLogAppender *appender;
	const char *prefixFormat;

	WLog_Init();

	if (!(wlog_root = WLog_GetRoot())) {
		fprintf(stderr, "Failed to get the logger root\n");
		goto fail;
	}

	if (!WLog_SetLogAppenderType(wlog_root, wlog_appender_type)) {
		fprintf(stderr, "Failed to initialize the logger appender type\n");
		goto fail;
	}

	if (!(layout = WLog_GetLogLayout(wlog_root))) {
		fprintf(stderr, "Failed to get the logger layout\n");
		goto fail;
	}

	switch (wlog_appender_type) {
	case WLOG_APPENDER_JOURNALD:
		if (!(appender = WLog_GetLogAppender(wlog_root))) {
			fprintf(stderr, "failed to retrieve root appender\n");
			goto fail;
		}
		if (!WLog_ConfigureAppender(appender, "identifier", "ogon-rdp")) {
			fprintf(stderr, "failed to set journald identifier\n");
			goto fail;
		}
		prefixFormat = "%tid-[%lv:%fl@%fn:%ln] - ";
		break;
	case WLOG_APPENDER_CONSOLE:
	default:
		prefixFormat = "[%yr.%mo.%dy %hr:%mi:%se:%ml] [%pid:%tid] [%lv:%mn] [%fl|%fn|%ln] - ";
		break;
	}

	if (!WLog_Layout_SetPrefixFormat(wlog_root, layout, prefixFormat)) {
		fprintf(stderr, "Failed to set the logger output format\n");
		goto fail;
	}

	if (!WLog_SetLogLevel(wlog_root, logLevel)) {
		fprintf(stderr, "Failed to set the logger level\n");
		goto fail;
	}
	return;
fail:
	exit(1);
}

static void parseCommandLine(int argc, char **argv, int *no_daemon, int *kill_process,
		unsigned *wlog_appender_type, DWORD *log_level) {
	DWORD flags;
	int status = 0;
	COMMAND_LINE_ARGUMENT_A *arg;

	*no_daemon = *kill_process = 0;

	flags = 0;
	flags |= COMMAND_LINE_SEPARATOR_EQUAL;
	flags |= COMMAND_LINE_SIGIL_DASH;
	flags |= COMMAND_LINE_SIGIL_DOUBLE_DASH;

	status = CommandLineParseArgumentsA(argc, argv, ogon_args, flags, NULL, NULL, NULL);

	if (status != COMMAND_LINE_STATUS_PRINT_HELP && status != 0) {
		fprintf(stderr,"Failed to parse command line: %d\n", status);
		printhelp(argv[0]);
		exit(-1);
	}

	arg = ogon_args;

	do {
		if (!(arg->Flags & COMMAND_LINE_VALUE_PRESENT))
			continue;

		CommandLineSwitchStart(arg)

		CommandLineSwitchCase(arg, "kill") {
			*kill_process = 1;
		}
		CommandLineSwitchCase(arg, "nodaemon") {
			*no_daemon = 1;
		}
		CommandLineSwitchCase(arg, "version") {
			printf("ogon %s (commit %s)\n", OGON_VERSION_FULL, GIT_REVISION);
			exit(0);
		}
		CommandLineSwitchCase(arg, "buildconfig") {
			printf("Build configuration: " BUILD_CONFIG "\n"
				"Build type:          " BUILD_TYPE "\n"
				"CFLAGS:             " CFLAGS "\n"
				"Compiler:            " COMPILER_ID ", " COMPILER_VERSION "\n");
			exit(0);
		}
		CommandLineSwitchCase(arg, "h") {
			printhelp(argv[0]);
			exit(0);
		}
		CommandLineSwitchCase(arg, "help") {
			printhelp(argv[0]);
			exit(0);
		}
		CommandLineSwitchCase(arg, "port") {
			g_listen_port = atoi(arg->Value);
			if (g_listen_port == 0) {
				fprintf(stderr, "invalid port number %"PRIu16"\n", g_listen_port);
				exit(1);
			}
		}
		CommandLineSwitchCase(arg, "log") {
			if (!strcmp(arg->Value, "syslog")) {
				*wlog_appender_type = WLOG_APPENDER_SYSLOG;
			} else if (!strcmp(arg->Value, "journald")) {
				*wlog_appender_type = WLOG_APPENDER_JOURNALD;
			} else {
				fprintf(stderr, "unknown logging backend '%s'\n", arg->Value);
				fprintf(stderr, "valid options are 'syslog' and 'journald'\n");
				exit(1);
			}
		}
		CommandLineSwitchCase(arg, "loglevel") {
			if (!strcasecmp(arg->Value, "debug")) {
				*log_level = WLOG_DEBUG;
			} else if (!strcasecmp(arg->Value, "info")) {
				*log_level = WLOG_INFO;
			} else if (!strcasecmp(arg->Value, "warn")) {
				*log_level = WLOG_WARN;
			} else if (!strcasecmp(arg->Value, "error")) {
				*log_level = WLOG_ERROR;
			} else {
				fprintf(stderr, "unknown logging level '%s'\n", arg->Value);
				exit(1);
			}
		}
		CommandLineSwitchEnd(arg)
	}
	while ((arg = CommandLineFindNextArgumentA(arg)) != NULL);
}

void killProcess(char *pid_file)  __attribute__ ((noreturn));

void killProcess(char *pid_file) {
	FILE* fp;
	char text[256];
	pid_t pid;

	fprintf(stderr, "stopping ogon-rdp-server\n");

	fp = NULL;

	if (PathFileExistsA(pid_file)) {
		fp = fopen(pid_file, "r");
	}

	if (!fp) {
		fprintf(stderr, "problem opening pid file [%s]\n", pid_file);
		fprintf(stderr, "maybe its not running\n");
	}
	else {
		int status;
		ZeroMemory(text, 32);
		status = fread((void*) text, 1, 31, fp);
		fclose(fp);

		if (status <= 0) {
			fprintf(stderr, "Coudln't read the process id\n");
			exit(1);
		}
		pid = atoi(text);

		if (pid > 0) {
			fprintf(stderr, "stopping process id %lu\n", (unsigned long) pid);
			kill(pid, SIGTERM);
		} else {
			fprintf(stderr, "refusing to kill pid 0\n");
		}

	}

	exit(0);
}

void checkPidFile(char *pid_file) {

	FILE* fp;

	if (!PathFileExistsA(OGON_VAR_PATH)) {
		fprintf(stderr, "Directory '%s' doesn't exist or isn't accessible\n", OGON_VAR_PATH);
		exit(1);
	}

	if (!PathFileExistsA(OGON_PID_PATH)) {
		fprintf(stderr, "Directory '%s' doesn't exist or isn't accessible\n", OGON_PID_PATH);
		exit(1);
	}

	/* make sure we can write to pid file */
	fp = fopen(pid_file, "w+");

	if (!fp) {
		fprintf(stderr, "running in daemon mode with no access to pid files, quitting\n");
		exit(0);
	}

	if (fwrite((void*) "0", 1, 1, fp) < 1) {
		fprintf(stderr, "running in daemon mode with no access to pid files, quitting\n");
		fclose(fp);
		exit(0);
	}

	fclose(fp);
	DeleteFileA(pid_file);
}

void daemonizeCode(char *pid_file) {

	FILE* fp;
	pid_t pid;
	char text[256];

#ifndef __sun
	int maxfd;
#endif
	int fd;

	/* start of daemonizing code */
	pid = fork();

	if (pid == -1) {
		fprintf(stderr, "problem forking\n");
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
		fprintf(stderr, "error opening pid file [%s]\n", pid_file);
	} else {
		sprintf_s(text, sizeof(text), "%lu", (unsigned long) pid);
		if (fwrite((void*) text, strlen(text), 1, fp) != 1) {
			fprintf(stderr, "problem writing to pid file..\n");
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
	for (fd = 0; fd < maxfd; fd++) {
		close(fd);
	}
#endif // __sun
	fd = open("/dev/null", O_RDWR);
	if (fd != STDIN_FILENO) {
		fprintf(stderr, "/dev/null is not STDIN_FILENO\n");
		exit(-1);
	}
	if (dup2(STDIN_FILENO, STDOUT_FILENO) != STDOUT_FILENO) {
		fprintf(stderr, "dup2 failed. result is not STDOUT_FILENO\n");
		exit(-1);
	}
	if (dup2(STDIN_FILENO, STDERR_FILENO) != STDERR_FILENO) {
		fprintf(stderr, "dup2 failed. result is not STDERR_FILENO\n");
		exit(-1);
	}
	/* end of daemonizing code */
}

int main(int argc, char** argv) {
	int no_daemon;
	int kill_process;
	char pid_file[256];
	int ret = 0;
	unsigned wlog_appender_type = WLOG_APPENDER_CONSOLE;
	DWORD log_level = WLOG_ERROR;

#ifndef WIN32
	sigset_t set;
	struct sigaction act;
#endif

	no_daemon = kill_process = 0;

	parseCommandLine(argc, argv, &no_daemon, &kill_process, &wlog_appender_type, &log_level);

	sprintf_s(pid_file, 255, "%s/%s", OGON_PID_PATH, PIDFILE);

	if (kill_process) {
		killProcess(pid_file);
	}

	if (PathFileExistsA(pid_file)) {
		fprintf(stderr, "It looks like ogon-rdp-server is already running,\n");
		fprintf(stderr, "if not delete the file %s and try again\n", pid_file);
		return 1;
	}

#ifndef WIN32
	/* block all signals per default */
	sigfillset(&set);
	if (pthread_sigmask(SIG_BLOCK, &set, NULL)) {
		fprintf(stderr, "error blocking all signals\n");
		return 1;
	}
#endif

	if (!no_daemon) {
		checkPidFile(pid_file);
	}

	if (!no_daemon)	{
		daemonizeCode(pid_file);
	}

	initializeWLog(wlog_appender_type, log_level);

	WLog_INFO(TAG, "ogon-rdp-server %s (commit %s) started", OGON_VERSION_FULL, GIT_REVISION);
	WLog_INFO(TAG, "protocol version %d.%d", OGON_PROTOCOL_VERSION_MAJOR, OGON_PROTOCOL_VERSION_MINOR);

	if (!winpr_InitializeSSL(WINPR_SSL_INIT_ENABLE_LOCKING)) {
		WLog_ERR(TAG, "Unable to initialize winpr SSL");
		ret = 1;
		goto fail_init_ssl;
	}

	if (!(g_term_event = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		WLog_ERR(TAG, "error creating termination event");
		ret = 1;
		goto fail_term_event;
	}

#ifndef WIN32
	if (!(g_signal_event = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		WLog_ERR(TAG, "error creating signal event");
		ret = 1;
		goto fail_signal_event;
	}
#endif

	if (!app_context_init()) {
		WLog_ERR(TAG, "Unable to initialize app context");
		ret = 1;
		goto fail_app_context;
	}

	if (ogon_icp_start(g_term_event, OGON_PROTOCOL_VERSION_MAJOR, OGON_PROTOCOL_VERSION_MINOR) < 0) {
		WLog_ERR(TAG, "error creating ICP server");;
		ret = 1;
		goto fail_icp_start;
	}

	ogon_icp_set_disconnect_cb(session_manager_connection_lost);

#ifdef WITH_OPENH264
	ogon_openh264_library_open();
#endif

#ifndef WIN32
	memset(g_signals, 0, sizeof(g_signals));

	/* setup signal handler */
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = &signal_handler_native;
	/* our signal handler uses the sa_sigaction prototype */
	act.sa_flags = SA_SIGINFO;
	/* handle the following signals */
	sigaction(SIGINT, &act, NULL);
	sigaction(SIGTERM, &act, NULL);
	/* and unblock them as well */
	sigemptyset(&set);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGTERM);
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);
#endif

	WLog_DBG(TAG, "entering main loop");

	ogon_main_loop(g_listen_port);

	WLog_DBG(TAG, "returned from main loop, stopping connections");

	app_context_stop_all_connections();

	WLog_DBG(TAG, "all connections stopped, stopping subsystems");

#ifdef WITH_OPENH264
	ogon_openh264_library_close();
#endif

	ogon_icp_shutdown();
fail_icp_start:
	app_context_uninit();
fail_app_context:
#ifndef WIN32
	CloseHandle(g_signal_event);
fail_signal_event:
#endif
	CloseHandle(g_term_event);
fail_term_event:
	winpr_CleanupSSL(WINPR_SSL_CLEANUP_GLOBAL);
fail_init_ssl:
	if (!no_daemon)	{
		DeleteFileA(pid_file);
	}

	WLog_DBG(TAG, "Terminating..");
	WLog_Uninit();

	return ret;
}
