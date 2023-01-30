/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Wrapper to access Property Manager from C
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

#ifndef OGON_SMGR_PROPERTYCWRAPPER_H_
#define OGON_SMGR_PROPERTYCWRAPPER_H_

#include <winpr/wtypes.h>
#include "PropertyLevel.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>

bool getPropertyBool(UINT32 sessionID, const char *path, bool *value);
bool getPropertyNumber(UINT32 sessionID, const char *path, long *value);
bool getPropertyString(UINT32 sessionID, const char *path, char *value, unsigned int valueLength);

bool setPropertyBool(PROPERTY_LEVEL level, UINT32 sessionID, const char *path, bool value);
bool setPropertyNumber(PROPERTY_LEVEL level, UINT32 sessionID, const char *path, long value);
bool setPropertyString(PROPERTY_LEVEL level, UINT32 sessionID, const char *path, char *value);

#ifdef __cplusplus
}
#endif

#endif /* OGON_SMGR_PROPERTYCWRAPPER_H_ */
