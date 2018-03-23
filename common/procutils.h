/**
 * ogon - Free Remote Desktop Services
 * procutils
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * Norbert Federa <norbert.federa@thincast.com>
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

#ifndef _OGON_PROCUTILS_H_
#define _OGON_PROCUTILS_H_

#include <sys/types.h>
#include <winpr/wtypes.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


BOOL get_parent_pid(const pid_t pid, pid_t *ppid);
char* get_process_name(const pid_t pid);

#ifdef __cplusplus
};
#endif /* __cplusplus */

#endif /* _OGON_PROCUTILS_H_ */
