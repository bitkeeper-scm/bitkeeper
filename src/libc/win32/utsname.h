/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_UTSNAME_H_
#define	_UTSNAME_H_
/* unix utsname.h simulation, Andrew Chang 1998 */

struct	utsname
{
	char sysname[NBUF_SIZE];
	char nodename[NBUF_SIZE];
};

extern int uname(struct utsname *);
#endif /* _USTNAME_H_ */

