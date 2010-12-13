#include "bkd.h"
#include "bam.h"
#include "nested.h"

int
cmd_nested(int ac, char **av)
{
	char	*nlid;
	int	verbose = 0;

	unless (av[1]) {
		out("ERROR-invalid command\n");
		return (1);
	}
	unless (proj_isProduct(0)) {
		out("ERROR-nested only in product\n");
		return (1);
	}
	unless (nlid = getenv("_NESTED_LOCK")) {
		out("ERROR-nested command expects nested lock\n");
		return (1);
	}
	if (streq(av[1], "unlock")) {
		if (av[2] && streq(av[2], "-R")) {
			// Yuck.  Too lazy to getopt this
			if (av[3]) if (streq(av[3], "-v")) verbose = 1;
			bkd_doResolve(av[0], verbose);
		}
		if (nested_unlock(0, nlid)) {
			error("%s", nested_errmsg());
			return (1);
		}
		out("@OK@\n");
	} else if (streq(av[1], "abort")) {
		if (nested_abort(0, nlid)) {
			error("%s", nested_errmsg());
			return (1);
		}
		system("bk -?BK_NO_REPO_LOCK=YES abort -qf 2>" DEVNULL_WR);
		out("@OK@\n");
	} else {
		/* fail */
		out("ERROR-Invalid argument to nested command\n");
		return (1);
	}
	return (0);
}

