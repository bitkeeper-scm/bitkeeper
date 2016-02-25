/*
 * Copyright 1999-2001,2005,2008,2012,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sccs.h"

/* This is a wrapper around localtime()
 * bk source should not call localtime() directly
 *
 * It does two things:
 *   - returns the offsets from GMT in offsetp, even for
 *     platforms that don't have tm->tm_gmtoff
 *     timezone offset in seconds normalized to (-12h, +13h].
 *     The extra hour is for daylight savings time.
 *
 *   - For machines with a weird timezone we round the gmt offset to the
 *     nearest minute and adjust the tm struct to match.
 */
struct tm *
localtimez(time_t *timep, long *offsetp)
{
	struct tm	*tm;
	int		offset;
	int		offset_sec;
	time_t		before = *timep;

#ifdef HAVE_GMTOFF
	tm = localtime(timep);
	offset	= tm->tm_gmtoff;
#else
#ifdef	HAVE_TIMEZONE
	/* Note that configure will not define HAVE_TIMEZONE unless
	 * both timezone and altzone exist.  This is because we will
	 * get the offset wrong everywhere but in the USA if we try
	 * to calculate it using only timezone.
	 */
	tm = localtime(timep);
	offset	= -((tm->tm_isdst > 0) ? altzone : timezone);
#else
	int		year, yday;

	/* Take the difference between gmtime() and localtime() as the
	 * time zone.  This works on all systems but has extra overhead.
	 */
	tm = gmtime(timep);
	year = tm->tm_year;
	yday = tm->tm_yday;
	offset = -((tm->tm_hour*60 + tm->tm_min)*60 + tm->tm_sec);
	tm = localtime(timep);
	offset += (tm->tm_hour*60 + tm->tm_min)*60 + tm->tm_sec;

	/*
	 * We find the difference of the time of the day and then look
	 * at the year and the day. If they don't match we assume that
	 * the localtime must be one day before or after gmtime.
	 */
	if (year != tm->tm_year) {
		offset += (tm->tm_year - year)*(24*60*60);
	} else if (yday != tm->tm_yday) {
		offset += (tm->tm_yday - yday)*(24*60*60);
	}
#endif
#endif
	assert(*timep == before);
	/*
	 * Handle weird people who have seconds in their timezone.
	 * We just silents remove the seconds portion and only use
	 * up the the minutes.
	 */
	offset_sec = offset % 60;
	if (offset_sec) {
		time_t	adjtime = *timep - offset_sec;
		offset -= offset_sec;
		tm = localtime(&adjtime);
	}

	/*
	 * The offset should always be between (-12h, 13h].
	 * 13 is for daylight time.
	 *
	 * assert(offset > -(12*60*60));
	 * assert(offset <= (13*60*60));
	 *
	 * But glibc actually works correctly with larger timezones,
	 * and we have checks in other places so I am commenting out
	 * this assert.
	 */

	if (offsetp) *offsetp = offset;
	return (tm);
}


/*
 * Return time zone in [+/-]hh:mm format
 * We need this becuase, %Z with strftime() does not return
 * the format we want on Windows.
 */
char    *
tzone(long offset)
{
	static  char buf[8];
	int     hwest, mwest;
	char    sign = '+';

	/*
	 * What I want is to have 8 hours west of GMT to be -08:00.
	 */
	if (offset < 0) {
		sign = '-';
		offset = -offset;
	}
	hwest = offset / 3600;
	mwest = (offset % 3600) / 60;
	assert(offset - hwest * 3600 - mwest * 60 == 0);
	sprintf(buf, "%c%02d:%02d", sign, hwest, mwest);
	return (buf);
}
