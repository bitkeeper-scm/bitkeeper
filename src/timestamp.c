#include "sccs.h"

/*
 * Just output raw unix time.
 * Portable (for us) replacement for: (gnu) date +%s
 */
int
timestamp_main(int ac, char **av)
{
	printf("%d\n", (u32)time(0));

	return (0);
}
