#include "system.h"
#include "sccs.h"

int
gethost_main(int ac, char **av)
{
	extern	char	*sccs_gethost();
	char 	*host;

	if (ac == 2 && !strcmp("--help", av[1])) {
		system("bk help gethost");
		return (1);
	}
	if (ac == 2 && streq("-r", av[1])) {
		host = sccs_realhost();
	} else {
		host = sccs_gethost();
	}
	unless (host && *host) return (1);
	printf("%s\n", host);
	/* make sure we have a good domain name */
	unless (strchr(host, '.')) return (1);
	return (0);
}
