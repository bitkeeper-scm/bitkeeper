/*
 * Copyright 1999-2007,2009-2010,2014-2016 BitMover, Inc
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

#include "sccs.h"
#include "bkd.h"

/*
 * Show bkd version
 */
int
cmd_version(int ac, char **av)
{
	out("OK-");
	out(BKD_VERSION1_2);
	out("\n");
	return (0);
}

/*
 * remote enabled commands need to be in a bkd_*.c file
 */

int
version_main(int ac, char **av)
{
	int	c;
	char	*p;
	char	**remcmd, *freeme;
	int	i;
	int	justver = 0;
	int	rc;

	while ((c = getopt(ac, av, "s", 0)) != -1) {
		switch (c) {
		    case 's':
			justver = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}

	if (av[optind] && av[optind+1]) {
		fprintf(stderr,
		    "%s: only one URL on the command line\n", prog);
		return (1);
	}
	if (av[optind]) {
		freeme = aprintf("-q@%s", av[optind]);
		remcmd = addLine(0, "bk");
		remcmd = addLine(remcmd, freeme);
		for (i = 0; i < optind; i++) remcmd = addLine(remcmd, av[i]);
		remcmd = addLine(remcmd, 0);
		rc = spawnvp(_P_WAIT, "bk", &remcmd[1]);
		rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 99;
		freeLines(remcmd, 0);
		free(freeme);
		return (rc);
	}

	if (justver) {
		p = bk_vers;
		if (strneq(p, "bk-", 3)) p += 3;
		puts(p);
		return (0);
	}

	bkversion(stdout);
	return (0);
}

void
bkversion(FILE *f)
{
	time_t	now = time(0);
	char	new_vers[64];
	char	new_utc[16];
	char	buf[MAXLINE];

	buf[0] = 0;
	fprintf(f, "BitKeeper version is ");
	if (*bk_tag) {
		fprintf(f, "%s ", bk_tag);
		unless (strchr(bk_tag, '+')) fprintf(f, "%s ", bk_utc);
	} else {
		fprintf(f, "%s ", bk_utc);
	}
	fprintf(f, "for %s\n", bk_platform);
	fflush(f);	/* put out ver info while waiting for lease */
	fprintf(f, "Built by: %s in %s\n", bk_build_user, bk_build_dir);

	strftime(buf, sizeof(buf), "%a %b %d %Y %H:%M:%S %Z",
	    localtimez(&bk_build_timet, 0));
	fprintf(f, "Built on: %s (%s ago)\n", buf,
	    age(now - bk_build_timet, " "));

	fprintf(f, "Running on: %s\n", platform());

	/* latest version information */
	unless (upgrade_latestVersion(new_vers, new_utc) ||
	    streq(new_vers, bk_vers)) {
		fprintf(f, "Latest version: %s  (released %s ago)\n",
		    new_vers,
		    age(now - sccs_date2time(new_utc, 0), " "));
	}

	if (test_release) {
		float	exp;

		exp = ((time_t)bk_build_timet - now + test_release) / (float)DAY;
		if (exp > 0) {
			fprintf(f, "Expires in: %.1f days (test release).\n",
			    exp);
		} else {
			fprintf(f, "Expired (test release).\n");
		}
	}
}
