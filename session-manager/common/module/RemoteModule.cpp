/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Remote Module Class
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

#ifndef WIN32
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#endif

#include "RemoteModule.h"
#include <winpr/wlog.h>
#include <winpr/crt.h>
#include <winpr/pipe.h>
#include "Module.pb.h"
#include <appcontext/ApplicationContext.h>
#include "../../common/global.h"
#include "../../common/procutils.h"


#define BUF_SIZE 4096

namespace ogon { namespace sessionmanager { namespace module {

	static wLog *logger_RemoteModule = WLog_Get("ogon.sessionmanager.module.remotemodule");

	RemoteModule::RemoteModule() {
	}

	int RemoteModule::initModule(const std::string &moduleFileName,
		RDS_MODULE_ENTRY_POINTS *entrypoints) {

		if (!entrypoints){
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "no entry points!");
			return -1;
		}

		setFileName(moduleFileName);

		if (!entrypoints->Name) {
			 WLog_Print(logger_RemoteModule, WLOG_ERROR,
				"no ModuleName is set for module %s", moduleFileName.c_str());
			 return -1;
		}

		setName(std::string(entrypoints->Name));
		setVersion(entrypoints->Version);

		return 0;
	}

	RemoteModule::~RemoteModule() {
	}


	RDS_MODULE_COMMON* RemoteModule::newContext() {
		REMOTE_MODULE* remoteModule = (REMOTE_MODULE *)calloc(1, sizeof(REMOTE_MODULE));

		if (!remoteModule) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "calloc failed!");
			return NULL;
		}

		remoteModule->context = new RemoteModuleTransportContext();
		remoteModule->launcherStarted = false;

