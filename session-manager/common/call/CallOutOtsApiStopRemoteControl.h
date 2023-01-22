/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call StopRemoteControl (session manager to ogon)
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

#ifndef OGON_SMGR_CALL_CALLOUTOTSAPISTOPREMOTECONTROL_H_
#define OGON_SMGR_CALL_CALLOUTOTSAPISTOPREMOTECONTROL_H_

#include <string>
#include "CallOut.h"
#include <ICP.pb.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallOutOtsApiStopRemoteControl: public CallOut {
	public:
		CallOutOtsApiStopRemoteControl();
		virtual ~CallOutOtsApiStopRemoteControl();

		virtual unsigned long getCallType() const;

		virtual bool encodeRequest();
		virtual bool decodeResponse();

		void setConnectionId(UINT32 connectionId);
		bool isSuccess();

	private:
		UINT32 mConnectionId;
		bool mSuccess;
	};

	typedef std::shared_ptr<CallOutOtsApiStopRemoteControl> CallOutOtsApiStopRemoteControlPtr;

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* OGON_SMGR_CALL_CALLOUTOTSAPISTOPREMOTECONTROL_H_ */
