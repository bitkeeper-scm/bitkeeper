#include "system.h"
#include "sccs.h"

main()
{
	time_t	tt = time(0);
	struct	tm *tm;
	char	tmp[50];
	extern	long timezone;
	extern	int daylight;
	int	hwest, mwest;
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
		/*
		 * What I want is to have 8 hours west of GMT to be -08:00.
		 */
		hwest = timezone / 3600;
		mwest = timezone % 3600;
		if (hwest < 0) {
			sign = '+';
			hwest = -hwest;
			mwest = -mwest;
		}
		/*
		 * XXX - I have not thought this through.
		 * This is blindly following what /bin/date does.
		 */
		if (daylight) hwest--;
		sprintf(tmp, "%c%02d:%02d", sign, hwest, mwest);
	}
	printf("%s\n", tmp);
}
