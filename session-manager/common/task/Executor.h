/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Global Executor which runs Task objects
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

#ifndef OGON_SMGR_EXECUTOR_H_
#define OGON_SMGR_EXECUTOR_H_

#include "Task.h"
#include <utils/SignalingQueue.h>
#include <list>

#define PIPE_BUFFER_SIZE	0xFFFF

namespace ogon { namespace sessionmanager { namespace task {

	class Executor {
	public:
		Executor();
		~Executor();

		bool start();
		bool stop();

		void runExecutor();

		bool addTask(TaskPtr task);


	private:
		static void* execThread(void *arg);
		static void* execTask(void *arg);

		bool checkThreadHandles(const HANDLE value) const;
		bool waitThreadHandles(const HANDLE value) const;

	private:
		HANDLE mhStopEvent;
		HANDLE mhServerThread;

		HANDLE mhStopThreads;
		HANDLE mhTaskThreadStarted;

		bool mRunning;

		std::list<HANDLE> mTaskThreadList;
		CRITICAL_SECTION mCSection;
		SignalingQueue<TaskPtr> mTasks;
	};

} /*task*/ } /*sessionmanager*/ } /*ogon*/

namespace taskNS = ogon::sessionmanager::task;

#endif /* OGON_SMGR_EXECUTOR_H_ */
