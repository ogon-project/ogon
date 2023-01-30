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

#include "procutils.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

BOOL get_parent_pid(const pid_t pid, pid_t *ppid) {
	char buffer[2048];
	size_t size;
	FILE *fp;

	*ppid = 0;
	snprintf(buffer, sizeof(buffer), "/proc/%lu/stat", (unsigned long)pid);
	fp = fopen(buffer, "r");
	if (!fp) {
		return FALSE;
	}
	size = fread(buffer, sizeof(char), sizeof(buffer), fp);
	fclose(fp);
	if (size <= 0) {
		return FALSE;
	}
	char *ptr;
	buffer[size - 1] = '\0';
	/* For format details see man 5 proc */
	/* %d (%s) %c %d .. == pid (comm) state ppid */
	strtok(buffer, ")");
	strtok(nullptr, " ");
	ptr = strtok(nullptr, " ");
	*ppid = atoi(ptr);
	return TRUE;
}

char *get_process_name(const pid_t pid) {
	FILE *fp;
	size_t size;
	char buffer[4096];
	char path[32];

	snprintf(path, sizeof(path), "/proc/%lu/cmdline", (unsigned long)pid);

	if (!(fp = fopen(path, "r"))) {
		return nullptr;
	}

	memset(buffer, 0, sizeof(buffer));
	size = fread(buffer, sizeof(char), sizeof(buffer) - 1, fp);
	fclose(fp);

	return size < 1 ? nullptr : strdup(buffer);
}
