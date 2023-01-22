/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Qt Backend Module
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

#include <sys/types.h>
#include <sys/wait.h>

#include <winpr/crt.h>
#include <winpr/environment.h>
#include <winpr/pipe.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/version.h>
#include <winpr/wlog.h>

#include <ogon/backend.h>

#include <ogon/module.h>
#include "qt_module.h"
#include "../common/module_helper.h"
#include "../../common/global.h"

#include <ogon/api.h>

static RDS_MODULE_CONFIG_CALLBACKS gConfig;
static RDS_MODULE_STATUS_CALLBACKS gStatus;

static wLog *gModuleLog;

struct rds_module_qt
{
	RDS_MODULE_COMMON commonModule;

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	HANDLE monitorThread;
	HANDLE monitorStopEvent;
};
typedef struct rds_module_qt rdsModuleQt;

static DWORD qt_clean_up_process(PROCESS_INFORMATION *pi) {
	DWORD ret = 0;
	if (pi->hProcess) {
		GetExitCodeProcess(pi->hProcess, &ret);
		CloseHandle(pi->hProcess);
		pi->hProcess = NULL;
	}
	if (pi->hThread) {
		CloseHandle(pi->hThread);
		pi->hThread = NULL;
	}	return ret;
}

static int qt_rds_stop_process(PROCESS_INFORMATION *pi, unsigned int wait_sec, UINT32 sessionId) {
	int ret = 0;
#ifdef WIN32
	if (!pi->hProcess ) {
		return 0;
	}
	TerminateProcess(pi.hProcess, 0);

	 // Wait until child process exits.
	WaitForSingleObject(pi->hProcess, 5);
#else
	if (gStatus.removeMonitoringProcess(pi->dwProcessId)) {
		TerminateChildProcess(pi->dwProcessId, wait_sec * 1000, NULL, sessionId);
	}
#endif
	qt_clean_up_process(pi);
	return ret;
}

static RDS_MODULE_COMMON* qt_rds_module_new(void)
{
	rdsModuleQt* qt = (rdsModuleQt *)calloc(1, sizeof(rdsModuleQt));
	if (!qt) {
		fprintf(stderr, "%s: error allocating qt module memory\n", __FUNCTION__);
		return NULL;
	}

	WLog_Print(gModuleLog, WLOG_DEBUG, "RdsModuleNew");

	return (RDS_MODULE_COMMON*) qt;
}

static void qt_rds_module_free(RDS_MODULE_COMMON* module)
{
	WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": RdsModuleFree", module->sessionId);
	free(module);
}

