#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

static void	print_name(char *);

/*
 * _g2sccs - convert gfile names to sfile names
 */
int
_g2sccs_main(int ac, char **av)
{
	int	i;

	if (ac == 2 && streq("--help", av[1])) {
		fprintf(stderr, "usage: bk g2sccs file file file ... | -\n");
		return (1);
	}
	if ((ac > 1) && strcmp(av[ac-1], "-")) {
		for (i = 1; i < ac; ++i) {
			print_name(av[i]);
		}
	} else {
		char	buf[MAXPATH];

		while (fnext(buf, stdin)) {
			chop(buf);
			print_name(buf);
		}
	}
	return (0);
}

static void
print_name(char *name)
{
	name = name2sccs(name);
	printf("%s\n", name);
	free(name);
}