		return (RDS_MODULE_COMMON*) remoteModule;

	}

	void RemoteModule::freeContext(RDS_MODULE_COMMON *context) {
		REMOTE_MODULE* remoteModule = (REMOTE_MODULE*)context;
		std::string encodedRequest;
		ogon::module::ModuleExitRequest request;
		std::string moduleStartReturn;

		if (!remoteModule->launcherStarted) {
			goto exit;
		}

		if (!request.SerializeToString(&encodedRequest)) {
			// failed to serialize
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error serializing startModuleRequest");
			goto exit_out;
		}

		remoteModule->context->mCurrentCallID = getNextCallID();

		if (writepbRpc(*remoteModule->context, encodedRequest, remoteModule->context->mCurrentCallID,
					   ogon::module::ModuleExit, false, true )) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "write failed!");
			goto exit_out;
		}

		serveOneRemoteCall(remoteModule, NULL);

	exit_out:
		CloseHandle(remoteModule->context->mhRead);
		CloseHandle(remoteModule->context->mhWrite);
	exit:
		delete remoteModule->context;
		free(remoteModule);
	}


	size_t RemoteModule::getEnvLength(char *env) {
		char *cp = env;
		if (env == NULL)
			return 0;

		while (*cp || *(cp + 1))
		{
			cp ++;
		}
		return cp - env + 2;

	}

	std::string RemoteModule::start(RDS_MODULE_COMMON *context) {
		REMOTE_MODULE* remoteModule = (REMOTE_MODULE*)context;
		UINT error;

		if (!startLauncher(remoteModule)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "could not start launcher process!");
			return std::string();
		}

		std::string encodedRequest;
		ogon::module::ModuleStartRequest request;

		request.set_sessionid(context->sessionId);
		request.set_username(context->userName);
		request.set_userdomain(context->domain);
		request.set_baseconfigpath(context->baseConfigPath);
		request.set_envblock(context->envBlock, getEnvLength(context->envBlock));
		request.set_modulefilename(getModuleFile());
		request.set_remoteip(context->remoteIp);

		if (!request.SerializeToString(&encodedRequest)) {
			// failed to serialize
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error serializing startModuleRequest");
			return std::string();
		}

		remoteModule->context->mCurrentCallID = getNextCallID();

		if (writepbRpc(*remoteModule->context, encodedRequest, remoteModule->context->mCurrentCallID,
					   ogon::module::ModuleStart, false, true )) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "write failed!");
			return std::string();
		}

		std::string moduleStartReturn;

		error = serveOneRemoteCall(remoteModule, &moduleStartReturn);
		if (error == REMOTE_CLIENT_SUCCESS) {
			return moduleStartReturn;
		}

		return std::string();
	}

	int RemoteModule::stop(RDS_MODULE_COMMON *context) {
		ogon::module::ModuleStopRequest request;
		std::string encodedRequest;
		REMOTE_MODULE* remoteModule = (REMOTE_MODULE*)context;
		UINT error;

		if (!remoteModule->launcherStarted) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "launcher not started!");
			return REMOTE_CLIENT_ERROR;
		}

		if (!request.SerializeToString(&encodedRequest)) {
			// failed to serialize
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error serializing stopModuleRequest");
			return REMOTE_CLIENT_ERROR;
		}

		remoteModule->context->mCurrentCallID = getNextCallID();

		if (writepbRpc(*remoteModule->context, encodedRequest, remoteModule->context->mCurrentCallID,
					   ogon::module::ModuleStop, false, true )) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "write failed!");
			return REMOTE_CLIENT_ERROR;
		}

		int moduleStopReturn;

		error = serveOneRemoteCall(remoteModule, &moduleStopReturn);
		if (error == REMOTE_CLIENT_SUCCESS) {
			return moduleStopReturn;
		}

		return REMOTE_CLIENT_ERROR;
	}

	int RemoteModule::connect(RDS_MODULE_COMMON *context) {
		ogon::module::ModuleConnectRequest request;
		std::string encodedRequest;
		REMOTE_MODULE* remoteModule = (REMOTE_MODULE*)context;
		UINT error;

		if (!remoteModule->launcherStarted) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "launcher not started!");
			return REMOTE_CLIENT_ERROR;
		}

		if (!request.SerializeToString(&encodedRequest)) {
			// failed to serialize
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error serializing connectModuleRequest");
			return REMOTE_CLIENT_ERROR;
		}

		remoteModule->context->mCurrentCallID = getNextCallID();

		if (writepbRpc(*remoteModule->context, encodedRequest, remoteModule->context->mCurrentCallID,
					   ogon::module::ModuleConnect, false, true )) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "write failed!");
			return REMOTE_CLIENT_ERROR;
		}

		int moduleConnectReturn;

		error = serveOneRemoteCall(remoteModule, &moduleConnectReturn);
		if (error == REMOTE_CLIENT_SUCCESS) {
			return moduleConnectReturn;
		}

		return REMOTE_CLIENT_ERROR;
	}

	int RemoteModule::disconnect(RDS_MODULE_COMMON *context) {
		ogon::module::ModuleDisconnectRequest request;
		std::string encodedRequest;
		REMOTE_MODULE* remoteModule = (REMOTE_MODULE*)context;
		UINT error;

		if (!remoteModule->launcherStarted) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "launcher not started!");
			return REMOTE_CLIENT_ERROR;
		}

		if (!request.SerializeToString(&encodedRequest)) {
			// failed to serialize
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error serializing disconnectModuleRequest");
			return REMOTE_CLIENT_ERROR;
		}

		remoteModule->context->mCurrentCallID = getNextCallID();

		if (writepbRpc(*remoteModule->context, encodedRequest, remoteModule->context->mCurrentCallID,
					   ogon::module::ModuleDisconnect, false, true )) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "write failed!");
			return REMOTE_CLIENT_ERROR;
		}

		int moduleDisconnectReturn;

		error = serveOneRemoteCall(remoteModule, &moduleDisconnectReturn);
		if (error == REMOTE_CLIENT_SUCCESS) {
			return moduleDisconnectReturn;
		}

		return REMOTE_CLIENT_ERROR;
	}

	std::string RemoteModule::getWinstationName(RDS_MODULE_COMMON *context) {

		REMOTE_MODULE* remoteModule = (REMOTE_MODULE*)context;
		std::string encodedRequest;
		ogon::module::ModuleGetCustomInfoRequest request;

		if (!remoteModule->launcherStarted) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "launcher not started!");
			return getName() + ":";
		}

		if (!request.SerializeToString(&encodedRequest)) {
			// failed to serialize
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error serializing getCustomInfoModuleRequest");
			return NULL;
		}

		remoteModule->context->mCurrentCallID = getNextCallID();

		if (writepbRpc(*remoteModule->context, encodedRequest, remoteModule->context->mCurrentCallID,
					   ogon::module::ModuleGetCustomInfo, false, true )) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "write failed!");
			return NULL;
		}

		std::string moduleGetCustomInfoReturn;

		serveOneRemoteCall(remoteModule, &moduleGetCustomInfoReturn);

		if (moduleGetCustomInfoReturn.size() == 0) {
			return getName() + ":";
		}

		return getName() + ":" + moduleGetCustomInfoReturn;
	}

	BOOL RemoteModule::startLauncher(REMOTE_MODULE *context) {
		STARTUPINFO StartupInfo;
		PROCESS_INFORMATION ProcessInformation;
		HANDLE stdInEnd;
		HANDLE stdOutEnd;
		char lpCommandLine[BUF_SIZE];
		BOOL status = FALSE;
		RemoteModuleTransportContext *remotecontext = context->context;

		ZeroMemory(&StartupInfo, sizeof(STARTUPINFO));
		StartupInfo.cb = sizeof(STARTUPINFO);
		ZeroMemory(&ProcessInformation, sizeof(PROCESS_INFORMATION));

		std::string base;
		std::string debugfile;

		if (!CreatePipe(&remotecontext->mhRead, &stdOutEnd, NULL, 0)) {
			printf("Read pipe creation failed. error=%" PRIu32 "\n", GetLastError());
			return FALSE;
		}

		if (!CreatePipe(&stdInEnd, &remotecontext->mhWrite, NULL, 0)) {
			printf("Write pipe creation failed. error=%" PRIu32 "\n", GetLastError());
			goto error_out_read;
		}

		base.assign(context->commonModule.baseConfigPath);
		base = base + ".launcherdebugfile";

		if (APP_CONTEXT.getPropertyManager()->getPropertyString(context->commonModule.sessionId, base, debugfile) &&
			base.size()) {
			sprintf_s(lpCommandLine, BUF_SIZE, "%s %" PRIu32 " '%s'", getLauncherExecutable(context).c_str(),
					  context->commonModule.sessionId, debugfile.c_str());
		} else {
			sprintf_s(lpCommandLine, BUF_SIZE, "%s %" PRIu32 "", getLauncherExecutable(context).c_str(), context->commonModule.sessionId);
		}

		StartupInfo.hStdOutput = stdOutEnd;
		StartupInfo.hStdInput = stdInEnd;


		status = CreateProcess(NULL,
							   lpCommandLine,
							   NULL,
							   NULL,
							   FALSE,
							   0,
							   NULL,
							   NULL,
							   &StartupInfo,
							   &ProcessInformation);


		CloseHandle(ProcessInformation.hProcess);
		CloseHandle(ProcessInformation.hThread);
		CloseHandle(stdOutEnd);
		CloseHandle(stdInEnd);

		if (!status) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR, "problem starting launcher process (status %" PRId32 " - cmd %s)",
					   status, lpCommandLine);
			goto error_out_write;
		}

		WLog_Print(logger_RemoteModule, WLOG_TRACE, "started launcher process pid %" PRIu32 "", ProcessInformation.dwProcessId);

		remotecontext->mLauncherpid = ProcessInformation.dwProcessId;
		context->launcherStarted = true;

		APP_CONTEXT.getProcessMonitor()->addProcess(ProcessInformation.dwProcessId,
													context->commonModule.sessionId,
													TRUE, (RDS_MODULE_COMMON*)context);

		return status;
	error_out_write:
		CloseHandle(remotecontext->mhWrite);
		remotecontext->mhWrite = NULL;
	error_out_read:
		CloseHandle(remotecontext->mhRead);
		remotecontext->mhRead = NULL;
		return status;
	}

	BOOL RemoteModule::stopLauncher(REMOTE_MODULE *context) {
#ifdef WIN32
			return FALSE;
#else

#define CYCLE_TIME 100

		RemoteModuleTransportContext *remotecontext = context->context;
		pid_t pid;
		pid_t ppid;

		DWORD launcherpid = remotecontext->mLauncherpid;
		if (!launcherpid) {
			return FALSE;
		}

		if (launcherpid == GetCurrentProcessId()) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR, "called with my own process id %" PRIu32 "", launcherpid);
			return FALSE;
		}

		pid = (pid_t) launcherpid;

		if (!get_parent_pid(pid, &ppid) || ppid == 0) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR, "unable to retive parend pid of %" PRIu32 "", launcherpid);
			return FALSE;
		}

		/* Only kill the process if the session-manager is the parent */
		if (ppid == getpid()) {
			kill(pid, SIGKILL);
		}

		CloseHandle(context->context->mhRead);
		CloseHandle(context->context->mhWrite);
		context->context->mhRead = NULL;
		context->context->mhWrite = NULL;
		context->launcherStarted = false;
		return TRUE;

