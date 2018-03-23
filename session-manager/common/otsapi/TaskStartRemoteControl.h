/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * otsapi task for starting remote control (shadowing)
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#ifndef _OGON_SMGR_OTSAPI_TASKSTARTREMOTECONTROL_H_
#define _OGON_SMGR_OTSAPI_TASKSTARTREMOTECONTROL_H_

#include <task/InformableTask.h>
#include <session/SessionAccessor.h>
#include <boost/enable_shared_from_this.hpp>

namespace ogon { namespace sessionmanager { namespace otsapi {

/**
 * @brief
 */
class TaskStartRemoteControl: public taskNS::InformableTask, sessionNS::SessionAccessor,
		public boost::enable_shared_from_this<TaskStartRemoteControl>
{
	public:
		TaskStartRemoteControl(UINT32 sessionID, UINT32 targetSession, BYTE HotkeyVk, INT16 HotkeyModifiers, DWORD flags, UINT32 timeout);
		~TaskStartRemoteControl();
		virtual void run();
		BOOL getResult();
		virtual void abortTask();
		BOOL startMessaging();
		static void* execMessagingThread(void *arg);

		virtual void informDone();

		void sendRemoteControlMessage();

	private:
		UINT32 mSessionID;
		UINT32 mTargetSession;
		BYTE mHotkeyVk;
		INT16 mHotkeyModifiers;
		BOOL mResult;
		UINT32 mTimeout;
		HANDLE mMessagingStarted;
		BOOL mSendMessage;
		DWORD mMessageResult;
		DWORD mStage;
		DWORD mFlags;
};

typedef boost::shared_ptr<TaskStartRemoteControl> TaskStartRemoteControlPtr;


} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/

namespace otsapiNS = ogon::sessionmanager::otsapi;

#endif /* _OGON_SMGR_OTSAPI_TASKSTARTREMOTECONTROL_H_ */
