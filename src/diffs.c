/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

private	int	cset_boundries(sccs *s, char *rev);

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
	int	verbose = 0, rc, c;
	int	empty = 0, errors = 0, mdiff = 0;
	u32	flags = DIFF_HEADER|SILENT, kind;
	pid_t	pid = 0; /* lint */
	char	*name;
	char	*Rev = 0, *boundaries = 0;
	RANGE_DECL;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help diffs");
		return (1);
	}

	if (name = strrchr(av[0], '/')) {
		kind = streq(++name, "sdiffs") ? DF_SDIFF : DF_DIFF;
	} else {
		kind = streq(av[0], "sdiffs") ? DF_SDIFF : DF_DIFF;
	}
	while ((c = getopt(ac, av, "AbBcC|d;DefhHIl|Mm|npr|R|suUvw")) != -1) {
		switch (c) {
		    case 'A': flags |= GET_ALIGN; break;
		    case 'b': kind |= DF_GNUb; break;		/* doc 2.0 */
		    case 'B': kind |= DF_GNUB; break;		/* doc 2.0 */
		    case 'c': kind |= DF_CONTEXT; break;	/* doc 2.0 */
		    case 'C': getMsg("diffs-C", 0, 0, 0, stdout); exit(0);
		    case 'D': flags |= GET_PREFIXDATE; break;	/* doc 2.0 */
		    case 'e': empty = 1; break;
		    case 'f': flags |= GET_MODNAME; break;	/* doc 2.0 */
		    case 'h': flags &= ~DIFF_HEADER; break;	/* doc 2.0 */
		    case 'H':
			flags |= DIFF_COMMENTS;
			putenv("BK_YEAR4=YES");  /* rm when YEAR4 is default */
			break;
		    case 'I': kind |= DF_IFDEF; break;
		    case 'l': boundaries = optarg; break;	/* doc 2.0 */
		    case 'M': flags |= GET_REVNUMS; break;	/* doc 2.0 */
		    case 'm':
			kind |= DF_IFDEF;
			mdiff = 1;
			if (optarg && (*optarg == 'r')) flags |= GET_LINENAME;
			break;
		    case 'n': kind |= DF_RCS; break;		/* doc 2.0 */
		    case 'p': kind |= DF_GNUp; break;		/* doc 2.0 */
		    case 'R': Rev = optarg; break;		/* doc 2.0 */
		    case 's':					/* doc 2.0 */
			kind &= ~DF_DIFF;
			kind |= DF_SDIFF;
			break;
		    case 'u': kind |= DF_UNIFIED; break;	/* doc 2.0 */
		    case 'U': flags |= GET_USER; break;		/* doc 2.0 */
		    case 'v': verbose = 1; break;		/* doc 2.0 */
		    case 'w': kind |= DF_GNUw; break;		/* doc 2.0 */
		    RANGE_OPTS('d', 'r');			/* doc 2.0 */
		    default:
usage:			system("bk help -s diffs");
			return (1);
		}
	}

	if ((things && (boundaries || Rev)) || (boundaries && Rev)) {
		fprintf(stderr, "%s: -C/-R must be alone\n", av[0]);
		return (1);
	}

	/*
	 * If we specified both revisions then we don't need the gfile.
	 * If we specifed one rev, then the gfile is also optional, we'll
	 * do the parent against that rev if no gfile.
	 * If we specified no revs then there must be a gfile.
	 */
	if (boundaries) things = 2;
	if ((flags & GET_PREFIX) &&
	    !Rev && (things != 2) && !streq("-", av[ac-1])) {
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
		if ((pid = spawnvp_wPipe(mav, &fd, BIG_PIPE)) == -1) {
			perror("mdiff");
			exit(1);
		}
		dup2(fd, 1); close(fd);
	}
	name = sfileFirst("diffs", &av[optind], 0);
	while (name) {
		int	ex = 0;
		sccs	*s = 0;
		char	*r1 = 0, *r2 = 0;
		int	save = things;

		/* unless we are given endpoints, don't diff */
		unless (things || boundaries || Rev || sfileRev()) {
			char	*gfile = sccs2name(name);

			unless (writable(gfile)) {
				free(gfile);
				goto next;
			}
			free(gfile);
		}
		s = sccs_init(name, flags);
		unless (s && HASGRAPH(s)) {
			errors |= 2;
			goto next;
		}
		if (boundaries) {
			if (cset_boundries(s, boundaries)) goto next;
		} else if (Rev) {
			/* r1 == r2  means diff against the parent(s)(s)  */
			/* XXX TODO: probably needs to support -R+	  */
			r1 = r2 = Rev;
		} else {
			int	restore = 0;

			/*
			 * XXX - if there are other commands which want the
			 * edited file as an arg, we should make this code
			 * be a function.  export/rset/gnupatch use it.
			 */
			if (r[0] && (things == 1) && streq(r[0], ".")) {
				restore = 1;
				if (HAS_GFILE(s) && IS_WRITABLE(s)) {
					things = 0;
					r[0] = 0;
				} else {
					r[0] = "+";
				}
			}
			if (rangeProcess("diffs", s, 0,
			    (flags & SILENT) == 0, 1, &things, rd, r, d)) {
				unless (empty) goto next;
				s->rstart = s->tree;
			}
			if (restore) r[0] = ".";
		}
		if (things) {
			unless (s->rstart && (r1 = s->rstart->rev)) goto next;
			if ((things == 2) && (s->rstart == s->rstop)) goto next;
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
		if (HAS_GFILE(s) && !IS_WRITABLE(s) && (things <= 1)) {
			ex = GET_EXPAND;
		}
#endif

		/*
		 * Optimize out the case where we we are readonly and diffing
		 * TOT.
		 * IS_EDITED() doesn't work because they could have chmod +w
		 * the file.
		 *
		 * XXX - I'm not sure this works with -C but we'll fix it in
		 * the 3.1 tree.
		 */
		if (!things && !Rev && !IS_WRITABLE(s)) goto next;

		/*
		 * Optimize out the case where we have a locked file with
		 * no changes at TOT.
		 * IS_EDITED() doesn't work because they could have chmod +w
		 * the file.
		 */
		if (!things && IS_WRITABLE(s) && HAS_PFILE(s) &&
		    !MONOTONIC(s) && !Rev &&
		    !sccs_hasDiffs(s, flags|ex, 1)) {
			goto next;
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
		things = save;
	}
	sfileDone();
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
cset_boundries(sccs *s, char *rev)
{
	delta	*d = sccs_getrev(s, rev, 0, 0);

	unless (d) {
		fprintf(stderr, "No delta %s in %s\n", rev, s->gfile);
		return (1);
	}
	d->flags |= D_RED;
	for (d = s->table; d; d = d->next) {
		if (d->flags & D_CSET) s->rstop = d;
		if (d->flags & D_RED) break;
	}
	unless (d) {
		fprintf(stderr, "No csets in %s?\n", s->gfile);
		return (1);
	}
	s->rstart = d;
	while (d && (d != s->tree)) {
		s->rstart = d;
		if ((d != s->rstop) && (d->flags & D_CSET)) {
			break;
		}
		d = d->parent;
	}
	/*
	 * If they picked a delta which is pending, use the last delta
	 * as the boundry.
	 */
	unless (s->rstop) {
		for (d = s->rstart;
		    d && d->kid && (d->kid->type == 'D'); d = d->kid);
		s->rstop = d;
	}
	return (0);
}
