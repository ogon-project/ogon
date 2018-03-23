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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <call/CallOutOtsApiStartRemoteControl.h>
#include <appcontext/ApplicationContext.h>

using ogon::icp::OtsApiStartRemoteControlRequest;
using ogon::icp::OtsApiStartRemoteControlResponse;

namespace ogon { namespace sessionmanager { namespace call {

	CallOutOtsStartRemoteControl::CallOutOtsStartRemoteControl() :
		mConnectionId(0),
		mTargetConnectionId(0),
		mHotkey(0),
		mModifiers(0),
		mSuccess(false),
		mFlags(0)
	{
	}

	CallOutOtsStartRemoteControl::~CallOutOtsStartRemoteControl() {
	}

	unsigned long CallOutOtsStartRemoteControl::getCallType() const {
		return ogon::icp::OtsApiStartRemoteControl;
	}


	bool CallOutOtsStartRemoteControl::encodeRequest(){
		OtsApiStartRemoteControlRequest req;
		req.set_connectionid(mConnectionId);
		req.set_targetconnectionid(mTargetConnectionId);
		req.set_hotkeyvk(mHotkey);
		req.set_hotkeymodifiers(mModifiers);
		req.set_flags(mFlags);

		if (!req.SerializeToString(&mEncodedRequest)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallOutOtsStartRemoteControl::decodeResponse() {
		OtsApiStartRemoteControlResponse resp;

		if (!resp.ParseFromString(mEncodedResponse)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}
		mSuccess = resp.success();
		return true;
	}


	void CallOutOtsStartRemoteControl::setConnectionId(UINT32 connectionId) {
		mConnectionId = connectionId;
	}

	void CallOutOtsStartRemoteControl::setTargetConnectionId(UINT32 connectionId) {
		mTargetConnectionId = connectionId;
	}

	void CallOutOtsStartRemoteControl::setHotkeyVk(BYTE hotkey) {
		mHotkey = hotkey;
	}

	void CallOutOtsStartRemoteControl::setHotkeyModifiers(UINT16 modifier) {
		mModifiers = modifier;
	}

	bool CallOutOtsStartRemoteControl::isSuccess() {
		return mSuccess;
	}

	void CallOutOtsStartRemoteControl::setFlags(DWORD flags) {
		mFlags = flags;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
