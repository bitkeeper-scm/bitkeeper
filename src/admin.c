/* Copyright (c) 1998 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");


#define	OP(W, V, F) if (next##W < A_SZ-1) { \
			W[next##W].thing = V; \
			W[next##W++].flags = F; \
		  } else { /* CSTYLED */ \
			fprintf(stderr, "admin: argument overflow.\n"); \
			exit(1); \
		    }

private	int	do_checkin(char *nm, char *ep, char *cp, int fl,
		   char *rev, char *newf, char *com);
private	void	clearCset(sccs *s, int flags, int which);
private	void	touch(sccs *s);
private	int	setMerge(sccs *sc, char *merge, char *rev);
extern	int     newrev(sccs *s, pfile *pf); 

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
	char	*encp = 0, *compp = 0;
	int	error = 0;
	int	bigpad = 0;
	int	fastSym = 0, dopath = 0, rmCset = 0;
	int	doDates = 0, touchGfile = 0;
	char	*m = 0;
	delta	*d = 0;
	int 	was_edited = 0, new_delta = 0;
	pfile	pf;
	project	*proj = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		//fprintf(stderr, "%s", help);
		system("bk help admin");
		return (1);
	}
	bzero(f, sizeof(f));
	bzero(u, sizeof(u));
	bzero(s, sizeof(s));
	while ((c =
	    getopt(ac, av, "a;d;e;E;f;F;i|M;m;p|r;S;t|y|Z|0BChHnqsTuz"))
	       != -1) {
		switch (c) {
		/* user|group */
		    case 'a':	OP(u, optarg, A_ADD); break;
		    case 'e':	OP(u, optarg, A_DEL); break;
		/* flags */
		    case 'f':	OP(f, optarg, A_ADD); new_delta = 1; break;
		    case 'F':
		    case 'd':	OP(f, optarg, A_DEL); new_delta = 1; break;
		/* new file options */
		    case 'i':	newfile = optarg ? optarg : "-";
				flags |= NEWFILE; break;
		    case 'n':	flags |= NEWFILE; break;
		    case 'r':	rev = optarg; break;
		    case 'y':	comment = optarg; break;
		    case 'M':	merge = optarg; flags |= NEWCKSUM; break;
		/* mode */
		/* XXX should accept octal modes too */
		    case 'm':	m = optarg;
		    		switch (m[0]) {
				    case '-':
				    case 'l':
				    case 'd':
					break;
				    default:
				    	fprintf(stderr,
					    "%s: mode must be like ls -l\n",
					    av[0]);
					goto usage;
				}
		   		flags |= NEWCKSUM;
				break;
		/* pathname */
		    case 'p':	path = optarg;
		    		flags |= ADMIN_SHUTUP|NEWCKSUM;
				dopath++;
				break;
		/* encoding and compression */
		    case 'Z':	compp = optarg ? optarg : "gzip";
				flags |= NEWCKSUM;
				touchGfile++;
				break;
		    case 'E':	encp = optarg; break;
		/* symbols */
		    case 'S':	OP(s, optarg, A_ADD); break;
		/* text */
		    case 't':	text = optarg ? optarg : ""; new_delta = 1; break;
		    case 'T':	text = ""; new_delta = 1; break;
		/* singletons */
		    case '0':	flags |= ADMIN_ADD1_0|NEWCKSUM; break;
		    case 'B':	bigpad++; break;
		    case 'C':	rmCset++; flags |= NEWCKSUM; break;
		    case 'h':	if (flags & ADMIN_FORMAT) {
		    			flags |= ADMIN_BK;
				} else if (flags & ADMIN_BK) {
					flags |= ADMIN_TIME;
				} else {
		    			flags |= ADMIN_FORMAT;
				}
				break;
		    case 'H':	flags |= ADMIN_FORMAT|ADMIN_ASCII|ADMIN_TIME;
				break;
		    case 's':
		    case 'q':	flags |= SILENT; break;
		    case 'u':	doDates = 1; flags |= NEWCKSUM; break;
		    case 'z':	init_flags |= INIT_NOCKSUM;
		    		flags |= NEWCKSUM;
				touchGfile++;
				break;
		    default:	fprintf(stderr, "admin: bad option %c.\n", c);
				goto usage;
		}
	}
	if ((flags & ADMIN_FORMAT) && ((flags & ~(ADMIN_CHECKS|SILENT)) ||
	    nextf || nextu || nexts || nextp || comment || path || rmCset ||
	    newfile || doDates || rev)) {
		fprintf(stderr, "admin: -h option must be alone.\n");
		goto usage;
	}
	if (merge && ((flags & ~(ADMIN_CHECKS|SILENT|NEWCKSUM)) ||
	    nextf || nextu || nexts || nextp || comment || path || rmCset ||
	    newfile || doDates)) {
		fprintf(stderr, "admin: -M option must be alone or with -r\n");
		goto usage;
	}
	if ((flags & ADMIN_ADD1_0) &&
	    ((flags & ~(ADMIN_CHECKS|ADMIN_ADD1_0|SILENT|NEWCKSUM)) ||
	    nextf || nextu || nexts || nextp || comment || path || rmCset ||
	    newfile || doDates || rev)) {
		fprintf(stderr, "admin: -0 option must be alone\n");
		goto usage;
	}
	if (comment && !(flags & NEWFILE)) {
		fprintf(stderr,
		    "admin: comment may only be specifed with -i and/or -n\n");
		goto usage;
	}
	/* All of these need to be here: m/nextf are for resolve,
	 * newfile is for !BK mode.
	 */
	if (rev && (!(flags & NEWFILE) && !merge && !m && !nextf)) {
		fprintf(stderr, "%s %s\n",
		    "admin: revision may only be specified with",
		    "-i and/or -n or -M or -m or -f or -F\n");
		goto usage;
	}
	if ((flags & NEWFILE) && nextf) {
		fprintf(stderr,
		    "admin: cannot have -f with -i and/or -n\n");
		goto usage;
	}
	if ((flags & NEWFILE) && text && !text[0]) {
		fprintf(stderr,
		    "admin: -t must have file arg with -i and/or -n\n");
		goto usage;
	}
	name = sfileFirst("admin", &av[optind], 0);
	if ((flags & NEWFILE) && sfileNext()) {
		fprintf(stderr, "admin: Only one file with -i/-n\n");
		goto usage;
	}
	unless (flags & NEWFILE) init_flags |= INIT_SAVEPROJ;

	/*
	 * If we are adding exactly one symbol, do it quickly.
	 */
	fastSym = !(flags & ~(SILENT|NEWCKSUM)) && !nextf && !nextu && !nextp &&
	    !rev && nexts && (s[0].flags == A_ADD) && !s[1].flags;
	if (fastSym) init_flags |= (INIT_MAPWRITE|INIT_NOCKSUM);
	while (name) {
		if (flags & NEWFILE) {
			if (do_checkin(name, encp, compp,
				       flags&(SILENT|NEWFILE),
				       rev, newfile, comment)) {
				error  = 1;
				name = sfileNext();
				continue;
			}
		}
		sc = sccs_init(name, init_flags, proj);
		unless (sc) { name = sfileNext(); continue; }
		if (!proj && (init_flags & INIT_SAVEPROJ)) proj = sc->proj;
		unless (sc->tree) {
			fprintf(stderr,
				"admin: can't read delta table in %s\n",
			    sc->sfile);
			sccs_free(sc);
			name = sfileNext();
			error = 1;
			continue;
		}
		if (flags & ADMIN_ADD1_0) {
			if (streq(sc->tree->rev, "1.0")) {
				verbose((stderr,
				    "admin: %s already has 1.0\n", sc->gfile));
				sccs_free(sc);
				name = sfileNext();
				continue;
			}
			if (sccs_admin(sc, 0, flags, 0, 0, 0, 0, 0, 0, 0, 0)) {
			    	fprintf(stderr,
				    "admin: failed to add 1.0 to %s\n",
				    sc->gfile);
				exit(1);
			}
			sccs_free(sc);
			continue;
		}
		if (bigpad) {
			sc->state |= S_BIGPAD;
			flags |= NEWCKSUM;
		}
		if (fastSym && sc->landingpad) {
			int	rc = sccs_addSym(sc, flags, s[0].thing);

			if (rc == -1) error = 1;
			if (rc != EAGAIN) goto next;
		}
		if (dopath) {
			delta	*top = findrev(sc, 0);

			if (top->pathname && !(top->flags & D_DUPPATH)) {
				free(top->pathname);
			}
			top->flags &= ~(D_NOPATH|D_DUPPATH);
			top->pathname = strdup(path ? path : sc->gfile);
		}
		if (rmCset) clearCset(sc, flags, rmCset);
		if (doDates) sccs_fixDates(sc);
		if (merge) {
			if (setMerge(sc, merge, rev) == -1) {
				error = 1;
				goto next;
			}
		}
		if (new_delta) {
			if (IS_EDITED(sc)) {
				was_edited = 1;
				newrev(sc, &pf);
				if (sccs_clean(sc, SILENT)) {
					fprintf(stderr,
					"admin: cannot clean %s\n", sc->gfile);
					goto next;
				}
			} else {
				was_edited = 0;
			}
		}
		if (rev) d = findrev(sc, rev);
		if (sccs_admin(
			    sc, d, flags, encp, compp, f, 0, u, s, m, text)) {
			sccs_whynot("admin", sc);
			error = 1;
		}
		if (touchGfile) touch(sc);
		/*
		 * re init so sccs_get would work
		 */
		if (new_delta && was_edited) {
			int gflags = SILENT|GET_SKIPGET|GET_EDIT;
			char *nrev;

			sccs_free(sc);
			sc = sccs_init(name, init_flags, proj);
			nrev = findrev(sc, pf.newrev) ? pf.newrev: pf.oldrev;
			if (sccs_get(sc, nrev, 0, 0, 0, gflags, "-")) {
				fprintf(stderr, "cannot adjust p file\n");	
			}
		}
next:		sccs_free(sc);
		name = sfileNext();
	}
	sfileDone();
	if (proj) proj_free(proj);
	return (error);
