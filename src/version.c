#include "system.h"
#include "sccs.h"
#include "logging.h"

extern	int	test_release;
extern	unsigned build_timet;

int
version_main(int ac, char **av)
{
	char buf[100];
	float	exp;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help version");
		return (0);
	}
	lease_check((project *)-1);	/* disable lease check */
	if (proj_cd2root()) {
		getMsg("version", " ", 0, 0, stdout);
		return (0);
	}
	getMsg("version", bk_model(buf, sizeof(buf)), 0, 0, stdout);
	if (test_release) {
		exp = ((time_t)build_timet - time(0)) / (24*3600.0) + 14;
		if (exp > 0) {
			printf("Expires in: %.1f days (test release).\n", exp);
		} else {
			printf("Expired (test release).\n");
		}
	}
	return (0);
}
