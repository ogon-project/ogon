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

#ifndef OGON_SMGR_CALL_TASKCALLINLOGONUSER_H_
#define OGON_SMGR_CALL_TASKCALLINLOGONUSER_H_

#include <task/ThreadTask.h>
#include <call/CallIn.h>
#include <string>

namespace ogon { namespace sessionmanager { namespace call {

	class TaskCallInLogonUser: public taskNS::ThreadTask {
	public:
		virtual void run();
		void setCallIn(CallInPtr call);

	private:
		CallInPtr mCall;
	};

	typedef std::shared_ptr<TaskCallInLogonUser> TaskCallInLogonUserPtr;

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* OGON_SMGR_CALL_TASKCALLINLOGONUSER_H_ */
