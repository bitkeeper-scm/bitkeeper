#include "system.h"
#include "sccs.h"

int
status_main(int ac, char **av)
{
	int c;
	int verbose = 0;
	char status_log[MAXPATH], *project_path;
	char buf[MAXLINE];
	FILE *f;

	while ((c = getopt(ac, av, "v")) != -1) { 
		switch (c) {
		    case 'v': verbose++; break;
		}
	}
	if (project_path = av[optind]) chdir(project_path);
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "status: can not find root directory\n");
		return(1);  /* error exit */
	}
	status(verbose, stdout);
	return (0);
}

