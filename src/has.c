#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("%K%");

/*
 * has - list which changesets include this changeset
 *
 * usage: has [-a] rev
 */

int
has_main(int ac, char **av)
{
	delta	*d, *e;
	sccs	*s;
	int	c, revs = 0, all = 0;
	char	cset[] = CHANGESET;
	char	*rev;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help has");
		return (0);
	}
	while ((c = getopt(ac, av, "ar")) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    case 'r': revs = 1; break;
		}
	}
	rev = av[optind];
	
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "has: cannot find project root\n");
		exit(1);
	}
	unless (s = sccs_init(cset, INIT_NOCKSUM, 0)) {
		perror(CHANGESET);
		sccs_free(s);
		exit(1);
	}
	unless (d = sccs_getrev(s, rev, 0, 0)) {
		fprintf(stderr, "has: cannot find %s\n", rev);
		sccs_free(s);
		exit(1);
	}
	for (e = s->table; e; e = e->next) {
		ser_t	*map;
		

		unless (e->type == 'D') continue;
		unless (all || (e->flags & D_SYMBOLS)) continue;
		map = sccs_set(s, e, 0, 0);
		unless (map[d->serial] == 1) {
			free(map);
			continue;
		}
		unless (e->flags & D_SYMBOLS) {
			printf("%s\n", e->rev);
		} else {
			symbol	*sym;

			for (sym = s->symbols; sym; sym = sym->next) {
				unless (sym->d == e) continue;
				if (revs) printf("%-*s ", s->revLen, e->rev);
				printf("%s\n", sym->symname);
			}
		}
		free(map);
		if (e == d) break;
	}
	sccs_free(s);
	exit(0);
}
