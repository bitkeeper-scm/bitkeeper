/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private	int	hasGfile(char *name);

/*
 * This works even if there isn't a gfile.
 */
int
clean_main(int ac, char **av)
{
	sccs	*s = 0;
	int	flags = SILENT;
	int	sflags = 0;
	int	c;
	int	ret = 0;
	char	*name;
	
	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help clean");
		return (1);
	}
	while ((c = getopt(ac, av, "pqv")) != -1) {
		switch (c) {
		    case 'p': flags |= PRINT; break;		/* doc 2.0 */
		    case 'q': 					/* doc 2.0 */
			flags |= CLEAN_SHUTUP; sflags |= SF_SILENT; break;
		    case 'v': flags &= ~SILENT; break;		/* doc 2.0 */
			break;
		    default:
			system("bk help -s clean");
			return (1);
		}
	}

	name = sfileFirst("clean", &av[optind], SF_DELETES|sflags);
	while (name) {
		unless (hasGfile(name)) goto next;
		s = sccs_init(name, SILENT|INIT_NOCKSUM);
		if (s) {
			if (sccs_clean(s, flags)) ret = 1;
			sccs_free(s);
		}
next:		name = sfileNext();
	}
	sfileDone();
	return (ret);
}

/*
 * XXX - for bk -r clean, we should have this test also in sfiles.c
 */
private	int
hasGfile(char *sfile)
{
	char	*gfile = sccs2name(sfile);
	int	ret;
	
	assert(gfile);
	ret = exists(gfile);
	free(gfile);
	return (ret);
}
