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

#ifndef OGON_SMGR_CALL_CALLOUT_H_
#define OGON_SMGR_CALL_CALLOUT_H_

#include <call/Call.h>
#include <winpr/synch.h>

namespace ogon { namespace sessionmanager { namespace call {

	class CallOut : public Call {
	public:
		CallOut();
		~CallOut();

		virtual bool encodeRequest() = 0;
		std::string getEncodedRequest() const;

		void setEncodedeResponse(const std::string &encodedResponse);
		virtual bool decodeResponse() = 0;

		void	initAnswerHandle();
		HANDLE	getAnswerHandle();

		void	setResult(uint32_t result);
		void	setErrorDescription(const std::string &error);

	private :
		HANDLE mAnswer;
	};

	typedef std::shared_ptr<CallOut> CallOutPtr;

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* OGON_SMGR_CALL_CALLOUT_H_ */
