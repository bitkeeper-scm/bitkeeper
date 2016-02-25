/*
 * Copyright 2001,2003-2005,2010-2011,2013,2015-2016 BitMover, Inc
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

int
csets_main(int ac, char **av)
{
	int	diffs = 0, gui = 1, verbose = 0, standalone = 0, resync = 0;
	int	stats = 0;
	int	c;
	int	rc = 0;
	longopt	lopts[] = {
		{ "stats", 320 }, /* --stats */
		{ 0, 0 }
	};

	while ((c = getopt(ac, av, "DTv", lopts)) != -1) {
		switch (c) {
		    case 'D': diffs = 1; break;
		    case 'T': gui = 0; break;
		    case 'v': gui = 0; verbose = 1; break;
		    case 320:	/* --stats */
			gui = 0; diffs = 1;
			stats = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	bk_nested2root(standalone);
	if (diffs) {
		char	*range;

		range = backtick("bk range -u", &rc);
		if (rc) goto out;
		if (gui) {
			rc = systemf("bk difftool -r%s", range);
		} else {
			rc = systemf("bk rset --elide -Hr%s | bk diffs %s -",
			    range, stats ? "--stats-only" : "-Hpu");
		}
		goto out;
	}
	if (exists("RESYNC/BitKeeper/etc/csets-in")) {
		chdir("RESYNC");
		resync = 1;
	}
	if (exists("BitKeeper/etc/csets-in")) {
		if (gui) {
			printf("Viewing %sBitKeeper/etc/csets-in\n",
			    resync ? "RESYNC/" : "");
			rc = SYSRET(system("bk changes -nd:I: - "
			    "< BitKeeper/etc/csets-in "
			    "| bk csettool -"));
		} else {
			rc = SYSRET(systemf("bk changes %s - "
			    "< BitKeeper/etc/csets-in", verbose ? "-v" : ""));
		}
	} else {
		fprintf(stderr, "Cannot find csets to view.\n");
		rc = 1;
	}
out:	return (rc);
}
