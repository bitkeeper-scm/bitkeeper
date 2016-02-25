/*
 * Copyright 2009-2013,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
	unless (proj_isEnsemble(0)) {
		out("ERROR-nested only in a nested repo\n");
		return (1);
	}
	unless (nlid = getenv("_BK_NESTED_LOCK")) {
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
		if (nested_unlock(0, nlid) &&
		    (nl_errno != NL_LOCK_FILE_NOT_FOUND)) {
			error("%s", nested_errmsg());
			return (1);
		}
		out("@OK@\n");
	} else if (streq(av[1], "abort")) {
		if (nested_abort(0, nlid) &&
		    (nl_errno != NL_LOCK_FILE_NOT_FOUND)) {
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

