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

#ifndef _OGON_SMGR_CALL_TASKENDREMOTECONTROL_H_
#define _OGON_SMGR_CALL_TASKENDREMOTECONTROL_H_

#include <task/Task.h>
#include <session/SessionAccessor.h>
#include <string>

namespace ogon { namespace sessionmanager { namespace call {

class TaskEndRemoteControl : public taskNS::Task, sessionNS::SessionAccessor {

	public:
		TaskEndRemoteControl(UINT32 connectionId);
		virtual void run();
		bool getResult();

	private:
		UINT32 mConnectionId;
		bool mResult;

};

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_ENDREMOTECONTROL_H_ */
