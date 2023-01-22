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

#ifndef _OGON_SMGR_THREADTASK_H_
#define _OGON_SMGR_THREADTASK_H_

#include <task/Task.h>

namespace ogon { namespace sessionmanager { namespace task {

	class ThreadTask: public Task {
	public:
	 ThreadTask() : mhStarted(nullptr), mhStop(nullptr) {}
	 virtual ~ThreadTask() {}
	 virtual void run() = 0;

	 void setHandles(HANDLE stopHandle, HANDLE startedHandle) {
		 mhStop = stopHandle;
		 mhStarted = startedHandle;
		}

		virtual void preProcess(){
			informStarted();
		};

		void informStarted() {
			SetEvent(mhStarted);
		}

	private:
		HANDLE mhStarted;
	protected:
		HANDLE mhStop;
	};

	typedef std::shared_ptr<ThreadTask> ThreadTaskPtr;

} /*task*/ } /*sessionmanager*/ } /*ogon*/

namespace taskNS = ogon::sessionmanager::task;

#endif /* _OGON_SMGR_THREADTASK_H_ */
