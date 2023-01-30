/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Signaling queue template
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

#ifndef OGON_SMGR_SIGNALINGQUEUE_H_
#define OGON_SMGR_SIGNALINGQUEUE_H_

#include <list>
#include <winpr/synch.h>

template<typename QueueElement> class SignalingQueue
{
public:
	SignalingQueue(){
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400)) {
			throw std::bad_alloc();
		}
		if (!(mSignalHandle = CreateEvent(nullptr, TRUE, FALSE, nullptr))) {
			throw std::bad_alloc();
		}
	}

	~SignalingQueue() {
		mlist.clear();
		CloseHandle(mSignalHandle);
		DeleteCriticalSection(&mCSection);
	}

	HANDLE getSignalHandle() {
		return mSignalHandle;
	}

	void addElement(QueueElement element) {
		EnterCriticalSection(&mCSection);
		mlist.push_back(element);
		SetEvent(mSignalHandle);
		LeaveCriticalSection(&mCSection);
	}

	std::list<QueueElement> getAllElements() {
		std::list<QueueElement> all;
		EnterCriticalSection(&mCSection);
		all = mlist;
		mlist.clear();
		ResetEvent(mSignalHandle);
		LeaveCriticalSection(&mCSection);
		return all;
	}

private:
	HANDLE mSignalHandle;
	CRITICAL_SECTION mCSection;
	std::list<QueueElement> mlist;
};

#endif /* OGON_SMGR_SIGNALINGQUEUE_H_ */
