/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call SendMessage (session manager to ogon)
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

#ifndef _OGON_SMGR_CALL_CALLOUTMESSAGE_H_
#define _OGON_SMGR_CALL_CALLOUTMESSAGE_H_

#include <ogon/message.h>
#include <string>
#include "CallOut.h"
#include <ICP.pb.h>
#include <list>
#include <string>


namespace ogon { namespace sessionmanager { namespace call {

	/**
	 * @brief
	 */
	class CallOutMessage: public CallOut {
	public:
		CallOutMessage();
		virtual ~CallOutMessage();

		virtual unsigned long getCallType() const;
		virtual bool encodeRequest();
		virtual bool decodeResponse();

		void setConnectionId(UINT32 connectionId);
		void setType(UINT32 type);
		void setParameterNumber(UINT32 number);
		void setParameter1(const std::string &param);
		void setParameter2(const std::string &param);
		void setParameter3(const std::string &param);
		void setParameter4(const std::string &param);
		void setParameter5(const std::string &param);
		void setTimeout(UINT32 timeout);
		void setStyle(UINT32 style);
		UINT32 getResult() const;

	private:
		UINT32 mConnectionId;
		UINT32 mType;
		UINT32 mStyle;
		UINT32 mParameterNumber;
		std::string mParameter1;
		std::string mParameter2;
		std::string mParameter3;
		std::string mParameter4;
		std::string mParameter5;
		UINT32 mTimeout;
		UINT32 mResult;
	};

	typedef boost::shared_ptr<CallOutMessage> CallOutMessagePtr;

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* _OGON_SMGR_CALL_CALLOUTMESSAGE_H_ */
