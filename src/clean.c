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
	project	*proj = 0;

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
		s = sccs_init(name, SILENT|INIT_NOCKSUM|INIT_SAVEPROJ, proj);
		if (s) {
			if (sccs_clean(s, flags)) ret = 1;
			unless (proj) proj = s->proj;
			sccs_free(s);
		}
next:		name = sfileNext();
	}
	sfileDone();
	if (proj) proj_free(proj);
	return (ret);
}

/*
 * XXX - for bk -r clean, we should have this test also in sfiles.c
 */
private	int
hasGfile(char *name)
{
	char	gfile[MAXPATH];

	strcpy(gfile, name);
	name = strrchr(gfile, '/');
	assert(name);	/* has to be SCCS/ at least */
	if (name == &gfile[4]) {
		assert(strneq(gfile, "SCCS/", 5));
		name += 3;
		return (exists(name));
	}
	/* name points here v  */
	assert(strneq("/SCCS/s.", name - 5, 8));
	memmove(name - 4, name + 3, strlen(name + 3) + 1);
	return (exists(gfile));
}
