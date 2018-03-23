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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "TaskEndRemoteControl.h"
#include "CallInRemoteControlEnded.h"
#include <appcontext/ApplicationContext.h>

using ogon::icp::RemoteControlEndedRequest;
using ogon::icp::RemoteControlEndedResponse;

namespace ogon {
namespace sessionmanager {
namespace call {

CallInRemoteControlEnded::CallInRemoteControlEnded() {
	mSuccess = false;
	mConnectionId = 0;
	mConnectionIdTarget = 0;
}

CallInRemoteControlEnded::~CallInRemoteControlEnded() {
}

unsigned long CallInRemoteControlEnded::getCallType() const {
	return ogon::icp::RemoteControlEnded;
}

bool CallInRemoteControlEnded::decodeRequest() {
	// decode protocol buffers
	RemoteControlEndedRequest req;

	if (!req.ParseFromString(mEncodedRequest)) {
		// failed to parse
		mResult = 1;// will report error with answer
		return false;
	}
	mConnectionId = req.spyid();
	mConnectionIdTarget = req.spiedid();
	return true;
}

bool CallInRemoteControlEnded::prepare() {
	if (!putInSessionExecutor_conId(mConnectionId)) {
		mSuccess = false;
		return false;
	}
	return true;
}

bool CallInRemoteControlEnded::encodeResponse() {
	// encode protocol buffers
	RemoteControlEndedResponse resp;
	resp.set_success(mSuccess);

	if (!resp.SerializeToString(&mEncodedResponse)) {
		// failed to serialize
		mResult = 1;
		return false;
	}
	return true;
}

bool CallInRemoteControlEnded::doStuff() {
	TaskEndRemoteControl endRemoteControlTask(mConnectionId);
	endRemoteControlTask.run();
	mSuccess = endRemoteControlTask.getResult();
	return true;
}

} /*call*/
} /*sessionmanager*/
} /*ogon*/
