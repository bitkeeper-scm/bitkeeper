/*
 * sccslog - take a list of s.files and generate a time sorted sccslog.
 *
 * Copyright (c) 1997 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");
char	*log_help = "\n\
usage: sccslog [-hpv] [-c<d>] [-r<r>] [file list...] OR [-] OR []\n\n\
    -c<dates>	Cut off dates.  See range(1) for details.\n\
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

int	compar(const void *a, const void *b);
void	sortlog(int flags);
void	printlog(void);
void	sccslog(sccs *s);
void	stealTree(delta *d, delta *stop);
void	freelog(void);

delta	*list, **sorted;
int	n;
int	pflag;		/* do basenames */
int	Cflg;		/* comments for changesets */

int
main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	save, c, flags = SILENT;
	char	*rev;
	int	inroot = 0;
	RANGE_DECL;

#ifdef WIN32
	_setmode(_fileno(stdout), _O_BINARY);
#endif
	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, log_help);
		return (0);
	}
	while ((c = getopt(ac, av, "Cc;pr|v")) != -1) {
		switch (c) {
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
again:		unless (s = sccs_init(name, INIT_NOCKSUM|flags)) {
			continue;
		}
		if (!s->tree) {
			/*
			 * XXX - what this should do is try opening 
			 * root/name and only switch to root if that
			 * works.
			 */
			unless (inroot) {
				inroot = 1;
				if (sccs_cd2root(0, 0) == 0) {
					sccs_free(s);
					goto again;
				}
			}
			goto next;
		}
		RANGE("sccslog", s, 1, 0);
		save = n;
		sccslog(s);
		verbose((stderr, "%s: %d deltas\n", s->sfile, n - save));
next:		sccs_free(s);
	}
	sfileDone();
	verbose((stderr, "Total %d deltas\n", n));
	if (n) {
		sortlog(flags);
		printlog();
		freelog();
	}
	purify_list();
	return (0);
}

int compar(const void *a, const void *b)
{
	register	delta *d1, *d2;

	d1 = *((delta**)a);
	d2 = *((delta**)b);
	return (d2->date - d1->date);
}

void
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

void
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
void
sccslog(sccs *s)
{
	delta	*d, *e;
	delta	*start = s->rstart;
	delta	*stop = s->rstop;

	if (!start && !stop) {
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
	start->siblings = 0;
	stealTree(start, stop);
	for (d = s->table; d; ) {
		d->kid = d->siblings = 0;
		if (d->flags & D_STEAL) {
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
void
stealTree(delta *d, delta *stop)
{
	if (d->zone) {
		assert(d->zone[0]);
		assert(d->zone[1]);
		assert(d->zone[2]);
		assert(d->zone[3]);
		assert(d->zone[4]);
		assert(d->zone[5]);
		assert(!d->zone[6]);
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
	d->flags |= D_STEAL;
	if (d == stop) return;
	if (d->kid) stealTree(d->kid, stop);
	if (d->siblings) stealTree(d->siblings, stop);
}

void
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
