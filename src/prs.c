/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");
char	*prs_help = "\n\
usage: prs [-abhmMv] [-c<date>] [-C<rev>] [-r<rev>] [files...]\n\n\
    -a		print info on all deltas, not just data deltas\n\
    -b		reverse the order of the printed deltas\n\
    -c<date>	Cut off dates.  See sccsrange(1) for details.\n\
    -C<rev>	make the range be all revs that are the same cset as this\n\
    -d<spec>    Specify output data specification\n\
    -h		Suppress range header\n\
    -m		print metadata, such as users and symbols.\n\
    -M		do not include branch deltas which are not merged\n\
    -o		print the set of not specified deltas\n\
    -r<rev>	Specify a revision, or part of a range.\n\
    -v		Complain about SCCS files that do not match the range.\n\n\
    Note that <date> may be a symbol, which implies the date of the\n\
    delta which matches the symbol.\n\n\
    Range specifications:\n\
    -r+		    prints the most recent delta\n\
    -r1.3..1.6	    prints all deltas 1.3 through 1.6\n\
    -c9207..92	    prints all deltas from July 1 '92 to Dec 31 '92\n\
    -c92..92	    prints all deltas from Jan 1 '92 to Dec 31 '92\n\n";

int
prs_main(int ac, char **av)
{
	sccs	*s;
	delta	*e;
	int	reverse = 0, doheader = 1;
	int	init_flags = INIT_NOCKSUM;
	int	flags = 0;
	int	opposite = 0;
	int	c;
	char	*name;
	char	*cset = 0;
	int	noisy = 0;
	int	expand = 1;
	char	*dspec = NULL;
	RANGE_DECL;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		fprintf(stderr, prs_help);
		return (1);
	}
	while ((c = getopt(ac, av, "abc;C;d:hmMor|v")) != -1) {
		switch (c) {
		    case 'a':
			/* think: -Ma, the -M set expand already */
			if (expand < 2) expand = 2;
			flags |= PRS_ALL;
			break;
		    case 'b': reverse++; break;
		    case 'C': cset = optarg; break;
		    case 'd': dspec = optarg; break;
		    case 'h': doheader = 0; break;
		    case 'm': flags |= PRS_META; break;
		    case 'M': expand = 3; break;
		    case 'o': opposite = 1; doheader = 0; break;
		    case 'v': noisy = 1; break;
		    RANGE_OPTS('c', 'r');
		    default:
usage:			fprintf(stderr, "prs: usage error, try --help\n");
			return (1);
		}
	}
	if (things && cset) {
		fprintf(stderr, "prs: -r or -C but not both.\n");
		exit(1);
	}
	for (name = sfileFirst("prs", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, init_flags, 0)) continue;
		if (!s->tree) goto next;
		if (cset) {
			delta	*d = sccs_getrev(s, cset, 0, 0);

			if (!d) goto next;
			rangeCset(s, d);
		} else {
			RANGE("prs", s, expand, noisy);
			/* happens when we have only 1.0 delta */
			unless (s->rstart) goto next;
		}
		assert(s->rstop);
		if (flags & PRS_ALL) sccs_markMeta(s);
		if (doheader) {
			printf("======== %s %s", s->gfile, s->rstart->rev);
			if (s->rstop != s->rstart) {
				printf("..%s", s->rstop->rev);
			}
			printf(" ========\n");
		}
		if (opposite) {
			for (e = s->table; e; e = e->next) {
				if (e->flags & D_SET) {
					e->flags &= ~D_SET;
				} else {
					e->flags |= D_SET;
				}
			}
		}
		sccs_prs(s, flags, reverse, dspec, stdout);
next:		sccs_free(s);
	}
	sfileDone();
	purify_list();
	return (0);
}
