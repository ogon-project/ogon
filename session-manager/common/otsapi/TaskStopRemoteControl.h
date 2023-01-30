/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * otsapi task for stopping remote control (shadowing)
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

#ifndef OGON_SMGR_OTSAPI_TASKSTOPREMOTECONTROL_H_
#define OGON_SMGR_OTSAPI_TASKSTOPREMOTECONTROL_H_

#include <task/InformableTask.h>
#include <session/SessionAccessor.h>

namespace ogon { namespace sessionmanager { namespace otsapi {

	class TaskStopRemoteControl: public taskNS::InformableTask, sessionNS::SessionAccessor {
	public:
		TaskStopRemoteControl(UINT32 sessionId, UINT32 timeout);
		virtual ~TaskStopRemoteControl(){};
		virtual void run();
		BOOL getResults();
		virtual void abortTask();

	private:
		UINT32 mSessionId;
		BOOL mResult;
		UINT32 mTimeOut;
	};

	typedef std::shared_ptr<TaskStopRemoteControl> TaskStopRemoteControlPtr;

} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/

namespace otsapiNS = ogon::sessionmanager::otsapi;

#endif /* OGON_SMGR_OTSAPI_TASKSTOPREMOTECONTROL_H_ */
