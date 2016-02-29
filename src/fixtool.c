/*
 * Copyright 2006,2013,2016 BitMover, Inc
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

private int	fixtool(char *file, int ask, char *diffopts);

int
fixtool_main(int ac,  char **av)
{
	int	c;
	int	ask = 1;
	FILE	*f;
	char	**nav = 0;
	char	diffopts[20];
	char	buf[MAXPATH];

	strcpy(diffopts, "-");
	while ((c = getopt(ac, av, "fpsuw", 0)) != -1) {
		switch (c) {
		    case 'u': strcat(diffopts, "u"); break;
		    case 'p': strcat(diffopts, "p"); break;
		    case 's': strcat(diffopts, "s"); break;
		    case 'w': strcat(diffopts, "w"); break;
		    case 'f': ask = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	if (av[optind] == 0) {
		if (proj_cd2root()) {
			fprintf(stderr,
			    "fixtool: unable to find package root.\n");
		    	exit(1);
		}
	}
	unless (diffopts[1]) strcpy(diffopts, "");
	nav = addLine(nav, "bk");
	nav = addLine(nav, "gfiles");
	nav = addLine(nav, "-cg");
	while (av[optind]) nav = addLine(nav, av[optind++]);
	nav = addLine(nav, 0);
	f = popenvp(&nav[1], "r");
	unless (fnext(buf, f)) {
		fprintf(stderr, "fixtool: nothing to fix.\n");
	} else do {
		chomp(buf);
		if (fixtool(buf, ask, diffopts)) break;
	} while (fnext(buf, f));
	pclose(f);
	freeLines(nav, 0);
	return (0);
}

private int
fixtool(char *file, int ask, char *diffopts)
{
	int	ret;
	char	*p, *prev, *merge;
	char	buf[MAXPATH];

	if (ask) {
		system("bk clear");
		p = aprintf("bk diffs %s '%s' | %s", diffopts, file, pager());
		system(p);
		free(p);
		fflush(stdout);
		p = aprintf("\nFix %s? y)es q)uit n)o u)nedit: [no]", file);
		ret = prompt(p, buf);
		free(p);
		unless (ret) return (0);
		switch (buf[0]) {
		    case 'y': break;
		    case 'q': return (1);
		    case 'u': sys("bk", "unedit", file, SYS); return (0);
		    case 'n': return (0);
		    default:  return (0);
		}
	}
	prev = bktmp(0);
	sysio(0, prev, 0, "bk", "get", "-kqpr+", file, SYS);
	merge = bktmp(0);
	sys("bk", "fmtool", prev, file, merge, SYS);
	if (size(merge) == 0) {
		unlink(prev);
		unlink(merge);
		free(prev);
		free(merge);
		return (0);
	}
	sprintf(buf, "%s~", file);
	rename(file, buf);
	if (fileCopy(merge, file)) {
		perror(merge);
	} else {
		unlink(merge);
	}
	unlink(prev);
	free(prev);
	free(merge);
	return (0);
}
