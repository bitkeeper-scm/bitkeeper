#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

void	doit(char *);

/*
 * g2sccs - convert gfile names to sfile names
 */
int
main(int ac, char **av)
{
	int	i;
	
	if (ac > 1) {
		for (i = 1; i < ac; ++i) {
			doit(av[i]);
		}
	} else {
		char	buf[MAXPATH];

		while (fnext(buf, stdin)) {
			chop(buf);
			doit(buf);
		}
	}
	return (0);
}

void
doit(char *name)
{
	name = name2sccs(name);
	printf("%s\n", name);
	free(name);
}
