#include "system.h"
#include "sccs.h"

#define	LEVEL "BitKeeper/etc/level"

int
level_main(int ac,  char **av)
{
	if (ac == 2 && streq("--help", av[1])) {
usage:		system("bk help level");
		return (0);
	}

	unless (av[1]) {
		printf("Repository level is %d\n", getlevel());
		exit(0);
	}

	unless (isdigit(av[1][0])) goto usage;
	exit(setlevel(atoi(av[1])));
}

int
setlevel(int level)
{
	char	buf[200];
	FILE	*f;

	if (sccs_cd2root(0, 0) == -1) return (1);

	unless (f = fopen(LEVEL, "wt")) {
		perror(LEVEL);
		return (1);
	}
	fprintf(f, "# This is the repository level, do not delete.\n");
	fprintf(f, "%d\n", level);
	fclose(f);
	return (0);
}

getlevel()
{
	if (sccs_cd2root(0, 0) == -1) return (1000000);
	if (exists(LEVEL)) {
		char	buf[200];
		FILE	*f;

		unless (f = fopen(LEVEL, "rt")) return (1000000);
		/* skip the header */
		fgets(buf, sizeof(buf), f);
		if (fgets(buf, sizeof(buf), f)) {
			fclose(f);
			return (atoi(buf));
		}
	}
	return (1);
}
