/* Copyright (c) 1999 Zack Weinberg. */

/* This file implements a wrapper around localtime() which
 * adds timezone information and makes the interface a bit
 * friendlier.
 * We copy the struct tm returned by localtime, and return the
 * timezone offset in seconds normalized to (-12h, +13h].
 * The extra hour is for daylight savings time.
 */
#include "system.h"

struct tm *
localtimez(const time_t *timep, long *offsetp)
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
	/* Take the difference between gmtime() and localtime() as the
	 * time zone.  This works on all systems but has extra overhead.
	 */
	tm = gmtime(timep);
	offset = -((tm->tm_hour*60 + tm->tm_min)*60 + tm->tm_sec);
	tm = localtime(timep);
	offset += (tm->tm_hour*60 + tm->tm_min)*60 + tm->tm_sec;
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

	/* Normalize offset to (-12h, 13h].
	 * Thanks to Chris Wedgwood for the fix.
	 */
	while (offset <= -(12*60*60)) offset += (24*60*60);
	while (offset > (13*60*60)) offset -= (24*60*60);
	
	if (offsetp) *offsetp = offset;
	return (tm);
}


