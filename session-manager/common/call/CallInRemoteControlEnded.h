/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Call sent from ogon when shadowing was stopped for a connection
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

#ifndef OGON_SMGR_CALL_CALLINREMOTECONTROLENDED_H_
#define OGON_SMGR_CALL_CALLINREMOTECONTROLENDED_H_

#include "CallFactory.h"
#include "CallIn.h"
#include <ICP.pb.h>

namespace ogon {
namespace sessionmanager {
namespace call {

/**
 * @brief
 */
class CallInRemoteControlEnded: public CallIn {
	public:
		CallInRemoteControlEnded();
		virtual ~CallInRemoteControlEnded();

		virtual unsigned long getCallType() const;
		virtual bool decodeRequest();
		virtual bool prepare();
		virtual bool doStuff();
		virtual bool encodeResponse();
	private:
		UINT32 mConnectionId;
		UINT32 mConnectionIdTarget;
		bool mSuccess;
};

FACTORY_REGISTER_DWORD(CallFactory, CallInRemoteControlEnded, ogon::icp::RemoteControlEnded);

} /*call*/
} /*sessionmanager*/
} /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* OGON_SMGR_CALL_CALLINREMOTECONTROLENDED_H_ */
