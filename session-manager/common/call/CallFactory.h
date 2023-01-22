/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Factory for rpc calls
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

#ifndef OGON_SMGR_CALL_CALLFACTORY_H_
#define OGON_SMGR_CALL_CALLFACTORY_H_

#include <utils/FactoryBase.h>
#include <utils/SingletonBase.h>
#include <call/Call.h>

#include <string>

#define CALL_FACTORY ogon::sessionmanager::call::CallFactory::instance()

namespace ogon { namespace sessionmanager { namespace call {

	/**
	* @class	PacketFactory.
	*
	* @brief	Factory for creating packetes.
	*
	* @author	Martin Haimberger
	*/

	class CallFactory :public FactoryBase<Call, unsigned long>, public SingletonBase<CallFactory> {
		SINGLETON_ADD_INITIALISATION(CallFactory)
	};

} /*call*/ } /*sessionmanager*/ } /*ogon*/

namespace callNS = ogon::sessionmanager::call;

#endif /* OGON_SMGR_CALL_CALLFACTORY_H_ */
