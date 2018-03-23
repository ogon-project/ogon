/**
 * ogon - Free Remote Desktop Services
 * Command Line Interface for ogon
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

#include <stdio.h>
#include <string.h>
#include <string>
#include <arpa/inet.h>

#include <winpr/wtypes.h>
#include <winpr/cmdline.h>
#include <winpr/wtsapi.h>
#include <winpr/string.h>
#include <winpr/timezone.h>
#include <winpr/input.h>
#include <time.h>
#include <winpr/user.h>
#include <getopt.h>

using namespace std;

static struct option long_options[] = {
		{"help", no_argument, 0, 'h' },
		{"list", no_argument, 0, 'l' },
		{"listdetail",  no_argument, 0, 'x' },
		{"user", required_argument, 0, 'u' },
		{"password", required_argument, 0, 'p'},
		{"session", required_argument, 0, 's' },
		{"server", required_argument, 0, 'e' },
		{"disconnect", no_argument, 0, 'd' },
		{"logoff", no_argument, 0, 'o' },
		{"disconnectUser", required_argument, 0, 'D' },
		{"logoffUser", required_argument, 0, 'O' },
		{"startShadow", no_argument, 0,  'm' },
		{"stopShadow", no_argument, 0,  't' },
		{"text", required_argument, 0,  'g' },
		{0, 0, 0, 0 }
};

#define SHORT_OPTS "hlxu:p:s:e:doD:O:mtg:"

#define TICKS_PER_SECOND 10000000
#define EPOCH_DIFFERENCE 11644473600LL
time_t convertWindowsTimeToUnixTime(long long int input){
	long long int temp;
	temp = input / TICKS_PER_SECOND; //convert from 100ns intervals to seconds;
	temp = temp - EPOCH_DIFFERENCE;  //subtract number of seconds between epochs
	return (time_t) temp;
}

#define MESSAGE_TITLE "ogon message"

const char * stateToString(WTS_CONNECTSTATE_CLASS connectState) {
	switch (connectState) {
		case WTSActive:
			return "Active";
		case WTSConnected:
			return "Connected";
		case WTSConnectQuery:
			return "ConnectQuery";
		case WTSShadow:
			return "Shadow";
		case WTSDisconnected:
			return "Disconnected";
		case WTSIdle:
			return "Idle";
		case WTSListen:
			return "Listen";
		case WTSReset:
			return "Reset";
		case WTSDown:
			return "Down";
		case WTSInit:
			return "Init";
		default:
			return "Unkown";
	}
}

const char * protoTypeToString(USHORT type) {
	switch (type) {
		case 0:
			return "console session";
		case 1:
			return "legacy value, should not be set";
		case 2:
			return "rdp session";
		default:
			return "unknown value";
	}
}

const char * addrFamToString(DWORD type) {
	switch (type) {
		case 2:
			return "IPv4";
		case 23:
			return "IPv6";
		default:
			return "unknown value";
	}
}

BOOL printSessionDetailed(UINT32 sessionId, WTS_CONNECTSTATE_CLASS connectState, HANDLE hServer) {

	LPSTR pBuffer = NULL;
	BOOL bSuccess;
	DWORD bytesReturned = 0;
	char buffer[25];

	printf("SessionId: %" PRIu32 " State: %s\n", sessionId, stateToString(connectState));

	/* WTSUserName */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSSessionInfo, &pBuffer, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSSessionInfo failed: %" PRIu32 "\n", GetLastError());
		return FALSE;
	}

	PWTSINFO info = (PWTSINFO)pBuffer;
	printf("\tUserName:                    %s\n", info->UserName);
	printf("\tDomainName:                  %s\n", info->Domain);
	printf("\tWinStationName:              %s\n", info->WinStationName);
	if (info->ConnectTime.QuadPart > 0) {
		time_t testtime = convertWindowsTimeToUnixTime(info->ConnectTime.QuadPart);
		strftime(buffer, 25, "%Y-%m-%d %H:%M", localtime(&testtime));
	} else {
		strncpy(buffer, "Not Available", 25);
	}
	printf("\tConnect Time:                %s\n", buffer);
	if (info->LogonTime.QuadPart > 0) {
		time_t testtime = convertWindowsTimeToUnixTime(info->LogonTime.QuadPart);
		strftime(buffer, 25, "%Y-%m-%d %H:%M", localtime(&testtime));
	} else {
		strncpy(buffer, "Not Available", 25);
	}
	printf("\tLogon Time:                  %s\n", buffer);
	WTSFreeMemory(pBuffer);

	if ( !((connectState == WTSActive) || (connectState == WTSShadow))) {
		return TRUE;
	}

	/* WTSClientProtocolType */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientProtocolType, &pBuffer, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSClientProtocolType failed: %" PRIu32 "\n", GetLastError());
		return FALSE;
	}

	printf("\tSessionType:                 %s\n", protoTypeToString(*((USHORT*)pBuffer)));
	WTSFreeMemory(pBuffer);

	/* WTSClientBuildNumber */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientBuildNumber, &pBuffer, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSClientBuildNumber failed: %" PRIu32 "\n", GetLastError());
		return FALSE;
	}

	printf("\tClientBuildNumber:           %" PRIu32 "\n", *((ULONG*)pBuffer));
	WTSFreeMemory(pBuffer);

	/* WTSClientName */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientName, &pBuffer, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSClientName failed: %" PRIu32 "\n", GetLastError());
		return FALSE;
	}

	printf("\tClientName:                  %s\n", (char*)pBuffer);
	WTSFreeMemory(pBuffer);

	/* WTSClientProductId */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientProductId, &pBuffer, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSClientProductId failed: %" PRIu32 "\n", GetLastError());
		return FALSE;
	}

	printf("\tClientProductId:             %" PRIu16 "\n", *((USHORT*)pBuffer));
	WTSFreeMemory(pBuffer);

	/* WTSClientHardwareId */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientHardwareId, &pBuffer, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSClientHardwareId failed: %" PRIu32 "\n", GetLastError());
		return FALSE;
	}

	printf("\tClientHardwareId:            %" PRIu32 "\n", *((ULONG*)pBuffer));
	WTSFreeMemory(pBuffer);

	/* WTSClientAddress */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientAddress, &pBuffer, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSClientAddress failed: %" PRIu32 "\n", GetLastError());
		return FALSE;
	}

	PWTS_CLIENT_ADDRESS clientAddress = (PWTS_CLIENT_ADDRESS)pBuffer;

	printf("\tClientAddressFamily:         %s\n", addrFamToString(clientAddress->AddressFamily));

	if (clientAddress->AddressFamily == AF_INET) {
		printf("\tClientAddress:               %s\n", (char *)clientAddress->Address);
	} else if (clientAddress->AddressFamily == AF_INET6) {
		char ip6buffer[INET6_ADDRSTRLEN];
		const char *string = inet_ntop(AF_INET6, clientAddress->Address, ip6buffer, sizeof(ip6buffer));
		printf("\tWTSClientAddress:               %s\n", string);
	}
	WTSFreeMemory(pBuffer);

	/* WTSClientDisplay */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSClientDisplay, &pBuffer, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSClientDisplay failed: %" PRIu32 "\n", GetLastError());
		return FALSE;
	}

	PWTS_CLIENT_DISPLAY clientDisplay = (PWTS_CLIENT_DISPLAY)pBuffer;
	printf("\tClientDisplay:               %" PRIu32 "x%" PRIu32 " bpp: %" PRIu32 "\n\n",
			clientDisplay->HorizontalResolution, clientDisplay->VerticalResolution,
			clientDisplay->ColorDepth);
	WTSFreeMemory(pBuffer);

	return TRUE;
}

