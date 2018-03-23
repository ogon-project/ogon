/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * makecert helper
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
 * David Fort <contact@hardening-consulting.com>
 * Martin Haimberger <martin.haimberger@thincast.com>
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

#include <winpr/path.h>
#include <winpr/tools/makecert.h>
#include <winpr/wlog.h>
#include "MakeCert.h"
#include "StringHelpers.h"

static const char* makecert_argv[4] = {
	"makecert",
	"-rdp",
	"-live",
	"-silent"
};

static int makecert_argc = (sizeof(makecert_argv) / sizeof(char *));

static wLog *logger_makeCert = WLog_Get("ogon.sessionmanager.makecert");


int ogon_generate_certificate(std::string &certFile, std::string &keyFile) {
	MAKECERT_CONTEXT* context ;
	int ret = -1;

	if (PathFileExistsA(certFile.c_str()) && PathFileExistsA(keyFile.c_str())) {
		return 0;
	}

	if (!stringEndsWith(certFile, ".crt")) {
		WLog_Print(logger_makeCert, WLOG_ERROR,
				"certificate filename (%s) MUST end with .crt", certFile.c_str());
		return -1;
	}

	if (!stringEndsWith(keyFile, ".key")) {
		WLog_Print(logger_makeCert, WLOG_ERROR,
				"key filename (%s) MUST end with .key", keyFile.c_str());
		return -1;
	}

	std::string baseCertFile = certFile.substr(0, certFile.length() - 4); // drop the .crt
	std::string baseKeyFile = keyFile.substr(0, keyFile.length() - 4); // drop the .key

	if (!(context = makecert_context_new())) {
		WLog_Print(logger_makeCert, WLOG_ERROR, "makecert_context_new failed!");
		goto out;
	}

	if (makecert_context_process(context, makecert_argc, const_cast<char **>(makecert_argv)) < 0) {
		WLog_Print(logger_makeCert, WLOG_ERROR, "makecert_context_process failed!");
		goto out;
	}

	if (makecert_context_set_output_file_name(context, const_cast<char *>(baseCertFile.c_str())) < 0) {
		WLog_Print(logger_makeCert, WLOG_ERROR, "makecert_context_set_output_file_name failed!");
		goto out;
	}

	if (makecert_context_output_certificate_file(context, NULL) < 0) {
		WLog_Print(logger_makeCert, WLOG_ERROR, "failed to create certfile %s!", certFile.c_str());
		goto out;
	}

	if (makecert_context_set_output_file_name(context, const_cast<char *>(baseKeyFile.c_str())) < 0) {
		WLog_Print(logger_makeCert, WLOG_ERROR, "makecert_context_set_output_file_name failed!");
		goto out;
	}

	if (makecert_context_output_private_key_file(context, NULL) < 0)	{
		WLog_Print(logger_makeCert, WLOG_ERROR, "failed to create keyfile %s!", keyFile.c_str());
		goto out;
	}

	ret = 0;

out:
	makecert_context_free(context);

	return ret;
}
