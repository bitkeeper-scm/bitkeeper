#include "system.h"
#include "sccs.h"
#ifdef WIN32
#include <io.h>
#include <fcntl.h>
#endif

int
getuser_main(int ac, char **av)
{
	int	real = av[1] && streq(av[1], "-r");

#ifdef WIN32
	setmode(1, _O_BINARY);
#endif
	printf("%s\n", real ? sccs_realuser() : sccs_getuser());
	return (0);
}
