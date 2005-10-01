#include "system.h"
#include "sccs.h"
#include "cmd.h"

private int
absolute(char *path)
{
	/* Any C:whatever is viewed as absolute */
	if (win32() && isalpha(*path) && (path[1] == ':')) return (1);

	return ((*path == '/') ||
	    strneq("./", path, 2) || strneq("../", path, 3));
}

/*
 * Copyright (c) 2001 Larry McVoy       All rights reserved.
 */

char	*
whichp(char *exe, int internal, int external)
{
        char	*path;
	char	*s, *t;
	CMD	*cmd;
	char	buf[MAXPATH];

	if (internal) {
		assert(bin);
		if (cmd = cmd_lookup(exe, strlen(exe))) {
			path = aprintf("%s/bk %s", bin, exe);
			return (path);
		}
	}
	unless (external) return (0);

	if (executable(exe) && absolute(exe)) return (strdup(exe));

        path = aprintf("%s%c", getenv("PATH"), PATH_DELIM);
	s = strrchr(path, PATH_DELIM);
	if (s[-1] == PATH_DELIM) *s = 0;
	for (s = t = path; *t; t++) {
		if (*t == PATH_DELIM) {
			*t = 0;
			sprintf(buf, "%s/%s", *s ? s : ".", exe);
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
		}
	}
	if (path = whichp(av[optind], internal, external)) {
		puts(path);
		free(path);
		return (0);
	}
	return (1);
}

