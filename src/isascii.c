#include "system.h"
#include "sccs.h"

/* Look for files containing binary data that BitKeeper cannot handle.
 * This consists of (a) NULs, (b) \n followed by \001.
 */
int
isascii_main(int ac, char **av)
{
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help isascii");
		return (1);
	}

	if (ac != 2) {
		system("bk help -s isascii");
		return (1);
	}
	return (!ascii(av[1]));
}
