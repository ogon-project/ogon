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

#include <appcontext/ApplicationContext.h>
#include <otsapi/OgonServerSSL.h>
#include <otsapi/OTSApiHandler.h>
#include <otsapi/OTSApiServer.h>
#include <utils/CSGuard.h>
#include <utils/MakeCert.h>

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSSLServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/server/TThreadedServer.h>

#include <winpr/platform.h>
#include <winpr/thread.h>
#include <winpr/wlog.h>

#ifdef __linux__
#include <signal.h>
#endif

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace apache::thrift::server;

using std::shared_ptr;

namespace ogon{ namespace sessionmanager{ namespace otsapi {

	static wLog *logger_OTSApiServer = WLog_Get("ogon.sessionmanager.otsapiserver");

	OTSApiServer::OTSApiServer() {
		if (!InitializeCriticalSectionAndSpinCount(&mCSection, 0x00000400) ) {
			WLog_Print(logger_OTSApiServer, WLOG_FATAL,
				"Failed to initialize OTSApiServer critical section");
			throw std::bad_alloc();
		}
		if (!(mhStarted = CreateEvent(NULL, TRUE, FALSE, NULL))) {
			WLog_Print(logger_OTSApiServer, WLOG_FATAL,
					   "Failed to create OTSApiServer started event");
			throw std::bad_alloc();
		}
		mServerThread = NULL;
		mPort = 9091;
		mSuccess = false;
	}

	OTSApiServer::~OTSApiServer() {
		DeleteCriticalSection(&mCSection);
		CloseHandle(mhStarted);
	}

	void OTSApiServer::setServer(std::shared_ptr<apache::thrift::server::TServer> server) {
		mServer = server;
	}

	void OTSApiServer::setPort(DWORD port) {
		mPort = port;
	}

	DWORD OTSApiServer::getPort() {
		return mPort;
	}

	CRITICAL_SECTION* OTSApiServer::getCritSection() {
		return &mCSection;
	}

	void OTSApiServer::setSuccess(bool success) {
		mSuccess = success;
		SetEvent(mhStarted);
	}

	shared_ptr<TSSLSocketFactory> OTSApiServer::getSSLSocketFactory(std::string certFile, std::string keyFile) {
		shared_ptr<TSSLSocketFactory> factory(new OgonSSLSocketFactory());
		factory->loadCertificate(certFile.c_str());
		factory->loadPrivateKey(keyFile.c_str());
		factory->authenticate(false);
		return factory;
	}

	void OTSApiServer::serverThread(void *param) {
		std::string certFile;
		std::string keyFile;
		configNS::PropertyManager *propertyManager = APP_CONTEXT.getPropertyManager();

		propertyManager->getPropertyString(0, "ssl.certificate", certFile);
		propertyManager->getPropertyString(0, "ssl.key", keyFile);

		if (ogon_generate_certificate(certFile, keyFile) < 0) {
			WLog_Print(logger_OTSApiServer, WLOG_INFO, "Error ensuring certificate file");
			raise(SIGINT);
			return;
		}

		OTSApiServer *server = (OTSApiServer *)param;
		try {
			shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());
			shared_ptr<OTSApiHandler> handler(new OTSApiHandler());
			shared_ptr<TProcessor> processor(new otsapiProcessor(handler));
			shared_ptr<TSSLSocketFactory> sslfactory = getSSLSocketFactory(certFile, keyFile);
			shared_ptr<TServerTransport> serverTransport(new TSSLServerSocket(server->getPort(), sslfactory));
			shared_ptr<TTransportFactory> transportFactory(new TFramedTransportFactory());

			shared_ptr<TServer> thriftServer(new TThreadedServer(processor, serverTransport, transportFactory, protocolFactory));
			server->setServer(thriftServer);
			server->setSuccess(true);
			WLog_Print(logger_OTSApiServer, WLOG_INFO, "OTSApiServer started on port %" PRIu32 "", server->getPort());
			thriftServer->serve();
			WLog_Print(logger_OTSApiServer, WLOG_INFO, "OTSApiServer stopped.");
		} catch (const TException &tx) {
			WLog_Print(logger_OTSApiServer, WLOG_FATAL, "TException in thrift server thread: %s", tx.what());
			server->setSuccess(false);
			raise(SIGINT);
		} catch (...) {
			WLog_Print(logger_OTSApiServer, WLOG_FATAL, "exception in thrift server thread!");
			server->setSuccess(false);
			raise(SIGINT);
		}
	}

	bool OTSApiServer::startOTSApi() {
		DWORD result;
		CSGuard guard(&mCSection);

		if (!(mServerThread = CreateThread(NULL, 0,
				(LPTHREAD_START_ROUTINE) OTSApiServer::serverThread,
				(void*) this, 0, NULL)))
		{
			WLog_Print(logger_OTSApiServer, WLOG_ERROR, "failed to create thread");
			return false;
		}

		result = WaitForSingleObject(mhStarted, 1 * 1000);

		if (result != WAIT_OBJECT_0) {
			return false;
		}
		return mSuccess;
	}

	bool OTSApiServer::stopOTSApi() {
		CSGuard guard(&mCSection);
		if (mServer != NULL) {
			WLog_Print(logger_OTSApiServer, WLOG_INFO, "Stopping OTSApiServer ...");
			mServer->stop();
			WaitForSingleObject(mServerThread,INFINITE);
			CloseHandle(mServerThread);
			mServerThread = NULL;
			mServer.reset();
		}
		return true;
	}
} /*otsapi*/ } /*sessionmanager*/ } /*ogon*/


