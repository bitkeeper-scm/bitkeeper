/*
 * sccslog - take a list of s.files and generate a time sorted sccslog.
 *
 * Copyright (c) 1997 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

private	char	*log_help = "\n\
usage: sccslog [-pCv] [-c<d>] [-r<r>] [file list...] OR [-] OR []\n\n\
    -A		select all uncommited deltas in a file.\n\
    -c<dates>	Cut off dates.  See 'bk help dates' for details.\n\
    -C		produce comments for a changeset\n\
    -p		show basenames instead of full pathnames.\n\
    -r<r>	specify a revision or a part of a range.\n\
    -v		be verbose about errors and processing\n\n\
    Note that <date> may be a symbol, which implies the date of the\n\
    delta which matches the symbol.\n\n\
    Range specifications:\n\
    -r+		    prints the most recent delta\n\
    -r1.3..1.6	    prints all deltas 1.3 through 1.6\n\
    -d9207..92	    prints all deltas from July 1 '92 to Dec 31 '92\n\
    -d92..92	    prints all deltas from Jan 1 '92 to Dec 31 '92\n\n";

private	int	compar(const void *a, const void *b);
private	void	sortlog(int flags);
private	void	printlog(void);
private	void	sccslog(sccs *s);
private	void	reallocDelta(delta *d);
private	void	freelog(void);

private	delta	*list, **sorted;
private	int	n;
private	int	pflag;		/* do basenames */
private	int	Cflg;		/* comments for changesets */
private	int	Aflg;		/* select all uncomitted deltas in a file */

int
sccslog_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	save, c, flags = INIT_SAVEPROJ|SILENT;
	project	*proj = 0;
	RANGE_DECL;

#ifdef WIN32
	_setmode(_fileno(stdout), _O_BINARY);
#endif
	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, log_help);
		return (0);
	}
	while ((c = getopt(ac, av, "ACc;pr|v")) != -1) {
		switch (c) {
		    case 'A': Aflg++; break;
		    case 'C': Cflg++; break;
		    case 'p': pflag++; break;
		    case 'v': flags &= ~SILENT; break;
		    RANGE_OPTS('c', 'r');
		    default:
usage:			fprintf(stderr, "sccslog: usage error, try --help.\n");
			return (1);
		}
	}

	for (name = sfileFirst("sccslog", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, INIT_NOCKSUM|flags, proj)) {
			continue;
		}
		unless (proj) proj = s->proj;
		unless (s->tree) goto next;
		RANGE("sccslog", s, 2, 0);
		if (Aflg) {
			delta *d = sccs_top(s);

			while (d) {
				if (d->flags & D_CSET) break;
				d->flags |= D_SET;
				d = d->parent;
			}
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
		printlog();
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

private	void
printlog()
{
	int	i, j;
	delta	*d;

	for (j = 0; j < n; ++j) {
		d = sorted[j];
		unless (d->type == 'D') continue;
		if (Cflg) {
			EACH(d->comments) {
				if (d->pathname) {
					printf("%-8s\t", basenm(d->pathname));
				}
				printf("%s\n", d->comments[i]);
			}
			continue;
		}
		if (d->pathname) {
			unless (pflag) {
				printf("%s\n  ", d->pathname);
			} else {
				printf("%s ", basenm(d->pathname));
			}
		}
		printf("%s %s %s", d->rev, d->sdate, d->user);
		if (d->hostname) printf("@%s", d->hostname);
		printf(" +%d -%d\n", d->added, d->deleted);
		EACH(d->comments) {
			if (d->comments[i][0] == '\001') continue;
			printf("  %s\n", d->comments[i]);
		}
		printf("\n");
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
