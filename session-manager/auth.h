/**
 * ogon - Free Remote Desktop Services
 * Authentication Module Interface
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 * Copyright (c) 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Authors:
 * Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Martin Haimberger <martin.haimberger@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OGON_SMGR_AUTH_H_
#define OGON_SMGR_AUTH_H_

#include <winpr/wtypes.h>

typedef struct rds_auth_module_entry_points_v1 RDS_AUTH_MODULE_ENTRY_POINTS_V1;
typedef RDS_AUTH_MODULE_ENTRY_POINTS_V1 RDS_AUTH_MODULE_ENTRY_POINTS;

/**
 * Authentication Module Entry Points
 */

#ifdef __cplusplus
extern "C" {
#endif

struct _rds_auth_module
{
	void* dummy;
};
typedef struct _rds_auth_module rdsAuthModule;

typedef rdsAuthModule* (*pRdsAuthModuleNew)(void);
typedef void (*pRdsAuthModuleFree)(rdsAuthModule* auth);

/* the domain can be modified by an auth module
 * this is the case if the authmodule does not support domains
 * then the authmodule should set the domain to the same value
 * or "". If the domain is set, the memory is freed afterwards.
 * */

typedef int (*pRdsAuthLogonUser)(rdsAuthModule* auth, const char* username, char** domain, const char* password);

struct rds_auth_module_entry_points_v1
{
	DWORD Version;

	pRdsAuthModuleNew New;
	pRdsAuthModuleFree Free;

	pRdsAuthLogonUser LogonUser;
	const char* Name;
};

#define RDS_AUTH_MODULE_INTERFACE_VERSION	1
#define RDS_AUTH_MODULE_ENTRY_POINT_NAME	"RdsAuthModuleEntry"

typedef int (*pRdsAuthModuleEntry)(RDS_AUTH_MODULE_ENTRY_POINTS* pEntryPoints);

#ifdef __cplusplus
}
#endif

#endif /* OGON_SMGR_AUTH_H_ */
