#include "system.h"
#include "sccs.h" 

int
receive_main(int ac,  char **av)
{
	int	c, new = 0;
	char	buf[MAXLINE], opts[MAXLINE] = "";
	char	*path;

	if (bk_mode() == BK_BASIC) {
		fprintf(stderr, upgrade_msg);
		exit(1);
	}

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help receive");
		return (0);
	}

	while ((c = getopt(ac, av, "acFiStv")) != -1) {
		switch (c) { 
		    case 'a': strcat(opts, " -a"); break;
		    case 'c': strcat(opts, " -c"); break;
		    case 'F': strcat(opts, " -F"); break;
		    case 'i': strcat(opts, " -i"); new =1; break;
		    case 'S': strcat(opts, " -S"); break;
		    case 't': strcat(opts, " -t"); break;
		    case 'v': strcat(opts, " -v"); break;
		    default :
			system("bk help -s receive");
			exit(1);
		}
	}
	unless (av[optind]) {
		if (sccs_cd2root(0, 0)) {
usage:			fprintf(stderr,
			"usage: bk receive [takepatch options] [pathname]\n");
			exit(1);
		}
	} else {
		path = av[optind++];
		if ((path == NULL) || (av[optind] != NULL)) goto usage;
		if (new && !isdir(path)) mkdirp(path);
		if (chdir(path) != 0) {
			perror(path);
			exit(1);
		}
	}
	sprintf(buf, "bk unwrap | bk takepatch %s", opts);
	return (system(buf));
}
