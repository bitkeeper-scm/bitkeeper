#include "system.h"
#include "sccs.h"

#define	PARENT "BitKeeper/log/parent"

int
parent_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	char	parent[MAXPATH] = PARENT;
	char	pdir[MAXPATH];
	FILE	*f;
	int	c, do_remove = 0, quiet = 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help parent");
		return (0);
	}

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "parent: cannot find package root.\n");
		exit(1);
	}

	while ((c = getopt(ac, av, "qr")) != -1) {
		switch (c) {
		    case 'q': quiet = 1; break;	/* doc 2.0 */
		    case 'r': do_remove = 1; break;	/* doc 2.0 */
		    default:
			system("bk help -s parent");
			exit(1);
		}
	}
	if (do_remove) {
		if (exists(PARENT)) {
			unlink(PARENT);
		}
		unless (quiet) {
			unless(exists(PARENT)){
				printf("Removed parent pointer\n");
			}
		}
		if (exists(PARENT)) exit(1);
		exit(0);
	}

#ifdef WIN32
	/* account for dos path */
	if (av[optind] && strchr(av[optind], ':') && av[optind][1] != ':') {
#else
	if (av[optind] && strchr(av[optind], ':')) {
#endif
		/* we have a host:dir format */
		mkdirf(parent);
		f = fopen(parent, "wb");
		assert(f);
		fprintf(f, "%s\n", av[optind]);
		fclose(f);
		unless(quiet) printf("Set parent to %s\n", av[optind]);
		exit(0);
	}

	if (av[optind] == NULL) {
		if (exists(PARENT)) {
			printf("Parent repository is ");
			f = fopen(PARENT, "rt");
			while (fgets(buf, sizeof(buf), f)) fputs(buf, stdout);
			fclose(f);
			exit(0);
		}
		printf("This package has no parent\n");
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/etc", av[optind]);
	unless (isdir(buf)) {
		printf("%s is not a BitKeeper package root\n", av[optind]);
		exit(1);
	}

	strcpy(parent,  fullname(PARENT, 0));
	if (chdir(av[optind]) != 0) {
		fprintf(stderr, "Cannot find %s\n", av[1]);
		exit(1);
	}
	getcwd(pdir, sizeof(pdir));
	assert(IsFullPath(pdir));
	mkdirf(parent);
	f = fopen(parent, "wb");
	assert(f);
	fprintf(f, "%s:%s\n", sccs_gethost(),  pdir);
	fclose(f);
	unless (quiet) {
		printf("Set parent to %s:%s\n", sccs_gethost(),  pdir);
	}
	exit (0);
}