static char* qt_rds_module_start(RDS_MODULE_COMMON* module)
{
	BOOL status;
	char* pipeName;
	char qPipeName[256];
	long xres, yres,colordepth;
	char lpCommandLine[256];
	const char* endpoint = "Qt";
	char cmd[256];

	rdsModuleQt *qt = (rdsModuleQt *)module;
	DWORD SessionId = module->sessionId;

	WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": RdsModuleStart: Endpoint: %s", SessionId, endpoint);

	ZeroMemory(&(qt->si), sizeof(STARTUPINFO));
	qt->si.cb = sizeof(STARTUPINFO);
	ZeroMemory(&(qt->pi), sizeof(PROCESS_INFORMATION));

	initResolutions(&gConfig, SessionId, &xres, &yres, &colordepth);

	sprintf_s(qPipeName, sizeof(qPipeName), "/tmp/.pipe/ogon_%" PRIu32 "_%s", SessionId, endpoint);
	if (!SetEnvironmentVariableEBA(&module->envBlock, "OGON_PIPE_PATH", qPipeName)) {
		WLog_Print(gModuleLog, WLOG_FATAL, "s %" PRIu32 ": Could not set OGON_PIPE_PATH in the environment block", SessionId);
		return NULL;
	}

	if (!getPropertyStringWrapper(module->baseConfigPath, &gConfig, SessionId, "cmd", cmd, 256)) {
		WLog_Print(gModuleLog, WLOG_FATAL, "s %" PRIu32 ": Could not query %s.cmd, stopping qt module again, because of missing command ",
				   SessionId, module->baseConfigPath);
		return NULL;
	}

	sprintf_s(lpCommandLine, sizeof(lpCommandLine),	"%s -platform ogon", cmd);

	WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": Starting process with command line: %s", SessionId, lpCommandLine);

	status = CreateProcessAsUserA(module->userToken, NULL, lpCommandLine, NULL,
			NULL, FALSE, 0, module->envBlock, NULL,	&(qt->si), &(qt->pi));
	if (!status) {
		WLog_Print(gModuleLog, WLOG_FATAL, "s %" PRIu32 ": Could not start qt application %s", SessionId, cmd);
		goto out_create_process_error;
	}

	gStatus.addMonitoringProcess(qt->pi.dwProcessId, module->sessionId, TRUE, module);

	WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": Process %" PRIu32 "/%" PRIu32 " created with status: %" PRId32 "",
			   SessionId, qt->pi.dwProcessId, qt->pi.dwThreadId, status);

	pipeName = (char *)malloc(256);
	if (!pipeName) {
		WLog_Print(gModuleLog, WLOG_FATAL, "s %" PRIu32 ": out of memory while allocating pipeName", SessionId);
		goto out_pipe_name_error;
	}

	ogon_named_pipe_get_endpoint_name(SessionId, endpoint, pipeName, 256);
	ogon_named_pipe_clean(pipeName);

	if (!WaitNamedPipeA(pipeName, 5 * 1000)) {
		WLog_Print(gModuleLog, WLOG_FATAL, "s %" PRIu32 ": WaitNamedPipe failure: %s", SessionId, pipeName);
		goto out_wait_pipe_error;
	}
	return pipeName;

out_wait_pipe_error:
	free(pipeName);
out_pipe_name_error:
	qt_rds_stop_process(&(qt->pi), 2, SessionId);
out_create_process_error:
	return NULL;

}

static int qt_rds_module_stop(RDS_MODULE_COMMON *module)
{
	rdsModuleQt *qt = (rdsModuleQt*) module;

	WLog_Print(gModuleLog, WLOG_DEBUG, "s %" PRIu32 ": RdsModuleStop", module->sessionId);

	qt_rds_stop_process(&(qt->pi), 2, module->sessionId);
	return 0;
}

static char *qt_get_custom_info(RDS_MODULE_COMMON *module)
{
	rdsModuleQt *qt = (rdsModuleQt*)module;
	char *customInfo = (char *)malloc(11);
	if (!customInfo) {
		WLog_Print(gModuleLog, WLOG_ERROR, "s %" PRIu32 ": malloc failed", module->sessionId);
		return NULL;
	}

	snprintf(customInfo, 11, "%" PRIu32 "", qt->pi.dwProcessId);
	return customInfo;
}

static int qt_module_init() {
#if WINPR_VERSION_MAJOR < 3
	WLog_Init();
#endif
	gModuleLog = WLog_Get("com.ogon.module.qt");
	return 0;
}

static int qt_module_destroy() { return 0; }
static int qt_rds_module_connect(RDS_MODULE_COMMON *module) {
	OGON_UNUSED(module);
	return 0;
}

static int qt_rds_module_disconnect(RDS_MODULE_COMMON *module) {
	OGON_UNUSED(module);
	return 0;
}

OGON_API int RdsModuleEntry(RDS_MODULE_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->Version = 1;
	pEntryPoints->Name = "Qt";

	pEntryPoints->Init = qt_module_init;
	pEntryPoints->New = qt_rds_module_new;
	pEntryPoints->Free = qt_rds_module_free;

	pEntryPoints->Start = qt_rds_module_start;
	pEntryPoints->Stop = qt_rds_module_stop;
	pEntryPoints->getCustomInfo = qt_get_custom_info;
	pEntryPoints->Destroy = qt_module_destroy;
	pEntryPoints->Connect = qt_rds_module_connect;
	pEntryPoints->Disconnect = qt_rds_module_disconnect;

	gStatus = pEntryPoints->status;
	gConfig = pEntryPoints->config;

	return 0;
}
