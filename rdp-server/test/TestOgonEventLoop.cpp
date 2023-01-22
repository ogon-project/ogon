/**
 * ogon - Free Remote Desktop Services
 * RDP Server
 * Eventloop Test
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * David Fort <contact@hardening-consulting.com>
 *
 * Permission to use, copy, modify, distribute, and sell this file for any
 * purpose is hereby granted without fee, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and this
 * permission notice appear in supporting documentation.
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of this file.
 *
 * THIS FILE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <string>

#include <winpr/collections.h>
#include <winpr/file.h>
#include <winpr/memory.h>
#include <winpr/path.h>
#include <winpr/pipe.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include "../common/global.h"

#include "../eventloop.cpp"

typedef struct _pipe_client_context pipe_client_context;
typedef struct _pipe_server_context pipe_server_context;

typedef int (*pipe_client_factory)(pipe_server_context *context, HANDLE handle);

struct _pipe_server_context {
	ogon_event_loop *evloop;

	int connections;
	HANDLE listenHandle;
	ogon_event_source *listenerSource;

	pipe_client_context *client;

	HANDLE finishClient;
	HANDLE pingPong1;
	HANDLE pingPong2;
	int fastlyWritten;

	pipe_client_factory clientFactory;
};

struct _pipe_client_context {
	int readBytes;
	int writtenBytes;
	int blockedWrites;
	HANDLE handle;
	pipe_server_context *server;
	ogon_event_source *evSource;
};

static int handle_pipe_connection(int mask, int fd, HANDLE handle, void *data) {
	pipe_server_context *context = (pipe_server_context *)data;
	DWORD dwPipeMode;
	HANDLE newHandle;

	OGON_UNUSED(fd);
	OGON_UNUSED(handle);
	/* fprintf(stderr, "%s(fd=%d mask=0x%x)\n", __FUNCTION__, fd, mask); */
	context->connections++;

	if ((mask & OGON_EVENTLOOP_READ) == 0) return -1;

	if (!ConnectNamedPipe(context->listenHandle, NULL) &&
			(GetLastError() != ERROR_PIPE_CONNECTED))
		return -1;

	dwPipeMode = PIPE_NOWAIT;
	newHandle = context->listenHandle;
	SetNamedPipeHandleState(newHandle, &dwPipeMode, NULL, NULL);

	return context->clientFactory(context, newHandle);
}

int handle_pipe_bytes(int mask, int fd, HANDLE handle, void *data) {
	char buffer[200];
	DWORD readBytes = 0xdeadbeef;
	pipe_client_context *context = (pipe_client_context *)data;

	OGON_UNUSED(fd);
	OGON_UNUSED(mask);

	if (!(mask & OGON_EVENTLOOP_READ)) return 0;

	if (!ReadFile(handle, buffer, sizeof(buffer), &readBytes, NULL) ||
			!readBytes) {
		if (GetLastError() == ERROR_NO_DATA) return 0;

		// fprintf(stderr, "%s: removing, read this time=0x%x, total=%d\n",
		// __FUNCTION__, readBytes, context->readBytes);
		eventloop_remove_source(&context->evSource);
		context->server->client = 0;
		delete (context);
		return -1;
	}

	// fprintf(stderr, "%s: read this time=%d total=%d\n", __FUNCTION__,
	// readBytes, context->readBytes);
	context->readBytes += readBytes;
	return 0;
}

int client1_factory(pipe_server_context *serverContext, HANDLE handle) {
	pipe_client_context *clientContext = new (pipe_client_context);
	if (!clientContext) {
		fprintf(stderr, "%s: Failed to allocate clientContext\n", __FUNCTION__);
		return -1;
	}
	clientContext->handle = handle;
	clientContext->server = serverContext;
	clientContext->evSource =
			eventloop_add_handle(serverContext->evloop, OGON_EVENTLOOP_READ,
					clientContext->handle, handle_pipe_bytes, clientContext);
	if (!clientContext->evSource) {
		fprintf(stderr,
				"%s: unable to create to add the client handle in the "
				"eventloop\n",
				__FUNCTION__);
		return -1;
	}

	serverContext->client = clientContext;
	return 0;
}

#define LISTEN_PIPE_NAME "\\\\.\\pipe\\listen"
#define CLIENT1_BYTES (10 * 1000)
#define CLIENT1_WRITE_LIMIT 300

