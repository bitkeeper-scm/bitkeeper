/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private	int	sccs_gone(int quiet, FILE *f);

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

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help rm");
		return (0);
	}

	debug_main(av);
	if (streq(basenm(av[0]), "rm")) useCommonDir = 1;
        while ((c = getopt(ac, av, "d")) != -1) {
                switch (c) {
                    case 'd': useCommonDir++; break;		/* undoc? 2.0 */
                    default:
usage:			system("bk help -s rm");
                        return (1);
                }
        }
	if (ac < 2) goto usage;

	for (name = sfileFirst("sccsrm",&av[optind], 0);
	    name; name = sfileNext()) {
		errors |= sccs_rm(name, NULL, useCommonDir);
	}
	sfileDone();
	return (errors);
}

char *
sccs_rmName(sccs *s, int useCommonDir)
{
	char	path[MAXPATH];
	char	*r, *t, *b;
	int	try = 0;
	delta	*d;

	b = basenm(s->sfile);
	if (useCommonDir) {
		unless (s && s->proj && s->proj->root) {
			fprintf(stderr, "sccsrm: cannot find root?\n");
			return (NULL);
		}
		sprintf(path, "%s/BitKeeper/deleted/SCCS",
				fullname(sPath(s->proj->root, 1), 0));
		t = &path[strlen(path)];
		*t++ = '/';
	} else {
		strcpy(path, s->sfile);
		t = strrchr(path, '/');
		assert(t);
		t++;
	}
	d = sccs_ino(s);
	if (d->random) {
		r = d->random;
	} else {
		char	buf[50];

		sprintf(buf, "%05u", d->sum);
		r = buf;
	}
	for (try = 0; ; try++) {
		if (try) {
			sprintf(t, "s..del-%s~%s~%d", &b[2], r, try);
		} else {
			sprintf(t, "s..del-%s~%s", &b[2], r);
		}
		unless (exists(path)) break;
	}
	return(strdup(path));
}

int
sccs_rm(char *name, char *del_name, int useCommonDir)
{
	char	*rmName;
	char	*sfile;
	int	error = 0;
	sccs	*s;

	sfile = name2sccs(name);
	s = sccs_init(sfile, 0, 0);
	unless (s && HASGRAPH(s) && BITKEEPER(s)) {
		fprintf(stderr,
			"Warning: %s is not a BitKeeper flle, ignored\n", name);
		return (1);
	}
	sccs_close(s);
	rmName = sccs_rmName(s, useCommonDir);
	if (del_name) strcpy(del_name, rmName);
	error |= sccs_mv(sfile, rmName, 0, 1, 0);
	sccs_free(s);
	free(rmName);
	free(sfile);
	return (error);
}

int
gone_main(int ac, char **av)
{
	int	c;
	int	quiet = 0;
	char	tmpfile[MAXPATH];
	FILE	*f;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help gone");
		return (1);
	}

	while ((c =  getopt(ac, av, "q")) != -1) { 
		switch (c) {
		    case 'q': quiet++; break;	/* doc 2.0 */
		    default: 
usage:			      system("bk help -s gone");
			      return (1);
		}
	}
	unless (av[optind]) goto usage;

	if (streq("-", av[optind])) exit(sccs_gone(quiet, stdin));
	if (bktemp(tmpfile)) exit(1);
	f = fopen(tmpfile, "w");
	while (av[optind]) fprintf(f, "%s\n", av[optind++]);
	fclose(f);
	f = fopen(tmpfile, "r");
	if (sccs_gone(quiet, f)) {
		fclose(f);
		unlink(tmpfile);
		exit(1);
	}
	fclose(f);
	unlink(tmpfile);
	exit(0);
}

private int
sccs_gone(int quiet, FILE *f)
{
	sccs	*s;
	char	s_gone[MAXPATH], g_gone[MAXPATH];
	char	key[MAXKEY];
	FILE	*gfile;
	char	*root;

	root = sccs_root(0);
	assert(root);
	sprintf(s_gone, "%s/BitKeeper/etc/SCCS/s.gone", root);
	sprintf(g_gone, "%s/BitKeeper/etc/gone", root);
	comments_save("Gone");
	if (exists(s_gone)) {
		s = sccs_init(s_gone, SILENT|INIT_SAVEPROJ, 0);
		assert(s);
		unless (IS_EDITED(s)) {
			sccs_get(s, 0, 0, 0, 0, SILENT|GET_EDIT, "-"); 
		}
		gfile = fopen(g_gone, "ab");
		while (fnext(key, f)) fputs(key, gfile);
		fclose(gfile);
		s = sccs_restart(s);
		sccs_delta(s, SILENT|DELTA_DONTASK, 0, 0, 0, 0);
	} else {
		gfile = fopen(g_gone, "wb");
		while (fnext(key, f)) fputs(key, gfile);
		fclose(gfile);
		s = sccs_init(s_gone, SILENT|INIT_SAVEPROJ, 0);
		assert(s);
		sccs_delta(s, SILENT|NEWFILE|DELTA_DONTASK, 0, 0, 0, 0);
	}
	sccs_free(s);
	unless (quiet) {
		fprintf(stderr,
		    "Do not forget to commit the gone file "
		    "before attempting a pull\n");
	}
	return (0);
}
