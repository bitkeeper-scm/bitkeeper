/*
 * Stolen from get.c and stripped down.
 *
 * Copyright (c) 1997 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

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
		system("bk help annotate");
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
		    case 'a': flags |= GET_ALIGN; break;	/* doc 2.0 */
		    case 'c': cdate = optarg; break;	/* doc 2.0 */
		    case 'd': flags |= GET_PREFIXDATE; break;	/* doc 2.0 */
		    case 'k': flags &= ~GET_EXPAND; break;	/* doc 2.0 */
		    case 'm': flags |= GET_REVNUMS; break;	/* doc 2.0 */
		    case 'n': flags |= GET_MODNAME; break;	/* doc 2.0 */
		    case 'N': flags |= GET_LINENUM; break;	/* doc 2.0 */
		    case 'r': rev = optarg; break;	/* doc 2.0 */
		    case 'u': flags |= GET_USER; break;	/* doc 2.0 */

		    default:
usage:			system("bk help -s annotate");
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
		if (s->encoding & E_BINARY) {
			fprintf(stderr, "Skipping binary file %s\n", s->gfile);
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
