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

#ifndef OGON_SMGR_PROCESSMONITOR_H_
#define OGON_SMGR_PROCESSMONITOR_H_

#include <ogon/module.h>
#include <winpr/wtypes.h>
#include <winpr/synch.h>
#include <list>

namespace ogon { namespace sessionmanager { namespace process {

	/**
	 * @brief
	 */
	struct _RDS_PROCESS_INFO {
		DWORD processId;
		UINT32 sessionId;
		bool terminateSessionOnExit;
		RDS_MODULE_COMMON *context;
	};

	typedef struct _RDS_PROCESS_INFO RDS_PROCESS_INFO;

	/**
	 * @brief
	 */
	class ProcessMonitor {
	public:
		ProcessMonitor();
		~ProcessMonitor();

		bool start();
		bool stop();

		void run();

		void addProcess(DWORD processId, UINT32 sessionId, bool terminateSessionOnExit, RDS_MODULE_COMMON *context);
		bool removeProcess(DWORD processId);


	private:
		static void* execThread(void *arg);

	private:
		CRITICAL_SECTION mCSection;
		HANDLE mhStopEvent;
		HANDLE mhServerThread;
		bool mRunning;
		std::list<RDS_PROCESS_INFO> mProcessInfos;
	};

} /*process*/ } /*sessionmanager*/ } /*ogon*/

namespace processNS = ogon::sessionmanager::process;

#endif /* OGON_SMGR_PROCESSMONITOR_H_ */
