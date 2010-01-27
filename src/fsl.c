#include "system.h"
#include "sccs.h"

private void
usage1(char *cmd)
{
	fprintf(stderr, "bk %s file\n", cmd);
	exit(1);
}

private void
usage2(char *cmd)
{
	fprintf(stderr, "bk %s from to\n", cmd);
	exit(1);
}

private void
usage_chmod(void)
{
	fprintf(stderr, "bk _chmod <octal_mode> file\n");
	exit(1);
}

int
fslcp_main(int ac, char **av)
{
	if (ac < 3) usage2(av[0]);
	if (fileCopy(av[1], av[2])) return (1);
	return (0);
}

int
fslmv_main(int ac, char **av)
{
	if (ac < 3) usage2(av[0]);
	unlink(av[2]);
	return (rename(av[1], av[2]) ? 1 : 0);
}

int
fslrm_main(int ac, char **av)
{
	int	c;
	int	rc = 0, force = 0;

	while ((c = getopt(ac, av, "f", 0)) != -1) {
		switch (c) {
		    case 'f': force = 1; break;
		    default: usage1(av[0]); break;
		}
	}

	unless (av[optind]) usage1(av[0]);
	while (--ac >= optind) {
		if (unlink(av[ac]) && !force) {
			perror(av[ac]);
			rc = 1;
		}
	}
	return (rc);
}

int
fslchmod_main(int ac, char **av)
{
	int	i;
	int	rc = 0;
	mode_t	mode;

	if (ac < 3) usage_chmod();
	mode = strtol(av[1], 0, 8);
	if (mode == 0) {
		fprintf(stderr,
		    "bk _fslchmod: %s is not a valid non-zero octal mode\n",
		    av[1]);
		exit(1);
	}
	for (i = 2; i < ac; i++) {
		if (chmod(av[i], mode)) {
			perror(av[i]);
			rc = 1;
		}
	}
	return (rc);
}

int
fslmkdir_main(int ac, char **av)
{
	int	rc = 0;
	if (ac < 2) usage1(av[0]);
	while (--ac) {
		if (mkdir(av[ac], 0775)) {
			perror(av[ac]);
			rc = 1;
		}
	}
	return (rc);
}
