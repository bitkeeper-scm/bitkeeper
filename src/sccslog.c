/*
 * sccslog - take a list of s.files and generate a time sorted sccslog.
 *
 * Copyright (c) 1997-2001 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

private	int	compar(const void *a, const void *b);
private	void	sortlog(int flags);
private	void	printConsolidatedLog(FILE *);
private	void	printSortedLog(FILE *);
private	void	printlog(FILE *);
private void	pdelta(delta *d, FILE *f);
private	void	do_dspec(sccs *s, delta *d);
private	void	sccslog(sccs *s);
private	void	reallocDelta(sccs *s, delta *d);
private	void	freelog(void);
private	int	isBlank(char *p);
private	char	**str2line(char **lines, char *prefix, char *str);
private	char	*line2str(char **comments);
private	char	**db2line(MDBM *db);
private	void	saveComment(MDBM *db, char *rrev, char *comment, char *gfile);
private	void	freeComment(MDBM *db);

private	delta	*list, **sorted;
private	int	n;
private	int	ChangeSet;	/* if set, ChangeSet is one of the files */

private	struct {
	u32	uncommitted:1;	/* -A: select all uncomitted deltas in a file */
	u32	changeset:1;	/* -C: comment format used by bk commit */
	u32	rmdups:1;	/* -D: factor out duplicate comments */
	char	*dspec;		/* -d: override default format */
	u32	forwards:1;	/* -f: print in oldest..newest order */
	int	indent;		/* -i: indent amount */
	u32	indentOpt:1;	/* 1 if -i was present */
	u32	newline:1;	/* -n: like prs -n */
	u32	basenames:1;	/* -p: do basenames */
	u32	sort:1;		/* -s: time sorted pathname|rev output */
	char	*dbuf;		/* where to put the formatted dspec */
} opts;

int
sccslog_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	save, c, flags = INIT_SAVEPROJ|SILENT;
	project	*proj = 0;
	RANGE_DECL;

	setmode(1, _O_TEXT);
	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help sccslog");
		return (0);
	}
	while ((c = getopt(ac, av, "AbCc;d|Dfi;npr|sv")) != -1) {
		switch (c) {
		    case 'A': opts.uncommitted = 1; break;	/* doc 2.0 */
		    case 'C': opts.changeset = 1; break;	/* doc 2.0 */
		    case 'D': opts.rmdups = 1; break;
		    case 'd': opts.dspec = optarg; break;
		    case 'f': opts.forwards = 1; break;
		    case 'i':					/* doc 2.0 */
			opts.indent = atoi(optarg);
			opts.indentOpt = 1;
			break;
		    case 'n': opts.newline = 1; break;
		    case 'p': opts.basenames = 1; break;	/* doc 2.0 */
		    case 's': opts.sort = 1; break;		/* doc 2.0 */
		    case 'v': flags &= ~SILENT; break;		/* doc 2.0 */
		    RANGE_OPTS('c', 'r');			/* doc 2.0 */
		    default:
usage:			system("bk help -s sccslog");
			return (1);
		}
	}

	for (name = sfileFirst("sccslog", &av[optind], 0); name; ) {
		unless (s = sccs_init(name, INIT_NOCKSUM|flags, proj)) {
			name = sfileNext();
			continue;
		}
		unless (proj) proj = s->proj;
		unless (HASGRAPH(s)) goto next;
		do {
			if (opts.uncommitted) {
				delta *d = sccs_top(s);

				while (d) {
					if (d->flags & D_CSET) break;
					d->flags |= D_SET;
					d = d->parent;
				}
				s->state |= S_SET;
			} else if (things || sfileRev()) {
				s->state |= S_SET;
				RANGE("sccslog", s, 2, 0);
			}
		} while ((name = sfileNext()) && streq(s->sfile, name));
		save = n;
		sccslog(s);
		verbose((stderr, "%s: %d deltas\n", s->sfile, n - save));
next:		sccs_free(s);
	}
	sfileDone();
	if (proj) proj_free(proj);
	if (opts.dbuf) {
		free(opts.dbuf);
		opts.dbuf = 0;
	}
	verbose((stderr, "Total %d deltas\n", n));
	if (n) {
		pid_t	pid;

		pid = mkpager();
		if (ChangeSet && !opts.indentOpt) opts.indent = 2;
		sortlog(flags);
		printlog(stdout);
		fclose(stdout);
		freelog();
		if (pid > 0) waitpid(pid, 0, 0);
	}
	return (0);
}

