#include "bkd.h"

private int purge_list(int, int, int, int);

int
parent_main(int ac,  char **av)
{
	char	buf[MAXLINE];
	FILE	*f;
	int	c, do_remove = 0, quiet = 0, shell = 0, literal = 0;
	int	tflag = 0, Tflag = 0;
	int	fflag = 0, Fflag = 0;
	int	i;
	remote	*r;
	char	*fp;
	char	*parentFile, *which;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help parent");
		return (0);
	}

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "parent: cannot find package root.\n");
		return (1);
	}

	while ((c = getopt(ac, av, "lqpfFitTr")) != -1) {
		switch (c) {
		    case 'l': literal = 1; break;
		    case 'p': shell = 1; break;
		    case 'q': quiet = 1; break;			/* doc 2.0 */
		    case 't': tflag = 1; break;
		    case 'T': Tflag = 1; break;
		    case 'f': fflag = 1; break;
		    case 'F': Fflag = 1; break;
		    case 'r': do_remove = 1; break;		/* doc 2.0 */
		    default:
			system("bk help -s parent");
			return (1);
		}
	}

	/*
	 * There are three basic function
	 * a) remove
	 * b) list
	 * c) add
	 *
	 * There are also three files
	 * a) PARENT		(single entry only)
	 * b) PUSH_PARENT	(multiple entry)
	 * c) PULL_PARENT	(multiple entry)
 	 * 
	 * i.e. nine cases in this code path
	 */

	/*
	 * Handle remove
	 */
	if (do_remove || Tflag || Fflag) {
		if (tflag || fflag) {
			fprintf(stderr,
			    "parent: -r/-T/-F flags must be alone\n");
			return (1);
		}
		return (purge_list(do_remove, Tflag, Fflag, quiet));
	}

	if (tflag && fflag) {
		fprintf(stderr,
		    "parent: -t/-f flags must be alone\n");
		return (1);
	}

	if (tflag) {
		parentFile = PUSH_PARENT;
		which = "push prent";
	} else if (fflag) {
		parentFile = PULL_PARENT;
		which = "pull parent";
	} else {
		parentFile = PARENT;
		which = "parent";
	}

	/*
	 * Handle list
	 */
	if (av[optind] == NULL) {
		char	**pList = NULL;

		if (tflag) {
			pList = getParentList(PUSH_PARENT, pList);
			unless (pList) goto empty;
			unless (shell) printf("Push Parent are:\n");
			EACH(pList) printf("%s\n", pList[i]);
			freeLines(pList);
			return (0);
		} else if (fflag) {
			pList = getParentList(PULL_PARENT, pList);
			unless (pList) goto empty;
			unless (shell) printf("Pull Parent are:\n");
			EACH(pList) printf("%s\n", pList[i]);
			freeLines(pList);
			return (0);
		} else {
			fp = getParent();
			unless (fp) goto empty;
			unless (shell) printf("Parent repository is ");
			printf("%s\n", fp);
			free(fp);
			return (0);
		}
empty:		fprintf(stderr, "This package has no %s\n", which);
		return (1);
	}

	/*
	 * Handle add
	 */
	r = remote_parse(av[optind], 0);
	unless (r) {
		fprintf(stderr, "Invalid parent address: %s\n", av[optind]);
		return (1);
	}
	if (!literal && isLocalHost(r->host)) {
		/*
		 * We skip the path check of the url has a port address
		 * there are two reasons for this:
		 * a) In release 3.0 bkd will support virtual path
		 *    we will not know the real path until access time
		 * b) Some site may be using ssh port forwarding
		 *    which means the path is not really local.
		 */
		if (r->path && !r->port) {
			sprintf(buf, "%s/BitKeeper/etc", r->path);
			unless (isdir(buf)) {
				printf("bk parent: attempting to set parnet "
				    "url to \"%s\"\n"
				    "but \"%s\" is not a BitKeeper root.\n"
				    "To force this url, use bk parent -l.\n",
				     av[optind], r->path);
				return (1);
			}
			unless (IsFullPath(r->path)) {
				fp = r->path;
				r->path = strdup(fullname(r->path, 0));
				free(fp);
			}
		}
		if (r->host) free(r->host);
		r->host = strdup(sccs_gethost());
	}


	strcpy(buf, parentFile);
	mkdirf(buf);
	if (streq(parentFile, PARENT)) {
		f = fopen(parentFile, "wb");
	} else {
		f = fopen(parentFile, "ab");
	}
	unless (f) {
		perror(PARENT);
		return (1);
	}
	fprintf(f, "%s\n", remote_unparse(r));
	fclose(f);
	unless (quiet) printf("Set %s to %s\n", which, remote_unparse(r));
	remote_free(r);
	return (0);
}

private int
purge_list(int do_remove, int Tflag, int Fflag, int quiet)
{
	int	rc = 0;

	if (do_remove) { /* purge parent file */
		if (exists(PARENT)) unlink(PARENT);
		if (exists(PARENT)) {
			rc = 1;
		} else {
			unless (quiet) printf("Removed parent pointer\n");
		}
	}

	if (Tflag) { /* purge push-parent */
		if (exists(PUSH_PARENT)) unlink(PUSH_PARENT);
		if (exists(PUSH_PARENT)) {
			rc = 1;
		} else {
			unless (quiet) printf("Removed push-parent pointer\n");
		}
	}

	if (Fflag) { /* purge pull-parent */
		if (exists(PULL_PARENT)) unlink(PULL_PARENT);
		if (exists(PULL_PARENT)) {
			rc = 1;
		} else {
			unless (quiet) printf("Removed pull-parent pointer\n");
		}
	}
	return (rc);
}


char	*
getParent()
{
	char	*parentFile, *p;
	int	f, len;

	parentFile = PARENT;

	assert(bk_proj && bk_proj->root);
	p = aprintf("%s/%s", bk_proj->root, parentFile);
	f = open(p, 0, 0);
	free(p);
	unless (f) return (0);
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

char	**
getParentList(char *parentFile, char **pList)
{
	char	buf[MAXPATH];
	char	*p;
	FILE	*f;

	assert(bk_proj && bk_proj->root);
	p = aprintf("%s/%s", bk_proj->root, parentFile);
	f = fopen(p, "rt");
	free(p);
	unless (f) {
		unless (errno == ENOENT) perror(parentFile);
		return (pList);
	}
	while (fnext(buf, f)) {
		chomp(buf);
		pList = addLine(pList, strdup(buf));
	}
	fclose(f);
	return (pList);
}
