/*
 * Copyright 1998-2016 BitMover, Inc
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

#define	OP(W, V, F) if (next##W < A_SZ-1) { \
			W[next##W].thing = V; \
			W[next##W++].flags = F; \
		  } else { /* CSTYLED */ \
			fprintf(stderr, "admin: argument overflow.\n"); \
			exit(1); \
		    }

private	int	do_checkin(char *nm, u32 fl,
		   char *rev, char *newf, char *com);
private	int	setMerge(sccs *sc, char *merge, char *rev);
private	void	rootCsetFile(sccs *sc, char *csetFile);
private	void	replacePlus(sccs *s, pfile *pf);

int
admin_main(int ac, char **av)
{
	sccs	*sc;
	u32	flags = 0;
	u32	dflags = 0;	/* delta flags */
	int	init_flags = 0;
	char	*rev = 0;
	int	c;
	admin	f[A_SZ], u[A_SZ], s[A_SZ];
	int	nextf = 0, nextu = 0, nexts = 0, nextp = 0;
	char	*comment = 0, *text = 0, *newfile = 0;
	char	*path = 0, *merge = 0;
	char	*name;
	int	error = 0;
	int	addCsets = 0, dopath = 0, rmCsets = 0, newCset = 0;
	int	doDates = 0, touchGfile = 0;
	char	*m = 0;
	char	*csetFile = 0;
	char	*obscure = 0;
	ser_t	d = 0;
	int 	was_edited = 0, new_delta = 0;
	pfile	pf = {0};
	longopt	lopt[] = {
		{ "text;", 310 },
		{ 0, 0 }
	};

	bzero(f, sizeof(f));
	bzero(u, sizeof(u));
	bzero(s, sizeof(s));
	while ((c =
	    getopt(ac, av, "a;AC|d;e;E;f;F;i|M;m;O;p|P|r;S;y|Z|0DhHnqsuz",
		lopt)) != -1) {
		switch (c) {
		/* user|group */
		    case 'a':	OP(u, optarg, A_ADD); break; 	/* undoc? 2.0 */
		    case 'e':	OP(u, optarg, A_DEL); break; 	/* undoc? 2.0 */
		/* flags */
		    case 'f':					/* doc 2.0 */
				OP(f, optarg, A_ADD); new_delta = 1; break; 
		    case 'F':					/* doc 2.0 */
		    case 'd':					/* undoc 2.0 */
				OP(f, optarg, A_DEL); new_delta = 1; break; 
		/* new file options */
		    case 'i':	newfile = optarg ? optarg : "-"; /* doc 2.0 */
				dflags |= DELTA_NEWFILE; break;
		    case 'n':	dflags |= DELTA_NEWFILE; break;   	/* undoc? 2.0 */
		    case 'r':	rev = optarg; break;		/* undoc */
		    case 'y':	comment = optarg; break;	/* doc 2.0 */
		    case 'M':					/* undoc */
				merge = optarg; flags |= NEWCKSUM; break;
		/* mode */
		    case 'm':	m = optarg;			/* undoc */
				new_delta = 1;
		   		/* NEWCKSUM done in sccs_admin */
				break;
		/* pathname */
		    case 'P':	dopath++;
		    case 'p':	path = optarg;			/* undoc */
		    		flags |= ADMIN_SHUTUP|NEWCKSUM;
				dopath++;
				break;
		/* compression */
		    case 'Z':					/* doc 2.0 */
			unless (optarg && streq(optarg, "same")) {
				bk_setConfig("compression",
				    optarg ? optarg : "gzip");
			}
			flags |= NEWCKSUM;
			touchGfile++;
			break;
		    case 'E':	fprintf(stderr, "No longer supported.\n");
		    		exit(1);

		/* symbols */
		    case 'S':	OP(s, optarg, A_ADD); break;	/* undoc */
		/* singletons */
		    case '0':					/* doc 2.0 */
			flags |= ADMIN_ADD1_0|NEWCKSUM; break;
		    case 'A':					/* doc 2.0 */
			addCsets = 1; flags |= NEWCKSUM; break;
		    case 'C':					/* doc 2.0 */
			csetFile = optarg; newCset++; flags |= NEWCKSUM; break;
		    case 'D':					/* doc 2.0 */
			rmCsets = 1; flags |= NEWCKSUM; break;
		    case 'h':	unless (flags & ADMIN_FORMAT) {	/* doc 2.0 */
		    			flags |= ADMIN_FORMAT;
				} else unless (flags & ADMIN_BK) {
		    			flags |= ADMIN_BK;
				} else unless (flags & ADMIN_TIME) {
					flags |= ADMIN_TIME;
				}
				break;
		    case 'H':	/* obsolete, remove in 2009 */
				flags |= ADMIN_FORMAT|ADMIN_BK|ADMIN_TIME;
				break;
		    case 's':					/* undoc? 2.0 */
		    case 'q':	flags |= SILENT; break;		/* doc 2.0 */
		    case 'u':					/* undoc */
			doDates = 1; flags |= NEWCKSUM; break;
		    case 'z':	init_flags |= INIT_NOCKSUM;	/* doc 2.0 */
				init_flags |= INIT_WACKGRAPH;	/* doc 2.0 */
		    		flags |= NEWCKSUM;
				touchGfile++;
				break;
		    case 'O':	obscure = optarg; break;
		    case 310: // --text=
			text = optarg ? optarg : "";
			break;
		    default: bk_badArg(c, av);
		}
	}
	if ((flags & ADMIN_FORMAT) && ((flags & ~(ADMIN_CHECKS|SILENT)) ||
	    nextf || nextu || nexts || nextp || comment || path || newCset ||
	    newfile || doDates || rev)) {
		fprintf(stderr, "admin: -h option must be alone.\n");
		usage();
	}
	if (merge && ((flags & ~(ADMIN_CHECKS|SILENT|NEWCKSUM)) ||
	    nextf || nextu || nexts || nextp || comment || path || newCset ||
	    newfile || doDates)) {
		fprintf(stderr, "admin: -M option must be alone or with -r\n");
		usage();
	}
	if ((flags & ADMIN_ADD1_0) &&
	    ((flags & ~(ADMIN_CHECKS|ADMIN_ADD1_0|SILENT|NEWCKSUM)) ||
	    nextf || nextu || nexts || nextp || comment || path || newCset ||
	    newfile || doDates || rev)) {
		fprintf(stderr, "admin: -0 option must be alone\n");
		usage();
	}
	if (comment && !(dflags & DELTA_NEWFILE)) {
		fprintf(stderr,
		    "admin: comment may only be specified with -i and/or -n\n");
		usage();
	}
	if (obscure) {
		unless (getenv("BK_FORCE")) {
			fprintf(stderr, "Set BK_FORCE to do this\n");
			exit(1);
		}
		if (streq(obscure, "all")) {
			flags |= NEWCKSUM|ADMIN_OBSCURE;
		} else {
			fprintf(stderr,
			    "admin: unrecognized obscure option %s\n",
			    obscure);
			exit (1);
		}
	}
	/* All of these need to be here: m/nextf are for resolve,
	 * newfile is for !BK mode.
	 */
	if (rev && (!(dflags & DELTA_NEWFILE) && !merge && !m && !nextf && !path)) {
		fprintf(stderr, "%s %s\n",
		    "admin: revision may only be specified with",
		    "-i and/or -n or -M or -m or -f or -F\n");
		usage();
	}
	if ((dflags & DELTA_NEWFILE) && nextf) {
		fprintf(stderr,
		    "admin: cannot have -f with -i and/or -n\n");
		usage();
	}
	if ((dflags & DELTA_NEWFILE) && text && !text[0]) {
		fprintf(stderr,
		    "admin: -t must have file arg with -i and/or -n\n");
		usage();
	}
	name = sfileFirst("admin", &av[optind], 0);
	if ((dflags & DELTA_NEWFILE) && sfileNext()) {
		fprintf(stderr, "admin: Only one file with -i/-n\n");
		usage();
	}

	while (name) {
		if (dflags & DELTA_NEWFILE) {
			if (do_checkin(name, SILENT|dflags,
			    rev, newfile, comment)) {
				error  = 1;
				name = sfileNext();
				continue;
			}
		}
		sc = sccs_init(name, init_flags);
		unless (sc) { name = sfileNext(); continue; }
		unless (HASGRAPH(sc)) {
			fprintf(stderr,
				"admin: can't read delta table in %s\n",
			    sc->sfile);
			sccs_free(sc);
			name = sfileNext();
			error = 1;
			continue;
		}
		if (flags & ADMIN_ADD1_0) {
			if (sccs_adminFlag(sc, flags)) {
			    	fprintf(stderr,
				    "admin: failed to add 1.0 to %s\n",
				    sc->gfile);
				exit(1);
			}
			sccs_free(sc);
			name = sfileNext();
			continue;
		}
		if (dopath) {
			ser_t	d;

			for (d = TABLE(sc);
			    (dopath == 2) && (d >= TREE(sc)); d--) {
				if (d == TREE(sc)) break;
				/* ugly and inefficient temporary state */
				PATHNAME_SET(sc, d, path);
				SORTPATH_INDEX(sc, d) = PATHNAME_INDEX(sc, d);
			}
			d = rev ? sccs_findrev(sc, rev) : sccs_top(sc);
			sccs_parseArg(sc, d, 'P', path ? path : sc->gfile, 0);
		}
		if (newCset) {
			if (csetFile) rootCsetFile(sc, csetFile);
			flags |= NEWCKSUM;
		}
		if (rmCsets) {
			sccs_clearbits(sc, D_CSET);
		}
		/*
		 * Put cset marks in ALL deltas.  Probably only useful
		 * for restoring the ChangeSet file's marks after
		 * detaching a component repo.
		 */
		if (addCsets) {
			ser_t	d;

			for (d = TABLE(sc); d >= TREE(sc); --d) {
				if (d == TREE(sc)) break;
				if (TAG(sc, d)) continue;
				FLAGS(sc, d) |= D_CSET;
			}
		}
		if (doDates) sccs_fixDates(sc);
		if (merge) {
			if (setMerge(sc, merge, rev) == -1) {
				error = 1;
				goto next;
			}
		}
		if (new_delta) {
			if (EDITED(sc)) {
				was_edited = 1;
				sccs_read_pfile(sc, &pf);
				replacePlus(sc, &pf);
			} else {
				was_edited = 0;
			}
		}
		if (rev) d = sccs_findrev(sc, rev);
		if (sccs_admin( sc, d, flags, f, 0, u, s, m, text)) {
			sccs_whynot("admin", sc);
			error = 1;
		}
		if (touchGfile) sccs_touch(sc);
		/*
		 * re init so sccs_get would work
		 */
		if (new_delta && was_edited) {
			int gflags = SILENT|GET_SKIPGET|GET_EDIT;
			char *nrev;

			sccs_free(sc);
			sc = sccs_init(name, init_flags);
			nrev =
			    sccs_findrev(sc, pf.newrev) ? pf.newrev: pf.oldrev;
			if (sccs_get(sc, nrev,
			    pf.mRev, pf.iLst, pf.xLst, gflags, 0, 0)) {
				fprintf(stderr, "cannot adjust p file\n");
			}
		}
next:		sccs_free(sc);
		free_pfile(&pf);
		name = sfileNext();
	}
	if (sfileDone()) error = 1;
	return (error);
}

