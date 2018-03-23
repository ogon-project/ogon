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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>

#include <otsapi/TaskStartRemoteControl.h>
#include <appcontext/ApplicationContext.h>
#include <call/CallOutOtsApiStartRemoteControl.h>
#include <call/CallOutMessage.h>
#include <winpr/user.h>


namespace ogon { namespace sessionmanager { namespace otsapi {

static wLog *logger_taskStartRemoteControl = WLog_Get("ogon.sessionmanager.otsapi.taskstartremotecontrol");


TaskStartRemoteControl::TaskStartRemoteControl(UINT32 sessionID, UINT32 targetSession, BYTE HotkeyVk, INT16 HotkeyModifiers, DWORD flags, UINT32 timeout) {
	if (!(mMessagingStarted = CreateEvent(NULL, TRUE, FALSE, NULL))) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR,
				   "Failed to create task remote control messaging started event");
		throw std::bad_alloc();
	}
	mSessionID = sessionID;
	mTargetSession = targetSession;
	mHotkeyVk = HotkeyVk;
	mHotkeyModifiers = HotkeyModifiers;
	mTimeout = timeout;
	mResult = false;
	mSendMessage = false;
	mMessageResult = 0;
	mStage = 0;
	mFlags = flags;
}

TaskStartRemoteControl::~TaskStartRemoteControl() {
	CloseHandle(mMessagingStarted);
}

BOOL TaskStartRemoteControl::startMessaging() {

	HANDLE messagingThread;
	TaskStartRemoteControlPtr currentTask;
	currentTask = shared_from_this();

	if (!(messagingThread = CreateThread(NULL, 0,
			(LPTHREAD_START_ROUTINE) TaskStartRemoteControl::execMessagingThread, (void*) &currentTask,
			0, NULL)))
	{
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR, "s %" PRIu32 ": error creating thread", mSessionID);
		return false;
	}

	WaitForSingleObject(mMessagingStarted, INFINITE);
	return TRUE;
}

void* TaskStartRemoteControl::execMessagingThread(void *arg) {
	TaskStartRemoteControlPtr currentTask = * static_cast<TaskStartRemoteControlPtr*>(arg);

	currentTask->sendRemoteControlMessage();

	return NULL;
}


void TaskStartRemoteControl::informDone() {
	if (!mSendMessage) {
		InformableTask::informDone();
	}
}

void TaskStartRemoteControl::sendRemoteControlMessage() {

	DWORD status;
	UINT32 connectionId = 0;
	callNS::CallOutMessagePtr messageCall;

	SetEvent(mMessagingStarted);
	sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(mSessionID);
	if (!session) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR, "s %" PRIu32 ": No session found!", mSessionID);
		mMessageResult = 0;
		goto out;
	}

	connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mTargetSession);
	if (connectionId == 0)  {
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR,
			"s %" PRIu32 ": could not find connecitonId!", mTargetSession);
		mMessageResult = 0;
		goto out;
	}

	messageCall = callNS::CallOutMessagePtr(new callNS::CallOutMessage());
	messageCall->setConnectionId(connectionId);
	messageCall->setType(MESSAGE_REQUEST_REMOTE_CONTROL);
	messageCall->setParameterNumber(2);
	messageCall->setParameter1(session->getUserName());
	messageCall->setTimeout(30);
	messageCall->setStyle(MB_YESNO);


	APP_CONTEXT.getRpcOutgoingQueue()->addElement(messageCall);

	status = WaitForSingleObject(messageCall->getAnswerHandle(), 31 * 1000);
	if (status == WAIT_TIMEOUT) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_TRACE, "s %" PRIu32 ": Message timed out", mSessionID);
		mMessageResult = IDTIMEOUT;
		goto out;

	}

	if (messageCall->getResult() == 0) {
		// result 0 is only sent if timeout has appeared too early
		mMessageResult = IDTIMEOUT;
		goto out;
	}
	mMessageResult =  messageCall->getResult();

out:
	session = APP_CONTEXT.getSessionStore()->getSession(mSessionID);
	if (session) {
		session->addTask(shared_from_this());
	} else {
		abortTask();
	}
}

void TaskStartRemoteControl::run() {

	bool remoteControlRequest = false;
	mSendMessage = false;
	mResult = false;
	sessionNS::SessionPtr session = APP_CONTEXT.getSessionStore()->getSession(mSessionID);
	if (!session) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR, "s %" PRIu32 ": No session found!", mSessionID);
		return;
	}

	sessionNS::SessionPtr targetSession = APP_CONTEXT.getSessionStore()->getSession(mTargetSession);

	if (!targetSession) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR,
			"s %" PRIu32 ": Targetsession with sessionId %" PRIu32 " not found!", mSessionID, mTargetSession);
		return;
	}

	if ( session->getConnectState() == WTSShadow) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR,
			"s %" PRIu32 ": Session is already in shadowing state!", mSessionID);
		return;
	}

	if ( targetSession->getConnectState() != WTSActive) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR,
			"s %" PRIu32 ": Targetsession with sessionId %" PRIu32 " is not in WTSActive state!", mSessionID, mTargetSession);
		return;
	}

	UINT32 connectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mSessionID);
	if (connectionId == 0)  {
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR,
			"s %" PRIu32 ": could not find connecitonId for session!", mSessionID);
		return;
	}

	UINT32 targetconnectionId = APP_CONTEXT.getConnectionStore()->getConnectionIdForSessionId(mTargetSession);
	if (targetconnectionId == 0)  {
		WLog_Print(logger_taskStartRemoteControl, WLOG_ERROR,
			"s %" PRIu32 ": could not find connecitonId for sessionID %" PRIu32 " !", mSessionID, mTargetSession);
		return;
	}

	if (!APP_CONTEXT.getPropertyManager()->getPropertyBool(mTargetSession, "session.remotecontrol.ask", remoteControlRequest)) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_DEBUG, "s %" PRIu32 ": Could not get attribute 'session.remotecontrol.ask', using 'false' instead", mSessionID);
		remoteControlRequest = false;
	}

	if ((mStage == 0) && (remoteControlRequest)) {
		mSendMessage = true;
		startMessaging();
		mStage = 1;
		return;
	}

	if (remoteControlRequest && (mMessageResult != IDYES)) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_INFO,
			"s %" PRIu32 ": User denied remote control request for session %" PRIu32 "", mSessionID, mTargetSession);
		mResult = false;
		return;
	}

	callNS::CallOutOtsStartRemoteControlPtr remoteControl(new callNS::CallOutOtsStartRemoteControl());

	remoteControl->setConnectionId(connectionId);
	remoteControl->setTargetConnectionId(targetconnectionId);
	remoteControl->setHotkeyVk(mHotkeyVk);
	remoteControl->setHotkeyModifiers(mHotkeyModifiers);
	remoteControl->setFlags(mFlags);

	APP_CONTEXT.getRpcOutgoingQueue()->addElement(remoteControl);

	DWORD status = WaitForSingleObject(remoteControl->getAnswerHandle(), mTimeout);
	if (status == WAIT_TIMEOUT) {
		WLog_Print(logger_taskStartRemoteControl, WLOG_TRACE, "s %" PRIu32 ": OtsStartRemoteControl timed out", mSessionID);
		return;
	}
	mResult = remoteControl->isSuccess();
	if (mResult) {
		targetSession->addShadowedBy(session->getSessionID());
		setAccessorSession(session);
		startRemoteControl();
	}
}

BOOL TaskStartRemoteControl::getResult() {
	return mResult;
}

void TaskStartRemoteControl::abortTask() {
	mResult = false;
	InformableTask::abortTask();
}

} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/
