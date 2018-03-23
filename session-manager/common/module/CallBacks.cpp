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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "CallBacks.h"
#include <session/Session.h>
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>
#include <session/TaskEnd.h>

namespace ogon { namespace sessionmanager { namespace module {

	void CallBacks::addMonitoringProcess(DWORD processId, UINT32 sessionId, bool terminateSession, RDS_MODULE_COMMON *context) {
		APP_CONTEXT.getProcessMonitor()->addProcess(processId, sessionId, terminateSession, context);
	}
	bool CallBacks::removeMonitoringProcess(DWORD processId) {
		return APP_CONTEXT.getProcessMonitor()->removeProcess(processId);
	}


} /*module*/ } /*sessionmanager*/ } /*ogon*/
