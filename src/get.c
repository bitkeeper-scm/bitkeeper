/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");
char	*get_help = "\n\
usage: get [-bdeFgkmnpqsu] [-c<date>] [-G<name>] \n\
           [-i<revs>] [-r<revs>] [-x<revs>] [files...] OR [-]\n\n\
    A useful thing to note is that\n\
	sfiles src | get -e\n\
    will check out all SCCS files for editing.\n\n\
    -b		force a new branch\n\
    -c<date>	specify a cutoff date for the get.  The date format is\n\
		yy[mm[dd[hh[mm[ss]]]]] and may be prefixed or postfixed\n\
		with either \".\" or \",\".  See range(1) for date formats.\n\
		Symbols may be specified instead of dates, in which case\n\
		the date of the associated revision is used.\n\
    -d		prefix each line with date (not time)\n\
    -D		output a delta instead of a file\n\
    -e		get file for editing (locked)\n\
    -F		don't check the checksum\n\
    -g		just do locking, don't get the file\n\
    -G<name>	place the output file in <name>\n\
    -i<revs>	include specified revs in the get (rev, rev and/or rev-rev)\n\
    -k		don't expand keywords\n\
    -m		prefix each line with revision number\n\
    -n		prefix each line with file name\n\
    -N		prefix each line with a line number\n\
    -p		print file to stdout\n\
    -P		print file even if there are file format errors.\n\
    -q		run quietly\n\
    -r<r>	get revision <r>\n\
    -s		run quietly\n\
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
	int	flags = EXPAND, c, errors = 0;
	char	*iLst = 0, *xLst = 0, *name, *rev = 0, *cdate = 0, *Gname = 0;
	delta	*d;
	int	gdir = 0;
	int	getdiff = 0;

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, get_help);
		return (1);
	}
	if (streq(av[0], "edit")) flags |= EDIT;
	while ((c = getopt(ac, av, "bc;dDeFgG:i;kmnNpPqr;stux;")) != -1) {
		switch (c) {
		    case 'b': flags |= FORCEBRANCH; break;
		    case 'c': cdate = optarg; break;
		    case 'd': flags |= PREFIXDATE; break;
		    case 'D': getdiff = 1; break;
		    case 'e': flags |= EDIT; break;
		    case 'F': flags |= NOCKSUM; break;
		    case 'g': flags |= SKIPGET; break;
		    case 'G': Gname = optarg; break;
		    case 'i': iLst = optarg; break;
		    case 'k': flags &= ~EXPAND; break;
		    case 'm': flags |= REVNUMS; break;
		    case 'n': flags |= MODNAME; break;
		    case 'N': flags |= LINENUM; break;
		    case 'p': flags |= PRINT; break;
		    case 'P': flags |= PRINT|FORCE; break;
		    case 'q': flags |= SILENT; break;
		    case 'r': rev = optarg; break;
		    case 's': flags |= SILENT; break;
		    case 't': break;	/* compat, noop */
		    case 'u': flags |= USER; break;
		    case 'x': xLst = optarg; break;

		    default:
usage:			fprintf(stderr, "get: usage error, try get --help\n");
			return (1);
		}
	}
	if (flags & (PREFIXDATE|REVNUMS|USER|LINENUM)) flags &= ~EDIT;
	if (flags & EDIT) flags &= ~EXPAND;
	name = sfileFirst("get", &av[optind], 0);
	gdir = Gname && isdir(Gname);
	if (Gname && (flags & EDIT)) {
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
	for (; name; name = sfileNext()) {
		unless (s = sccs_init(name, flags)) continue;
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
			if (!(s->state & SFILE)) {
				fprintf(stderr, "co: %s doesn't exist.\n",
				    s->sfile);
			} else {
				perror(s->sfile);
			}
			sccs_free(s);
			continue;
		}
		if (cdate) {
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
		if (getdiff
		    ? sccs_getdiffs(s, rev, flags, out)
		    : sccs_get(s, rev, iLst, xLst, flags, out)) {
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
