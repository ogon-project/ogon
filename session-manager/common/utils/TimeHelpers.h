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

#ifndef OGON_SMGR_TIMEHELPERS_H_
#define OGON_SMGR_TIMEHELPERS_H_

#include <time.h>
#include <winpr/wtypes.h>
#include <boost/date_time/posix_time/posix_time.hpp>

void GetUnixTimeAsFileTime(time_t t, LPFILETIME lpSystemTimeAsFileTime);
time_t to_time_t(boost::posix_time::ptime t);
__uint64 convertFileTimeToint64( const FILETIME &fileTime);

#endif /* OGON_SMGR_TIMEHELPERS_H_ */
