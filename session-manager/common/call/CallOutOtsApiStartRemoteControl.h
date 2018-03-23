/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call StartRemoteControl (session manager to ogon)
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

#ifndef _OGON_SMGR_CALL_CALLOUTOTSAPISTARTREMOTECONTROL_H_
#define _OGON_SMGR_CALL_CALLOUTOTSAPISTARTREMOTECONTROL_H_

#include <string>
#include "CallOut.h"
#include <ICP.pb.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallOutOtsStartRemoteControl: public CallOut {
	public:
		CallOutOtsStartRemoteControl();
		virtual ~CallOutOtsStartRemoteControl();

		virtual unsigned long getCallType() const;

		virtual bool encodeRequest();
		virtual bool decodeResponse();

		void setConnectionId(UINT32 connectionId);
		void setTargetConnectionId(UINT32 connectionId);
		void setHotkeyVk(BYTE hotkey);
		void setHotkeyModifiers(UINT16 modifier);
		void setFlags(DWORD flags);

		bool isSuccess();

	private:
		UINT32 mConnectionId;
		UINT32 mTargetConnectionId;
		BYTE mHotkey;
		UINT16 mModifiers;
		DWORD mFlags;

		bool mSuccess;
	};

	typedef boost::shared_ptr<CallOutOtsStartRemoteControl> CallOutOtsStartRemoteControlPtr;

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_CALLOUTOTSAPISTARTREMOTECONTROL_H_ */
