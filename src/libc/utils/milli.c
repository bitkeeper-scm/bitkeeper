#include "system.h"

char *
milli(void)
{
	struct	timeval	tv;
	u64	now, start;
	double	d;
	static	char time[20];	/* 12345.999\0 plus slop */

	gettimeofday(&tv, 0);
	unless (getenv("BK_SEC")) {
		safe_putenv("BK_SEC=%u", (u32)tv.tv_sec);
		safe_putenv("BK_MSEC=%u", (u32)tv.tv_usec / 1000);
		d = 0;
	} else {
		start = (u64)atoi(getenv("BK_SEC")) * (u64)1000;
		start += (u64)atoi(getenv("BK_MSEC"));
		now = (u64)tv.tv_sec * (u64)1000;
		now += (u64)(tv.tv_usec / 1000);
		d = now - start;
	}
	sprintf(time, "%6.3f", d / 1000.0);
	return (time);
}