BOOL printSession(UINT32 sessionId, WTS_CONNECTSTATE_CLASS connectState, HANDLE hServer, BOOL printHeader) {

	LPSTR pUsername = NULL;
	LPSTR pDomain = NULL;
	BOOL bSuccess;
	DWORD bytesReturned = 0;

	if (printHeader) {
		// 10 15 15 5
		printf("SessionId  Username        Domain          State\n");
	}

	/* WTSUserName */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSUserName, &pUsername, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSUserName failed: %" PRIu32 "\n", GetLastError());
		return FALSE;
	}

	/* WTSDomainName */

	bSuccess = WTSQuerySessionInformation(hServer, sessionId, WTSDomainName, &pDomain, &bytesReturned);

	if (!bSuccess) {
		printf("WTSQuerySessionInformation WTSDomainName failed: %" PRIu32 "\n", GetLastError());
		WTSFreeMemory(pUsername);
		return FALSE;
	}
	printf("%9u  %-15.15s %-15.15s %s\n", sessionId, (char*)pUsername, (char*)pDomain, stateToString(connectState));

	WTSFreeMemory(pUsername);
	WTSFreeMemory(pDomain);

	return TRUE;
}



bool listSessions(HANDLE hServer, bool detailed) {
	DWORD index;
	DWORD count;
	BOOL bSuccess;
	PWTS_SESSION_INFO pSessionInfo;
	BOOL first = true;

	count = 0;
	pSessionInfo = NULL;

	bSuccess = WTSEnumerateSessions(hServer, 0, 1, &pSessionInfo, &count);

	if (!bSuccess) {
		printf("WTSEnumerateSessions failed: %" PRIu32 "\n", GetLastError());
		return false;
	}

	if (count == 0) {
		printf("No sessions found!\n");
	}

	for (index = 0; index < count; index++) {
		if (detailed) {
			printSessionDetailed(pSessionInfo[index].SessionId, pSessionInfo[index].State, hServer);
		} else {
			printSession(pSessionInfo[index].SessionId, pSessionInfo[index].State, hServer, first);
			first = false;
		}
	}

	WTSFreeMemory(pSessionInfo);
	return true;
}


