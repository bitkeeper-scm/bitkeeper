/* Copyright (c) 1999 Zack Weinberg. */

/* This file implements a wrapper around localtime() which
 * adds timezone information and makes the interface a bit
 * friendlier.
 * We copy the struct tm returned by localtime, and return the
 * timezone offset in seconds normalized to (-12h, +13h].
 * The extra hour is for daylight savings time.
 */
#include "sccs.h"

struct tm *
localtimez(time_t *timep, long *offsetp)
{
	struct tm	*tm;
	int		offset;
	int		offset_sec;
	int		year, yday;
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


