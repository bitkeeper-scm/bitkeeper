/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

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
 */

int
diffs_main(int ac, char **av)
{
	sccs	*s;
	int	flags = DIFF_HEADER|SILENT, verbose = 0, rc, c;
	int	errors = 0;
	char	kind;
	char	*name;
	project	*proj = 0;
	char	*Rev = 0, *cset = 0;
	char	*opts, optbuf[20];
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
	opts = optbuf;
	*opts++ = '-';
	*opts = 0;
	while ((c = getopt(ac, av, "bBcC|d;DfhMnpr|R|suUvw")) != -1) {
		switch (c) {
		    case 'b': /* fall through */		/* doc 2.0 */
		    case 'B': *opts++ = c; *opts = 0; break;	/* doc 2.0 */
		    case 'h': flags &= ~DIFF_HEADER; break;	/* doc 2.0 */
		    case 'c': kind = DF_CONTEXT; break;		/* doc 2.0 */
		    case 'C': cset = optarg; break;		/* doc 2.0 */
		    case 'D': flags |= GET_PREFIXDATE; break;	/* doc 2.0 */
		    case 'f': flags |= GET_MODNAME; break;	/* doc 2.0 */
		    case 'M': flags |= GET_REVNUMS; break;	/* doc 2.0 */
		    case 'p': kind = DF_PDIFF; break;		/* doc 2.0 */
		    case 'n': kind = DF_RCS; break;		/* doc 2.0 */
		    case 'R': Rev = optarg; break;		/* doc 2.0 */
		    case 's': kind = DF_SDIFF; break;		/* doc 2.0 */
		    case 'u': kind = DF_UNIFIED; break;		/* doc 2.0 */
		    case 'U': flags |= GET_USER; break;		/* doc 2.0 */
		    case 'v': verbose = 1; break;		/* doc 2.0 */
		    case 'w': *opts++ = c; *opts = 0; break;	/* doc 2.0 */
		    RANGE_OPTS('d', 'r');			/* doc 2.0 */
		    default:
usage:			system("bk help -s diffs");
			return (1);
		}
	}
	if (opts[-1] == '-') opts = 0; else opts = optbuf;

	if ((things && (cset || Rev)) || (cset && Rev)) {
		fprintf(stderr, "%s: -C/-R must be alone\n", av[0]);
		return (1);
	}

	/*
	 * If we specified both revisions then we don't need the gfile.
	 * If we specifed one rev, then the gfile is also optional, we'll
	 * do the parent against that rev if no gfile.
	 * If we specified no revs then there must be a gfile.
	 */
	if (cset) things = 2;
	if ((flags & GET_PREFIX) && (things != 2) && !streq("-", av[ac-1])) {
		fprintf(stderr,
		    "%s: must have both revisions with -d|u|m|s\n", av[0]);
		return (1);
	}

	/* XXX - if we are doing cset | diffs then we don't need the GFILE.
	 * Currently turned off in sfiles.
	 */
	if (things || cset || Rev) {
		name = sfileFirst("diffs", &av[optind], 0);
	} else {
		name = sfileFirst("diffs", &av[optind], SF_GFILE);
	}
	while (name) {
		int	ex = 0;
		char	*r1 = 0, *r2 = 0;

		s = sccs_init(name, INIT_SAVEPROJ|flags, proj);
		unless (s && HASGRAPH(s)) {
			errors |= 2;
			goto next;
		}
		unless (proj) proj = s->proj;
		if (cset) {
			if (cset_boundries(s, cset)) goto next;
		} else if (Rev) {
			/* r1 == r2  means diff against the parent(s)(s)  */
			/* XXX TODO: probably needs to support -R+	  */
			r1 = r2 = Rev;
		} else {
			RANGE("diffs", s, 0, (flags & SILENT) == 0);
		}
		if (things) {
			unless (s->rstart && (r1 = s->rstart->rev)) goto next;
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
		/* XXX - probably busted in split root */
		if (HAS_GFILE(s) && !IS_WRITABLE(s) && (things <= 1)) {
			ex = GET_EXPAND;
		}

		/*
		 * Optimize out the case where we we are readonly and diffing
		 * TOT.
		 * IS_EDITED() doesn't work because they could have chmod +w
		 * the file.
		 */
		if (!things && !Rev && !IS_WRITABLE(s)) goto next;

		/*
		 * Optimize out the case where we have a locked file with
		 * no changes at TOT.
		 * IS_EDITED() doesn't work because they could have chmod +w
		 * the file.
		 */
		if (!things && !Rev && IS_WRITABLE(s) && HAS_PFILE(s) &&
		    !sccs_hasDiffs(s, GET_DIFFTOT|flags|ex, 1)) {
			goto next;
		}
		
		/*
		 * Errors come back as -1/-2/-3/0
		 * -2/-3 means it couldn't find the rev; ignore.
		 */
		rc = sccs_diffs(s, r1, r2, ex|flags, kind, opts, stdout);
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
next:		if (s) sccs_free(s);
		name = sfileNext();
	}
	if (proj) proj_free(proj);
	sfileDone();
	return (errors);
}

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
