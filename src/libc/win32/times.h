/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_TIMES_H_
#define	_TIMES_H_
/* unix times.h simulation, Andrew Chang 1998 */

struct timezone
{
	int not_used;
};

extern void gettimeofday(struct timeval *, struct timezone *);
#endif /* _TIMES_H_ */

