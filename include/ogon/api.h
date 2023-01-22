/**
 * ogon - Free Remote Desktop Services
 * API Header
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

#ifndef OGON_API_H_
#define OGON_API_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if __GNUC__ >= 4
	#define OGON_API __attribute__ ((visibility("default")))
#else
	#define OGON_API 
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* OGON_API_H_ */
