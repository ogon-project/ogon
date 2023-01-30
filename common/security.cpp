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

#include <sys/socket.h>

#if defined(__FreeBSD__) || defined(__APPLE__)
#include <sys/ucred.h>
#endif

#include "security.h"

BOOL ogon_socket_credentials(
		int fd, uid_t *uid, BOOL *haveUid, pid_t *pid, BOOL *havePid) {
#if defined(SO_PEERCRED)
	/* Linux */
#if defined(_WIN32)
	int credLength;
#else
	unsigned int credLength;
#endif
	struct {
		pid_t pid;
		uid_t uid;
		gid_t gid;
	} credentials;

	credLength = sizeof(credentials);
	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &credentials, &credLength) < 0)
		return FALSE;

	*uid = credentials.uid;
	*haveUid = TRUE;
	*pid = credentials.pid;
	*havePid = TRUE;

#elif defined(LOCAL_PEERCRED)
	/* FreeBSD and OS X */
	struct xucred xucred;
	unsigned int xucredLen;

	xucredLen = sizeof(xucred);
	if (getsockopt(fd, SOL_SOCKET, LOCAL_PEERCRED, &xucred, &xucredLen) < 0)
		return FALSE;

	*uid = credentials.uid;
	*haveUid = TRUE;

	/* PID retrieval not supported */
	*pid = 0;
	*havePid = FALSE;

#else
	*uid = 0;
	*haveUid = FALSE;
	*pid = 0;
	*havePid = FALSE;

#endif

	return TRUE;
}