/*
 * Handle -n.  Allowable options are:
 *	-i (imples -n)
 *	-r
 *	-y
 * XXX - have a way of including a symbol?
 *
 * XXX - this is really yucky.  The only reason this is here is because the
 * gfile name can be different than the init file.  The real answer should be
 * to stuff the initFile into the sccs* and have checkin() respect that.
 */
private	int
do_checkin(char *name,
	u32 flags, char *rev, char *newfile, char *comment)
{
	ser_t	d = 0;
	sccs	*s;
	int	error;
	struct	stat sb;

	unless (s = sccs_init(name, (flags & 0xf))) return (-1);
	if (rev && !streq(rev, "1.1") && proj_root(s->proj)) {
		fprintf(stderr,
		    "admin: can not specify initial rev for BK files\n");
		sccs_free(s);
		return (-1);
	}
	if (HAS_SFILE(s)) {
		fprintf(stderr, "admin: %s exists.\n", s->sfile);
		sccs_free(s);
		return (1);
	}

	/*
	 * If they specified a gfile and it is different than the implied
	 * gfile, and the implied gfile exists, that's too weird, fail it.
	 */
	if ((s->state & S_GFILE) && newfile && strcmp(newfile, s->gfile)) {
		fprintf(stderr,
		    "admin: gfile %s exists and is "
		    "different than specified init file %s\n",
		    s->gfile, newfile);
		sccs_free(s);
		return (-1);
	}

	if (newfile && (lstat(newfile, &sb) == 0)) {
		if (S_ISLNK(sb.st_mode) || S_ISREG(sb.st_mode)) {
			s->mode = sb.st_mode;
		} else {
			verbose((stderr,
			    "admin: ignoring modes on %s\n", newfile));
		}
		if (S_ISLNK(sb.st_mode)) {
			char	p[MAXPATH];
			int	n;

			n = readlink(newfile, p, sizeof(p));
			if (n > 0) {
				p[n] = 0;
				s->symlink = strdup(p);
			} else {
				perror(newfile);
				sccs_free(s);
				return (-1);
			}
		}
	}
	unless (s->mode) {
		mode_t	mask = ~umask(0);

		umask(~mask);
		s->mode = S_IFREG | (0777 & mask);
	}
	if (rev) {
		d = sccs_parseArg(s, d, 'R', rev, 0);
		if ((FLAGS(s, d) & D_BADFORM) ||
		    (!R0(s, d) || (!R1(s, d) && (R0(s, d) != 1))) ||
		    (R2(s, d) && !R3(s, d))) {
			sccs_freedelta(s, d);
			fprintf(stderr, "admin: bad revision: %s for %s\n",
			    rev, s->sfile);
			sccs_free(s);
			return (-1);
		}
	}
	if (newfile) {
		free(s->gfile);
		s->gfile = strdup(newfile);
	} else {
		flags |= DELTA_EMPTY;
	}
	s->state |= S_GFILE;
	flags |= DELTA_SAVEGFILE;
	if (comment) { d = sccs_parseArg(s, d, 'C', comment, 0); }
	if ((error = sccs_delta(s, flags, d, 0, 0, 0))) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "admin: failed to check in %s.\n",
			    s->sfile);
		}
	}
	sccs_free(s);
	return (error);
}

