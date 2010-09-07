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
	int	rflags = SILENT|RANGE_SET;
	int	flags = 0, sf_flags = 0, rc = 0, one = 0;
	int	c;
	char	*name;
	char	*cset = 0, *tip = 0;
	int	want_parent = 0;
	pid_t	pid = 0;	/* pager */
	char	*dspec = 0;
	RANGE	rargs = {0};

	while ((c = getopt(ac, av, "1abc;C;d:DfhMnopr;Y", 0)) != -1) {
		switch (c) {
		    case '1': one = 1; doheader = 0; break;
		    case 'a':					/* doc 2.0 */
			flags |= PRS_ALL;
			break;
		    case 'D': break;	/* obsoleted in 4.0 */
		    case 'f':					/* doc */
		    case 'b': reverse++; break;			/* undoc */
		    case 'C': cset = optarg; break;		/* doc 2.0 */
		    case 'd': dspec = strdup(optarg); break;	/* doc 2.0 */
		    case 'h': doheader = 0; break;		/* doc 2.0 */
		    case 'M': 	/* for backward compat, undoc 2.0 */
			      break;
		    case 'n': flags |= PRS_LF; break;		/* doc 2.0 */
		    case 'o': 
			 fprintf(stderr,
			     "%s: the -o option has been removed\n", av[0]);
			 usage();
		    case 'p': want_parent = 1; break;
		    case 'x':
			fprintf(stderr, "prs: -x support dropped\n");
			usage();
		    case 'Y': 	/* for backward compat, undoc 2.0 */
			      break;
		    case 'c':
			if (range_addArg(&rargs, optarg, 1)) usage();
			break;
		    case 'r':
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    default: bk_badArg(c, av);
		}
	}
	// XXX removed BK_LOG_DSPEC (ok?)
	unless (dspec) {
		char	*specf;
		char	*spec = log ? "dspec-log" : "dspec-prs";

		specf = bk_searchFile(spec);
		TRACE("Reading dspec from %s", specf ? specf : "(not found)");
		unless (specf && (dspec = loadfile(specf, 0))) {
			fprintf(stderr,
			    "%s: cant find %s/%s\n", av[0], bin, spec);
			return (1);
		}
		free(specf);
	}
	dspec_collapse(&dspec, 0, 0);

	if (rargs.rstart && (cset || tip)) {
		fprintf(stderr, "%s: -c, -C, and -r are mutually exclusive.\n",
		    av[0]);
		exit(1);
	}
	if (cset && want_parent) {
		fprintf(stderr, "%s: -p and -C are mutually exclusive.\n",
		    av[0]);
		exit(1);
	}
	if (log) pid = mkpager();
	for (name = sfileFirst(av[0], &av[optind], sf_flags);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, init_flags)) continue;
		unless (HASGRAPH(s)) goto next;
		if (cset) {
			unless (e = sccs_findrev(s, cset)) goto next;
			range_cset(s, e, D_SET);
		} else if (want_parent) {
			if (range_process(av[0], s, rflags, &rargs)) {
				goto next;
			}
			unless (s->rstart && (s->rstart == s->rstop)
			    && !s->rstart->merge) {
				fprintf(stderr,
				    "Warning: %s: -p requires a single "
				    "non-merge revision\n", s->gfile);
				goto next;
			}
			unless (s->rstart = PARENT(s, s->rstart)) {
				fprintf(stderr,
				    "Warning: %s: %s has no parent\n",
				    s->gfile, s->rstop->rev);
				goto next;
			}
			s->rstop->flags &= ~D_SET;
			s->rstart->flags |= D_SET;
			s->rstop = s->rstart;
		} else {
			if (range_process(av[0], s, rflags, &rargs)) {
				goto next;
			}
			if (!rargs.rstart && !sfileRev() &&
			    streq(s->tree->rev, "1.0")) {
				/* we don't want 1.0 by default */
				s->tree->flags &= ~D_SET;
				if (s->rstart == s->tree) {
					s->rstart = KID(s->tree);
				}
			}
		}
		if (flags & PRS_ALL) range_markMeta(s);
		if (doheader) {
			printf("======== %s ", s->gfile);
			if (rargs.rstart) {
				printf("%s", rargs.rstart);
				if (rargs.rstop) printf("..%s", rargs.rstop);
				putchar(' ');
			}
			printf("========\n");
		}
		s->prs_one = one;
		sccs_prs(s, flags, reverse, dspec, stdout);
		sccs_free(s);
		continue;
		
next:		rc = 1;
		sccs_free(s);
	}
	if (sfileDone()) rc = 1;
	free(dspec);
	if (log && (pid > 0)) {
		fclose(stdout);
		waitpid(pid, 0, 0);
	}
	return (rc);
}
