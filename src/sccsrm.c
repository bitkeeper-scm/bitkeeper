/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");

/*
 * Emulate rm(1)
 *
 * usage: rm a b ....
 */
int
main(int ac, char **av)
{
	char	*name;
	char	*dest;
	int	errors = 0;
	int	dofree = 0;

	debug_main(av);
	if (ac < 2) {
usage:		fprintf(stderr, "usage: %s file1 file2 ...\n", av[0]);
		exit(1);
	}
	dest = av[ac-1];
	if ((name = strrchr(dest, '/')) &&
	    (name >= dest + 4) && strneq(name - 4, "SCCS/s.", 7)) {
		dest = sccs2name(dest);
		dofree++;
	}
	for (name = sfileFirst("sccsrm",&av[1], 0); name; name = sfileNext()) {
		errors |= sccs_rm(name, dest);
	}
	if (dofree) free(dest);
	sfileDone();
	purify_list();
	return (errors);
}

#include "comments.c"
			
int
sccs_rm(char *name, char *dest)
{
	char	path[MAXPATH];
	char	cmd[MAXPATH];
	char	*gfile, *sfile;
	sccs	*s;
	delta	*d;
	char	*t, *b;
	int	try = 0;
	int	error = 0;

	s = sccs_init(name, NOCKSUM);
	unless (HAS_SFILE(s)) {
		fprintf(stderr, "sccsrm: not an SCCS file: %s\n", name);
		sccs_free(s);
		return (1);
	}
	if (IS_EDITED(s)) {
		fprintf(stderr, "sccsrm: refusing to move edited %s\n", name);
		sccs_free(s);
		return (1);
	}
	if (access(s->gfile, W_OK) == 0) {
		fprintf(stderr, "sccsrm: writable but not edited %s?\n", name);
		sccs_free(s);
		return (1);
	}
	strcpy(path, s->sfile);
	t = strrchr(path, '/');
	assert(t);
	t++;
	b = basenm(s->sfile);
	for (try = 0; ; try++) {
		if (try) {
			sprintf(t, "s..del-%s~%d", b, try);
		} else {
			sprintf(t, "s..del-%s", b);
		}
		unless (exists(path)) break;
	}
	sprintf(cmd, "bk sccsmv %s %s\n", s->sfile, path);
	error = system(cmd);
out:	sccs_free(s);
	return (error);
}
