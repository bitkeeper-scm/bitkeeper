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
/*
 * The stucture of GUID looks like the following:
 * struct GUID {
 *	DWORD	Data1;
 *	WORD	Data2;
 *	WORD	Data3;
 *	BYTE	Data4[8];
 * };
 */
void
randomBits(char *buf)
{
	GUID guid;
	u32 h1, h2,l1, l2;

	CoCreateGuid(&guid); /* get 128 bits unique id */
	/*
	 * Convert 128 bit id to 64 bit id:
	 * Xor the hi 64 bits with the low 64 bits
	 */
	memmove(&h1, &(guid.Data1), sizeof (u32));
	memmove(&h2, &(guid.Data2), sizeof (u32));
	memmove(&l1, &(guid.Data4[0]), sizeof (u32));
	memmove(&l2, &(guid.Data4[3]), sizeof (u32));
	sprintf(buf,"%x%x", h1 ^ l1, h2 ^ l2);
}
#endif