private	int
setMerge(sccs *sc, char *merge, char *rev)
{
	ser_t	d, p;

	unless (d = sccs_findrev(sc, rev)) {
		fprintf(stderr, "admin: can't find %s in %s\n",
		    rev, sc->sfile);
		return -1;
	}
	unless (p = sccs_findrev(sc, merge)) {
		fprintf(stderr, "admin: can't find %s in %s\n",
		    merge, sc->sfile);
		return -1;
	}
	MERGE_SET(sc, d, p);
	return 0;
}

/*
 * XXX TODO move this to slib.c
 */
void
sccs_touch(sccs *s)
{
	struct utimbuf ut;

	/* TODO: We need to handle split root config here */
	unless(s->gfile && exists(s->gfile)) return;
	ut.actime = ut.modtime = time(0);
	utime(s->gfile, &ut);
}

/*
 * Mimic the old DUP idea in changing the root value of csetfile,
 * and rippling that out to other places that had the same value
 * as the parent.
 */
private	void
rootCsetFile(sccs *sc, char *csetFile)
{
	int	last, new_cf;
	char	*orig;
	ser_t	d, p;

	d = TREE(sc);
	orig = strdup(CSETFILE(sc, d));
	sccs_parseArg(sc, d, 'B', csetFile, 0);
	new_cf = CSETFILE_INDEX(sc, d);
	FLAGS(sc, d) |= D_RED;
	last = d;

	for (++d; d <= TABLE(sc); ++d) {
		unless (p = PARENT(sc, d)) continue;
		unless (FLAGS(sc, p) & D_RED) continue;
		unless (streq(orig, CSETFILE(sc, d))) continue;
		CSETFILE_INDEX(sc, d) = new_cf;
		FLAGS(sc, d) |= D_RED;
		last = d;
	}
	for (d = last; d >= TREE(sc); --d) {
		FLAGS(sc, d) &= ~D_RED;
	}
	free(orig);
}

/*
 * The pfile can have '+' in it.  The pfile is about to be re-written.
 * Translate '+' to a revision first.
 */
private	void
_replacePlus(sccs *s, char **listp)
{
	char	**revs;
	char	*list = *listp;
	int	i, changed = 0;

	unless (list && strchr(list, '+')) return;

	revs = splitLine(list, ",", 0);
	EACH(revs) {
		if (streq(revs[i], "+")) {
			free(revs[i]);
			revs[i] = strdup(REV(s, sccs_top(s)));
			changed = 1;
		}
	}
	if (changed) {
		free(list);
		*listp = joinLines(",", revs);
	}
	freeLines(revs, free);
}

private	void
replacePlus(sccs *s, pfile *pf)
{
	_replacePlus(s, &pf->mRev);
	_replacePlus(s, &pf->iLst);
	_replacePlus(s, &pf->xLst);
}
