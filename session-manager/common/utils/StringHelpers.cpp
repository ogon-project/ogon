/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * helper templates for strings
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

#include "StringHelpers.h"

namespace std{

	bool stringEndsWith(const string &compString, const string &suffix)
	{
		return compString.rfind(suffix) == (compString.size()-suffix.size());
	}

	bool stringStartsWith(const string &string2comp, const string &startswith)
	{
		return string2comp.size() >= startswith.size()
			&& equal(startswith.begin(), startswith.end(), string2comp.begin());
	}

}



namespace boost {
	template<> bool lexical_cast<bool, std::string>(const std::string &arg) {
		std::istringstream ss(arg);
		bool b;
		ss >> std::boolalpha >> b;
		return b;
	}

	template<> std::string lexical_cast<std::string, bool>(const bool &b) {
		std::ostringstream ss;
		ss << std::boolalpha << b;
		return ss.str();
	}
}
