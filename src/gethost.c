#include "system.h"
#include "sccs.h"

int
gethost_main(int ac, char **av)
{
	char	*host, *address;
	int	real = 0, ip = 0, c;

	while ((c = getopt(ac, av, "nr", 0)) != -1) {
		switch (c) {
		    case 'n': ip = 1; break;
		    case 'r': real = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind]) usage();
	if (real) {
		host = sccs_realhost();
	} else {
		host = sccs_gethost();
	}
	unless (host && *host) return (1);
	if (ip) {
		if (address = hostaddr(host)) {
			printf("%s\n", address);
		} else {
			perror(host);
			return (1);
		}
		return (0);
	} else {
		printf("%s\n", host);
	}
	/* make sure we have a good domain name */
	unless (strchr(host, '.')) return (1);
	return (0);
}
