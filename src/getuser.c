#include <stdio.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

int
getuser_main(int ac, char **av)
{
	extern	char	*sccs_getuser();
	char *user;

#ifdef WIN32
	setmode(1, _O_BINARY);
#endif
	if (ac == 2 && !strcmp("--help", av[1])) {
		system("bk help getuser");
		return (1);
	}
	user = sccs_getuser();
	if ((user == NULL) || (*user == '\0')) return (1);
	printf("%s\n", user);
	return (0);
}
