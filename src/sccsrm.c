/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

int	sccs_rm(char *name, int useCommonDir);	/* XXX - mv to slib.c? */

/*
 * Emulate rm(1)
 *
 * usage: rm a b ....
 */
int
rm_main(int ac, char **av)
{
	char	*name;
	int	c, errors = 0;
	int 	useCommonDir = 0;

	debug_main(av);
	if (streq(basenm(av[0]), "rm")) useCommonDir = 1;
        while ((c = getopt(ac, av, "d")) != -1) {
                switch (c) {
                    case 'd': useCommonDir++; break;
                    default:
                        fprintf(stderr,
			    "usage: %s [-d] file1 file2 ...\n", av[0]);
                        return (1);
                }
        }
	if (ac < 2) {
		fprintf(stderr, "usage: %s [-d] file1 file2 ...\n", av[0]);
		exit(1);
	}
	for (name = sfileFirst("sccsrm",&av[optind], 0);
	    name; name = sfileNext()) {
		errors |= sccs_rm(name, useCommonDir);
	}
	sfileDone();
	purify_list();
	return (errors);
}

int
sccs_rm(char *name, int useCommonDir)
{
	char	path[MAXPATH], root[MAXPATH];
	char	*sfile;
	char	*t, *b;
	int	try = 0;
	int	error = 0;
	extern	char *_relativeName();

	sfile = name2sccs(name);
	b = basenm(sfile);
	if (useCommonDir) {
		_relativeName(&b[2], 0, 0, 1, root);
		unless(root[0]) {
			fprintf(stderr, "sccsrm: can not find root?\n");
			return (1);
		}
		sprintf(path, "%s/BitKeeper/deleted/SCCS", sPath(root, 1));
		t = &path[strlen(path)];
		*t++ = '/';
	} else {
		strcpy(path, sfile);
		t = strrchr(path, '/');
		assert(t);
		t++;
	}
	for (try = 0; ; try++) {
		if (try) {
			sprintf(t, "s..del-%s~%d", &b[2], try);
		} else {
			sprintf(t, "s..del-%s", &b[2]);
		}
		unless (exists(path)) break;
	}
	error |= sccs_mv(sfile, path, 0, 1);
	free(sfile);
	return (error);
}
