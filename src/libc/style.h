/*
 * Copyright 2006-2008,2011-2012,2015-2016 BitMover, Inc
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

#ifndef	_STYLE_H_
#define	_STYLE_H_

#define	private		static
#define	max(a,b)	((a)>(b)?(a):(b))
#define	min(a,b)	((a)<(b)?(a):(b))
#define	bcopy(s,d,l)	memmove(d,s,l)
#define	bcmp(a,b,l)	memcmp(a,b,l)
#define	bzero(s,l)	memset(s,0,l)
#undef creat
#define creat(p,m)	open(p,O_CREAT|O_WRONLY|O_TRUNC,m)
#define	streq(a,b)	(!strcmp((a),(b)))
#define	strneq(a,b,n)	(!strncmp((a),(b),(n)))
#define	begins_with(a,b) ({				  \
	char	*_a = (a), *_b = (b);			  \
	int	_lenb = strlen(_b);			  \
	strneq(_a, _b, _lenb); })
#define	ends_with(a,b)	({				  \
	char	*_a = (a), *_b = (b);			  \
	int	_lena = strlen(_a);			  \
	int	_lenb = strlen(_b);			  \
	((_lena >= _lenb) && streq(_a + _lena - _lenb, _b));})
#define	index(s, c)	strchr(s, c)
#define	rindex(s, c)	strrchr(s, c)
#define	notnull(s)	((s) ? (s) : "")
#define	unless(e)	if (!(e))
#define	new(type)	(type*)calloc(1, sizeof(type))
#define	verbose(x)	unless (flags&SILENT) fprintf x
#define	strsz(s)	(sizeof(s) - 1)
#define fnext(buf, in)  fgets(buf, sizeof(buf), in)

/* types */
typedef unsigned char		u8;
typedef unsigned short		u16;
typedef unsigned int		u32;
typedef	unsigned long long	u64;
typedef signed char		i8;
typedef signed short		i16;
typedef signed int		i32;
typedef	signed long long	i64;

/* cast int's to and from pointers */
#define	int2p(i)			((void *)(long)(i))
#define	uint2p(i)			((void *)(unsigned long)(i))
#define	p2int(p)			((int)(long)(p))
#define	p2uint(p)			((unsigned int)(unsigned long)(p))

#ifdef	DEBUG
#	define	debug(x)	fprintf x
#else
#	define	debug(x)
#	define	debug_main(x)
#endif
#ifdef	DEBUG2
#	define	debug2(x)	fprintf x
#else
#	define	debug2(x)
#endif
#ifdef LINT
#	define	WHATSTR(X)	pid_t	getpid(void)
#else
#	define	WHATSTR(X)	static const char what[] = X
#endif
#define	HERE()	fprintf(stderr, "%s:%d\n", __FILE__, __LINE__)

#endif