/*
 * Compare to deltas in a way suitable for qsort.
 */
int
sccs_dcmp(delta *d1, delta *d2)
{
	char	k1[MAXKEY], k2[MAXKEY];

	sccs_sdelta(0, d1, k1);
	sccs_sdelta(0, d2, k2);
	return (strcmp(k2, k1));
}

/*
 * Note that these two are identical except for the d1/d2 assignment.
 */
private	int
compar(const void *a, const void *b)
{
	register	delta *d1, *d2;

	d1 = *((delta**)a);
	d2 = *((delta**)b);
	if (d2->date != d1->date) return (d2->date - d1->date);
	unless (ChangeSet) return (sccs_dcmp(d1, d2));
	if (streq(d1->pathname, GCHANGESET)) return (-1);
	if (streq(d2->pathname, GCHANGESET)) return (1);
	return (sccs_dcmp(d1, d2));
}

private	int
forwards(const void *a, const void *b)
{
	register	delta *d1, *d2;

	d1 = *((delta**)b);
	d2 = *((delta**)a);
	if (d2->date != d1->date) return (d2->date - d1->date);
	unless (ChangeSet) return (sccs_dcmp(d1, d2));
	if (streq(d1->pathname, GCHANGESET)) return (-1);
	if (streq(d2->pathname, GCHANGESET)) return (1);
	return (sccs_dcmp(d1, d2));
}

private	void
sortlog(int flags)
{
	int	i = n;
	delta	*d;

	verbose((stderr, "Sorting...."));
	sorted = malloc(n * sizeof(sorted));
	if (!sorted) {
		perror("malloc");
		exit(1);
	}
	for (d = list; d; d = d->next) {
		assert(i > 0);
		unless (d->date || streq("70/01/01 00:00:00", d->sdate)) {
			assert(d->date);
		}
		sorted[--i] = d;
	}
	assert(i == 0);
	qsort(sorted, n, sizeof(sorted), opts.forwards ? forwards : compar);
	verbose((stderr, "done.\n"));
}

private void
printConsolidatedLog(FILE *f)
{
	int	i, j;
	delta	*d;
	MDBM	*db = mdbm_mem();
	char	**lines;

	assert(db);
	for (j = 0; j < n; ++j) {
		d = sorted[j];
		unless (d->type == 'D') continue;
		unless (d->comments) continue;
		saveComment(db, d->rev, line2str(d->comments), d->pathname);
	}

	lines = db2line(db);

	EACH (lines) {
		fprintf(f, "%s\n", lines[i]);
	}
	freeLines(lines, free);
	freeComment(db);
}

private	void
printSortedLog(FILE *f)
{
	int	i, j;
	delta	*d;

	for (j = 0; j < n; ++j) {
		d = sorted[j];
		unless (d->type == 'D') continue;
		unless (opts.forwards && ChangeSet) {
			pdelta(d, f);
			continue;
		}
		if (streq(GCHANGESET, d->pathname)) {
			pdelta(d, f);
			continue;
		}
		/*
		 * We're on a regular file, go find the ChangeSet,
		 * do that first, then work forward to the ChangeSet.
		 */
		for (i = j+1; i < n; ++i) {
			d = sorted[i];
			unless (d->type == 'D') continue;
			if (streq(d->pathname, GCHANGESET)) {
				pdelta(d, f);
				break;
			}
		}
		while ((j < i) && (j <n)) pdelta(sorted[j++], f);
	}
}

