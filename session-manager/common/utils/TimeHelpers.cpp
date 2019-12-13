/**
 * ogon - Free Remote Desktop Services
 * Session Manager
 * helper functions for time
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

#include "TimeHelpers.h"

void GetUnixTimeAsFileTime(time_t t, LPFILETIME lpSystemTimeAsFileTime)
{
	ULARGE_INTEGER time64;

	time64.u.HighPart = 0;

	/* time represented in tenths of microseconds since midnight of January 1, 1601 */

	time64.QuadPart = t + 11644473600LL; /* Seconds since January 1, 1601 */
	time64.QuadPart *= 10000000; /* Convert timestamp to tenths of a microsecond */

	lpSystemTimeAsFileTime->dwLowDateTime = time64.u.LowPart;
	lpSystemTimeAsFileTime->dwHighDateTime = time64.u.HighPart;
}

time_t to_time_t(boost::posix_time::ptime t)
{
	using namespace boost::posix_time;
	boost::posix_time::ptime epoch(boost::gregorian::date(1970, boost::gregorian::Jan, 1));
	boost::posix_time::time_duration::sec_type x = (t - epoch).total_seconds();
	return time_t(x);
}


__uint64 convertFileTimeToint64(const FILETIME &fileTime)
{
	ULARGE_INTEGER lv_Large ;

	lv_Large.u.LowPart  = fileTime.dwLowDateTime;
	lv_Large.u.HighPart = fileTime.dwHighDateTime;

	return lv_Large.QuadPart ;
}
