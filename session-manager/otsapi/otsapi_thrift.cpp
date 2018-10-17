/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * OTSAPI WTSAPI Stubs
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
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Library AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <errno.h>

#include <map>
#include <sstream>
#include <fstream>


#include <winpr/crt.h>
#include <winpr/pipe.h>
#include <winpr/wtsapi.h>
#include <winpr/synch.h>
#include <winpr/environment.h>
#include <winpr/handle.h>
#include <winpr/ssl.h>

#include <ogon/version.h>

#include <otsapi/otsapi.h>

#include <otsapi/ogon_ssl.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/protocol/TBinaryProtocol.h>

#include "../../common/global.h"


using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;

typedef struct {
	UINT32 sessionId;
	UINT32 instance;
	std::string virtualName;
} THandleInfo;

typedef std::map<HANDLE , THandleInfo> THandleInfoMap;
typedef std::pair<HANDLE, THandleInfo> THandleInfoPair;

typedef struct {
	boost::shared_ptr<TTransport> transport;
	boost::shared_ptr<ogon::otsapiClient> client;
	std::string authToken;
	bool authTokenScanned;
	THandleInfoMap handleInfoMap;
	CRITICAL_SECTION cSection;
	DWORD sessionId;
	std::string host;
	DWORD port;
} TSessionInfo;

typedef std::map<HANDLE, TSessionInfo *> TSessionMap;

TSessionInfo gCurrentServer;
CRITICAL_SECTION gCSection;
TSessionMap gSessionMap;

#define TOKEN_DIR_PREFIX "/tmp/ogon.session."
#define CHECK_AUTH_TOKEN(con) if(!con->authTokenScanned){ getAuthToken(con->authToken, con->sessionId); con->authTokenScanned=true;}
#define CHECK_CLIENT_CONNECTION(con) if(!con->transport || !con->transport->isOpen()) { connectClient(con, con->host.c_str(), con->port);}

void lib_load(void );
void lib_unload(void );

/** @brief */
class APIInitMgr {
public :
	APIInitMgr() {
		lib_load();
	}
	~APIInitMgr() {
		lib_unload();
	}
};

static APIInitMgr gInitMgr;

static BOOL connectClient(TSessionInfo *con, const std::string &host, DWORD port) {
	try {
		TSSLSocketFactory::setManualOpenSSLInitialization(true);
		boost::shared_ptr<TSSLSocketFactory> factory(new OgonSSLSocketFactory());
		factory->authenticate(false);

		boost::shared_ptr<TSSLSocket> socket = factory->createSocket(host, port);
		socket->setConnTimeout(5 * 1000);

		boost::shared_ptr<TTransport> transport(new TFramedTransport(socket));
		boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));

		boost::shared_ptr<ogon::otsapiClient> client(new ogon::otsapiClient(protocol));

		con->client = client;
		con->transport = transport;
		transport->open();
		ogon::TVersion smversion;
		ogon::TVersion ourversion;
		ourversion.VersionMajor = OGON_PROTOCOL_VERSION_MAJOR;
		ourversion.VersionMinor = OGON_PROTOCOL_VERSION_MINOR;
		client->getVersionInfo(smversion, ourversion);
		if (smversion.VersionMajor != OGON_PROTOCOL_VERSION_MAJOR) {
			fprintf(stderr, "%s: received protocol version info with %" PRId32 ".%" PRId32 " but own protocol version is %d.%d\n",
				__FUNCTION__, smversion.VersionMajor, smversion.VersionMinor,
				OGON_PROTOCOL_VERSION_MAJOR, OGON_PROTOCOL_VERSION_MINOR);
			return FALSE;
		}
		return TRUE;
	} catch (const TException &tx) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		return FALSE;
	} catch (...) {
		fprintf(stderr, "%s: unexpected exception\n", __FUNCTION__);
		return FALSE;
	}
}

static int getAuthToken(std::string &authToken, DWORD &sessionId) {
	char* sid;

	if (!(sid = getenv("OGON_SID"))) {
		return -1;
	}

	std::string filename(TOKEN_DIR_PREFIX);
	filename += sid;

	sessionId = atoi(sid);

	std::ifstream tokenfile (filename.c_str());
	if (!tokenfile.is_open()) {
		return -1;
	}

	getline(tokenfile, authToken);
	tokenfile.close();
	return 0;
}

static UINT32 getSessionId(TSessionInfo *con, UINT32 sessionId) {
	if (sessionId == (UINT32)WTS_CURRENT_SESSION){
		if (con != NULL) {
			return con->sessionId;
		}
	}
	return sessionId;
}

static TSessionInfo *getSessionInfo(HANDLE serverHandle) {

	if (serverHandle == WTS_CURRENT_SERVER_HANDLE) {
		return &gCurrentServer;
	}

	TSessionInfo *returnValue = NULL;

	EnterCriticalSection(&gCSection);
	TSessionMap::iterator it = gSessionMap.find(serverHandle);
	if (it != gSessionMap.end()) {
		returnValue = it->second;
	}
	LeaveCriticalSection(&gCSection);

	return returnValue;
}

static BOOL initSessionInfo(TSessionInfo *info) {
	if (info == NULL) {
		return FALSE;
	}
	info->authToken = "";
	info->authTokenScanned = false;
	info->sessionId = 0;
	if (!InitializeCriticalSectionAndSpinCount(&info->cSection, 0x00000400))
	{
		fprintf(stderr, "%s: failed to initialize critical section", __FUNCTION__);
		return FALSE;
	}
  	return TRUE;
}

static TSessionInfo *newSessionInfo(void) {
	TSessionInfo *info = new TSessionInfo();
	if(info != NULL) {
		if (!initSessionInfo(info)) {
			delete(info);
			info = NULL;
		}
	}
	return info;
}

static BOOL freeSessionInfo(TSessionInfo *info) {
	if (info == NULL) {
		return FALSE;
	}
	DeleteCriticalSection(&info->cSection);
	delete(info);
	return TRUE;
}

void lib_unload(void) {
	EnterCriticalSection(&gCSection);

	TSessionMap::iterator sessionIter;

	for (sessionIter = gSessionMap.begin(); sessionIter != gSessionMap.end(); ++sessionIter) {
		THandleInfoMap::iterator iter;
		TSessionInfo *sessionInfo = sessionIter->second;
		for (iter = sessionInfo->handleInfoMap.begin(); iter != sessionInfo->handleInfoMap.end(); ++iter) {
			THandleInfo info = iter->second;
			sessionInfo->client->virtualChannelClose(sessionInfo->authToken,
				getSessionId(sessionInfo, info.sessionId),
				info.virtualName, info.instance);
			sessionInfo->transport->close();
		}
		WTSCloseServer((HANDLE)sessionInfo);
		freeSessionInfo(sessionInfo);
	}

	if (gCurrentServer.transport) {
		gCurrentServer.transport->close();
	}
	LeaveCriticalSection(&gCSection);

	DeleteCriticalSection(&gCSection);
}

void lib_load(void) {
	if (!InitializeCriticalSectionAndSpinCount(&gCSection, 0x00000400))
	{
		fprintf(stderr, "%s: failed to initialize critical section", __FUNCTION__);
		throw std::bad_alloc();
	}
	initSessionInfo(&gCurrentServer);
	gCurrentServer.host = "localhost";
	gCurrentServer.port = 9091;
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
}

