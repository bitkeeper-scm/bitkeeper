/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");
char	*delta_help = "\n\
usage: delta [-cGilnpqs] [-I<f>] [-S<sym>] [-y<c>] [files...]\n\n\
    -c		Skip the checksum generation (not advised)\n\
    -D<file>	Specify a file of diffs to be used as the change\n\
    -G		use gfile mod time as checkin time\n\
    -i		Initial checkin, create a new revision history\n\
    -l		Follow checkin with a locked checkout like ``get -e''\n\
    -L<lod>	Delta is the lod.1, i.e., creates a new line of development\n\
    -n		Retain the edited g-file, which is normally deleted\n\
    -p		Print differences\n\
    -q		Run silently\n\
    -s		Run silently\n\
    -S<sym>	Set the symbol <sym> to be the revision created\n\
    -I<file>	Take the initial rev/date/user/comments/etc from <file>\n\
    		See prs for file format information\n\
    -y<comment>	Sets the revision comment to <comment>.\n\
    -Y		prompts for comment and then uses that for all files.\n\n";

/*
 * Not implemented:
 *  -g<revs>	Specify  a  list of deltas to omit.
 *  -m<mrList>	If the 'v' flag is set, then you must specify an MR number
 *  		with the checkin (MR == modification request number).
 *  -r<rev>
 *
 * TODO -
 *	support ~e escape for dropping into the editor on the comments so far.
 */

#include "comments.c"
int	newrev(sccs *s, pfile *pf);

int
main(int ac, char **av)
{
	sccs	*s;
	int	flags = FORCE|BRANCHOK;
	int	c;
	char	*initFile = 0;
	char	*diffsFile = 0;
	char	*name;
	char	*sym = 0;
	char	*lod = 0;
	FILE	*diffs = 0;
	FILE	*init = 0;
	pfile	pf;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
help:		fprintf(stderr, delta_help);
		return (1);
	}
	while ((c = getopt(ac, av, "cD:g;GI;ilL;m;npqRS;sy|Y")) != -1) {
		switch (c) {
		    /* SCCS flags */
		    case 'g': fprintf(stderr, "-g Not implemented.\n");
			    goto help;
		    case 'm': fprintf(stderr, "-m Not implemented.\n");
			    goto help;
		    case 'n': flags |= SAVEGFILE; break;
		    case 'p': flags |= PRINT; break;
		    case 'r': fprintf(stderr, "-r Not implemented.\n");
			    goto help;
		    case 's': flags |= SILENT; break;
		    case 'y':
			comment = optarg;
			gotComment = 1;
			flags |= DONTASK;
			break;

		    /* LM flags */
		    case 'c': flags |= NOCKSUM; break;
		    case 'D': diffsFile = optarg; break;
		    case 'G': flags |= GTIME; break;
		    case 'I': initFile = optarg; break;
		    case 'i': flags |= NEWFILE; break;
		    case 'l': flags |= SKIPGET|SAVEGFILE|EDIT; break;
		    case 'L': lod = optarg; break;
		    case 'q': flags |= SILENT; break;
		    case 'R': flags |= PATCH; break;
		    case 'S': sym = optarg; break;
		    case 'Y': flags |= DONTASK; break;

		    default:
usage:			fprintf(stderr, "delta: usage error, try --help.\n");
			return (1);
		}
	}

	name = sfileFirst("delta", &av[optind], 0);
	/* They can only have an initFile for one file...
	 * So we go get the next file and error if there
	 * is one.
	 */
	if ((initFile || diffsFile) && name && sfileNext()) {
		fprintf(stderr, "delta: only one file "
		    "may be specified with init or diffs file.\n");
		goto usage;
	}
	if (initFile && (flags & DONTASK)) {
		fprintf(stderr,
		    "delta: only init file or comment, not both.\n");
		goto usage;
	}
	if (diffsFile && !(diffs = fopen(diffsFile, "r"))) {
		fprintf(stderr,
		    "delta: Can't open diffs file '%s'.\n", diffsFile);
		goto usage;
	}
	if (initFile && !(init = fopen(initFile, "r"))) {
		fprintf(stderr,
		    "delta: Can't open init file '%s'.\n", initFile);
		goto usage;
	}
	if (lod && !(flags & NEWFILE)) {
		fprintf(stderr, "delta: -L requires -i.\n");
		goto usage;
	}

	while (name) {
		delta	*d = 0;
		char	*nrev;

		if (flags & DONTASK) unless (d = getComments()) goto usage;
		unless (s = sccs_init(name, flags)) {
			if (d) sccs_freetree(d);
			name = sfileNext();
			continue;
		}
		if (sym) {
			if (!d) d = calloc(1, sizeof(*d));
			d->sym = strdup(sym);
		}
		if (lod) {
			if (!d) d = calloc(1, sizeof(*d));
			d->lod = (struct lod *)strdup(lod);
			d->flags |= D_LODSTR;
		}
		nrev = NULL;
		unless (flags & NEWFILE) {
			if ((flags & EDIT) && (newrev(s, &pf) == -1)) {
				goto next;
			}
			nrev = pf.newrev;
		}
		if (sccs_delta(s, flags, d, init, diffs) == -1) {
			sccs_whynot("delta", s);
			if (init) fclose(init);
			if (diffs) fclose(diffs);
			sccs_free(s);
			commentsDone(saved);
			sfileDone();
			purify_list();
			return (1);
		}
		if (flags & EDIT) {
			int	f = flags & (EDIT|SKIPGET|SILENT);

			s = sccs_restart(s);
			unless (s) {
				fprintf(stderr,
				    "ci: can't restart %s\n", name);
				goto next;
			}

			if (sccs_get(s, nrev, 0, 0, 0, f, "-")) {
				unless (BEEN_WARNED(s)) {
					fprintf(stderr,
					"get of %s failed, skipping it.\n",
					name);
				}
			}
		}
next:		if (init) fclose(init);
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

int
newrev(sccs *s, pfile *pf)
{
	FILE *f;
 	unless (f = fopen(s->pfile, "r")) {     
                fprintf(stderr, "delta: can't open %s\n", s->pfile);   
		perror("get_newrev");
		return -1;
	}
	if (fscanf(f, "%s %s ", pf->oldrev, pf->newrev) != 2) {
                fprintf(stderr, "delta: can't get new rev\n");   
		fclose(f);
		return -1;
	}
	fclose(f);
	return 0;
}
