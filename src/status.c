#include "system.h"
#include "sccs.h"

int
status_main(int ac, char **av)
{
	int c;
	int verbose = 0;
	char *package_path;

	while ((c = getopt(ac, av, "v", 0)) != -1) { 
		switch (c) {
		    case 'v': verbose++; break;			/* doc 2.0 */
		    default: bk_badArg(c, av);
		}
	}
	if (package_path = av[optind]) {
		if (chdir(package_path)) {
			perror(package_path);
			return (1); /* failed */
		}
	}
	if (proj_cd2root()) {
		fprintf(stderr, "status: cannot find root directory\n");
		return(1);  /* error exit */
	}
	status(verbose, stdout);
	return (0);
}

