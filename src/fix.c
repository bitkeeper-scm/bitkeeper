#include "system.h"
#include "sccs.h"
private void cfile(sccs *s, char *revs);
private void check(char *path, char *rev);
private	void do_cset(char *qflag);
private void doit(char *file, char *revs, char *qflag, char *force);

int
fix_main(int ac,  char **av)
{
	int	c, i;
	int	cset = 0;
	char	*qflag = "-q";

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help fix");
		return (1);
	}
	while ((c = getopt(ac, av, "cqv")) != -1) {
		switch (c) {
		    case 'c': cset = 1; break;
		    case 'q': break;				/* undoc 2.0 */
		    case 'v': qflag = ""; break;		/* doc 2.0 */
		    default :
			system("bk help -s fix");
			return (1);
		}
	}
	if (cset) {
		do_cset(qflag);
	} else {
		i =  optind - 1;
		while (av[++i]) {
			doit(av[i], "+", qflag, "");
		}
	}
	return (0);
}

private void
do_cset(char *qflag)
{
	int	c;
	char	*revs, *n, *p;
	sccs	*s;
	delta	*d;
	char	path[MAXPATH];
	char	lastpath[MAXPATH];
	FILE	*f = popen("bk cset -r+", "r");

	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "fix: can't find repository root\n");
		exit(1);
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
		*p++ = 0;
		if (lastpath[0] && streq(path, lastpath)) continue;
		strcpy(lastpath, path);
		n = name2sccs(path);
		s = sccs_init(n, 0, 0);
		unless (s && HASGRAPH(s)) {
			fprintf(stderr,
			    "fix: no graph in %s?\n", s->gfile);
			exit(1);
		}
		d = sccs_getrev(s, "+", 0, 0);
		unless (d && streq(p, d->rev)) {
			fprintf(stderr,
			    "Unable to fix this changeset, "
			    "%s has a delta beyond %s\n",
			    s->gfile, p);
			exit(1);
		}
		if (sccs_clean(s, SILENT)) {
			fprintf(stderr,
			    "Unable to fix this changeset, %s is modified\n",
			    s->gfile);
			exit(1);
		}
		free(n);
		sccs_free(s);
	}
	pclose(f);

	/*
	 * OK, go fix it up.
	 */
	f = popen("bk cset -r+", "r");
	revs = 0;
	lastpath[0] = 0;
	while (fnext(path, f)) {
		c = chop(path);
		assert(c == '\n');
		p = strchr(path, '|');
		assert(p);
		*p++ = 0;
		if (!revs || streq(path, lastpath)) {
			if (revs) {
				char *tmp = aprintf("%s,%s", revs, p);
				free(revs);
				revs = tmp;
			} else {
new:				revs = strdup(p);
				strcpy(lastpath, path);
			}
			continue;
		}
		assert(lastpath[0]);
		check(lastpath, revs);
		doit(lastpath, revs, qflag, "-C");
		free(revs);
		revs = 0;
		goto new;
	}
	assert(lastpath[0]);
	check(lastpath, revs);
	doit(lastpath, revs, qflag, "-C");
	pclose(f);

}

private void
check(char *path, char *rev)
{
	char	*p, *n;
	sccs	*s;
	delta	*d;
	
	if (p = strchr(rev, ',')) *p = 0;
	n = name2sccs(path);
	s = sccs_init(n, 0, 0);
	assert(s && HASGRAPH(s));
	d = sccs_getrev(s, "+", 0, 0);
	assert(d && streq(rev, d->rev));
	if (p) *p = ',';
	assert(sccs_clean(s, SILENT) == 0);
	sccs_free(s);
	free(n);
}

private void
doit(char *file, char *revs, char *qflag, char *force)
{
	char	buf[MAXLINE];
	char	fixfile[MAXPATH];
	char	*p = 0;
	sccs	*s;
	delta	*d;
	mode_t	mode = 0;
	int	cset;

	sprintf(fixfile, "%s-%d", file, getpid());
	if (exists(fixfile)) {
		printf("%s exists, skipping that file", fixfile);
		return;
	}
	assert(sccs_filetype(file) == 0);
	p = name2sccs(file);
	s = sccs_init(p, SILENT, 0);
	unless (s && HASGRAPH(s)) {
		fprintf(stderr, "%s does not exists\n", s->sfile);
		sccs_free(s);
		return;
	}
	cset = CSET(s);
	if (sccs_clean(s, SILENT)) {
		fprintf(stderr, "Unable to fix modified file %s\n", s->gfile);
		sccs_free(s);
		free(p);
		return;
	}
	unless (cset) {
		get(p, SILENT|PRINT, fixfile);
		d = sccs_getrev(s, "+", 0, 0);
		mode = d->mode;
	}
	cfile(s, revs);
	sccs_free(s);
	sprintf(buf, "bk stripdel %s %s -r%s %s", qflag, force, revs, file);
	if (system(buf)) {
		unlink(fixfile);
	} else {
		if (exists(p) && !cset) {
			int gflags = SILENT|GET_SKIPGET|GET_EDIT;
			s = sccs_init(p, SILENT, 0);
			assert(s);
			if (sccs_get(s, 0, 0, 0, 0, gflags, "-")) {
				fprintf(stderr, "cannot lock %s\n", file);
			}
			sccs_free(s);
		}
		unless (cset) {
			if (rename(fixfile, file) == -1) perror(file);
			if (mode) chmod(file, mode);
		}
	}
	free(p);
}

private void
cfile(sccs *s, char *revs)
{
	char	*out = sccs_Xfile(s, 'c');
	char	*r = aprintf("-hr%s", revs);

	sysio(0, out,0, "bk", "prs", r, "-d$each(:C:){(:C:)\n}", s->gfile, SYS);
	free(r);
}
