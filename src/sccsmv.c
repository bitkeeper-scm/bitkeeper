/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * Returm true if Master repository
 */
private int
isMasterTree()
{
	char root[MAXPATH];
	
	unless (bk_proj->root) return (0);
	concat_path(root, bk_proj->root, BKMASTER);
	return (exists(root));
}

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
	int	isUnDelete = 0;
	int	errors = 0;
	int	dofree = 0;
	int	force = 0;
	int	i;
	int	c;

	has_proj("mv");
	if ((bk_mode() == BK_BASIC) && !isMasterTree()) {
		fprintf(stderr, upgrade_msg);
		return(1);
	}
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help mv");
		return (0);
	}

	while ((c = getopt(ac, av, "fu")) != -1) {
		switch (c) {
		    case 'f':	force = 1; break;
		    case 'u':	isUnDelete = 1; break;
		    default:	system("bk help -s mv");
				return (1);
		}
	}

	debug_main(av);
	if (ac < 3) {
		system("bk help -s mv");
		return (1);
	}
	dest = av[ac-1];
	cleanPath(dest, dest);
	if ((name = strrchr(dest, '/')) &&
	    (name >= dest + 4) && strneq(name - 4, "SCCS/s.", 7)) {
		dest = sccs2name(dest);
		dofree++;
	}
	if (strchr(dest, BK_FS)) {
		fprintf(stderr, "%c is not allowed in pathname\n", BK_FS);
		return (1);
	}

	for (i = optind; i < ac-1; i++) {
		if (isdir(av[i])) {
			fprintf(stderr, 
"mv only moves files.  Use mvdir to move directories.\n");
			return (1);
		}
	}
	isDir = isdir(dest);
	if (ac - (optind - 1) > 3 && !isDir) {
		fprintf(stderr,
		    "Multiple files must be moved to a directory!\n");
		return (1);
	}
	av[ac-1] = 0;
	for (name =
	    sfileFirst("sccsmv", &av[optind], sflags);
						name; name = sfileNext()) {
again:		errors |= sccs_mv(name, dest, isDir, 0, isUnDelete, force);
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
