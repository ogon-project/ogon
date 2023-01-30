/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * call task for stopping remote control (shadowing)
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
#include <appcontext/ApplicationContext.h>

namespace ogon { namespace sessionmanager { namespace call {

TaskEndRemoteControl::TaskEndRemoteControl(UINT32 connectionId) : mResult(false) {
	mConnectionId = connectionId;
}

void TaskEndRemoteControl::run() {
	sessionNS::ConnectionPtr currentConnection = APP_CONTEXT.getConnectionStore()->getConnection(mConnectionId);
	if ((currentConnection == nullptr) ||
			(currentConnection->getSessionId() == 0)) {
		mResult = false;
		return;
	}

	sessionNS::SessionPtr currentSession = APP_CONTEXT.getSessionStore()->getSession(currentConnection->getSessionId());

	if (!currentSession) {
		mResult = false;
		return;
	}
	setAccessorSession(currentSession);
	stopRemoteControl();
	mResult = true;
}

bool TaskEndRemoteControl::getResult() {
	return mResult;
}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
