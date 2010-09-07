#include "sccs.h"

/*
 * 0 means product
 * 1 means component
 * 2 means traditional
 * 3 means error
 */
int
repotype_main(int ac, char **av)
{
	int	flags = 0;
	int	rc;

	if (av[1] && streq(av[1], "-q")) {
		flags |= SILENT;
		ac--, av++;
	}
	if (av[1] && chdir(av[1])) {
		perror(av[1]);
		return (3);
	}
	if (proj_cd2root()) {
		verbose((stderr, "%s: not in a repository.\n", prog));
		return (3);
	}
	if (proj_isComponent(0)) {
		verbose((stdout, "component\n"));
		rc = 1;
	} else if (proj_isProduct(0)) {
		verbose((stdout, "product\n"));
		rc = 0;
	} else {
		verbose((stdout, "traditional\n"));
		rc = 2;
	}
	/* exit status only for -q, so we don't have to do
	 * test "`bk repotype` || true" = whatever
	 * on sgi.
	 */
	unless (flags & SILENT) rc = 0;
	return (rc);
}
