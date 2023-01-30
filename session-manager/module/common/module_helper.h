/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Module Helper
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

#ifndef OGON_SMGR_MODULEHELPER_H_
#define OGON_SMGR_MODULEHELPER_H_

#include <ogon/module.h>

#ifdef __cplusplus
extern "C" {
#endif

void initResolutions(RDS_MODULE_CONFIG_CALLBACKS* config, UINT32 sessionId , long* xres, long* yres, long* colordepth);

bool getPropertyBoolWrapper(const char* basePath, RDS_MODULE_CONFIG_CALLBACKS* config, UINT32 sessionID, const char* path, bool* value);
bool getPropertyNumberWrapper(const char* basePath, RDS_MODULE_CONFIG_CALLBACKS* config, UINT32 sessionID, const char* path, long* value);
bool getPropertyStringWrapper(const char* basePath, RDS_MODULE_CONFIG_CALLBACKS* config, UINT32 sessionID, const char* path, char* value, unsigned int valueLength);
bool TerminateChildProcessAfterTimeout(DWORD dwProcessId, DWORD dwMilliseconds, int* pExitCode, UINT32 sessionID = 0);
bool TerminateChildProcess(DWORD dwProcessId, DWORD dwTimeout, int* pExitCode, UINT32 sessionID);

#ifdef __cplusplus
}
#endif

#endif /* OGON_SMGR_MODULEHELPER_H_ */
