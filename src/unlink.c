#include "system.h"
#include "sccs.h"

int
unlink_main(int ac, char **av)
{
	char	c;
	int	errors = 0;
	char	buf[MAXPATH];

	while (fnext(buf, stdin)) {
		unless ((c = chop(buf)) == '\n') {
			fprintf(stderr, "Bad filename '%s%c'\n", buf, c);
			errors = 1;
			continue;
		}
		if (unlink(buf)) errors = 1;
	}
	return (errors);
}

int
sccs_keyunlink(char *key, MDBM *idDB, MDBM *dirs)
{
	sccs	*s;
	int	ret;
	char	*t;

	unless (s = sccs_keyinit(key, INIT_NOCKSUM, idDB)) {
		fprintf(stderr, "Cannot init key %s\n", key);
		return (1);
	}
	if (sccs_clean(s, SILENT)) {
		fprintf(stderr, "Bad clean of %s\n", s->sfile);
		return (2);
	}
	sccs_close(s);
	ret = unlink(s->sfile) ? 4 : 0;
	if (dirs) {
		if (t = strrchr(s->sfile, '/')) {
			*t = 0;
			mdbm_store_str(dirs, s->sfile, "", MDBM_INSERT);
			*t = '/';
		}
	}
	sccs_free(s);
	return (ret);
}

int
keyunlink_main(int ac, char **av)
{
	char	c;
	int	errors = 0;
	char	buf[MAXKEY];
	MDBM	*idDB;

	unless (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
		perror("idcache");
		exit(1);
	}
	proj_cd2root();
	while (fnext(buf, stdin)) {
		unless ((c = chop(buf)) == '\n') {
			fprintf(stderr, "Bad key '%s%c'\n", buf, c);
			errors |= 1;
			continue;
		}
		/* XXX - empty dirs, see csetprune.c:rmKeys */
		errors |= sccs_keyunlink(buf, idDB, 0);
	}
	return (errors);
}

int
link_main(int ac, char **av)
{
	char	c;
	int	errors = 0;
	char	buf[MAXPATH];
	char	new[MAXPATH];

	unless (av[1] && isdir(av[1])) {
		fprintf(stderr, "Usage: %s directory\n", av[0]);
		exit(1);
	}
	while (fgets(buf, sizeof(buf), stdin)) {
		unless ((c = chop(buf)) == '\n') {
			fprintf(stderr, "Bad filename '%s%c'\n", buf, c);
			errors = 1;
			continue;
		}
		sprintf(new, "%s/%s", av[1], basenm(buf));
		if (link(buf, new)) {
			perror(new);
			errors = 1;
		}
	}
	return (errors);
}

int
exists_main(int ac, char **av)
{
	char	c;
	char	buf[MAXPATH];

	while (fgets(buf, sizeof(buf), stdin)) {
		unless ((c = chop(buf)) == '\n') {
			fprintf(stderr, "Bad filename '%s%c'\n", buf, c);
			exit(2);
		}
		if (exists(buf)) {
			printf("%s\n", buf);
			exit(0);
		}
	}
	exit(1);
}

