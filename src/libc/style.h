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
#define	index(s, c)	strchr(s, c)
#define	rindex(s, c)	strrchr(s, c)
#define setlinebuf(f)	setvbuf(f, NULL, _IOLBF, 0)
#define	notnull(s)	((s) ? (s) : "")
#define	unless(e)	if (!(e))
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

#endif
