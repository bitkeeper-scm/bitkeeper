/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

int
prs_main(int ac, char **av)
{
	sccs	*s;
	delta	*e;
	int	reverse = 0, doheader = 1;
	int	init_flags = INIT_NOCKSUM|INIT_SAVEPROJ;
	int	flags = 0;
	int	opposite = 0;
	int	rc = 0, c;
	char	*name, *xrev = 0;
	char	*cset = 0;
	int	noisy = 0;
	int	expand = 1;
	char	*dspec = NULL, *year4 = NULL;
	project	*proj = 0;
	RANGE_DECL;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help prs");
		return (1);
	}
	while ((c = getopt(ac, av, "abc;C;d:hmMor|x:vY")) != -1) {
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
		    case 'x': xrev = optarg; break;
		    case 'v': noisy = 1; break;
		    case 'Y': year4 = strdup("BK_YEAR4=1");
			      putenv(year4);
			      break;
		    RANGE_OPTS('c', 'r');
		    default:
usage:			system("bk help -s prs");
			if (year4) free(year4);
			return (1);
		}
	}
	if (things && cset) {
		fprintf(stderr, "prs: -r or -C but not both.\n");
		exit(1);
	}
	for (name = sfileFirst("prs", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, init_flags, proj)) continue;
		unless (proj) proj = s->proj;
		unless (s->tree) goto next;
		if (cset) {
			delta	*d = sccs_getrev(s, cset, 0, 0);

			if (!d) {
				rc = 1;
				goto next;
			}
			rangeCset(s, d);
		} else {
			RANGE("prs", s, expand, noisy);
			/* happens when we have only 1.0 delta */
			unless (s->rstart) goto next;
		}
		assert(s->rstop);
		if (flags & PRS_ALL) sccs_markMeta(s);
		if (doheader) {
			printf("======== %s %s%s%s",
			    s->gfile,
			    opposite ? "!" : "",
			    s->rstart->rev,
			    (xrev && streq(xrev, "1st")) ? "+" : "");
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
		if (xrev) {
			unless (s->state & S_SET) { 
				int	check = strcmp(xrev, "1st");

				for (e = s->rstop; e; e = e->next) {
					unless (check && streq(xrev, e->rev)) {
						e->flags |= D_SET;
					}
					if (e == s->rstart) break;
				}
				s->state |= S_SET;
				unless (check) s->rstart->flags &= ~D_SET;
			} else {
				if (streq(xrev, "1st")) {
					s->rstart->flags &= ~D_SET;
				} else {
					e = findrev(s, xrev);
					if (e) e->flags &= ~D_SET;
				}
			}
		}
		sccs_prs(s, flags, reverse, dspec, stdout);
		sccs_free(s);
		continue;
		
next:		rc = 1;
		sccs_free(s);
	}
	sfileDone();
	if (proj) proj_free(proj);
	if (year4) free(year4);
	return (rc);
}