HANDLE connect2Pipe(const std::string &pipeName) {
	HANDLE hNamedPipe;

	if (!WaitNamedPipeA(pipeName.c_str(), 5000)) {
		fprintf(stderr, "%s: waitNamedPipe(%s) failure\n", __FUNCTION__, pipeName.c_str());
		return INVALID_HANDLE_VALUE;
	}

	hNamedPipe = CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
			OPEN_EXISTING, 0, NULL);

	if ((!hNamedPipe) || (hNamedPipe == INVALID_HANDLE_VALUE)) {
		fprintf(stderr, "%s: failed to create named pipe %s\n", __FUNCTION__, pipeName.c_str());
		return INVALID_HANDLE_VALUE;
	}
	return hNamedPipe;
}


/**
 * WTSAPI Stubs
 */


BOOL WINAPI ogon_WTSStartRemoteControlSessionExA(LPSTR pTargetServerName,
		ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers, DWORD flags) {

	OGON_UNUSED(pTargetServerName);
	TSessionInfo *currentCon;
	BOOL bSuccess;

	if (TargetLogonId == (UINT32)WTS_CURRENT_SESSION) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	currentCon = getSessionInfo(WTS_CURRENT_SERVER_HANDLE);
	if (currentCon == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	CHECK_AUTH_TOKEN(currentCon);
	CHECK_CLIENT_CONNECTION(currentCon);

	try {
		bSuccess = currentCon->client->startRemoteControlSession(currentCon->authToken, currentCon->sessionId,
				TargetLogonId, HotkeyVk, HotkeyModifiers, flags);
	} catch (const TException &tx) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	} catch (...) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	}

	if (bSuccess) {
		SetLastError(ERROR_SUCCESS);
	} else {
		SetLastError(ERROR_GEN_FAILURE);
	}

	return bSuccess;
}


BOOL WINAPI ogon_WTSStartRemoteControlSessionExW(LPWSTR pTargetServerName,
	ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers, DWORD flags) {
	char converted[256];

	int result = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) pTargetServerName,
			wcslen((const wchar_t *)pTargetServerName) + 1, converted, sizeof(converted), NULL, NULL);
	if (result == sizeof(converted)) {
		converted[result-1] = 0;
	} else {
		converted[result] = 0;
	}
	return ogon_WTSStartRemoteControlSessionExA(converted, TargetLogonId, HotkeyVk, HotkeyModifiers, flags);
}

BOOL WINAPI ogon_WTSStartRemoteControlSessionW(LPWSTR pTargetServerName,
	ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers) {
	return ogon_WTSStartRemoteControlSessionExW(pTargetServerName, TargetLogonId, HotkeyVk, HotkeyModifiers, 0);
}

BOOL WINAPI ogon_WTSStartRemoteControlSessionA(LPSTR pTargetServerName,
	ULONG TargetLogonId, BYTE HotkeyVk, USHORT HotkeyModifiers) {
	return ogon_WTSStartRemoteControlSessionExA(pTargetServerName, TargetLogonId, HotkeyVk, HotkeyModifiers, 0);
}

BOOL WINAPI ogon_WTSStopRemoteControlSession(ULONG LogonId) {
	TSessionInfo *currentCon;
	BOOL bSuccess;

	currentCon = getSessionInfo(WTS_CURRENT_SERVER_HANDLE);
	if (currentCon == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	CHECK_AUTH_TOKEN(currentCon);
	CHECK_CLIENT_CONNECTION(currentCon);

	try {
		bSuccess = currentCon->client->stopRemoteControlSession(currentCon->authToken, currentCon->sessionId, LogonId);
	} catch (const TException &tx) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	} catch (...) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	}

	if (bSuccess) {
		SetLastError(ERROR_SUCCESS);
	} else {
		SetLastError(ERROR_GEN_FAILURE);
	}

	return bSuccess;
}

BOOL WINAPI ogon_WTSConnectSessionW(ULONG LogonId, ULONG TargetLogonId,
	PWSTR pPassword, BOOL bWait) {

	OGON_UNUSED(LogonId);
	OGON_UNUSED(TargetLogonId);
	OGON_UNUSED(pPassword);
	OGON_UNUSED(bWait);
	return FALSE;
}

BOOL WINAPI ogon_WTSConnectSessionA(ULONG LogonId, ULONG TargetLogonId,
	PSTR pPassword, BOOL bWait) {

	OGON_UNUSED(LogonId);
	OGON_UNUSED(TargetLogonId);
	OGON_UNUSED(pPassword);
	OGON_UNUSED(bWait);
	return FALSE;
}

BOOL WINAPI ogon_WTSEnumerateServersW(LPWSTR pDomainName, DWORD Reserved,
	DWORD Version, PWTS_SERVER_INFOW *ppServerInfo, DWORD *pCount) {

	OGON_UNUSED(pDomainName);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(Version);
	OGON_UNUSED(ppServerInfo);
	OGON_UNUSED(pCount);
	return FALSE;
}

BOOL WINAPI ogon_WTSEnumerateServersA(LPSTR pDomainName, DWORD Reserved,
	DWORD Version, PWTS_SERVER_INFOA *ppServerInfo, DWORD *pCount) {

	OGON_UNUSED(pDomainName);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(Version);
	OGON_UNUSED(ppServerInfo);
	OGON_UNUSED(pCount);
	return FALSE;
}

HANDLE WINAPI ogon_WTSOpenServerA(LPSTR pServerName) {
	TSessionInfo *sessionInfo = newSessionInfo();
	if (sessionInfo == NULL) {
		SetLastError(ERROR_OUTOFMEMORY);
		return INVALID_HANDLE_VALUE;
	}

	if (connectClient(sessionInfo, pServerName, 9091)) {
		EnterCriticalSection(&gCSection);
		sessionInfo->host.assign(pServerName);
		sessionInfo->port = 9091;
		gSessionMap[(HANDLE)sessionInfo] = sessionInfo;
		LeaveCriticalSection(&gCSection);
		SetLastError(ERROR_SUCCESS);
		return (HANDLE)sessionInfo;
	}

	SetLastError(ERROR_INTERNAL_ERROR);
	freeSessionInfo(sessionInfo);
	return INVALID_HANDLE_VALUE;
}

HANDLE WINAPI ogon_WTSOpenServerW(LPWSTR pServerName) {
	char converted[256];

	int result = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR) pServerName,
			wcslen((const wchar_t *)pServerName) + 1, converted, sizeof(converted), NULL, NULL);
	if (result == sizeof(converted)) {
		converted[result-1] = 0;
	} else {
		converted[result] = 0;
	}

	return ogon_WTSOpenServerA(converted);
}


HANDLE WINAPI ogon_WTSOpenServerExW(LPWSTR pServerName) {
	return ogon_WTSOpenServerW(pServerName);
}

HANDLE WINAPI ogon_WTSOpenServerExA(LPSTR pServerName) {
	return ogon_WTSOpenServerA(pServerName);
}

void WINAPI ogon_WTSCloseServer(HANDLE hServer) {
	if (hServer == WTS_CURRENT_SERVER_HANDLE) {
		return;
	}

	TSessionInfo *returnValue = NULL;

	EnterCriticalSection(&gCSection);
	if (gSessionMap.find(hServer) != gSessionMap.end()) {
		returnValue = gSessionMap[hServer];
		gSessionMap.erase(hServer);
	}
	LeaveCriticalSection(&gCSection);

	if (!returnValue) {
		freeSessionInfo(returnValue);
	}
}

