/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Transport helper class for remote modules
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

#include "RemoteModuleTransport.h"
#include <winpr/wlog.h>
#include <winpr/pipe.h>
#include <netinet/in.h>

#define BUF_SIZE 4096

namespace ogon { namespace sessionmanager { namespace module {

	static wLog *logger_RemoteModuleTransport = WLog_Get("ogon.sessionmanager.module.remotemoduletransport");

	RemoteModuleTransportContext::RemoteModuleTransportContext() {
		mhRead = NULL;
		mhWrite = NULL;
		mHeaderRead = 0;
		mPacktLength = 0;
		mPayloadRead = 0;
		mCurrentCallID = 0;
		mState = READ_HEADER;
		mLauncherpid = 0;
	};

	RemoteModuleTransport::RemoteModuleTransport() {
		mNextCallID = 1;
	}

	RemoteModuleTransport::~RemoteModuleTransport() {
	}

	UINT RemoteModuleTransport::readHeader(RemoteModuleTransportContext &context, DWORD timeout) {
		BOOL fSuccess;
		DWORD lpNumberOfBytesRead = 0;

		DWORD result = WaitForSingleObject(context.mhRead, timeout);

		switch (result) {
			case WAIT_OBJECT_0:
				break;
			case WAIT_TIMEOUT:
				WLog_Print(logger_RemoteModuleTransport, WLOG_ERROR,
						   "waiting for data failed, timed out!");
				return REMOTE_CLIENT_ERROR_TIMEOUT;
			case WAIT_FAILED:
			default:
				WLog_Print(logger_RemoteModuleTransport, WLOG_ERROR,
						   "waiting for data failed, aborting remot communication!");
				return REMOTE_CLIENT_ERROR;
		}

		fSuccess = ReadFile(context.mhRead, context.mHeaderBuffer + context.mHeaderRead,
							4 - context.mHeaderRead, &lpNumberOfBytesRead, NULL);

		if (!fSuccess || lpNumberOfBytesRead == 0) {
			WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "error reading header (read: %" PRIu32 " fSuccess: %" PRId32 ")",
					   lpNumberOfBytesRead, fSuccess);
			context.mState = ERROR_TRANSPORT;
			return REMOTE_CLIENT_ERROR;
		}

		context.mHeaderRead += lpNumberOfBytesRead;

		if (context.mHeaderRead == 4) {
			context.mPacktLength = ntohl(*(DWORD*)context.mHeaderBuffer);
			if (context.mPacktLength > REMOTE_CLIENT_BUFFER_SIZE) {
				WLog_Print(logger_RemoteModuleTransport, WLOG_ERROR, "packet length (%" PRIu32 ") exceeds maxbuffer size of %lu", context.mPacktLength, (unsigned long) REMOTE_CLIENT_BUFFER_SIZE);
				return REMOTE_CLIENT_ERROR;
			}
			/* WLog_Print(logger_RemoteModuleTransport, WLOG_TRACE, "header read, packet size %" PRIu32 "", context.mPacktLength); */
			context.mState = READ_PAYLOAD;
			return REMOTE_CLIENT_SUCCESS;
		}

		return REMOTE_CLIENT_NEEDS_MORE_DATA;
	}

	UINT RemoteModuleTransport::readPayload(RemoteModuleTransportContext &context, DWORD timeout) {
		BOOL fSuccess;
		DWORD lpNumberOfBytesRead = 0;

		DWORD result = WaitForSingleObject(context.mhRead, timeout);

		switch (result) {
			case WAIT_OBJECT_0:
				break;
			case WAIT_TIMEOUT:
				WLog_Print(logger_RemoteModuleTransport, WLOG_ERROR,
						   "waiting for data failed, timed out!");
				return REMOTE_CLIENT_ERROR_TIMEOUT;
			case WAIT_FAILED:
			default:
				WLog_Print(logger_RemoteModuleTransport, WLOG_ERROR,
						   "waiting for data failed, aborting remot communication!");
				return REMOTE_CLIENT_ERROR;
		}

		fSuccess = ReadFile(context.mhRead, context.mPayloadBuffer + context.mPayloadRead,
							context.mPacktLength - context.mPayloadRead, &lpNumberOfBytesRead, NULL);

		if (!fSuccess || lpNumberOfBytesRead == 0) {
			WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "error reading payload");
			context.mState = ERROR_TRANSPORT;
			return REMOTE_CLIENT_ERROR;
		}

