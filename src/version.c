#include "system.h"
#include "sccs.h"

int
version_main()
{
	if (sccs_cd2root(0, 0) == -1) {
		gethelp("version", " ", 0, stdout);
		return (0);
	}
	gethelp("version", bk_model(), 0, stdout);
	return (0);
}
