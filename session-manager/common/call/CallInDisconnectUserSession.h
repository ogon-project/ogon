/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call DisconnectUserSession (ogon to session manager)
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

#ifndef OGON_SMGR_CALL_CALLINDISCONNECTUSERSESSION_H_
#define OGON_SMGR_CALL_CALLINDISCONNECTUSERSESSION_H_

#include "CallFactory.h"
#include <string>
#include "CallIn.h"
#include <ICP.pb.h>
#include <session/Session.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallInDisconnectUserSession: public CallIn {
	public:
		CallInDisconnectUserSession();
		virtual ~CallInDisconnectUserSession();

		virtual unsigned long getCallType() const;
		virtual bool decodeRequest();
		virtual bool encodeResponse();
		virtual bool prepare();
		virtual bool doStuff();

	private:

		UINT32 mConnectionId;
		bool mDisconnected;
	};

	FACTORY_REGISTER_DWORD(CallFactory, CallInDisconnectUserSession, ogon::icp::DisconnectUserSession);

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* OGON_SMGR_CALL_CALLINDISCONNECTUSERSESSION_H_ */
