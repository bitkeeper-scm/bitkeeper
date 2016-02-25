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

#include "system.h"

/*
 * Trim leading/trailing white space.  Ours is a little different than
 * some in that it shifts left so that you either get back the same
 * address or NULL.  Nicer for free().
 *
 * If you want trimdup() that is #define trimdup(s) trim(strdup(s))
 */
char *
trim(char *buf)
{
	char	*s, *t;
	char	*trailing = 0;	/* just past last non-space char */

	unless (buf) return (0);
	s = t = buf;		/* src and dest */
	while (*s && isspace(*s)) s++;
	while (1) {
		if (t != s) *t = *s;
		unless (*s) break;
		t++;
		unless (isspace(*s)) trailing = t;
		s++;
	}
	if (trailing) *trailing = 0;
	return (buf);
}
