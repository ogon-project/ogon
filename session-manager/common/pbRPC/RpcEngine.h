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

#ifndef _OGON_SMGR_RPCENGINE_H_
#define _OGON_SMGR_RPCENGINE_H_

#include <winpr/synch.h>
#include <pbRPC.pb.h>
#include <call/Call.h>
#include <call/CallOut.h>
#include <list>

#define PIPE_BUFFER_SIZE	0xFFFF

namespace ogon { namespace pbrpc {

	/**
	 * @brief
	 */
	typedef struct {
		BOOL haveUid;
		BOOL havePid;
		pid_t pid;
		uid_t uid;
	} PeerCredentials;

	/**
	 * @brief
	 */
	class RpcEngine {
	public:
		RpcEngine();
		~RpcEngine();

		bool startEngine();
		bool stopEngine();

		HANDLE acceptClient();
		int serveClient();
		void resetStatus();

		BOOL getOgonCredentials(PeerCredentials *credentials) const;
	private:
		int createServerPipe(void);
		HANDLE createServerPipe(const char* endpoint);
		static void listenerThread(void* arg);
		int read();
		int readHeader();
		int readPayload();
		int processData();
		int send(ogon::sessionmanager::call::CallPtr call);
		int sendError(uint32_t callID, uint32_t callType);
		int sendInternal(const std::string &data);
		int processOutgoingCall(ogon::sessionmanager::call::CallPtr call);

	private:
		CRITICAL_SECTION mCSection;

		HANDLE mhClientPipe;
		HANDLE mhServerPipe;
		HANDLE mhServerThread;

		HANDLE mhStopEvent;

		DWORD mPacktLength;

		DWORD mHeaderRead;
		BYTE mHeaderBuffer[4];
		BOOL mFirstPacket;

		DWORD mPayloadRead;
		BYTE mPayloadBuffer[PIPE_BUFFER_SIZE];

		RPCBase mpbRPC;
		std::list<callNS::CallOutPtr> mAnswerWaitingQueue;

		long mNextOutCall;
	};

} /*pbrpc*/ } /*ogon*/

namespace pbRPC = ogon::pbrpc;

#endif /* _OGON_SMGR_RPCENGINE_H_ */
