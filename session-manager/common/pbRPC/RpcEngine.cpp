/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * RPC engine based on google protocol buffers
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

#include "RpcEngine.h"
#ifdef __linux__
#include <signal.h>
#endif
#include <appcontext/ApplicationContext.h>
#include <utils/CSGuard.h>
#include <call/CallFactory.h>
#include <call/CallIn.h>

#include <winpr/platform.h>
#include <winpr/pipe.h>
#include <winpr/thread.h>
#include <winpr/wlog.h>

#include <ogon/version.h>

#include <arpa/inet.h>

#include "../../common/security.h"

#define CLIENT_ERROR 2
#define SERVER_SHUTDOWN -1
#define CLIENT_SUCCESS 0
#define OGON_SESSION_MANAGER_PIPE "\\\\.\\pipe\\ogon_SessionManager"

static wLog *logger_RPCEngine = WLog_Get("ogon.pbrpc.rpcengine");

namespace ogon { namespace pbrpc {


	RpcEngine::RpcEngine() : mhClientPipe(INVALID_HANDLE_VALUE),
		mhServerPipe(INVALID_HANDLE_VALUE), mhServerThread(INVALID_HANDLE_VALUE),
		mPacktLength(0), mHeaderRead(0), mFirstPacket(TRUE), mPayloadRead(0), mNextOutCall(1)
	{
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_RPCEngine, WLOG_ERROR,
				"Failed to initialize rpc engine critical section");
			throw std::bad_alloc();
		}

