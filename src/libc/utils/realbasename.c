/*
 * Copyright 2009,2016 BitMover, Inc
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

#define	FSLAYER_NODEFINES
#include "system.h"

#ifndef WIN32

char *
realBasename(const char *path, char *realname)
{
	/* implement me */
	assert(0);
	return(0);
}

#else  /* WIN32 */

char *
realBasename(const char *path, char *realname)
{
	HANDLE	h;
	WIN32_FIND_DATA	data;
	char	*p;

	if ((h = FindFirstFile(path, &data)) == INVALID_HANDLE_VALUE) {
		assert(strchr(path, '\\') == 0);
		if (p = strrchr(path, '/')) {
			strcpy(realname, ++p);
		} else {
			strcpy(realname, path);
		}
	} else {
		strcpy(realname, data.cFileName);
		FindClose(h);
	}
	return (realname);
}
#endif	/* WIN32 */
