#include "sccs.h"
WHATSTR("%W% %@%");

/*
 * g2sccs - convert gfile names to sfile names
 */
int
main(int ac, char **av)
{
	int	i;
	char	buf[1024];
	
	if (ac > 1) {
		for (i = 1; i < ac; ++i) {
			doit(av[i]);
		}
	} else {
		while (fnext(buf, stdin)) {
			chop(buf);
			doit(buf);
		}
	}
	return (0);
}

doit(char *name)
{
	name = name2sccs(name);
	printf("%s\n", name);
	free(name);
}
