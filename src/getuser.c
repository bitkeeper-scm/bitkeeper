#include "system.h"
#include "sccs.h"

int
getuser_main(int ac, char **av)
{
	extern	char	*sccs_getuser();
	char *user;

	if (ac == 2 && !strcmp("--help", av[1])) {
		system("bk help getuser");
		return (1);
	}
	user = sccs_getuser();
	if ((user == NULL) || (*user == '\0')) return (1);
	printf("%s\n", user);
	return (0);
}
