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
private	int	Cflg;		/* comments for changesets */
private	int	Aflg;		/* select all uncomitted deltas in a file */

int
sccslog_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	indent = 0, save, c, flags = INIT_SAVEPROJ|SILENT;
	project	*proj = 0;
	RANGE_DECL;

	setmode(1, _O_BINARY);
	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help sccslog");
		return (0);
	}
	while ((c = getopt(ac, av, "ACc;i;pr|v")) != -1) {
		switch (c) {
		    case 'A': Aflg++; break;			/* doc 2.0 */
		    case 'C': Cflg++; break;			/* doc 2.0 */
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

private	void
printlog(int indent)
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
