#include "system.h"

int
main(int volatile ac, char **av, char **env)
{
	char	*p;

	putenv("BK_GUI=YES");

	/* s/bkg\.exe$/bk.exe/ */
	if ((p = strrchr(av[0], 'g')) &&
	    (p - 2 >= av[0]) && strneq(p-2, "bkg", 3)) {
		memmove(p, p+1, strlen(p+1) + 1);	/* remove the g */
	} else {
		av[0] = "bk";
	}
	if (spawnvp(_P_DETACH, "bk", av) < 0) return (1);
	return (0);
}
