/*
 * sccslog - take a list of s.files and generate a time sorted sccslog.
 *
 * Copyright (c) 1997 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

private	int	compar(const void *a, const void *b);
private	void	sortlog(int flags);
private	void	printlog(int);
private	void	sccslog(sccs *s);
private	void	reallocDelta(delta *d);
private	void	freelog(void);

private	delta	*list, **sorted;
private	int	n;
private	int	pflag;		/* do basenames */
private	int	Cflg;		/* comment format used by bk resolve */
private	int	Aflg;		/* select all uncomitted deltas in a file */
private	int	fflg;		/*
				 * factor out common comments from 
				 * different delta
				 */

private int isBlank(char *p);
private char **str2line(char **lines, char *prefix, char *str);
private char *line2str(char **comments);
private char **db2line(MDBM *db);
private int sameStr(char *s1, char *s2);
private void saveComment(MDBM *db, char *rev, char *comment_str, char *gfile);
private void freeComment(MDBM *db);

int
sccslog_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	indent = 0, save, c, flags = INIT_SAVEPROJ|SILENT;
	project	*proj = 0;
	RANGE_DECL;

	setmode(1, _O_TEXT);
	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help sccslog");
		return (0);
	}
	while ((c = getopt(ac, av, "ACc;fi;pr|v")) != -1) {
		switch (c) {
		    case 'A': Aflg++; break;			/* doc 2.0 */
		    case 'C': Cflg++; break;			/* doc 2.0 */
		    case 'f': fflg++; break;
		    case 'i': indent = atoi(optarg); break;	/* doc 2.0 */
		    case 'p': pflag++; break;			/* doc 2.0 */
		    case 'v': flags &= ~SILENT; break;		/* doc 2.0 */
		    RANGE_OPTS('c', 'r');			/* doc 2.0 */
		    default:
usage:			system("bk help -s sccslog");
			return (1);
		}
	}

	for (name = sfileFirst("sccslog", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, INIT_NOCKSUM|flags, proj)) {
			continue;
		}
		unless (proj) proj = s->proj;
		unless (HASGRAPH(s)) goto next;
		if (Aflg) {
			delta *d = sccs_top(s);

			while (d) {
				if (d->flags & D_CSET) break;
				d->flags |= D_SET;
				d = d->parent;
			}
		} else {
			RANGE("sccslog", s, 2, 0);
		}
		save = n;
		sccslog(s);
		verbose((stderr, "%s: %d deltas\n", s->sfile, n - save));
next:		sccs_free(s);
	}
	sfileDone();
	if (proj) proj_free(proj);
	verbose((stderr, "Total %d deltas\n", n));
	if (n) {
		sortlog(flags);
		printlog(indent);
		freelog();
	}
	return (0);
}

private	int
compar(const void *a, const void *b)
{
	register	delta *d1, *d2;

	d1 = *((delta**)a);
	d2 = *((delta**)b);
	return (d2->date - d1->date);
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
	qsort(sorted, n, sizeof(sorted), compar);
	verbose((stderr, "done.\n"));
}

private void
printConsolidatedLog(int indent)
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
		printf("%s\n", lines[i]);
	}
	freeLines(lines);
	freeComment(db);
}

private	void
printSortedLog(int indent)
{
	int	i, j;
	delta	*d;

	for (j = 0; j < n; ++j) {
		d = sorted[j];
		unless (d->type == 'D') continue;
		if (Cflg) {
			EACH(d->comments) {
				if (indent) printf("%*s", indent, "");
				if (d->pathname) {
					printf("%-8s\t", basenm(d->pathname));
				}
				printf("%s\n", d->comments[i]);
			}
			continue;
		}
		if (indent) printf("%*s", indent, "");
		if (d->pathname) {
			unless (pflag) {
				printf("%s\n  ", d->pathname);
				if (indent) printf("%*s", indent, "");
			} else {
				printf("%s ", basenm(d->pathname));
			}
		}
		printf("%s %s %s", d->rev, d->sdate, d->user);
		if (d->hostname) printf("@%s", d->hostname);
		printf(" +%d -%d\n", d->added, d->deleted);
		EACH(d->comments) {
			if (d->comments[i][0] == '\001') continue;
			if (indent) printf("%*s", indent, "");
			printf("  %s\n", d->comments[i]);
		}
		printf("\n");
	}
}

private	void
printlog(int indent)
{
	if (fflg) {
		printConsolidatedLog(indent);
	} else {
		printSortedLog(indent);
	}
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
	int	partial = 0;

	for (d = s->table; d; d = d->next) {
		/* XXX - need to screan out meta/removed? */
		unless (d->flags & D_SET) {
			partial = 1;
			break;
		}
	}
	unless (partial) {
		for (d = s->table, n++; d && d->next; n++, d = d->next) {
			if (d->zone) {
				assert(d->zone[0]);
				assert(d->zone[1]);
				assert(d->zone[2]);
				assert(d->zone[3]);
				assert(d->zone[4]);
				assert(d->zone[5]);
				assert(!d->zone[6]);
			}
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
			reallocDelta(d);
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
reallocDelta(delta *d)
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
#define EACH_KV(d)      for (kv = mdbm_first(d); \
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
 * Return TURE if s1 is same as s2
 */
private int
sameStr(char *s1, char *s2)
{
	if (s1 == NULL) {
		if (s2 == NULL) return (1);
		return (0);
	}

	/* When we get here, s1 != NULL */
	if (s2 == NULL) return (0);

	/* When we get here, s1 != NULL && s2 != NULL */
	return (streq(s1, s2));
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
