/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");
char	*prs_help = "\n\
usage: prs [-mv] [-c<d>] [-I<rev>] [-r<r>] [files...]\n\n\
    -1		Print exactly one record, as specified by -r/-c\n\
    -c<date>	Cut off dates.  See sccsrange(1) for details.\n\
    -d<spec>    Specify output data specification\n\
    -I<rev>	Print a single rev in the format needed for ``delta -I''\n\
    		<rev> is option, uses TOT if no rev specified.\n\
    -m		print metadata, such as users and symbols.\n\
    -r<rev>	Specify a revision, or part of a range.\n\
    -v		Complain about SCCS files that do not match the range.\n\n\
    -h		Supress range header.\n\n\
    Note that <date> may be a symbol, which implies the date of the\n\
    delta which matches the symbol.\n\n\
    Range specifications:\n\
    	-cAlpha,Beta	Everything after Alpha up to and including Beta.\n\
	-c'[97,98]'	Everything in 1997 and 1998.\n\
	-c97..98	Same thing, alternative notation.\n\
	-c'[97' -r1.19	Everything from the start of '97 up to rev 1.19.\n\n";

int
main(int ac, char **av)
{
	sccs	*s;
	int	doheader = 1, didone = 0, flags = SILENT|SHUTUP;
	int	c;
	char	*name;
	char	*r[2], *d[2];
	int	things = 0, rd = 0;
	int	noisy = 0;
	int	one = 0;
	char	*dspec = NULL;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		fprintf(stderr, prs_help);
		return (1);
	}
	r[0] = r[1] = d[0] = d[1] = 0;
	while ((c = getopt(ac, av, "1c;d:hI|mr|v")) != -1) {
		switch (c) {
		    case '1': one = 1; break;
		    case 'c':
			if (things == 2) goto usage;
			d[rd++] = optarg;
			things += tokens(optarg);
			break;
		    case 'd':
			dspec = optarg;
			break;
		    case 'h':
			doheader = 0;
			break;
		    case 'm': flags &= ~SILENT; break;
		    case 'I': one = 1; doheader = 0;
		    /* Fall through */
		    case 'r':
			if (things == 2) goto usage;
			r[rd++] = notnull(optarg);
			things += tokens(notnull(optarg));
			break;
		    case 'v': noisy = 1; break;
		    default:
usage:			fprintf(stderr, "prs: usage error, try --help\n");
			return (1);
		}
	}
	if ((things == 1) && r[0] && (roundType(r[0]) == EXACT)) one = 1;
	if (one && (things != 1)) {
		fprintf(stderr, "prs: -1 needs a date or a revision.\n");
		goto usage;
	}
	for (name = sfileFirst("prs", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, flags)) continue;
		if (!s->tree) goto next;
		rangeReset(s);
		if (things == 2) {
			if (rangeAdd(s, r[0], d[0])) {
				if (noisy) {
not0:					fprintf(stderr,
					    "prs: no delta ``%s'' in %s\n",
					    r[0] ? r[0] : d[0], s->sfile);
				}
				goto next;
			}
			if ((r[1] || d[1]) && (rangeAdd(s, r[1], d[1]) == -1)) {
				if (noisy) {
					fprintf(stderr,
					    "prs: no delta ``%s'' in %s\n",
					    r[1] ? r[1] : d[1], s->sfile);
				}
				goto next;
			}
			if (!s->rstart) goto next;
		} else if (things == 1) {
			delta	*tmp = sccs_getrev(s, r[0], d[0], ROUNDUP);

			if (!tmp) if (noisy) goto not0; else goto next;
			if (one) {
				s->rstop = s->rstart = tmp;
			} else if (roundType(r[0] ? r[0] : d[0]) == ROUNDUP) {
				s->rstart = s->tree;
				s->rstop = tmp;
			} else {
				s->rstart = tmp;
			}
		}
		if (didone++) printf("\n");
		if (doheader) {
			printf("======== %s %s", s->gfile,
			    s->rstart ? s->rstart->rev : s->tree->rev);
			if (s->rstop != s->rstart) {
				printf("..%s", s->rstop ? s->rstop->rev : "");
			}
			printf(" ========\n");
		}
		sccs_prs(s, flags, dspec, stdout);
next:		sccs_free(s);
	}
	sfileDone();
	purify_list();
	return (0);
}
