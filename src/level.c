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
	char	*root, *lfile;
	FILE	*f;


	unless (root = sccs_root(0)) {
		fprintf(stderr, "setlevel: Error: cannot find package root\n");
		return (1);
	}

	lfile = aprintf("%s/%s", root, LEVEL);
	free(root);
	unless (f = fopen(lfile, "wt")) {
		perror(lfile);
		free(lfile);
		return (1);
	}
	fprintf(f, "# This is the repository level, do not delete.\n");
	fprintf(f, "%d\n", level);
	fclose(f);
	free(lfile);
	return (0);
}

int
getlevel(void)
{
	char	*root, *lfile;

	unless (root = sccs_root(0)) {
		fprintf(stderr, "getlevel: Error: cannot find package root\n");
		return (1); /* should we force a -1 here ? */
	}

	lfile = aprintf("%s/%s", root, LEVEL);
	free(root);
	if (exists(lfile)) {
		char	buf[200];
		FILE	*f;

		unless (f = fopen(lfile, "rt")) return (1000000);
		/* skip the header */
		fgets(buf, sizeof(buf), f);
		if (fgets(buf, sizeof(buf), f)) {
			fclose(f);
			free(lfile);
			return (atoi(buf));
		}
	}
	free(lfile);
	return (1);
}
