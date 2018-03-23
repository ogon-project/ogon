/**
 * ogon - Free Remote Desktop Services
 * Common Security Functions
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * David Fort <contact@hardening-consulting.com>
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

#ifndef _OGON_SECURITY_H_
#define _OGON_SECURITY_H_

#include <unistd.h>
#include <winpr/wtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *	Tries to get peer's identity using the socket and SO_PEERCRED like functions.
 *
 * @param fd a file descriptor of a connected socket
 * @param uid the uid to fill
 * @param haveUid if we have been able to fill the uid
 * @param pid the pid to fill
 * @param havePid if we have been able to fill the pid
 * @return if the function was successful
 */
BOOL ogon_socket_credentials(int fd, uid_t *uid, BOOL *haveUid, pid_t *pid, BOOL *havePid);

#ifdef __cplusplus
}
#endif

#endif /* _OGON_SECURITY_H_ */
