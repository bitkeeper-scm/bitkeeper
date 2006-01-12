#include "system.h"

private int
absolute(char *path)
{
	/* Any C:whatever is viewed as absolute */
	if (win32() && isalpha(*path) && (path[1] == ':')) return (1);

	return ((*path == '/') ||
	    strneq("./", path, 2) || strneq("../", path, 3));
}

char *
which(char *exe)
{
        char	*path;
	char	*s, *t;
	char	buf[MAXPATH];

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