private void
pdelta(delta *d, FILE *f)
{
	int	indent, i;

	if (opts.dspec) {
		if (d->comments && d->comments[1]) {
			fputs(d->comments[1], f);
			if (opts.newline) fputc('\n', f);
		}
		return;
	}
	if (opts.sort) {
		fprintf(f, "%s|%s\n", d->pathname, d->rev);
		return;
	}
	if (d->pathname && streq(d->pathname, "ChangeSet")) {
		indent = 0;
	} else {
		indent = opts.indent;
	}
	if (opts.changeset) {
		EACH(d->comments) {
			if (indent) fprintf(f, "%*s", indent, "");
			if (d->pathname) {
				fprintf(f, "%-8s\t", basenm(d->pathname));
			}
			fprintf(f, "%s\n", d->comments[i]);
		}
		return;
	}
	if (indent) fprintf(f, "%*s", indent, "");
	if (d->pathname) {
		unless (opts.basenames) {
			fprintf(f, "%s\n  ", d->pathname);
			if (indent) fprintf(f, "%*s", indent, "");
		} else {
			fprintf(f, "%s ", basenm(d->pathname));
		}
	}
	fprintf(f, "%s %s %s", d->rev, d->sdate, d->user);
	if (d->hostname) fprintf(f, "@%s", d->hostname);
	fprintf(f, " +%d -%d\n", d->added, d->deleted);
	EACH(d->comments) {
		if (d->comments[i][0] == '\001') continue;
		if (indent) fprintf(f, "%*s", indent, "");
		fprintf(f, "  %s\n", d->comments[i]);
	}
	fprintf(f, "\n");
}

private	void
printlog(FILE *f)
{
	if (opts.rmdups) {
		printConsolidatedLog(f);
	} else {
		printSortedLog(f);
	}
}

private void
do_dspec(sccs *s, delta *d)
{
	unless (opts.dbuf) opts.dbuf = malloc(16<<10);
	opts.dbuf[0] = 0;
	sccs_prsbuf(s, d, PRS_ALL, opts.dspec, opts.dbuf);
	freeLines(d->comments, free);
	d->comments = addLine(0, strdup(opts.dbuf));
}

/*
 * Save the info.
 * If no revisions specified, save the whole tree.
 * If one revision specified (r1 == r2), save just that.
 * Otherwise, save everything from r1 down, pruning at r2.
 */
private	void
sccslog(sccs *s)
{
	delta	*d, *e;

	unless (SET(s)) {
		if (CSET(s)) ChangeSet = 1;
		for (d = s->table, n++; d; n++, d = d->next) {
			if (opts.dspec) do_dspec(s, d);
			unless (d->pathname) d->pathname = strdup(s->gfile);
			unless (d->next) break;
		}
		if (list) {
			assert(d);
			d->next = list;
		}
		list = s->table;
		s->table = s->tree = 0;
		return;
	}
	for (d = s->table; d; ) {
		d->kid = d->siblings = 0;
		if (d->flags & D_SET) {
			if (CSET(s)) ChangeSet = 1;
			if (opts.dspec) do_dspec(s, d);
			reallocDelta(s, d);
			e = d->next;
			d->next = list;
			list = d;
			n++;
			d = e;
		} else {
			e = d;
			d = d->next;
			sccs_freetree(e);
		}
	}
	s->table = s->tree = 0;
}

/*
 * Take all the deltas from start down, pruning at stop.
 * Put them on the list (destroying the delta table list).
 */
private	void
reallocDelta(sccs *s, delta *d)
{
	if (d->zone) {
		if (d->flags & D_DUPZONE) {
			d->flags &= ~D_DUPZONE;
			d->zone = strdup(d->zone);
		}
	}
	if (d->flags & D_DUPPATH) {
		d->flags &= ~D_DUPPATH;
		d->pathname = strdup(d->pathname);
	}
	unless (d->pathname) d->pathname = strdup(s->gfile);
	if (d->flags & D_DUPHOST) {
		d->flags &= ~D_DUPHOST;
		d->hostname = strdup(d->hostname);
	}
}

private	void
freelog()
{
	delta	*d;

	for (d = list; d; d = list) {
		n--;
		list = list->next;
		d->siblings = d->kid = 0;
		sccs_freetree(d);
	}
	if (sorted) free(sorted);
}



/*
 * This code below is gotten from findcset.c in the 2.1 dev tree
 */
#define	EACH_KV(d)	for (kv = mdbm_first(d); \
			    kv.key.dsize; kv = mdbm_next(d))

/*
 * Return true if blank line
 */
private int
isBlank(char *p)
{
	while (*p) {
		unless (isspace(*p++))  return (0);
	}
	return (1);
}

/*
 * Convert a string into line array
 */
