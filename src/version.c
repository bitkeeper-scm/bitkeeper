#include "system.h"
#include "sccs.h"
#include "logging.h"

int
version_main(int ac, char **av)
{
	char buf[100];

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help version");
		return (0);
	}

	if (sccs_cd2root(0, 0) == -1) {
		getMsg("version", " ", 0, 0, stdout);
		return (0);
	}
	getMsg("version", bk_model(buf, sizeof(buf)), 0, 0, stdout);
	return (0);
}
