/*
 * Copyright 1999-2001,2006,2009-2010,2016 BitMover, Inc
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

int
samepath(char *a, char *b)
{
	char a1[MAXPATH],b1[MAXPATH];

	fullname(a, a1);
	fullname(b, b1);
	return (patheq(a1, b1));
}


/*
 * Check whether two paths "overlap".
 * Do any of the leading path components match?
 *
 * Returns 0 if paths are non-overlapping, or
 * Returns index of first unmatching point if
 * one of the paths passed in is a complete subpath of the other.
 *    abase[ret] == 0, if abase is a subset of bbase (includes matching)
 *    abase[ret] == '/', if bbase is a subset of abase (not matching)
 *    bbase[ret] == 0, if bbase is a subset of abase (includes matching)
 *    bbase[ret] == '/', if abase is a subset of bbase (not matching)
 * Note: if streq(abase, "/") || streq(bbase, "/"), then ret could be
 * 0 and still a match.  However, shouldn't happen so we don't put an assert
 * for that.  Could avoid the problem of 0 index being a valid return by
 * returning "char *a", which is what we want on the returning side anyway.
 * But then the returning index is symmetrical -- applies both to abase
 * and bbase.
 */
int
paths_overlap(char *abase, char *bbase)
{
	char	*a = abase, *b = bbase;

	assert(*a && *b);
	while (*a && (*a == *b)) ++a, ++b;

	return ((((*a == 0) || (*a == '/')) &&
	    ((*b == 0) || (*b == '/'))) ? (a - abase) : 0);
}

#ifdef WIN32

/* make comparisons case insensitive and forward slash vs backslash agnostic */
int
patheq(char *a, char *b)
{
	char	*ad = strdup(a);
	char	*bd = strdup(b);
	int	rc;

	assert(ad && bd);
	localName2bkName(ad, ad);
	localName2bkName(bd, bd);
	rc = !strcasecmp(ad, bd);
	free(ad);
	free(bd);
	return (rc);
}

#else	/* UNIX */

int
patheq(char *a, char *b)
{
	return(streq(a, b));
}
#endif
