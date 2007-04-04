#include "sccs.h"
#include "binpool.h"

/*
 * Simple key sync.
 * Receive a list of keys on stdin and return a list of
 * keys not found locally.
 * Currently only -B (for binpool) is implemented.
 */
int
havekeys_main(int ac, char **av)
{
	char	*dfile;
	int	c;
	int	binpool = 0;
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "BLq")) != -1) {
		switch (c) {
		    case 'B': binpool = 1; break;
		    case 'q': break;	/* ignored for now */
		    default:
usage:			fprintf(stderr, "usage: bk %s [-q] [-B] -\n", av[0]);
			return (1);
		}
	}
	unless (av[optind] && streq(av[optind], "-")) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "%s: must be run in a bk repository.\n",av[0]);
		return (1);
	}
	while (fnext(buf, stdin)) {
		chomp(buf);
		unless (dfile = bp_lookupkeys(0, buf)) {
			puts(buf); /* we don't have this one */
		}
		free(dfile);
	}
	fflush(stdout);
	return (0);
}

