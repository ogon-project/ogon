/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Property levels
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

#ifndef _OGON_SMGR_PROPERTYLEVEL_H_
#define _OGON_SMGR_PROPERTYLEVEL_H_

#include <string>

typedef enum _PROPERTY_LEVEL
{
	Global = 1,
	UserGroup = 2,
	User = 3
}
PROPERTY_LEVEL, *PPROPERTY_LEVEL;

typedef enum _PROPERTY_STORE_TYPE
{
	BoolType = 1,
	NumberType = 2,
	StringType = 3
} PROPERTY_STORE_TYPE, *PPROPERTY_STORE_TYPE;


typedef struct _PROPERTY_STORE_HELPER {
	PROPERTY_STORE_TYPE type;
	bool boolValue;
	long numberValue;
	std::string stringValue;
} PROPERTY_STORE_HELPER, *PPROPERTY_STORE_HELPER;

#endif /* _OGON_SMGR_PROPERTYLEVEL_H_ */
