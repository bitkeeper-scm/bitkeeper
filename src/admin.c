/* Copyright (c) 1998 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

static const char help[] = "\n\
usage: admin options [- | file file file...]\n\
\n\
    -q			run quietly\n\
    -r<rev>		revision to add or modify\n\
    -y<comment>		comment for change\n\
    -n			create new SCCS history file\n\
    -i[<file>]		read initial text from <file> (default stdin)\n\
    -t[<file>]		read description from <file>\n\
    -T			clear description\n\
    -h			check s.file structure\n\
    -H			same as -h, plus check file contents are ASCII\n\
    -z			recalculate file checksum\n\
\n\
    -f<f><val>		set flag (value is optional)\n\
    -F<f>		delete flag\n\
    -d<f>		delete flag (ATT compat)\n\
\n\
    -m<mode>		set the mode of the file\n\
    -M<merge>		Merge branch <merge> into TOT or <rev>\n\
    -L<lod>:<rev>	create a new LOD parented at <rev>\n\
    -S<sym>:<rev>	associate <sym> with <rev>\n\
    -p<path>:<rev>	set/change path of <rev> to <path>\n\
    -P<rev>		revert to default path for <rev>\n\
\n\
    -B			make the landing pad bigger\n\
    -C			remove the changeset marks\n\
    -CC			remove the changeset marks and pointer\n\
    -Z[alg]		compress stored s.file with <alg>, which may be:\n\
		gzip	like gzip(1) (default)\n\
		none	no compression\n\
    -E[enc]		treat file as encoded with <enc>, which may be:\n\
		text	plain text\n\
		ascii	same\n\
		binary	binary file (must uuencode before diffing)\n\
		uugzip	same, but compress before uuencode\n\
    -u			make sure that all dates are increasing\n\
			(dangerous, this changes the keys)\n\
\n\
    -a<u>|<g>		add user/group (ATT compat)\n\
    -e<u>|<g>		delete user/group (ATT compat)\n";


#define	OP(W, V, F) if (next##W < A_SZ-1) { \
			W[next##W].thing = V; \
			W[next##W++].flags = F; \
		  } else { /* CSTYLED */ \
			fprintf(stderr, "admin: argument overflow.\n"); \
			exit(1); \
		    }

int	do_checkin(char *nm, char *ep, char *cp, int fl,
		   char *rev, char *newf, char *com);
void	clearCset(sccs *s, int flags, int which);
void	clearPath(sccs *s, int flags);
void	touch(sccs *s);
int	setMerge(sccs *sc, char *merge, char *rev);

