#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * key2rev - convert keys names to revs
 *
 * usage: cat keys | key2rev file
 */
int
key2rev_main(int ac, char **av)
{
	char	*name;
	delta	*d;
	sccs	*s;
	char	buf[MAXPATH];

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help key2rev");
		return (0);
	}

	unless (av[1]) {
		system("bk help -s key2rev");
		return (1);
	}
	name = name2sccs(av[1]);
	unless (s = sccs_init(name, 0)) {
		perror(name);
		return (1);
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
	return (0);
}
