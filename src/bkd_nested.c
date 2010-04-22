#include "bkd.h"
#include "bam.h"
#include "nested.h"

int
cmd_nested(int ac, char **av)
{
	char	*nlid;
	int	verbose = 0;

	unless (av[1]) {
		out("ERROR-invalid command");
		return (1);
	}
	unless (nlid = getenv("_NESTED_LOCK")) {
		out("@OK@\n");
		return (0);
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
		system("bk abort -f 2>" DEVNULL_WR);
		out("@OK@\n");
	} else {
		/* fail */
		out("ERROR-Invalid argument to nested command\n");
		return (1);
	}
	return (0);
}

