/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");
char	*prs_help = "\n\
usage: prs [-mv] [-c<d>] [-I<rev>] [-r<r>] [files...]\n\n\
    -1		Print exactly one record, as specified by -r/-c\n\
    -c<date>	Cut off dates.  See sccsrange(1) for details.\n\
    -d<spec>    Specify output data specification\n\
    -m		print metadata, such as users and symbols.\n\
    -r<rev>	Specify a revision, or part of a range.\n\
    -v		Complain about SCCS files that do not match the range.\n\n\
    -h		Supress range header.\n\n\
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
	int	flags = NOCKSUM|SILENT|SHUTUP;
	int	c;
	char	*name, *rev;
	int	noisy = 0;
	char	*dspec = NULL;
	RANGE_DECL;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		fprintf(stderr, prs_help);
		return (1);
	}
	while ((c = getopt(ac, av, "bc;d:hmr|v")) != -1) {
		switch (c) {
		    case 'b': reverse++;
		    case 'd':
			dspec = optarg;
			break;
		    case 'h':
			doheader = 0;
			break;
		    case 'm': flags &= ~SILENT; break;
		    case 'v': noisy = 1; break;
		    RANGE_OPTS('c', 'r');
		    default:
usage:			fprintf(stderr, "prs: usage error, try --help\n");
			return (1);
		}
	}
	for (name = sfileFirst("prs", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, flags)) continue;
		if (!s->tree) goto next;
		RANGE("prs", s, 1, noisy);
		assert(s->rstart);
		assert(s->rstop);
		if (didone++) printf("\n");
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
