#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

static void	print_name(char *);

/*
 * g2sccs - convert gfile names to sfile names
 */
int
g2sccs_main(int ac, char **av)
{
	int	i;

	if (ac > 1) {
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