int
main(int ac, char **av)
{
	sccs	*sc;
	int	flags = 0;
	int	init_flags = 0;
	char	*rev = 0;
	int	c;
	admin	f[A_SZ], l[A_SZ], u[A_SZ], s[A_SZ];
	int	nextf = 0, nextl = 0, nextu = 0, nexts = 0, nextp = 0;
	char	*comment = 0, *text = 0, *newfile = 0;
	char	*path = 0, *merge = 0;
	char	*name;
	char	*encp = 0, *compp = 0;
	int	error = 0;
	int	bigpad = 0;
	int	fastSym = 0, dopath = 0, rmCset = 0, rmPath = 0;
	int	doDates = 0, touchGfile = 0;
	char	*m = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		fprintf(stderr, "%s", help);
		return (1);
	}
	bzero(f, sizeof(f));
	bzero(u, sizeof(u));
	bzero(s, sizeof(s));
	bzero(l, sizeof(l));
	while ((c =
	    getopt(ac, av, "a;e;f;F;d;i|nr;y|M;m;p|PZ|E;L|S;t|TBChHsquz"))
	       != -1) {
		switch (c) {
		/* user|group */
		    case 'a':	OP(u, optarg, A_ADD); break;
		    case 'e':	OP(u, optarg, A_DEL); break;
		/* flags */
		    case 'f':	OP(f, optarg, A_ADD); break;
		    case 'F':
		    case 'd':	OP(f, optarg, A_DEL); break;
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
		    case 'P':	rmPath = 1; flags |= NEWCKSUM; break;
		/* encoding and compression */
		    case 'Z':	compp = optarg ? optarg : "gzip";
				flags |= NEWCKSUM;
				touchGfile++;
				break;
		    case 'E':	encp = optarg; break;
		/* LOD's */
		    case 'L':	OP(l, optarg, A_ADD); break;
		/* symbols */
		    case 'S':	OP(s, optarg, A_ADD); break;
		/* text */
		    case 't':	text = optarg ? optarg : ""; break;
		    case 'T':	text = ""; break;
		/* singletons */
		    case 'B':	bigpad++; break;
		    case 'C':	rmCset++; flags |= NEWCKSUM; break;
		    case 'h':	flags |= ADMIN_FORMAT; break;
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
	if ((flags & ADMIN_FORMAT) &&
	    ((flags & ~(ADMIN_FORMAT|ADMIN_ASCII|ADMIN_TIME|SILENT)) ||
	    nextf || nextu || nexts || nextp || rev)) {
		fprintf(stderr, "admin: -h option must be alone.\n");
		goto usage;
	}
	if ((merge) &&
	    ((flags & ~(ADMIN_FORMAT|ADMIN_ASCII|ADMIN_TIME|SILENT|NEWCKSUM)) ||
	    nextf || nextu || nexts || nextp || comment || path || 
	    rmCset || rmPath || doDates)) {
		fprintf(stderr, "admin: -M option must be alone or with -r\n");
		goto usage;
	}
	if (comment && !(flags & NEWFILE)) {
		fprintf(stderr,
		    "admin: comment may only be specifed with -i and/or -n\n");
		goto usage;
	}
	if (rev && (!(flags & NEWFILE) && !merge && !m)) {
		fprintf(stderr, "%s %s\n",
		    "admin: revision may only be specified with",
		    "-i and/or -n or -M or -m\n");
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
	/*
	 * If we are adding exactly one symbol, do it quickly.
	 */
#if 0 /* Broken because of permission checking in init */
	fastSym = !(flags & ~SILENT) && !nextf && !nextu && !nextp &&
	    !rev && nexts && (s[0].flags == A_ADD) && !s[1].flags;
	if (fastSym) init_flags |= (INIT_MAPWRITE|INIT_NOCKSUM);
#endif
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
		sc = sccs_init(name, init_flags, 0);
		unless (sc) { name = sfileNext(); continue; }
		unless (sc->tree) {
			fprintf(stderr,
				"admin: can't read delta table in %s\n",
			    sc->sfile);
			sccs_free(sc);
			name = sfileNext();
			error = 1;
			continue;
		}
		if (bigpad) sc->state |= S_BIGPAD;
#ifndef	USE_STDIO
		if (fastSym && sc->landingpad) {
			int rc;
		    	rc = sccs_addSym(sc, flags, s[0].thing);
			if (rc == -1) error = 1;
			if (rc != EAGAIN) goto next;
			
		}
#endif
		if (dopath) {
			if (sc->tree->pathname) {
				verbose((stderr,
				    "%s has a path already.\n", sc->sfile));
			} else {
				sc->tree->pathname = 
				    strdup(path ? path : sc->gfile);
			}
		}
		if (rmCset) clearCset(sc, flags, rmCset);
		if (rmPath) clearPath(sc, flags);
		if (doDates) sccs_fixDates(sc);
		if (m) {
			delta	*d;
			
			sc->state |= S_RANGE2;
			if (d = sccs_getrev(sc, rev, 0, 0)) d = modeArg(d, m);
		}
		if (merge) {
			if (setMerge(sc, merge, rev) == -1) {
				error = 1;
				goto next;
			}
		}
		if (sccs_admin(sc, flags, encp, compp, f, l, u, s, text)) {
			sccs_whynot("admin", sc);
			error = 1;
		}
		if (touchGfile) touch(sc);
next:		sccs_free(sc);
		name = sfileNext();
	}
	sfileDone();
	purify_list();
	return (error);
usage:	fprintf(stderr, "admin: usage error, try `admin --help' for info.\n");
	return (1);
}

void
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

void
clearPath(sccs *s, int flags)
{
	delta	*d;
	int	name = 0;

	for (d = s->table; d; d = d->next) {
		if (d->pathname && !(d->flags & D_DUPPATH)) {
			if (!name) {
				verbose((stderr,
				    "RM paths from %s\n", s->sfile));
				name = 1;
			}
			verbose((stderr, "\tPATH: %s\n", d->pathname));
			free(d->pathname);
			d->pathname = 0;
		} else {
			d->pathname = 0;
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
int
do_checkin(char *name, char *encp, char *compp,
	   int flags, char *rev, char *newfile, char *comment)
{
	delta	*d = 0;
	sccs	*s;
	int	error, enc;

	unless (s = sccs_init(name, flags, 0)) return (-1);
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

	if (newfile) {
		struct	stat sb;

		if (lstat(newfile, &sb) == 0) {
			if (S_ISLNK(sb.st_mode) ||
			    S_ISREG(sb.st_mode) ||
			    S_ISDIR(sb.st_mode)) {
				s->mode = sb.st_mode;
			} else {
				verbose((stderr,
				    "admin: ignoring modes on %s\n", newfile));
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
	if ((error = sccs_delta(s, flags|DELTA_SAVEGFILE, d, 0, 0))) {
		unless (BEEN_WARNED(s)) {
			fprintf(stderr, "admin: failed to check in %s.\n",
			    s->sfile);
		}
	}
	sccs_free(s);
	return (error);
}

int
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

void
touch(sccs *s) 
{
	struct utimbuf ut;

	/* TODO: We need to handle split root config here */
	unless(s->gfile && exists(s->gfile)) return;
	ut.actime = ut.modtime = time(0);
	utime(s->gfile, &ut);
}
