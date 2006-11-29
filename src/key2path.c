/*
 * Copyright (c) 2002, Andrew Chang
 */
#include "system.h"
#include "sccs.h"

int
key2path_main(int ac, char **av)
{
	
	char	*path;
	char	key[MAXKEY];
	MDBM	*idDB;

	if (proj_cd2root()) {
		fprintf(stderr, "pull: cannot find package root.\n");
		exit(1);
	}

	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("idcache");
		exit(1);
	}

	while (fnext(key, stdin)) {
		chomp(key);

		path = key2path(key, idDB);
		unless (path) {
			fprintf(stderr, "Can't find path for key %s\n",key);
			mdbm_close(idDB);
			return (1);
		}
		printf("%s\n", path);
		free(path);
	}

	mdbm_close(idDB);
	return (0);
}

/*
 * Take one or two keys and return the path to the s.file or possibly
 * a binpool data file.
 * We do not return a path to a file that does not exist.
 */
char *
key2path(char *key, MDBM *idDB)
{
	char	*path;
	char	*path, *p, *t, *r;

	/* We need to be called at the root */
	if (getenv("BK_REGRESSIONS")) assert(exists(BKROOT));

	if (path = mdbm_fetch_str(idDB, key)) {
		path = name2sccs(path);
		goto out;
	}
	
	/* If we get <rootkey> <deltakey> see if it is a binpool file */
	if (p = separator(key)) {
		*p++ = 0;
		path = bp_key2path(key, p, idDB);
		p[-1] = ' ';
		if (path) {
			printf("%s\n", path);
			path[strlen(path) - 2] = 'a';
			return (path);
		}
	}

	if (p = separator(key)) {
		if (path) {
			path = name2sccs(path);
			goto out;
		}

		for (t = key; *t++ != '|'; );
		for (r = t; *r != '|'; r++);
	/* Try exploding the key and looking for the original path */
	unless (t = strchr(key, '|')) return (0);
	for (r = ++t; *r != '|'; r++);
	assert(*r == '|');
	*r = 0;
	path = name2sccs(t);
	*r = '|';
out:	unless (isreg(path)) {
		free(path);
		return (0);
	}

	if (path) return (strdup(path));
	return (NULL);
	return (path);
}
