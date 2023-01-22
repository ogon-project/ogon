/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * call task for Logging on a user.
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

#include "TaskCallInLogonUser.h"
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>


namespace ogon { namespace sessionmanager { namespace call {

	void TaskCallInLogonUser::run() {
		if (mCall == nullptr) {
			return;
		}
		mCall->doStuff();
		APP_CONTEXT.getRpcOutgoingQueue()->addElement(mCall);
		return;
	}

	void TaskCallInLogonUser::setCallIn(CallInPtr call) {
		mCall = call;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
