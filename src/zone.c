#include "system.h"

int
main(void)
{
	time_t	tt = time(0);
	struct	tm tm;
	char	tmp[50];
	long	offset;
	int	hwest, mwest;
	char	sign = '+';

	offset = localtimez(tt, &tm);
	
	strftime(tmp, sizeof(tmp), "%y/%m/%d %H:%M:%S", &tm);

	/*
	 * What I want is to have 8 hours west of GMT to be -08:00.
	 */
	hwest = offset / 3600;
	mwest = (offset % 3600) / 60;
	if (hwest < 0) {
		sign = '-';
		hwest = -hwest;
		mwest = -mwest;
	}
	sprintf(tmp, "%c%02d:%02d", sign, hwest, mwest);
	printf("%s\n", tmp);
}
