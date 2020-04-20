/**
 * ogon - Free Remote Desktop Services
 * Internal Communication Protocol (ICP)
 * Client Stubs
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
 * Bernhard Miklautz <bernhard.miklautz@thincast.com>
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

#ifndef _OGON_RDPSRV_ICPCLIENTSTUBS_H_
#define _OGON_RDPSRV_ICPCLIENTSTUBS_H_

#include <winpr/wtypes.h>

#include "../commondefs.h"

int ogon_icp_Ping(BOOL* pong);
int ogon_icp_DisconnectUserSession(UINT32 connectionId, BOOL* disconnected);
int ogon_icp_DisconnectUserSession_async(UINT32 connectionId);
int ogon_icp_LogonUser(UINT32 connectionId, const char *username, const char *domain,
		const char *password, const char *clientHostName,
		const char *clientAddress,UINT32 clientBuild, UINT16 clientProductId, UINT32 hardwareID, UINT16 protocol,
		UINT32 width, UINT32 height, UINT32 bpp, ogon_backend_props *props,
		UINT32* maxWidth, UINT32* maxHeight);
int ogon_icp_sendResponse(UINT32 tag, UINT32 type, UINT32 status, BOOL success, void *responseparam1);
int ogon_icp_get_property_bool(UINT32 connectionId, char *path, BOOL *value);
int ogon_icp_get_property_number(UINT32 connectionId, char *path, INT32 *value);
int ogon_icp_get_property_string(UINT32 connectionId, char *path, char **value);
int ogon_icp_RemoteControlEnded(UINT32 spyId, UINT32 spiedId);

/** @brief the type of property */
typedef enum {
	PROPERTY_BOOL,
	PROPERTY_NUMBER,
	PROPERTY_STRING
} PropertyType;

/** @brief a property to request to the SM */
typedef struct {
	/** request
	 * @{ */
	char *path;
	PropertyType propertyType;
	/** @} */

	/** result
	 * @{ */
	BOOL success;
	union {
			BOOL boolValue;
			INT32 intValue;
			char *stringValue;
	} v;
	/** @} */
} PropertyItem;

#define PROPERTY_ITEM_INIT_BOOL(K, V) { .path = K, .propertyType = PROPERTY_BOOL, .success = TRUE, .v = { .boolValue = V } }
#define PROPERTY_ITEM_INIT_INT(K, V) { .path = K, .propertyType = PROPERTY_NUMBER, .success = TRUE, .v = { .intValue = V } }
#define PROPERTY_ITEM_INIT_STRING(K) { .path = K, .propertyType = PROPERTY_STRING, .success = FALSE, .v = { .stringValue = NULL } }

void ogon_PropertyItem_free(PropertyItem *items);

int ogon_icp_get_property_bulk(UINT32 connectionId, PropertyItem *items);


#endif /* _OGON_RDPSRV_ICPCLIENTSTUBS_H_ */
