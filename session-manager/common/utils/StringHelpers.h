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

#ifndef _OGON_SMGR_STRINGHELPERS_H_
#define _OGON_SMGR_STRINGHELPERS_H_

#include <vector>
#include <string>
#include <boost/lexical_cast.hpp>

template<typename T>
std::vector<T>
split(const T & str, const T & delimiters) {
	std::vector<T> v;
	typename T::size_type start = 0;
	typename T::size_type pos = str.find_first_of(delimiters, start);
	while (pos != T::npos) {
		if(pos != start) // ignore empty tokens
		v.push_back(str.substr(start, pos - start));
		start = pos + 1;
		pos = str.find_first_of(delimiters, start);
	}
	if(start < str.length()) {// ignore trailing delimiter
		v.push_back(str.substr(start, str.length() - start));
	}
	return v;
}

namespace std{

	bool stringEndsWith(const string &compString, const string &suffix);
	bool stringStartsWith(const string &string2comp, const string &startswith);
	inline std::string trim(std::string &str) {
		str.erase(0, str.find_first_not_of(' '));	//prefixing spaces
		str.erase(str.find_last_not_of(' ') +1 );	//surfixing spaces
		return str;
	}
}

namespace boost {
	template<> bool lexical_cast<bool, std::string>(const std::string &arg);
	template<> std::string lexical_cast<std::string, bool>(const bool &b);
}

std::string wchar_to_utf8(const std::u16string &wstr);
std::u16string utf8_to_wchar(const std::string &str);

#endif /* _OGON_SMGR_STRINGHELPERS_H_ */
