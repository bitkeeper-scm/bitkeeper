#include "bkd.h"
#include "bam.h"
#include "nested.h"

int
cmd_nested(int ac, char **av)
{
	unless (av[1]) {
		out("ERROR-invalid command");
		return (1);
	}
	if (streq(av[1], "unlock")) {
		if (nested_unlock(0, getenv("BK_NESTED_LOCK"))) {
			out(nested_errmsg(1));
			return (1);
		}
		if (av[2] && streq(av[2], "-R")) bkd_doResolve(av);
		out("@OK@\n");
		return (0);
	} else if (streq(av[1], "abort")) {
		if (nested_abort(0, getenv("BK_NESTED_LOCK"))) {
			out(nested_errmsg(1));
			return (1);
		}
		out("@OK@\n");
	} else {
		/* fail */
		out("ERROR-Invalid argument to nested command\n");
		return (1);
	}
	return (1);
}

