/*
 * Copyright 2016 BitMover, Inc
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

private	void	stream_patch(int strip, FILE *f, FILE *fout, int co_only);

/*
 * a wrapper around the systems 'patch(1)' utility.
 *
 * It reads a patch from stdin and feeds it to the real patch command.
 * It reads any filenames and tries to checkout any bk files.
 */
int
patch_main(int ac, char **av)
{
	FILE	*f, *fout = NULL;
	int	i, c;
	int	strip = -1;
	int	co = 0;
	char	*input = 0;
	char	*dir = 0;

	start_cwd = strdup(proj_cwd());

	/*
	 * Now we need to look at the command line.
	 * We need:
	 *   - -i/--input <patchfile>
	 *         read patch from file
	 *   - -p/--strip <num>
	 *	   to know how to string pathnames)
	 *   - -d/--directory <dir>
	 *	   switch to directory first
	 */
	for (i = 1; av[i]; i++) {
		unless (av[i][0] == '-') continue;
		if (streq(av[i], "--")) break;
		if (av[i][1] == '-') {
			// --long-option
			c = strcspn(av[i], "=");
			if (strneq(av[i]+2, "input", c)) {
				input = av[i][c] ? av[i] + c + 1 : av[i+1];
			} else if (strneq(av[i]+2, "strip", c)) {
				strip = atoi(av[i][c] ? av[i] + c + 1 : av[i+1]);
			} else if (strneq(av[i]+2, "directory", c)) {
				dir = av[i][c] ? av[i] + c + 1 : av[i+1];
			} else if (streq(av[i]+2, "checkout-only")) {
				co = 1;
			}
		} else {
			c = strcspn(av[i], "ipdg");
			switch (av[i][c]) {
			    case 'i':
				input = av[i][c+1] ? av[i] + c + 1 : av[i+1];
				break;
			    case 'p':
				strip = atoi(av[i][c+1] ?
				    av[i] + c + 1 : av[i+1]);
				break;
			    case 'd':
				dir = av[i][c+1] ? av[i] + c + 1 : av[i+1];
				break;
			    default:
				break;
			}
		}
	}
	if (dir && chdir(dir)) {
		fprintf(stderr, "%s: failed to chdir to '%s'\n", prog, dir);
		return (1);
	}
	if (input) {
		unless (f = fopen(input, "r")) {
			fprintf(stderr, "%s: unable to read '%s'\n",
			    prog, input);
			return (1);
		}
		fout = fopen(DEVNULL_WR, "w");
		stream_patch(strip, f, fout, co);
	}
	if (dir) chdir(start_cwd);

	/* first start the patch command */
	unless (co || (fout = popenvp(av, "w"))) {
		fprintf(stderr, "Failed to find 'patch' command on PATH.\n");
		return (1);
	}
	unless (input) {
		if (dir) chdir(dir);

		stream_patch(strip, stdin, fout, co);
	}
	if (fout) return (SYSRET(pclose(fout)));
	return (0);
}

private int
checkout(char *file, int strip, int co_only)
{
	char	*p;
	int	rc;

//fprintf(stderr, "checkout(%s, %d)\n", file, strip);
	if (strip < 0) {
		file = basenm(file);
	} else {
		while (strip-- > 0) {
			unless (p = strchr(file, '/')) {
				 /* not enough /'s for strip count */
				return (-2);
			}
			while (*p == '/') p++;
			file = p;
		}
	}
	rc = get(file, SILENT|GET_EDIT|GET_NOREGET);
	if (co_only) printf("checked out %s\n", file);
//fprintf(stderr, "get(%s) = %d\n", file, rc);
	return (rc);
}

private void
stream_patch(int strip, FILE *f, FILE *fout, int co_only)
{
	char	*line, *p;
	char	**items;
	int	i;
	size_t	len;

	/*
	 * some sample outputs:
	 *
	 * diff -ur a/file1 b/file1
	 * --- a/file    2016-06-14 09:40:06.855128226 -0400
	 * +++ b/file    2016-06-14 09:40:11.595106909 -0400
	 *
	 * diff -ur "a/file 1" "b/file 1"
	 * --- "a/file 1"    2016-06-14 09:40:06.855128226 -0400
	 * +++ "b/file 1"    2016-06-14 09:40:11.595106909 -0400
	 *
	 * diff -Nur "a/\"quote" "b/\"quote"
	 * --- "a/\"quote"    1969-12-31 19:00:00.000000000 -0500
	 * +++ "b/\"quote"    2016-06-14 09:42:23.306514641 -0400
	 */
	while (line = fgetln(f, &len)) {
		if (((len > 7) && strneq(line, "Index: ", 7)) ||
		    ((len > 5) && strneq(line, "diff ", 5)) ||
		    ((len > 4) && strneq(line, "--- ", 4))) {
			p = strndup(line, len);
			items = shellSplit(p);
			free(p);
			i = 2;	/* item to return */
			if (streq(items[1] , "diff")) {
				/* skip arguments to diff */
				EACH_START(2, items, i) {
					if (items[i][0] != '-') break;
				}
			}
			if (i <= nLines(items)) checkout(items[i], strip, co_only);
			freeLines(items, free);
		}
		unless (co_only) fwrite(line, 1, len, fout);
	}
}
