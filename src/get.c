/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");
char	*get_help = "\n\
usage: get [-bdeFgHkmnpqsu] [-c<date>] [-G<name>] \n\
           [-i<revs>] [-r<revs>] [-x<revs>] [files...] OR [-]\n\n\
    A useful thing to note is that\n\
	bk sfiles src | bk get -e -\n\
    will check out all SCCS files for editing.\n\n\
    -b		force a new branch\n\
    -c<date>	specify a date for the get.  The date format is\n\
		yy[mm[dd[hh[mm[ss]]]]] and may be prefixed with either a\n\
		\"+\" or \"-\" to round up/down, respectively.\n\
		The latest delta found before the date is used.\n\
		Symbols may be specified instead of dates, in which case\n\
		the date of the associated revision is used.\n\
    -d		prefix each line with date (not time)\n\
    -D		output a delta as diff(1) style diffs instead of a file\n\
    -DD		output a delta as BK style diffs instead of a file\n\
    -DDD	output a delta as hash (MDBM) style diffs instead of a file\n\
    -e		get file for editing (locked)\n\
    -F		don't check the checksum\n\
    -g		just do locking, don't get the file\n\
    -G<name>	place the output file in <name>\n\
    -i<revs>	include specified revs in the get (rev, rev and/or rev-rev)\n\
    -h		reverse the files sense of hash (turn on if off)\n\
    -k		don't expand keywords\n\
    -m		prefix each line with revision number\n\
    -M<rev>	merge with revision <rev>\n\
    -n		prefix each line with file name\n\
    -N		prefix each line with a line number\n\
    -p		print file to stdout\n\
    -P		print file even if there are file format errors.\n\
    -q		run quietly\n\
    -r<r>	get revision <r>\n\
    -R		revision is part of pathname, i.e., foo.c:1.2\n\
    -s		run quietly\n\
    -T		make output file modification same as\n\
		its corresponding delta\n\
    -u		prefix each line with user id\n\
    -x<revs>	exclude specified list of revs in get (same as -i)\n\n\
    Not implemented:\n\
	    -a, -l (use ``sccslog file.c'' instead)\n\
	    floor/ceiling/locked/user permission checking\n\n";

/*
 * The weird setup is so that I can #include this file into sccssh.c
 */
int
get_main(int ac, char **av, char *out)
{
	sccs	*s;
	int	iflags = 0, flags = GET_EXPAND, c, errors = 0;
	char	*iLst = 0, *xLst = 0, *name, *rev = 0, *cdate = 0, *Gname = 0;
	char	*mRev = 0;
	delta	*d;
	int	gdir = 0;
	int	getdiff = 0;
	int	hasrevs = 0;
	int	dohash = 0;

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, get_help);
		return (1);
	}
	if (streq(av[0], "edit")) flags |= GET_EDIT;
	while ((c = getopt(ac, av, "bc;dDeFgG:hHi;kmM|nNpPqr;RstTux;")) != -1) {
		switch (c) {
		    case 'b': flags |= GET_BRANCH; break;
		    case 'c': cdate = optarg; break;
		    case 'd': flags |= GET_PREFIXDATE; break;
		    case 'D': getdiff++; break;
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
		    case 's': flags |= SILENT; break;
		    case 't': break;	/* compat, noop */
		    case 'T': flags |= GET_DTIME; break;
		    case 'u': flags |= GET_USER; break;
		    case 'x': xLst = optarg; break;

		    default:
usage:			fprintf(stderr, "get: usage error, try get --help\n");
			return (1);
		}
	}
	if (flags & GET_PREFIX) {
		if (flags & GET_EDIT) {
			fprintf(stderr, "get: can't mix -e with -dNum\n");
			return(1);
		}
	}
	if (flags & GET_EDIT) flags &= ~GET_EXPAND;
	name = sfileFirst("get", &av[optind], hasrevs);
	gdir = Gname && isdir(Gname);
	if (Gname && (flags & GET_EDIT)) {
		fprintf(stderr, "get: can't edit and rename at same time.\n");
		return (1);
	}
	if ((Gname && !isdir(Gname)) || iLst || xLst) {
		if (sfileNext()) {
			fprintf(stderr,
			    "%s: only one file name with -G/i/x.\n", av[0]);
			goto usage;
		}
	}
	if (flags & GET_PATH) {
		if (Gname) {
			fprintf(stderr,
			    "get: can't use -G and -H at the same time.\n");
			return (1);
		}
		if (flags & (GET_EDIT|PRINT)) {
			fprintf(stderr,
			    "get: can't use -e|p and -H at the same time.\n");
			return (1);
		}
	}
	// 	 make sure -G -p is not used at the same time
	if ((rev || cdate) && hasrevs) {
		fprintf(stderr, "get: can't specify more than one rev.\n");
		return (1);
	}
	switch (getdiff) {
	    case 0: break;
	    case 1: flags |= GET_DIFFS; break;
	    case 2: flags |= GET_BKDIFFS; break;
	    case 3: flags |= GET_HASHDIFFS; break;
	    default:
		fprintf(stderr, "get: invalid D flag value %d\n", getdiff);
		return(1);
	}
	if (getdiff && (flags & GET_PREFIX)) {
		fprintf(stderr, "get: -D and prefixes not supported\n");
		return(1);
	}
		
	for (; name; name = sfileNext()) {
		unless (s = sccs_init(name, iflags, 0)) continue;
		if (Gname) {
			if (gdir) {
				char	buf[1024];

				sprintf(buf, "%s/%s", Gname, basenm(s->gfile));
				free(s->gfile);
				s->gfile = strdup(buf);
				s = check_gfile(s, 0);
				assert(s);
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
		if ((flags & (GET_DIFFS|GET_BKDIFFS|GET_HASHDIFFS))
		    ? sccs_getdiffs(s, rev, flags, out)
		    : sccs_get(s, rev, mRev, iLst, xLst, flags, out)) {
			unless (BEEN_WARNED(s)) {
				fprintf(stderr,
				    "get of %s failed, skipping it.\n", name);
			}
			errors = 1;
		}
		sccs_free(s);
	}
	sfileDone();
#ifndef	NOPURIFY
	purify_list();
#endif
	return (errors);
}

int
main(int ac, char **av)
{
	return (get_main(ac, av, "-"));
}
