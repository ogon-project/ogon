/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * WTS Permission Flags
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

#ifndef _OGON_SMGR_PERMISSION_H_
#define _OGON_SMGR_PERMISSION_H_

#define WTS_PERM_FLAGS_QUERY_INFORMATION        0x0001
#define WTS_PERM_FLAGS_SET_INFORMATION          0x0002
#define WTS_PERM_FLAGS_REMOTE_CONTROL           0x0004
#define WTS_PERM_FLAGS_LOGON                    0x0008
#define WTS_PERM_FLAGS_LOGOFF                   0x0010
#define WTS_PERM_FLAGS_MESSAGE                  0x0020
#define WTS_PERM_FLAGS_CONNECT                  0x0040
#define WTS_PERM_FLAGS_DISCONNECT               0x0080
#define WTS_PERM_FLAGS_VIRTUAL_CHANNEL          0x0100

#define WTS_PERM_FLAGS_FULL                     0x01FF /* all permissions */
#define WTS_PERM_FLAGS_USER                     0x0049 /* WTS_PERM_FLAGS_CONNECT | WTS_PERM_FLAGS_QUERY_INFORMATION | WTS_PERM_FLAGS_LOGON */
#define WTS_PERM_FLAGS_GUEST                    0x0008 /* WTS_PERM_FLAGS_LOGON */

#endif /* _OGON_SMGR_PERMISSION_H_ */
