#include "system.h"
#include "sccs.h" 

#define PARENT "BitKeeper/log/parent"

main(int ac,  char **av)
{
	char buf[MAXLINE];
	FILE *f;
	int i = 0;

	platformInit();  
	cd2root();
	if (ac == 1) {
		if (exists(PARENT)) {
			printf("Parent repository is ");
			f = fopen(PARENT, "rt");
			while (fgets(buf, sizeof(buf), f)) fputs(buf, stdout);
			fclose(f);
		}
		return (0);
	}
	if (ac > 2) {
		fprintf(stderr , "usage: bk parent [host:]dir\n");
		exit(1);
	}
	if (strchr(av[1], ':')) {
		/* we have a host:dir format */
		f = fopen(PARENT, "wb");
		fprintf(f , "%s\n", av[1]);
		fclose(f);
	} else {
		char parent[MAXPATH];
		char pdir[MAXPATH];
		strcpy(parent,  fullname(PARENT, 0));
		if (chdir(av[1]) != 0) {
			fprintf(stderr, "Can not find %s\n", av[1]);
			exit(1);
		}
		getcwd(pdir, sizeof(pdir));
		assert(IsFullPath(pdir));
		f = fopen(parent, "wb");
		fprintf(f, "%s:%s\n", sccs_gethost(),  pdir);
		fclose(f);
		printf("Set parent to %s:%s\n", sccs_gethost(),  pdir);
	}
	return (0);
}
