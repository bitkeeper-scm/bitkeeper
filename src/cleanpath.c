
#include "system.h"
#include "sccs.h"

int
cleanpath_main(int ac, char **av)
{
	char buf[MAXPATH];

	unless (av[1]) return (0);

	cleanPath(av[1], buf);
	printf("%s\n", buf);
	return (0);
}
