#include "system.h"
#include "sccs.h"

/* Look for files containing binary data that BitKeeper cannot handle.
 * This consists of (a) NULs, (b) \n followed by \001.
 */
int
isascii_main(int ac, char **av)
{
	if (ac != 2) {
		fprintf(stderr, "usage: %s filename\n", av[0]);
	}
	return (!ascii(av[1]));
}
