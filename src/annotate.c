/*
 * Stolen from get.c and stripped down.
 *
 * Copyright (c) 1997 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");
private	const char annotate_help[] = "\
usage: annotate [-adkmnNu] [-r<rev> | -c<date>] [files... | -]\n\
   -a		align prefix output in a human readable form.\n\
   -c<date>	get the latest revision before the date\n\
   -d		prefix each line with the date it was last modified\n\
   -k		do not expand keywords\n\
   -m		prefix each line with the revision which created that line\n\
   -N		prefix each line with its line number\n\
   -n		prefix each line with the filename\n\
   -r<rev>	get this revision\n\
   -u		prefix each line with the user who last modified it\n\n\
The annotate command can get it's options from $BK_ANNOTATE\n\
we usually set this to BK_ANNOTATE=mnu\n";

#define	BASE_FLAGS	(GET_EXPAND|PRINT|SILENT)

int
annotate_main(int ac, char **av)
{
	sccs	*s;
	int	iflags = INIT_SAVEPROJ, flags = BASE_FLAGS;
	int	c, errors = 0;
	char	*t, *name, *rev = 0, *cdate = 0;
	delta	*d;
	project	*proj = 0;

	debug_main(av);
	name = strrchr(av[0], '/');

	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, annotate_help);
		return (1);
	}
	for (t = getenv("BK_ANNOTATE"); t && *t; t++) {
		switch (*t) {
		    case 'a': flags |= GET_ALIGN; break;
		    case 'd': flags |= GET_PREFIXDATE; break;
		    case 'k': flags &= ~GET_EXPAND; break;
		    case 'm': flags |= GET_REVNUMS; break;
		    case 'n': flags |= GET_MODNAME; break;
		    case 'N': flags |= GET_LINENUM; break;
		    case 'u': flags |= GET_USER; break;
		}
	}
	while ((c = getopt(ac, av, "ac;dkmnNr;u")) != -1) {
		switch (c) {
		    case 'a': flags |= GET_ALIGN; break;
		    case 'c': cdate = optarg; break;
		    case 'd': flags |= GET_PREFIXDATE; break;
		    case 'k': flags &= ~GET_EXPAND; break;
		    case 'm': flags |= GET_REVNUMS; break;
		    case 'n': flags |= GET_MODNAME; break;
		    case 'N': flags |= GET_LINENUM; break;
		    case 'r': rev = optarg; break;
		    case 'u': flags |= GET_USER; break;

		    default:
usage:			fprintf(stderr, "%s: usage error, try get --help\n",
				av[0]);
			return (1);
		}
	}
	if (flags == BASE_FLAGS) flags |= GET_REVNUMS|GET_USER;
	name = sfileFirst("get", &av[optind], SF_HASREVS);
	for (; name; name = sfileNext()) {
		unless (s = sccs_init(name, iflags, proj)) continue;
		unless (proj) proj = s->proj;
		unless (s->tree) {
			sccs_free(s);
			continue;
		}
		if (cdate) {
			s->state |= S_RANGE2;
			d = sccs_getrev(s, 0, cdate, ROUNDUP);
			if (!d) {
				fprintf(stderr,
				    "No delta like %s in %s\n",
				    cdate, s->sfile);
				sccs_free(s);
				continue;
			}
			rev = d->rev;
		} else unless (rev) {
			rev = sfileRev();
		}
		if (sccs_get(s, rev, 0, 0, 0, flags, "-")) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "get of %s failed, skipping it.\n", name);
			}
			errors = 1;
		}
		sccs_free(s);
	}
	sfileDone();
	if (proj) proj_free(proj);
	return (errors);
}
