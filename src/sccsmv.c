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
mv_main(int ac, char **av)
{
	char	*name, *name2 = 0, *dest;
	int	isDir, sflags = 0;
	int	errors = 0;
	int	dofree = 0;

	if (bk_mode() == BK_BASIC) {
		fprintf(stderr, upgrade_msg);
		return(1);
	}

	debug_main(av);
	if (ac < 3) {
usage:		fprintf(stderr, "usage: %s from to\n", av[0]);
		exit(1);
	}
	dest = av[ac-1];
	cleanPath(dest, dest);
	if ((name = strrchr(dest, '/')) &&
	    (name >= dest + 4) && strneq(name - 4, "SCCS/s.", 7)) {
		dest = sccs2name(dest);
		dofree++;
	}
	/*
	 * If they specified a directory as the first arg,
	 * or if there is more than one file,
	 * and the last arg doesn't exist, create it as a directory.
	 */
	if (isdir(av[1]) || (name2 = sfileNext())) {
		unless (isdir(dest)) mkdir(dest, 0777);
		/* mvdir includes deleted files */
		if (isdir(av[1])) sflags |= SF_DELETES; 
	}
	isDir = isdir(dest);
	unless ((isDir > 0) || (ac == 3)) goto usage;
	av[ac-1] = 0;
	for (name =
	    sfileFirst("sccsmv" ,&av[1], sflags); name; name = sfileNext()) {
again:		errors |= sccs_mv(name, dest, isDir, 0);
		if (name2) {
			name = name2;
			name2 = 0;
			goto again;
		}
	}
	if (dofree) free(dest);
	sfileDone();
	return (errors);
}
