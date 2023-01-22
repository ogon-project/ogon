/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * otsapi task for disconnecting a user
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

#ifndef OGON_SMGR_OTSAPI_TASKDISCONNECT_H_
#define OGON_SMGR_OTSAPI_TASKDISCONNECT_H_

#include <task/InformableTask.h>
#include <session/Connection.h>
#include <session/SessionAccessor.h>

namespace ogon { namespace sessionmanager { namespace otsapi {

	class TaskDisconnect: public taskNS::InformableTask, sessionNS::SessionAccessor{
	public:
		TaskDisconnect(UINT32 sessionId, BOOL wait, DWORD timeout);
		virtual ~TaskDisconnect(){};
		virtual void run();
		BOOL getResults();
		virtual void abortTask();

	private:
		BOOL disconnectSession();
		UINT32 mSessionId;
		BOOL mWait;
		DWORD mTimeout;
		bool mDisconnected;
	};

	typedef std::shared_ptr<TaskDisconnect> TaskDisconnectPtr;

} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/

namespace otsapiNS = ogon::sessionmanager::otsapi;

#endif /* OGON_SMGR_OTSAPI_TASKDISCONNECT_H_ */
