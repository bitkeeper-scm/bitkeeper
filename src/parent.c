#include "bkd.h"

#define	PARENT "BitKeeper/log/parent"

char * getParent(char *buf, int len);

int
parent_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	FILE	*f;
	int	c, do_remove = 0, quiet = 0;
	remote	*r;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help parent");
		return (0);
	}

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "parent: cannot find package root.\n");
		return (1);
	}

	while ((c = getopt(ac, av, "qr")) != -1) {
		switch (c) {
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
		if (getParent(buf, sizeof buf)) {
			printf("Parent repository is %s\n", buf);
			return (0);
		}
		printf("This package has no parent\n");
		return (1);
	}


	r = remote_parse(av[optind], 0);
	unless (r) {
		fprintf(stderr, "Invalid parent address: %s\n", av[optind]);
		return (1);
	}
	if (!r->host || isLocalHost(r->host)) {
		if (r->path) {
			sprintf(buf, "%s/BitKeeper/etc", r->path);
			unless (isdir(buf)) {
				printf("%s is not a BitKeeper package root\n",
					r->path);
				return (1);
			}
			unless (IsFullPath(r->path)) {
				free(r->path);
				r->path = strdup(fullname(r->path, 0));
			}
		}
		if (r->host) free(r->host);
		r->host = strdup(sccs_gethost());
	}


	strcpy(buf, PARENT);
	mkdirf(buf);
	f = fopen(PARENT, "wb");
	assert(f);
	fprintf(f, "%s\n", remote_unparse(r));
	fclose(f);
	unless (quiet) printf("Set parent to %s\n", remote_unparse(r));
	free(r);
	return (0);
}


char *
getParent(char *buf, int len)
{
	char	*p;
	FILE 	*f;

	assert(bk_proj && bk_proj->root);
	p = aprintf("%s/%s", bk_proj->root, PARENT);
	f = fopen(p, "rt");
	free(p);
	unless (f)  return (NULL);
	buf[0] = 0;
	fgets(buf, len, f);
	chop(buf);
	fclose(f);
	return (buf);
}
