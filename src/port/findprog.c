#include "../system.h"
#include "../sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy       All rights reserved.
 */

int
findprog(char *prog)
{
        char	*path;
	char	*s, *t;
	char	buf[MAXPATH];

        path = aprintf("%s%c", getenv("PATH"), PATH_DELIM);
	s = strrchr(path, PATH_DELIM);
	if (s[-1] == PATH_DELIM) *s = 0;
	for (s = t = path; *t; t++) {
		if (*t == PATH_DELIM) {
			*t = 0;
			sprintf(buf, "%s/%s", *s ? s : ".", prog);
			if (executable(buf)) {
				free(path);
				return (1);
			}
			s = &t[1];
		}
	}
	free(path);
	return (0);
}

char	*
prog2path(char *prog)
{
        char	*path;
	char	*s, *t;
	char	buf[MAXPATH];

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
