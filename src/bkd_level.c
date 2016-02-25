/*
 * Copyright 2001-2003,2010-2012,2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"

#define	LEVEL "BitKeeper/etc/level"

int
level_main(int ac,  char **av)
{
	int	c;
	int	justlist = 0;

	while ((c = getopt(ac, av, "l", 0)) != -1) {
		switch (c) {
		    case 'l': justlist = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	unless (av[optind]) {
		unless (justlist) printf("Repository level is ");
		printf("%d\n", getlevel());
		return (0);
	}
	if (justlist) usage();
	unless (isdigit(av[optind][0])) usage();
	if (getenv("_BK_IN_BKD")) {
		/* cross dependency w/ bkd.c, see comment in same cset */
		unless (strstr(getenv("_BKD_OPTS"), "-U")) {
			fprintf(stderr, "cannot set level remotely\n");
			exit(1);
		}
	}
	return (setlevel(atoi(av[optind])));
}

int
setlevel(int level)
{
	char	*root, *lfile;
	FILE	*f;

	unless (root = proj_root(proj_product(0))) {
		fprintf(stderr, "setlevel: Error: cannot find package root\n");
		return (1);
	}

	lfile = aprintf("%s/%s", root, LEVEL);
	unless (f = fopen(lfile, "wt")) {
		perror(lfile);
		free(lfile);
		return (1);
	}
	fprintf(f, "# This is the repository level, do not delete.\n");
	fprintf(f, "%d\n", level);
	fclose(f);
	free(lfile);
	return (0);
}

int
getlevel(void)
{
	char	*root, *lfile;

	unless (root = proj_root(proj_product(0))) {
		fprintf(stderr, "getlevel: Error: cannot find package root\n");
		return (1); /* should we force a -1 here ? */
	}
	lfile = aprintf("%s/%s", root, LEVEL);
	if (exists(lfile)) {
		char	buf[200];
		FILE	*f;

		unless (f = fopen(lfile, "rt")) return (1000000);
		/* skip the header */
		fgets(buf, sizeof(buf), f);
		if (fgets(buf, sizeof(buf), f)) {
			fclose(f);
			free(lfile);
			return (atoi(buf));
		}
	}
	free(lfile);
	return (1);
}