BOOL WINAPI ogon_WTSEnumerateSessionsA(HANDLE hServer, DWORD Reserved,
	DWORD Version, PWTS_SESSION_INFOA *ppSessionInfo, DWORD *pCount) {

	OGON_UNUSED(Reserved);

	ogon::TReturnEnumerateSession result;
	PWTS_SESSION_INFOA pSessionInfoA = NULL;
	TSessionInfo *currentCon;

	/* Check parameters. */
	if ((Version != 1) || (ppSessionInfo == NULL) || (pCount == NULL)) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	currentCon = getSessionInfo(hServer);
	if (currentCon == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	CHECK_AUTH_TOKEN(currentCon);
	CHECK_CLIENT_CONNECTION(currentCon);

	try {
		currentCon->client->enumerateSessions(result, currentCon->authToken, Version);
		if (!result.returnValue){
			SetLastError(ERROR_INTERNAL_ERROR);
			return FALSE;
		}
	} catch (const TException &tx) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	} catch (...) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	}

	SetLastError(ERROR_SUCCESS);

	DWORD count = (DWORD)result.sessionInfoList.size();
	if (!count) {
		*pCount = 0;
		*ppSessionInfo = NULL;
		return TRUE;
	}

	/* Allocate memory (including space for strings). */
	int cbExtra = 0;
	for (DWORD index = 0; index < count; index++) {
		ogon::TSessionInfo sessionInfo = result.sessionInfoList.at(index);
		cbExtra += sessionInfo.winStationName.length() + 1;
	}

	pSessionInfoA = (PWTS_SESSION_INFOA)calloc(1, sizeof(WTS_SESSION_INFOA) * count + cbExtra);
	if (!pSessionInfoA) {
		SetLastError(ERROR_OUTOFMEMORY);
		return FALSE;
	}

	LPBYTE pExtra = (LPBYTE)pSessionInfoA + (count * sizeof(WTS_SESSION_INFOA));

	for (DWORD index = 0; index < count; index++) {
		ogon::TSessionInfo sessionInfo = result.sessionInfoList.at(index);

		pSessionInfoA[index].SessionId = (DWORD)sessionInfo.sessionId;
		pSessionInfoA[index].pWinStationName = (LPSTR)pExtra;
		pSessionInfoA[index].State = (WTS_CONNECTSTATE_CLASS)sessionInfo.connectState;

		strcpy((LPSTR)pExtra, sessionInfo.winStationName.c_str());
		pExtra += sessionInfo.winStationName.length() + 1;
	}

	*ppSessionInfo = pSessionInfoA;
	*pCount = count;

	return TRUE;
}

BOOL WINAPI ogon_WTSEnumerateSessionsW(HANDLE hServer, DWORD Reserved,
	DWORD Version, PWTS_SESSION_INFOW *ppSessionInfo, DWORD *pCount) {

	PWTS_SESSION_INFOA pSessionInfoA;
	PWTS_SESSION_INFOW pSessionInfoW;
	DWORD count;

	if (!ogon_WTSEnumerateSessionsA(hServer, Reserved, Version, &pSessionInfoA, &count)) {
		return FALSE;
	}

	if (!count) {
		*ppSessionInfo = 0;
		*pCount = 0;
		return TRUE;
	}

	/* Allocate memory (including space for strings). */
	int cbExtra = 0;
	for (DWORD index = 0; index < count; index++) {
		cbExtra += MultiByteToWideChar(CP_ACP, 0, pSessionInfoA[index].pWinStationName, -1, NULL, 0);
	}

	pSessionInfoW = (PWTS_SESSION_INFOW)calloc(1, sizeof(WTS_SESSION_INFOW) * count + cbExtra);
	if (!pSessionInfoW) {
		SetLastError(ERROR_OUTOFMEMORY);
		WTSFreeMemory(pSessionInfoA);
		return FALSE;
	}

	LPBYTE pExtra = (LPBYTE)pSessionInfoW + (count * sizeof(WTS_SESSION_INFOW));

	/* Fill memory with session information. */
	for (DWORD index = 0; index < count; index++) {
		pSessionInfoW[index].SessionId = pSessionInfoA[index].SessionId;
		pSessionInfoW[index].State = pSessionInfoA[index].State;

		if (pSessionInfoA[index].pWinStationName) {
			pSessionInfoW[index].pWinStationName = (LPWSTR)pExtra;

			int size = MultiByteToWideChar(CP_ACP, 0, pSessionInfoA[index].pWinStationName,
				-1, pSessionInfoW[index].pWinStationName, cbExtra);
			pExtra += size;
			cbExtra -= size;
		}
	}
	*ppSessionInfo = pSessionInfoW;
	*pCount = count;

	return TRUE;
}

BOOL WINAPI ogon_WTSEnumerateSessionsExW(HANDLE hServer, DWORD *pLevel,
	DWORD Filter, PWTS_SESSION_INFO_1W *ppSessionInfo, DWORD *pCount) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pLevel);
	OGON_UNUSED(Filter);
	OGON_UNUSED(ppSessionInfo);
	OGON_UNUSED(pCount);

	return FALSE;
}

BOOL WINAPI ogon_WTSEnumerateSessionsExA(HANDLE hServer, DWORD *pLevel,
	DWORD Filter, PWTS_SESSION_INFO_1A *ppSessionInfo, DWORD *pCount) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pLevel);
	OGON_UNUSED(Filter);
	OGON_UNUSED(ppSessionInfo);
	OGON_UNUSED(pCount);

	return FALSE;
}

BOOL WINAPI ogon_WTSEnumerateProcessesW(HANDLE hServer, DWORD Reserved,
	DWORD Version, PWTS_PROCESS_INFOW *ppProcessInfo, DWORD *pCount) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(Version);
	OGON_UNUSED(ppProcessInfo);
	OGON_UNUSED(pCount);

	return FALSE;
}

BOOL WINAPI ogon_WTSEnumerateProcessesA(HANDLE hServer, DWORD Reserved,
	DWORD Version, PWTS_PROCESS_INFOA *ppProcessInfo, DWORD *pCount) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(Version);
	OGON_UNUSED(ppProcessInfo);
	OGON_UNUSED(pCount);

	return FALSE;
}

BOOL WINAPI ogon_WTSTerminateProcess(HANDLE hServer, DWORD ProcessId,
	DWORD ExitCode) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(ProcessId);
	OGON_UNUSED(ExitCode);

	return FALSE;
}

