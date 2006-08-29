#include "sccs.h"
#include "cmd.h"

/*
 * Copyright (c) 2001 Larry McVoy       All rights reserved.
 */
int
which_main(int ac, char **av)
{
	char	*path, *exe;
	CMD	*cmd;
	int	c, external = 1, internal = 1;

	while ((c = getopt(ac, av, "ei")) != -1) {
		switch (c) {
		    case 'i': external = 0; break;
		    case 'e': internal = 0; break;
usage:		    default:
			fprintf(stderr, "usage: bk which [-i] [-e] cmd\n");
			return (2);
		}
	}
	unless (av[optind] && !av[optind+1]) goto usage;

	exe = av[optind];
	if (internal) {
		assert(bin);
		if (cmd = cmd_lookup(exe, strlen(exe))) {
			printf("%s/bk %s\n", bin, exe);
			return (0);
		}
	}
	if (external) {
		if (path = which(exe)) {
			puts(path);
			free(path);
			return (0);
		}
	}
	return (1);
}

