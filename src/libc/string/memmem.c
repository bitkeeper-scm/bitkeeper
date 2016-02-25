/*
 * Copyright 2006,2015-2016 BitMover, Inc
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

#include "string/str.cfg"
#ifdef BK_STR_MEMMEM
#include "local_string.h"

/* Local extention to standard C library, so sue me. */

/* Return the first occurrence of SUB in DATA  */
char *
memmem(char *data, int datalen, char *sub, int sublen)
{
	char	*p;
	char	*end = data + datalen - sublen;

	if (sublen == 0) return (data);
	if (sublen > datalen) return (0);

	for (p = data; p <= end; ++p) {
		if ((*p == *sub) && !memcmp(p + 1, sub + 1, sublen - 1)) {
			return (p);
		}
	}
	return (0);
}
#endif
