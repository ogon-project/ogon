/**
 * ogon - Free Remote Desktop Services
 * Backend Library
 * Shared Memory Damage Buffer
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
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
 * Under the GNU Affero General Public License version 3 section 7 the
 * copyright holders grant the additional permissions set forth in the
 * ogon Library AGPL Exceptions version 1 as published by
 * Thincast Technologies GmbH.
 *
 * For more information see the file LICENSE in the distribution of this file.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <ogon/dmgbuf.h>
#include <sys/shm.h>
#include <sys/stat.h>

#include "../common/global.h"

#define TAG OGON_TAG("backend.dmgbuf")

#define OGON_DMGBUF_ALIGN(n, m) (n + m - n % m)
#define OGON_DMGBUF_DATA_ALIGNMENT 256

#define OGON_DMGBUF_MAGIC 0xCACAB0B2
#define OGON_DMGBUF_MAX_RECTS 100
#define OGON_DMGBUF_POS_MAGIC 0
#define OGON_DMGBUF_POS_SHMID (sizeof(UINT32) + OGON_DMGBUF_POS_MAGIC)
#define OGON_DMGBUF_POS_WIDTH (sizeof(UINT32) + OGON_DMGBUF_POS_SHMID)
#define OGON_DMGBUF_POS_HEIGHT (sizeof(UINT32) + OGON_DMGBUF_POS_WIDTH)
#define OGON_DMGBUF_POS_SCANLINE (sizeof(UINT32) + OGON_DMGBUF_POS_HEIGHT)
#define OGON_DMGBUF_POS_MAX_RECTS (sizeof(UINT32) + OGON_DMGBUF_POS_SCANLINE)
#define OGON_DMGBUF_POS_NUM_RECTS (sizeof(UINT32) + OGON_DMGBUF_POS_MAX_RECTS)
#define OGON_DMGBUF_POS_RECTS (sizeof(UINT32) + OGON_DMGBUF_POS_NUM_RECTS)
#define OGON_DMGBUF_POS_PADDING \
	(OGON_DMGBUF_POS_RECTS + OGON_DMGBUF_MAX_RECTS * sizeof(RDP_RECT))
#define OGON_DMGBUF_POS_DATA \
	OGON_DMGBUF_ALIGN(OGON_DMGBUF_POS_PADDING, OGON_DMGBUF_DATA_ALIGNMENT)

#define OGON_DMGBUF_MEM_SIZE(h, s) (OGON_DMGBUF_POS_DATA + h * s)

#define OGON_DMGBUF_PTR_MAGIC(p) \
	((UINT32 *)(((char *)p) + OGON_DMGBUF_POS_MAGIC))
#define OGON_DMGBUF_PTR_SHMID(p) \
	((INT32 *)(((char *)p) + OGON_DMGBUF_POS_SHMID))
#define OGON_DMGBUF_PTR_WIDTH(p) \
	((UINT32 *)(((char *)p) + OGON_DMGBUF_POS_WIDTH))
#define OGON_DMGBUF_PTR_HEIGHT(p) \
	((UINT32 *)(((char *)p) + OGON_DMGBUF_POS_HEIGHT))
#define OGON_DMGBUF_PTR_SCANLINE(p) \
	((UINT32 *)(((char *)p) + OGON_DMGBUF_POS_SCANLINE))
#define OGON_DMGBUF_PTR_MAX_RECTS(p) \
	((UINT32 *)(((char *)p) + OGON_DMGBUF_POS_MAX_RECTS))
#define OGON_DMGBUF_PTR_NUM_RECTS(p) \
	((UINT32 *)(((char *)p) + OGON_DMGBUF_POS_NUM_RECTS))
#define OGON_DMGBUF_PTR_RECTS(p) \
	((RDP_RECT *)(((char *)p) + OGON_DMGBUF_POS_RECTS))
#define OGON_DMGBUF_PTR_DATA(p) ((BYTE *)(((char *)p) + OGON_DMGBUF_POS_DATA))

#define OGON_DMGBUF_GOOD(p) \
	(p && *OGON_DMGBUF_PTR_MAGIC(p) == OGON_DMGBUF_MAGIC)

int ogon_dmgbuf_set_user(void *handle, unsigned int user_id) {
	struct shmid_ds buf;

	if (!OGON_DMGBUF_GOOD(handle)) {
		return -1;
	}

	if (shmctl(*OGON_DMGBUF_PTR_SHMID(handle), IPC_STAT, &buf) < 0) {
		WLog_ERR(TAG, "shmctl IPC_STAT failed, error=%s(%d)", strerror(errno),
				errno);
		return -1;
	}

	if (buf.shm_perm.uid != user_id) {
		buf.shm_perm.uid = user_id;
		if (shmctl(*OGON_DMGBUF_PTR_SHMID(handle), IPC_SET, &buf) < 0) {
			WLog_ERR(TAG, "shmctl IPC_SET failed, error=%s(%d)",
					strerror(errno), errno);
			return -1;
		}
	}
	return 0;
}

BYTE *ogon_dmgbuf_get_data(void *handle) {
	if (!OGON_DMGBUF_GOOD(handle)) {
		return nullptr;
	}

	return OGON_DMGBUF_PTR_DATA(handle);
}

int ogon_dmgbuf_get_id(void *handle) {
	if (!OGON_DMGBUF_GOOD(handle)) {
		return -1;
	}

	return *OGON_DMGBUF_PTR_SHMID(handle);
}

UINT32 ogon_dmgbuf_get_fbsize(void *handle) {
	if (OGON_DMGBUF_GOOD(handle)) {
		auto scanline = *OGON_DMGBUF_PTR_SCANLINE(handle);
		auto height = *OGON_DMGBUF_PTR_HEIGHT(handle);

		return scanline * height;
	}

	return 0;
}

RDP_RECT *ogon_dmgbuf_get_rects(void *handle, UINT32 *num_rects) {
	if (!OGON_DMGBUF_GOOD(handle)) {
		return nullptr;
	}

	if (num_rects) {
		*num_rects = *OGON_DMGBUF_PTR_NUM_RECTS(handle);
	}

	return OGON_DMGBUF_PTR_RECTS(handle);
}

UINT32 ogon_dmgbuf_get_max_rects(void *handle) {
	if (!OGON_DMGBUF_GOOD(handle)) {
		return 0;
	}

	return *OGON_DMGBUF_PTR_MAX_RECTS(handle);
}

int ogon_dmgbuf_set_num_rects(void *handle, UINT32 num_rects) {
	if (!OGON_DMGBUF_GOOD(handle)) {
		return -1;
	}

	if (num_rects > *OGON_DMGBUF_PTR_MAX_RECTS(handle)) {
		return -1;
	}

	*OGON_DMGBUF_PTR_NUM_RECTS(handle) = num_rects;

	return 0;
}

void *ogon_dmgbuf_new(int width, int height, int scanline) {
	void *handle;
	int shm_flag;
	auto shm_size = OGON_DMGBUF_MEM_SIZE(height, scanline);
	int segment_id = -1;

	if (width < 1 || height < 1 || scanline < width * 4) {
		WLog_ERR(TAG, "invalid parameters: width=%d height=%d scanline=%d",
				width, height, scanline);
		return nullptr;
	}

	/* allocate shared memory segment */
	shm_flag = IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	segment_id = shmget(IPC_PRIVATE, shm_size, shm_flag);
	if (segment_id == -1) {
		WLog_ERR(TAG, "shmget failed, error=%s(%d)", strerror(errno), errno);
		return nullptr;
	}

	/* attach the shared memory segment */
	handle = shmat(segment_id, nullptr, 0);
	if (handle == ((void *)(size_t)(-1))) {
		WLog_ERR(TAG, "shmat failed, error=%s(%d)", strerror(errno), errno);
		return nullptr;
	}
	WLog_DBG(TAG, "attached to new shared memory segment 0x%08X", segment_id);

	/* mark the shared memory segment for automatic deletion */
	if (shmctl(segment_id, IPC_RMID, nullptr) < 0) {
		WLog_ERR(TAG, "error unreferencing SHM segment %d, error=%s(%d)",
				segment_id, strerror(errno), errno);
		return nullptr;
	}

	memset(handle, 0, shm_size);
	*OGON_DMGBUF_PTR_MAGIC(handle) = OGON_DMGBUF_MAGIC;
	*OGON_DMGBUF_PTR_SHMID(handle) = segment_id;
	*OGON_DMGBUF_PTR_WIDTH(handle) = width;
	*OGON_DMGBUF_PTR_HEIGHT(handle) = height;
	*OGON_DMGBUF_PTR_SCANLINE(handle) = scanline;
	*OGON_DMGBUF_PTR_MAX_RECTS(handle) = OGON_DMGBUF_MAX_RECTS;

	/* verify that data is correctly aligned */
	if ((unsigned long)OGON_DMGBUF_PTR_DATA(handle) %
			OGON_DMGBUF_DATA_ALIGNMENT) {
		WLog_ERR(TAG,
				"internal error: damage buffer data is not correctly aligned!");
		return nullptr;
	}

	return handle;
}

void ogon_dmgbuf_free(void *handle) {
	int segment_id = 0;
	WLog_DBG(TAG, "freeing damage buffer %p", handle);

	if (!OGON_DMGBUF_GOOD(handle)) {
		return;
	}

	segment_id = *OGON_DMGBUF_PTR_SHMID(handle);

	/* detach shared memory segment. note: it was marked for auto deletion */
	shmdt(handle);
	WLog_DBG(TAG, "detached shared memory segment 0x%08X", segment_id);
}

void *ogon_dmgbuf_connect(int buffer_id) {
	void *handle = shmat(buffer_id, nullptr, 0);
	if (handle == ((void *)(size_t)(-1))) {
		WLog_ERR(TAG, "shmat(%d) failed, error=%s(%d)", buffer_id,
				strerror(errno), errno);
		return nullptr;
	}

	WLog_DBG(TAG, "attached to an existing shared memory segment 0x%08X",
			buffer_id);
	if (!OGON_DMGBUF_GOOD(handle)) {
		WLog_ERR(TAG, "error, invalid magic");
		shmdt(handle);
		return nullptr;
	}

	return handle;
}
