#include "system.h"
#include "sccs.h"

int
gethost_main(int ac, char **av)
{
	char 	*host;

	if (ac == 2 && !strcmp("--help", av[1])) {
		system("bk help gethost");
		return (1);
	}

	host = sccs_gethost();
	if ((host == NULL) || (*host == '\0')) return (1);
	printf("%s\n", host);
	/* make sure we have a good domain name */
	unless (strchr(host, '.')) return (1);
	return (0);
}
