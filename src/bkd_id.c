#include "sccs.h"

int
id_main(int ac, char **av)
{
	int	repo = 0;
	int	product = 0;
	int	c;

	while ((c = getopt(ac, av, "rp", 0)) != -1) {
		switch (c) {
		    case 'p': product = 1; break;
		    case 'r': repo = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	if (product && repo) usage();
	if (proj_cd2root()) {
		fprintf(stderr, "id: not in a repository.\n");
		return (1);
	}
	if (product) {
		if (proj_isComponent(0)) {
			printf("%s\n", proj_csetFile(0));
			return (0);
		}
		fprintf(stderr, "Not a component.\n");
		return (1);
	}
	if (repo) {
		printf("%s\n", proj_repoID(0));
	} else {
		printf("%s\n", proj_rootkey(0));
	}
	return (0);
}