		context.mPayloadRead += lpNumberOfBytesRead;
		if (context.mPayloadRead == context.mPacktLength) {
			//WLog_Print(logger_RemoteModuleTransport, WLOG_TRACE, "payload read");
			context.mState = PROCESS_PAYLOAD;
			return REMOTE_CLIENT_SUCCESS;
		}

		return REMOTE_CLIENT_NEEDS_MORE_DATA;
	}

	UINT RemoteModuleTransport::read(RemoteModuleTransportContext &context, DWORD timeout) {
		UINT error;

		error = readHeader(context, timeout);
		while (error ==  REMOTE_CLIENT_NEEDS_MORE_DATA)
		{
			error = readHeader(context, timeout);
			if (error == REMOTE_CLIENT_ERROR) {
				WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "read header failed!");
				return REMOTE_CLIENT_ERROR;
			}
			if (error == REMOTE_CLIENT_NEEDS_MORE_DATA) {
				Sleep(10);
			}
		}

		if (error != REMOTE_CLIENT_SUCCESS) {
			return error;
		}

		error = readPayload(context, timeout);
		while (error ==  REMOTE_CLIENT_NEEDS_MORE_DATA)
		{
			error = readPayload(context, timeout);
			if (error == REMOTE_CLIENT_ERROR) {
				WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "read payload failed!");
				return REMOTE_CLIENT_ERROR;
			}
			if (error == REMOTE_CLIENT_NEEDS_MORE_DATA) {
				Sleep(10);
			}
		}

		return error;
	}

	UINT RemoteModuleTransport::process(RemoteModuleTransportContext &context, void* customData) {
		ogon::pbrpc::RPCBase mpbRPC;
		UINT result = REMOTE_CLIENT_ERROR;

		mpbRPC.ParseFromArray(context.mPayloadBuffer, context.mPayloadRead);

		uint32_t callID = mpbRPC.tag();
		uint32_t callType = mpbRPC.msgtype();
		if (mpbRPC.status() == ogon::pbrpc::RPCBase_RPCSTATUS_SUCCESS) {
			context.mHeaderRead = 0;
			context.mPayloadRead = 0;
			context.mState = READ_HEADER;
			/* WLog_Print(logger_RemoteModuleTransport, WLOG_TRACE, "received %s %" PRIu32 " with id %" PRIu32 "", mpbRPC.isresponse() ? "answer" : "request", callType, callID); */
			result = processCall(context, callID, callType, mpbRPC.isresponse(), mpbRPC.payload(), customData);
		} else {
			WLog_Print(logger_RemoteModuleTransport, WLOG_ERROR, "RPC status was %s",
					   mpbRPC.status() == ogon::pbrpc::RPCBase_RPCSTATUS_FAILED ?
					   "RPCBase_RPCSTATUS_FAILED" : "RPCBase_RPCSTATUS_NOTFOUND");
		}
		return result;
	}

	UINT RemoteModuleTransport::serveOneRead(RemoteModuleTransportContext &context, void* customData, DWORD timeout) {
		UINT error = REMOTE_CLIENT_ERROR;

		if (context.mState == ERROR_TRANSPORT) {
			return REMOTE_CLIENT_ERROR;
		}

		if (context.mState == READ_HEADER) {
			error = readHeader(context, timeout);
			if (error != REMOTE_CLIENT_NEEDS_MORE_DATA && error != REMOTE_CLIENT_SUCCESS) {
				WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "error while reading!");
				return error;
			}
		}

		if (context.mState == READ_PAYLOAD) {
			error = readPayload(context, timeout);
			if (error != REMOTE_CLIENT_NEEDS_MORE_DATA && error != REMOTE_CLIENT_SUCCESS) {
				WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "error while reading!");
				return error;
			}
		}

		if (context.mState == PROCESS_PAYLOAD) {
			if ((error = process(context, customData)))
				WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "error while processing!");
		}

		return error;
	}


	UINT RemoteModuleTransport::serveOneCall(RemoteModuleTransportContext &context, void* customData, DWORD timeout) {
		UINT error;

		if (context.mState == ERROR_TRANSPORT) {
			return REMOTE_CLIENT_ERROR;
		}

		do {
			error = read(context, timeout);
			if (error && error != REMOTE_CLIENT_CONTINUE) {
				WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "error while reading!");
				return error;
			}

			if ((error = process(context, customData)) && error != REMOTE_CLIENT_CONTINUE && error < 1000)
				WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "error while processing!");

		} while (error == REMOTE_CLIENT_CONTINUE);

		return error;
	}

	UINT RemoteModuleTransport::writepbRpc(RemoteModuleTransportContext &context, const std::string &data,
										   uint32_t callID, uint32_t callType, bool isResponse, bool success) {
		std::string encodedRequest;

		if (context.mState == ERROR_TRANSPORT) {
			return REMOTE_CLIENT_ERROR;
		}

		ogon::pbrpc::RPCBase mpbRPC;
		mpbRPC.Clear();
		mpbRPC.set_tag(callID ? callID : getNextCallID());
		mpbRPC.set_isresponse(isResponse);
		mpbRPC.set_status(success ? ogon::pbrpc::RPCBase_RPCSTATUS_SUCCESS : ogon::pbrpc::RPCBase_RPCSTATUS_FAILED);
		mpbRPC.set_msgtype(callType);
		mpbRPC.set_payload(data);

		/* WLog_Print(logger_RemoteModuleTransport, WLOG_TRACE, "sent %s %" PRIu32 " with id %" PRIu32 "", isResponse ? "answer" : "request", callType, callID); */

		if (!mpbRPC.SerializeToString(&encodedRequest)) {
			// failed to serialize
			WLog_Print(logger_RemoteModuleTransport, WLOG_ERROR , "error serializing pbRpc!");
			return REMOTE_CLIENT_ERROR;
		}

		return writeInternal(context, encodedRequest);
	}

	UINT RemoteModuleTransport::writeInternal(RemoteModuleTransportContext &context, const std::string &data) {
		BOOL fSuccess;
		DWORD lpNumberOfBytesWritten;
		DWORD messageSize = htonl(data.size());

		if (context.mState == ERROR_TRANSPORT) {
			return REMOTE_CLIENT_ERROR;
		}

		fSuccess = WriteFile(context.mhWrite, &messageSize,
							 4, &lpNumberOfBytesWritten, NULL);

		if (!fSuccess || (lpNumberOfBytesWritten == 0)) {
			WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "error writing header data");
			context.mState = ERROR_TRANSPORT;
			return REMOTE_CLIENT_ERROR;
		}

		fSuccess = WriteFile(context.mhWrite, data.data(),
							 data.size(), &lpNumberOfBytesWritten, NULL);

		if (!fSuccess || (lpNumberOfBytesWritten == 0)) {
			WLog_Print(logger_RemoteModuleTransport, WLOG_DEBUG, "error writing payload data");
			context.mState = ERROR_TRANSPORT;
			return REMOTE_CLIENT_ERROR;
		}

		return REMOTE_CLIENT_SUCCESS;
	}

	UINT32 RemoteModuleTransport::getNextCallID() {
		mNextCallID++;
		if (mNextCallID == 0)
			mNextCallID++;
		return mNextCallID;
	}

} /*module*/ } /*sessionmanager*/ } /*ogon*/