bool terminateUserSessions(HANDLE hServer, const char* user, bool onlyDisconnect) {
	DWORD index;
	DWORD count;
	BOOL bSuccess;
	PWTS_SESSION_INFO pSessionInfo;
	LPSTR pUserName = NULL;
	DWORD bytesReturned = 0;
	UINT32 sessionId;
	BOOL bReturnValue = true;

	count = 0;
	pSessionInfo = NULL;

	bSuccess = WTSEnumerateSessions(hServer, 0, 1, &pSessionInfo, &count);

	if (!bSuccess) {
		printf("WTSEnumerateSessions failed: %" PRIu32 "\n", GetLastError());
		return false;
	}

	if (count == 0) {
		return true;
	}

	for (index = 0; index < count; index++) {
		sessionId = pSessionInfo[index].SessionId;

		if (!WTSQuerySessionInformation(hServer, sessionId, WTSUserName, &pUserName, &bytesReturned) ||
		    !pUserName || !bytesReturned)
		{
			printf("WTSQuerySessionInformation failed: %" PRIu32 "\n", GetLastError());
			bReturnValue = false;
			continue;
		}

		bSuccess = strcmp(pUserName, user) == 0;
		WTSFreeMemory(pUserName);
		pUserName = NULL;

		if (!bSuccess) {
			continue;
		}

		if (onlyDisconnect) {
			if (pSessionInfo[index].State == WTSDisconnected) {
				continue;
			}
			if (!WTSDisconnectSession(hServer, sessionId, true)) {
				printf("WTSDisconnectSession failed for session id %" PRIu32 ". Error %" PRIu32 "\n",
					sessionId, GetLastError());
				bReturnValue = false;
			} else {
				printf("Session id %" PRIu32 " disconnected.\n", sessionId);
			}
		}
		else {
			if (!WTSLogoffSession(hServer, sessionId, true)) {
				printf("WTSLogoffSession failed for session id %" PRIu32 ". Error %" PRIu32 "\n",
					sessionId, GetLastError());
				bReturnValue = false;
			} else {
				printf("Session id %" PRIu32 " logged off.\n", sessionId);
			}
		}
	}

	WTSFreeMemory(pSessionInfo);

	return bReturnValue;
}

