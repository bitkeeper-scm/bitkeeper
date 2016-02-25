/*
 * Copyright 2004-2007,2012-2013,2016 BitMover, Inc
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

#ifndef	_LOCAL_STRING_H
#define	_LOCAL_STRING_H

#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <strings.h>	// BSD funcs

#ifndef	MK_STR_CFG
#include "string/str.cfg"
#endif

#ifdef	BK_STR_MEMMEM
char *memmem(char *data, int datalen, char *sub, int sublen);
#endif
#ifdef	BK_STR_STRCASESTR
char *strcasestr(const char *s, const char *find);
#endif
#ifdef	BK_STR_STRNDUP
char *strndup(const char *str, size_t n);
#endif
#ifdef	BK_STR_STRSEP
char *strsep(char **stringp, const char *delim);
#endif

/* bk defined functions */
char *trim(char *s);
int strcnt(char *p, char ch);

/*
 * Returns pointer to first charector in 's' that contains one of the
 * characters in 'chrs', or NULL if none found.
 */
static inline char *
strchrs(char *s, char *chrs)
{
	char	*ret;

	ret = s + strcspn(s, chrs);
	return (*ret ? ret : 0);
}
#endif
