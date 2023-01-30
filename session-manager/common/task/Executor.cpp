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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Executor.h"

#ifdef __linux__
#include <signal.h>
#endif
#include <appcontext/ApplicationContext.h>
#include <utils/CSGuard.h>
#include <task/ThreadTask.h>

#include <winpr/platform.h>
#include <winpr/thread.h>
#include <winpr/wlog.h>

#include <boost/pointer_cast.hpp>


namespace ogon { namespace sessionmanager { namespace task {

	static wLog *logger_Executor = WLog_Get("ogon.sessionmanager.task.Executor");

	Executor::Executor() {
		mhStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		mhTaskThreadStarted = CreateEvent(nullptr, TRUE, FALSE, nullptr);
		mhStopThreads = CreateEvent(nullptr, TRUE, FALSE, nullptr);

		if (!mhStopEvent || !mhTaskThreadStarted || !mhStopThreads) {
			WLog_Print(logger_Executor, WLOG_FATAL,
				"Failed to create executor events");
			throw std::bad_alloc();
		}
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_Executor, WLOG_FATAL,
				"Failed to initialize executor critical section");
			throw std::bad_alloc();
		}
		mhServerThread = nullptr;
		mRunning = false;
	}

	Executor::~Executor() {
		DeleteCriticalSection(&mCSection);
		CloseHandle(mhStopEvent);
		CloseHandle(mhTaskThreadStarted);
		CloseHandle(mhStopThreads);
	}

	bool Executor::start() {
		CSGuard guard(&mCSection);

		if (mRunning) {
			WLog_Print(logger_Executor, WLOG_ERROR,
				"Executor Thread already started!");
			return false;
		}

		if (!(mhServerThread = CreateThread(nullptr, 0,
					  (LPTHREAD_START_ROUTINE)Executor::execThread,
					  (void *)this, 0, nullptr))) {
			WLog_Print(logger_Executor, WLOG_ERROR, "failed to create thread");
			return false;
		}
		mRunning = true;
		return true;
	}

	bool Executor::stop() {
		CSGuard guard(&mCSection);

		mRunning = false;
		if (mhServerThread) {
			SetEvent(mhStopEvent);
			WaitForSingleObject(mhServerThread, INFINITE);
			CloseHandle(mhServerThread);
			mhServerThread = nullptr;
		} else {
			WLog_Print(logger_Executor, WLOG_ERROR,
				"Executor was not started before.");
			return false;
		}
		return true;
	}

	void Executor::runExecutor() {
		DWORD nCount;
		HANDLE queueHandle = mTasks.getSignalHandle();
		HANDLE events[2];

		nCount = 0;
		events[nCount++] = mhStopEvent;
		events[nCount++] = queueHandle;

		while (1) {
			WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

			if (WaitForSingleObject(mhStopEvent, 0) == WAIT_OBJECT_0) {
				SetEvent(mhStopThreads);
				// shut down all threads
				mTaskThreadList.erase(
					remove_if(mTaskThreadList.begin(), mTaskThreadList.end(),
						std::bind1st( std::mem_fun( &Executor::waitThreadHandles), this))
					, mTaskThreadList.end());
				break;
			}

			if (WaitForSingleObject(queueHandle, 0) == WAIT_OBJECT_0) {
				std::list<TaskPtr> currentTasks = mTasks.getAllElements();
				std::list<TaskPtr>::const_iterator iter;
				for(iter = currentTasks.begin(); iter != currentTasks.end(); ++iter) {
					TaskPtr currentTask = *iter;
					ThreadTaskPtr threadTask = std::dynamic_pointer_cast<ThreadTask>(currentTask);
					if (threadTask) {
						// start Task as thread
						threadTask->setHandles(mhStopThreads, mhTaskThreadStarted);
						HANDLE taskThread = CreateThread(nullptr, 0,
								(LPTHREAD_START_ROUTINE)Executor::execTask,
								(void *)&currentTask, 0, nullptr);
						if (!taskThread) {
							WLog_Print(logger_Executor, WLOG_ERROR, "failed to create task thread");
							// dont abort the whole Sessionmanager, just signal the failure
							// of the task
							currentTask->abortTask();
							currentTask.reset();
							continue;
						}
						mTaskThreadList.push_back(taskThread);
						WaitForSingleObject(mhTaskThreadStarted, INFINITE);
						ResetEvent(mhTaskThreadStarted);
						currentTask.reset();
					} else {
						currentTask->preProcess();
						currentTask->run();
						currentTask->postProcess();
						currentTask.reset();
					}
				}
				mTaskThreadList.erase(
					remove_if(mTaskThreadList.begin(), mTaskThreadList.end(),
						std::bind1st( std::mem_fun( &Executor::checkThreadHandles), this)),
					mTaskThreadList.end());
			}
		}
	}

	bool Executor::addTask(TaskPtr task) {
		CSGuard guard(&mCSection);
		if (mRunning) {
			mTasks.addElement(task);
			return true;
		} else {
			return false;
		}
	}

	void* Executor::execThread(void *arg) {
		Executor *executor;

		executor = (Executor *) arg;
		WLog_Print(logger_Executor, WLOG_INFO, "started Executor thread");

		executor->runExecutor();

		WLog_Print(logger_Executor, WLOG_INFO, "stopped Executor thread");
		return nullptr;
	}

	void* Executor::execTask(void *arg) {
		ThreadTaskPtr *taskptr = static_cast<ThreadTaskPtr*>(arg);
		ThreadTaskPtr task = *taskptr;

		task->preProcess();
		WLog_Print(logger_Executor, WLOG_TRACE, "started Task thread");
		task->run();
		WLog_Print(logger_Executor, WLOG_TRACE, "stopped Task thread");
		task->postProcess();
		return nullptr;
	}


	bool Executor::checkThreadHandles(const HANDLE value) const {
		if (WaitForSingleObject(value,0) == WAIT_OBJECT_0) {
			// thread exited ...
			CloseHandle(value);
			return true;
		}
		return false;
	}

	bool Executor::waitThreadHandles(const HANDLE value) const {
		if (WaitForSingleObject(value,INFINITE) == WAIT_OBJECT_0) {
			// thread exited ...
			CloseHandle(value);
			return true;
		}
		return false;
	}

} /*task*/ } /*sessionmanager*/ } /*ogon*/
