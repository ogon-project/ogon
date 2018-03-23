/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Callback Handlers for Modules
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

#ifndef _OGON_SMGR_MODULE_CALLBACKS_H_
#define _OGON_SMGR_MODULE_CALLBACKS_H_

#include <ogon/module.h>

namespace ogon { namespace sessionmanager { namespace module {

	class CallBacks {
	public:
		static void addMonitoringProcess(DWORD processId, UINT32 sessionId, bool terminateSession, RDS_MODULE_COMMON *context);
		static bool removeMonitoringProcess(DWORD processId);
	};

} /*module*/ } /*sessionmanager*/ } /*ogon*/

#endif /* _OGON_SMGR_MODULE_CALLBACKS_H_ */
