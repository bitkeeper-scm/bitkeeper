#ifdef WIN32
#include <objbase.h>
#endif
#include "../system.h"
#include "../sccs.h"

#ifndef WIN32
/*
 * Dig out more information to make the file unique and stuff it in buf.
 * Use /dev/urandom, /dev/random if present.  Otherwise use anything else
 * we can find.
 */
void
randomBits(char *buf)
{
	int	fd;
    	u32	a, b;

	if (((fd = open("/dev/urandom", 0, 0)) >= 0) ||
	    ((fd = open("/dev/random", 0, 0)) >= 0)) {
		read(fd, &a, 4);
		read(fd, &b, 4);
		close(fd);
	} else {
		/* XXX This is not nearly as random as it should be.  */
		struct timeval tv;
		u32 x, y;

		gettimeofday(&tv, NULL);
		x = (u32)(long)sbrk(0) ^ (u32)(long)&a;
		y = ((u32)getpid() << 16) + (u32)getuid();
		y ^= tv.tv_usec;

		a = (x & 0xAAAAAAAA) + (y & 0x55555555);
		b = (y & 0xAAAAAAAA) + (x & 0x55555555);
	}
	sprintf(buf, "%x%x", a, b);
}
#else
void
randomBits(char *buf)
{
	GUID guid;

	CoCreateGuid(&guid); /* get 128 bits unique id */
	assert(guid.Data1 != 0);
	/* convert to 64 bits id */
	sprintf(buf,"%.8x", (DWORD) (guid.Data1 * getpid()));

}
#endif
