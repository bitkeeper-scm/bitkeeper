/*
 * Copyright 2008,2015-2016 BitMover, Inc
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
 * replace all occurances of 'search' in 'str' with 'replace' writing
 * the result to 'output'.
 *
 * if output=0, return the output in a malloc'ed string
 *
 * inplace edits are OK, if they don't make the string longer.
 */
char *
str_subst(char *str, char *search, char *replace, char *output)
{
	char	*p, *s, *t;
	int	slen, rlen, n;
	char	buf[MAXLINE];	/* limited, oh well. */

	unless (output) output = buf;
	slen = strlen(search);
	rlen = strlen(replace);
	if (str == output) assert(slen >= rlen);
	s = str;
	t = output;
	while (p = strstr(s, search)) {
		/* copy leading text */
		n = p - s;
		while (n--) *t++ = *s++;

		/* make subst */
		strcpy(t, replace);
		t += rlen;
		s += slen;
	}
	/* copy remaining text */
	while ((*t++ = *s++));
	if (output == buf) {
		assert((t - output) + strlen(s) < sizeof(buf));
		output = strdup(output);
	}
	return (output);
}
