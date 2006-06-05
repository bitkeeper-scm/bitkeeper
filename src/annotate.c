/*
 * Stolen from get.c and stripped down.
 *
 * Copyright (c) 1997 L.W.McVoy
 */
#include "system.h"
#include "sccs.h"
#include "range.h"

#define	BASE_FLAGS	(GET_EXPAND|PRINT|SILENT)
#define	ME		"annotate"

int
annotate_main(int ac, char **av)
{
	sccs	*s;
	delta	*d;
	int	flags = BASE_FLAGS, errors = 0;
	int	pnames = getenv("BK_PRINT_EACH_NAME") != 0;
	int	c;
	char	*t, *name, *range = 0, *Rev = 0, *rev = 0, *cdate = 0;
	char	all[10];

	name = strrchr(av[0], '/');

	if (t = getenv("BK_ANNOTATE")) {
		if ((flags = annotate_args(flags, t)) == -1) {
			fprintf(stderr,
			    "annotate: bad flags in $BK_ANNOTATE\n");
			return (1);
		}
	}
	while ((c = getopt(ac, av, "A;a;Bc;hkr;R|")) != -1) {
		switch (c) {
		    case 'A':
			flags |= GET_ALIGN;
			/*FALLTHROUGH*/
		    case 'a':
			flags = annotate_args(flags, optarg);
			if (flags == -1) goto usage;
			break;
		    case 'B': break;		   /* skip binary, default */	
		    case 'c': cdate = optarg; break;		/* doc 2.0 */
		    case 'h': flags |= GET_NOHASH; break;
		    case 'k': flags &= ~GET_EXPAND; break;	/* doc 2.0 */
		    case 'r': Rev = optarg; break;		/* doc 2.0 */
		    case 'R':
			unless (range = optarg) {
		    		sprintf(all, "1.0..");
				range = all;
			}
			flags &= ~GET_EXPAND;
			break;
		    default:
usage:			system("bk help -s annotate");
			return (1);
		}
	}

	if (Rev && closedRange(Rev)) goto usage;
	if (cdate && closedRange(cdate)) goto usage;
	if (range && (Rev || cdate)) goto usage;

	/* original annotate only, not -R sccscat replacement */
	if (!range && (flags == BASE_FLAGS)) flags |= GET_REVNUMS|GET_USER;
	name = sfileFirst(ME, &av[optind], 0);
	for (; name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) {
err:			errors = 1;
			sccs_free(s);
			continue;
		}
		if (s->encoding & E_BINARY) goto err;
		if (range) {
			int	e = closedRange(range) ? 3 : 2;
			int	things = 1;
			char	*d[2] = { 0, 0 };

			c = rangeProcess(ME, s, e, 0, 0, &things, 1, &range, d);
			if (c) goto err;
		} else if (cdate) {
			s->state |= S_RANGE2;
			d = sccs_getrev(s, 0, cdate, ROUNDUP);
			unless (d) {
				fprintf(stderr,
				    "No delta like %s in %s\n",
				    cdate, s->sfile);
				goto err;
			}
			rev = d->rev;
		} else if (Rev) {
			rev = Rev;
		} else {
			rev = sfileRev();
		}
		if (pnames) {
			printf("|FILE|%s|CRC|%u\n", s->gfile, crc(s->gfile));
		}
		if (range) {
			c = sccs_cat(s, flags, "-");
		} else {
			c = sccs_get(s, rev, 0, 0, 0, flags, "-");
		}
		if (c) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "annotate of %s failed, skipping it.\n",
				    name);
			}
			errors = 1;
		}
		sccs_free(s);
	}
	if (sfileDone()) errors = 1;
	return (errors);
}
