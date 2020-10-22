/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call SwitchTo (session manager to ogon)
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

#ifndef _OGON_SMGR_CALL_CALLOUTSWITCHTO_H_
#define _OGON_SMGR_CALL_CALLOUTSWITCHTO_H_

#include <string>
#include "CallOut.h"
#include <ICP.pb.h>

namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallOutSwitchTo: public CallOut {
	public:
		CallOutSwitchTo();
		virtual ~CallOutSwitchTo();

		virtual unsigned long getCallType() const;

		virtual bool encodeRequest();
		virtual bool decodeResponse();

		void setConnectionId(UINT32 connectionId);
		void setServiceEndpoint(const std::string &serviceEndpoint, const std::string &ogonCookie,
				const std::string &backendCookie);
		void setMaxWidth(long width);
		void setMaxHeight(long height);

		bool isSuccess();

	private:
		UINT32 mConnectionId;
		long mMaxWidth;
		long mMaxHeight;
		std::string mServiceEndpoint;
		std::string mOgonCookie;
		std::string mBackendCookie;
		bool mSuccess;
	};

	typedef std::shared_ptr<CallOutSwitchTo> CallOutSwitchToPtr;

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_CALLOUTSWITCHTO_H_ */