void printhelprow(const char *kshort, const char *klong,const char *helptext) {
	printf("    %s, %-20s %s\n", kshort,klong,helptext);
}

void printhelp(const char *bin) {
	printf("Usage: %s [options]\n", bin);
	printf("\noptions:\n\n");
	printhelprow("-h", "--help", "prints this help screen");
	printhelprow("-l", "--list", "lists all sessions");
	printhelprow("-x", "--listdetail", "lists all session detailed");
	printhelprow("-u", "--user=<username>", "user to connect with");
	printhelprow("-p", "--password=<password>","password for user");
	printhelprow("-s", "--session=<sessionid>", "the sessionid");
	printhelprow("-d", "--disconnect","disconnects a given session");
	printhelprow("-o", "--logoff", "logs off a given session");
	printhelprow("-e", "--server=<remoteservername>", "name of a remote server");
	printhelprow("-m", "--startShadow", "start shadowing the given session");
	printhelprow("-t", "--stopShadow", "stop shadowing the given session");
	printhelprow("-g", "--text=<text>", "display a message in the specified session");
	printhelprow("-D", "--disconnectUser=<username>", "disconnect all sessions of the specified user");
	printhelprow("-O", "--logoffUser=<username>", "log off all sessions of the specified user");
	printf("\nexamples:\n\n");
	printf("  disconnect within a rdp session: ogon-cli -d\n");
	printf("  disconnect outside a rdp session: ogon-cli -d -s <sessionId> -u <username> -p <password>\n\n");
	printf("  logoff within a rdp session: ogon-cli -o\n");
	printf("  logoff outside a rdp session: ogon-cli -o -s <sessionId> -u <username> -p <password>\n\n");
	printf("  list sessions: ogon-cli -l -u <username> -p <password>\n");
	printf("  list sessions detailed: ogon-cli -x -u <username> -p <password>\n\n");
	printf("  shadow a session: ogon-cli -s <sessionId> -m\n\n");
	printf("    Note: To abort shadowing press CTRL + F10.\n");

}

