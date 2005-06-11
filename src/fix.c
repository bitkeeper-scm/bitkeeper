#include "system.h"
#include "sccs.h"
private void	cfile(sccs *s, char *rev);
private	int	do_cset(char *qflag, int flags, int save);
private int	doit(char *file, char *rev, char *qflg, int flags, char *force);
private	int 	simple(sccs *s, delta *top, int flags);

int
fix_main(int ac,  char **av)
{
	int	c, i;
	int	save = 1, cset = 0, flags = 0, rc = 0;
	char	*qflag = "-q";

	while ((c = getopt(ac, av, "cqsv")) != -1) {
		switch (c) {
		    case 'c': cset = 1; break;
		    case 'q': flags |= SILENT; break;		/* undoc 2.0 */
		    case 's': save = 0; break;
		    case 'v': qflag = ""; break;		/* doc 2.0 */
		    default :
			system("bk help -s fix");
			return (1);
		}
	}
	if (cset) {
		rc = do_cset(qflag, flags, save);
	} else {
		i =  optind - 1;
		while (av[++i]) {
			rc |= doit(av[i], "+", qflag, flags, "");
		}
	}
	return (rc);
}

private int
do_cset(char *qflag, int flags, int save)
{
	int	c, i;
	char	*revs, *n, *p;
	sccs	*s;
	delta	*d;
	char	**lines = 0;
	FILE	*f = popen("bk cset -r+", "r");
	char	path[MAXPATH], lastpath[MAXPATH];

	if (proj_cd2root()) {
		fprintf(stderr, "fix: can't find repository root\n");
		return (1);
	}
	/* 
	 * check to see if there are later deltas.
	 * XXX - we could try locking the tree with the RESYNC dir.
	 */
	lastpath[0] = 0;
	while (fnext(path, f)) {
		/* man/man1/bk-has.1|1.1 */
		c = chop(path);
		assert(c == '\n');
		p = strchr(path, '|');
		assert(p);
		*p = 0;
		if (lastpath[0] && streq(path, lastpath)) continue;
		strcpy(lastpath, path);
		*p = '|';
		lines = addLine(lines, strdup(path));
	}
	if (pclose(f)) {
		fprintf(stderr, "Sorry, unable to fix this cset.\n");
		return (1);
	}

	EACH(lines) {
		p = strchr(lines[i], '|');
		assert(p);
		*p++ = 0;	/* Note: restored below, don't move p */
		n = name2sccs(lines[i]);
		s = sccs_init(n, 0);
		unless (s && HASGRAPH(s)) {
			fprintf(stderr,
			    "fix: no graph in %s?\n", s->gfile);
			return (1);
		}
		d = sccs_top(s);
		if (CSET(s) && d->merge) {
			fprintf(stderr, "Unable to fix merge changesets.\n");
			sccs_free(s);
			return (1);
		}
		unless (d && streq(p, d->rev)) {
			fprintf(stderr,
			    "Unable to fix this changeset, "
			    "%s has a delta beyond %s\n",
			    s->gfile, p);
			sccs_free(s);
			return (1);
		}
		if (sccs_clean(s, SILENT)) {
			fprintf(stderr,
			    "Unable to fix this changeset, %s is modified\n",
			    s->gfile);
			sccs_free(s);
			return (1);
		}
		free(n);
		sccs_free(s);
		p[-1] = '|';
	}

	if (trigger("fix", "pre")) return (1);

	/*
	 * Saving the patch can fail if the repo is in a screwed up state,
	 * eg one of the files in the patch is missing and not goned.
	 */
	if (save) {
		int	rc = sysio(0, "BitKeeper/tmp/fix.patch",
			    0, "bk", "makepatch", "-r+", SYS);
		if (rc) {
			fprintf(stderr, "fix: unable to save patch, abort.\n");
			return (1);
		}
	}

	/*
	 * OK, go fix it up.
	 */
	save_log_markers();
	revs = 0;
	EACH(lines) {
		p = strchr(lines[i], '|');
		assert(p);
		*p++ = 0;
		doit(lines[i], p, qflag, flags, "-C");
	}
	freeLines(lines, free);
	update_log_markers(streq(qflag, ""));
	return (0);
}

/*
 * Strip top rev off sfile and leave the contents of the top delta edited
 * in the gfile if and only if it is a content change.
 *
 * XXX if it is a content change plus a rename because they moved the 
 * s.file by hand and then edited it, modified it, and checked it in,
 * this does the wrong thing.  It's not idempotent wrt citool.
 *
 * XXX Ideally this function would leave the gfile untouched so the
 * user doesn't need to rebuild after running 'bk fix -c'.  At the
 * moment we end up refetching the gfile.
 */
