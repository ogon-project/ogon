/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Guard object for critical sections
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

#ifndef _OGON_SMGR_CSGUARD_H_
#define _OGON_SMGR_CSGUARD_H_

#include <winpr/synch.h>

class CSGuard {
public:
	CSGuard(CRITICAL_SECTION *criticalSection) {
		mCriticalSection = criticalSection;
		EnterCriticalSection(mCriticalSection);
	}

	void leaveGuard() {
		LeaveCriticalSection(mCriticalSection);
		mCriticalSection = NULL;
	}

	~CSGuard() {
		if (mCriticalSection != NULL) {
			LeaveCriticalSection(mCriticalSection);
		}			
	}

private:
	CRITICAL_SECTION *mCriticalSection;

};


#endif /* _OGON_SMGR_CSGUARD_H_ */