BOOL WINAPI ogon_WTSQuerySessionInformationA(HANDLE hServer, DWORD SessionId,
	WTS_INFO_CLASS wtsInfoClass, LPSTR* ppBuffer, DWORD* pBytesReturned) {

	ogon::TReturnQuerySessionInformation result;
	TSessionInfo *currentCon;

	if ((ppBuffer == NULL) || (pBytesReturned == NULL)) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	currentCon = getSessionInfo(hServer);
	if (currentCon == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	CHECK_AUTH_TOKEN(currentCon);
	CHECK_CLIENT_CONNECTION(currentCon);

	*ppBuffer = NULL;
	*pBytesReturned = 0;

	try {
		currentCon->client->querySessionInformation(result, currentCon->authToken,
			getSessionId(currentCon, SessionId), wtsInfoClass);
		if (!result.returnValue) {
			SetLastError(ERROR_INTERNAL_ERROR);
			return FALSE;
		}
	} catch (const TException &tx) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	} catch (...) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	}

	SetLastError(ERROR_SUCCESS);

	/* Return the result. */
	switch (wtsInfoClass) {
		case WTSSessionId:
		case WTSConnectState:
		case WTSClientBuildNumber:
		case WTSClientHardwareId: {
			ULONG *pulValue = (ULONG *)malloc(sizeof(ULONG));
			if (!pulValue) {
				SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}

			*pulValue = (ULONG)result.infoValue.int32Value;
			*ppBuffer = (LPSTR)pulValue;
			*pBytesReturned = sizeof(ULONG);
			return TRUE;
		}

		case WTSClientProductId:
		case WTSClientProtocolType: {
			USHORT *pusValue = (USHORT *)malloc(sizeof(USHORT));
			if (!pusValue) {
				SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}

			*pusValue = (USHORT)result.infoValue.int16Value;

			*ppBuffer = (LPSTR)pusValue;
			*pBytesReturned = sizeof(USHORT);
			return TRUE;
		}

		case WTSUserName:
		case WTSWinStationName:
		case WTSDomainName:
		case WTSClientName: {
			int size = result.infoValue.stringValue.length() + 1;
			LPSTR pszValue = (LPSTR)malloc(size);
			if (!pszValue) {
				SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}

			strncpy(pszValue, result.infoValue.stringValue.c_str(), size);

			*ppBuffer = pszValue;
			*pBytesReturned = size;
			return TRUE;
		}

		case WTSClientAddress: {
			unsigned char buf[sizeof(struct in6_addr)];
			int size = sizeof(WTS_CLIENT_ADDRESS);
			WTS_CLIENT_ADDRESS *pClientAddress = (WTS_CLIENT_ADDRESS *)calloc(size, 1);
			if (!pClientAddress) {
				SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}

			int ret = inet_pton(AF_INET, result.infoValue.stringValue.c_str(), buf);
			if (ret == 1) {
				// found ipv4 address
				strcpy((char *)pClientAddress->Address, result.infoValue.stringValue.c_str());
				pClientAddress->AddressFamily = AF_INET;
			} else {
				ret = inet_pton(AF_INET6, result.infoValue.stringValue.c_str(), buf);
				if (ret == 1) {
					//found ipv6 address
					memcpy((void *)pClientAddress->Address, buf, 16);
					pClientAddress->AddressFamily = AF_INET6;
				} else {
					free(pClientAddress);
					SetLastError(ERROR_UNSUPPORTED_TYPE);
					fprintf(stderr, "%s: unknown address kind\n", __FUNCTION__);
					return FALSE;
				}
			}

			*ppBuffer = (LPSTR)pClientAddress;
			*pBytesReturned = size;
			return TRUE;
		}

		case WTSClientDisplay: {
			int size = sizeof(WTS_CLIENT_DISPLAY);
			WTS_CLIENT_DISPLAY *pClientDisplay = (WTS_CLIENT_DISPLAY *)malloc(size);
			if (!pClientDisplay) {
				SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}

			pClientDisplay->HorizontalResolution = result.infoValue.displayValue.displayWidth;
			pClientDisplay->VerticalResolution = result.infoValue.displayValue.displayHeight;
			pClientDisplay->ColorDepth = result.infoValue.displayValue.colorDepth;

			*ppBuffer = (LPSTR)pClientDisplay;
			*pBytesReturned = size;
			return TRUE;
		}
		case WTSSessionInfo: {
			int size = sizeof(WTSINFOA);
			PWTSINFOA pWTSINFO = (PWTSINFOA)calloc(1, size);
			if (!pWTSINFO) {
				SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}
			pWTSINFO->State = (WTS_CONNECTSTATE_CLASS)result.infoValue.WTSINFO.State;
			pWTSINFO->SessionId = result.infoValue.WTSINFO.SessionId;
			strncpy(pWTSINFO->WinStationName, result.infoValue.WTSINFO.WinStationName.c_str(), WINSTATIONNAME_LENGTH - 2);
			pWTSINFO->WinStationName[WINSTATIONNAME_LENGTH - 1] = 0;
			strncpy(pWTSINFO->Domain, result.infoValue.WTSINFO.Domain.c_str(), DOMAIN_LENGTH - 2);
			pWTSINFO->Domain[DOMAIN_LENGTH - 1] = 0;
			strncpy(pWTSINFO->UserName, result.infoValue.WTSINFO.UserName.c_str(), USERNAME_LENGTH - 1);
			pWTSINFO->WinStationName[USERNAME_LENGTH] = 0;

			pWTSINFO->ConnectTime.QuadPart = result.infoValue.WTSINFO.ConnectTime;
			pWTSINFO->DisconnectTime.QuadPart = result.infoValue.WTSINFO.DisconnectTime;
			pWTSINFO->LastInputTime.QuadPart = result.infoValue.WTSINFO.LastInputTime;
			pWTSINFO->LogonTime.QuadPart = result.infoValue.WTSINFO.LogonTime;
			pWTSINFO->_CurrentTime.QuadPart = result.infoValue.WTSINFO.CurrentTime;

			*ppBuffer = (LPSTR)pWTSINFO;
			*pBytesReturned = size;
			return TRUE;
		}
		case WTSLogonTime: {
			int size = sizeof(LARGE_INTEGER);
			LARGE_INTEGER *largeint = (LARGE_INTEGER *)malloc(size);
			if (!largeint) {
				SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}
			largeint->QuadPart = result.infoValue.int64Value;
			*ppBuffer = (LPSTR)largeint;
			*pBytesReturned = size;
			return TRUE;
		}
		default:
			SetLastError(ERROR_UNSUPPORTED_TYPE);
			return FALSE;
	}
}

BOOL WINAPI ogon_WTSQuerySessionInformationW(HANDLE hServer, DWORD SessionId,
	WTS_INFO_CLASS WTSInfoClass, LPWSTR *ppBuffer, DWORD *pBytesReturned) {

	LPSTR pBuffer;
	DWORD cbBuffer;
	BOOL bSuccess;

	*ppBuffer = NULL;
	*pBytesReturned = 0;

	bSuccess = ogon_WTSQuerySessionInformationA(hServer, SessionId, WTSInfoClass, &pBuffer, &cbBuffer);
	if (!bSuccess) {
		return FALSE;
	}

	switch (WTSInfoClass) {
		case WTSUserName:
		case WTSWinStationName:
		case WTSDomainName:
		case WTSClientName: {
			int status = ConvertToUnicode(CP_UTF8, 0, pBuffer, -1, ppBuffer, 0);
			if (status > 0) {
				*pBytesReturned = status;
			} else {
				SetLastError(ERROR_INTERNAL_ERROR);
				bSuccess = FALSE;
			}
			WTSFreeMemory(pBuffer);
			break;
		}

		case WTSSessionInfo: {
			DWORD size = sizeof(WTSINFOW);
			PWTSINFOW pWTSINFOW = (PWTSINFOW)calloc(1, size);
			PWTSINFOA pWTSINFOA = (PWTSINFOA)pBuffer;
			if (!pWTSINFOW) {
				SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}
			pWTSINFOW->State = pWTSINFOA->State;
			pWTSINFOW->SessionId = pWTSINFOA->SessionId;
			pWTSINFOW->ConnectTime = pWTSINFOA->ConnectTime;
			pWTSINFOW->DisconnectTime = pWTSINFOA->DisconnectTime;
			pWTSINFOW->LastInputTime = pWTSINFOA->LastInputTime;
			pWTSINFOW->LogonTime = pWTSINFOA->LogonTime;
			MultiByteToWideChar(CP_UTF8, 0, pWTSINFOA->WinStationName, -1, pWTSINFOW->WinStationName, WINSTATIONNAME_LENGTH);
			MultiByteToWideChar(CP_UTF8, 0, pWTSINFOA->Domain, -1, pWTSINFOW->Domain, DOMAIN_LENGTH);
			MultiByteToWideChar(CP_UTF8, 0, pWTSINFOA->UserName, -1, pWTSINFOW->UserName, USERNAME_LENGTH + 1);

			*ppBuffer = (LPWSTR)pWTSINFOW;
			*pBytesReturned = size;
			WTSFreeMemory(pBuffer);
			break;
		}

		default:
			*ppBuffer = (LPWSTR)pBuffer;
			*pBytesReturned = cbBuffer;
			break;
	}

	return bSuccess;
}

