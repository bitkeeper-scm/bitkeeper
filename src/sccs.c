/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * sccs.c - provide semi compatible interfaces to the sccs front end command.
 *
 * create - Create (initialize) history files.  Same as bk new && bk get.
 * deledit - same as a delta -L
 * delget - same as delta -u
 * val - alias for admin -hhhh
 *
 * Some aliases:
 * enter - same as bk new (done in bk.c)
 * sccsdiff - alias for diffs (done in bk.c)
 */
int
create_main(int ac, char **av)
{
	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
		system("bk help sccs");
		return (1);
	}

	av[0] = "add";
	if (delta_main(ac, av)) exit(1);
	av[0] = "get";
	if (get_main(ac, av)) exit(1);
	exit(0);
}

deledit_main(int ac, char **av)
{
	int	i;
	char	**nav = malloc((ac + 3) * sizeof(char*));

	nav[0] = "delta";
	nav[1] = "-l";
        nav[2] = "-Y";
	i = 1;
	while (nav[i+2] = av[i++]);
	return (delta_main(ac + 2, nav));
}

delget_main(int ac, char **av)
{
	int	i;
	char	**nav = malloc((ac + 3) * sizeof(char*));

	nav[0] = "delta";
	nav[1] = "-u";
	nav[2] = "-Y";
	i = 1;
	while (nav[i+2] = av[i++]);
	return (delta_main(ac + 2, nav));
}

val_main(int ac, char **av)
{
	int	i;
	char	**nav = malloc((ac + 2) * sizeof(char*));

	nav[0] = "admin";
	nav[1] = "-hhh";
	i = 1;
	while (nav[i+1] = av[i++]);
	return (admin_main(ac + 1, nav));
}
