#include "system.h"

char *
milli(void)
{
	struct	timeval	tv;
	u64	now, start;
	double	d;
	static	char time[20];	/* 12345.999\0 plus slop */

	gettimeofday(&tv, 0);
	unless (getenv("_BK_SEC")) {
		safe_putenv("_BK_SEC=%u", (u32)tv.tv_sec);
		safe_putenv("_BK_MSEC=%u", (u32)tv.tv_usec / 1000);
		d = 0;
	} else {
		start = (u64)atoi(getenv("_BK_SEC")) * (u64)1000;
		start += (u64)atoi(getenv("_BK_MSEC"));
		now = (u64)tv.tv_sec * (u64)1000;
		now += (u64)(tv.tv_usec / 1000);
		d = now - start;
	}
	sprintf(time, "%7.3f", d / 1000.0);
	return (time);
}

/*
 * Return the usecs since the last call
 */
u32
milli_gap(void)
{
	struct	timeval	tv;
	u64	now, start;
	static	u32 SECOND, MSECOND;

	gettimeofday(&tv, 0);

	/*
	 * First call, set the values to now
	 */
	unless (SECOND) {
		SECOND = (u32)tv.tv_sec;
		MSECOND = (u32)(tv.tv_usec / 1000);
		return (0);
	}

	/*
	 * start is the previous time in milliseconds,
	 * now is the current time in milliseconds.
	 */
	start = SECOND * 1000;
	start += MSECOND;
	now = (u64)tv.tv_sec * (u64)1000;
	now += (u64)(tv.tv_usec / 1000);

	/*
	 * Save now.
	 */
	SECOND = (u32)tv.tv_sec;
	MSECOND = (u32)tv.tv_usec / 1000;

	return ((u32)(now - start));
}
