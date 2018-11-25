/**
 * ogon - Free Remote Desktop Services
 * Backend Process Launcher
 * Module Class for handling communication with session-manager
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

#include "ModuleCommunication.h"
#include <winpr/wlog.h>
#include <Module.pb.h>
#include <winpr/sspicli.h>
#include <winpr/library.h>
#include <winpr/string.h>
#include "../common/global.h"
#include <unistd.h>

namespace ogon { namespace launcher {

	static wLog *logger_ModuleCommunication = WLog_Get("ogon.launcher.modulecommunication");


	ModuleCommunication::ModuleCommunication(pgetPropertyBool getPropBool, pgetPropertyNumber getPropNumber,
	 	pgetPropertyString getPropString, pAddMonitoringProcess addMonitoring, pRemoveMonitoringProcess removeMonitoring,
 		pSignalStop signalStop) :
			mGetPropertyBool(getPropBool), mGetPropertyNumber(getPropNumber), mGetPropertyString(getPropString),
			mAddMonitoringProcess(addMonitoring), mRemoveMonitoringProcess(removeMonitoring), mStop(signalStop),
			mSessionId(0), mSessionPID(0), mUserToken(NULL), mSessionStarted(false),
			mModuleLib(NULL), mModuleContext(NULL), mStartSystemSession(false) {

		memset(&mEntrypoints, 0, sizeof(RDS_MODULE_ENTRY_POINTS));
	}

	ModuleCommunication::~ModuleCommunication() {
		if (mEntrypoints.Free) {
			if (mModuleContext) {
				mEntrypoints.Free(mModuleContext);
				mModuleContext = NULL;
			}
			mEntrypoints.Destroy();
			memset(&mEntrypoints, 0, sizeof(RDS_MODULE_ENTRY_POINTS));
		}
		if (mModuleLib) {
			FreeLibrary(mModuleLib);
			mModuleLib = NULL;
		}
		CloseHandle(mUserToken);
	}

	UINT ModuleCommunication::processCall(RemoteModuleTransportContext &context, const UINT32 callID,
										  const UINT32 callType, const bool isResponse, const std::string &payload,
										  void *customData) {

		OGON_UNUSED(context);

		switch (callType)
		{
			case ogon::module::ModuleStart :
				if (isResponse) {
					WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "ModuleStart does not expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleStart(payload, callID);

			case ogon::module::ModuleStop :
				if (isResponse) {
					WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "ModuleStop does not expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleStop(payload, callID);

			case ogon::module::ModuleGetCustomInfo :
				if (isResponse) {
					WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "ModuleGetCustomInfo does only expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleGetCustomInfo(payload, callID);

			case ogon::module::ModuleConnect :
				if (isResponse) {
					WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "ModuleConnect does not expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleConnect(payload, callID);

			case ogon::module::ModuleDisconnect :
				if (isResponse) {
					WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "ModuleDisconnect does not expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleDisconnect(payload, callID);

			case ogon::module::PropertyBool :
				if (!isResponse) {
					WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "PropertyBool does not expect request!");
					return REMOTE_CLIENT_ERROR;
				}
				return processGetPropertyBool(payload, customData);

			case ogon::module::PropertyNumber :
				if (!isResponse) {
					WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "PropertyNumber does not expect request!");
					return REMOTE_CLIENT_ERROR;
				}
				return processGetPropertyNumber(payload, customData);

			case ogon::module::PropertyString :
				if (!isResponse) {
					WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "PropertyString does not expect request!");
					return REMOTE_CLIENT_ERROR;
				}
				return processGetPropertyString(payload, customData);

			case ogon::module::ModuleExit :
				if (isResponse) {
					WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "ModuleExit does not expect answer!");
					return REMOTE_CLIENT_ERROR;
				}
				return processModuleExit(payload, callID);

			default:
				WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "Unkown ID %" PRIu32 " received!", callType);
				break;
		}

		return REMOTE_CLIENT_ERROR;
	}

	UINT ModuleCommunication::doRead() {
		return serveOneCall(mContext, NULL);
	}

	void ModuleCommunication::initHandles(HANDLE readHandle, HANDLE writeHandle) {
		mContext.mhRead = readHandle;
		mContext.mhWrite = writeHandle;
	}

	UINT ModuleCommunication::generateUserToken() {

		if (mUserName.length() == 0) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "failed, no username!");
			return REMOTE_CLIENT_ERROR;
		}

		if (mUserToken) {
			CloseHandle(mUserToken);
			mUserToken = NULL;
		}

		return LogonUserA(mUserName.c_str(), mDomain.c_str(), NULL, LOGON32_LOGON_INTERACTIVE,
						  LOGON32_PROVIDER_DEFAULT, &mUserToken) ? REMOTE_CLIENT_SUCCESS : REMOTE_CLIENT_ERROR;
	}

	UINT ModuleCommunication::loadModule() {
		pRdsModuleEntry entry_function;
		int ret;

		mModuleLib = LoadLibrary(mModuleFileName.c_str());

		if (!mModuleLib) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "LoadLibrary for file '%s' failed!", mModuleFileName.c_str());
			return REMOTE_CLIENT_ERROR;
		}

		// get the exports
		entry_function = (pRdsModuleEntry) GetProcAddress(mModuleLib, RDS_MODULE_ENTRY_POINT_NAME);

		if (!entry_function) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "LoadLibrary for file '%s' failed!", mModuleFileName.c_str());
			goto out_free;
		}

		mEntrypoints.config.getPropertyBool = mGetPropertyBool;
		mEntrypoints.config.getPropertyNumber = mGetPropertyNumber;
		mEntrypoints.config.getPropertyString = mGetPropertyString;
		mEntrypoints.status.addMonitoringProcess = mAddMonitoringProcess;
		mEntrypoints.status.removeMonitoringProcess = mRemoveMonitoringProcess;
		ret = entry_function(&mEntrypoints);

		if (ret != 0) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "EntryFunction failed with error %d", ret);
			goto out_free;
		}

		if ((ret = mEntrypoints.Init())) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "Init failed with error %d", ret);
			goto out_free;
		}

		if (!(mModuleContext = mEntrypoints.New())) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "New failed");
			goto out_destoy;
		}

		return REMOTE_CLIENT_SUCCESS;

	out_destoy:
		mEntrypoints.Destroy();
		memset(&mEntrypoints, 0, sizeof(RDS_MODULE_ENTRY_POINTS));
	out_free:
		FreeLibrary(mModuleLib);
		mModuleLib = NULL;
		return REMOTE_CLIENT_ERROR;
	}

	UINT ModuleCommunication::processModuleStart(const std::string &payload, const UINT32 callID) {
		ogon::module::ModuleStartRequest request;
		ogon::module::ModuleStartResponse response;
		UINT error = REMOTE_CLIENT_SUCCESS;
		char *result;
		std::string encodedResponse;
		std::string strResult;

		if (!request.ParseFromString(payload)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "error deserializing startModuleRequest");
			return REMOTE_CLIENT_ERROR;
		}

		mSessionId = request.sessionid();
		mUserName = request.username();
		mDomain = request.userdomain();
		mBaseConfigPath = request.baseconfigpath();
		mEnv = request.envblock();
		mModuleFileName = request.modulefilename();
		mRemoteIp = request.remoteip();

		if (mUserName.length()) {
			if ((error = generateUserToken())) {
				WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "generateUserToken failed!");
				return error;
			}
			mStartSystemSession = true;
		}

		if ((error = loadModule())) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "loadModule failed!");
			return error;
		}

		mModuleContext->sessionId = mSessionId;
		mModuleContext->userName = _strdup(mUserName.c_str());
		if (!mModuleContext->userName) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "_strdup failed!");
			goto out_context;
		}

		mModuleContext->domain = _strdup(mDomain.c_str());
		if (!mModuleContext->domain) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "_strdup failed!");
			goto out_context;
		}

		mModuleContext->baseConfigPath = _strdup(mBaseConfigPath.c_str());
		if (!mModuleContext->baseConfigPath) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "_strdup failed!");
			goto out_context;
		}

		mModuleContext->remoteIp = _strdup(mRemoteIp.c_str());
		if (!mModuleContext->remoteIp) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "_strdup failed!");
			goto out_context;
		}

		mModuleContext->userToken = mUserToken;

		mModuleContext->envBlock = (char *) malloc(mEnv.size());
		if (!mModuleContext->envBlock) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "malloc failed for envBlock !");
			goto out_context;
		}

		memcpy(mModuleContext->envBlock, mEnv.data(), mEnv.size());

		if (mStartSystemSession) {
			if (!session.init(mUserName, "ogon", mRemoteIp, mSessionPID, mSessionId) ||
				!session.startSession() ||
				!session.populateEnv(&mModuleContext->envBlock))
			{
				WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "error during session initialization");
				goto out_init;
			}

		}

		result = mEntrypoints.Start(mModuleContext);

		mSessionStarted = true;

		if (result) {
			strResult.assign(result);
		}

		response.set_pipename(strResult);

		if (!response.SerializeToString(&encodedResponse)) {
			// failed to serialize
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "error serializing startModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		if ((error = writepbRpc(mContext, encodedResponse, callID, ogon::module::ModuleStart, true, true))) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "write failed!");
		}

		return error;
	out_init:
		free(mModuleContext->envBlock);
		mModuleContext->envBlock = NULL;
	out_context:
		ogonModule(mModuleContext);
		return REMOTE_CLIENT_ERROR;
	}

	UINT ModuleCommunication::processModuleExit(const std::string &payload, const UINT32 callID) {
		ogon::module::ModuleExitRequest request;
		ogon::module::ModuleExitResponse response;
		UINT error = REMOTE_CLIENT_SUCCESS;
		std::string encodedResponse;

		if (!request.ParseFromString(payload)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error deserializing exitModuleRequest");
			return REMOTE_CLIENT_ERROR;
		}

		response.set_success(true);

		if (!response.SerializeToString(&encodedResponse)) {
			// failed to serialize
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error serializing exitModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		if ((error = writepbRpc(mContext, encodedResponse, callID, ogon::module::ModuleExit, true, true ))) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "write failed!");
		}

		mStop();

		return REMOTE_CLIENT_EXIT;
	}

	UINT ModuleCommunication::processModuleStop(const std::string &payload, const UINT32 callID) {
		ogon::module::ModuleStopRequest request;
		ogon::module::ModuleStopResponse response;
		UINT error = REMOTE_CLIENT_SUCCESS;
		int result;
		std::string encodedResponse;

		if (!request.ParseFromString(payload)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR, "error deserializing stopModuleRequest");
			return REMOTE_CLIENT_ERROR;
		}

		result = mEntrypoints.Stop(mModuleContext);

		if (mStartSystemSession) {
			session.stopSession();
		}

		mSessionStarted = false;

		response.set_success(result);

		if (!response.SerializeToString(&encodedResponse)) {
			// failed to serialize
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error serializing stopModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		if ((error = writepbRpc(mContext, encodedResponse, callID, ogon::module::ModuleStop, true, true ))) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "write failed!");
		}

		return error;
	}

	UINT ModuleCommunication::processModuleGetCustomInfo(const std::string &payload, const UINT32 callID) {
		ogon::module::ModuleGetCustomInfoRequest request;
		ogon::module::ModuleGetCustomInfoResponse response;
		UINT error = REMOTE_CLIENT_SUCCESS;
		char *result;
		std::string encodedResponse;

		if (!request.ParseFromString(payload)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error deserializing getCustomInfoModuleRequest");
			return REMOTE_CLIENT_ERROR;
		}

		result = mEntrypoints.getCustomInfo(mModuleContext);

		std::string test(result);

		response.set_info(test);

		if (!response.SerializeToString(&encodedResponse)) {
			// failed to serialize
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error serializing getCustomInfoModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		if ((error = writepbRpc(mContext, encodedResponse, callID, ogon::module::ModuleGetCustomInfo, true, true ))) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "write failed!");
		}

		return error;
	}

	UINT ModuleCommunication::processModuleConnect(const std::string &payload, const UINT32 callID) {
		ogon::module::ModuleConnectRequest request;
		ogon::module::ModuleConnectResponse response;
		UINT error = REMOTE_CLIENT_SUCCESS;
		int result;
		std::string encodedResponse;
		std::string strResult;

		if (!request.ParseFromString(payload)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error deserializing connectModuleRequest");
			return REMOTE_CLIENT_ERROR;
		}

		result = mEntrypoints.Connect(mModuleContext);

		response.set_success(result);

		if (!response.SerializeToString(&encodedResponse)) {
			// failed to serialize
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error serializing connectModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		if ((error = writepbRpc(mContext, encodedResponse, callID, ogon::module::ModuleConnect, true, true ))) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "write failed!");
		}

		return error;
	}

	UINT ModuleCommunication::processModuleDisconnect(const std::string &payload, const UINT32 callID) {
		ogon::module::ModuleDisconnectRequest request;
		ogon::module::ModuleDisconnectResponse response;
		UINT error = REMOTE_CLIENT_SUCCESS;
		int result;
		std::string encodedResponse;
		std::string strResult;

		if (!request.ParseFromString(payload)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error deserializing disconnectModuleRequest");
			return REMOTE_CLIENT_ERROR;
		}

		result = mEntrypoints.Disconnect(mModuleContext);

		response.set_success(result);

		if (!response.SerializeToString(&encodedResponse)) {
			// failed to serialize
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error serializing disconnectModuleResponse");
			return REMOTE_CLIENT_ERROR;
		}

		if ((error = writepbRpc(mContext, encodedResponse, callID, ogon::module::ModuleDisconnect, true, true ))) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "write failed!");
		}

		return error;
	}

	struct propertyBoolResult {
		bool success;
		bool value;
	};

	bool ModuleCommunication::getPropertyBool(UINT32 sessionID, const char *path, bool *value) {
		ogon::module::PropertyBoolRequest request;
		UINT error;

		request.set_sessionid(sessionID);
		request.set_path(path);

		std::string encodedRequest;

		if (!request.SerializeToString(&encodedRequest)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "SerializeToString failed!");
			return false;
		}

		error = writepbRpc(mContext,encodedRequest, getNextCallID(), ogon::module::PropertyBool, false, true );

		if (error) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "writepbRpc failed!");
			return false;
		}

		propertyBoolResult propertyBool;

		error = serveOneCall(mContext, &propertyBool);
		if (error == REMOTE_CLIENT_SUCCESS)
		{
			if (propertyBool.success) {
				*value = propertyBool.value;
				return true;
			}
			return false;
		}
		WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "serveOneCall failed!");
		return false;
	}


	struct propertyNumberResult {
		bool success;
		long value;
	};

	bool ModuleCommunication::getPropertyNumber(UINT32 sessionID, const char *path, long *value) {
		ogon::module::PropertyNumberRequest request;
		UINT error;

		request.set_sessionid(sessionID);
		request.set_path(path);

		std::string encodedRequest;

		if (!request.SerializeToString(&encodedRequest)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "SerializeToString failed!");
			return false;
		}

		error = writepbRpc(mContext,encodedRequest, getNextCallID(), ogon::module::PropertyNumber, false, true );

		if (error) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "writepbRpc failed!");
			return false;
		}

		propertyNumberResult propertyNumber;

		error = serveOneCall(mContext, &propertyNumber);
		if (error == REMOTE_CLIENT_SUCCESS)
		{
			if (propertyNumber.success) {
				*value = propertyNumber.value;
				return true;
			}
			return false;
		}
		WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "serveOneCall failed!");
		return false;
	}

	struct propertyStringResult {
		bool success;
		std::string value;
	};

	bool ModuleCommunication::getPropertyString(UINT32 sessionID, const char *path, char *value,
												unsigned int valueLength) {
		ogon::module::PropertyStringRequest request;
		UINT error;

		request.set_sessionid(sessionID);
		request.set_path(path);

		std::string encodedRequest;

		if (!request.SerializeToString(&encodedRequest)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "SerializeToString failed!");
			return false;
		}

		error = writepbRpc(mContext, encodedRequest, getNextCallID(), ogon::module::PropertyString, false, true );

		if (error) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "writepbRpc failed!");
			return false;
		}

		propertyStringResult propertyString;

		error = serveOneCall(mContext, &propertyString);

		if (error == REMOTE_CLIENT_SUCCESS)
		{
			if (propertyString.success) {
				strncpy(value, propertyString.value.c_str(), valueLength);
				return true;
			}
			return false;
		}
		WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "serveOneCall failed!");
		return false;
	}

	UINT ModuleCommunication::processGetPropertyBool(const std::string &payload, void *customData) {
		ogon::module::PropertyBoolResponse response;

		if (!response.ParseFromString(payload)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error deserializing PropertyBoolResponse");
			return REMOTE_CLIENT_ERROR;
		}

		propertyBoolResult *returnValue = (propertyBoolResult *) customData;
		if ((returnValue->success = response.success())) {
			returnValue->value = response.value();
		}

		return REMOTE_CLIENT_SUCCESS;
	}

	UINT ModuleCommunication::processGetPropertyNumber(const std::string &payload, void *customData) {
		ogon::module::PropertyNumberResponse response;

		if (!response.ParseFromString(payload)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error deserializing PropertyNumberResponse");
			return REMOTE_CLIENT_ERROR;
		}

		propertyNumberResult *returnValue = (propertyNumberResult *) customData;
		if ((returnValue->success = response.success())) {
			returnValue->value = response.value();
		}

		return REMOTE_CLIENT_SUCCESS;
	}

	UINT ModuleCommunication::processGetPropertyString(const std::string &payload, void *customData) {
		ogon::module::PropertyStringResponse response;

		if (!response.ParseFromString(payload)) {
			WLog_Print(logger_ModuleCommunication, WLOG_ERROR , "error deserializing PropertyStringResponse");
			return REMOTE_CLIENT_ERROR;
		}

		propertyStringResult *returnValue = (propertyStringResult *) customData;
		if ((returnValue->success = response.success())) {
			returnValue->value.assign(response.value());
		}

		return REMOTE_CLIENT_SUCCESS;
	}

	void ModuleCommunication::ogonModule(RDS_MODULE_COMMON *module) {
		free(module->userName);
		free(module->remoteIp);
		free(module->baseConfigPath);
		free(module->domain);
		free(module->envBlock);
		CloseHandle(module->userToken);
	}

	void ModuleCommunication::setSessionPID(pid_t sessionPID) {
		mSessionPID = sessionPID;
	}

	bool ModuleCommunication::stopModule() {
		int ret;
		if (!mSessionStarted || !mEntrypoints.Stop || !mModuleContext) {
			return false;
		}
		ret = mEntrypoints.Stop(mModuleContext);
		if (mStartSystemSession) {
			session.stopSession();
		}
		mSessionStarted = false;
		return (ret == 0);
	}

	} /* launcher */ } /* ogon */
