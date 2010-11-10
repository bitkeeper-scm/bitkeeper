#include "system.h"
#include "sccs.h"

int
root_main(int ac, char **av)
{
	char	*p;
	int	c;
	int	product = 1;

	while ((c = getopt(ac, av, "PRs;", 0)) != -1) {
		switch (c) {
		    case 'P':	break;			// do not doc
		    case 'R':	product = 0; break;	// do not doc
		    case 's':
			product = 0;
			unless (streq(optarg, ".")) usage();
			break;
		    default:	bk_badArg(c, av);
		}
	}
	if (av[optind]) {
		p = isdir(av[optind]) ? av[optind] : dirname(av[optind]);
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
