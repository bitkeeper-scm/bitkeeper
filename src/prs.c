/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"

int
prs_main(int ac, char **av)
{
	return (log_main(ac, av));
}

int
log_main(int ac, char **av)
{
	sccs	*s;
	delta	*e;
	int	log = streq(av[0], "log");
	int	reverse = 0, doheader = !log;
	int	init_flags = INIT_NOCKSUM;
	int	flags = 0, sf_flags = 0;
	int	opposite = 0;
	int	rc = 0, c;
	char	*name;
	char	*cset = 0, *tip = 0;
	int	noisy = 0;
	int	want_parent = 0;
	pid_t	pid = 0;	/* pager */
	char	*dspec = getenv("BK_LOG_DSPEC");
	RANGE	rargs = {0};

	unless (dspec) dspec = log ? ":LOG:" : ":PRS:";

	while ((c = getopt(ac, av, "abc;C;d:DfhMnopr;vY")) != -1) {
		switch (c) {
		    case 'a':					/* doc 2.0 */
			flags |= PRS_ALL;
			break;
		    case 'D': break;	/* obsoleted in 4.0 */
		    case 'f':					/* doc */
		    case 'b': reverse++; break;			/* undoc */
		    case 'C': cset = optarg; break;		/* doc 2.0 */
		    case 'd': dspec = optarg; break;		/* doc 2.0 */
		    case 'h': doheader = 0; break;		/* doc 2.0 */
		    case 'M': 	/* for backward compat, undoc 2.0 */
			      break;
		    case 'n': flags |= PRS_LF; break;		/* doc 2.0 */
		    case 'o': opposite = 1; doheader = 0; break; /* doc 2.0 */
		    case 'p': want_parent = 1; break;
		    case 'x':
			fprintf(stderr, "prs: -x support dropped\n");
			goto usage;
		    case 'v': noisy = 1; break;			/* doc 2.0 */
		    case 'Y': 	/* for backward compat, undoc 2.0 */
			      break;
		    case 'c':
			if (range_addArg(&rargs, optarg, 1)) goto usage;
			break;
		    case 'r':
			if (range_addArg(&rargs, optarg, 0)) goto usage;
			break;
		    default:
usage:			sys("bk", "help", "-s", av[0], SYS);
			return (1);
		}
	}

	if (rargs.rstart && (cset || tip)) {
		fprintf(stderr, "%s: -c, -C, and -r are mutually exclusive.\n",
		    av[0]);
		exit(1);
	}
	if (log) pid = mkpager();
	for (name = sfileFirst(av[0], &av[optind], sf_flags);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, init_flags)) continue;
		unless (HASGRAPH(s)) goto next;
		if (cset) {
			unless (e = sccs_findrev(s, cset)) {
				rc = 1;
				goto next;
			}
			range_cset(s, e);
			if (flags & PRS_ALL) sccs_markMeta(s);
		} else {
			if (range_process(av[0], s,
				SILENT|RANGE_SET, &rargs)) {
				goto next;
			}
			if (!rargs.rstart && !sfileRev() &&
			    streq(s->tree->rev, "1.0")) {
				/* we don't want 1.0 by default */
				s->tree->flags &= ~D_SET;
				if (s->rstart == s->tree) {
					s->rstart = s->tree->kid;
				}
			}
			/* happens when we have only 1.0 delta */
			unless (s->rstart) goto next;
		}
		assert(s->rstop);
		if (doheader) {
			printf("======== %s %s%s",
			    s->gfile,
			    opposite ? "!" : "",
			    s->rstart->rev);
			if (s->rstop != s->rstart) {
				printf("-%s", s->rstop->rev);
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
		if (want_parent) {
			unless (SET(s)) {
				for (e = s->rstop; e; e = e->next) {
					e->flags |= D_SET;
					if (e == s->rstart) break;
				}
				s->state |= S_SET;
			}
			for (e = s->table; e; e = e->next) {
				if (e->flags & D_SET) {
					e->flags &= ~D_SET;
					if (e->parent) {
						e->parent->flags |= D_RED;
					} else {
						fprintf(stderr,
					      "Warning: %s: %s has no parent\n",
						    s->gfile, e->rev);
					}
				}
			}
			for (e = s->table; e; e = e->next) {
				if (e->flags & D_RED) {
					e->flags |= D_SET;
					e->flags &= ~D_RED;
				}
			}
		}
		sccs_prs(s, flags, reverse, dspec, stdout);
		sccs_free(s);
		continue;
		
next:		rc = 1;
		sccs_free(s);
	}
	if (sfileDone()) rc = 1;
	if (log && (pid > 0)) {
		fclose(stdout);
		waitpid(pid, 0, 0);
	}
	return (rc);
}