private int
doit(char *file, char *rev, char *qflag, int flags, char *force)
{
	char	buf[MAXLINE];
	char	fixfile[MAXPATH];
	char	*p = 0;
	sccs	*s;
	delta	*d;
	mode_t	mode = 0;
	int	cset;
	int	rc = 0;

	sprintf(fixfile, "%s-%u", file, getpid());
	if (exists(fixfile)) {
		printf("%s exists, skipping that file", fixfile);
		return (1);
	}
	assert(sccs_filetype(file) == 0);
	p = name2sccs(file);
	s = sccs_init(p, SILENT);
	unless (s && HASGRAPH(s)) {
		fprintf(stderr, "%s removed while fixing?\n", s->sfile);
		sccs_free(s);
		return (1);
	}
	cset = CSET(s);
	if (sccs_clean(s, SILENT)) {
		fprintf(stderr, "Unable to fix modified file %s\n", s->gfile);
		sccs_free(s);
		free(p);
		return (1);
	}
	unless (cset) {
		d = sccs_top(s);
		unless (simple(s, d, flags)) {
			free(p);
			p = sccs_Xfile(s, 'd');
			close(creat(p, 0664));
			d->flags &= ~D_CSET;
			sccs_get(s, 0, 0, 0, 0, SILENT, "-");
			sccs_newchksum(s);
			sccs_free(s);
			return (0);
		}

		assert(streq(d->rev, rev) || streq(rev, "+"));

		/*
		 * If this file was bk new'd in this cset then
		 * just get the c.file and gfile and unlink s.file.
		 * bk citool will put new it again.
		 * Note that if they newed and then delta-ed it
		 * before committing we don't take this code path,
		 * that's on purpose.  Undo top delta only.
		 */
		if ((d == s->tree) || (d->parent == s->tree)) {
			/* make sure this is 1.0->1.1 or just old 1.1 only */
			assert(d->serial <= 2);

			cfile(s, "1.1");
			sccs_get(s, 0, 0, 0, 0, SILENT, "-");
			chmod(s->gfile, d->mode);

			/* careful */
			free(p);
			p = sccs_Xfile(s, 'd');
			unlink(p);
			p = strdup(s->sfile);
			sccs_free(s);
			unlink(p);
			free(p);
			return (0);
		}

		mode = d->mode;
		sccs_get(s, 0, 0, 0, 0, SILENT|PRINT, fixfile);
	}
	cfile(s, rev);
	sccs_free(s);
	sprintf(buf, "bk stripdel %s %s -r%s '%s'", qflag, force, rev, file);
	if (system(buf)) {
		unlink(fixfile);
		rc = 1;
	} else {
		if (exists(p) && !cset) {
			int gflags = SILENT|GET_SKIPGET|GET_EDIT;
			s = sccs_init(p, SILENT);
			assert(s);
			if (!IS_EDITED(s) &&
			    sccs_get(s, 0, 0, 0, 0, gflags, "-")) {
				fprintf(stderr, "cannot lock %s\n", file);
				rc = 1;
			}
			sccs_free(s);
		}
		unless (cset) {
			if (rename(fixfile, file) == -1) {
				perror(file);
				rc = 1;
			}
			if (mode) chmod(file, mode);
			utime(file, 0);	/* touch gfile */
		}
	}
	free(p);
	return (rc);
}

/*
 * Return true if this is a pure content change delta.
 * Exception: we allow mode changes, we can catch those.
 */
private	int
simple(sccs *s, delta *top, int flags)
{
	unless (streq(top->pathname, top->parent->pathname)) {
		verbose((stderr,
		    "Not fixing %s because of pathname change.\n", s->gfile));
		return (0);
	}
	if (top->symlink) {
		verbose((stderr,
		    "Not fixing %s because it is a symlink.\n", s->gfile));
		return (0);
	}
	if (top->include) {
		verbose((stderr,
		    "Not fixing %s because it has includes.\n", s->gfile));
		return (0);
	}
	if (top->exclude) {
		verbose((stderr,
		    "Not fixing %s because it has excludes.\n", s->gfile));
		return (0);
	}
	if (top->merge) {
		verbose((stderr,
		    "Not fixing %s because tip is a merge delta.\n", s->gfile));
		return (0);
	}
	if (top->flags & D_XFLAGS) {
		verbose((stderr,
		    "Not fixing %s because it has xflags change.\n", s->gfile));
		return (0);
	}
	if (top->flags & D_TEXT) {
		verbose((stderr,
		    "Not fixing %s because it has text change.\n", s->gfile));
		return (0);
	}
	unless (top->added || top->deleted) {
		verbose((stderr,
		    "Not fixing %s because it has no changes.\n", s->gfile));
		return (0);
	}
	assert(!top->dangling);
	return (1);
}

private void
cfile(sccs *s, char *rev)
{
	char	*out = sccs_Xfile(s, 'c');
	char	*r = aprintf("-hr%s", rev);

	sysio(0, out,0, "bk", "prs", r, "-d$each(:C:){(:C:)\n}", s->gfile, SYS);
	free(r);
}
