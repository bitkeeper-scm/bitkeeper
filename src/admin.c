/* Copyright (c) 1998 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");

private	char	*help = "\n\
usage: admin options [-] OR [file file file...]\n\n\
    A very useful thing to note is that\n\
	    bk sfiles | bk admin -what -ever -\n\
    works.  Note the trailing \"-\" which says to read files from stdin.\n\n\
    -a<u>|<g>		add user/group\n\
    -A<u>|<g>		delete user/group\n\n\
    -b			force binary encoding (uuencode)\n\
    -B			make the landing pad bigger\n\
    -C			remove the changeset information\n\
    -d<f>		delete flag (ATT compat)\n\
    -e<u>|<g>		delete user/group (ATT compat)\n\
    -f<f><val>		add flag (value is optional)\n\
    -F<f>		delete flag\n\n\
    -g			force gzipped binary encoding (gzip | uuencode)\n\
    -h			check s.file format for correctness\n\
    -H			check the s.file format, insisting on 7 bit ascii.\n\
    -i<file>		initial text in <file> or stdin for -n\n\
    -l<a|r, r>		unlock releases (not implemented)\n\
    -L<lod>:<rev>	creat a new LOD parented at <rev>\n\
    -m<mode>		set the mode of the file\n\
    -M<merge>		Merge branch <merge> into TOT or <rev>\n\
    -n			create a new SCCS file\n\
    -p<path>		set the initial pathname of the file to <path>\n\
    -P			remove all pathname information (DANGEROUS)\n\
    -q			run quietly\n\
    -r<rev>		revision number of new file or effected delta\n\
    -s<sym>:<rev>	set symbol associated with rev, no rev means TOT\n\
    -t<file>		set or (if no file) delete descriptive text\n\
    -T			delete descriptive text\n\
    -u			make sure that all dates are increasing\n\
    -y<com>		initial checkin comment\n\
    -z			recompute checksum\n\n";

#define	OP(W, V, F) if (next##W < A_SZ-1) { \
			W[next##W].thing = V; \
			W[next##W++].flags = F; \
		  } else { /* CSTYLED */ \
			fprintf(stderr, "admin: argument overflow.\n"); \
			exit(1); \
		    }

int	do_checkin(char *nm, int enc, int fl, char *rev, char *newf, char *com);
void	clearCset(sccs *s, int flags);
void	clearPath(sccs *s, int flags);
int	setMerge(sccs *sc, char *merge, char *rev);

