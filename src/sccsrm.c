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

	sfile = name2sccs(name);
	b = basenm(sfile);
	if (useCommonDir) {
		_relativeName(&b[2], 0, 0, 1, 0, root);
		unless(root[0]) {
			fprintf(stderr, "sccsrm: cannot find root?\n");
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

private project *proj;
private char *root;
int
gone_main(int ac, char **av)
{
	int	error = 0;
	int	quiet = 0;

	if (av[1] && streq(av[1], "-q")) quiet++, av++;
	unless (av[1]) {
		fprintf(stderr, "usage: bk gone [-q] [-|key...]\n");
		exit(1);
	}
	if (streq("-", av[1])) {
		char buf[MAXPATH];

		while (fgets(buf, sizeof(buf), stdin)) {
			chop(buf);
			if (sccs_gone(buf)) {
				error = 1;
			} else unless (quiet) {
				fprintf(stderr,
				    "Do not forget to commit the gone file "
				    "before attempting a pull\n");
				quiet = 1;
			}
		}
	} else {
		int	i = 1;

		while (av[i]) {
			if (sccs_gone(av[i++])) {
				error = 1;
			} else unless (quiet) {
				fprintf(stderr,
				    "Do not forget to commit the gone file "
				    "before attempting a pull\n");
				quiet = 1;
			}
		}
	}

	if (proj) proj_free(proj);
	if (root) free(root);
	return (error);
}


int
sccs_gone(char *key)
{

	sccs	*s1;
	char	s_gone[MAXPATH], g_gone[MAXPATH];
	FILE	*f;

	unless (root) root = proj ? strdup(proj->root) : sccs_root(0);
	assert(root);
	sprintf(s_gone, "%s/BitKeeper/etc/SCCS/s.gone", root);
	sprintf(g_gone, "%s/BitKeeper/etc/gone", root);
	comments_save("Gone");
	if (exists(s_gone)) {
		s1 = sccs_init(s_gone, SILENT|INIT_SAVEPROJ, proj);
		assert(s1);
		unless (IS_EDITED(s1)) {
			sccs_get(s1, 0, 0, 0, 0, SILENT|GET_EDIT, "-"); 
		}
		f = fopen(g_gone, "ab");
		fprintf(f, "%s\n", key);
		fclose(f);
		s1 = sccs_restart(s1);
		sccs_delta(s1, SILENT|DELTA_DONTASK, 0, 0, 0, 0);
	} else {
		f = fopen(g_gone, "wb");
		fprintf(f, "%s\n", key);
		fclose(f);
		s1 = sccs_init(s_gone, SILENT|INIT_SAVEPROJ, proj);
		assert(s1);
		sccs_delta(s1, SILENT|NEWFILE|DELTA_DONTASK, 0, 0, 0, 0);
	}
	unless (proj) proj = s1->proj;
	sccs_free(s1);
	return (0);
}
