/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Task Object for an Executor
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

#ifndef _OGON_SMGR_TASK_H_
#define _OGON_SMGR_TASK_H_

#include <memory>
#include <winpr/synch.h>

namespace ogon { namespace sessionmanager { namespace task {

	class Task {
	public:
		Task(){}
		virtual ~Task() {}
		virtual void run() = 0;
		virtual void preProcess(){};
		virtual void postProcess(){};
		virtual void abortTask(){};
	};

	typedef std::shared_ptr<Task> TaskPtr;

} /*task*/ } /*sessionmanager*/ } /*ogon*/

namespace taskNS = ogon::sessionmanager::task;

#endif /* _OGON_SMGR_TASK_H_ */
