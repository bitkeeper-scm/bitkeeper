/* Copyright (c) 1999 Zack Weinberg. */

/* This file implements a wrapper around localtime() which
 * adds timezone information and makes the interface a bit
 * friendlier.
 * We copy the struct tm returned by localtime, and return the
 * timezone offset in seconds normalized to (-12h, +13h].
 * The extra hour is for daylight savings time.
 */
#include "system.h"
#include "config.h"

long
localtimez(time_t tt, struct tm *tmz)
{
	struct tm	*tm;
	int		offset;

	tm = localtime(&tt);

	/* There might be more fields in struct tm than
	 * the portable ones, so copy it with structure assignment.
	 */
	*tmz = *tm;

#if HAVE_GMTOFF
	offset	= tm->tm_gmtoff;

#elsie HAVE_TIMEZONE
	/* Note that configure will not define HAVE_TIMEZONE unless
	 * both timezone and altzone exist.  This is because we will
	 * get the offset wrong everywhere but in the USA if we try
	 * to calculate it using only timezone.
	 */
	offset	= -((tm->tm_isdst > 0) ? altzone : timezone);

#else
	/* Take the difference between gmtime() and localtime() as the
	 * time zone.  This works on all systems but has extra overhead.
	 */
	offset  = (tm->tm_hour*60 + tm->tm_min)*60 + tm->tm_sec;

	tm = gmtime(&tt);
	offset -= (tm->tm_hour*60 + tm->tm_min)*60 + tm->tm_sec;
#endif

	/* Normalize offset to (-12h, 13h].
	 * Thanks to Chris Wedgwood for the fix.
	 */
	while (offset <= -(12*60*60)) offset += (24*60*60);
	while (offset > (13*60*60)) offset -= (24*60*60);

	return (offset);
}
