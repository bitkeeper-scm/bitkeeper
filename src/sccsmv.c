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
	char	*name, *dest, c;
	int	isDir, createDelta = 1;
	int	errors = 0;
	int	dofree = 0;

	debug_main(av);
	while ((c = getopt(ac, av, "z")) != -1) {   
		switch (c) {
		    case 'z': createDelta = 0; break;
		    default: 
			fprintf(stderr, "delta: usage error\n");
			return (1);
		}
	}
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
	unless (isDir || ((ac - optind) == 2)) goto usage;
	av[ac-1] = 0;
	for (name =
	    sfileFirst("sccsmv",&av[optind], 0); name; name = sfileNext()) {
		errors |= sccs_mv(name, dest, isDir, createDelta);
	}
	if (dofree) free(dest);
	sfileDone();
	purify_list();
	return (errors);
}

#include "comments.c"
			
int
sccs_mv(char *name, char *dest, int isDir, int createDelta)
{
	char	path[MAXPATH];
	char	path2[MAXPATH];
	char	*gfile, *sfile, *nrev = 0;
	sccs	*s;
	delta	*d;
	int	error = 0, wasEdited = 0;
	pfile	pf;
	int	flags = SILENT|FORCE;; 

	s = sccs_init(name, NOCKSUM);
	unless (HAS_SFILE(s)) {
		fprintf(stderr, "sccsmv: not an SCCS file: %s\n", name);
		sccs_free(s);
		return (1);
	}
	if (isDir) {
		sprintf(path, "%s/SCCS/s.%s", dest, basenm(s->gfile));
		sfile = path;
		sprintf(path2, "%s/%s", dest, basenm(s->gfile));
		gfile = path2;
	} else {
		char *t;
		t = name2sccs(dest);
		sfile = strdup(sPath(t, isDir));
		free(t);
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
	if (IS_EDITED(s) && !error) {
		char *p;
		p = strrchr(sfile, '/');
		p = p ? &p[1]: sfile;
		assert(*p == 's');
		*p = 'p';
		error = mv(s->pfile, sfile);
		*p = 's';
	}
	if (error) goto out;
	if (IS_EDITED(s)) {
		sccs_free(s);
		if (!createDelta) goto out;
		/* extract the branch/LOD info */
		unless (s = sccs_init(sfile, 0)) { error++; goto out; }
		newrev(s, &pf);
		nrev = pf.newrev;
		flags |= SAVEGFILE;
		wasEdited = 1;
	} else {
		sccs_free(s);
		unless (s = sccs_init(sfile, 0)) { error++; goto out; }
		if (sccs_get(s, 0, 0, 0, 0, SILENT|EDIT, "-")) {
			error = 1;
			goto out;
		}
		if (!createDelta) goto out;
		s = sccs_restart(s);
	}
	comment = "Renamed by sccsmv";
	gotComment = 1;
	unless (s && (d = getComments(0))) {
		error = 1;
		goto out;
	}
	if (sccs_delta(s, flags, d, 0, 0) == -1) error = 1;
	if (wasEdited) {
		int flag = EDIT|SKIPGET|SILENT;

		unless (s = sccs_restart(s)) {
			error = 1;
			goto out;
		}
		if (sccs_get(s, nrev, 0, 0, 0, flag, "-")) { 
			fprintf(stderr,
				"get of %s failed, skipping it.\n", name); 
		}
	}
out:	if (s) sccs_free(s);
	unless (isDir) free(sfile);
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
