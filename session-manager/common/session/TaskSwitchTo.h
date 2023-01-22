/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task for switching to another session
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

#ifndef OGON_SMGR_SESSION_TASKSWITCHTO_H_
#define OGON_SMGR_SESSION_TASKSWITCHTO_H_

#include <task/InformableTask.h>
#include <session/Connection.h>
#include <session/SessionAccessor.h>

namespace ogon { namespace sessionmanager { namespace session {

	class TaskSwitchTo: public taskNS::InformableTask, sessionNS::SessionAccessor{
	public:
		TaskSwitchTo(UINT32 connectionId, UINT32 sessionId);
		virtual ~TaskSwitchTo(){};
		virtual void run();
		UINT32 getResults(bool &disconnected);
		virtual void abortTask();

	private:
		UINT32 mConnectionId;
		UINT32 mSessionId;
		bool mSuccess;
		UINT32 mResult;
	};

	typedef std::shared_ptr<TaskSwitchTo> TaskSwitchToPtr;

} /*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* OGON_SMGR_SESSION_TASKSWITCHTO_H_ */
