#include "system.h"
#include "sccs.h"

int
status_main(int ac, char **av)
{
	int c;
	int verbose = 0;
	char *package_path;

	if (av[1] && streq(av[1], "--help")) {
		system("bk help status");
		return (0);
	}
	while ((c = getopt(ac, av, "v")) != -1) { 
		switch (c) {
		    case 'v': verbose++; break;			/* doc 2.0 */
		    default:
			system("bk help -s status");
			return (1);
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

