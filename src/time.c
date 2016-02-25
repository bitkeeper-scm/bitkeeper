/*
 * Copyright 2009-2010,2016 BitMover, Inc
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

/*
 * This is a mimimal subset of the linux time(1) command.
 */
int
time_main(int ac, char **av)
{
	int	c;
	char	*freeme = 0, *format = "%e secs\n";
	char	*p, *cmd = 0;
	int	rc;
	struct	timeval tv1, tv2;

	while ((c = getopt(ac, av, "c:f;", 0)) != -1) {
		switch (c) {
		    case 'c': cmd = strdup(optarg); break;
		    case 'f': freeme = format = strdup(optarg); break;
		    default: bk_badArg(c, av); rc = 1; goto out;
		}
	}
	unless ((cmd && cmd[0] && !av[optind]) ||
	    (!cmd && av[optind] && av[optind][0])) {
		usage();
	}
	unless (cmd) {
		char	**list = 0;

		for (c = optind; av[c]; c++) {
			list = addLine(list, shellquote(av[c]));
		}
		cmd = joinLines(" ", list);
		freeLines(list, free);
	}
	gettimeofday(&tv1, 0);
	rc = system(cmd);
	gettimeofday(&tv2, 0);
	rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 255;
	free(cmd);
	cmd = 0;

	for (p = format; *p; p++) {
		switch (*p) {
		    case '%':
			++p;
			switch (*p) {
			    case '%':
				fputc(*p, stderr);
				break;
			    case 'e':
				fprintf(stderr, "%.1f",
				    (float)(tv2.tv_sec - tv1.tv_sec) +
				    (tv2.tv_usec - tv1.tv_usec) / 1e6 + 0.05);
				break;
			    default:
				fputc('%', stderr);
				unless (*p) goto out; /* string ends in % */
				fputc(*p, stderr);
				break;
			}
			break;
		    case '\\':
			++p;
			switch (*p) {
			    case '\\': fputc(*p, stderr); break;
			    case 'n': fputc('\n', stderr); break;
			    default:
				fputc('\\', stderr);
				unless (*p) goto out; /* string ends in \ */
				fputc(*p, stderr);
				break;
			}
			break;
		    default:
			fputc(*p, stderr);
		}
	}
out:
	if (cmd) free(cmd);
	if (freeme) free(freeme);
	return (rc);
}