BOOL WINAPI ogon_WTSQueryUserConfigW(LPWSTR pServerName, LPWSTR pUserName,
	WTS_CONFIG_CLASS WTSConfigClass, LPWSTR *ppBuffer, DWORD *pBytesReturned) {

	OGON_UNUSED(pServerName);
	OGON_UNUSED(pUserName);
	OGON_UNUSED(WTSConfigClass);
	OGON_UNUSED(ppBuffer);
	OGON_UNUSED(pBytesReturned);

	return FALSE;
}

BOOL WINAPI ogon_WTSQueryUserConfigA(LPSTR pServerName, LPSTR pUserName,
	WTS_CONFIG_CLASS WTSConfigClass, LPSTR *ppBuffer, DWORD *pBytesReturned) {

	OGON_UNUSED(pServerName);
	OGON_UNUSED(pUserName);
	OGON_UNUSED(WTSConfigClass);
	OGON_UNUSED(ppBuffer);
	OGON_UNUSED(pBytesReturned);

	return FALSE;
}

BOOL WINAPI ogon_WTSSetUserConfigW(LPWSTR pServerName, LPWSTR pUserName,
	WTS_CONFIG_CLASS WTSConfigClass, LPWSTR pBuffer, DWORD DataLength) {

	OGON_UNUSED(pServerName);
	OGON_UNUSED(pUserName);
	OGON_UNUSED(WTSConfigClass);
	OGON_UNUSED(pBuffer);
	OGON_UNUSED(DataLength);

	return FALSE;
}

BOOL WINAPI ogon_WTSSetUserConfigA(LPSTR pServerName, LPSTR pUserName,
	WTS_CONFIG_CLASS WTSConfigClass, LPSTR pBuffer, DWORD DataLength) {

	OGON_UNUSED(pServerName);
	OGON_UNUSED(pUserName);
	OGON_UNUSED(WTSConfigClass);
	OGON_UNUSED(pBuffer);
	OGON_UNUSED(DataLength);

	return FALSE;
}

BOOL WINAPI ogon_WTSSendMessageW(HANDLE hServer, DWORD SessionId,
	LPWSTR pTitle, DWORD TitleLength, LPWSTR pMessage, DWORD MessageLength,
	DWORD Style, DWORD Timeout, DWORD *pResponse, BOOL bWait) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(SessionId);
	OGON_UNUSED(pTitle);
	OGON_UNUSED(TitleLength);
	OGON_UNUSED(pMessage);
	OGON_UNUSED(MessageLength);
	OGON_UNUSED(Style);
	OGON_UNUSED(Timeout);
	OGON_UNUSED(pResponse);
	OGON_UNUSED(bWait);

	return FALSE;
}

BOOL WINAPI ogon_WTSSendMessageA(HANDLE hServer, DWORD SessionId, LPSTR pTitle,
	DWORD TitleLength, LPSTR pMessage, DWORD MessageLength, DWORD Style,
	DWORD Timeout, DWORD *pResponse, BOOL bWait) {

	OGON_UNUSED(TitleLength);
	DWORD result = 0;
	TSessionInfo *currentCon;

	if ((pMessage == NULL) || (MessageLength == 0)) {
		return FALSE;
	}

	std::string title(pTitle);
	std::string message(pMessage);

	currentCon = getSessionInfo(hServer);
	if (currentCon == NULL) {
		return FALSE;
	}

	CHECK_AUTH_TOKEN(currentCon);
	CHECK_CLIENT_CONNECTION(currentCon);

	try {
		result = currentCon->client->sendMessage(currentCon->authToken,
			getSessionId(currentCon, SessionId), title, message, Style, Timeout,
			bWait);
	} catch (const TException &tx) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		return FALSE;
	} catch (...) {
		return FALSE;
	}
	*pResponse = result;
	return TRUE;
}

BOOL WINAPI ogon_WTSDisconnectSession(HANDLE hServer, DWORD SessionId, BOOL bWait) {
	TSessionInfo *currentCon;

	currentCon = getSessionInfo(hServer);
	if (currentCon == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	CHECK_AUTH_TOKEN(currentCon);
	CHECK_CLIENT_CONNECTION(currentCon);
	SetLastError(ERROR_SUCCESS);

	try {
		bool result = currentCon->client->disconnectSession(currentCon->authToken,
			getSessionId(currentCon, SessionId), bWait);
		if (!result) {
			SetLastError(ERROR_INTERNAL_ERROR);
		}
		return result;
	} catch (const TException &tx) {
		SetLastError(ERROR_INTERNAL_ERROR);
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		return FALSE;
	} catch (...) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	}
}

BOOL WINAPI ogon_WTSLogoffSession(HANDLE hServer, DWORD SessionId, BOOL bWait) {
	TSessionInfo *currentCon;

	currentCon = getSessionInfo(hServer);
	if (currentCon == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	CHECK_AUTH_TOKEN(currentCon);
	CHECK_CLIENT_CONNECTION(currentCon);
	SetLastError(ERROR_SUCCESS);

	try {
		bool result = currentCon->client->logoffSession(currentCon->authToken,
			getSessionId(currentCon, SessionId), bWait);
		if (!result) {
			SetLastError(ERROR_INTERNAL_ERROR);
		}
		return result;
	} catch (const TException &tx) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	} catch (...) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	}
}

BOOL WINAPI ogon_WTSShutdownSystem(HANDLE hServer, DWORD ShutdownFlag) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(ShutdownFlag);

	return FALSE;
}

BOOL WINAPI ogon_WTSWaitSystemEvent(HANDLE hServer, DWORD EventMask,
	DWORD *pEventFlags) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(EventMask);
	OGON_UNUSED(pEventFlags);

	return FALSE;
}

