#include "system.h"
#include "sccs.h"

/*
 * Copyright (c) 2001 Larry McVoy       All rights reserved.
 */

char	*
whichp(char *prog, int internal, int external)
{
        char	*path;
	char	*s, *t;
	MMAP	*m;
	int	i, lineno, len;
	char	buf[MAXPATH];
	extern	char *bin;
	extern	struct command cmdtbl[];
	extern	struct tool guis[];

	unless (internal) goto PATH;

	for (i = 0; cmdtbl[i].name; i++) {
		if (streq(cmdtbl[i].name, prog)) {
			path = aprintf("%s/bk %s", bin, prog);
			return (path);
		}
	}

	for (i = 0; guis[i].prog; i++) {
		if (streq(guis[i].prog, prog) ||
		    (guis[i].alias && streq(guis[i].alias, prog))) {
			prog = guis[i].prog;
		    	path = aprintf("%s/bk %s", bin, prog);
			return (path);
		}
	}

	assert(bin);

	path = aprintf("%s/bk.script", bin);
	m = mopen(path, "rt");
	free(path);
	unless (m) return (0);
	lineno = 0;
	len = strlen(prog);
	while (s = mnext(m)) {
		lineno++;
		/* _inode() { */
		unless ((s + len + 3) < (m->mmap + m->size)) break;
		unless ((s[0] == '_') && (s[1] != '_')) continue;
		unless (strneq(++s, prog, len)) continue;
		s += len;
		if (strneq(s, "()", 2)) {
			mclose(m);
			path = aprintf("%s/bk %s (line %d of bk.script)",
				    bin, prog, lineno);
			return (path);
		}
	}
	mclose(m);

PATH:	unless (external) return (0);

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
		}
	}
	if (path = whichp(av[optind], internal, external)) {
		puts(path);
		free(path);
		return (0);
	}
	return (1);
}

