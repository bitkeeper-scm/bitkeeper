#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * key2rev - convert keys names to revs
 *
 * usage: cat keys | key2rev file
 */
int
main(int ac, char **av)
{
	char	*name;
	delta	*d;
	sccs	*s;
	char	buf[MAXPATH];

	unless (av[1]) {
		fprintf(stderr, "Usage: %s file < keys\n", av[0]);
		exit(1);
	}
	name = name2sccs(av[1]);
	unless (s = sccs_init(name, 0)) {
		perror(name);
		exit(1);
	}
	free(name);
	while (fnext(buf, stdin)) {
		chop(buf);
		unless (d = sccs_findKey(s, buf)) {
			fprintf(stderr, "Can't find %s in %s\n", buf, s->sfile);
			exit(1);
		}
		printf("%s\n", d->rev);
	}
	sccs_free(s);
	exit(0);
}
