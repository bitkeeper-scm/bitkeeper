/* Copyright (c) 2000 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * repo - repository level stuff
 *
 */
int
repo_main(int ac, char **av)
{
	int	c;
	int	what = 0;
	int	force = 0;

	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr,
		    "usage: %s -r|w|l|u [repository root]\n", av[0]);
		return (1);
	}
	while ((c = getopt(ac, av, "frwRWl")) != -1) {
		switch (c) {
		    case 'f': force = 1; break;
		    case 'r':
		    case 'w':
		    case 'R':
		    case 'W':
		    case 'l':
			unless (what) {
				what = c;
				break;
			}
			/* fall through */
		    default: goto usage;
		}
	}
	if (av[optind]) chdir(av[optind]);
	sccs_cd2root(0, 0);
	switch (what) {
	    case 'r':	/* read lock the repository */
		if (repository_rdlock()) {
			fprintf(stderr, "read lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		exit(0);
	    
	    case 'w':
		if (repository_wrlock()) {
			fprintf(stderr, "write lock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		exit(0);

	    case 'R':
		if (repository_rdunlock(force)) {
			fprintf(stderr, "read unlock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		exit(0);
	    
	    case 'W':
		if (repository_wrunlock()) {
			fprintf(stderr, "write unlock failed.\n");
			repository_lockers(0);
			exit(1);
		}
		exit(0);
	    
	    case 'l':
		repository_lockers(0);
		/* fall through */
	    
	    default:
		if (repository_locked(0)) exit(1);
		exit(0);
	}
}
