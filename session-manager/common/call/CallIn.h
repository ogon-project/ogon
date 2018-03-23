/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Baseclass for incoming rpc calls (ogon to session manager)
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

#ifndef _OGON_SMGR_CALL_CALLIN_H_
#define _OGON_SMGR_CALL_CALLIN_H_

#include <call/Call.h>
#include <winpr/wtypes.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallIn: public Call {
	public:
		CallIn();
		virtual ~CallIn();

		void setEncodedRequest(const std::string &encodedRequest);
		virtual bool decodeRequest() = 0;

		virtual bool encodeResponse() = 0;
		std::string getEncodedResponse() const;
		boost::shared_ptr<CallIn> shared_from_this();

		virtual bool prepare() {return true;};
		virtual bool doStuff() = 0;
	protected:
		bool putInSessionExecutor_conId(UINT32 connectionId);
		bool putInSessionExecutor_sesId(UINT32 sessionId);
	};

	typedef boost::shared_ptr<CallIn> CallInPtr;

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif // _OGON_SMGR_CALL_CALLIN_H_
