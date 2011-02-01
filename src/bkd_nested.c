#include "bkd.h"
#include "bam.h"
#include "nested.h"

int
cmd_nested(int ac, char **av)
{
	char	*nlid;
	int	c;
	int	resolve = 0;
	int	verbose = 0;
	int	quiet = 0;

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
		while ((c = getopt(ac-1, av+1, "qRv", 0)) != -1) {
			switch(c) {
			    case 'q': quiet = 1; break;
			    case 'R': resolve = 1; break;
			    case 'v': verbose = 1; break;
			    default:
				/* ignore unknown */
				break;
			}
		}
		if (resolve) bkd_doResolve(av[0], quiet, verbose);
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

