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

	platformInit();
	while ((c = getopt(ac, av, "v")) != -1) { 
		switch (c) {
		    case 'v': verbose++; break;
		}
	}
	if (project_path = av[optind]) chdir(project_path);
	sccs_cd2root(0, 0);
	sprintf(status_log, "%s/bk_status%d", TMP_PATH, getpid());
	status(verbose, status_log);
	f = fopen(status_log, "r");
	while (fgets(buf, sizeof(buf), f)) fputs(buf, stdout);
	fclose(f);
	unlink(status_log);
	return (0);
}

