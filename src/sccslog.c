/*
 * sccslog - take a list of s.files and generate a time sorted sccslog.
 *
 * Copyright (c) 1997 L.W.McVoy
 */
#include "sccs.h"
WHATSTR("%W%");
char	*log_help = "\n\
usage: sccslog [-1hpv] [-d<d>] [-r<r>] [file list...] OR [-] OR []\n\n\
    -1		Print exactly one record, as specified by -r/-c\n\
    -c<dates>	Cut off dates.  See range(1) for deltails.\n\
    -d<dates>	Same as -c (-d is compat with diffs).\n\
    -h		show hostnames, i.e., user@host in log.\n\
    -p		show full pathnames instead of basenames.\n\
    -r<r>	specify a revision or a part of a range.\n\
    -v		be verbose about errors and processing\n\n\
    Note that <date> may be a symbol, which implies the date of the\n\
    delta which matches the symbol.\n\n\
    Range specifications:\n\
	-cAlpha,Beta    Everything after Alpha up to and including Beta.\n\
	-c'[97,98]'	Everything in 1997 and 1998.\n\
	-c97..98	Same thing, alternative notation.\n\
	-c'[97' -r1.19	Everything from the start of '97 up to rev 1.19.\n\n";

int	compar(const void *a, const void *b);
void	sortlog(int flags);
void	printlog(void);
void	sccslog(sccs *s);
void	stealTree(delta *d, delta *stop);
void	freelog(void);

delta	*list, **sorted;
int	n;
int	pflag;		/* do full pathnames */
int	hflag;		/* user@host instead of just user */

int
main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	int	save, c, flags = SILENT;
	char	*r[2], *d[2];
	int	things = 0, rd = 0;
	int	one = 0;

#ifdef WIN32
	_setmode(_fileno(stdout), _O_BINARY);
#endif
	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, log_help);
		return (0);
	}
	r[0] = r[1] = d[0] = d[1] = 0;
	while ((c = getopt(ac, av, "1c;d;hpr|v")) != -1) {
		switch (c) {
		    case '1': one++; break;
		    case 'c': case 'd':
			if (things == 2) goto usage;
			d[rd++] = optarg;
			things += tokens(optarg);
			break;
		    case 'h': hflag++; break;
		    case 'p': pflag++; break;
		    case 'r':
			if (things == 2) goto usage;
			r[rd++] = notnull(optarg);
			things += tokens(notnull(optarg));
			break;
		    case 'v': flags &= ~SILENT; break;
		    default:
usage:			fprintf(stderr, "sccslog: usage error, try --help.\n");
			return (1);
		}
	}
	if ((things == 1) && r[0] && (roundType(r[0]) == EXACT)) one = 1;
	if (one && (things != 1)) {
		fprintf(stderr, "sccslog: -1 needs a date or a revision.\n");
		goto usage;
	}

	for (name = sfileFirst("sccslog", &av[optind], SFILE);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, NOCKSUM|flags)) {
			continue;
		}
		if (!s->tree) goto next;
		rangeReset(s);
		if (things == 2) {
			if (rangeAdd(s, r[0], d[0])) {
not0:				verbose((stderr,
				    "sccslog: no delta ``%s'' in %s\n",
				    r[0] ? r[0] : d[0], s->sfile));
				goto next;
			}
			if ((r[1] || d[1]) && (rangeAdd(s, r[1], d[1]) == -1)) {
				verbose((stderr,
				    "sccslog: no delta ``%s'' in %s\n",
				    r[1] ? r[1] : d[1], s->sfile));
				goto next;
			}
			if (!s->rstart) goto next;
		} else if (things == 1) {
			delta	*tmp = sccs_getrev(s, r[0], d[0], ROUNDUP);

			if (!tmp) {
				unless (flags & SILENT) {
					goto not0;
				} else {
					goto next;
				}
			}
			if (one) {
				s->rstop = s->rstart = tmp;
			} else if (roundType(r[0] ? r[0] : d[0]) == ROUNDUP) {
				s->rstart = s->tree;
				s->rstop = tmp;
			} else {
				s->rstart = tmp;
			}
		}
		save = n;
		sccslog(s);
		verbose((stderr, "%s: %d deltas\n", s->sfile, n - save));
next:		sccs_free(s);
	}
	sfileDone();
	verbose((stderr, "Total %d deltas\n", n));
	sortlog(flags);
	printlog();
	freelog();
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
		d->date = sccs_date2time(d->sdate, d->zone, 0);
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
		if (d->pathname) {
			if (pflag) {
				printf("%s\n  ", d->pathname);
			} else {
				printf("%s ", basenm(d->pathname));
			}
		}
		printf("%s %s %s", d->rev, d->sdate, d->user);
		if (hflag && d->hostname) printf("@%s", d->hostname);
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
