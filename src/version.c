#include "system.h"
#include "sccs.h"

int
version_main()
{
	char buf[100];

	if (sccs_cd2root(0, 0) == -1) {
		getmsg("version", " ", 0, stdout);
		return (0);
	}
	getmsg("version", bk_model(buf, sizeof(buf)), 0, stdout);
	return (0);
}
