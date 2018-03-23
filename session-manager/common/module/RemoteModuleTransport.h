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

#ifndef _OGON_SMGR_REMOTEMODULETRANSPORT_H_
#define _OGON_SMGR_REMOTEMODULETRANSPORT_H_

#include "Module.h"
#include <string.h>
#include <pbRPC.pb.h>
#include <winpr/synch.h>


namespace ogon { namespace sessionmanager { namespace module {

	/**
	 * @brief
	 */

	#define REMOTE_CLIENT_BUFFER_SIZE	0xFFFF


	#define REMOTE_CLIENT_SUCCESS 0
	#define REMOTE_CLIENT_NEEDS_MORE_DATA 1
	#define REMOTE_CLIENT_ERROR 2
	#define REMOTE_CLIENT_CONTINUE 3
	#define REMOTE_CLIENT_ERROR_TIMEOUT 4

	enum TRANSPORT_STATE{
		READ_HEADER = 1,
		READ_PAYLOAD = 2,
		PROCESS_PAYLOAD = 3,
		ERROR_TRANSPORT = 4
	};


	class RemoteModuleTransportContext {
	public:
		RemoteModuleTransportContext();
		HANDLE mhRead;
		HANDLE mhWrite;
		BYTE mHeaderBuffer[4];
		UINT mHeaderRead;
		UINT mPacktLength;
		BYTE mPayloadBuffer[REMOTE_CLIENT_BUFFER_SIZE];
		UINT mPayloadRead;
		UINT32 mCurrentCallID;
		TRANSPORT_STATE mState;
		DWORD mLauncherpid;
	};


	class RemoteModuleTransport {
	public:
		RemoteModuleTransport();
		virtual ~RemoteModuleTransport();

	protected:

		UINT readHeader(RemoteModuleTransportContext &context, DWORD timeout = INFINITE);
		UINT readPayload(RemoteModuleTransportContext &context, DWORD timeout = INFINITE);
		UINT read(RemoteModuleTransportContext &context, DWORD timeout = INFINITE);
		UINT process(RemoteModuleTransportContext &context, void* customData);
		virtual UINT processCall(RemoteModuleTransportContext &context, const UINT32 callID, const UINT32 callType,
			const bool isResponse, const std::string &payload, void* customData) = 0;

		UINT serveOneCall(RemoteModuleTransportContext &context, void* customData, DWORD timeout = INFINITE);
		UINT serveOneRead(RemoteModuleTransportContext &context, void* customData, DWORD timeout = INFINITE);
		UINT writepbRpc(RemoteModuleTransportContext &context, const std::string &data, uint32_t callID, uint32_t callType, bool isResponse, bool success);
		UINT writeInternal(RemoteModuleTransportContext &context, const std::string &data);

		UINT32 getNextCallID();

	private:
		UINT32 mNextCallID;

	};

} /*module*/ } /*sessionmanager*/ } /*ogon*/

namespace moduleNS = ogon::sessionmanager::module;

#endif /* _OGON_SMGR_REMOTEMODULETRANSPORT_H_ */
