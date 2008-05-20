#include "system.h"

int
glob_main(int ac, char **av)
{
	char	*glob = av[1];
	int	i, matched = 0;

	unless (av[1] && av[2]) {
		system("bk help -s glob");
		return (1);
	}
	for (i = 2; av[i]; i++) {
		if (match_one(av[i], glob, 0)) {
			printf("%s matches.\n", av[i]);
			matched = 1;
		}
	}
	unless (matched) {
		printf("No match.\n");
		return (1);
	}
	return (0);
}
