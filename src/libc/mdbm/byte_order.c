/*
 * Copyright 1999-2001,2004,2006,2011-2012,2016 BitMover, Inc
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
	/* [rick] ugly? It's a common embedded sys idiom and compiles well */
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
		uchar	p[4];
		uint32	f;
	} u;

	u.p[0] = (s >> 24) & 0xff;
	u.p[1] = (s >> 16) & 0xff;
	u.p[2] = (s >> 8)  & 0xff;
	u.p[3] = (s)       & 0xff;
	return (u.f);
}

uint16
_le16toh(uint16 s)
{
	uchar	*p = (uchar *)&s;

	return	(p[0] | p[1] << 8);
}

uint16
_htole16(uint16 s)
{
	union {
		uchar	p[2];
		uint16	f;
	} u;

	u.p[0] = (s)       & 0xff;
	u.p[1] = (s >> 8)  & 0xff;

	return	(u.f);
}

uint32
_le32toh(uint32 s)
{
	uchar	*p = (uchar *)&s;

	return	(p[0] |
		 p[1] << 8 |
		 p[2] << 16 |
	         p[3] << 24);
}

uint32
_htole32(uint32 s)
{
	union {
		uchar	p[4];
		uint32	f;
	} u;

	u.p[0] = (s)       & 0xff;
	u.p[1] = (s >> 8)  & 0xff;
	u.p[2] = (s >> 16) & 0xff;
	u.p[3] = (s >> 24) & 0xff;

	return	(u.f);
}
