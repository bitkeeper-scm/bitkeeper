#include "system.h"
#include "sccs.h"

/*
 * Given a time_t, compute the timezone in effect at our
 * present location as of the time it refers to.
 * Returns minutes east or west of GMT; west is negative.  Directly
 * opposite Greenwich is considered positive (matches date -R).
 *
 * This implementation may look a bit silly, but it is
 * maximally portable.
 *
 * Yes, there are timezones that aren't a whole number of hours
 * off of GMT.  Try Pacific/Pitcairn, for example.
 */
private int
gettimezone(time_t when)
{
	struct	tm *tm;
	struct	tm gt, lt;
	int	delta;

	tm = gmtime(&when);
	gt = *tm;
	tm = localtime(&when);
	lt = *tm;

	delta = (lt.tm_hour - gt.tm_hour)*60 + (lt.tm_min - gt.tm_min);
	if (delta > 12*60) delta = -(24*60 - delta);
	return delta;
}


main()
{
	time_t	tt = time(0);
	struct	tm *tm;
	char	tmp[50];
	int	hwest, mwest;
	div_t	tz;
	char	sign = '-';


	tm = localtime(&tt);
	strftime(tmp, sizeof(tmp), "%y/%m/%d %H:%M:%S", tm);
	strftime(tmp, sizeof(tmp), "%z", tm);
	if (strlen(tmp) == 5) {
		tmp[6] = 0;
		tmp[5] = tmp[4];
		tmp[4] = tmp[3];
		tmp[3] = ':';
	} else {
		mwest = gettimezone(tt);
		if (mwest < 0) {
			mwest = -mwest;
			sign = '-';
		} else {
			sign = '+';
		}
		tz = div(mwest, 60);
		hwest = tz.quot;
		mwest = tz.rem;
		sprintf(tmp, "%c%02d:%02d", sign, hwest, mwest);
	}
	printf("%s\n", tmp);
}