HANDLE WINAPI ogon_WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags) {

	std::string virtualName(pVirtualName);
	if (!(flags & WTS_CHANNEL_OPTION_DYNAMIC) && virtualName.length() > 8) {
		/* static channels are limited to 8 char */
		SetLastError(ERROR_NOT_FOUND);
		return NULL;
	}
	TSessionInfo *currentCon = getSessionInfo(WTS_CURRENT_SERVER_HANDLE);
	CHECK_AUTH_TOKEN(currentCon);
	CHECK_CLIENT_CONNECTION(currentCon);

	ogon::TReturnVirtualChannelOpen result;

	try {
		currentCon->client->virtualChannelOpen(result, currentCon->authToken,
			getSessionId(currentCon, SessionId), virtualName,
			(flags & WTS_CHANNEL_OPTION_DYNAMIC) ? true : false, flags);
	} catch (TException &ex) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, ex.what());
		SetLastError(ERROR_INTERNAL_ERROR);
		return NULL;
	} catch (...) {
		fprintf(stderr, "%s: unhandled exception during call\n", __FUNCTION__);
		SetLastError(ERROR_INTERNAL_ERROR);
		return NULL;
	}

	if (result.pipeName.size() == 0) {
		SetLastError(ERROR_NOT_FOUND);
		return NULL;
	}

	HANDLE hNamedPipe = connect2Pipe(result.pipeName);
	if (hNamedPipe == INVALID_HANDLE_VALUE) {
		SetLastError(ERROR_NOT_FOUND);
		return NULL;
	}

	DWORD dwPipeMode = PIPE_NOWAIT;
	if (!SetNamedPipeHandleState(hNamedPipe, &dwPipeMode, NULL, NULL)) {
		CloseHandle(hNamedPipe);
		SetLastError(ERROR_NOT_FOUND);
		return NULL;
	}

	THandleInfo info;
	info.sessionId = getSessionId(currentCon, SessionId);
	info.instance = result.instance;
	info.virtualName = virtualName;


	EnterCriticalSection(&currentCon->cSection);
	currentCon->handleInfoMap[hNamedPipe] = info;
	LeaveCriticalSection(&currentCon->cSection);
	SetLastError(ERROR_SUCCESS);

	return hNamedPipe;
}

HANDLE WINAPI ogon_WTSVirtualChannelOpen(HANDLE hServer, DWORD SessionId,
	LPSTR pVirtualName) {

	if (hServer != WTS_CURRENT_SERVER_HANDLE) {
		SetLastError(ERROR_INVALID_HANDLE);
		return NULL;
	}
	return ogon_WTSVirtualChannelOpenEx(SessionId, pVirtualName, 0);
}

BOOL WINAPI ogon_WTSVirtualChannelClose(HANDLE hChannelHandle) {

	THandleInfo info;
	BOOL result = FALSE;

	TSessionInfo *currentCon = getSessionInfo(WTS_CURRENT_SERVER_HANDLE);

	EnterCriticalSection(&currentCon->cSection);
	THandleInfoMap::iterator it = currentCon->handleInfoMap.find(hChannelHandle);
	if (it == currentCon->handleInfoMap.end()) {
		LeaveCriticalSection(&currentCon->cSection);
		SetLastError(ERROR_INVALID_HANDLE);
		goto out;
	}

	info = it->second;
	currentCon->handleInfoMap.erase(it);
	LeaveCriticalSection(&currentCon->cSection);

	CHECK_AUTH_TOKEN(currentCon);
	CHECK_CLIENT_CONNECTION(currentCon);
	SetLastError(ERROR_SUCCESS);

	try {
		result = currentCon->client->virtualChannelClose(currentCon->authToken,
				getSessionId(currentCon, info.sessionId), info.virtualName, info.instance);
		if (!result) {
			SetLastError(ERROR_INTERNAL_ERROR);
		}
	} catch (const TException &tx) {
		SetLastError(ERROR_INTERNAL_ERROR);
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
	} catch (...) {
		SetLastError(ERROR_INTERNAL_ERROR);
	}

out:
	CloseHandle(hChannelHandle);
	return result;
}

BOOL WINAPI ogon_WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut,
		PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead) {

	if (ReadFile(hChannelHandle, Buffer, BufferSize, pBytesRead, NULL)) {
		return TRUE;
	}

	if (TimeOut == 0) {
		return FALSE;
	}

	DWORD error = GetLastError();
	if (error != ERROR_NO_DATA) {
		SetLastError(error);
		return FALSE;
	}

	DWORD result = WaitForSingleObject(hChannelHandle, TimeOut);
	switch (result) {
		case WAIT_OBJECT_0:
			return ReadFile(hChannelHandle, Buffer, BufferSize, pBytesRead, NULL);

		case WAIT_TIMEOUT:
		case WAIT_FAILED:
			SetLastError(ERROR_NO_DATA);
			break;
		default:
			fprintf(stderr, "%s: unexpected result %" PRIu32 "\n", __FUNCTION__, result);
			break;
	}

	return FALSE;
}

BOOL WINAPI ogon_WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer,
	ULONG Length, PULONG pBytesWritten) {

	ULONG written;
	ULONG writtenTotal = 0;
	BOOL success = TRUE;
	BOOL reentry = FALSE;

	char buffer[4];
	buffer[0] = 0xFF & Length;
	buffer[1] = ((0xFF << 8) & Length) >> 8;
	buffer[2] = ((0xFF << 16) & Length) >> 16;
	buffer[3] = ((0xFF << 24) & Length) >> 24;

	while (writtenTotal < 4) {
		// writting headers for ogon
		if (reentry) {
			Sleep(10);
		}
		reentry = TRUE;
		success = WriteFile(hChannelHandle, buffer + writtenTotal, 4 - writtenTotal, &written, NULL);
		if (!success) {
			*pBytesWritten = 0;
			return FALSE;
		}

		writtenTotal += written;
	}

	writtenTotal = 0;
	reentry = FALSE;
	while (writtenTotal < Length) {
		if (reentry) {
			Sleep(10);
		}
		reentry = TRUE;
		success = WriteFile(hChannelHandle, Buffer + writtenTotal, Length - writtenTotal, &written, NULL);
		if (!success) {
			*pBytesWritten = 4 + writtenTotal;
			return FALSE;
		}
		writtenTotal += written;
	}

	*pBytesWritten = writtenTotal;

	return success;
}

BOOL WINAPI ogon_WTSVirtualChannelPurgeInput(HANDLE hChannelHandle) {

	OGON_UNUSED(hChannelHandle);

	return TRUE;
}

BOOL WINAPI ogon_WTSVirtualChannelPurgeOutput(HANDLE hChannelHandle) {

	OGON_UNUSED(hChannelHandle);

	return TRUE;
}

BOOL WINAPI ogon_WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass,
											PVOID *ppBuffer, DWORD *pBytesReturned) {
	if (hChannelHandle == INVALID_HANDLE_VALUE) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	if ((ppBuffer == NULL) || (pBytesReturned == NULL)) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	switch (WtsVirtualClass) {
		case WTSVirtualClientData:
			return FALSE;
		case WTSVirtualFileHandle:
		case WTSVirtualEventHandle:
			/* In our case we can simply copy back the channel handle */
			if (!(*ppBuffer = (PVOID)malloc(sizeof(HANDLE)))) {
				SetLastError(ERROR_OUTOFMEMORY);
				return FALSE;
			}
			memcpy(*ppBuffer, &hChannelHandle, sizeof(HANDLE));
			*pBytesReturned = sizeof(HANDLE);
			break;
		default:
			SetLastError(ERROR_UNSUPPORTED_TYPE);
			return FALSE;
	}
	SetLastError(ERROR_SUCCESS);
	return TRUE;
}

