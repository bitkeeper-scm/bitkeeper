/*
 * Copyright 2000-2005,2014-2016 BitMover, Inc
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

/*
 * _find - find regular files and symbolic links
 */
private int	do_print(char *path, char type, void *data);
private	char 	**globs = 0;
private	int	wantdir = 0;

int
find_main(int ac, char **av)
{
	int	i;
	char	**dirs = 0;


	for (i = 1; av[i]; i++) {
		if (streq("-name", av[i])) {
			globs = addLine(globs, strdup(av[++i]));
		} else if (streq("-type", av[i])) {
			++i;
			if (streq("d", av[i])) wantdir = 1;
		} else {
			dirs = addLine(dirs, strdup(av[i]));
		}
	}
	unless (dirs) dirs = addLine(0, strdup("."));
	EACH (dirs) {
		walkdir(dirs[i], (walkfns){ .file = do_print}, 0);
	}
	freeLines(dirs, free);
	if (globs) freeLines(globs, free);
	return (0);
}

private int
do_print(char *path, char type, void *data)
{
	char	*t;
	int	isdir = (type == 'd');

	if (strneq(path, "./", 2)) path += 2;
	t = strrchr(path, '/');
	t = t ? (t+1) : path;

	if ((wantdir && isdir) || (!wantdir && !isdir)) {
		unless (globs && !match_globs(t, globs, 0)) {
			printf("%s\n", path);
		}
	}
	return (0);
}