		if (!(mhStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL))) {
			WLog_Print(logger_RPCEngine, WLOG_ERROR,
				"Failed to create rpc engine stop event");
			throw std::bad_alloc();
		}
	}

	RpcEngine::~RpcEngine() {
		DeleteCriticalSection(&mCSection);
		CloseHandle(mhStopEvent);
		google::protobuf::ShutdownProtobufLibrary();
	}

	HANDLE RpcEngine::createServerPipe(const char *endpoint) {

		DWORD dwPipeMode;
		HANDLE hNamedPipe;

		dwPipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;

		hNamedPipe = CreateNamedPipe(endpoint, PIPE_ACCESS_DUPLEX, dwPipeMode, PIPE_UNLIMITED_INSTANCES,
				PIPE_BUFFER_SIZE, PIPE_BUFFER_SIZE, 0, NULL);

		if (hNamedPipe == INVALID_HANDLE_VALUE) {
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "creating namedpipe %s failed", endpoint);
			return NULL;
		}

		return hNamedPipe;
	}

	bool RpcEngine::startEngine() {

		CSGuard guard(&mCSection);

		if (!(mhServerThread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) RpcEngine::listenerThread, (void*)this,
				0, NULL)))
		{
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "failed to create thread");
			return false;
		}
		return true;
	}

	bool RpcEngine::stopEngine() {
		CSGuard guard(&mCSection);
		if (mhServerThread) {
			SetEvent(mhStopEvent);
			WaitForSingleObject(mhServerThread, INFINITE);
			CloseHandle(mhServerThread);
			mhServerThread = NULL;
		}
		return true;
	}

	int RpcEngine::createServerPipe(void) {
		mhServerPipe = createServerPipe(OGON_SESSION_MANAGER_PIPE);
		if (mhServerPipe == INVALID_HANDLE_VALUE) {
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "Could not create named pipe %s",
						OGON_SESSION_MANAGER_PIPE);
			return SERVER_SHUTDOWN;
		}
		return CLIENT_SUCCESS;
	}

	void RpcEngine::listenerThread(void *arg) {
		RpcEngine *engine;

		engine = (RpcEngine *)arg;
		WLog_Print(logger_RPCEngine, WLOG_TRACE, "started RPC listener thread");

		while (1) {
			if (engine->createServerPipe() != CLIENT_SUCCESS) {
				WLog_Print(logger_RPCEngine, WLOG_ERROR, "server pipe could not be created!");
				break;
			}

			HANDLE clientPipe = engine->acceptClient();
			if (clientPipe == INVALID_HANDLE_VALUE) {
				WLog_Print(logger_RPCEngine, WLOG_ERROR, "client accept failed!");
				break;
			}

			if (engine->serveClient() == SERVER_SHUTDOWN) {
				WLog_Print(logger_RPCEngine, WLOG_ERROR, "server shutting down!");
				break;
			}
			engine->resetStatus();
			APP_CONTEXT.rpcDisconnected();

			if (engine->mAnswerWaitingQueue.size()) {
				WLog_Print(logger_RPCEngine, WLOG_WARN, "answer waiting queue not empty, discarding entries!");
				std::list<callNS::CallOutPtr>::iterator it;
				for ( it = engine->mAnswerWaitingQueue.begin(); it != engine->mAnswerWaitingQueue.end(); ++it) {
					callNS::CallOutPtr currentCallOut = (callNS::CallOutPtr)(*it);
					currentCallOut->setResult(2);
				}
				engine->mAnswerWaitingQueue.clear();
			}
		}
		WLog_Print(logger_RPCEngine, WLOG_TRACE, "stopped RPC listener thread");
		return;
	}

	HANDLE RpcEngine::acceptClient() {
		DWORD nCount;
		HANDLE events[2];

		if (!mhServerPipe) {
			return INVALID_HANDLE_VALUE;
		}
		mFirstPacket = TRUE;

		nCount = 0;
		events[nCount++] = mhStopEvent;
		events[nCount++] = mhServerPipe;

		WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(mhStopEvent, 0) == WAIT_OBJECT_0) {
			WLog_Print(logger_RPCEngine, WLOG_TRACE, "got shutdown signal");
			CloseHandle(mhServerPipe);
			mhServerPipe = INVALID_HANDLE_VALUE;
			return INVALID_HANDLE_VALUE;
		}

		if (WaitForSingleObject(mhServerPipe, 0) == WAIT_OBJECT_0) {
			BOOL fConnected;
			DWORD dwPipeMode;

			fConnected = ConnectNamedPipe(mhServerPipe, NULL);
			if (!fConnected) {
				fConnected = (GetLastError() == ERROR_PIPE_CONNECTED);
			}

			if (!fConnected) {
				WLog_Print(logger_RPCEngine, WLOG_ERROR, "could not connect client");
				CloseHandle(mhServerPipe);
				mhServerPipe = INVALID_HANDLE_VALUE;
				return INVALID_HANDLE_VALUE;
			}

			mhClientPipe = mhServerPipe;

			dwPipeMode = PIPE_WAIT;
			if (!SetNamedPipeHandleState(mhClientPipe, &dwPipeMode, NULL, NULL)) {
				WLog_Print(logger_RPCEngine, WLOG_ERROR, "SetNamedPipeHandleState failed! ");
				CloseHandle(mhServerPipe);
				mhServerPipe = INVALID_HANDLE_VALUE;
				return INVALID_HANDLE_VALUE;

			}
			WLog_Print(logger_RPCEngine, WLOG_TRACE, "connect client with handle %p", mhClientPipe);

			return mhClientPipe;
		}

		return INVALID_HANDLE_VALUE;
	}

	int RpcEngine::read() {
		if (mPacktLength > 0) {
			return readPayload();
		}

		return readHeader();
	}

	int RpcEngine::readHeader() {

		BOOL fSuccess;
		DWORD lpNumberOfBytesRead = 0;

		fSuccess = ReadFile(mhClientPipe, mHeaderBuffer + mHeaderRead,
				4 - mHeaderRead, &lpNumberOfBytesRead, NULL);

		if (!fSuccess || (lpNumberOfBytesRead == 0)) {
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "error reading header");
			return CLIENT_ERROR;
		}

		mHeaderRead += lpNumberOfBytesRead;

		if (mHeaderRead == 4) {
			mPacktLength = ntohl(*(DWORD *)mHeaderBuffer);
			WLog_Print(logger_RPCEngine, WLOG_TRACE, "header read, packet size %" PRIu32 "", mPacktLength);
		}
		return CLIENT_SUCCESS;
	}

	int RpcEngine::readPayload() {

		BOOL fSuccess;
		DWORD lpNumberOfBytesRead = 0;

		fSuccess = ReadFile(mhClientPipe, mPayloadBuffer + mPayloadRead,
				mPacktLength - mPayloadRead, &lpNumberOfBytesRead, NULL);

		if (!fSuccess || (lpNumberOfBytesRead == 0)) {
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "error reading payload");
			return CLIENT_ERROR;
		}

		mPayloadRead += lpNumberOfBytesRead;
		return CLIENT_SUCCESS;
	}

	int RpcEngine::processData() {

		mpbRPC.Clear();
		mpbRPC.ParseFromArray(mPayloadBuffer, mPayloadRead);

		uint32_t callID = mpbRPC.tag();
		uint32_t callType = mpbRPC.msgtype();

		if (mpbRPC.isresponse()) {
			// search the stored call to fill back answer

			callNS::CallOutPtr foundCallOut;
			std::list<callNS::CallOutPtr>::iterator it;

			for ( it = mAnswerWaitingQueue.begin(); it != mAnswerWaitingQueue.end(); it++) {
				callNS::CallOutPtr currentCallOut = (callNS::CallOutPtr)(*it);

				if (currentCallOut->getTag() == callID) {
					foundCallOut = currentCallOut;
					mAnswerWaitingQueue.remove(foundCallOut);
					break;
				}
			}

			if (foundCallOut == NULL) {
				WLog_Print(logger_RPCEngine, WLOG_ERROR,
					"Received answer for callID %" PRIu32 ", but no responding call was found",
					callID);
				return CLIENT_ERROR;
			}

			// fill the answer and signal
			if (mpbRPC.status() == RPCBase_RPCSTATUS_SUCCESS) {
				foundCallOut->setEncodedeResponse(mpbRPC.payload());
				foundCallOut->decodeResponse();
				foundCallOut->setResult(0);
			} else if (mpbRPC.status() == RPCBase_RPCSTATUS_FAILED) {
				foundCallOut->setErrorDescription(mpbRPC.errordescription());
				foundCallOut->setResult(1);
			} else if (mpbRPC.status() == RPCBase_RPCSTATUS_NOTFOUND) {
				foundCallOut->setResult(2);
			}
			return CLIENT_SUCCESS;
		}

		if (mFirstPacket) {
			if (!mpbRPC.has_versioninfo()) {
				return CLIENT_ERROR;
			}

			WLog_Print(logger_RPCEngine, WLOG_DEBUG, "received ogon version information %" PRIu32 ".%" PRIu32 "",
						mpbRPC.versioninfo().major(), mpbRPC.versioninfo().minor());

			UINT32 majorversion = mpbRPC.versioninfo().major();

			std::string serialized;

			mpbRPC.Clear();
			mpbRPC.set_isresponse(true);
			mpbRPC.set_tag(callID);
			mpbRPC.set_msgtype(-1);
			VersionInfo *info = mpbRPC.mutable_versioninfo();
			info->set_major(OGON_PROTOCOL_VERSION_MAJOR);
			info->set_minor(OGON_PROTOCOL_VERSION_MINOR);

			mpbRPC.set_status(RPCBase_RPCSTATUS_SUCCESS);
			mpbRPC.SerializeToString(&serialized);
			sendInternal(serialized);

			if (majorversion > OGON_PROTOCOL_VERSION_MAJOR) {
				return CLIENT_ERROR;
			}
			mFirstPacket = false;
			return CLIENT_SUCCESS;
		}

		callNS::CallPtr createdCall = callNS::CallPtr(CALL_FACTORY.createClass(callType));
		if (createdCall == NULL) {
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "no registered class for calltype=%" PRIu32 "", callType);
			sendError(callID, callType);
			return CLIENT_ERROR;
		}

		callNS::CallInPtr createdCallIn = boost::dynamic_pointer_cast<callNS::CallIn>(createdCall);
		if (createdCallIn) {
			// we got an CallIn object ... so handle it
			createdCallIn->setEncodedRequest(mpbRPC.payload());
			createdCallIn->setTag(callID);

			WLog_Print(logger_RPCEngine, WLOG_TRACE, "call upacked for callType=%" PRIu32 " and callID=%" PRIu32 "",
				callType, callID);

			createdCallIn->decodeRequest();
			if (!createdCallIn->prepare()) {
				//error could not prepare, sending error response
				APP_CONTEXT.getRpcOutgoingQueue()->addElement(createdCallIn);
			}
			return CLIENT_SUCCESS;
		}

		WLog_Print(logger_RPCEngine, WLOG_ERROR, "callobject had wrong baseclass, callType=%" PRIu32 "",
					callType);
		sendError(callID, callType);
		return CLIENT_ERROR;
	}

	int RpcEngine::send(ogon::sessionmanager::call::CallPtr call) {
		std::string serialized;

		callNS::CallInPtr callIn = boost::dynamic_pointer_cast<callNS::CallIn>(call);

		if (callIn) {
			// this is a CallIn
			// create answer
			mpbRPC.Clear();
			mpbRPC.set_isresponse(true);
			mpbRPC.set_tag(callIn->getTag());
			mpbRPC.set_msgtype(callIn->getCallType());

			if (call->getResult() != 0) {
				WLog_Print(logger_RPCEngine, WLOG_TRACE,
					"call for callType=%lu and callID=%" PRIu32 " failed, sending error response",
					callIn->getCallType(), callIn->getTag());
				mpbRPC.set_status(RPCBase_RPCSTATUS_FAILED);
				std::string errordescription = callIn->getErrorDescription();

				if (errordescription.size() > 0) {
					mpbRPC.set_errordescription(errordescription);
				}
			} else {
				WLog_Print(logger_RPCEngine, WLOG_TRACE,
					"call for callType=%lu and callID=%" PRIu32 " success, sending response",
					callIn->getCallType(), callIn->getTag());
				mpbRPC.set_status(RPCBase_RPCSTATUS_SUCCESS);
				mpbRPC.set_payload(callIn->getEncodedResponse());
			}

			mpbRPC.SerializeToString(&serialized);
			return sendInternal(serialized);
		}

		callNS::CallOutPtr callOut = boost::dynamic_pointer_cast<callNS::CallOut>(call);
		if (callOut) {
			// this is a CallOut
			// create answer
			mpbRPC.Clear();
			mpbRPC.set_isresponse(false);
			mpbRPC.set_tag(callOut->getTag());
			mpbRPC.set_msgtype(callOut->getCallType());
			mpbRPC.set_status(RPCBase_RPCSTATUS_SUCCESS);
			mpbRPC.set_payload(callOut->getEncodedRequest());

			mpbRPC.SerializeToString(&serialized);
			return sendInternal(serialized);
		} else {
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "callobject had wrong baseclass");
			return CLIENT_ERROR;
		}
	}

	int RpcEngine::sendError(uint32_t callID, uint32_t callType) {
		std::string serialized;

		mpbRPC.Clear();
		mpbRPC.set_isresponse(true);
		mpbRPC.set_tag(callID);
		mpbRPC.set_msgtype(callType);
		mpbRPC.set_status(RPCBase_RPCSTATUS_NOTFOUND);

		mpbRPC.SerializeToString(&serialized);

		return sendInternal(serialized);
	}

	int RpcEngine::sendInternal(const std::string &data) {

		BOOL fSuccess;
		DWORD lpNumberOfBytesWritten;
		DWORD messageSize = htonl(data.size());

		fSuccess = WriteFile(mhClientPipe, &messageSize,
				4, &lpNumberOfBytesWritten, NULL);

		if (!fSuccess || (lpNumberOfBytesWritten == 0)) {
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "error sending");
			return CLIENT_ERROR;
		}

		fSuccess = WriteFile(mhClientPipe, data.c_str(),
				data.size(), &lpNumberOfBytesWritten, NULL);

		if (!fSuccess || (lpNumberOfBytesWritten == 0)) {
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "error sending");
			return CLIENT_ERROR;
		}
		return CLIENT_SUCCESS;
	}

	void RpcEngine::resetStatus() {
		mPacktLength = 0;
		mHeaderRead = 0;
		mPayloadRead = 0;
	}

	BOOL RpcEngine::getOgonCredentials(PeerCredentials *credentials) const {
		int fd = GetEventFileDescriptor(mhClientPipe);
		if (fd < 0)
			return FALSE;

		return ogon_socket_credentials(fd, &credentials->uid, &credentials->haveUid,
											&credentials->pid, &credentials->havePid);
	}

	int RpcEngine::serveClient() {

		DWORD nCount;
		SignalingQueue<callNS::CallPtr> *outgoingQueue = APP_CONTEXT.getRpcOutgoingQueue();
		HANDLE queueHandle = outgoingQueue->getSignalHandle();
		HANDLE events[3];

		nCount = 0;
		events[nCount++] = mhStopEvent;
		events[nCount++] = mhClientPipe;
		events[nCount++] = queueHandle;

		int retValue = CLIENT_ERROR;

		while (1) {
			WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

			if (WaitForSingleObject(mhStopEvent, 0) == WAIT_OBJECT_0) {
				retValue = SERVER_SHUTDOWN;
				break;
			}

			if (WaitForSingleObject(mhClientPipe, 0) == WAIT_OBJECT_0) {
				retValue = read();

				if (retValue != CLIENT_SUCCESS) {
					break;
				}

				// process the data
				if (mPayloadRead == mPacktLength) {
					if ((retValue = processData()) != CLIENT_SUCCESS) {
						WLog_Print(logger_RPCEngine, WLOG_ERROR, "processData failed");
						break;
					}
					resetStatus();
				}
			}

			if (WaitForSingleObject(queueHandle, 0) == WAIT_OBJECT_0) {
				std::list<callNS::CallPtr> calls = outgoingQueue->getAllElements();
				std::list<callNS::CallPtr>::const_iterator iter;

				for(iter = calls.begin(); iter != calls.end();++iter) {
					callNS::CallPtr currentCall = *iter;

					if ((retValue = processOutgoingCall(currentCall)) != CLIENT_SUCCESS) {
						WLog_Print(logger_RPCEngine, WLOG_ERROR, "processOutgoingCall failed");
						break;
					}
				}
			}
		}

		CloseHandle(mhClientPipe);
		mhClientPipe = INVALID_HANDLE_VALUE;
		mhServerPipe = INVALID_HANDLE_VALUE;
		return retValue;
	}

	int RpcEngine::processOutgoingCall(ogon::sessionmanager::call::CallPtr call) {
		int retVal;

		callNS::CallOutPtr callOut = boost::dynamic_pointer_cast<callNS::CallOut>(call);
		if (callOut) {
			// this is a CallOut
			callOut->encodeRequest();
			callOut->setTag(mNextOutCall++);

			if ((retVal = send(call)) == CLIENT_SUCCESS) {
				mAnswerWaitingQueue.push_back(callOut);
				return retVal;
			}

			WLog_Print(logger_RPCEngine, WLOG_ERROR, "error sending call, informing call");
			callOut->setResult(1); // for failed
			return retVal;

		}

		callNS::CallInPtr callIn = boost::dynamic_pointer_cast<callNS::CallIn>(call);
		if (callIn) {
			// this is a async in call ... sending answer
			callIn->encodeResponse();
			retVal = send(callIn);
		} else {
			WLog_Print(logger_RPCEngine, WLOG_ERROR, "call had wrong type, dropping client connection!");
			return CLIENT_ERROR;
		}
		return retVal;
	}

} /*pbrpc*/ } /*ogon*/
