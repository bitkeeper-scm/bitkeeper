/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"

private int	nulldiff(char *name, u32 kind, u32 flags);

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
 *
 * Rules for -r option:
 *
 *  diffs file
 *	no gfile or readonly gfile
 *		skip
 *	edited gfile
 *		diff TOT gfile
 *  diffs -r<rev> file   or  echo 'file|<rev>' | bk diffs -
 *	no gfile or readonly gfile
 *		diff <rev> TOT    (don't diff with gfile)
 *	edited gfile
 *		diff <rev> gfile
 *  diffs -r+ file
 *	no gfile
 *		skip
 *	readonly gfile
 *		diff TOT gfile (and do keywork expansion on TOT)
 *	edited gfile
 *		diff TOT gfile
 *  diffs -r<rev1> -r<rev2> file or echo 'file|<rev1>' | bk diffs -r<rev2> -
 *	state of gfile doesn't matter
 *		diff <rev1> <rev2>
 *
 *  XXX - need a -N which makes diffs more like diff -Nr, esp. w/ diffs -r@XXX
 */
int
diffs_main(int ac, char **av)
{
	int	rc, c;
	int	verbose = 0, empty = 0, errors = 0, mdiff = 0, force = 0;
	u32	flags = DIFF_HEADER|SILENT, kind = DF_DIFF;
	pid_t	pid = 0; /* lint */
	char	*name;
	char	*Rev = 0, *boundaries = 0;
	RANGE	rargs = {0};

	while ((c = getopt(ac, av, "a;A;bBcC|d;efhHIl|m|nNpr|R|suvw")) != -1) {
		switch (c) {
		    case 'A':
			flags |= GET_ALIGN;
			/*FALLTHROUGH*/
		    case 'a':
			flags = annotate_args(flags, optarg);
			if (flags == -1) goto usage;
			break;
		    case 'b': kind |= DF_GNUb; break;		/* doc 2.0 */
		    case 'B': kind |= DF_GNUB; break;		/* doc 2.0 */
		    case 'c': kind |= DF_CONTEXT; break;	/* doc 2.0 */
		    case 'C': getMsg("diffs_C", 0, 0, stdout); exit(0);
		    case 'e': empty = 1; break;			/* don't doc */
		    case 'f': force = 1; break;
		    case 'h': flags &= ~DIFF_HEADER; break;	/* doc 2.0 */
		    case 'H':
			flags |= DIFF_COMMENTS;
			break;
		    case 'I': kind |= DF_IFDEF; break;		/* internal */
		    case 'l': boundaries = optarg; break;	/* doc 2.0 */
		    case 'm':					/* internal */
			kind |= DF_IFDEF;
			mdiff = 1;
			if (optarg && (*optarg == 'r')) flags |= GET_LINENAME;
			break;
		    case 'n': kind |= DF_RCS; break;		/* doc 2.0 */
		    case 'N': kind |= DF_GNUN; break;
		    case 'p': kind |= DF_GNUp; break;		/* doc 2.0 */
		    case 'R': unless (Rev = optarg) Rev = "-"; break;
		    case 's':					/* doc 2.0 */
			kind &= ~DF_DIFF;
			kind |= DF_SDIFF;
			break;
		    case 'u': kind |= DF_UNIFIED; break;	/* doc 2.0 */
		    case 'v': verbose = 1; break;		/* doc 2.0 */
		    case 'w': kind |= DF_GNUw; break;		/* doc 2.0 */
		    case 'd':
			if (range_addArg(&rargs, optarg, 1)) goto usage;
			break;
		    case 'r':
			if (range_addArg(&rargs, optarg, 0)) goto usage;
			break;
		    default:
usage:			system("bk help -s diffs");
			return (1);
		}
	}

	if ((rargs.rstart && (boundaries || Rev)) || (boundaries && Rev)) {
		fprintf(stderr, "%s: -C/-R must be alone\n", av[0]);
		return (1);
	}

	/*
	 * If we specified both revisions then we don't need the gfile.
	 * If we specifed one rev, then the gfile is also optional, we'll
	 * do the parent against that rev if no gfile.
	 * If we specified no revs then there must be a gfile.
	 */
	if ((flags & GET_PREFIX) &&
	    !Rev && (!rargs.rstop || boundaries) && !streq("-", av[ac-1])) {
		fprintf(stderr,
		    "%s: must have both revisions with -A|U|M|O\n", av[0]);
		return (1);
	}

	if (mdiff) {
		char	*mav[20];
		int	i, fd;

		mav[i=0] = "bk";
		mav[++i] = "mdiff";
		if (flags & GET_PREFIX) {
			flags |= GET_ALIGN;
			mav[++i] = "-A";
		} else {
			assert(!(flags & GET_ALIGN));
		}
		if (flags & GET_LINENAME) mav[++i] = "-r";
		mav[++i] = 0;
		if ((pid = spawnvpio(&fd, 0, 0, mav)) == -1) {
			perror("mdiff");
			exit(1);
		}
		dup2(fd, 1); close(fd);
	}
	name = sfileFirst("diffs", &av[optind], 0);
	while (name) {
		int	ex = 0;
		sccs	*s = 0;
		delta	*d;
		char	*r1 = 0, *r2 = 0;

		/*
		 * Unless we are given endpoints, don't diff.
		 * This is a big performance win.
		 * 2005-06: Endpoints meaning extended for diffs -N.
		 */
		unless (force || rargs.rstart || boundaries || Rev || sfileRev()) {
			char	*gfile = sccs2name(name);

			unless (writable(gfile) ||
			    ((kind&DF_GNUN) && !exists(name) && exists(gfile))){
				free(gfile);
				goto next;
			}
			free(gfile);
		}
		s = sccs_init(name, flags);
		unless (s && HASGRAPH(s)) {
			if (nulldiff(name, kind, flags) == 2) goto out;
			goto next;
		}
		if (boundaries) {
			unless (d = sccs_findrev(s, boundaries)) {
				fprintf(stderr,
				    "No delta %s in %s\n",
				    boundaries, s->gfile);
				goto next;
			}
			range_cset(s, d);
		} else if (Rev) {
			/* r1 == r2  means diff against the parent(s)(s)  */
			/* XXX TODO: probably needs to support -R+	  */
			if (streq(Rev, "-")) {
				unless (r1 = r2 = sfileRev()) {
					fprintf(stderr,
					    "diffs: -R- needs file|rev.\n");
					goto next;
				}
			} else {
				r1 = r2 = Rev;
			}
		} else {
			int	restore = 0;

			/*
			 * XXX - if there are other commands which want the
			 * edited file as an arg, we should make this code
			 * be a function.  export/rset/gnupatch use it.
			 */
			// XXX
			if (rargs.rstart && streq(rargs.rstart, ".")) {
				restore = 1;
				if (HAS_GFILE(s) && WRITABLE(s)) {
					rargs.rstart = 0;
				} else {
					rargs.rstart = "+";
				}
			}
			if (range_process("diffs", s, RANGE_ENDPOINTS,&rargs)) {
				unless (empty) goto next;
				s->rstart = s->tree;
			}
			if (restore) rargs.rstart = ".";
		}
		if (s->rstart) {
			unless (r1 = s->rstart->rev) goto next;
			if ((rargs.rstop) && (s->rstart == s->rstop)) goto next;
			if (s->rstop) r2 = s->rstop->rev;
		}
#if	0
		/*
		 * The ONLY time keyword expansion should be enabled
		 * is for 'bk diffs -r+ file' where there is an unlocked
		 * gfile.  This is where the user wants to verify that
		 * the gfile actually matches the TOT.  All other times
		 * we are comparing committed versions to each other or an
		 * editted gfile.
		 * XXX  The case above is currently broken, so I just
		 * disabled keywork expansion entirely.
		 */
		if (HAS_GFILE(s) && !WRITABLE(s) && (things <= 1)) {
			ex = GET_EXPAND;
		}
#endif

		/*
		 * Optimize out the case where we we are readonly and diffing
		 * TOT.
		 * EDITED() doesn't work because they could have chmod +w
		 * the file.
		 *
		 * XXX - I'm not sure this works with -C but we'll fix it in
		 * the 3.1 tree.
		 */
		if (!r1 && (!HAS_GFILE(s) || (!force && !WRITABLE(s)))) {
			goto next;
		}

		/*
		 * Optimize out the case where we have a locked file with
		 * no changes at TOT.
		 * EDITED() doesn't work because they could have chmod +w
		 * the file.
		 */
		if (!r1 && WRITABLE(s) && HAS_PFILE(s) && !MONOTONIC(s)) {
			unless (sccs_hasDiffs(s, flags|ex, 1)) goto next;
			if (BAM(s)) {
				if (flags & DIFF_HEADER) {
					printf("===== %s %s vs edited =====\n",
					    s->gfile, sccs_top(s)->rev);
				}
				printf("Binary file %s differs\n", s->gfile);
				goto next;
			}
		}

		/*
		 * Errors come back as -1/-2/-3/0
		 * -2/-3 means it couldn't find the rev; ignore.
		 *
		 * XXX - need to catch a request for annotations w/o 2 revs.
		 */
		rc = sccs_diffs(s, r1, r2, ex|flags, kind, stdout);
		switch (rc) {
		    case -1:
			fprintf(stderr,
			    "diffs of %s failed.\n", s->gfile);
			break;
		    case -2:
		    case -3:
			break;
		    case 0:	
			if (verbose) fprintf(stderr, "%s\n", s->gfile);
			break;
		    default:
			fprintf(stderr,
			    "diffs of %s failed.\n", s->gfile);
		}
next:		if (s) {
			sccs_free(s);
			s = 0;
		}
		name = sfileNext();
	}
out:	if (sfileDone()) errors |= 4;
	if (mdiff) {
		u32	status;

		fflush(stdout);		/* just in case */
		close(1);
		waitpid(pid, &status, 0);
		errors |= WIFEXITED(status) ? WEXITSTATUS(status) : 1;
	}
	return (errors);
}

private int
nulldiff(char *name, u32 kind, u32 flags)
{
	int	ret = 0;
	char	*here, *file, *p;

	name = sccs2name(name);
	unless (kind & DF_GNUN) {
		printf("New file: %s\n", name);
		goto out;
	}
	unless (ascii(name)) {
		fprintf(stderr, "Warning: skipping binary '%s'\n", name);
		goto out;
	}
	if (flags & DIFF_HEADER) {
		printf("===== New file: %s =====\n", name);
		/* diff() uses write, not stdio */
		if (fflush(stdout)) {
			ret = 2;
			goto out;
		}
	}
	
	/*
	 * Wayne liked this better but I had to work around a nasty bug in
	 * that the chdir() below changes the return from proj_cwd().
	 * Hence the strdup.  I think we want a pushd/popd sort of interface.
	 */
	here = strdup(proj_cwd());
	p = strrchr(here, '/');
	assert(p);
	file = aprintf("%s/%s", p+1, name);
	chdir("..");
	ret = diff(DEVNULL_RD, file, kind, "-");
	chdir(here);
	free(here);
	free(file);
out:	free(name);
	return (ret);
}