static void *client1_thread(void *arg) {
	pipe_server_context *server = (pipe_server_context *)arg;
	HANDLE clientHandle;
	DWORD written, toWrite;
	int i;
	char *sendPtr;

	std::string buffer;
	buffer.resize(CLIENT1_BYTES);

	sendPtr = buffer.data();
	for (i = 0; i < CLIENT1_BYTES; i++) sendPtr[i] = (char)i;

	clientHandle = CreateFileA(LISTEN_PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, 0, NULL);
	if (clientHandle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "%s: unable to connect on the named pipe\n",
				__FUNCTION__);
		return 0;
	}

	sendPtr = buffer.data();
	toWrite = CLIENT1_BYTES;

	while (toWrite) {
		if (!WriteFile(clientHandle, sendPtr,
					(toWrite > CLIENT1_WRITE_LIMIT) ? CLIENT1_WRITE_LIMIT
													: toWrite,
					&written, NULL)) {
			fprintf(stderr, "error writing");
			break;
		}

		// fprintf(stderr, "%d written\n", written);
		Sleep(1);

		toWrite -= written;
		sendPtr += written;
	}

	// fprintf(stderr, "%s waiting signal to finish\n", __FUNCTION__);

	WaitForSingleObject(server->finishClient, INFINITE);

	// fprintf(stderr, "%s finished\n", __FUNCTION__);
	CloseHandle(clientHandle);
	return 0;
}

#define PINPONG_LOOPS 10

static void *pingpong_thread(void *arg) {
	pipe_server_context *server = (pipe_server_context *)arg;
	int i;

	for (i = 0; i < PINPONG_LOOPS; i++) {
		SetEvent(server->pingPong1);
		if (WaitForSingleObject(server->pingPong2, 500) != WAIT_OBJECT_0) {
			fprintf(stderr, "%s: pingPong2, not notified\n", __FUNCTION__);
			return 0;
		}
	}

	WaitForSingleObject(server->finishClient, INFINITE);
	return 0;
}

static int handle_pingPong1(int mask, int fd, HANDLE handle, void *data) {
	pipe_server_context *server = (pipe_server_context *)data;

	OGON_UNUSED(fd);
	OGON_UNUSED(handle);

	if (!(mask & OGON_EVENTLOOP_READ)) {
		fprintf(stderr, "%s: not expecting mask 0x%x\n", __FUNCTION__, mask);
		return -1;
	}

	if (WaitForSingleObject(server->pingPong1, 1) != WAIT_OBJECT_0) {
		fprintf(stderr, "%s: looks like pingPong1 is not notified\n",
				__FUNCTION__);
	}

	SetEvent(server->pingPong2);
	return 0;
}

#define FAST_WRITER_STEPS 1000
#define SLOW_READER_BYTES 100 * 1000

static int handle_big_writer(int mask, int fd, HANDLE handle, void *data) {
	char inBuffer[FAST_WRITER_STEPS];
	DWORD written, toWrite;
	pipe_client_context *context = (pipe_client_context *)data;

	OGON_UNUSED(fd);

	ZeroMemory(inBuffer, sizeof(inBuffer));
	if (!(mask & OGON_EVENTLOOP_WRITE)) {
		fprintf(stderr, "%s: not expecting missing write flag, mask=0x%x\n",
				__FUNCTION__, mask);
		return -1;
	}

	while (TRUE) {
		toWrite = sizeof(inBuffer);
		if (context->writtenBytes + toWrite > SLOW_READER_BYTES)
			toWrite = SLOW_READER_BYTES - context->writtenBytes;

		if (!WriteFile(handle, inBuffer, toWrite, &written, NULL)) {
			break;
		}

		// fprintf(stderr, "written %d\n", written);
		if (!written)  // output buffer is full
		{
			context->blockedWrites++;
			break;
		}

		context->writtenBytes += written;
	}
	return 0;
}

int big_writer_factory(pipe_server_context *context, HANDLE handle) {
	OGON_UNUSED(handle);

	pipe_client_context *clientContext = new (pipe_client_context);
	if (!clientContext) {
		fprintf(stderr, "%s: Failed to allocate clientContext\n", __FUNCTION__);
		return -1;
	}
	clientContext->handle = context->listenHandle;
	clientContext->server = context;

	context->client = clientContext;

	clientContext->evSource =
			eventloop_add_handle(context->evloop, OGON_EVENTLOOP_WRITE,
					clientContext->handle, handle_big_writer, clientContext);
	if (!clientContext->evSource) {
		fprintf(stderr,
				"%s: unable to create to add the client handle in the "
				"eventloop\n",
				__FUNCTION__);
		return -1;
	}

	return 0;
}

