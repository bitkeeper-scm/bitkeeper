#include "system.h"
#include "sccs.h"

int
mklock_main(int ac, char **av)
{
	printf("%u %s\n", getpid(), sccs_gethost());
	return 0;
}
