#include "string/str.cfg"
#ifdef BK_STR_STRCASESTR
#include "local_string.h"

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * %sccs.include.redist.c%
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "%W% (Berkeley) %G%";
#endif /* LIBC_SCCS and not lint */

#include <string.h>
#include <ctype.h>

/*
 * Find the first occurrence of find in s.
 */
char *
strcasestr(s, find)
	register const char *s, *find;
{
	register char c, sc;
	register size_t len;

	if ((c = toupper(*find++)) != 0) {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while (toupper(sc) != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *)s);
}
#endif
