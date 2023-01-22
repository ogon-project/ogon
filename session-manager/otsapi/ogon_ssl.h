/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * OTSAPI: Thrift SSL socket replacement class
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
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Library AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifndef _OGON_SMGR_OTSAPISSL_H_
#define _OGON_SMGR_OTSAPISSL_H_

#include <memory>
#include <thrift/transport/TSSLSocket.h>

namespace apache { namespace thrift { namespace transport {

/** @brief */
class OgonSSLSocket: public TSSLSocket {
public:
	~OgonSSLSocket() {
		/* Hack to prevent closing socket from forked processes */
		if (firstProcessId != getpid()) {
			socket_  = -1;
			ssl_ = nullptr;
		}
	};

protected:

	pid_t firstProcessId;

	/**
	* Constructor.
	*/

	OgonSSLSocket(std::shared_ptr<SSLContext> ctx): TSSLSocket(ctx) {
		firstProcessId = getpid();
	};

	/**
	* Constructor, create an instance of TSSLSocket given an existing socket.
	*
	* @param socket An existing socket
	*/

	OgonSSLSocket(std::shared_ptr<SSLContext> ctx, int socket):
		TSSLSocket(ctx, socket) {
		firstProcessId = getpid();
	};

	/**
	* Constructor.
	*
	* @param host  Remote host name
	* @param port  Remote port number
	*/

	OgonSSLSocket(std::shared_ptr<SSLContext> ctx, std::string host,
		int port): TSSLSocket(ctx, host, port)
	{
		firstProcessId = getpid();
	};


	virtual void authorize() {
		return;
	}

	friend class OgonSSLSocketFactory;
};


/**
 * SSL socket factory. SSL sockets should be created via SSL factory.
 */

class OgonSSLSocketFactory: public TSSLSocketFactory {
public:

	/**
	* Create an instance of TSSLSocket with a fresh new socket.
	*/

	virtual std::shared_ptr<TSSLSocket> createSocket() {
		std::shared_ptr<TSSLSocket> ssl(new OgonSSLSocket(ctx_));
		ssl->server(server());
		return ssl;
	}

	/**
	* Create an instance of TSSLSocket with the given socket.
	*
	* @param socket An existing socket.
	*/

	virtual std::shared_ptr<TSSLSocket> createSocket(int socket) {
		std::shared_ptr<TSSLSocket> ssl(new OgonSSLSocket(ctx_, socket));
		ssl->server(server());
		return ssl;
	}

	/**
	* Create an instance of TSSLSocket.
	*
	* @param host  Remote host to be connected to
	* @param port  Remote port to be connected to
	*/

	virtual std::shared_ptr<TSSLSocket> createSocket(const std::string &host,
		int port) {
		std::shared_ptr<TSSLSocket> ssl(new OgonSSLSocket(ctx_, host, port));
		ssl->server(server());
		return ssl;
	}
};

} /*apache*/} /*thrift*/ } /*transport*/

#endif /* _OGON_SMGR_OTSAPISSL_H_ */
