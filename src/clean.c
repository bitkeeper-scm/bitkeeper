/* Copyright (c) 1997 L.W.McVoy */
#include "sccs.h"
WHATSTR("%W%");

/*
 * clean - clean up files, or unedit them
 *	-p	print (show diffs)
 *	-s	silent
 *	-u	unedit the file, discarding changes.
 *
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
usage:		fprintf(stderr, "usage: %s [-puv] [files...]\n", av[0]);
		fprintf(stderr, "\t-p\tprint changes to the modified files\n"
				"\t-u\tunedit, discarding changes\n"
				"\t-v\tlist files being cleaned\n");
		return (1);
	}
	if (streq("unedit", av[0])) {
		flags |= UNEDIT;
	}
	while ((c = getopt(ac, av, "puv")) != -1) {
		switch (c) {
		    case 'p': flags |= PRINT; break;
		    case 'u': flags |= UNEDIT; break;
		    case 'v': flags &= ~SILENT; break;
		    default:
			goto usage;
		}
	}
	/*
	 * Too dangerous to unedit everything automagically,
	 * make 'em spell it out.
	 */
	if (flags & UNEDIT) {
		unless (name =
		    sfileFirst("clean", &av[optind], SFILE|GFILE|NOEXPAND)) {
			fprintf(stderr,
			    "clean: must have explicit list "
			    "when discarding changes.\n");
			exit(1);
		}
	} else {
		name = sfileFirst("clean", &av[optind], SFILE|GFILE);
	}
	while (name) {
		if ((s = sccs_init(name, flags))) {
			(void)sccs_clean(s, flags);
			sccs_free(s);
		}
		name = sfileNext();
	}
	sfileDone();
	purify_list();
	return (0);
}
