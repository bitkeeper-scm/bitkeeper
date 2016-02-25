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

/*
 * concatenate two paths "first" and "second", and put the result in "buf"
 * first or second may be null.
 */
void
concat_path(char *buf, char *first, char *second)
{
	int	len;

	if (buf == second) {
		/*
		 * insert first before buf
		 * ex: unless (IsFullPath(foo)) concat_path(foo, proj_cwd(), foo)
		 */
		assert(buf != first);
		unless (first && *first) return;
		if (second && *second) {
			len = strlen(first);
			memmove(buf+len+1, buf, strlen(buf)+1);
			memcpy(buf, first, len);
			buf[len] = '/';
		} else {
			strcpy(buf, first);
		}
		return;
	}

	unless (first && *first) {
		strcpy(buf, second);
		return;
	}
	if (buf != first) strcpy(buf, first);
	unless (second && *second) return;

	len = strlen(buf);
	/* first trim trailing . off string ending in /. */
	if ((len >= 2) && (buf[len-2] == '/') && (buf[len-1] == '.')) {
		buf[--len] = 0;
	}

	/* if second starts with /, then skip it. */
	if (*second == '/') ++second;

	/* if first doesn't end with /, then add it */
	unless (buf[len-1] == '/') buf[len++] = '/';

	/* append second to end of first */
	strcpy(&buf[len], second);
}
