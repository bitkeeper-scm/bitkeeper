/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");
char	*prs_help = "\n\
usage: prs [-hmv] [-c<d>] [-I<rev>] [-r<r>] [files...]\n\n\
    -c<date>	Cut off dates.  See sccsrange(1) for details.\n\
    -C		do not include branch deltas which are not merged\n\
    -d<spec>    Specify output data specification\n\
    -h		Suppress range header.\n\
    -m		print metadata, such as users and symbols.\n\
    -r<rev>	Specify a revision, or part of a range.\n\
    -v		Complain about SCCS files that do not match the range.\n\n\
    Note that <date> may be a symbol, which implies the date of the\n\
    delta which matches the symbol.\n\n\
    Range specifications:\n\
    -r+		    prints the most recent delta\n\
    -r1.3..1.6	    prints all deltas 1.3 through 1.6\n\
    -d9207..92	    prints all deltas from July 1 '92 to Dec 31 '92\n\
    -d92..92	    prints all deltas from Jan 1 '92 to Dec 31 '92\n\n";

int
main(int ac, char **av)
{
	sccs	*s;
	int	reverse = 0, doheader = 1, didone = 0;
	int	init_flags = INIT_NOCKSUM;
	int	flags = 0;
	int	c;
	char	*name;
	int	noisy = 0;
	int	expand = 1;
	char	*dspec = NULL;
	RANGE_DECL;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		fprintf(stderr, prs_help);
		return (1);
	}
	while ((c = getopt(ac, av, "bc;Cd:hmr|v")) != -1) {
		switch (c) {
		    case 'b': reverse++; break;
		    case 'C': expand = 3; break;
		    case 'd':
			dspec = optarg;
			break;
		    case 'h':
			doheader = 0;
			break;
		    case 'm': flags |= PRS_META; break;
		    case 'v': noisy = 1; break;
		    RANGE_OPTS('c', 'r');
		    default:
usage:			fprintf(stderr, "prs: usage error, try --help\n");
			return (1);
		}
	}
	for (name = sfileFirst("prs", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, init_flags, 0)) continue;
		if (!s->tree) goto next;
		RANGE("prs", s, expand, noisy);
		unless(s->rstart) goto next; /* happen when we have only 1.0 delta */
		assert(s->rstop);
		if (doheader) {
			printf("======== %s %s", s->gfile, s->rstart->rev);
			if (s->rstop != s->rstart) {
				printf("..%s", s->rstop->rev);
			}
			printf(" ========\n");
		}
		sccs_prs(s, flags, reverse, dspec, stdout);
next:		sccs_free(s);
	}
	sfileDone();
	purify_list();
	return (0);
}
