#include "system.h"
#include "sccs.h" 

extern char *bin;

receive_main(int ac,  char **av)
{
	int c, new = 0;
	char buf[MAXLINE], opts[MAXLINE] = "";
	char *path;

	platformInit();  

	while ((c = getopt(ac, av, "aciv")) != -1) {
		switch (c) { 
		    case 'a': strcat(opts, " -a"); break;
		    case 'c': strcat(opts, " -c"); break;
		    case 'i': strcat(opts, " -i"); new =1; break;
		    case 'v': strcat(opts, " -v"); break;
		    default :
			fprintf(stderr, "unknow option <%c>\n", c);
			exit(1);
		}
	}
	path = av[optind++];
	if ((path == NULL) || (av[optind] != NULL)) {
		fprintf(stderr,
			"usage: bk receive [takepatch options] pathname\n");
		exit(1);
	}

	if (new && !isdir(path)) mkdirp(path);
	if (chdir(path) != 0) {
		perror(path);
		exit(1);
	}
	sprintf(buf, "%sbk unwrap | %sbk takepatch %s", bin, bin, opts);
	return (system(buf));
}
