/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Baseclass for outgoing rpc calls (session manager to ogon)
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

#include "CallOut.h"
#include <string>
#include <winpr/handle.h>

namespace ogon { namespace sessionmanager { namespace call {

	CallOut::CallOut() : mAnswer(NULL) {
		initAnswerHandle();
	}

	CallOut::~CallOut() {
		if (mAnswer) {
			CloseHandle(mAnswer);
			mAnswer = NULL;
		}
	}

	std::string CallOut::getEncodedRequest() const {
		return mEncodedRequest;
	}

	void CallOut::setEncodedeResponse(const std::string &encodedResponse) {
		mEncodedResponse = encodedResponse;
	}

	void CallOut::initAnswerHandle() {
		if (mAnswer == NULL) {
			if (!(mAnswer = CreateEvent(NULL, TRUE, FALSE, NULL))) {
				throw std::bad_alloc();
			}
		}
	}

	HANDLE CallOut::getAnswerHandle() {
		return mAnswer;
	}

	void CallOut::setResult(uint32_t result) {
		mResult = result;
		SetEvent(mAnswer);
	}

	void CallOut::setErrorDescription(const std::string &error) {
		mErrorDescription = error;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
