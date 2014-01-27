#include "system.h"
#include "sccs.h"
#include "pcre.h"

int
regex_main(int ac, char **av)
{
	pcre	*re;
	int	i, off;
	const char	*error;
	int	matched = 0;

	unless (av[1] && av[2]) usage();
	unless (re = pcre_compile(av[1], 0, &error, &off, 0)) {
		fprintf(stderr, "pcre_compile returned 0: %s\n", error);
		return(1);
	}
	for (i = 2; av[i]; i++) {
		unless (pcre_exec(re, 0, av[i], strlen(av[i]), 0, 0, 0, 0)) {
			printf("%s matches.\n", av[i]);
			matched = 1;
		}
	}
	unless (matched) printf("No match.\n");
	free(re);
	return (matched ? 0 : 1);
}
