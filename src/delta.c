/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");
char	*delta_help = "\n\
usage: delta [-iluYpq] [-S<sym>] [-Z<alg>] [-y<c>] [files...]\n\n\
   -a		check in new work automatically\n\
   -c		don't verify file checksum\n\
   -D<file>	take diffs from <file>\n\
   -E		set file encoding (like admin)\n\
   -f		force ci of null delta - default when invoked as delta\n\
   -g		obsolete, SCCS compat\n\
   -h		invert sense of file's hash flag\n\
   -i		initial checkin, create a new revision history\n\
   -I<file>	use init file\n\
   -l		follow checkin with a locked checkout like ``get -e''\n\
   -m		(delta) obsolete, SCCS compat;  (ci) same as -y\n\
   -n		preserve gfile, kill pfile; SCCS compat\n\
   -p		print differences before prompting for comments.\n\
   -q		run silently.\n\
   -r		obsolete, SCCS compat\n\
   -R		respect rev (?)\n\
   -s		same as -q\n\
   -S<sym>	set the symbol <sym> to be the revision created\n\
   -u		follow checkin with an unlocked checkout like ``get''\n\
   -Y		prompt for one comment, then use it for all the files.\n\n\
   -y<comment>	sets the revision comment to <comment>.\n\
   -Z, -Z<alg>	compress stored s.file with <alg>, which may be:\n\
		gzip	like gzip(1) (default)\n\
		none	no compression\n";

#include "comments.c"
int	newrev(sccs *s, pfile *pf);

extern void rcs(char *cmd, int ac, char **av) NORETURN;

