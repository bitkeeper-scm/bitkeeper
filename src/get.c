/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * The weird setup is so that I can #include this file into sccssh.c
 */
int
_get_main(int ac, char **av, char *out)
{
	sccs	*s;
	int	iflags = INIT_SAVEPROJ, flags = GET_EXPAND, c, errors = 0;
	char	*iLst = 0, *xLst = 0, *name, *rev = 0, *cdate = 0, *Gname = 0;
	char	*mRev = 0;
	delta	*d;
	int	gdir = 0;
	int	getdiff = 0;
	int	hasrevs = 0;
	int	dohash = 0;
	int	commitedOnly = 0;
	int	branch_ok = 0;
	project	*proj = 0;
	MDBM	*realNameCache = 0;
	char	realname[MAXPATH];

	debug_main(av);
	name = strrchr(av[0], '/');
	if (name) name++;
	else name = av[0];
	if (streq(name, "co")) {
		if (!isdir("SCCS") && isdir("RCS")) {
			rcs("co", ac, av);
			/* NOTREACHED */
		}
	} else if (streq(name, "edit")) {
		flags |= GET_EDIT;
	} else if (streq(name, "_get")) {
		branch_ok = 1;
	}

	if (ac == 2 && streq("--help", av[1])) {
		sprintf(realname, "bk help %s", name);
		system(realname);
		return (1);
	}
	if (streq(av[0], "edit")) flags |= GET_EDIT;
	while ((c =
	    getopt(ac, av, "ac;CdDeFgG:hHi;klmM|nNpPqr;RSstTux;")) != -1) {
		switch (c) {
		    case 'a': flags |= GET_ALIGN; break;
		    case 'c': cdate = optarg; break;
		    case 'C': commitedOnly = 1; break;
		    case 'd': flags |= GET_PREFIXDATE; break;
		    case 'D': getdiff++; break;
		    case 'l':	/* undoc in get, doc-ed in co */
		    case 'e': flags |= GET_EDIT; break;
		    case 'F': iflags |= INIT_NOCKSUM; break;
		    case 'g': flags |= GET_SKIPGET; break;
		    case 'G': Gname = optarg; break;
		    case 'h': dohash = 1; break;
		    case 'H': flags |= GET_PATH; break;
		    case 'i': iLst = optarg; break;
		    case 'k': flags &= ~GET_EXPAND; break;
		    case 'm': flags |= GET_REVNUMS; break;
		    case 'M': mRev = optarg; break;
		    case 'n': flags |= GET_MODNAME; break;
		    case 'N': flags |= GET_LINENUM; break;
		    case 'p': flags |= PRINT; break;
		    case 'P': flags |= PRINT|GET_FORCE; break;
		    case 'q': flags |= SILENT; break;
		    case 'r': rev = optarg; break;
		    case 'R': hasrevs = SF_HASREVS; break;
		    case 's': flags |= SILENT; break;	/* undoc */
		    case 'S': flags |= GET_NOREGET; break;
		    case 't': break;	/* compat, noop, undoc */
		    case 'T': flags |= GET_DTIME; break;
		    case 'u': flags |= GET_USER; break;
		    case 'x': xLst = optarg; break;

		    default:
usage:			sprintf(realname, "bk help -s %s", name);
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
	if (flags & GET_EDIT) flags &= ~GET_EXPAND;
	name = sfileFirst("get", &av[optind], hasrevs);
	gdir = Gname && isdir(Gname);
	if (Gname && (flags & GET_EDIT)) {
		fprintf(stderr, "%s: can't use -G and -e/-l together.\n",
			av[0]);
		goto usage;
	}
	if (Gname && (flags & GET_PATH)) {
		fprintf(stderr, "%s: can't use -G and -H together.\n",
			av[0]);
		goto usage;
	}
	if (Gname && (flags & PRINT)) {
		fprintf(stderr, "%s: can't use -G and -p together,\n",
			av[0]);
		goto usage;
	}
	if (((Gname && !isdir(Gname)) || iLst || xLst) && sfileNext()) {
		fprintf(stderr,
			"%s: only one file name with -G/i/x.\n", av[0]);
		goto usage;
	}
	if ((flags & GET_PATH) && (flags & (GET_EDIT|PRINT))) {
		fprintf(stderr, "%s: can't use -e/-l/-p and -H together.\n",
			av[0]);
		goto usage;
	}
	if (commitedOnly && (flags & GET_EDIT)) {
		fprintf(stderr,
		    "%s: -C can not be combined with edit.\n", av[0]);
		goto usage;
	}
	if (commitedOnly && (rev || cdate)) {
		fprintf(stderr,
		    "%s: -C can not be combined with rev/date.\n", av[0]);
		goto usage;
	}
	if ((rev || cdate) && hasrevs) {
		fprintf(stderr, "%s: can't specify more than one rev.\n",
			av[0]);
		goto usage;
	}
	switch (getdiff) {
	    case 0: break;
	    case 1: flags |= GET_DIFFS; break;
	    case 2: flags |= GET_BKDIFFS; break;
	    case 3: flags |= GET_HASHDIFFS; break;
	    default:
		fprintf(stderr, "%s: invalid D flag value %d\n",
			av[0], getdiff);
		return(1);
	}
	if (getdiff && (flags & GET_PREFIX)) {
		fprintf(stderr, "%s: -D and prefixes not supported\n", av[0]);
		return(1);
	}

	realNameCache = mdbm_open(NULL,0, 0, GOOD_PSIZE);
	assert(realNameCache);
	for (; name; name = sfileNext()) {
		getRealName(name, realNameCache, realname);
		unless (s = sccs_init(realname, iflags, proj)) continue;
		unless (proj) proj = s->proj;
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
		if (!s->tree) {
			if (!(s->state & S_SFILE)) {
				fprintf(stderr, "co: %s doesn't exist.\n",
				    s->sfile);
			} else {
				perror(s->sfile);
			}
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
		}
		if (dohash) {
			if (s->state & S_HASH) {
				s->state &= ~S_HASH;
			} else {
				s->state |= S_HASH;
			}
		}
		if (hasrevs) rev = sfileRev();
		if (commitedOnly) {
			delta	*d = sccs_top(s);

			while (d && !(d->flags & D_CSET)) d = d->parent;
			if (!d) {
				verbose((stderr,
				    "No committed deltas in %s\n", s->gfile));
				errors = 1;
				sccs_free(s);
				continue;
			}
			rev = d->rev;
		}
		if ((s->state & S_BITKEEPER) &&
		    (flags & GET_EDIT) && rev && !branch_ok) {
			fprintf(stderr,
			    "Do not use -r to create branch, "
			    "use \"bk setlod\"\n");
			errors = 1;
			sccs_free(s);
			continue;
			
		}
		if ((flags & (GET_DIFFS|GET_BKDIFFS|GET_HASHDIFFS))
		    ? sccs_getdiffs(s, rev, flags, out)
		    : sccs_get(s, rev, mRev, iLst, xLst, flags, out)) {
			unless (BEEN_WARNED(s)) {
				verbose((stderr,
				    "get of %s failed, skipping it.\n", name));
			}
			errors = 1;
		}
		sccs_free(s);
	}
	sfileDone();
	if (proj) proj_free(proj);
	if (realNameCache) mdbm_close(realNameCache);
	return (errors);
}

int
get_main(int ac, char **av)
{
	return (_get_main(ac, av, "-"));
}
