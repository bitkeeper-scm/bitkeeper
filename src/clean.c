/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
WHATSTR("@(#)%K%");

char *clean_help = 
"\nclean - clean up files.\n\
\n\
The default behaviour is to clean up all checked out files,\n\
locked or unlocked.  Files are cleaned if they are unmodified,\n\
so a \"co -l; clean\" is a null operation.\n\
\n\
usage: clean [-puv] [files...]\n\
    -p	print, i.e., show diffs of modified files\n\
    -u	clean even modified files, discarding changes (DANGEROUS).\n\
    -v	list files being cleaned\n\n";

/*
 * This works even if there isn't a gfile.
 */
int
main(int ac, char **av)
{
	sccs	*s = 0;
	int	flags = SILENT;
	int	c;
	char	*name;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, clean_help);
		return (1);
	}
	if (streq("unedit", av[0])) {
		flags |= CLEAN_UNEDIT;
	}
	while ((c = getopt(ac, av, "puv")) != -1) {
		switch (c) {
		    case 'p': flags |= PRINT; break;
		    case 'u': flags |= CLEAN_UNEDIT; break;
		    case 'v': flags &= ~SILENT; break;
		    default:
			goto usage;
		}
	}
	/*
	 * Too dangerous to unedit everything automagically,
	 * make 'em spell it out.
	 */
	if (flags & CLEAN_UNEDIT) {
		unless (name =
		    sfileFirst("clean",
				&av[optind], SF_GFILE|SF_NODIREXPAND)) {
			fprintf(stderr,
			    "clean: must have explicit list "
			    "when discarding changes.\n");
			exit(1);
		}
	} else {
		name = sfileFirst("clean", &av[optind], SF_GFILE);
	}
	while (name) {
		if ((s = sccs_init(name, SILENT|INIT_NOCKSUM))) {
			(void)sccs_clean(s, flags);
			sccs_free(s);
		}
		name = sfileNext();
	}
	sfileDone();
	purify_list();
	return (0);
}
