#include "system.h"
#include "sccs.h"

void gethelp(char *help_name, char *bkarg, FILE *f);

int
main(int ac, char **av)
{
	platformInit(); 
	unless (av[1]) {
usage:		fprintf(stderr, "usage: gethelp help_name bkarg\n");
		exit(1);
	}
	gethelp(av[1], av[2], stdout);
	return (0);
}
