#include "system.h"
#include "sccs.h"

int
root_main(int ac, char **av)
{
	char	*p;
	int	i = 1, product = 0;

	if (av[i] && streq(av[i], "-P")) {
		product = 1;
		i++;
	}
	if (av[i]) {
		p = isdir(av[i]) ? av[i] : dirname(av[i]);
		if (chdir(p)) {
			perror(p);
			return(1);
		}
	}
	if (product) {
		unless (p = proj_root(proj_product(0))) {
			fprintf(stderr, "cannot find product root\n");
			exit(1);
		}
	} else {
		unless (p = proj_root(0)) {
			fprintf(stderr, "cannot find package root\n");
			exit(1);
		}
	}
	printf("%s\n", p);
	return(0);
}
