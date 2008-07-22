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
		fprintf(stderr, "key2path: cannot find package root.\n");
		exit(1);
	}

	unless (idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
		perror("idcache");
		exit(1);
	}

	while (fnext(key, stdin)) {
		chomp(key);

		unless (path = key2path(key, idDB)) {
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
	char	*path, *t;

	if (path = mdbm_fetch_str(idDB, key)) return (strdup(path));

	unless (path = strchr(key, '|')) return (0);
	path++;
	unless (t = strchr(path, '|')) return (0);
	*t = 0;
	path = strdup(path);
	*t = '|';
	return (path);
}