private char **
str2line(char **lines, char *prefix, char *str)
{
	char	*p, *q;

	q = p = str;
	while (1) {
		if (!*q) {
			if ((&q[-1] >= str) && (q[-1] != '\n')) {
				lines = addLine(lines,
						aprintf("%s%s", prefix, p));
			}
			break;
		}
		if (*q == '\n') {
			*q = 0;
			lines = addLine(lines, aprintf("%s%s", prefix, p));
			*q++ = '\n';
			p = q;
		} else {
			q++;
		}
	}
	return (lines);
}

/*
 * Convert line array into a regular string
 */
private char *
line2str(char **comments)
{
	int	i, len = 0;
	char	*buf, *p, *q;

	EACH(comments) {
		len += strlen(comments[i]) + 1;
	}

	p = buf = malloc(++len);
	EACH(comments) {
		q = comments[i];
		if (isBlank(q)) continue; /* skip blank line */
		while (*q) *p++ = *q++;
		*p++ = '\n';
	}
	*p = 0;
	assert(buf + len > p);
	return (buf);
}

/*
 * Convert cset commnent strore in a mdbm into lines format
 */
private char **
db2line(MDBM *db)
{
	kvpair	kv;
	char	**lines = 0;
	char	*comment, *gfiles, *lastg, *p, *q;
	char	*many = "Many files:";
	int	i, len = 0;
	MDBM	*gDB;

	lastg = "";
	EACH_KV(db) {
		/*
		 * Compute length of file list
		 */
		comment = kv.key.dptr;
		memcpy(&gDB, kv.val.dptr, sizeof (MDBM *));
		len = 0;
		EACH_KV(gDB) {
			len += strlen(kv.key.dptr) + 2;
		}
		len += 2;

		/*
		 * Extract gfile list and set it as the section header
		 * Skip gfile list if too long
		 */
		if (len <= 80) {
			p = gfiles = malloc(len);
			i = 1;
			EACH_KV(gDB) {
				q = kv.key.dptr;
				if (i++ > 1) {
					*p++ = ',';
					*p++ = ' ';
				}
				while (*q) *p++ = *q++;
			}
			*p++ = ':';
			*p = 0;
			assert(gfiles + len > p);

		} else {
			gfiles = strdup(many);
		}

		/*
		 * If gfile list is same as previous, skip
		 */
		if (!streq(lastg, gfiles)) {
			lines = addLine(lines, gfiles);
		}
		lastg = gfiles;

		/*
		 * Now extract the comment block
		 */
		lines = str2line(lines, "  ", comment);
	}
	return (lines);
}

/*
 * We dump the comments into a mdbm to factor out repeated comments
 * that came from different files.
 */
private void
saveComment(MDBM *db, char *rev, char *comment_str, char *gfile)
{
	datum	k, v, tmp;
	MDBM	*gDB = 0;
	int	ret;

#define	BK_REV_1_0_DEFAULT_COMMENT	"BitKeeper file "

	if (isBlank(comment_str)) return;
	if ((streq("1.0", rev)) &&
	    strneq(BK_REV_1_0_DEFAULT_COMMENT, comment_str, 15)) {
		comment_str = "new file";
	}

	k.dptr = (char *) comment_str;
	k.dsize = strlen(comment_str) + 1;
	tmp = mdbm_fetch(db, k);
	if (tmp.dptr) memcpy(&gDB, tmp.dptr, sizeof (MDBM *));
	unless (gDB) gDB = mdbm_mem();
	ret = mdbm_store_str(gDB, basenm(gfile), "", MDBM_INSERT);
	/* This should work, or it will be another file with the same
	 * basename.
	 */
	assert(ret == 0 || (ret == 1 && errno == EEXIST));
	unless (tmp.dptr) {
		v.dptr = (char *) &gDB;
		v.dsize = sizeof (MDBM *);
		ret = mdbm_store(db, k, v, MDBM_REPLACE);
	}
}

private void
freeComment(MDBM *db)
{
	kvpair	kv;
	MDBM	*gDB;

	EACH_KV(db) {
		memcpy(&gDB, kv.val.dptr, sizeof (MDBM *));
		mdbm_close(gDB);
	}
	mdbm_close(db);
}
