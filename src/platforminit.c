#include "sccs.h"
WHATSTR("@(#)%K%");

#ifdef WIN32
void platformSpecificInit(char * name, int flag)
{
	setmode(fileno(stdout), _O_BINARY);
	setmode(fileno(stderr), _O_BINARY);
	nt2bmfname(name, name); /* translate NT filename to bitkeeper format */
}
#else
void platformSpecificInit(char * name, int flag) {}
#endif
