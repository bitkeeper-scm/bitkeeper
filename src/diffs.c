/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

/*
 * diffs - show differences of SCCS revisions.
 *
 * diffs file file file....
 *	for each file that is checked out, diff it against the old version.
 * diffs -r<rev> file
 *	diff the checked out (or TOT) against rev like so
 *	diff rev TOT
 * diffs -r<r1>..<r2> file
 *	diff the two revisions like so
 *	diff r1 r2
 *
 * In a quite inconsistent but (to me) useful fashion, I don't default to
 * all files when there are no arguments.  I want
 *	diffs
 *	diffs -dalpha1
 * to behave differently.
 */
char	*diffs_help = "\n\
usage: diffs [-acDMsuU] [-d<d>] [-r<r>] [files...]\n\n\
    -a		do diffs on all sfiles\n\
    -c		do context diffs\n\
    -d<dates>	diff using date or symbol\n\
    -h		don't print headers\n\
    -D		prefix lines with dates\n\
    -M		prefix lines with revision numbers\n\
    -p		procedural diffs, like diff -p\n\
    -r<r>	diff revision <r>\n\
    -s		do side by side\n\
    -u		do unified diffs\n\
    -U		prefix lines with user names\n\
    -v		be verbose about non matching ranges\n\n\
    Ranges of dates, symbols, and/or revisions may be specified.\n\n\
    -r1.3..1.6	    diffs 1.3 vs 1.6\n\
    -d9207..92	    diffs changes from July 1 '92 to Dec 31 '92\n\
    -d92..92	    diffs changes from Jan 1 '92 to Dec 31 '92\n\
    The date can be a symbol instead of a date.  Dates and revisions may\n\
    be mixed and matched, see range(1) for a full description.\n\n\
";

int
main(int ac, char **av)
{
	sccs	*s;
	int	all = 0, flags = SILENT, c;
	char	kind;
	char	*name;
	int	headers = 1;
	RANGE_DECL;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		fprintf(stderr, diffs_help);
		return (1);
	}

	if (name = strrchr(av[0], '/')) {
		kind = streq(++name, "sdiffs") ? DF_SDIFF : DF_DIFF;
	} else {
		kind = streq(av[0], "sdiffs") ? DF_SDIFF : DF_DIFF;
	}
	while ((c = getopt(ac, av, "acd;DhMnpr|suUv")) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    case 'h': headers = 0; break;
		    case 'c': kind = DF_CONTEXT; break;
		    case 'D': flags |= PREFIXDATE; break;
		    case 'M': flags |= REVNUMS; break;
		    case 'p': kind = DF_PDIFF; break;
		    case 'n': kind = DF_RCS; break;
		    case 's': kind = DF_SDIFF; break;
		    case 'u': kind = DF_UNIFIED; break;
		    case 'U': flags |= USER; break;
		    RANGE_OPTS('d', 'r');
		    default:
usage:			fprintf(stderr, "diffs: usage error, try --help\n");
			return (1);
		}
	}

	/*
	 * If we specified both revisions then we don't need the gfile.
	 * If we specifed one rev, then the gfile is also optional, we'll
	 * do TOT if it isn't there.
	 * If we specified no revs then there must be a gfile.
	 */
	if ((flags&(PREFIXDATE|USER|REVNUMS)) && (things != 2)) {
		fprintf(stderr,
		    "%s: must have both revisions with -d|u|m\n", av[0]);
		return (1);
	}

	if (all || things) {
		name = sfileFirst("diffs", &av[optind], 0);
	} else {
		name = sfileFirst("diffs", &av[optind], SF_GFILE);
	}
	while (name) {
		int	ex = 0;
		char	*r1 = 0, *r2 = 0;

		unless ((s = sccs_init(name, flags))) goto next;
		RANGE("diffs", s, 0, (flags & SILENT) == 0);
		if (things) {
			unless (s->rstart && (r1 = s->rstart->rev)) goto next;
			if (s->rstop) r2 = s->rstop->rev;
			/*
			 * If we did a date specification and that covered only
			 * one delta, bump it backwards to get some diffs.
			 */
			if (d[0] && (!r[1] || d[1]) &&
			    (s->rstart == s->rstop) && s->rstart->parent) {
				s->rstart = s->rstart->parent;
				r1 = s->rstart->rev;
			}
		}
		if (HAS_GFILE(s) && !IS_WRITABLE(s)) ex = EXPAND;
		if (headers) ex |= VERBOSE;

		/*
		 * Errors come back as -1/-2/-3/0
		 * -2/-3 means it couldn't find the rev; ignore.
		 */
		switch (sccs_diffs(s, r1, r2, ex|flags, kind, stdout)) {
		    case -2:
		    case -3:
		    case 0:	break;
		    default:
			fprintf(stderr,
			    "diffs of %s failed.\n", s->gfile);
		}
next:		if (s) sccs_free(s);
		name = sfileNext();
	}
	sfileDone();
	purify_list();
	return (0);
}
