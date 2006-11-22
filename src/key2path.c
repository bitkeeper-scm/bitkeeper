#include "system.h"
#include "sccs.h"

int
key2path_main(int ac, char **av)
{
	
	char	*path;
	char	key[MAXKEY];
	MDBM	*idDB;
	int	ret = 0;

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

		if (streq("@END OF KEYS@", key)) break;
		unless (path = key2path(key, idDB)) {
			/* Maybe it's already a path? */
			path = name2sccs(key);
			if (sccs_path(path)) {
				printf("%s\n", path);
				free(path);
				continue;
			}
			free(path);
			path = 0;
		}
		unless (path) {
			fprintf(stderr, "Can't find path for key %s\n", key);
			ret = 1;
			continue;
		}
		printf("%s\n", path);
		free(path);
	}
	mdbm_close(idDB);
	return (ret);
}

/*
 * Return true if the path named is a revision controlled file.
 * We expect to be called from the root of the tree so the delta path
 * should match.  That should prevent ../bk-3.secret/src/slib.c holes.
 */
int
sccs_path(char *path)
{
	char	*sfile = 0;
	sccs	*s;
	int	ok;

	sfile = name2sccs(path);
	s = sccs_init(sfile, 0);
	ok = (s && HASGRAPH(s) &&
	    streq(proj_root(s->proj), proj_cwd()) &&
	    streq(s->gfile, sccs_top(s)->pathname));
	sccs_free(s);
	free(sfile);
	return (ok);
}

char *
key2path(char *key, MDBM *idDB)
{
	char	*path, *p;

	path = mdbm_fetch_str(idDB, key);
	/* handle the case that they are sending us a changeset file */
	if (!path && (p = separator(key))) {
		*p = 0;
		path = mdbm_fetch_str(idDB, key);
		*p = ' ';
	}
	unless (path) {
		char    *t, *r;

		unless (t = strchr(key, '|')) return (0);
		for (r = ++t; *r != '|'; r++);
		assert(*r == '|');
		*r = 0;
		path = strdup(t);
		*r = '|';
	}
	unless (path) {
		fprintf(stderr, "Can't find path for key %s\n",key);
		exit(1);
	}
	if (path) return (name2sccs(path));
	return (0);
}
