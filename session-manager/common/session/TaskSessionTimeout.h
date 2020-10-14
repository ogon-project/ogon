/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task for session timeout
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

#ifndef _OGON_SMGR_SESSION_TASKSESSIONTIMEOUT_H_
#define _OGON_SMGR_SESSION_TASKSESSIONTIMEOUT_H_

#include <task/ThreadTask.h>

namespace ogon { namespace sessionmanager { namespace session {

	class TaskSessionTimeout: public taskNS::ThreadTask {
	public:
		virtual void run();
		virtual bool isThreaded();
	};

	typedef std::shared_ptr<TaskSessionTimeout> TaskSessionTimeoutPtr;

} /*session*/ } /*sessionmanager*/ } /*ogon*/

namespace sessionNS = ogon::sessionmanager::session;

#endif /* _OGON_SMGR_SESSION_TASKSESSIONTIMEOUT_H_ */