int
main(int ac, char **av)
{
	sccs	*s;
	int	iflags = 0;
	int	dflags = 0;
	int	gflags = 0;
	int	sflags = SF_GFILE|SF_WRITE_OK;
	int	isci = 0;
	int	checkout = 0;
	int	c, rc, enc;
	char	*initFile = 0;
	char	*diffsFile = 0;
	char	*name;
	char	*sym = 0;
	char	*compp = 0, *encp = 0;
	MMAP	*diffs = 0;
	MMAP	*init = 0;
	pfile	pf;

	debug_main(av);
	name = strrchr(av[0], '/');
	if (name) name++;
	else name = av[0];
	if (streq(name, "ci")) {
		if (!isdir("SCCS") && isdir("RCS")) {
			rcs("ci", ac, av);
			/* NOTREACHED */
		}
		isci = 1;
	} else if (streq(name, "delta")) {
		dflags = DELTA_FORCE;
	}

	if (ac > 1 && streq("--help", av[1])) {
help:		fputs(delta_help, stderr);
		return (1);
	}
	while ((c = getopt(ac, av,
			   "acD:E|fg;GhI;ilm|npqRrS;suy|YZ|")) != -1) {
		switch (c) {
		    /* SCCS flags */
		    case 'n': dflags |= DELTA_SAVEGFILE; break;
		    case 'p': dflags |= PRINT; break;
		    case 'y':
		    comment:
			comment = optarg;
			gotComment = 1;
			dflags |= DELTA_DONTASK;
			break;
		    case 's': /* fall through */

		    /* RCS flags */
		    case 'q': dflags |= SILENT; gflags |= SILENT; break;
		    case 'f': dflags |= DELTA_FORCE; break;
		    case 'i': dflags |= NEWFILE;
			      sflags |= SF_NODIREXPAND;
			      break;
		    case 'l': gflags |= GET_SKIPGET|GET_EDIT;
		    	      dflags |= DELTA_SAVEGFILE;
			      checkout = 1;
			      break;
		    case 'u': gflags |= GET_EXPAND;
			      checkout = 1;
			      break;

		    /* flags with different meaning in RCS and SCCS */
		    case 'm':
			    if (isci) goto comment;
			    /* else fall through */

		    /* obsolete SCCS flags */
		    case 'g':
		    case 'r':
			    fprintf(stderr, "-%c not implemented.\n", c);
			    goto help;

		    /* LM flags */
		    case 'a':
		    	dflags |= DELTA_AUTO;
			dflags &= ~DELTA_FORCE;
			break;
		    case 'c': iflags |= INIT_NOCKSUM; break;
		    case 'D': diffsFile = optarg;
			      sflags = ~(SF_GFILE | SF_WRITE_OK);
			      break;
		    case 'G': iflags |= INIT_GTIME; break;
		    case 'h': dflags |= DELTA_HASH; break;
		    case 'I': initFile = optarg; break;
		    case 'R': dflags |= DELTA_PATCH; break;
		    case 'S': sym = optarg; break;
		    case 'Y': dflags |= DELTA_DONTASK; break;
		    case 'Z': compp = optarg ? optarg : "gzip"; break;
		    case 'E': encp = optarg; break;

		    default:
usage:			fprintf(stderr, "%s: usage error, try --help.\n",
				av[0]);
			return (1);
		}
	}
	enc = sccs_encoding(0, encp, compp);
	if (enc == -1) goto usage;

	name = sfileFirst(av[0], &av[optind], sflags);
	/* They can only have an initFile for one file...
	 * So we go get the next file and error if there
	 * is one.
	 */
	if ((initFile || diffsFile) && name && sfileNext()) {
		fprintf(stderr,
"%s: only one file may be specified with init or diffs file.\n", av[0]);
		goto usage;
	}
	if (initFile && (dflags & DELTA_DONTASK)) {
		fprintf(stderr,
		    "%s: only init file or comment, not both.\n", av[0]);
		goto usage;
	}
	if ((gflags & GET_EXPAND) && (gflags & GET_EDIT)) {
		fprintf(stderr, "%s: -l and -u are mutually exclusive.\n",
			av[0]);
		goto usage;
	}
	if (diffsFile && !(diffs = mopen(diffsFile))) {
		fprintf(stderr, "%s: diffs file '%s': %s.\n",
			av[0], diffsFile, strerror(errno));
	       return (1);
	}
	if (initFile && !(init = mopen(initFile))) {
		fprintf(stderr,"%s: init file '%s': %s.\n",
			av[0], initFile, strerror(errno));
		return (1);
	}

	while (name) {
		delta	*d = 0;
		char	*nrev;

		if (dflags & DELTA_DONTASK) {
			unless (d = getComments(0)) goto usage;
		}
		unless (s = sccs_init(name, iflags, 0)) {
			if (d) sccs_freetree(d);
			name = sfileNext();
			continue;
		}
		if (dflags & DELTA_AUTO) {
			if (HAS_SFILE(s)) {
				dflags &= ~NEWFILE;
			} else {
				dflags |= NEWFILE;
			}
		}

		if (sym) {
			if (!d) d = calloc(1, sizeof(*d));
			d->sym = strdup(sym);
		}
		nrev = NULL;
		unless (dflags & NEWFILE) {
			if (checkout && (newrev(s, &pf) == -1)) {
				goto next;
			}
			nrev = pf.newrev;
		}
		s->encoding = sccs_encoding (s, encp, compp);
		rc = sccs_delta(s, dflags, d, init, diffs);
		if (rc == -2) goto next; /* no diff in file */
		if (rc == -1) {
			sccs_whynot("delta", s);
			if (init) mclose(init);
			sccs_free(s);
			commentsDone(saved);
			sfileDone();
			purify_list();
			return (1);
		}
		if (checkout) {
			s = sccs_restart(s);
			unless (s) {
				fprintf(stderr,
				    "%s: can't restart %s\n", av[0], name);
				goto next;
			}
			if (rc == -3) nrev = pf.oldrev;
			if (sccs_get(s, nrev, 0, 0, 0, gflags, "-")) {
				unless (BEEN_WARNED(s)) {
					fprintf(stderr,
					"get of %s failed, skipping it.\n",
					name);
				}
			}
		}
next:		if (init) mclose(init);
		/*
		 * No, sccs_diffs() does this.
		 * if (diffs) fclose(diffs);
		 */
		sccs_free(s);
		name = sfileNext();
	}
	sfileDone();
	commentsDone(saved);
	purify_list();
	return (0);
}

