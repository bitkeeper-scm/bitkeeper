#include "system.h"
#include "sccs.h"

int
mklock_main()
{
	printf("%d %s\n", getpid(), sccs_gethost());
	return 0;
}
