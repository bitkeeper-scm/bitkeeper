#include <stdio.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

int
main()
{
	extern	char	*sccs_gethost();

#ifdef WIN32
	setmode(1, _O_BINARY);
#endif
	printf("%s\n", sccs_gethost());
	return (0);
}
