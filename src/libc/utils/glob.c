/*
 * Copyright 1999-2004,2006,2008-2010,2012,2016 BitMover, Inc
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
#define		C(c)	(ignorecase ? tolower(c) : c)
#ifdef	TEST
#undef	calloc
#undef strdup
#endif

/*
 * This file contains routines to match file names against lists of
 * shell glob patterns.  We support only ? * [...] as metacharacters.
 * \ escapes the following character.  You are expected to provide
 * only the filename, not its path (therefore there is no special
 * treatment of /).
 */

private int
glob_equals(void)
{
	static int doit = -1;

	if (doit == -1) {
		char	*s = getenv("BK_GLOB_EQUAL");

		doit = s && streq(s, "YES");
	}
	return (doit);
}

char	*
is_glob(char *glob)
{
	int	escape = 0;
	char	*p = glob;

	unless (glob && *glob) return (0);

	for ( ; *glob; glob++) {
		if (*glob == '/') {
			p = glob;
			continue;
		}
		if (*glob == '\\') {
			escape = !escape;
			continue;
		}
		if (escape) {
			escape = 0;
			continue;
		}
		switch (*glob) {
		    case '=':	unless (glob_equals()) break;
		    case '?':
		    case '*':
		    case '[':	return (p);
		}
	}
	return (0);
}

/* Match a string against one glob pattern.  */
int
match_one(char *string, char *glob, int ignorecase)
{
	u8 	*p, *g;
	int	invert, match;

	p = string;
	g = glob;
	while (*g) {
		switch (*g) {
		    case '?':
			if (*p) break;
			return (0);
		    case '[':
			g++;
			if (invert = (*g == '^')) g++;
			unless (strchr(g, ']')) {
				fprintf(stderr, "bad glob: %s\n", glob);
				return (0);
			}
			match = 0;
			do {
				// XXX - we don't make sure it is not [a-]
				// XXX - we don't support [a\-xyz]
				if (g[1] == '-') {
					if ((C(*g) <= C(*p)) &&
					    (C(*p) <= C(g[2]))) {
					    	match = 1;
					}
					g += 2;
				} else if (C(*g) == C(*p)) {
					match = 1;
				}
			} while (*++g != ']');
			if (match ^ invert) break;
			return (0);
		    case '=':
		    	unless (glob_equals()) {
				if (C(*p) == C(*g)) break;
				return (0);
			}
			/* fall through */
		    case '*':
			g++;
			unless (*g) return (1);
			goto star;
		    case '\\':
			g++;
		    default:
			if (C(*p) == C(*g)) break;
			return (0);
		}
		p++;
		g++;
	}
	return (*p == 0);

	/* Star in the middle of a pattern is handled by scanning
	 * ahead for the next place the pattern might match, then
	 * recursively calling match_one with the rest of the pattern.
	 * If the next char in the pattern is not a metacharacter, we
	 * can seek forward rapidly; otherwise we can't.
	 */
star:	while (*p) {
		unless (*g == '\\' || *g == '[' || *g == '?' || *g == '*') {
			while (*p && C(*p) != C(*g)) p++;
			unless (*p) return (0);
		}
		if (match_one(p, g, ignorecase)) return (1);
		p++;
	}
	return (0);
}

/* If the string matches any of the globs in the vector, return a pointer
 * to the first glob that matched.  Otherwise return NULL.
 */
char *
match_globs(char *string, char **globs, int ignorecase)
{
	int i;
	if (globs == NULL) return NULL;

	EACH (globs) {
		if (match_one(string, globs[i], ignorecase)) {
			return globs[i];
		}
	}
	return NULL;
}
