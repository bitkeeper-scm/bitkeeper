#include "system.h"
#include "sccs.h"
#include "cmd.h"

/*
 * Copyright (c) 2001 Larry McVoy       All rights reserved.
 */

char	*
whichp(char *prog, int internal, int external)
{
        char	*path;
	char	*s, *t;
	CMD	*cmd;
	char	buf[MAXPATH];
	extern	char *bin;

	if (internal) {
		assert(bin);
		if (cmd = cmd_lookup(prog, strlen(prog))) {
			path = aprintf("%s/bk %s", bin, prog);
			return (path);
		}
	}
	unless (external) return (0);

	if ((prog[0] == '/') && executable(prog)) return (strdup(prog));

        path = aprintf("%s%c", getenv("PATH"), PATH_DELIM);
	s = strrchr(path, PATH_DELIM);
	if (s[-1] == PATH_DELIM) *s = 0;
	for (s = t = path; *t; t++) {
		if (*t == PATH_DELIM) {
			*t = 0;
			sprintf(buf, "%s/%s", *s ? s : ".", prog);
			if (executable(buf)) {
				free(path);
				return (strdup(buf));
			}
			s = &t[1];
		}
	}
	free(path);

	return (0);
}

int
which(char *prog, int internal, int external)
{
        char	*path = whichp(prog, internal, external);

	if (path) {
		free(path);
		return (1);
	}
	return (0);
}

int
which_main(int ac, char **av)
{
	char	*path;
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
	unless (av[optind]) goto usage;
	if (path = whichp(av[optind], internal, external)) {
		puts(path);
		free(path);
		return (0);
	}
	return (1);
}

