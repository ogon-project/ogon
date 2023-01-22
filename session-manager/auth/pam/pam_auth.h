/**
 * ogon - Free Remote Desktop Services
 * PAM Authentication Module
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

#ifndef OGON_SMGR_AUTH_PAM_H_
#define OGON_SMGR_AUTH_PAM_H_

#include "../../auth.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PAM_AUTH_DOMAIN ""

int RdsAuthModuleEntry(RDS_AUTH_MODULE_ENTRY_POINTS* pEntryPoints);

#ifdef __cplusplus
}
#endif

#endif /* OGON_SMGR_AUTH_PAM_H_ */
