#include "system.h"
#include "sccs.h"

int
root_main(int ac, char **av)
{
	char *p;

	if (av[1]) {
		p = isdir(av[1]) ? av[1] : dirname(av[1]);
		if (chdir(p)) {
			perror(p);
			return(1);
		}
	}
	unless (p = proj_root(0)) {
		fprintf(stderr, "cannot find package root\n");
		exit(1);
	}
	printf("%s\n", p);
	return(0);
}
