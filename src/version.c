#include "system.h"
#include "sccs.h"

int
version_main()
{
	gethelp("version", bk_mode() ? "Professional" : "Standard", 0, stdout);
	return (0);
}
