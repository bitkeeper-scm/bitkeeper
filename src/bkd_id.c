#include "sccs.h"

int
id_main(int ac, char **av)
{
	int	repo = 0;
	int	c;

	while ((c = getopt(ac, av, "r")) != -1) {
		switch (c) {
		    case 'r': repo = 1; break;
		    default:
usage:			sys("bk", "help", "-s", "id", SYS);
			return (1);
		}
	}
	if (av[optind]) goto usage;
	if (proj_cd2root()) {
		fprintf(stderr, "id: not in a repository.\n");
		return (1);
	}
	if (repo) {
		printf("%s\n", proj_repoID(0));
	} else {
		printf("%s\n", proj_rootkey(0));
	}
	return (0);
}
