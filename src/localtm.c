/* Copyright (c) 1999 Zack Weinberg. */

/* This file implements a wrapper around localtime() which
 * adds timezone information and makes the interface a bit
 * friendlier.
 * We copy the struct tm returned by localtime, and return the
 * timezone offset in seconds normalized to (-43200, 43200].
 */

#include "system.h"

long
localtimez(time_t tt, struct tm *tmz)
{
	struct tm *tm;
	int offset;

	tm = localtime(&tt);

	/* There might be more fields in struct tm than
	 * the portable ones, so copy it with structure assignment.
	 */
	*tmz = *tm;

#if defined HAVE_TM_GMTOFF
	offset	= tm->tm_gmtoff;

#elif defined HAVE_EXTERN_TIMEZONE
	/*
	 * Note that configure will define HAVE_EXTERN_TIMEZONE only if
	 * all three of daylight, timezone, and altzone exist.  This is
	 * because we will get the offset wrong everywhere but in the USA
	 * if we try to calculate it using only timezone.
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

	/* Normalize offset to (-43200, 43200].  */
	while (offset <= -43200) offset += 86400;
	while (offset > 43200) offset -= 86400;

	return offset;
}
