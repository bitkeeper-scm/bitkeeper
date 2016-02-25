/*
 * Copyright 2010,2016 BitMover, Inc
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
 * Turn a path into a relative path with respect to "base".
 * This assumes input is in absolute paths that have some subpath
 * in common (ie, no c:/foo and f:/bar) and that cleanpath has
 * been run (no doubling // and no trailing /).
 *
 * E.g.
 * char * rel = relpath("/foo/bar/baz", "/foo/a/b");
 * rel will be "../../a/b"
 *
 * Which is the answer to "assuming I cd $base, what would I have to
 * type to get to path?"
 *
 * The only assumption is that both "path" and "base" start from the
 * same common directory. This works fine if they are both absolute
 * paths,
 * NOTE: if they are both paths from the root of a repo this could
 * fail because it doesn't handle '.', and doesn't need to handle
 * '.'.  If does need looser restrictions, then make sure the test
 * cases test them (such as relpath(".", "src")).
 */
char *
relpath(const char *base, const char *path)
{
	const char *p = path, *b = base;
	const char *lsb = 0;	/* last / */
	char	*r;
	char	rel[MAXPATH];

	assert(p && b);
	r = rel;
	*r = 0;
	while (*b && (*b == *p)) {
		if (*b == '/') lsb = b;
		++b, ++p;
	}
	unless (lsb) {
		/* must have a '/' in common */
		assert("relpath only works with "
		    "abs paths with something common\n" == 0);
	}
	if (*b == 0) {
		if (*p == 0) return (strdup(".")); /* same */
		if (b - 1 == lsb) p--;	/* base is root */
		if (*p == '/') return (strdup(p+1)); /* subpath */
	}
	unless ((*p == 0) && (*b == '/')) { /* unless pure ../.. */
		b = lsb;
		p = path + (lsb - base) + 1;
	}
	while (*b) {
		if (*b++ == '/') {
			assert(*b); /* trailing or subpath: neither allowed */
			strcpy(r, "../");
			r += 3;
		}
	}
	strcpy(r, p);
	r += strlen(p);
	if ((r > rel + 1) && (r[-1] == '/')) r[-1] = 0;
	return (strdup(rel));
}
