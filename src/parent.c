#include "bkd.h"

#define	PARENT "BitKeeper/log/parent"

int
parent_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	FILE	*f;
	int	c, do_remove = 0, quiet = 0, shell = 0;
	remote	*r;
	char	*fp;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help parent");
		return (0);
	}

	if (proj_cd2root()) {
		fprintf(stderr, "parent: cannot find package root.\n");
		return (1);
	}

	while ((c = getopt(ac, av, "qpr")) != -1) {
		switch (c) {
		    case 'p': shell = 1; break;
		    case 'q': quiet = 1; break;			/* doc 2.0 */
		    case 'r': do_remove = 1; break;		/* doc 2.0 */
		    default:
			system("bk help -s parent");
			return (1);
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
		if (exists(PARENT)) return (1);
		return (0);
	}
	
	if (av[optind] == NULL) {
		if (fp = getParent()) {
			unless (shell) printf("Parent repository is ");
			printf("%s\n", fp);
			free(fp);
			return (0);
		}
		fprintf(stderr, "This package has no parent\n");
		return (1);
	}

	r = remote_parse(av[optind], 0);
	unless (r) {
		fprintf(stderr, "Invalid parent address: %s\n", av[optind]);
		return (1);
	}
	if (isLocalHost(r->host)) {
		if (r->path && IsFullPath(r->path)) {
			sprintf(buf, "%s/BitKeeper/etc", r->path);
			unless (isdir(buf)) {
				printf("%s is not a BitKeeper package root\n",
					r->path);
				return (1);
			}
		}
		if (r->host) free(r->host);
		r->host = strdup(sccs_gethost());
	}

	strcpy(buf, PARENT);
	mkdirf(buf);
	f = fopen(PARENT, "wb");
	unless (f) {
		perror(PARENT);
		return (1);
	}
	fprintf(f, "%s\n", remote_unparse(r));
	fclose(f);
	unless (quiet) printf("Set parent to %s\n", remote_unparse(r));
	remote_free(r);
	return (0);
}


char	*
getParent()
{
	char	*p;
	int	f, len;

	assert(proj_root(0));
	p = aprintf("%s/%s", proj_root(0), PARENT);
	f = open(p, 0, 0);
	free(p);
	unless (f >= 0) return (0);
	len = fsize(f);
	p = calloc(1, len+1);
	if (read(f, p, len) != len) {
		perror("parent");
		close(f);
		free(p);
		return (0);
	}
	chomp(p);
	close(f);
	return (p);
}
