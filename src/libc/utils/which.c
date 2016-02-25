/*
 * Copyright 2006,2016 BitMover, Inc
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

private int
absolute(char *path)
{
	/* Any C:whatever is viewed as absolute */
	if (win32() && isalpha(*path) && (path[1] == ':')) return (1);

	return ((*path == '/') ||
	    strneq("./", path, 2) || strneq("../", path, 3));
}

char *
which(char *exe)
{
        char	*path;
	char	*s, *t;
	char	buf[MAXPATH];

	if (executable(exe) && absolute(exe)) return (strdup(exe));

        path = aprintf("%s%c", getenv("PATH"), PATH_DELIM);
	s = strrchr(path, PATH_DELIM);
	if (s[-1] == PATH_DELIM) *s = 0;
	for (s = t = path; *t; t++) {
		if (*t == PATH_DELIM) {
			*t = 0;
			sprintf(buf, "%s/%s", *s ? s : ".", exe);
			if (executable(buf)) {
				free(path);
				s = strdup(buf);
				/* We had c:\build\buildenv\bin/cat */
				localName2bkName(s, s);
				return (s);
			}
			s = &t[1];
		}
	}
	free(path);

	return (0);
}
