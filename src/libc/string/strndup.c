#include "string/str.cfg"
#ifdef BK_STR_STRNDUP
/*
 * Copyright (c) 2004
 * 	Bitmover, Inc
 */
#include "local_string.h"

char	*
strndup(const char *s, size_t len)
{
    	char	*p;

	if (p = malloc(len+1)) {
		strncpy(p, s, len);
		p[len] = 0;
	}
	return (p);
}

#endif
