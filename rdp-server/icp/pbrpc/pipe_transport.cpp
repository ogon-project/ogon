/**
 * ogon - Free Remote Desktop Services
 * pbRPC: Simple Protocol Buffers based RPC
 * Named Pipe Transport
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/pipe.h>

#include "pipe_transport.h"

struct np_transport_context {
	pbRPCTransportContext context;
	HANDLE handle;
};
typedef struct np_transport_context NpTransportContext;

static int tp_npipe_open(pbRPCTransportContext *context, int timeout) {
	HANDLE hNamedPipe = nullptr;
	char pipeName[] = "\\\\.\\pipe\\ogon_SessionManager";
	NpTransportContext *np = (NpTransportContext *)context;

	if (!WaitNamedPipeA(pipeName, timeout)) {
		return -1;
	}
	hNamedPipe = CreateFileA(pipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr,
			OPEN_EXISTING, 0, nullptr);

	if ((!hNamedPipe) || (hNamedPipe == INVALID_HANDLE_VALUE)) {
		return -1;
	}

	np->handle = hNamedPipe;

	return 0;
}

static int tp_npipe_close(pbRPCTransportContext *context) {
	NpTransportContext *np = (NpTransportContext *)context;

	if (np->handle) {
		CloseHandle(np->handle);
		np->handle = nullptr;
	}

	return 0;
}

static int tp_npipe_write(
		pbRPCTransportContext *context, char *data, unsigned int datalen) {
	DWORD bytesWritten;
	BOOL fSuccess = FALSE;
	NpTransportContext *np = (NpTransportContext *)context;

	fSuccess = WriteFile(
			np->handle, data, datalen, (LPDWORD)&bytesWritten, nullptr);

	if (!fSuccess || (bytesWritten < datalen)) {
		return -1;
	}

	return bytesWritten;
}

static int tp_npipe_read(
		pbRPCTransportContext *context, char *data, unsigned int datalen) {
	NpTransportContext *np = (NpTransportContext *)context;
	DWORD bytesRead;
	BOOL fSuccess = FALSE;

	fSuccess = ReadFile(np->handle, data, datalen, &bytesRead, nullptr);

	if (!fSuccess || (bytesRead < datalen)) {
		return -1;
	}

	return bytesRead;
}

static HANDLE tp_npipe_get_fds(pbRPCTransportContext *context) {
	NpTransportContext *np = (NpTransportContext *)context;
	return np->handle;
}

pbRPCTransportContext *tp_npipe_new() {
	auto np = new NpTransportContext;
	if (!np) return nullptr;

	pbRPCTransportContext *ctx = &np->context;
	ctx->open = tp_npipe_open;
	ctx->close = tp_npipe_close;
	ctx->read = tp_npipe_read;
	ctx->write = tp_npipe_write;
	ctx->get_fds = tp_npipe_get_fds;
	return ctx;
}

void tp_npipe_free(pbRPCTransportContext *context) { delete (context); }
