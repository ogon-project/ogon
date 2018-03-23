/**
 * ogon - Free Remote Desktop Services
 * Backend Library
 * Service Helpers
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
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

#ifndef _OGON_SERVICE_HELPER_H_
#define _OGON_SERVICE_HELPER_H_

#include <ogon/backend.h>

typedef struct _ogon_backend_service ogon_backend_service;

/** @brief an enumeration describing the result of the ogon_service_incoming_bytes function */
typedef enum _ogon_incoming_bytes_result {
	OGON_INCOMING_BYTES_OK,
	OGON_INCOMING_BYTES_BROKEN_PIPE,
	OGON_INCOMING_BYTES_INVALID_MESSAGE,
	OGON_INCOMING_BYTES_WANT_MORE_DATA,
} ogon_incoming_bytes_result;


#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Creates a ogon_backend_service. Note that <em>endpoint</em> is supposed to
 *	have the format of windows/winPr pipes (\\.\pipe\name)
 *
 * @param SessionId the session id
 * @param endpoint the named pipe to listen for incoming connection
 * @return an instance of ogon_backend_service, NULL in case of OOM
 */
OGON_API ogon_backend_service* ogon_service_new(DWORD SessionId, const char* endpoint);

/**
 *	Binds the listening named pipe, and return the listening socket.
 *
 * @param service the ogon_backend_service that should be bound
 * @return the handle of the listening socket, NULL or INVALID_HANDLE_VALUE in case of error
 */
OGON_API HANDLE ogon_service_bind_endpoint(ogon_backend_service *service);

/**
 * Returns the file descriptor of the listening socket, this is useful if you
 * want to monitor read / write availability in an external event loop.
 *
 * @param service the ogon_backend_service to query
 * @return the file descriptor of the listening socket, -1 if invalid
 */
OGON_API int ogon_service_server_fd(ogon_backend_service *service);

/**
 * Handles an incoming connection
 *
 * @param service the ogon_backend_service that may accept
 * @return the handle of the accepted connection, INVALID_HANDLE_VALUE or NULL if something failed
 */
OGON_API HANDLE ogon_service_accept(ogon_backend_service* service);

/**
 * Sets the message callbacks for the backend service. These callbacks will be called
 * when processing <em>ogon_service_incoming_bytes</em>
 *
 * @param service the ogon_backend_service
 * @param cbs callbacks to install
 */
OGON_API void ogon_service_set_callbacks(ogon_backend_service* service, ogon_client_interface *cbs);

/**
 * Returns the file descriptor of the connected client which is supposed to be
 * ogon. This is useful if you want to monitor read / write availability in
 * an external event loop.
 *
 * @param service the ogon_backend_service to query
 * @return the file descriptor of the client socket, -1 if invalid
 */
OGON_API int ogon_service_client_fd(ogon_backend_service *service);

/**
 * Checks that the connected client is ogon. On platforms that supports it, this is performed
 * by checking that the connected client PID / UID matches the OGON_PID and OGON_UID
 * environment variables that have been set by the service that launched the backend (most
 * probably the sessionManager). This function should be called just after <em>ogon_service_accept</em>.
 *
 * @param service the ogon_backend_service to check
 * @return if the client is legitimate to connect
 */
OGON_API BOOL ogon_service_check_peer_credentials(ogon_backend_service *service);

/**
 * Performs the same checks than <em>ogon_service_check_peer_credentials</em>, but working
 * directly on the client file descriptor.
 *
 * @param fd the file descriptor
 * @return if the remote peer is legitimate to connect
 */
OGON_API BOOL ogon_check_peer_credentials(int fd);

/**
 * treat incoming bytes and call user set callbacks if messages are received.
 *
 * @param service the ogon_backend_service to check
 * @param cb_data a pointer that will be given to the callbacks
 * @return an incoming_bytes_result describing the result of the operation
 */
OGON_API ogon_incoming_bytes_result ogon_service_incoming_bytes(ogon_backend_service *service, void *cb_data);

/**
 * Injects the given message to ogon
 *
 * @param service the ogon_backend_service to talk to ogon
 * @param msg a message that should be send
 * @return TRUE on success, FALSE on failure
 */
OGON_API BOOL ogon_service_write_message(ogon_backend_service *service, UINT16 type, ogon_message *msg);


/**
 * Closes the connection with ogon
 * @param service the ogon_backend_service
 */
OGON_API void ogon_service_kill_client(ogon_backend_service *service);

/**
 * Releases the memory associated with this ogon_backend_service
 *
 * @param service the ogon_backend_service
 */
OGON_API void ogon_service_free(ogon_backend_service* service);


#ifdef __cplusplus
}
#endif


#endif /* _OGON_SERVICE_HELPER_H_ */
