/* generated automatically with cproto */
#ifndef	_STRING_H
#define	_STRING_H	1
#define	_STRING_H_	1

#include <stdio.h>

int bcmp(const void *b1, const void *b2, size_t length);
void bcopy(const void *src0, void *dst0, size_t length);
void bzero(void *b, size_t length);
int ffs(int mask);
char *index(const char *p, int ch);
void *memccpy(void *t, const void *f, int c, size_t n);
void *memchr(const void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *memmem(char *data, int datalen, char *sub, int sublen);
void *memmove(void *dst, const void *src, size_t length);
void *memset(void *dst0, int c0, size_t length);
char *rindex(const char *p, int ch);
int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
char *strcat(char *s, const char *append);
char *strchr(const char *p, int ch);
int strcmp(const char *s1, const char *s2);
char *strcpy(char *to, const char *from);
size_t strcspn(const char *s1, const char *s2);
char *strdup(const char *str);
char *strerror(int num);
size_t strlen(const char *str);
char *strncat(char *dst, const char *src, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
char *strncpy(char *dst, const char *src, size_t n);
char *strndup(const char *str, size_t n);
char *strpbrk(const char *s1, const char *s2);
char *strrchr(const char *p, int ch);
char *strsep(char **stringp, const char *delim);
size_t strspn(const char *s1, const char *s2);
char *strstr(const char *s, const char *find);
char *strcasestr(const char *s, const char *find);
char *strtok(char *s, const char *delim);

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
