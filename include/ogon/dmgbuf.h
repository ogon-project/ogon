/**
 * ogon - Free Remote Desktop Services
 * Backend Library
 * Shared Memory Damage Buffer
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

#ifndef _OGON_DMGBUF_H_
#define _OGON_DMGBUF_H_

#include <freerdp/types.h>
#include <ogon/api.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 *
 * @param handle
 * @param userId
 * @return
 */
OGON_API int ogon_dmgbuf_set_user(void *handle, unsigned int user_id);

/**
 *
 * @param handle
 * @return
 */
OGON_API BYTE* ogon_dmgbuf_get_data(void *handle);

/**
 *
 * @param handle
 * @return
 */
OGON_API int ogon_dmgbuf_get_id(void *handle);

/**
 *
 * @param handle
 * @return
 */
OGON_API UINT32 ogon_dmgbuf_get_fbsize(void *handle);

/**
 *
 * @param handle
 * @param numRects
 * @return
 */
OGON_API RDP_RECT* ogon_dmgbuf_get_rects(void *handle, UINT32 *num_rects);

/**
 *
 * @param handle
 * @return
 */
OGON_API UINT32 ogon_dmgbuf_get_max_rects(void *handle);

/**
 *
 * @param handle
 * @param numRects
 * @return
 */
OGON_API int ogon_dmgbuf_set_num_rects(void *handle, UINT32 num_rects);

/**
 *
 * @param width
 * @param height
 */
OGON_API void* ogon_dmgbuf_new(int width, int height, int scanline);

/**
 *
 * @param handle
 */
OGON_API void ogon_dmgbuf_free(void *handle);

/**
 * @param bufid
 */
OGON_API void* ogon_dmgbuf_connect(int buffer_id);


#ifdef __cplusplus
}
#endif

#endif /* _OGON_DMGBUF_H_ */
