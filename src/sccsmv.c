/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");

/*
 * Emulate mv(1)
 *
 * usage: mv a b
 * usage: mv a [b c d ...] dir
 */
int
main(int ac, char **av)
{
	char	*name;
	char	*dest;
	int	isDir;
	int	errors = 0;
	int	dofree = 0;

	debug_main(av);
	if (ac < 3) {
usage:		fprintf(stderr, "usage: %s from to\n", av[0]);
		exit(1);
	}
	dest = av[ac-1];
	if ((name = strrchr(dest, '/')) &&
	    (name >= dest + 4) && strneq(name - 4, "SCCS/s.", 7)) {
		dest = sccs2name(dest);
		dofree++;
	}
	isDir = isdir(dest);
	unless (isDir || (ac == 3)) goto usage;
	av[ac-1] = 0;
	for (name = sfileFirst("sccsmv",&av[1], 0); name; name = sfileNext()) {
		errors |= sccs_mv(name, dest, isDir);
	}
	if (dofree) free(dest);
	sfileDone();
	purify_list();
	return (errors);
}

#include "comments.c"
			
int
sccs_mv(char *name, char *dest, int isDir)
{
	char	path[MAXPATH];
	char	path2[MAXPATH];
	char	*gfile, *sfile;
	sccs	*s;
	delta	*d;
	int	error = 0;

	s = sccs_init(name, NOCKSUM);
	unless (HAS_SFILE(s)) {
		fprintf(stderr, "sccsmv: not an SCCS file: %s\n", name);
		sccs_free(s);
		return (1);
	}
	if (IS_EDITED(s)) {
		fprintf(stderr, "sccsmv: refusing to move edited %s\n", name);
		sccs_free(s);
		return (1);
	}
	if (access(s->gfile, W_OK) == 0) {
		fprintf(stderr, "sccsmv: writable but not edited %s?\n", name);
		sccs_free(s);
		return (1);
	}
	if (isDir) {
		sprintf(path, "%s/SCCS/s.%s", dest, basenm(s->gfile));
		sfile = path;
		sprintf(path2, "%s/%s", dest, basenm(s->gfile));
		gfile = path2;
	} else {
		sfile = name2sccs(dest);
		gfile = dest;
	}
	if (exists(sfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", sfile);
		return (1);
	}
	if (exists(gfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", gfile);
		return (1);
	}
	error |= mv(s->sfile, sfile);
	if (!error && exists(s->gfile)) error = mv(s->gfile, gfile);
	unless (isDir) free(sfile);
	sccs_free(s);
	if (error) return (error);
	unless (s = sccs_init(sfile, 0)) return (1);
	if (sccs_get(s, 0, 0, 0, 0, SILENT|EDIT, "-")) {
		error = 1;
		goto out;
	}
	comment = "Renamed by sccsmv";
	gotComment = 1;
	unless ((s = sccs_restart(s)) && (d = getComments(0))) {
		error = 1;
		goto out;
	}
	if (sccs_delta(s, SILENT|FORCE, d, 0, 0) == -1) error = 1;
out:	sccs_free(s);
	if (gotComment) commentsDone(saved);
	return (error);
}

int
mv(char *src, char *dest)
{
	mksccsdir(dest);
	if (rename(src, dest)) {	/* try mv(1) */
		char	cmd[MAXPATH*2 + 5];

		sprintf(cmd, "/bin/mv %s %s", src, dest);
		if (system(cmd)) return (1);
	}
	return (0);
}
