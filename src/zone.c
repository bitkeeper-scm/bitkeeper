#include "system.h"
#include "sccs.h"

char	*
sccs_zone(time_t tt)
{
	static	char buf[8];
	long	offset;
	int	hwest, mwest;
	char	sign = '+';

	localtimez(&tt, &offset);
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

int
zone_main(int ac, char **av)
{
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help zone");
		return (0);
	}
	printf("%s\n", sccs_zone(time(0)));
	return (0);
}
