/*
 * Copyright (c) 2004
 * 	Bitmover, Inc
 */

#include <string.h>
#include <stdlib.h>

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

