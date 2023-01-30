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

#ifndef OGON_SMGR_INFORMABLETASK_H_
#define OGON_SMGR_INFORMABLETASK_H_

#include <task/Task.h>
#include <winpr/synch.h>

namespace ogon { namespace sessionmanager { namespace task {

	/**
	 *	@brief
	 */
	class InformableTask: public Task {
	public:
		InformableTask(){
			if (!(mhDone = CreateEvent(nullptr, TRUE, FALSE, nullptr))) {
				throw std::bad_alloc();
			}
		}
		virtual ~InformableTask() {
			CloseHandle(mhDone);
		}
		virtual void run() = 0;
		virtual void postProcess(){
			informDone();
		};

		virtual void informDone() {
			SetEvent(mhDone);
		}
		HANDLE getHandle() {
			return mhDone;
		}

		virtual void abortTask(){
			SetEvent(mhDone);
		};

	private:
		HANDLE mhDone;
	};

	typedef std::shared_ptr<InformableTask> InformableTaskPtr;

} /*task*/ } /*sessionmanager*/ } /*ogon*/

namespace taskNS = ogon::sessionmanager::task;

#endif /* OGON_SMGR_INFORMABLETASK_H_ */
