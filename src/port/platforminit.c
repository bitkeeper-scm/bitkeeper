#include "../system.h"
#include "../sccs.h"
WHATSTR("@(#)%K%");

#ifndef WIN32
void platformSpecificInit(char *name) {}
#else
void platformSpecificInit(char *name)
{
	setmode(1, _O_BINARY);
	setmode(2, _O_BINARY);

	/* translate NT filename to bitkeeper format */
	if (name) nt2bmfname(name, name);
}
#endif
