/**
 * ogon - Free Remote Desktop Services
 * PAM Authentication Module
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 * Copyright (c) 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/path.h>

#include <security/pam_appl.h>
#include "../../common/global.h"

#include "pam_auth.h"
#include <ogon/api.h>

#include <malloc.h>

struct t_user_pass
{
	char user[256];
	char pass[256];
};

struct t_auth_info
{
	struct t_user_pass user_pass;
	int session_opened;
	int did_setcred;
	struct pam_conv pamc;
	pam_handle_t *ph;
};

static void get_service_name(char* service_name)
{
	service_name[0] = '\0';
	if (PathFileExistsA("/etc/pam.d/ogon"))
	{
		strncpy(service_name, "ogon", 255);
	}
	else
	{
		strncpy(service_name, "gdm", 255);
	}
}

static int verify_pam_conv(int num_msg, const struct pam_message **msg, struct pam_response **resp, void *appdata_ptr)
{
	auto user_pass = static_cast<struct t_user_pass *>(appdata_ptr);

	auto reply = static_cast<struct pam_response *>(
			calloc(sizeof(struct pam_response), num_msg));
	if (!reply) {
		return (PAM_BUF_ERR);
	}

	for (int i = 0; i < num_msg; i++) {
		/* make sure the values are initialized properly */
		reply[i].resp_retcode = PAM_SUCCESS;

		switch (msg[i]->msg_style)
		{
			case PAM_PROMPT_ECHO_ON: /* username */
				reply[i].resp = _strdup(user_pass->user);
				if (!reply[i].resp) {
					goto out_fail;
				}
				break;

			case PAM_PROMPT_ECHO_OFF: /* password */
				reply[i].resp = _strdup(user_pass->pass);
				if (!reply[i].resp) {
					goto out_fail;
				}
				break;

			default:
				fprintf(stderr,"unknown in verify_pam_conv\r\n");
				goto out_fail;
		}
	}

	*resp = reply;
	return PAM_SUCCESS;

out_fail:
	for (int i = 0; i < num_msg; ++i) {
		if (reply[i].resp) {
			memset(reply[i].resp, 0, strlen(reply[i].resp));
			free(reply[i].resp);
		}
	}
	memset(reply, 0, sizeof(struct pam_response) * num_msg);
	free(reply);
	*resp = nullptr;
	return PAM_CONV_ERR;
}

static long ogon_authenticate_pam(
		const char *username, const char *password, int *errorcode) {
	int error;
	char service_name[256];

	get_service_name(service_name);

	auto auth_info = static_cast<struct t_auth_info *>(
			calloc(1, sizeof(struct t_auth_info)));
	if (!auth_info) {
		return 0;
	}
	strncpy(auth_info->user_pass.user, username, 255);
	if (password) {
		strncpy(auth_info->user_pass.pass, password, 255);
	}
	auth_info->pamc.conv = &verify_pam_conv;
	auth_info->pamc.appdata_ptr = &(auth_info->user_pass);
	error = pam_start(
			service_name, nullptr, &(auth_info->pamc), &(auth_info->ph));

	if (error != PAM_SUCCESS) {
		fprintf(stderr, "pam_start failed: %s\n", pam_strerror(auth_info->ph, error));
		goto out;
	}

	error = pam_authenticate(auth_info->ph, 0);

	if (error != PAM_SUCCESS) {
		fprintf(stderr, "pam_authenticate failed: %s\n", pam_strerror(auth_info->ph, error));
		goto out;
	}

	error = pam_acct_mgmt(auth_info->ph, 0);

	if (error != PAM_SUCCESS) {
		fprintf(stderr, "pam_acct_mgmt failed: %s\n", pam_strerror(auth_info->ph, error));
		goto out;
	}

out:
	if (errorcode) {
		*errorcode = error;
	}

	if (auth_info->ph) {
		pam_end(auth_info->ph, error);
	}
	free(auth_info);

	return (error == PAM_SUCCESS ? 1 : 0);
}

/**
 * ogon Authentication Module Interface
 */

struct rds_auth_module_pam
{
	rdsAuthModule common;
};
typedef struct rds_auth_module_pam rdsAuthModulePam;

static rdsAuthModulePam *rds_auth_module_new(void) {
	rdsAuthModulePam* pam;

	pam = (rdsAuthModulePam*) malloc(sizeof(rdsAuthModulePam));

	return pam;
}

static void rds_auth_module_free(rdsAuthModulePam *pam) {
	if (!pam)
		return;

	free(pam);
}

static int rds_auth_logon_user(rdsAuthModulePam *pam, const char *username,
		char **domain, const char *password) {
	OGON_UNUSED(domain);
	int error_code = 0;
	long auth_status = 0;
	char* domainName;

	if (!pam)
		return -1;

	auth_status = ogon_authenticate_pam(username, password, &error_code);

	if (!auth_status)
		return -1;

	domainName = static_cast<char *>(malloc(strlen(PAM_AUTH_DOMAIN) + 1));
	if (!domainName)
		return -1;
	strncpy(domainName, PAM_AUTH_DOMAIN, strlen(PAM_AUTH_DOMAIN) + 1);
	*domain = domainName;
	return 0;
}

int RdsAuthModuleEntry(RDS_AUTH_MODULE_ENTRY_POINTS *pEntryPoints) {
	pEntryPoints->Version = 1;

	pEntryPoints->New = (pRdsAuthModuleNew) rds_auth_module_new;
	pEntryPoints->Free = (pRdsAuthModuleFree) rds_auth_module_free;

	pEntryPoints->LogonUser = (pRdsAuthLogonUser) rds_auth_logon_user;
	pEntryPoints->Name = "PAM";

	return 0;
}