#endif /* WIN32 not defined */

	}

	UINT RemoteModule::processModuleStart(const std::string &payload, void* customData) {
		ogon::module::ModuleStartResponse response;

		if (!response.ParseFromString(payload)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error deserializing startModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		std::string *returnValue = (std::string *) customData;
		returnValue->assign(response.pipename());
		//WLog_Print(logger_RemoteModule, WLOG_TRACE, "got pipename %s", returnValue->c_str());
		return REMOTE_CLIENT_SUCCESS;
	}


	UINT RemoteModule::processModuleStop(const std::string &payload, void* customData) {
		ogon::module::ModuleStopResponse response;

		if (!response.ParseFromString(payload)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error deserializing stopModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		int *returnValue = (int *) customData;
		*returnValue = response.success();
		return REMOTE_CLIENT_SUCCESS;
	}

	UINT RemoteModule::processModuleGetCustomInfo(const std::string &payload, void* customData) {
		ogon::module::ModuleGetCustomInfoResponse response;

		if (!response.ParseFromString(payload)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error deserializing ModuleGetCustomInfoResponse");
			return REMOTE_CLIENT_ERROR;
		}

		std::string *returnValue = (std::string *) customData;
		returnValue->assign(response.info());
		return REMOTE_CLIENT_SUCCESS;
	}

	UINT RemoteModule::processModuleConnect(const std::string &payload, void* customData) {
		ogon::module::ModuleConnectResponse response;

		if (!response.ParseFromString(payload)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error deserializing connectModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		int *returnValue = (int *) customData;
		*returnValue = response.success();
		return REMOTE_CLIENT_SUCCESS;
	}

	UINT RemoteModule::processModuleDisconnect(const std::string &payload, void* customData) {
		ogon::module::ModuleDisconnectResponse response;

		if (!response.ParseFromString(payload)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error deserializing disconnectModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		int *returnValue = (int *) customData;
		*returnValue = response.success();
		return REMOTE_CLIENT_SUCCESS;
	}


	UINT RemoteModule::processPropertyNumber(RemoteModuleTransportContext &context, const UINT32 callID, const std::string &payload) {
		ogon::module::PropertyNumberRequest request;

		if (!request.ParseFromString(payload)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error deserializing PropertyNumberRequest");
			return REMOTE_CLIENT_ERROR;
		}

		UINT32 sessionID = request.sessionid();
		std::string path = request.path();
		long value;
		ogon::module::PropertyNumberResponse response;

		if (!APP_CONTEXT.getPropertyManager()->getPropertyNumber(sessionID, path, value)) {
			//WLog_Print(logger_RemoteModule, WLOG_TRACE , "could not get property %s", path.c_str());
			response.set_success(false);
		} else {
			//WLog_Print(logger_RemoteModule, WLOG_TRACE , "got property %s", path.c_str());
			response.set_success(true);
			response.set_value(value);
		}

		std::string encodedResponse;

		if (!response.SerializeToString(&encodedResponse)) {
			// failed to serialize
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error serializing PropertyNumberResponse");
			return REMOTE_CLIENT_ERROR;
		}

		if (writepbRpc(context, encodedResponse, callID, ogon::module::PropertyNumber, true, true )) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "write failed!");
			return REMOTE_CLIENT_ERROR;
		}

		return REMOTE_CLIENT_CONTINUE;
	}


	UINT RemoteModule::processPropertyString(RemoteModuleTransportContext &context, const UINT32 callID, const std::string &payload) {
		ogon::module::PropertyStringRequest request;

		if (!request.ParseFromString(payload)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error deserializing PropertyStringRequest");
			return REMOTE_CLIENT_ERROR;
		}

		UINT32 sessionID = request.sessionid();
		std::string path = request.path();
		std::string value;
		ogon::module::PropertyStringResponse response;

		if (!APP_CONTEXT.getPropertyManager()->getPropertyString(sessionID, path, value)) {
			//WLog_Print(logger_RemoteModule, WLOG_TRACE , "could not get property %s", path.c_str());
			response.set_success(false);
		} else {
			//WLog_Print(logger_RemoteModule, WLOG_TRACE , "got property %s", path.c_str());
			response.set_success(true);
			response.set_value(value);
		}

		std::string encodedResponse;

		if (!response.SerializeToString(&encodedResponse)) {
			// failed to serialize
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error serializing PropertyStringResponse");
			return REMOTE_CLIENT_ERROR;
		}

		if (writepbRpc(context, encodedResponse, callID, ogon::module::PropertyString, true, true )) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "write failed!");
			return REMOTE_CLIENT_ERROR;
		}

		return REMOTE_CLIENT_CONTINUE;
	}


	UINT RemoteModule::processPropertyBool(RemoteModuleTransportContext &context, const UINT32 callID, const std::string &payload) {

		ogon::module::PropertyBoolRequest request;

		if (!request.ParseFromString(payload)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error deserializing PropertyBoolRequest");
			return REMOTE_CLIENT_ERROR;
		}

		UINT32 sessionID = request.sessionid();
		std::string path = request.path();
		bool value;
		ogon::module::PropertyBoolResponse response;

		if (!APP_CONTEXT.getPropertyManager()->getPropertyBool(sessionID, path, value)) {
			//WLog_Print(logger_RemoteModule, WLOG_TRACE , "could not get property %s", path.c_str());
			response.set_success(false);
		} else {
			//WLog_Print(logger_RemoteModule, WLOG_TRACE , "got property %s", path.c_str());
			response.set_success(true);
			response.set_value(value);
		}

		std::string encodedResponse;

		if (!response.SerializeToString(&encodedResponse)) {
			// failed to serialize
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error serializing PropertyBoolResponse");
			return REMOTE_CLIENT_ERROR;
		}

		if (writepbRpc(context, encodedResponse, callID, ogon::module::PropertyBool, true, true )) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "write failed!");
			return REMOTE_CLIENT_ERROR;
		}

		return REMOTE_CLIENT_CONTINUE;
	}

	UINT RemoteModule::processModuleExit(const std::string &payload, void* customData) {
		ogon::module::ModuleExitResponse response;
		OGON_UNUSED(customData);

		if (!response.ParseFromString(payload)) {
			WLog_Print(logger_RemoteModule, WLOG_ERROR , "error deserializing exitModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}
		return REMOTE_CLIENT_SUCCESS;
	}

	UINT RemoteModule::processCall(RemoteModuleTransportContext &context, const UINT32 callID, const UINT32 callType,
							 const bool isResponse, const std::string &payload, void* customData) {

		UINT32 currentCallID = context.mCurrentCallID;

		switch (callType)
		{
			case ogon::module::ModuleStart :
				if (!isResponse) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "ModuleStart does only expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				if (currentCallID != callID) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "wrong answer Id! Expected %" PRIu32 ", but got %" PRIu32 "", currentCallID, callID);
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleStart(payload, customData);

			case ogon::module::ModuleStop :
				if (!isResponse) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "ModuleStop does only expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				if (currentCallID != callID) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "wrong answer Id! Expected %" PRIu32 ", but got %" PRIu32 "", currentCallID, callID);
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleStop(payload, customData);

			case ogon::module::ModuleGetCustomInfo :
				if (!isResponse) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "ModuleGetCustomInfo does only expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				if (currentCallID != callID) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "wrong answer Id! Expected %" PRIu32 ", but got %" PRIu32 "", currentCallID, callID);
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleGetCustomInfo(payload, customData);

			case ogon::module::ModuleConnect :
				if (!isResponse) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "ModuleConnect does only expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				if (currentCallID != callID) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "wrong answer Id! Expected %" PRIu32 ", but got %" PRIu32 "", currentCallID, callID);
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleConnect(payload, customData);

			case ogon::module::ModuleDisconnect :
				if (!isResponse) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "ModuleDisconnect does only expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				if (currentCallID != callID) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "wrong answer Id! Expected %" PRIu32 ", but got %" PRIu32 "", currentCallID, callID);
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleDisconnect(payload, customData);

			case ogon::module::PropertyBool :
				if (isResponse) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "PropertyBool does not expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processPropertyBool(context, callID, payload);

			case ogon::module::PropertyNumber :
				if (isResponse) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "PropertyNumber does not expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processPropertyNumber(context, callID, payload);

			case ogon::module::PropertyString :
				if (isResponse) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "PropertyString does not expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processPropertyString(context, callID, payload);
			case ogon::module::ModuleExit :
				if (!isResponse) {
					WLog_Print(logger_RemoteModule, WLOG_ERROR , "PropertyString does only expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleExit(payload, customData);
			default:
				WLog_Print(logger_RemoteModule, WLOG_ERROR , "Unkown ID %" PRIu32 " received!", callType);
				break;
		}
		return REMOTE_CLIENT_ERROR;
	}


	UINT RemoteModule::serveOneRemoteCall(REMOTE_MODULE *context, void *customData) {
		UINT result = serveOneCall(*(context->context), customData, REMOTE_TIMEOUT);

		if (result == REMOTE_CLIENT_ERROR_TIMEOUT) {
			// stop launcher process
			WLog_Print(logger_RemoteModule, WLOG_TRACE, "stopping launcher %" PRIu32 "", context->context->mLauncherpid);

			if (!stopLauncher(context)) {
				WLog_Print(logger_RemoteModule, WLOG_ERROR , "stopLauncher failed!");
			}
		}

		return result;
	}

	std::string RemoteModule::getLauncherExecutable(REMOTE_MODULE *context) {
		std::string base;
		std::string executable;
		base.assign(context->commonModule.baseConfigPath);
		base = base + ".launcherexecutable";

		if (APP_CONTEXT.getPropertyManager()->getPropertyString(context->commonModule.sessionId, base, executable)) {
			return executable;
		}
		return REMOTE_LAUNCHER_PROCESS;
	}

} /*module*/ } /*sessionmanager*/ } /*ogon*/
