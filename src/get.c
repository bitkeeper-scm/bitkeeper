/*
 * Copyright 1997-2016 BitMover, Inc
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
#include "progress.h"

private int	get_rollback(sccs *s, char *rev, char *mRev,
		    char **iLst, char **xLst, char *prog);
private	int	bam(char *me, int q, char **files, int ac, char **av);

int
get_main(int ac, char **av)
{
	sccs	*s;
	int	iflags = 0, flags = GET_EXPAND|GET_NOREMOTE, c, errors = 0;
	char	*iLst = 0, *xLst = 0, *name, *rev = 0;
	char	*gfile = 0;
	char	*prog;
	char	*mRev = 0, *Rev = 0;
	ser_t	d;
	int	recursed = 0;
	int	getdiff = 0;
	int	sf_flags = 0;
	int	dohash = 0;
	int	branch_ok = 0;
	int	caseFoldingFS = 1;
	int	closetips = 0;
	int	skip_bin = 0;
	int	checkout = 0;
	int	tickout = 0;
	int	skip_bam = 0;
	int	n = 0, nfiles = -1;
	int	pnames = getenv("BK_PRINT_EACH_NAME") != 0;
	int	ac_optend;
	int	rollback = 0;
	FILE	*fout = 0;
	MDBM	*realNameCache = 0;
	char	*out = 0;
	char	**bp_files = 0;
	char	**bp_keys = 0;
	project	*bp_proj = 0;
	u64	bp_todo = 0;
	ticker	*tick = 0;
	int	Gname = 0;	/* -G on command line */
	char	*gdir = 0;	/* if -Gdir, the directory name */
	char	Gout[MAXPATH];	/* current file being written by -G */
	char	realname[MAXPATH];
	longopt	lopt[] = {
		{ "skip-bam", 310 },
		{ 0, 0 }
	};

	if (prog = strrchr(av[0], '/')) {
		prog++;
	} else {
		prog = av[0];
	}
	if (streq(prog, "edit")) {
		flags |= GET_EDIT;
	} else if (streq(prog, "_get")) {
		branch_ok = 1;
	} else if (streq(prog, "checkout")) {
		checkout = 1;
	}

	while ((c =
	    getopt(ac, av, "A;a;BDeFgG:hi;klM|N;pPqr;RSstTUx;", lopt)) != -1) {
		if (checkout && (c != 310) && !strchr("NqRTU", c)) {
			fprintf(stderr, "checkout: no options allowed\n");
			exit(1);
		}
		switch (c) {
		    case 'A':
			flags |= GET_ALIGN;
			/*FALLTHROUGH*/
		    case 'a':
			flags = annotate_args(flags, optarg);
			if (flags == -1) usage();
			break;
		    case 'B': skip_bin = 1; break;
		    case 'D': getdiff++; break;			/* doc 2.0 */
		    case 'l':					/* doc 2.0 co */
		    case 'e': flags |= GET_EDIT; break;		/* doc 2.0 */
		    case 'F':
			iflags |= INIT_NOCKSUM;
			putenv("_BK_FASTGET=YES");
			break;
		    case 'g': flags |= GET_SKIPGET; break;	/* doc 2.0 */
		    case 'G':
			Gname = 1;
			strcpy(Gout, optarg);
			break;
		    case 'h': dohash = 1; break;		/* doc 2.0 */
		    case 'i': iLst = optarg; break;		/* doc 2.0 */
		    case 'k': flags &= ~GET_EXPAND; break;	/* doc 2.0 */
		    case 'M': mRev = optarg; closetips = !mRev; break;
		    case 'N': nfiles = atoi(optarg); break;
		    case 'p': flags |= PRINT; break;		/* doc 2.0 */
		    case 'P': flags |= PRINT|GET_FORCE; break;	/* doc 2.0 */
		    case 'q': flags |= SILENT; break;		/* doc 2.0 */
		    case 'r': Rev = optarg; break;		/* doc 2.0 */
		    case 'R': recursed = 1; break;
		    case 's': flags |= SILENT; break;		/* undoc */
		    case 'S': flags |= GET_NOREGET; break;	/* doc 2.0 */
		    case 't': break;		/* compat, noop, undoc 2.0 */
		    case 'T': flags |= GET_DTIME; break;	/* doc 2.0 */
		    case 'U': tickout = 1; break;
		    case 'x': xLst = optarg; break;		/* doc 2.0 */
		    case 310: skip_bam++; break;		// --skip-bam
		    default: bk_badArg(c, av);
		}
	}
	if (flags & PRINT) {
		if (flags & (GET_EDIT|GET_SKIPGET|GET_NOREGET|GET_DTIME)) {
			usage();
		}
		fout = stdout;
	}
	ac_optend = optind;
	if (Gname && (flags & PRINT)) {
		fprintf(stderr, "%s: can't use -G and -p together,\n", av[0]);
		usage();
	}
	if (Gname) flags |= PRINT|GET_PERMS;
	if ((flags & (PRINT|GET_SKIPGET)) == (PRINT|GET_SKIPGET)) {
		fprintf(stderr, "%s: can't use -g with -p/G\n", prog);
		return(1);
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
	if (Gname && isdir(Gout)) {
		gdir = strdup(Gout);
		Gout[0] = 0;
	}
	if (((Gname && !gdir) || iLst || xLst) && sfileNext()) {
onefile:	fprintf(stderr,
		    "%s: only one file name with -G/i/x.\n", av[0]);
		usage();
	}
	if (av[optind] && av[optind+1] && strneq(av[optind+1], "-G", 2)) {
		if (Gname) goto onefile;
		Gname = 1;
		strcpy(Gout, av[optind+1] + 2);
		flags |= PRINT|GET_PERMS;
	}
	if (rev && closetips) {
		fprintf(stderr,
		    "%s: -M can not be combined with rev.\n", av[0]);
		usage();
	}
	if ((Rev || mRev || iLst || xLst || (flags & PRINT)) &&
	    (flags & GET_NOREGET)) {
		fprintf(stderr,
		    "%s: -S cannot be used with -r/-M/-i/-x/-p/-G.\n", av[0]);
		usage();
	}
	switch (getdiff) {
	    case 0: break;
	    case 1: flags |= GET_DIFFS; break;
	    case 2: flags |= GET_BKDIFFS; break;
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

	if (nfiles > -1) tick = progress_start(PROGRESS_BAR, nfiles);
	for (; name; name = sfileNext()) {
		d = 0;

		if (tick) progress(tick, ++n);
		if (tickout) {
			putchar('.');
			fflush(stdout);
		}
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
		if (flags & GET_NOREGET) {
			gfile = sccs2name(name);
			if (flags & GET_EDIT) {
				c = (writable(gfile) && xfile_exists(gfile, 'p'));
			} else {
				c = exists(gfile);
			}
			free(gfile);
			if (c) continue;
		}
		unless (s = sccs_init(name, iflags)) continue;
#ifdef	WIN32
		d = sccs_top(s);
		if (d && S_ISLNK(MODE(s, d))) {
			if (getenv("BK_WARN_SYMLINK")) {
				fprintf(stderr,
				    "warning: %s is a symlink, skipping it.\n",
				    s->gfile);
			}
			sccs_free(s);
			continue;
		}
#endif
		if (checkout) {
			flags &= ~(GET_EDIT|GET_EXPAND);
			switch (CO(s)) {
			    case CO_NONE:
			    case CO_LAST:
				sccs_free(s);
				continue;
			    case CO_EDIT:
			    	flags |= GET_EDIT;
				break;
			    case CO_GET:
				flags |= GET_EXPAND;
				break;
			}
		}
		if (Rev) {
			rev = Rev;
		} else {
			rev = sfileRev();
		}
		if (Gname) {
			if (gdir) {
				unless (d = sccs_findrev(s, rev)) {
					fprintf(stderr, "%s: cannot find rev "
					    "%s in %s\n", prog, rev, s->gfile);
					goto err;
				}
				sprintf(Gout, "%s/%s", gdir,
				    HAS_PATHNAME(s, d) ? PATHNAME(s, d) : s->gfile);
			}
			unlink(Gout);
			out = Gout;
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
		if (dohash) {
			if (HASH(s)) {
				s->xflags &= ~X_HASH;
			} else {
				s->xflags |= X_HASH;
			}
		}
		if (BITKEEPER(s) &&
		    (iLst || xLst) && !branch_ok && !(flags & GET_EDIT)) {
			unless (flags & PRINT) {
				fprintf(stderr,
				    "%s: can't specify include/exclude "
				    "without -p, -e or -l\n", av[0]);
				goto err;
			}
		}
		if (BITKEEPER(s) && rev && !branch_ok
		    && !(flags & GET_EDIT) && !streq(rev, "+")) {
			unless (flags & PRINT) {
				fprintf(stderr,
				    "%s: can't specify revisions without -p\n",
				    av[0]);
				goto err;
			}
		}
		/* -M with no args means to close the open tip */
		if (closetips) {
			ser_t	a, b;

			if (sccs_findtips(s, &a, &b)) {
				if (R2(s, a)) {
					mRev = REV(s, a);
				} else if (R2(s, b)) {
					mRev = REV(s, b);
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
		if (BITKEEPER(s) && ((flags & (PRINT|GET_EDIT)) == GET_EDIT) &&
		    rev && !branch_ok) {
			/* recalc iLst and xLst to be relative to tip */
			if (get_rollback(s, rev, mRev, &iLst, &xLst, av[0])) {
				goto next;
			}
			rollback = 1;
			rev = 0;	/* Use tip below */
		}
		if (pnames) {
			printf("|FILE|%s|CRC|%lu\n", s->gfile,
			    adler32(0, s->gfile, strlen(s->gfile)));
		}
		if ((flags & (GET_DIFFS|GET_BKDIFFS))
		    ? sccs_getdiffs(s, rev, flags, out)
		    : sccs_get(s, rev, mRev, iLst, xLst,
			flags, (out||fout) ? out : s->gfile, fout)) {
			if (s->cachemiss && !recursed) {
				if (skip_bam) goto next;
				if (bp_proj && (s->proj != bp_proj)) {
					if (bp_fetchkeys(prog, bp_proj,
						(flags & SILENT) ? 0 : 1,
						bp_keys, bp_todo)) {
						fprintf(stderr,
						    "%s: failed to fetch "
						    "BAM data\n", prog);
						return (1);
					}
					proj_free(bp_proj);
					bp_proj = 0;
					freeLines(bp_keys, free);
					bp_keys = 0;
					bp_todo = 0;
				}
				unless (bp_proj) {
					bp_proj = proj_init(proj_root(s->proj));
				}
				d = bp_fdelta(s, sccs_findrev(s, rev));
				bp_files = addLine(bp_files, strdup(name));
				bp_keys = addLine(bp_keys,
				    sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC));
				bp_todo += ADDED(s, d);	// XXX - not all of it
				goto next;
			}
			if (s->io_error) return (1);
			unless (BEEN_WARNED(s)) {
				verbose((stderr,
				    "%s of %s failed, skipping it.\n",
				    prog, s->gfile));
			}
			errors = 1;
		}
next:		sccs_free(s);
		if (rollback) {
			FREE(iLst);
			FREE(xLst);
		}
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
	if (bp_proj) {
		if (bp_fetchkeys(prog, bp_proj,
			(flags & SILENT) ? 0 : 1, bp_keys, bp_todo)) {
			fprintf(stderr, "%s: failed to fetch BAM data\n", prog);
			return (1);
		}
		proj_free(bp_proj);
		freeLines(bp_keys, free);
		bp_keys = 0;
		bp_todo = 0;
	}
	if (bp_files && !recursed) {

		/* If we already had an error don't let this turn that
		 * into a non-error.
		 */
		if (c = bam(prog, (flags & SILENT), bp_files, ac_optend, av)) {
		    	errors = c;
	    	}
	}
	if (tick) progress_done(tick, errors ? "errors" : "OK");
	freeLines(bp_files, free);
	freeLines(bp_keys, free);
	return (errors);
}

private int
bam(char *me, int q, char **files, int ac, char **av)
{
	char	*nav[100];
	FILE	*f;
	int	i;

	unless (files) return (0);
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

private int
get_rollback(sccs *s, char *rev, char *mRev, char **iLst, char **xLst, char *me)
{
	char	*inc = *iLst, *exc = *xLst;
	u8	*map;
	ser_t	d, m;

	*iLst = *xLst = 0;
	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "%s: cannot find %s in %s\n", me,rev, s->gfile);
		return (1);
	}
	unless (mRev) {
		m = 0;
	} else unless (m = sccs_findrev(s, mRev)) {
		fprintf(stderr, "%s: cannot find %s in %s\n", me,mRev,s->gfile);
		return (1);
	}
	unless (map = sccs_set(s, d, m, inc, exc)) return (1);
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
	if (streq(args, "none")) return (flags);
	while (*args) {
		switch (*args) {
			/* Larry sez users won't use seq so make it obscure */
		    case '0': flags |= GET_SEQ; break;
		    case '5': flags |= GET_MD5KEY; break;
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
