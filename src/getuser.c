#include <stdio.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

int
getuser_main()
{
	extern	char	*sccs_getuser();
	char *user;

#ifdef WIN32
	setmode(1, _O_BINARY);
#endif
	user = sccs_getuser();
	if ((user == NULL) || (*user == '\0')) return (1);
	printf("%s\n", user);
	return (0);
}
