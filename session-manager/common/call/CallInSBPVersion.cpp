/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * Class for rpc call SBP Version (ogon to session manager)
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

#include <call/CallInSBPVersion.h>
#include <ogon/version.h>
#include <winpr/wlog.h>
#include <appcontext/ApplicationContext.h>

using ogon::sbp::VersionInfoRequest;
using ogon::sbp::VersionInfoResponse;

namespace ogon { namespace sessionmanager { namespace call {

	static wLog *logger_CallInSBPVersion = WLog_Get("ogon.sessionmanager.call.callinsbpversion");


	CallInSBPVersion::CallInSBPVersion() : mSessionId(0), mVersionMajor(0),
		mVersionMinor(0) {
	}

	CallInSBPVersion::~CallInSBPVersion() {
	}

	unsigned long CallInSBPVersion::getCallType() const {
		return ogon::sbp::VersionInfo;
	}

	bool CallInSBPVersion::decodeRequest() {
		// decode protocol buffers
		VersionInfoRequest req;

		if (!req.ParseFromString(mEncodedRequest)) {
			// failed to parse
			mResult = 1;// will report error with answer
			return false;
		}
		mSessionId = req.sessionid();
		mVersionMajor = req.vmajor();
		mVersionMinor = req.vminor();

		return true;
	}

	bool CallInSBPVersion::encodeResponse() {
		// encode protocol buffers
		VersionInfoResponse resp;
		resp.set_vmajor(OGON_PROTOCOL_VERSION_MAJOR);
		resp.set_vminor(OGON_PROTOCOL_VERSION_MINOR);

		if (!resp.SerializeToString(&mEncodedResponse)) {
			// failed to serialize
			mResult = 1;
			return false;
		}
		return true;
	}

	bool CallInSBPVersion::prepare() {
		sessionNS::SessionPtr currentSession = APP_CONTEXT.getSessionStore()->getSession(mSessionId);

		if (currentSession == nullptr) {
			WLog_Print(logger_CallInSBPVersion, WLOG_ERROR,
				"Session %" PRIu32 " not found, cannot process SBP Version Info packet!",
				mSessionId);
			return false;
		}
		setAccessorSession(currentSession);
		setSBPVersion(mVersionMajor == OGON_PROTOCOL_VERSION_MAJOR);
		resetAccessorSession();
		// send answer back just now
		return false;
	}

	bool CallInSBPVersion::doStuff() {
		return true;
	}

} /*call*/ } /*sessionmanager*/ } /*ogon*/
