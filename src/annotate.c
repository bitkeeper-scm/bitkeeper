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
	int	flags = BASE_FLAGS;
	int	c, errors = 0;
	int	pnames = getenv("BK_PRINT_EACH_NAME") != 0;
	char	*t, *name, *Rev = 0, *rev = 0, *cdate = 0;
	delta	*d;

	debug_main(av);
	name = strrchr(av[0], '/');

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help annotate");
		return (1);
	}
	if (t = getenv("BK_ANNOTATE")) flags = annotate_args(flags, t);
	while ((c = getopt(ac, av, "A;a;c;kr;")) != -1) {
		switch (c) {
		    case 'A':
			flags |= GET_ALIGN;
			/*FALLTHROUGH*/
		    case 'a':
			flags = annotate_args(flags, optarg);
			if (flags == -1) goto usage;
			break;
		    case 'c': cdate = optarg; break;		/* doc 2.0 */
		    case 'k': flags &= ~GET_EXPAND; break;	/* doc 2.0 */
		    case 'r': Rev = optarg; break;		/* doc 2.0 */

		    default:
		usage:
			system("bk help -s annotate");
			return (1);
		}
	}
	if (flags == BASE_FLAGS) flags |= GET_REVNUMS|GET_USER;
	name = sfileFirst("annotate", &av[optind], SF_HASREVS);
	for (; name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) {
			sccs_free(s);
			continue;
		}
		if (s->encoding & E_BINARY) {
			sccs_free(s);
			continue;
		}
		if (cdate) {
			s->state |= S_RANGE2;
			d = sccs_getrev(s, 0, cdate, ROUNDUP);
			unless (d) {
				fprintf(stderr,
				    "No delta like %s in %s\n",
				    cdate, s->sfile);
				sccs_free(s);
				continue;
			}
			rev = d->rev;
		} else if (Rev) {
			rev = Rev;
		} else {
			rev = sfileRev();
		}
		if (pnames) {
			printf("FILE|%s|CRC|%x\n", s->gfile, crc(s->gfile));
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
	return (errors);
}
