#include "../system.h"
#include "../sccs.h"

int
findprog(char *prog)
{
#ifdef WIN32
	ERROR - awc, you need to do this
#else
        char	*path = strdup(getenv("PATH"));
	char	*s, *t;
	char	buf[MAXPATH];

	for (s = t = path; *t; t++) {
		if (*t == ':') {
			*t = 0;
			sprintf(buf, "%s/%s", s, prog);
			if (executable(buf)) {
				free(path);
				return (1);
			}
			s = &t[1];
		}
	}
	free(path);
	return (0);
#endif
}
