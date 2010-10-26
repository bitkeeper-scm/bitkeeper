/* Copyright (c) 1998 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "logging.h"

#define	OP(W, V, F) if (next##W < A_SZ-1) { \
			W[next##W].thing = V; \
			W[next##W++].flags = F; \
		  } else { /* CSTYLED */ \
			fprintf(stderr, "admin: argument overflow.\n"); \
			exit(1); \
		    }

private	int	do_checkin(char *nm, char *cp, int fl,
		   char *rev, char *newf, char *com);
private	int	setMerge(sccs *sc, char *merge, char *rev);

int
admin_main(int ac, char **av)
{
	sccs	*sc;
	int	flags = 0;
	int	init_flags = 0;
	char	*rev = 0;
	int	c;
	admin	f[A_SZ], u[A_SZ], s[A_SZ];
	int	nextf = 0, nextu = 0, nexts = 0, nextp = 0;
	char	*comment = 0, *text = 0, *newfile = 0;
	char	*path = 0, *merge = 0;
	char	*name;
	char	*compp = 0;
	int	error = 0;
	int	addCsets = 0, dopath = 0, rmCsets = 0, newCset = 0;
	int	doDates = 0, touchGfile = 0;
	char	*m = 0;
	char	*csetFile = 0;
	char	*obscure = 0;
	delta	*d = 0;
	int 	was_edited = 0, new_delta = 0;
	pfile	pf;

	bzero(f, sizeof(f));
	bzero(u, sizeof(u));
	bzero(s, sizeof(s));
	while ((c =
	    getopt(ac, av, "a;AC|d;e;E;f;F;i|M;m;O;p|P|r;S;t|y|Z|0DhHnqsTuz", 0))
	       != -1) {
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
				flags |= NEWFILE; break;
		    case 'n':	flags |= NEWFILE; break;   	/* undoc? 2.0 */
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
		    case 'Z':	compp = optarg ? optarg : "gzip"; /* doc 2.0 */
				touchGfile++;
		   		/* NEWCKSUM done in sccs_admin */
				break;
		    case 'E':	fprintf(stderr, "No longer supported.\n");
		    		exit(1);
		/* symbols */
		    case 'S':	OP(s, optarg, A_ADD); break;	/* undoc */
		/* text */
		    case 't':					/* doc 2.0 */
			text = optarg ? optarg : ""; new_delta = 1; break;
		    case 'T':					/* doc 2.0 */	
			text = ""; new_delta = 1; break;
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
		    		flags |= NEWCKSUM;
				touchGfile++;
				break;
		    case 'O':	obscure = optarg; break;
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
	if (comment && !(flags & NEWFILE)) {
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
		} else if (streq(obscure, "license")) {
			flags |= NEWCKSUM|ADMIN_RMLICENSE;
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
	if (rev && (!(flags & NEWFILE) && !merge && !m && !nextf && !path)) {
		fprintf(stderr, "%s %s\n",
		    "admin: revision may only be specified with",
		    "-i and/or -n or -M or -m or -f or -F\n");
		usage();
	}
	if ((flags & NEWFILE) && nextf) {
		fprintf(stderr,
		    "admin: cannot have -f with -i and/or -n\n");
		usage();
	}
	if ((flags & NEWFILE) && text && !text[0]) {
		fprintf(stderr,
		    "admin: -t must have file arg with -i and/or -n\n");
		usage();
	}
	name = sfileFirst("admin", &av[optind], 0);
	if ((flags & NEWFILE) && sfileNext()) {
		fprintf(stderr, "admin: Only one file with -i/-n\n");
		usage();
	}

	while (name) {
		if (flags & NEWFILE) {
			if (do_checkin(name, compp,
			    flags&(SILENT|NEWFILE), rev, newfile, comment)) {
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
			if (sccs_admin(sc, 0, flags, 0, 0, 0, 0, 0, 0, 0)) {
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
			delta	*d;

			for (d = sc->table; (dopath == 2) && d; d = NEXT(d)) {
				unless (NEXT(d)) break;
				/* ugly and inefficient temporary state */
				if (d->pathname) free(d->pathname);
				d->pathname = PATH_BUILD(path, "");
				d->flags &= ~D_DUPPATH;
			}
			d = rev ? sccs_findrev(sc, rev) : sccs_top(sc);
			sccs_parseArg(d, 'P', path ? path : sc->gfile, 0); 
		}
		if (newCset) {
			if (bk_notLicensed(sc->proj, LIC_ADM, 0)) exit(1);
			sccs_parseArg(sc->tree, 'B', csetFile, 0);
			flags |= NEWCKSUM;
		}
		if (rmCsets) {
			if (bk_notLicensed(sc->proj, LIC_ADM, 0)) exit(1);
			sccs_clearbits(sc, D_CSET);
		}
		/*
		 * Put cset marks in ALL deltas.  Probably only useful
		 * for restoring the ChangeSet file's marks after
		 * detaching a component repo.
		 */
		if (addCsets) {
			delta	*d;

			for (d = sc->table; d; d = NEXT(d)) {
				unless (NEXT(d)) break;
				if (TAG(d)) continue;
				d->flags |= D_CSET;
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
				sccs_read_pfile("admin", sc, &pf);
				if (unlink(sc->pfile)) {
					fprintf(stderr,
					"admin: cannot unlink %s\n", sc->pfile);
					goto next;
				}
			} else {
				was_edited = 0;
			}
		}
		if (rev) d = sccs_findrev(sc, rev);
		if (sccs_admin( sc, d, flags, compp, f, 0, u, s, m, text)) {
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
			if (sccs_get(sc, nrev, 0, 0, 0, gflags, "-")) {
				fprintf(stderr, "cannot adjust p file\n");	
			}
		}
next:		sccs_free(sc);
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
	char *compp, int flags, char *rev, char *newfile, char *comment)
{
	delta	*d = 0;
	sccs	*s;
	int	error;
	struct	stat sb;

	unless (s = sccs_init(name, flags)) return (-1);
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
		d = sccs_parseArg(d, 'R', rev, 0);
		if ((d->flags & D_BADFORM) ||
		    (!d->r[0] || (!d->r[1] && (d->r[0] != 1))) ||
		    (d->r[2] && !d->r[3])) {
			sccs_freetree(d);
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
	if (comment) { d = sccs_parseArg(d, 'C', comment, 0); }
	if ((error = sccs_delta(s, flags|DELTA_SAVEGFILE, d, 0, 0, 0))) {
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
	delta *d, *p;

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
	d->merge = p->serial;
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
