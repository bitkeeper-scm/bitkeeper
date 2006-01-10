/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_FTW_H_
#define	_FTW_H_
/* Unix ftw.h simulation, Andrew Chang 1998 */
/* TODO: match the defined values with Unix counter part */
#define FTW_NS  1
#define FTW_D   2
#define FTW_F   3
#define FTW_DNR 4

extern int ftw(const char *, int(*func)(const char *, struct stat *, int), int);
#endif /* _FTW_H_ */

