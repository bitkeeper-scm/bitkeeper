/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

char *clean_help = 
"usage: clean [-punv] [files...]\n\
    -p	print, i.e., show diffs of modified files\n\
    -q	quiet operation, do not complain about nonexistant files\n\
    -u	clean even modified files, discarding changes (DANGEROUS)\n\
    -n  leave the working copy of the file in place\n\
    -v	list files being cleaned\n\n\
\n\
The default behaviour is to clean up all checked out files,\n\
locked or unlocked.  Files are cleaned if they are unmodified,\n\
so \"bk get -e; bk clean\" is a null operation.\n";

/*
 * This works even if there isn't a gfile.
 */
int
main(int ac, char **av)
{
	sccs	*s = 0;
	int	flags = SILENT;
	int	sflags = SF_GFILE;
	int	c;
	char	*name;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fputs(clean_help, stderr);
		return (1);
	}
	if (streq("unedit", av[0]) || streq("unget", av[0])) {
		flags |= CLEAN_UNEDIT;
	}
	while ((c = getopt(ac, av, "npqsuv")) != -1) {
		switch (c) {
		    case 'p': flags |= PRINT; break;
		    case 's':
		    case 'q': flags |= CLEAN_SHUTUP; sflags |= SF_SILENT; break;
		    case 'u': flags |= CLEAN_UNEDIT; sflags &= ~SF_GFILE; break;
		    case 'n': flags |= CLEAN_UNLOCK; sflags &= ~SF_GFILE; break;
		    case 'v': flags &= ~SILENT; break;
			break;
		    default:
			goto usage;
		}
	}

	/*
	 * Too dangerous to unedit everything automagically,
	 * make 'em spell it out.
	 */
	if (flags & (CLEAN_UNEDIT|CLEAN_UNLOCK)) {
		unless (name =
		    sfileFirst("clean", &av[optind], sflags|SF_NODIREXPAND)) {
			fprintf(stderr,
			    "clean: must have explicit list "
			    "when discarding changes.\n");
			exit(1);
		}
	} else {
		name = sfileFirst("clean", &av[optind], SF_DELETES|sflags);
	}
	while (name) {
		if ((s = sccs_init(name, SILENT|INIT_NOCKSUM, 0))) {
			(void)sccs_clean(s, flags);
			sccs_free(s);
		}
		name = sfileNext();
	}
	sfileDone();
	purify_list();
	return (0);
}
