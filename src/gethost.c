#include <stdio.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

int
gethost_main()
{
	extern	char	*sccs_gethost();
	char 	*host;

#ifdef WIN32
	setmode(1, _O_BINARY);
#endif
	host = sccs_gethost();
	if ((host == NULL) || (*host == '\0')) return (1);
	printf("%s\n", host);
	return (0);
}