usage:	system("bk help -s admin");
	return (1);
}

private	void
clearCset(sccs *s, int flags, int which)
{
	delta	*d;
	int	name = 0;

	for (d = s->table; d; d = d->next) {
		d->flags &= ~D_CSET;
		unless (which == 2) continue;
		if (d->csetFile && !(d->flags & D_DUPCSETFILE)) {
			if (!name) {
				verbose((stderr,
				    "RM cset from %s\n", s->sfile));
				name = 1;
			}
			verbose((stderr, "\tFILE: %s\n", d->csetFile));
			free(d->csetFile);
			d->csetFile = 0;
		} else {
			d->csetFile = 0;
		}
	}
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
do_checkin(char *name, char *encp, char *compp,
	   int flags, char *rev, char *newfile, char *comment)
{
	delta	*d = 0;
	sccs	*s;
	int	error, enc;
	struct	stat sb;

	unless (s = sccs_init(name, flags, 0)) return (-1);
	if (rev && !streq(rev, "1.1") && s->proj && s->proj->root) {
		fprintf(stderr,
		    "admin: can not specify initial rev for BK files\n");
		sccs_free(s);
		return (-1);
	}
	enc = sccs_encoding(s, encp, compp);
	if (enc == -1) return (-1);

	s->encoding = enc;
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

	unless (d = findrev(sc, rev)) {
		fprintf(stderr, "admin: can't find %s in %s\n",
		    rev, sc->sfile);
		return -1;
	}
	unless (p = findrev(sc, merge)) {
		fprintf(stderr, "admin: can't find %s in %s\n",
		    merge, sc->sfile);
		return -1;
	}
	d->merge = p->serial;
	return 0;
}

private	void
touch(sccs *s)
{
	struct utimbuf ut;

	/* TODO: We need to handle split root config here */
	unless(s->gfile && exists(s->gfile)) return;
	ut.actime = ut.modtime = time(0);
	utime(s->gfile, &ut);
}
