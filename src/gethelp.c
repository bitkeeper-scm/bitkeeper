#include "system.h"
#include "sccs.h"

int
gethelp_main(int ac, char **av)
{
	unless (av[1]) {
		fprintf(stderr, "usage: gethelp help_name bkarg\n");
		exit(1);
	}
	return (gethelp(av[1], av[2], stdout) == 0);
}
