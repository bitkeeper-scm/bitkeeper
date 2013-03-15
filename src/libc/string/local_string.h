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