VOID WINAPI ogon_WTSFreeMemory(PVOID pMemory) {

	if (pMemory) {
		free(pMemory);
	}
}

BOOL WINAPI ogon_WTSFreeMemoryExW(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory,
	ULONG NumberOfEntries) {

	OGON_UNUSED(WTSTypeClass);
	OGON_UNUSED(pMemory);
	OGON_UNUSED(NumberOfEntries);

	return FALSE;
}

BOOL WINAPI ogon_WTSFreeMemoryExA(WTS_TYPE_CLASS WTSTypeClass, PVOID pMemory,
	ULONG NumberOfEntries) {

	OGON_UNUSED(WTSTypeClass);
	OGON_UNUSED(pMemory);
	OGON_UNUSED(NumberOfEntries);

	return FALSE;
}

BOOL WINAPI ogon_WTSRegisterSessionNotification(HWND hWnd, DWORD dwFlags) {

	OGON_UNUSED(hWnd);
	OGON_UNUSED(dwFlags);

	return FALSE;
}

BOOL WINAPI ogon_WTSUnRegisterSessionNotification(HWND hWnd) {

	OGON_UNUSED(hWnd);

	return FALSE;
}

BOOL WINAPI ogon_WTSRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd,
	DWORD dwFlags) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(hWnd);
	OGON_UNUSED(dwFlags);

	return FALSE;
}

BOOL WINAPI ogon_WTSUnRegisterSessionNotificationEx(HANDLE hServer, HWND hWnd) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(hWnd);

	return FALSE;
}

BOOL WINAPI ogon_WTSQueryUserToken(ULONG SessionId, PHANDLE phToken) {

	OGON_UNUSED(SessionId);
	OGON_UNUSED(phToken);

	return FALSE;
}

BOOL WINAPI ogon_WTSEnumerateProcessesExW(HANDLE hServer, DWORD *pLevel,
	DWORD SessionId, LPWSTR *ppProcessInfo, DWORD *pCount) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pLevel);
	OGON_UNUSED(SessionId);
	OGON_UNUSED(ppProcessInfo);
	OGON_UNUSED(pCount);

	return FALSE;
}

BOOL WINAPI ogon_WTSEnumerateProcessesExA(HANDLE hServer, DWORD* pLevel,
	DWORD SessionId, LPSTR *ppProcessInfo, DWORD *pCount) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pLevel);
	OGON_UNUSED(SessionId);
	OGON_UNUSED(ppProcessInfo);
	OGON_UNUSED(pCount);

	return FALSE;
}

BOOL WINAPI ogon_WTSEnumerateListenersW(HANDLE hServer, PVOID pReserved,
	DWORD Reserved, PWTSLISTENERNAMEW pListeners, DWORD *pCount) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListeners);
	OGON_UNUSED(pCount);

	return FALSE;
}

BOOL WINAPI ogon_WTSEnumerateListenersA(HANDLE hServer, PVOID pReserved,
	DWORD Reserved, PWTSLISTENERNAMEA pListeners, DWORD *pCount) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListeners);
	OGON_UNUSED(pCount);

	return FALSE;
}

BOOL WINAPI ogon_WTSQueryListenerConfigW(HANDLE hServer, PVOID pReserved,
	DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListenerName);
	OGON_UNUSED(pBuffer);

	return FALSE;
}

BOOL WINAPI ogon_WTSQueryListenerConfigA(HANDLE hServer, PVOID pReserved,
	DWORD Reserved, LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListenerName);
	OGON_UNUSED(pBuffer);

	return FALSE;
}

BOOL WINAPI ogon_WTSCreateListenerW(HANDLE hServer, PVOID pReserved,
	DWORD Reserved, LPWSTR pListenerName, PWTSLISTENERCONFIGW pBuffer, DWORD flag) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListenerName);
	OGON_UNUSED(pBuffer);
	OGON_UNUSED(flag);

	return FALSE;
}

BOOL WINAPI ogon_WTSCreateListenerA(HANDLE hServer, PVOID pReserved,
	DWORD Reserved, LPSTR pListenerName, PWTSLISTENERCONFIGA pBuffer, DWORD flag) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListenerName);
	OGON_UNUSED(pBuffer);
	OGON_UNUSED(flag);

	return FALSE;
}

BOOL WINAPI ogon_WTSSetListenerSecurityW(HANDLE hServer, PVOID pReserved,
	DWORD Reserved, LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
	PSECURITY_DESCRIPTOR pSecurityDescriptor) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListenerName);
	OGON_UNUSED(SecurityInformation);
	OGON_UNUSED(pSecurityDescriptor);

	return FALSE;
}

BOOL WINAPI ogon_WTSSetListenerSecurityA(HANDLE hServer, PVOID pReserved,
	DWORD Reserved, LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
	PSECURITY_DESCRIPTOR pSecurityDescriptor) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListenerName);
	OGON_UNUSED(SecurityInformation);
	OGON_UNUSED(pSecurityDescriptor);

	return FALSE;
}

BOOL WINAPI ogon_WTSGetListenerSecurityW(HANDLE hServer, PVOID pReserved,
		DWORD Reserved, LPWSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
		PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListenerName);
	OGON_UNUSED(SecurityInformation);
	OGON_UNUSED(pSecurityDescriptor);
	OGON_UNUSED(nLength);
	OGON_UNUSED(lpnLengthNeeded);

	return FALSE;
}

BOOL WINAPI ogon_WTSGetListenerSecurityA(HANDLE hServer, PVOID pReserved,
	DWORD Reserved, LPSTR pListenerName, SECURITY_INFORMATION SecurityInformation,
	PSECURITY_DESCRIPTOR pSecurityDescriptor, DWORD nLength, LPDWORD lpnLengthNeeded) {

	OGON_UNUSED(hServer);
	OGON_UNUSED(pReserved);
	OGON_UNUSED(Reserved);
	OGON_UNUSED(pListenerName);
	OGON_UNUSED(SecurityInformation);
	OGON_UNUSED(pSecurityDescriptor);
	OGON_UNUSED(nLength);
	OGON_UNUSED(lpnLengthNeeded);

	return FALSE;
}

BOOL CDECL ogon_WTSEnableChildSessions(BOOL bEnable) {

	OGON_UNUSED(bEnable);

	return FALSE;
}

BOOL CDECL ogon_WTSIsChildSessionsEnabled(PBOOL pbEnabled) {

	OGON_UNUSED(pbEnabled);

	return FALSE;
}

BOOL CDECL ogon_WTSGetChildSessionId(PULONG pSessionId) {

	OGON_UNUSED(pSessionId);

	return FALSE;
}

DWORD WINAPI ogon_WTSGetActiveConsoleSessionId(void) {
	return 0xFFFFFFFF;
}

BOOL CDECL ogon_WTSLogoffUser(HANDLE hServer) {
	BOOL retVal = FALSE;
	TSessionInfo *currentCon = getSessionInfo(hServer);
	if (currentCon == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return retVal;
	}

	if (!currentCon->authTokenScanned) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return retVal;
	}

	CHECK_CLIENT_CONNECTION(currentCon);

	try {
		retVal = currentCon->client->logoffConnection(currentCon->authToken);
	} catch (const TException &tx) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	} catch (...) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	}

	currentCon->authToken = "";
	currentCon->authTokenScanned = false;
	currentCon->sessionId = 0;
	if (!retVal) {
		SetLastError(ERROR_INTERNAL_ERROR);
	} else {
		SetLastError(ERROR_SUCCESS);
	}
	return retVal;
}

