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

#ifndef _OGON_SMGR_CALL_CALL_H_
#define _OGON_SMGR_CALL_CALL_H_

#include <string>
#include <stdint.h>
#include <memory>

namespace ogon { namespace sessionmanager { namespace call {

	class Call: public std::enable_shared_from_this<Call> {
	public:
		Call();
		virtual ~Call();

		virtual unsigned long getCallType() const = 0;

		void setTag(uint32_t tag);
		uint32_t getTag();

		uint32_t getResult();
		std::string getErrorDescription();

		void abort() {
			mResult = -2;
		}

	private:
		uint32_t mTag;
	protected:
		std::string mEncodedRequest;
		std::string mEncodedResponse;

		uint32_t mResult;
		// this is used if result ist not 0
		std::string mErrorDescription;
	};

	typedef std::shared_ptr<Call> CallPtr;

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_CALL_H_ */
