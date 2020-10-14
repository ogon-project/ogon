/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * OTSApiServer
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

#ifndef _OGON_SMGR_OTSAPI_OTSAPISERVER_H_
#define _OGON_SMGR_OTSAPI_OTSAPISERVER_H_

#include <winpr/synch.h>
#include <memory>
#include <thrift/server/TServer.h>
#include <thrift/transport/TSSLSocket.h>

namespace ogon{ namespace sessionmanager{ namespace otsapi {

	class OTSApiServer {
	public:
		OTSApiServer();
		virtual ~OTSApiServer();

		static void serverThread( void * parameter);
		bool startOTSApi();
		bool stopOTSApi();

		void setPort(DWORD port);
		DWORD getPort();

		void setServer(std::shared_ptr<apache::thrift::server::TServer> server);
		CRITICAL_SECTION * getCritSection();
		void setSuccess(bool success);

	private:
		static std::shared_ptr<apache::thrift::transport::TSSLSocketFactory>
			getSSLSocketFactory(std::string certFile, std::string keyFile);

		CRITICAL_SECTION	mCSection;
		HANDLE	mhStarted;
		std::shared_ptr<apache::thrift::server::TServer>	mServer;
		HANDLE	mServerThread;
		DWORD	mPort;
		bool	mSuccess;
	};

} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/

namespace otsapiNS = ogon::sessionmanager::otsapi;

#endif /* _OGON_SMGR_OTSAPI_OTSAPISERVER_H_ */