BOOL CDECL ogon_WTSLogonUser(HANDLE hServer, LPCSTR username, LPCSTR password, LPCSTR domain) {

	if ((username == NULL) || ( password== NULL)) {
		SetLastError(ERROR_INVALID_DATA);
		return FALSE;
	}

	TSessionInfo *currentCon = getSessionInfo(hServer);
	if (currentCon == NULL) {
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	if (currentCon->authTokenScanned) {
		ogon_WTSLogoffUser(hServer);
	}

	CHECK_CLIENT_CONNECTION(currentCon);

	ogon::TReturnLogonConnection result;

	std::string stdusername(username);
	std::string stdpassword(password);
	std::string stddomain;

	if (domain != NULL) {
		stddomain.assign(domain);
	}

	try {
		currentCon->client->logonConnection(result, stdusername, stdpassword, stddomain);
	} catch (const TException &tx) {
		fprintf(stderr, "%s: TException: %s\n", __FUNCTION__, tx.what());
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	} catch (...) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	}

	if (!result.success) {
		SetLastError(ERROR_INTERNAL_ERROR);
		return FALSE;
	}

	currentCon->authToken = result.authToken;
	currentCon->sessionId = 0;
	currentCon->authTokenScanned = true;
	SetLastError(ERROR_SUCCESS);
	return TRUE;
}

static WtsApiFunctionTable ogon_WtsApiFunctionTable =
{
	0, /* dwVersion */
	0, /* dwFlags */

	ogon_WTSStopRemoteControlSession, /* StopRemoteControlSession */
	ogon_WTSStartRemoteControlSessionW, /* StartRemoteControlSessionW */
	ogon_WTSStartRemoteControlSessionA, /* StartRemoteControlSessionA */
	ogon_WTSConnectSessionW, /* ConnectSessionW */
	ogon_WTSConnectSessionA, /* ConnectSessionA */
	ogon_WTSEnumerateServersW, /* EnumerateServersW */
	ogon_WTSEnumerateServersA, /* EnumerateServersA */
	ogon_WTSOpenServerW, /* OpenServerW */
	ogon_WTSOpenServerA, /* OpenServerA */
	ogon_WTSOpenServerExW, /* OpenServerExW */
	ogon_WTSOpenServerExA, /* OpenServerExA */
	ogon_WTSCloseServer, /* CloseServer */
	ogon_WTSEnumerateSessionsW, /* EnumerateSessionsW */
	ogon_WTSEnumerateSessionsA, /* EnumerateSessionsA */
	ogon_WTSEnumerateSessionsExW, /* EnumerateSessionsExW */
	ogon_WTSEnumerateSessionsExA, /* EnumerateSessionsExA */
	ogon_WTSEnumerateProcessesW, /* EnumerateProcessesW */
	ogon_WTSEnumerateProcessesA, /* EnumerateProcessesA */
	ogon_WTSTerminateProcess, /* TerminateProcess */
	ogon_WTSQuerySessionInformationW, /* QuerySessionInformationW */
	ogon_WTSQuerySessionInformationA, /* QuerySessionInformationA */
	ogon_WTSQueryUserConfigW, /* QueryUserConfigW */
	ogon_WTSQueryUserConfigA, /* QueryUserConfigA */
	ogon_WTSSetUserConfigW, /* SetUserConfigW */
	ogon_WTSSetUserConfigA, /* SetUserConfigA */
	ogon_WTSSendMessageW, /* SendMessageW */
	ogon_WTSSendMessageA, /* SendMessageA */
	ogon_WTSDisconnectSession, /* DisconnectSession */
	ogon_WTSLogoffSession, /* LogoffSession */
	ogon_WTSShutdownSystem, /* ShutdownSystem */
	ogon_WTSWaitSystemEvent, /* WaitSystemEvent */
	ogon_WTSVirtualChannelOpen, /* VirtualChannelOpen */
	ogon_WTSVirtualChannelOpenEx, /* VirtualChannelOpenEx */
	ogon_WTSVirtualChannelClose, /* VirtualChannelClose */
	ogon_WTSVirtualChannelRead, /* VirtualChannelRead */
	ogon_WTSVirtualChannelWrite, /* VirtualChannelWrite */
	ogon_WTSVirtualChannelPurgeInput, /* VirtualChannelPurgeInput */
	ogon_WTSVirtualChannelPurgeOutput, /* VirtualChannelPurgeOutput */
	ogon_WTSVirtualChannelQuery, /* VirtualChannelQuery */
	ogon_WTSFreeMemory, /* FreeMemory */
	ogon_WTSRegisterSessionNotification, /* RegisterSessionNotification */
	ogon_WTSUnRegisterSessionNotification, /* UnRegisterSessionNotification */
	ogon_WTSRegisterSessionNotificationEx, /* RegisterSessionNotificationEx */
	ogon_WTSUnRegisterSessionNotificationEx, /* UnRegisterSessionNotificationEx */
	ogon_WTSQueryUserToken, /* QueryUserToken */
	ogon_WTSFreeMemoryExW, /* FreeMemoryExW */
	ogon_WTSFreeMemoryExA, /* FreeMemoryExA */
	ogon_WTSEnumerateProcessesExW, /* EnumerateProcessesExW */
	ogon_WTSEnumerateProcessesExA, /* EnumerateProcessesExA */
	ogon_WTSEnumerateListenersW, /* EnumerateListenersW */
	ogon_WTSEnumerateListenersA, /* EnumerateListenersA */
	ogon_WTSQueryListenerConfigW, /* QueryListenerConfigW */
	ogon_WTSQueryListenerConfigA, /* QueryListenerConfigA */
	ogon_WTSCreateListenerW, /* CreateListenerW */
	ogon_WTSCreateListenerA, /* CreateListenerA */
	ogon_WTSSetListenerSecurityW, /* SetListenerSecurityW */
	ogon_WTSSetListenerSecurityA, /* SetListenerSecurityA */
	ogon_WTSGetListenerSecurityW, /* GetListenerSecurityW */
	ogon_WTSGetListenerSecurityA, /* GetListenerSecurityA */
	ogon_WTSEnableChildSessions, /* EnableChildSessions */
	ogon_WTSIsChildSessionsEnabled, /* IsChildSessionsEnabled */
	ogon_WTSGetChildSessionId, /* GetChildSessionId */
	ogon_WTSGetActiveConsoleSessionId, /* GetActiveConsoleSessionId */
	ogon_WTSLogonUser,
	ogon_WTSLogoffUser,
	ogon_WTSStartRemoteControlSessionExW, /* StartRemoteControlSessionW */
	ogon_WTSStartRemoteControlSessionExA /* StartRemoteControlSessionA */
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Export InitWtsApi() but do not expose it with a header file due to possible conflicts
 */

WINPR_API PWtsApiFunctionTable CDECL InitWtsApi(void) {
	return &ogon_WtsApiFunctionTable;
}

/**
 * Export explicit ogon_InitWtsApi() which we can safely expose with a header
 */

WINPR_API PWtsApiFunctionTable CDECL ogon_InitWtsApi(void) {
	return &ogon_WtsApiFunctionTable;
}

#ifdef __cplusplus
}
#endif
