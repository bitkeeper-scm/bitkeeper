/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"

#define	bp_fetchkeys	fetchkeys	

private int	get_rollback(sccs *s, char *rev,
		    char **iLst, char **xLst, char *prog);
private int	bam(char *me, int, char **files, char **keys, u64 n, int ac, char **av);

int
get_main(int ac, char **av)
{
	sccs	*s;
	int	iflags = 0, flags = GET_EXPAND|GET_NOREMOTE, c, errors = 0;
	char	*iLst = 0, *xLst = 0, *name, *rev = 0, *Gname = 0;
	char	*prog;
	char	*mRev = 0, *Rev = 0;
	delta	*d;
	int	recursed = 0;
	int	gdir = 0;
	int	getdiff = 0;
	int	sf_flags = 0;
	int	dohash = 0;
	int	branch_ok = 0;
	int	caseFoldingFS = 1;
	int	closetips = 0;
	int	skip_bin = 0;
	int	pnames = getenv("BK_PRINT_EACH_NAME") != 0;
	int	ac_optend;
	MDBM	*realNameCache = 0;
	char	*out = "-";
	char	**bp_files = 0;
	char	**bp_keys = 0;
	u64	bp_todo = 0;
	char	realname[MAXPATH];

	if (prog = strrchr(av[0], '/')) {
		prog++;
	} else {
		prog = av[0];
	}
	if (streq(prog, "edit")) {
		flags |= GET_EDIT;
	} else if (streq(prog, "_get")) {
		branch_ok = 1;
	}

	if (streq(av[0], "edit")) flags |= GET_EDIT;
	while ((c =
	    getopt(ac, av, "A;a;BCDeFgG:hi;klM|pPqr;RSstTx;")) != -1) {
		switch (c) {
		    case 'A':
			flags |= GET_ALIGN;
			/*FALLTHROUGH*/
		    case 'a':
			flags = annotate_args(flags, optarg);
			if (flags == -1) goto usage;
			break;
		    case 'B': skip_bin = 1; break;
		    case 'C': getMsg("get_C", 0, 0, stdout); return (1);
		    case 'D': getdiff++; break;			/* doc 2.0 */
		    case 'l':					/* doc 2.0 co */
		    case 'e': flags |= GET_EDIT; break;		/* doc 2.0 */
		    case 'F':
			iflags |= INIT_NOCKSUM;
			putenv("_BK_FASTGET=YES");
			break;
		    case 'g': flags |= GET_SKIPGET; break;	/* doc 2.0 */
		    case 'G': Gname = optarg; break;		/* doc 2.0 */
		    case 'h': dohash = 1; break;		/* doc 2.0 */
		    case 'i': iLst = optarg; break;		/* doc 2.0 */
		    case 'k': flags &= ~GET_EXPAND; break;	/* doc 2.0 */
		    case 'M': mRev = optarg; closetips = !mRev; break;
		    case 'p': flags |= PRINT; break;		/* doc 2.0 */
		    case 'P': flags |= PRINT|GET_FORCE; break;	/* doc 2.0 */
		    case 'q': flags |= SILENT; break;		/* doc 2.0 */
		    case 'r': Rev = optarg; break;		/* doc 2.0 */
		    case 'R': recursed = 1; break;
		    case 's': flags |= SILENT; break;		/* undoc */
		    case 'S': flags |= GET_NOREGET; break;	/* doc 2.0 */
		    case 't': break;		/* compat, noop, undoc 2.0 */
		    case 'T': flags |= GET_DTIME; break;	/* doc 2.0 */
		    case 'x': xLst = optarg; break;		/* doc 2.0 */

		    default:
usage:			sys("bk", "help", "-s", prog, SYS);
			return (1);
		}
	}
	ac_optend = optind;
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
	if (rev && closetips) {
		fprintf(stderr,
		    "%s: -M can not be combined with rev.\n", av[0]);
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
		caseFoldingFS = proj_isCaseFoldingFS(0);
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
			unless (HAS_SFILE(s)) {
				verbose((stderr, "%s: %s doesn't exist.\n",
				    prog, s->sfile));
			} else {
				perror(s->sfile);
			}
err:			sccs_free(s);
			errors = 1;
			continue;
		}
		if (skip_bin && BINARY(s)) {
			sccs_free(s);
			continue;
		}
		if (Rev) {
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
		if (BITKEEPER(s) &&
		    (iLst || xLst) && !branch_ok && !(flags & GET_EDIT)) {
			unless ((flags & PRINT) || Gname) {
				fprintf(stderr,
				    "%s: can't specify include/exclude "
				    "without -p, -e or -l\n", av[0]);
				goto err;
			}
		}
		if (BITKEEPER(s) && rev && !branch_ok
		    && !(flags & GET_EDIT) && !streq(rev, "+")) {
			unless ((flags & PRINT) || Gname) {
				fprintf(stderr,
				    "%s: can't specify revisions without -p\n",
				    av[0]);
				goto err;
			}
		}
		/* -M with no args means to close the open tip */
		if (closetips) {
			delta	*a, *b;

			if (sccs_findtips(s, &a, &b)) {
				if (a->r[2]) {
					mRev = a->rev;
				} else if (b->r[2]) {
					mRev = b->rev;
				} else {
					fprintf(stderr, "%s: ERROR -M with"
					    " neither tip on branch?\n",
					    av[0]);
					goto err;
				}
			} else {
				fprintf(stderr, "%s: No branches to close"
				    " in %s, skipping...\n",
				    av[0], s->gfile);
				goto err;
			}
		}
		if (BITKEEPER(s) && (flags & GET_EDIT) && rev && !branch_ok) {
			/* recalc iLst and xLst to be relative to tip */
			if (get_rollback(s, rev, &iLst, &xLst, av[0])) {
				goto next;
			}
			rev = 0;	/* Use tip below */
		}
		if (pnames) {
			printf("|FILE|%s|CRC|%u\n", s->gfile, crc(s->gfile));
		}
		if ((flags & (GET_DIFFS|GET_BKDIFFS|GET_HASHDIFFS))
		    ? sccs_getdiffs(s, rev, flags, out)
		    : sccs_get(s, rev, mRev, iLst, xLst, flags, out)) {
			if (s->cachemiss && !recursed) {
				d = bp_fdelta(s, sccs_findrev(s, rev));
				bp_files = addLine(bp_files, strdup(name));
				bp_keys = addLine(bp_keys,
				    sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC));
				bp_todo += d->added;	// XXX - not all of it
				goto next;
			}
			if (s->io_error) return (1);
			unless (BEEN_WARNED(s)) {
				verbose((stderr,
				    "%s of %s failed, skipping it.\n",
				    prog, name));
			}
			errors = 1;
		}
next:		sccs_free(s);
		/* sfileNext() will try and check out -G<whatever> */
		if (Gname && !gdir) {
			while ((name = sfileNext()) &&
			    strneq("SCCS/s.-G", name, 9));
			if (name) goto onefile;
			break;
		}
	}
	if (sfileDone()) errors = 1;
	if (realNameCache) mdbm_close(realNameCache);
	if (bp_files && !recursed) {
		/* If we already had an error don't let this turn that
		 * into a non-error.
		 */
		if (c = bam(prog, flags & SILENT,
		    bp_files, bp_keys, bp_todo, ac_optend, av)) {
		    	errors = c;
	    	}
	}
	freeLines(bp_files, free);
	freeLines(bp_keys, free);
	return (errors);
}

