/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task for shutting down a session (internally used by session)
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

#ifndef OGON_SMGR_SESSION_TASKSHUTDOWN_H_
#define OGON_SMGR_SESSION_TASKSHUTDOWN_H_

#include <task/InformableTask.h>
#include <session/Connection.h>
#include <session/SessionAccessor.h>

namespace ogon { namespace sessionmanager { namespace session {

	class TaskShutdown: public taskNS::Task, sessionNS::SessionAccessor{
	public:
		TaskShutdown(sessionNS::SessionPtr session);
		virtual ~TaskShutdown(){};
		virtual void run();
	};

	typedef std::shared_ptr<TaskShutdown> TaskShutdownPtr;

} /*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* OGON_SMGR_SESSION_TASKSHUTDOWN_H_ */
