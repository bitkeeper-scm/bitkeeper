/*
 * Copyright 1997-2006,2009-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"
#include "sccs.h"
#include "range.h"

typedef struct {
	int	flags;
	int	doheader;
	int	reverse;
	int	n_revs;
	char	*dspec;
	RANGE	rargs;
} Opts;

private	void	doLog(Opts *opts, sccs *s);

int
prs_main(int ac, char **av)
{
	return (log_main(ac, av));
}

int
log_main(int ac, char **av)
{
	Opts	*opts;
	sccs	*s = 0;
	ser_t	e;
	int	log = streq(av[0], "log");
	int	init_flags = INIT_NOCKSUM;
	int	rflags = 0;
	int	rc = 0;
	int	local = 0;
	char	*url = 0;
	int	standalone = 0;
	int	c;
	char	*name;
	char	*cset = 0, *tip = 0;
	int	want_parent = 0;
	pid_t	pid = 0;	/* pager */
	longopt	lopts[] = {
		{ "dspecf;", 300 },		/* let user pass in dspec */
		{ "lattice", 310 },		/* color lattice */
		{ "longest", 320 },		/* color longest */
		{ "standalone", 'S' },		/* alias */
		{ 0, 0 }
	};

	opts = new(Opts);
	opts->doheader = !log;
	while ((c = getopt(ac, av, "0123456789abc;C;d:DfhL|MnopSr;Y",
		    lopts)) != -1) {
		switch (c) {
		    case '0': case '1': case '2': case '3': case '4':
		    case '5': case '6': case '7': case '8': case '9':
			opts->doheader = 0;
			opts->n_revs = opts->n_revs * 10 + (c - '0');
			break;
		    case 'a':					/* doc 2.0 */
			opts->flags |= PRS_ALL;
			break;
		    case 'D': break;	/* obsoleted in 4.0 */
		    case 'f':					/* doc */
		    case 'b': opts->reverse++; break;		/* undoc */
		    case 'C': cset = optarg; break;		/* doc 2.0 */
		    case 'd':					/* doc 2.0 */
			if (opts->dspec) usage();
			opts->dspec = strdup(optarg);
			break;
		    case 'h': opts->doheader = 0; break;	/* doc 2.0 */
		    case 'L':
			local = 1;
			url = optarg;
			break;
		    case 'M': 	/* for backward compat, undoc 2.0 */
			      break;
		    case 'n': opts->flags |= PRS_LF; break;	/* doc 2.0 */
		    case 'o':
			 fprintf(stderr,
			     "%s: the -o option has been removed\n", av[0]);
			 usage();
		    case 'p': want_parent = 1; break;
		    case 'S': standalone = 1; break;
		    case 'x':
			fprintf(stderr, "prs: -x support dropped\n");
			usage();
		    case 'Y': 	/* for backward compat, undoc 2.0 */
			      break;
		    case 'c':
			if (range_addArg(&opts->rargs, optarg, 1)) usage();
			break;
		    case 'r':
			if (range_addArg(&opts->rargs, optarg, 0)) usage();
			break;
		    case 300:	/* --dspecf */
			if (opts->dspec) usage();
			unless (opts->dspec = loadfile(optarg, 0)) {
				fprintf(stderr,
				    "%s: cannot load file \"%s\"\n",
				    prog, optarg);
				return (1);
			}
			break;
		    case 310:	/* --lattice */
			if (rflags) bk_badArg(c, av);
		    	rflags = RANGE_LATTICE;
			break;
		    case 320:	/* --longest */
			if (rflags) bk_badArg(c, av);
		    	rflags = RANGE_LONGEST;
			break;
		    default: bk_badArg(c, av);
		}
	}
	unless (rflags) rflags = RANGE_SET;
	rflags |= SILENT;
	unless (opts->dspec) {
		char	*specf;
		char	*spec = log ? "dspec-log" : "dspec-prs";

		specf = bk_searchFile(spec);
		T_DEBUG("Reading dspec from %s", specf ? specf : "(not found)");
		unless (specf && (opts->dspec = loadfile(specf, 0))) {
			fprintf(stderr,
			    "%s: cant find %s/%s\n", av[0], bin, spec);
			return (1);
		}
		free(specf);
	}
	if (local) {
		if (range_urlArg(&opts->rargs, url, standalone) ||
		    range_addArg(&opts->rargs, "+", 0)) {
			return (1);
		}
	} else if (standalone) {
		fprintf(stderr,
		    "%s: -S only can be used if -L is also used\n", prog);
		return (1);
	}
	dspec_collapse(&opts->dspec, 0, 0);

	if (opts->rargs.rstart && (cset || tip)) {
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
	if (local && !av[optind]) {
		char	*slopts = aprintf("rm%s", standalone ? "S" : "");

		name = sfiles_local(opts->rargs.rstart, slopts);
		free(slopts);
	} else {
		name = sfileFirst(av[0], &av[optind], 0);
	}
	for (; name; name = sfileNext()) {
		unless (s && streq(name, s->sfile)) {
			if (s) {
				doLog(opts, s);
				sccs_free(s);
			}
			unless (s = sccs_init(name, init_flags)) continue;
		}
		unless (HASGRAPH(s)) goto next;
		if (cset) {
			unless (e = sccs_findrev(s, cset)) goto next;
			poly_range(s, e, 0);	/* pick one */
		} else if (want_parent) {
			if (range_process(av[0], s, rflags, &opts->rargs)) {
				goto next;
			}
			unless (s->rstart && (s->rstart == s->rstop)
			    && !MERGE(s, s->rstart)) {
				fprintf(stderr,
				    "Warning: %s: -p requires a single "
				    "non-merge revision\n", s->gfile);
				goto next;
			}
			unless (s->rstart = PARENT(s, s->rstart)) {
				fprintf(stderr,
				    "Warning: %s: %s has no parent\n",
				    s->gfile, REV(s, s->rstop));
				goto next;
			}
			FLAGS(s, s->rstop) &= ~D_SET;
			FLAGS(s, s->rstart) |= D_SET;
			s->rstop = s->rstart;
		} else {
			if (range_process(av[0], s, rflags, &opts->rargs)) {
				goto next;
			}
			if (!opts->rargs.rstart && !sfileRev() &&
			    streq(REV(s, TREE(s)), "1.0")) {
				/* we don't want 1.0 by default */
				FLAGS(s, TREE(s)) &= ~D_SET;
				if (s->rstart == TREE(s)) {
					s->rstart = sccs_kid(s, s->rstart);
				}
			}
		}
		continue;
next:		rc = 1;
		sccs_free(s);
		s = 0;
	}
	if (s) {
		doLog(opts, s);
		sccs_free(s);
	}
	if (sfileDone()) rc = 1;
	free(opts->dspec);
	free(opts);
	if (log && (pid > 0)) {
		fclose(stdout);
		waitpid(pid, 0, 0);
	}
	return (rc);
}

private void
doLog(Opts *opts, sccs *s)
{
	if (opts->flags & PRS_ALL) range_markMeta(s);
	if (opts->doheader) {
		printf("======== %s ", s->gfile);
		if (opts->rargs.rstart) {
			printf("%s", opts->rargs.rstart);
			if (opts->rargs.rstop) printf("..%s", opts->rargs.rstop);
			putchar(' ');
		}
		printf("========\n");
	}
	s->prs_nrevs = opts->n_revs;
	sccs_prs(s, opts->flags, opts->reverse, opts->dspec, stdout);
}
