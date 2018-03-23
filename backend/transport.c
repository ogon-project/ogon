/**
 * ogon - Free Remote Desktop Services
 * Backend Library
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
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

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/pipe.h>

#include "../common/global.h"
#include "protocol.h"

#define TAG OGON_TAG("backend.transport")

#define PIPE_BUFFER_SIZE 0xFFFF

void ogon_named_pipe_get_endpoint_name(DWORD id, const char *endpoint, char *dest, size_t len) {
	sprintf_s(dest, len, "\\\\.\\pipe\\ogon_%"PRIu32"_%s", id, endpoint);
}

BOOL ogon_named_pipe_clean(const char *pipeName) {
	BOOL result = TRUE;
	char *filename;

	if (!pipeName) {
		WLog_ERR(TAG, "%s called with null pipe name", __FUNCTION__);
		return FALSE;
	}

	if (!(filename = GetNamedPipeUnixDomainSocketFilePathA(pipeName))) {
		WLog_ERR(TAG, "GetNamedPipeUnixDomainSocketFilePathA(%s) failed", pipeName);
		return FALSE;
	}

	if (PathFileExistsA(filename)) {
		if (!DeleteFileA(filename)) {
			WLog_ERR(TAG, "Failed to delete '%s'", filename);
			result = FALSE;
		}
	}

	free(filename);
	return result;
}

BOOL ogon_named_pipe_clean_endpoint(DWORD id, const char *endpoint) {
	char pipeName[256];

	ogon_named_pipe_get_endpoint_name(id, endpoint, pipeName, sizeof(pipeName));
	return ogon_named_pipe_clean(pipeName);
}

HANDLE ogon_named_pipe_connect(const char *pipeName, DWORD nTimeOut) {
	HANDLE hNamedPipe;

	if (!WaitNamedPipeA(pipeName, nTimeOut)) {
		WLog_ERR(TAG, "WaitNamedPipeA failure: %s", pipeName);
		return INVALID_HANDLE_VALUE;
	}

	hNamedPipe = CreateFileA(pipeName, GENERIC_READ | GENERIC_WRITE,
		 0, NULL, OPEN_EXISTING, 0, NULL);

	if ((!hNamedPipe) || (hNamedPipe == INVALID_HANDLE_VALUE)) {
		WLog_ERR(TAG, "Failed to create named pipe %s", pipeName);
		return INVALID_HANDLE_VALUE;
	}

	return hNamedPipe;
}

HANDLE ogon_named_pipe_connect_endpoint(DWORD id, const char *endpoint, DWORD nTimeOut) {
	char pipeName[256];

	ogon_named_pipe_get_endpoint_name(id, endpoint, pipeName, sizeof(pipeName));
	return ogon_named_pipe_connect(pipeName, nTimeOut);
}

HANDLE ogon_named_pipe_create(const char *pipeName) {
	HANDLE hNamedPipe;

	hNamedPipe = CreateNamedPipe(pipeName, PIPE_ACCESS_DUPLEX,
		PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
		PIPE_UNLIMITED_INSTANCES, PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE,
		0, NULL);

	if (hNamedPipe == INVALID_HANDLE_VALUE) {
		WLog_ERR(TAG, "CreateNamedPipe '%s' failed", pipeName);
		return NULL;
	}

	return hNamedPipe;
}

HANDLE ogon_named_pipe_create_endpoint(DWORD id, const char *endpoint) {
	char pipeName[256];

	ogon_named_pipe_get_endpoint_name(id, endpoint, pipeName, sizeof(pipeName));
	return ogon_named_pipe_create(pipeName);
}

HANDLE ogon_named_pipe_accept(HANDLE hPipe) {
	DWORD pipeMode;

	if (!ConnectNamedPipe(hPipe, NULL) && GetLastError() != ERROR_PIPE_CONNECTED) {
		WLog_ERR(TAG, "Failed to accept named pipe connection (error 0x%08"PRIX32")", GetLastError());
		return NULL;
	}

	pipeMode = PIPE_NOWAIT;
	SetNamedPipeHandleState(hPipe, &pipeMode, NULL, NULL);

	return hPipe;
}
