/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Global process watcher
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

#ifdef __linux__
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#endif


#include "ProcessMonitor.h"

#include <appcontext/ApplicationContext.h>
#include <utils/CSGuard.h>

#include <winpr/platform.h>
#include <winpr/thread.h>
#include <winpr/wlog.h>

#include <session/TaskEnd.h>

namespace ogon { namespace sessionmanager { namespace process {

	static wLog *logger_ProcessMonitor = WLog_Get("ogon.sessionmanager.process.processmonitor");

	ProcessMonitor::ProcessMonitor() {
		if (!(mhStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL))) {
			WLog_Print(logger_ProcessMonitor, WLOG_FATAL,
				"Failed to create process monitor stop event");
			throw std::bad_alloc();
		}
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			WLog_Print(logger_ProcessMonitor, WLOG_FATAL,
				"Failed to initialize process monitor critical section");
			throw std::bad_alloc();
		}
		mhServerThread = NULL;
		mRunning = false;
	}

	ProcessMonitor::~ProcessMonitor() {
		DeleteCriticalSection(&mCSection);
		CloseHandle(mhStopEvent);
	}

	bool ProcessMonitor::start() {
		CSGuard guard(&mCSection);

		if (mRunning) {
			WLog_Print(logger_ProcessMonitor, WLOG_ERROR,
				"Process Monitor already started!");
			return false;
		}

		if (!(mhServerThread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) ProcessMonitor::execThread, (void*) this,
				0, NULL)))
		{
			WLog_Print(logger_ProcessMonitor, WLOG_ERROR, "failed to create thread");
			return false;
		}
		mRunning = true;
		return true;
	}

	bool ProcessMonitor::stop() {
		CSGuard guard(&mCSection);

		mRunning = false;
		if (mhServerThread) {
			SetEvent(mhStopEvent);
			WaitForSingleObject(mhServerThread, INFINITE);
			CloseHandle(mhServerThread);
			mhServerThread = NULL;
		} else {
			WLog_Print(logger_ProcessMonitor, WLOG_ERROR,
				"Executor was not started before.");
			return false;
		}
		return true;
	}

	void ProcessMonitor::run() {
		int status;
		bool erase;
		pid_t w;
		std::list<RDS_PROCESS_INFO>::iterator iter;
		std::map<UINT32, sessionNS::TaskEndPtr> endSessions;

		while (1) {
			EnterCriticalSection(&mCSection);
			iter = mProcessInfos.begin();

			while (iter != mProcessInfos.end()) {
				RDS_PROCESS_INFO info = *iter;
				erase = false;
				w = waitpid((pid_t) info.processId, &status, WNOHANG);

				if (w > 0) {
					WLog_Print(logger_ProcessMonitor, WLOG_TRACE, "Registered process %" PRIu32 " exited (status: %d)", info.processId, status);
					erase = true;
					if (info.terminateSessionOnExit) {
						// shutdown the session
						sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(info.sessionId);
						if (session) {
							if (!session->isCurrentModule(info.context)) {
								WLog_Print(logger_ProcessMonitor, WLOG_TRACE, "Shutdown was for previous module");
							} else {
								WLog_Print(logger_ProcessMonitor, WLOG_INFO, "s %" PRIu32 ": Process %" PRIu32 " exited, creating TaskEnd for session", info.sessionId, info.processId);
								sessionNS::TaskEndPtr end(new sessionNS::TaskEnd());
								end->setSessionId(info.sessionId);
								endSessions[info.sessionId] = end;
							}
						}
					}
				} else if (w == -1 && errno == ECHILD) {
					WLog_Print(logger_ProcessMonitor, WLOG_ERROR, "waitpid(%" PRIu32 ") failed (status: %d, errno=ECHILD) ", info.processId, status);
					erase = true;
				}

				if (erase) {
					mProcessInfos.erase(iter++);
				} else {
					++iter;
				}
			}

			LeaveCriticalSection(&mCSection);

			std::map<UINT32, sessionNS::TaskEndPtr>::iterator endSessionsIt;
			endSessionsIt = endSessions.begin();
			while (endSessionsIt != endSessions.end()) {
				WLog_Print(logger_ProcessMonitor, WLOG_TRACE, "adding end task for session %" PRIu32, endSessionsIt->first);
				sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(endSessionsIt->first);
				if (session) {
					session->addTask(endSessionsIt->second);
				} else {
					WLog_Print(logger_ProcessMonitor, WLOG_INFO, "session object for s %" PRIu32 " not found - ignoring", endSessionsIt->first);
				}
				++endSessionsIt;
			}
			endSessions.clear();

			if (WaitForSingleObject(mhStopEvent, 200) == WAIT_OBJECT_0) {
				break;
			}
		}
	}

	void ProcessMonitor::addProcess(DWORD processId, UINT32 sessionId, bool terminateSessionOnExit, RDS_MODULE_COMMON *context) {
		CSGuard guard(&mCSection);
		RDS_PROCESS_INFO info;
		info.processId = processId;
		info.sessionId = sessionId;
		info.terminateSessionOnExit = terminateSessionOnExit;
		info.context = context;
		mProcessInfos.push_back(info);
	}

	bool ProcessMonitor::removeProcess(DWORD processId) {
		CSGuard guard(&mCSection);
		std::list<RDS_PROCESS_INFO>::iterator iter;
		for(iter = mProcessInfos.begin(); iter != mProcessInfos.end(); iter++) {
			if ((*iter).processId == processId) {
				mProcessInfos.erase(iter);
				return true;
			}
		}
		return false;
	}

	void* ProcessMonitor::execThread(void *arg) {
		ProcessMonitor *monitor = (ProcessMonitor *) arg;

		WLog_Print(logger_ProcessMonitor, WLOG_INFO, "started ProcessMonitor thread");

		monitor->run();

		WLog_Print(logger_ProcessMonitor, WLOG_INFO, "stopped ProcessMonitor thread");
		return NULL;
	}


} /*task*/ } /*sessionmanager*/ } /*ogon*/