static void *slow_reader_thread(void *arg) {
	pipe_server_context *server = (pipe_server_context *)arg;
	HANDLE clientHandle;
	DWORD readBytes, totalRead;
	char buffer[200];

	clientHandle = CreateFileA(LISTEN_PIPE_NAME, GENERIC_READ | GENERIC_WRITE,
			0, NULL, OPEN_EXISTING, 0, NULL);
	if (clientHandle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "%s: unable to connect on the named pipe\n",
				__FUNCTION__);
		return 0;
	}

	totalRead = 0;
	while (totalRead != SLOW_READER_BYTES) {
		if (!ReadFile(clientHandle, buffer, sizeof(buffer), &readBytes, NULL) &&
				GetLastError() != ERROR_NO_DATA)
			break;

		// fprintf(stderr, "read %d\n", readBytes);
		Sleep(1);

		totalRead += readBytes;
	}

	WaitForSingleObject(server->finishClient, INFINITE);

	CloseHandle(clientHandle);
	return 0;
}

extern "C" int TestOgonEventLoop(int argc, char *argv[]) {
	ogon_event_source *source;
	pipe_server_context context;
	char *filename;
	HANDLE clientThread;
	int nb, loopTurns, testNo, bytesReceived;
	ogon_source_state sourceState;

	OGON_UNUSED(argc);
	OGON_UNUSED(argv);

	ZeroMemory(&context, sizeof(context));
	filename = GetNamedPipeUnixDomainSocketFilePathA(LISTEN_PIPE_NAME);
	if (PathFileExistsA(filename)) DeleteFileA(filename);
	free(filename);

	context.listenHandle = CreateNamedPipeA(LISTEN_PIPE_NAME,
			PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES, 1024, 1024, 0, NULL);
	if (context.listenHandle == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "unable to create listening pipe\n");
		return -1;
	}

	context.evloop = eventloop_create();
	if (!context.evloop) return -1;

	context.finishClient = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!context.finishClient) {
		fprintf(stderr, "unable to create finishClien1 event\n");
		return -1;
	}

	context.clientFactory = client1_factory;
	context.listenerSource =
			eventloop_add_handle(context.evloop, OGON_EVENTLOOP_READ,
					context.listenHandle, handle_pipe_connection, &context);
	if (!context.listenerSource) {
		fprintf(stderr, "unable to add the listenHandle in the eventloop\n");
		return -1;
	}

	clientThread = CreateThread(
			NULL, 0, (LPTHREAD_START_ROUTINE)client1_thread, &context, 0, NULL);
	if (!clientThread) {
		fprintf(stderr, "unable to start client thread\n");
		return -1;
	}

	/* =========================================================================
	 *
	 * In this test we launch a client thread that will connect on our listening
	 * named pipe. We should receive a "read available" notification when a peer
	 * is connecting.
	 *
	 */
	testNo = 0;
	fprintf(stderr, "%d: testing read availability during accept()\n",
			++testNo);
	nb = eventloop_dispatch_loop(context.evloop, 1 * 1000);
	if (!nb) {
		fprintf(stderr, "no treatment during dispatching\n");
		return -1;
	}

	if (!context.connections || !context.client) {
		fprintf(stderr, "no client connection\n");
		return -1;
	}

	/* =========================================================================
	 *	In this test we check that we can store and restore an event source
	 */
	fprintf(stderr, "%d: testing store/restore eventsource\n", ++testNo);

	/* ensure that we have already received some bytes */
	for (loopTurns = 0; !context.client->readBytes && loopTurns < 5;
			loopTurns++) {
		nb = eventloop_dispatch_loop(context.evloop, 100);
	}

	if (!context.client->readBytes) {
		fprintf(stderr, "no bytes received !\n");
		return -1;
	}

	/* remove the event source and restore it */
	bytesReceived = context.client->readBytes;
	eventsource_store_state(context.client->evSource, &sourceState);
	eventloop_remove_source(&context.client->evSource);
	// fprintf(stderr, "event source removed !\n");

	eventloop_dispatch_loop(context.evloop, 100);
	if (bytesReceived != context.client->readBytes) {
		fprintf(stderr,
				"I have received some more bytes with my event source removed "
				"!\n");
		return -1;
	}

	context.client->evSource =
			eventloop_restore_source(context.evloop, &sourceState);
	if (!context.client->evSource) {
		fprintf(stderr, "unable to restore the event source !\n");
		return -1;
	}
	// fprintf(stderr, "event source restored !\n");

	eventloop_dispatch_loop(context.evloop, 100);
	if (!context.client || (bytesReceived == context.client->readBytes)) {
		fprintf(stderr,
				"eventsource restore, but we didn't received more bytes !\n");
		return -1;
	}

	/* =========================================================================
	 *	In this test we check that we receive "read ready" notifications when
	 *	some bytes are sent by the client thread. We also check that we retrieve
	 *	the correct number of bytes.
	 */
	fprintf(stderr, "%d: testing read availability for bytes received\n",
			++testNo);
	for (loopTurns = 0; loopTurns < CLIENT1_BYTES; loopTurns++) {
		nb = eventloop_dispatch_loop(context.evloop, 100);
		if (context.client->readBytes >= CLIENT1_BYTES) break;
	}

	if (context.client->readBytes != CLIENT1_BYTES) {
		fprintf(stderr,
				"don't have the number of expected bytes, got %d instead of "
				"%d\n",
				context.client->readBytes, CLIENT1_BYTES);
		return -1;
	}

	/* =========================================================================
	 * In this test we check that the client finalisation triggers a "read
	 * ready" notification
	 */
	fprintf(stderr, "%d: testing read availability during shutdown\n",
			++testNo);
	SetEvent(context.finishClient);

	nb = eventloop_dispatch_loop(context.evloop, 100);
	if (!nb || context.client) {
		fprintf(stderr, "client1 finalisation not caught\n");
		return -1;
	}

	/* =========================================================================
	 * In this test we try to put an Event in the event loop, and verify that
	 * setting the event is correctly detected by a "read ready" notification.
	 * We do some pingpong between the client thread and the notifications.
	 */
	fprintf(stderr, "%d: testing an Event in the event loop\n", ++testNo);
	context.pingPong1 = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!context.pingPong1) {
		fprintf(stderr, "unable to create pingPong1 event\n");
		return -1;
	}
	context.pingPong2 = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!context.pingPong2) {
		CloseHandle(context.pingPong1);
		fprintf(stderr, "unable to create pingPong2 event\n");
		return -1;
	}

	source = eventloop_add_handle(context.evloop, OGON_EVENTLOOP_READ,
			context.pingPong1, handle_pingPong1, &context);
	if (!source) {
		fprintf(stderr, "unable to add an Event handle in the eventloop\n");
		return -1;
	}

	clientThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)pingpong_thread, &context, 0, NULL);
	if (!clientThread) {
		fprintf(stderr, "unable to start pingpong thread\n");
		return -1;
	}

	for (loopTurns = 0; loopTurns < PINPONG_LOOPS; loopTurns++) {
		nb = eventloop_dispatch_loop(context.evloop, 50);
		if (!nb) {
			fprintf(stderr, "no activity during turn %d\n", loopTurns);
			return -1;
		}
	}

	SetEvent(context.finishClient);
	eventloop_remove_source(&source);

	/* =========================================================================
	 *	In this test we instantiate a thread that will read bytes very slowly.
	 *	We checks that the write ready notifications are correctly handled.
	 */
	fprintf(stderr, "%d: testing detection of write ready\n", ++testNo);

	context.clientFactory = big_writer_factory;

	clientThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE)slow_reader_thread, &context, 0, NULL);
	if (!clientThread) {
		fprintf(stderr, "unable to start slow reader thread\n");
		return -1;
	}

	for (loopTurns = 0; loopTurns < SLOW_READER_BYTES; loopTurns++) {
		if (context.client && context.client->writtenBytes >= SLOW_READER_BYTES)
			break;

		nb = eventloop_dispatch_loop(context.evloop, 50);
	}

	if (context.client->writtenBytes != SLOW_READER_BYTES) {
		fprintf(stderr,
				"not all bytes written while slow reader was reading them\n");
		return -1;
	}

	if (!context.client->blockedWrites) {
		fprintf(stderr, "strange that write() has never blocked\n");
		return -1;
	}

	SetEvent(context.finishClient);

	return 0;
}