int
main(int ac, char **av, char **ev)
{
	sccs	*sc;
	int	flags = 0;
	char	*rev = 0;
	int	c;
	admin	f[A_SZ], l[A_SZ], u[A_SZ], s[A_SZ];
	int	nextf = 0, nextl = 0, nextu = 0, nexts = 0, nextp = 0;
	char	*comment = 0, *text = 0, *newfile = 0;
	char	*path = 0, *merge = 0;
	char	*name;
	int	encoding = E_ASCII, error = 0;
	int	bigpad = 0;
	int	fastSymOK = 1, fastSym, dopath = 0, rmCset = 0, rmPath = 0;
	int	doDates = 0;
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
	    getopt(ac, av, "a;A;e;f;F;d;i|L;m;M;np|r;y|s;STt|BbCghHPquz")) != -1) {
		switch (c) {
		/* user|group */
		    case 'a':	OP(u, optarg, A_ADD); break;
		    case 'A':
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
		    		flags |= SHUTUP|NEWCKSUM;
				dopath++;
				break;
		/* Changeset info */
		    case 'C':	rmCset = 1; flags |= NEWCKSUM; break;
		/* LOD's */
		    case 'L':	OP(l, optarg, A_ADD); break;
		/* Pathname info */
		    case 'P':	rmPath = 1; flags |= NEWCKSUM; break;
		/* symbols */
		    case 's':	OP(s, optarg, A_ADD); break;
		/* text */
		    case 't':	text = optarg ? optarg : ""; break;
		    case 'T':	text = ""; break;
		/* singletons */
		    case 'B':	bigpad++; break;
		    case 'b':	encoding = E_UUENCODE; break;
		    case 'g':	encoding = E_UUGZIP; break;
		    case 'h':	flags |= VERBOSE|CHECKFILE; break;
		    case 'H':
			flags |= VERBOSE|CHECKASCII|CHECKFILE;
			break;
		    case 'S':	fastSymOK = 0; break;
		    case 'q':	flags |= SILENT; break;
		    case 'u':	doDates = 1; flags |= NEWCKSUM; break;
		    case 'z':	flags |= NOCKSUM|NEWCKSUM; break;
		    default:	fprintf(stderr, "admin: bad option %c.\n", c);
				goto usage;
		}
	}
	if ((flags & CHECKFILE) &&
	    ((flags & ~(CHECKASCII|CHECKFILE|VERBOSE|SILENT)) ||
	    nextf || nextu || nexts || nextp || rev)) {
		fprintf(stderr, "admin: -h option must be alone.\n");
		goto usage;
	}
	if ((merge) &&
	    ((flags & ~(CHECKASCII|CHECKFILE|VERBOSE|SILENT|NEWCKSUM)) ||
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
	fastSym = !(flags & ~SILENT) && !nextf && !nextu && !nextp &&
	    !rev && nexts && (s[0].flags == A_ADD) && !s[1].flags &&
	    fastSymOK;
	if (fastSym) flags |= (MAP_WRITE|NOCKSUM);
	while (name) {
		if (flags & NEWFILE) {
			if (do_checkin(name, encoding, flags&(SILENT|NEWFILE),
			    rev, newfile, comment)) {
				error  = 1;
				name = sfileNext();
				continue;
			}
		}
		sc = sccs_init(name, flags);
		unless (sc) { name = sfileNext(); continue; }
		unless (sc->tree) {
			fprintf(stderr, "admin: can't read delta table in %s\n",
			    sc->sfile);
			sccs_free(sc);
			name = sfileNext();
			error = 1;
			continue;
		}
		if (bigpad) sc->state |= BIGPAD;
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
		if (rmCset) clearCset(sc, flags);
		if (rmPath) clearPath(sc, flags);
		if (doDates) sccs_fixDates(sc);
		if (m) {
			delta	*d;
			
			sc->state |= RANGE2;
			if (d = sccs_getrev(sc, rev, 0, 0)) d = modeArg(d, m);
		}
		if (merge) {
			if (setMerge(sc, merge, rev) == -1) {
				error = 1;
				goto next;
			}
		}
		if (sccs_admin(sc, flags, f, l, u, s, text)) {
			unless (BEEN_WARNED(sc)) {
				fprintf(stderr,
				    "admin of %s failed.\n",
				    sc->gfile);
			}
			error = 1;
		}
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
clearCset(sccs *s, int flags)
{
	delta	*d;
	int	name = 0;

	for (d = s->table; d; d = d->next) {
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
		if (d->cset) {
			if (!name) {
				verbose((stderr,
				    "RM cset from %s\n", s->sfile));
				name = 1;
			}
			verbose((stderr, "\tCSET: %s\n", d->cset));
			free(d->cset);
			d->cset = 0;
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
do_checkin(char *name, int encoding,
	int flags, char *rev, char *newfile, char *comment)
{
	delta	*d = 0;
	sccs	*s;
	int	error;

	unless (s = sccs_init(name, flags)) { return (-1); }
	s->encoding = encoding;
	if (HAS_SFILE(s)) {
		fprintf(stderr, "admin: %s exists.\n", s->sfile);
		sccs_free(s);
		return (1);
	}

	/*
	 * If they specified a gfile and it is different than the implied
	 * gfile, and the implied gfile exists, that's too weird, fail it.
	 */
	if ((s->state & GFILE) && newfile && strcmp(newfile, s->gfile)) {
		fprintf(stderr,
		    "admin: gfile %s exists and is "
		    "different than specified init file %s\n",
		    s->gfile, newfile);
		sccs_free(s);
		return (-1);
	}

	if (newfile) {
		struct	stat sb;

		if (stat(newfile, &sb) == 0) {
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
		flags |= EMPTY;
	}
	s->state |= GFILE;
	if (comment) { d = sccs_parseArg(d, 'C', comment, 0); }
	if ((error = sccs_delta(s, flags|SAVEGFILE, d, 0, 0))) {
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
