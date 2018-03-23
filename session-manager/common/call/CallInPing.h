/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for ping rpc call (ogon to session manager)
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#ifndef _OGON_SMGR_CALL_CALLINPING_H_
#define _OGON_SMGR_CALL_CALLINPING_H_

#include "CallFactory.h"
#include <string>
#include "CallIn.h"
#include <ICP.pb.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallInPing: public CallIn {
	public:
		CallInPing();
		virtual ~CallInPing();

		virtual unsigned long getCallType() const;
		virtual bool decodeRequest();
		virtual bool encodeResponse();
		virtual bool doStuff();
	};

	FACTORY_REGISTER_DWORD(CallFactory, CallInPing, ogon::icp::Ping);

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_CALLINPING_H_ */
