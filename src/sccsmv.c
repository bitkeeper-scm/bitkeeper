/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * Emulate mv(1)
 *
 * usage: mv a b
 * usage: mv a [b c d ...] dir
 */
int
main(int ac, char **av)
{
	char	*name, *dest;
	int	isDir;
	int	errors = 0;
	int	dofree = 0;

	debug_main(av);
	if (ac < 3) {
usage:		fprintf(stderr, "usage: %s from to\n", av[0]);
		exit(1);
	}
	dest = av[ac-1];
	if ((name = strrchr(dest, '/')) &&
	    (name >= dest + 4) && strneq(name - 4, "SCCS/s.", 7)) {
		dest = sccs2name(dest);
		dofree++;
	}
	isDir = isdir(dest);
	unless (isDir || (ac == 3)) goto usage;
	av[ac-1] = 0;
	for (name =
	    sfileFirst("sccsmv",&av[1], 0); name; name = sfileNext()) {
		errors |= sccs_mv(name, dest, isDir, 0);
	}
	if (dofree) free(dest);
	sfileDone();
	purify_list();
	return (errors);
}
