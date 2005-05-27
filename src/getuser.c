#include "system.h"
#include "sccs.h"

int
getuser_main(int ac, char **av)
{
	char	*user;
	int	real = 0;

	if (ac >= 2 && streq("-r", av[1])) {
		real = 1;
		ac--, av++;
	}
	user = real ? sccs_realuser() : sccs_getuser();
	if ((user == NULL) || (*user == '\0')) return (1);
	printf("%s\n", user);
	return (0);
}
