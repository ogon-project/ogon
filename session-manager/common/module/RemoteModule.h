/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Remote Module Class
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

#ifndef _OGON_SMGR_REMOTEMODULE_H_
#define _OGON_SMGR_REMOTEMODULE_H_

#define REMOTE_TIMEOUT 15000

#include "Module.h"
#include "RemoteModuleTransport.h"
#include <pbRPC.pb.h>


namespace ogon { namespace sessionmanager { namespace module {

	#define REMOTE_CLIENT_BUFFER_SIZE	0xFFFF
	#define REMOTE_LAUNCHER_PROCESS	"ogon-backend-launcher"

	#define REMOTE_CLIENT_SUCCESS 0
	#define REMOTE_CLIENT_NEEDS_MORE_DATA 1
	#define REMOTE_CLIENT_ERROR 2

	struct remote_module
	{
		RDS_MODULE_COMMON commonModule;
		char * remoteIP;
		char * domain;
		RemoteModuleTransportContext * context;
		bool launcherStarted;
	};

	typedef struct remote_module REMOTE_MODULE;

	class RemoteModule: public Module, protected RemoteModuleTransport {
	public:
		RemoteModule();
		virtual int initModule(const std::string &moduleFileName, RDS_MODULE_ENTRY_POINTS *entrypoints);
		virtual ~RemoteModule();

		virtual RDS_MODULE_COMMON *newContext();
		virtual void freeContext(RDS_MODULE_COMMON *context);

		virtual std::string start(RDS_MODULE_COMMON *context);
		virtual int stop(RDS_MODULE_COMMON *context);

		virtual int connect(RDS_MODULE_COMMON *context);
		virtual int disconnect(RDS_MODULE_COMMON *context);

		virtual std::string getWinstationName(RDS_MODULE_COMMON *context);

	protected:

		virtual UINT processCall(RemoteModuleTransportContext &context, const UINT32 callID, const UINT32 callType,
								 const bool isResponse, const std::string &payload, void* customData);

	private:

		UINT serveOneRemoteCall(REMOTE_MODULE *context, void *customData);

		BOOL startLauncher(REMOTE_MODULE *context);
		BOOL stopLauncher(REMOTE_MODULE *context);

		UINT processModuleStart(const std::string &payload, void* customData);
		UINT processModuleStop(const std::string &payload, void* customData);
		UINT processModuleGetCustomInfo(const std::string &payload, void* customData);
		UINT processModuleConnect(const std::string &payload, void* customData);
		UINT processModuleDisconnect(const std::string &payload, void* customData);
		UINT processModuleExit(const std::string &payload, void* customData);
		UINT processPropertyBool(RemoteModuleTransportContext &context, const UINT32 callID, const std::string &payload);
		UINT processPropertyNumber(RemoteModuleTransportContext &context, const UINT32 callID, const std::string &payload);
		UINT processPropertyString(RemoteModuleTransportContext &context, const UINT32 callID, const std::string &payload);
		size_t getEnvLength(char *env);
		std::string getLauncherExecutable(REMOTE_MODULE *context);

	};

} /*module*/ } /*sessionmanager*/ } /*ogon*/

namespace moduleNS = ogon::sessionmanager::module;

#endif /* _OGON_SMGR_REMOTEMODULE_H_ */
