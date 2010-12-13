#include "sccs.h"

int
id_main(int ac, char **av)
{
	int	repo = 0;
	int	product = 1;
	int	md5key = 0;
	int	c;

	while ((c = getopt(ac, av, "5rpS", 0)) != -1) {
		switch (c) {
		    case '5': md5key = 1; break;
		    case 'p': break;			// obsolete
		    case 'r': repo = 1; break;
		    case 'S': product = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	if (md5key && repo) {
		fprintf(stderr, "%s: No -r and -5 together.  "
		    "No MD5 form of the repository id.\n", prog);
		usage();
	}
	bk_nested2root(!product);
	if (repo) {
		printf("%s\n", proj_repoID(0));
	} else if (md5key) {
		printf("%s\n", proj_md5rootkey(0));
	} else {
		printf("%s\n", proj_rootkey(0));
	}
	return (0);
}
