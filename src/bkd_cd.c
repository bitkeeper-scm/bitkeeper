#include "bkd.h"

/*
 * Change directory.
 *
 * Usage: cd /full/path/name
 */
int
cmd_cd(int ac, char **av)
{
	char *p = av[1];

#ifdef WIN32
	/* convert /c:path => c:path */
	if (p && (p[0] == '/') && isalpha(p[1]) && (p[2] == ':')) p++;
#endif
	if (!p || chdir(p)) {
		out("ERROR-Can not change to directory '");
		out(av[1]);
		out("'\n");
		return (-1);
	}
	unless (exists("BitKeeper/etc")) {
		out("ERROR-directory '");
		out(av[1]);
		out("' is not a package root\n");
		return (-1);
	}
	if (bk_proj) proj_free(bk_proj);
	bk_proj = proj_init(0);
	out("OK-root OK\n");
	return (0);
}
