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

	if (sccs_cd2root(0, 0)) {
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

char *
key2path(char *key, MDBM *idDB)
{
	char	*path;

	path = mdbm_fetch_str(idDB, key);
	unless (path) {
		char    *t, *r;

		for (t = key; *t++ != '|'; );
		for (r = t; *r != '|'; r++);
		assert(*r == '|');
		*r = 0;
		path = strdup(t);
		*r = '|';
	}

	unless (path) {
		fprintf(stderr, "Can't find path for key %s\n",key);
		exit(1);
	}

	if (path) return (strdup(path));
	return (NULL);
}
