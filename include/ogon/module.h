/**
 * ogon - Free Remote Desktop Services
 * Backend Module Interface
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

#ifndef _OGON_MODULE_H_
#define _OGON_MODULE_H_

#include <winpr/wtypes.h>

typedef bool (*pgetPropertyBool)(UINT32 sessionID, const char* path, bool* value);
typedef bool (*pgetPropertyNumber)(UINT32 sessionID, const char* path, long* value);
typedef bool (*pgetPropertyString)(UINT32 sessionID, const char* path, char* value, unsigned int valueLength);

typedef struct rds_module_entry_points_v1 RDS_MODULE_ENTRY_POINTS_V1;
typedef RDS_MODULE_ENTRY_POINTS_V1 RDS_MODULE_ENTRY_POINTS;

struct _RDS_MODULE_COMMON
{
	UINT32 sessionId;
	char* userName;
	char* domain;
	HANDLE userToken;
	char* envBlock;
	char* baseConfigPath;
	char* remoteIp;
};

typedef struct _RDS_MODULE_COMMON RDS_MODULE_COMMON;


struct _RDS_MODULE_CONFIG_CALLBACKS
{
	pgetPropertyBool getPropertyBool;
	pgetPropertyNumber getPropertyNumber;
	pgetPropertyString getPropertyString;
};
typedef struct _RDS_MODULE_CONFIG_CALLBACKS RDS_MODULE_CONFIG_CALLBACKS;

typedef void (*pAddMonitoringProcess)(DWORD processId, UINT32 sessionId, bool terminateSession, RDS_MODULE_COMMON *context);
typedef bool (*pRemoveMonitoringProcess)(DWORD processId);

struct _RDS_MODULE_STATUS_CALLBACKS
{
	pAddMonitoringProcess addMonitoringProcess;
	pRemoveMonitoringProcess removeMonitoringProcess;
};
typedef struct _RDS_MODULE_STATUS_CALLBACKS RDS_MODULE_STATUS_CALLBACKS;

/**
 * Module Entry Points
 */

typedef RDS_MODULE_COMMON* (*pRdsModuleNew)();
typedef void (*pRdsModuleFree)(RDS_MODULE_COMMON* module);

typedef char* (*pRdsModuleStart)(RDS_MODULE_COMMON* module);
typedef int (*pRdsModuleStop)(RDS_MODULE_COMMON* module);
typedef char* (*pRdsModuleGetCustomInfo)(RDS_MODULE_COMMON* module);
typedef int (*pRdsModuleConnect)(RDS_MODULE_COMMON* module);
typedef int (*pRdsModuleDisconnect)(RDS_MODULE_COMMON* module);

typedef int (*pRdsModuleInit)();
typedef int (*pRdsModuleDestroy)();

struct rds_module_entry_points_v1
{
	DWORD Version;

	pRdsModuleInit Init;
	pRdsModuleNew New;
	pRdsModuleFree Free;

	pRdsModuleStart Start;
	pRdsModuleStop Stop;
	pRdsModuleGetCustomInfo getCustomInfo;
	pRdsModuleDestroy Destroy;
	pRdsModuleConnect Connect;
	pRdsModuleDisconnect Disconnect;
	const char* Name;

	RDS_MODULE_CONFIG_CALLBACKS config;
	RDS_MODULE_STATUS_CALLBACKS status;
};

#define RDS_MODULE_INTERFACE_VERSION	1
#define RDS_MODULE_ENTRY_POINT_NAME	"RdsModuleEntry"

typedef int (*pRdsModuleEntry)(RDS_MODULE_ENTRY_POINTS* pEntryPoints);

#endif /* _OGON_MODULES_H_ */
