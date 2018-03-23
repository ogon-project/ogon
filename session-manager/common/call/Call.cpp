/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Baseclass of an rpc call
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "Call.h"
#include <string>
#include <winpr/handle.h>

namespace ogon { namespace sessionmanager { namespace call {

	Call::Call():mTag(0), mResult(0) {
	}

	Call::~Call() {
	}

	void Call::setTag(uint32_t tag) {
		mTag = tag;
	}

	uint32_t Call::getTag() {
		return mTag;
	}

	uint32_t Call::getResult() {
		return mResult;
	}

	std::string Call::getErrorDescription() {
		return mErrorDescription;
	}

} /*call*/ } /*sessionmanager*/ } /*call*/
