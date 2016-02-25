/*
 * Copyright 1999-2001,2003-2004,2006,2009-2010,2015-2016 BitMover, Inc
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

/*
 * Given a pathname, make the directory.
 */
int
mkdirp(char *dir)
{
	char	*t = 0;

	while (1) {
		unless (mkdir(dir, 0777)) break;
		if (errno != ENOENT) {
			if (t) *t = '/';
			return (-1);
		}

		/*
		 * some component in the pathname doesn't exist
		 * go back one at a time until we find it
		 * for any other errno, we can just quit
		 */
		if (t) {
			*t-- = '/';
			while ((t > dir) && (*t != '/')) --t;
			if (t == dir) return (-1);
		} else {
			unless ((t = strrchr(dir, '/')) && (t > dir)) {
				return (-1);
			}
		}
		*t = 0;
	}
	/*
	 * Now if we had to go back to make a one of the component
	 * directories, we walk forward and build the path in order
	 */
	while (t) {
		*t++ = '/';
		while (*t && (*t != '/')) t++;
		if (*t) {
			*t = 0;
		} else {
			t = 0;
		}
		if (mkdir(dir, 0777)) {
			if (t) *t = '/';
			return (-1);
		}
	}
	return (0);
}

/*
 * Given a pathname:
 * return 0 if access(2) indicates mkdirp() will succeed
 * return -1 if access(2) indicates mkdirp() will fail, and set errno.
 */
int
test_mkdirp(char *dir)
{
	char	buf[MAXPATH];
	char	*t;
	int	ret;

	if (IsFullPath(dir)) {
		strcpy(buf, dir);
	} else {
		strcpy(buf, "./");
		strcat(buf, dir);
	}
	while (((ret = access(buf, W_OK)) == -1) && (errno == ENOENT)) {
		t = strrchr(buf, '/');
		unless (t) break;
		*t = 0;
	}
	if (!ret && !writable(buf)) ret = -1;
	return (ret);
}

/*
 * given a pathname, create the dirname if it doesn't exist.
 */
int
mkdirf(char *file)
{
	char	*s;
	int	ret;

	unless (s = strrchr(file, '/')) return (0);
	*s = 0;
	if (isdir_follow(file)) {
		*s = '/';
		return (0);
	}
	ret = mkdirp(file);
	*s = '/';
	return (ret);
}
