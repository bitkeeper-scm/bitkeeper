/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

int
get_main(int ac, char **av)
{
	sccs	*s;
	int	iflags = 0, flags = GET_EXPAND, c, errors = 0;
	char	*iLst = 0, *xLst = 0, *name, *rev = 0, *cdate = 0, *Gname = 0;
	char	*prog;
	char	*mRev = 0, *Rev = 0;
	delta	*d;
	int	gdir = 0;
	int	getdiff = 0;
	int	sf_flags = 0;
	int	dohash = 0;
	int	branch_ok = 0;
	int	caseFoldingFS = 1;
	int	skip_bin = 0;
	int	pnames = getenv("BK_PRINT_EACH_NAME") != 0;
	MDBM	*realNameCache = 0;
	char	*out = "-";
	char	realname[MAXPATH];

	debug_main(av);
	prog = strrchr(av[0], '/');
	if (prog) prog++;
	else prog = av[0];
	if (streq(prog, "co")) {
		if (!isdir("SCCS") && isdir("RCS")) {
			rcs("co", ac, av);
			/* NOTREACHED */
		}
	} else if (streq(prog, "edit")) {
		flags |= GET_EDIT;
	} else if (streq(prog, "_get")) {
		branch_ok = 1;
	}

	if (ac == 2 && streq("--help", av[1])) {
		sprintf(realname, "bk help %s", prog);
		system(realname);
		return (1);
	}
	if (streq(av[0], "edit")) flags |= GET_EDIT;
	if (streq("GET", user_preference("checkout"))) flags |= GET_NOREGET;
	while ((c =
	    getopt(ac, av, "A;a;Bc;CDeFgG:hi;klM|pPqr;RSstTx;")) != -1) {
		switch (c) {
		    case 'A':
			flags |= GET_ALIGN;
			/*FALLTHROUGH*/
		    case 'a':
			flags = annotate_args(flags, optarg);
			if (flags == -1) goto usage;
			break;
		    case 'B': skip_bin = 1; break;
		    case 'c': cdate = optarg; break;		/* doc 2.0 */
		    case 'C': getMsg("get_C", 0, 0, stdout); exit(0);
		    case 'D': getdiff++; break;			/* doc 2.0 */
		    case 'l':					/* doc 2.0 co */
		    case 'e': flags |= GET_EDIT; break;		/* doc 2.0 */
		    case 'F': iflags |= INIT_NOCKSUM; break;	/* doc 2.0 */
		    case 'g': flags |= GET_SKIPGET; break;	/* doc 2.0 */
		    case 'G': Gname = optarg; break;		/* doc 2.0 */
		    case 'h': dohash = 1; break;		/* doc 2.0 */
		    case 'i': iLst = optarg; break;		/* doc 2.0 */
		    case 'k': flags &= ~GET_EXPAND; break;	/* doc 2.0 */
		    case 'M': mRev = optarg; break;		/* doc 2.0 */
		    case 'p': flags |= PRINT; break;		/* doc 2.0 */
		    case 'P': flags |= PRINT|GET_FORCE; break;	/* doc 2.0 */
		    case 'q': flags |= SILENT; break;		/* doc 2.0 */
		    case 'r': Rev = optarg; break;		/* doc 2.0 */
		    case 'R': sf_flags |= SF_HASREVS; break;	/* doc 2.0 */
		    case 's': flags |= SILENT; break;		/* undoc */
		    case 'S': flags |= GET_NOREGET; break;	/* doc 2.0 */
		    case 't': break;		/* compat, noop, undoc 2.0 */
		    case 'T': flags |= GET_DTIME; break;	/* doc 2.0 */
		    case 'x': xLst = optarg; break;		/* doc 2.0 */

		    default:
usage:			sprintf(realname, "bk help -s %s", prog);
			system(realname);
			return (1);
		}
	}
	if (flags & GET_PREFIX) {
		if (flags & GET_EDIT) {
			fprintf(stderr, "%s: can't use -e with -dNum\n",
				av[0]);
			return(1);
		}
	}
	sf_flags |= SF_NOCSET;	/* prevents auto-expansion */
	if (flags & GET_EDIT) {
		flags &= ~GET_EXPAND;
	}
	name = sfileFirst("get", &av[optind], sf_flags);
	gdir = Gname && isdir(Gname);
	if (((Gname && !gdir) || iLst || xLst) && sfileNext()) {
onefile:	fprintf(stderr,
		    "%s: only one file name with -G/i/x.\n", av[0]);
		goto usage;
	}
	if (av[optind] && av[optind+1] && strneq(av[optind+1], "-G", 2)) {
		Gname = &av[optind+1][2];
	}
	if (Gname && (flags & GET_EDIT)) {
		fprintf(stderr, "%s: can't use -G and -e/-l together.\n",av[0]);
		goto usage;
	}
	if (Gname && (flags & PRINT)) {
		fprintf(stderr, "%s: can't use -G and -p together,\n", av[0]);
		goto usage;
	}
	switch (getdiff) {
	    case 0: break;
	    case 1: flags |= GET_DIFFS; break;
	    case 2: flags |= GET_BKDIFFS; break;
	    case 3: flags |= GET_HASHDIFFS; break;
	    default:
		fprintf(stderr, "%s: invalid D flag value %d\n", av[0],getdiff);
		return(1);
	}
	if (getdiff && (flags & GET_PREFIX)) {
		fprintf(stderr, "%s: -D and prefixes not supported\n", av[0]);
		return(1);
	}

	if (proj_root(0)) {
		caseFoldingFS = isCaseFoldingFS(proj_root(0));
		realNameCache = mdbm_open(NULL,0, 0, GOOD_PSIZE);
		assert(realNameCache);
	}

	for (; name; name = sfileNext()) {
		d = 0;

		if (caseFoldingFS) {
			/*
			 * For win32 FS and Samba.
			 * We care about the realname because we want
			 * the gfile to have the same case as the s.file
			 * Otherwise other bk command such as bk delta
			 * may be confused.
			 */
			getRealName(name, realNameCache, realname);
			name = realname;
		}
		unless (s = sccs_init(name, iflags)) continue;
		if (Gname) {
			if (gdir) {
				char	buf[1024];
				int	ret;

				sprintf(buf, "%s/%s", Gname, basenm(s->gfile));
				free(s->gfile);
				s->gfile = strdup(buf);
				ret = check_gfile(s, 0);
				assert(ret == 0);
			} else {
				free(s->gfile);
				s->gfile = strdup(Gname);
			}
		}
		unless (HASGRAPH(s)) {
			if (!(s->state & S_SFILE)) {
				verbose((stderr, "%s: %s doesn't exist.\n",
				    prog, s->sfile));
			} else {
				perror(s->sfile);
			}
			sccs_free(s);
			errors = 1;
			continue;
		}
		if (skip_bin && (s->encoding & E_BINARY)) {
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
		} else if (Rev) {
			rev = Rev;
		} else {
			rev = sfileRev();
		}
		if (dohash) {
			if (HASH(s)) {
				s->xflags &= ~X_HASH;
			} else {
				s->xflags |= X_HASH;
			}
		}
		if (BITKEEPER(s) && (flags & GET_EDIT) && rev && !branch_ok) {
			fprintf(stderr, "Cannot create branch\n");
			errors = 1;
			sccs_free(s);
			continue;
		}
		if (BITKEEPER(s) && rev && !branch_ok && !streq(rev, "+")) {
			unless ((flags & PRINT) || Gname) {
				fprintf(stderr,
				    "%s: can't specify revisions without -p\n",
				    av[0]);
				goto usage;
			}
		}
		if (pnames) {
			printf("|FILE|%s|CRC|%u\n", s->gfile, crc(s->gfile));
		}
		if ((flags & (GET_DIFFS|GET_BKDIFFS|GET_HASHDIFFS))
		    ? sccs_getdiffs(s, rev, flags, out)
		    : sccs_get(s, rev, mRev, iLst, xLst, flags, out)) {
			if (s->io_error) return (1);
			unless (BEEN_WARNED(s)) {
				verbose((stderr,
				    "%s of %s failed, skipping it.\n",
				    prog, name));
			}
			errors = 1;
		}
		sccs_free(s);
		/* sfileNext() will try and check out -G<whatever> */
		if (Gname && !gdir) {
			while ((name = sfileNext()) &&
			    strneq("SCCS/s.-G", name, 9));
			if (name) goto onefile;
			break;
		}
	}
	sfileDone();
	if (realNameCache) mdbm_close(realNameCache);
	return (errors);
}

/*
 * Parse file annotation command line arguemnts.
 * It would be nice if the order of the characters could control
 * the order that the fields appear.  But that would require
 * a different way to pass arguments to sccs_get().
 */
int
annotate_args(int flags, char *args)
{
	while (*args) {
		switch (*args) {
		    case 'b': flags |= GET_MODNAME; break;
		    case 'd': flags |= GET_PREFIXDATE; break;
		    case 'p': flags |= GET_RELPATH; break;
		    case 'm': /* pseudo backwards compat */
		    case 'r': /* better name */
			flags |= GET_REVNUMS; break;
		    case 'l': /* undoc-ed - alias */
		    case 'n': flags |= GET_LINENUM; break;
		    case 'O': flags |= GET_LINENAME; break;
		    case 'u': flags |= GET_USER; break;
		    default:
			flags = -1;
		}
		++args;
	}
	return (flags);
}
