/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Wrapper to access Property Manager from C
 *
 * Copyright (c) 2013-2018 Thincast Technologies GmbH
 *
 * Authors:
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

#include "PropertyCWrapper.h"

#include <appcontext/ApplicationContext.h>
#include <string>

bool getPropertyBool(UINT32 sessionID, const char *path, bool *value) {
	return APP_CONTEXT.getPropertyManager()->getPropertyBool(sessionID, std::string(path), *value);
}

bool getPropertyNumber(UINT32 sessionID, const char *path, long *value) {
	return APP_CONTEXT.getPropertyManager()->getPropertyNumber(sessionID, std::string(path), *value);
}

bool getPropertyString(UINT32 sessionID, const char *path, char *value, unsigned int valueLength)
{
	std::string stdvalue;
	bool retValue = APP_CONTEXT.getPropertyManager()->getPropertyString(sessionID, std::string(path), stdvalue);

	if (!retValue) {
		return false;
	}

	if (stdvalue.size() + 1 > valueLength) {
		return false;
	}

	memcpy(value, stdvalue.c_str(), stdvalue.size() + 1);
	return true;
}

bool setPropertyBool(PROPERTY_LEVEL /*level*/, UINT32 /*sessionID*/, const char* /*path*/, bool /*value*/) {
	return false;
}

bool setPropertyNumber(PROPERTY_LEVEL /*level*/, UINT32 /*sessionID*/, const char* /*path*/, long /*value*/) {
	return false;
}

bool setPropertyString(PROPERTY_LEVEL /*level*/, UINT32 /*sessionID*/, const char* /*path*/, char* /*value*/) {
	return false;
}