int main(int argc,char * const argv[]) {

	std::string username;
	std::string password;
	std::string terminateUser;
	bool list = false;
	bool detailed = false;
	UINT32 sessionId = 0;
	UINT32 currentSessionId = 0;
	bool disconnect = false;
	bool logoff = false;
	bool startShadow = false;
	bool stopShadow = false;
	bool textMessage = false;
	std::string message;
	char *envval = NULL;
	std::string servername = "";

	while (1) {
		int c;
		int option_index = 0;


		c = getopt_long(argc,  &argv[0], SHORT_OPTS,
						long_options, &option_index);

		if (c == -1)
			break;

		switch (c) {
			case 'h':
				printhelp(argv[0]);
				return 0;

			case 'l':
				list = true;
				detailed = false;
				break;

			case 'x':
				list = true;
				detailed = true;
				break;

			case 'u':
				username.assign(optarg);
				break;

			case 'p':
				password.assign(optarg);
				break;

			case 'e':
				servername.assign(optarg);
				break;

			case 'd':
				disconnect = true;
				break;

			case 'o':
				logoff = true;
				break;

			case 'D':
				disconnect = true;
				terminateUser.assign(optarg);
				break;

			case 'O':
				logoff = true;
				terminateUser.assign(optarg);
				break;

			case 's':
				sessionId = atoi (optarg);
				break;

			case 'm':
				startShadow = true;
				break;

			case 't':
				stopShadow = true;
				break;

			case 'g':
				message.assign(optarg);
				textMessage = true;
				break;

			case '?':
			default:
				printhelp(argv[0]);
				return 0;
		}
	}

	int retVal = -1;
	HANDLE serverHandle = WTS_CURRENT_SERVER_HANDLE;

	if(servername.size() > 0) {
		serverHandle = WTSOpenServerA(const_cast<char *>(servername.c_str()));
		if (serverHandle == INVALID_HANDLE_VALUE) {
			goto out;
		}
	}

	if (username.size() > 0 ) {
		if (!WTSLogonUser(serverHandle, username.c_str(), password.c_str(), NULL)) {
			printf("Incorrect username or password!\n");
			goto out;
		}
	}

	if (list) {
		listSessions(serverHandle, detailed);
		retVal = 0;
		goto out;
	}

	// try to use current session
	envval = getenv("OGON_SID");
	if (envval) {
		currentSessionId = atoi(envval);
	}

	if (startShadow) {
		if (sessionId == 0) {
			printf("No session id given\n");
			goto out;
		}
		if (currentSessionId == 0) {
			printf("Shadowing only possible within a RDP session\n");
			goto out;
		}
		if (WTSStartRemoteControlSession(WTS_CURRENT_SERVER_NAME, sessionId, VK_F10, REMOTECONTROL_KBDCTRL_HOTKEY)) {
			printf("Starting shadowing of session with id %" PRIu32 ".\n", sessionId);
			printf("To abort shadowing press CTRL + F10.\n");
			retVal = 0;
			goto out;
		} else {
			printf("Shadowing of session %" PRIu32 " failed!\n", sessionId);
			goto out;
		}
	}

	if (stopShadow) {
		if (sessionId == 0) {
			printf("No session id given\n");
			goto out;
		}

		/*if (currentSessionId == 0) {
			printf("Shadowing only possible within a RDP session\n");
			goto out;
		}*/

		if (WTSStopRemoteControlSession(sessionId)) {
			printf("Stopping shadowing of session with id %" PRIu32 ".\n", sessionId);
			retVal = 0;
			goto out;
		} else {
			printf("Stopping shadowing of session %" PRIu32 " failed!\n", sessionId);
			goto out;
		}
	}

	if (terminateUser.size()) {
		if (terminateUserSessions(serverHandle, terminateUser.c_str(), !logoff))
			retVal = 0;
		goto out;
	}

	if (sessionId == 0) {
		if (currentSessionId == 0) {
			printf("SessionId missing or could not be parsed!\n");
			goto out;
		}
		sessionId = currentSessionId;
	}

	if (textMessage) {
		char *message_title;
		char *message_text;
		if (message.size() == 0) {
			printf("No message to display!\n");
			goto out;
		}
		DWORD result;
		message_title = strdup(MESSAGE_TITLE);
		if (!message_title) {
			printf("Failed to allocate memory for message title!\n");
			goto out;
		}
		message_text = strdup(message.c_str());
		if (!message_text) {
			printf("Failed to allocate memory for message text!\n");
			free(message_title);
			goto out;
		}
		WTSSendMessageA(serverHandle, sessionId, message_title, strlen(MESSAGE_TITLE) + 1, message_text, message.size() + 1, MB_OK, 0, &result, FALSE);
		free(message_title);
		free(message_text);
		retVal = 0;
		goto out;
	}

	if (disconnect) {
		if (WTSDisconnectSession(serverHandle, sessionId, true)) {
			printf("Session with sessionId %" PRIu32 " disconnected.\n", sessionId);
			retVal = 0;
			goto out;
		} else {
			printf("Disconnect of session with sessionId %" PRIu32 " failed!\n", sessionId);
			goto out;
		}
	}

	if (logoff) {
		if (WTSLogoffSession(serverHandle, sessionId, true)) {
			printf("Session with sessionId %" PRIu32 " logged off.\n", sessionId);
			retVal = 0;
			goto out;
		} else {
			printf("Logoff of session with sessionId %" PRIu32 " failed!\n", sessionId);
			goto out;
		}
	}
out:
	if (serverHandle != WTS_CURRENT_SERVER_HANDLE) {
		WTSCloseServer(serverHandle);
	}
	return retVal;
}
