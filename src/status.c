#include "system.h"
#include "sccs.h"

private char    *status_help = "\n\
usage: status [-v] [repository]\n\n\
	-v  Verbose listing.  Lists users, files not under\
	    revision control, files modified and not checked in, and\
	    files with checked in, but not committed deltas, one per line.\n\n";

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
		    default:
usage:                  fprintf(stderr, "status: usage error, try --help.\n");
			return (1);
		}
	}
	if (project_path = av[optind]) chdir(project_path);
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "status: can not find root directory\n");
		return(1);  /* error exit */
	}
	sprintf(status_log, "%s/bk_status%d", TMP_PATH, getpid());
	status(verbose, status_log);
	f = fopen(status_log, "r");
	while (fgets(buf, sizeof(buf), f)) fputs(buf, stdout);
	fclose(f);
	unlink(status_log);
	return (0);
}

