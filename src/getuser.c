#include <stdio.h>
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

int
getuser_main()
{
	extern	char	*sccs_getuser();

#ifdef WIN32
	setmode(1, _O_BINARY);
#endif
	printf("%s\n", sccs_getuser());
	return (0);
}
