/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private char *unedit_help = "usage: unedit files...\n";

/*
 * This works even if there isn't a gfile.
 */
int
unedit_main(int ac, char **av)
{
	sccs	*s = 0;
	int	flags = SILENT|CLEAN_UNEDIT;
	int	sflags = SF_GFILE|SF_NODIREXPAND;
	int	ret = 0;
	char	*name;
	project	*proj = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		fputs(unedit_help, stderr);
		return (1);
	}

	/*
	 * Too dangerous to unedit everything automagically,
	 * make 'em spell it out.
	 */
	optind = 1;
	unless (name = sfileFirst("unedit", &av[optind], sflags)) {
		fprintf(stderr,
		  "unedit: must have explicit list when discarding changes.\n");
			return(1);
	}
	while (name) {
		s = sccs_init(name, SILENT|INIT_NOCKSUM|INIT_SAVEPROJ, proj);
		if (s) {
			unless (proj) proj = s->proj;
			if (sccs_clean(s, flags)) ret = 1;
			sccs_free(s);
		}
		name = sfileNext();
	}
	sfileDone();
	if (proj) proj_free(proj);
	purify_list();
	return (ret);
}
