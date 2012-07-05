#include "system.h"

/*
 * Generic routines to convert between network and byte order.
 * These work regardless of the wordsize or byte order of the
 * processor.
 *
 * In almost all machines ntoh and hton are equilivant operations, but
 * they are both coded here.
 */

uint16
_ntohs(uint16 s)
{
	uchar	*p = (uchar *)&s;

	return	(p[0] << 8 |
	         p[1]);
}

uint16
_htons(uint16 s)
{
	/* The ugly union is to workaround a compiler bug in redhat71 */
	union {
		uchar	p[2];
		uint16	f;
	} u;

	u.p[0] = (s >> 8) & 0xff;
	u.p[1] = (s)      & 0xff;
	return (u.f);
}

uint32
_ntohl(uint32 s)
{
	uchar	*p = (uchar *)&s;

	return	(p[0] << 24 |
		 p[1] << 16 |
		 p[2] << 8 |
	         p[3]);
}

uint32
_htonl(uint32 s)
{
	union {
		uchar	c[4];
		uint32	i;
	} p;

	p.c[0] = (s >> 24) & 0xff;
	p.c[1] = (s >> 16) & 0xff;
	p.c[2] = (s >> 8)  & 0xff;
	p.c[3] = (s)       & 0xff;

	return	(p.i);
}