extern int bp_fetchkeys(char *me, int quiet, char **keys, u64 todo);

private int
bam(char *me, int q, char **files, char **keys, u64 todo, int ac, char **av)
{
	char	*nav[100];
	FILE	*f;
	int	i;

	unless (files) return (0);
	if (bp_fetchkeys(me, q, keys, todo)) {
		fprintf(stderr, "%s: failed to fetch BAM data\n", me);
		return (1);
	}
	assert(ac < 90);
	nav[0] = "bk";
	for (i = 0; i < ac; i++) {
		nav[i+1] = av[i];
	}
	nav[++i] = "-R";
	nav[++i] = "-";
	nav[++i] = 0;
	f = popenvp(nav, "w");
	EACH(files) fprintf(f, "%s\n", files[i]);
	if (pclose(f)) return (1);
	return (0);
}

int
bp_fetchkeys(char *me, int quiet, char **keys, u64 todo)
{
	int	i;
	FILE	*f;
	char	*server = bp_serverURL();
	char	buf[MAXPATH];

	unless (server) {
		fprintf(stderr, "%s: no server for BAM data.\n", me);
		return (1);
	}
	unless (quiet) {
		fprintf(stderr,
		    "Fetching %u BAM files from %s...\n",
		    nLines(keys), server);
	}
	/* no recursion, I'm remoted to the server already */
	sprintf(buf,
	    "bk -q@'%s' -zo0 -Lr -Bstdin sfio -qoBl - |"
	    "bk -R sfio -%sriBb%s -", server, quiet ? "q" : "", psize(todo));
	f = popen(buf, "w");
	EACH(keys) fprintf(f, "%s\n", keys[i]);
	i = pclose(f);
	return (i != 0);
}

private int
get_rollback(sccs *s, char *rev, char **iLst, char **xLst, char *me)
{
	char	*inc = *iLst, *exc = *xLst;
	ser_t   *map;
	delta	*d;

	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "%s: cannot find %s in %s\n", me,rev, s->gfile);
		return (1);
	}
	unless (map = sccs_set(s, d, inc, exc)) return (1);
	d = sccs_top(s);
	if (sccs_graph(s, d, map, iLst, xLst)) {
		fprintf(stderr, "%s: cannot compute graph from set\n", me);
		return (1);
	}
	return (0);
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
		    //case '5': flags |= GET_MD5REV; break;
		    case 'b': flags |= GET_MODNAME; break;
		    case 'd': flags |= GET_PREFIXDATE; break;
		    case 'p': flags |= GET_RELPATH; break;
		    case 'm': /* pseudo backwards compat */
		    case 'r': /* better name */
			flags |= GET_REVNUMS; break;
		    case 'l': /* undoc-ed - alias */
		    case 'n': flags |= GET_LINENUM; break;
		    case 'O': flags |= GET_LINENAME; break;
		    case 'S': flags |= GET_SERIAL; break;
		    case 'u': flags |= GET_USER; break;
		    default:
			return (-1);
		}
		++args;
	}
	return (flags);
}
