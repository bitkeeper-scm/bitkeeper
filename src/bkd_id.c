#include "sccs.h"

int
id_main(int ac, char **av)
{
	int	repo = 0;
	int	product = 0;
	int	md5key = 0;
	int	c;

	while ((c = getopt(ac, av, "5rp", 0)) != -1) {
		switch (c) {
		    case '5': md5key = 1; break;
		    case 'p': product = 1; break;
		    case 'r': repo = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	if (md5key && repo) {
		fprintf(stderr, "%s: No -r and -5 together.  "
		    "No MD5 form of the repository id.\n", prog);
		usage();
	}
	if (proj_cd2root()) {
		fprintf(stderr, "id: not in a repository.\n");
		return (1);
	}
	if (product) {
		unless (proj_product(0)) {
			fprintf(stderr, "%s: not in a nested repository.\n",
			    prog);
			return (1);
		}
		proj_cd2product();
	}
	if (repo) {
		printf("%s\n", proj_repoID(0));
	} else if (md5key) {
		printf("%s\n", proj_md5rootkey(0));
	} else {
		printf("%s\n", proj_rootkey(0));
	}
	return (0);
}
