/* Copyright (c) 1997 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private	int	isMasterTree(void);


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
	char	*name, *dest;
	int	isDir;
	int	isUnDelete = 0;
	int	errors = 0;
	int	dofree = 0;
	int	force = 0;
	int	i;
	int	c;

	has_proj("mv");
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

	isDir = isdir(dest);
	if (ac - (optind - 1) > 3 && !isDir) {
		fprintf(stderr,
		    "Multiple files must be moved to a directory!\n");
		return (1);
	}
	av[ac-1] = 0;

	/*
	 * Five cases
	 * 1) File -> File
	 * 2) File -> Existing Dir
	 * 3) File -> Non-Existing Dir (error case)
	 * 4) Dir -> non-Existing Dir
	 * 5) Dir -> Existing Dir
	 */
	for (i = optind; i < (ac - 1); i++) {
		if (isdir(av[i])) {
			/*
			 * TODO: "bk mvdir" is a shell script,
			 * should re-code "bk mvdir" in C code
			 * defer this till we get to the 3.0 tree
			 */
			errors |= sys("bk", "mvdir", av[i], dest, SYS);
		} else {
			errors |= sccs_mv(av[i], dest,
						isDir, 0, isUnDelete, force);
		}
	}
	if (dofree) free(dest);
	return (errors);
}
