#include "sccs.h"

/*
 * 0 means product
 * 1 means component
 * 2 means not in nested
 * 3 means error
 */
int
product_main(int ac, char **av)
{
	int	flags = 0;

	if (av[1] && streq(av[1], "-q")) {
		flags |= SILENT;
		ac--, av++;
	}
	if (av[1] && chdir(av[1])) {
		perror(av[1]);
		return (3);
	}
	if (proj_cd2root()) {
		verbose((stderr, "product: not in a compsitory.\n"));
		return (3);
	}
	if (proj_isComponent(0)) {
		verbose((stdout, "This is a component.\n"));
		return (1);
	}
	if (proj_isProduct(0)) {
		verbose((stdout, "This is the product.\n"));
		return (0);
	}
	return (2);
}
