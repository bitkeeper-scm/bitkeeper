/*	$NetBSD: stdio.h,v 1.69 2007/05/30 21:14:37 tls Exp $	*/

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)stdio.h	8.5 (Berkeley) 4/29/95
 */

#ifndef	_STDIO_H_
#define	_STDIO_H_

#include "stdio_remap.h"

#include <stdarg.h>
#ifndef	WIN32
#include <unistd.h>
#endif
#include <sys/types.h>

#ifndef	NULL
#define	NULL	0
#endif

#ifndef __restrict
#define __restrict
#endif

/*
 * This is fairly grotesque, but pure ANSI code must not inspect the
 * innards of an fpos_t anyway.  The library internally uses off_t,
 * which we assume is exactly as big as eight chars.
 */
typedef off_t fpos_t;

/*
 * NB: to fit things in six character monocase externals, the stdio
 * code uses the prefix `__s' for stdio objects, typically followed
 * by a three-character attempt at a mnemonic.
 */

/* stdio buffers */
struct __sbuf {
	unsigned char *_base;
	int	_size;
};

/*
 * stdio state variables.
 *
 * The following always hold:
 *
 *	if (_flags&(__SLBF|__SWR)) == (__SLBF|__SWR),
 *		_lbfsize is -_bf._size, else _lbfsize is 0
 *	if _flags&__SRD, _w is 0
 *	if _flags&__SWR, _r is 0
 *
 * This ensures that the getc and putc macros (or inline functions) never
 * try to write or read from a file that is in `read' or `write' mode.
 * (Moreover, they can, and do, automatically switch from read mode to
 * write mode, and back, on "r+" and "w+" files.)
 *
 * _lbfsize is used only to make the inline line-buffered output stream
 * code as compact as possible.
 *
 * _ub, _up, and _ur are used when ungetc() pushes back more characters
 * than fit in the current _bf, or when ungetc() pushes back a character
 * that does not match the previous one in _bf.  When this happens,
 * _ub._base becomes non-nil (i.e., a stream has ungetc() data iff
 * _ub._base!=NULL) and _up and _ur save the current values of _p and _r.
 *
 * NB: see WARNING above before changing the layout of this structure!
 */
typedef	struct __sFILE {
	unsigned char *_p;	/* current position in (some) buffer */
	int	_r;		/* read space left for getc() */
	int	_w;		/* write space left for putc() */
	unsigned short _flags;	/* flags, below; this FILE is free if 0 */
	short	_file;		/* fileno, if Unix descriptor, else -1 */
	struct	__sbuf _bf;	/* the buffer (at least 1 byte, if !NULL) */
	int	_lbfsize;	/* 0 or -_bf._size, for inline putc */

	/* operations */
	void	*_cookie;	/* cookie passed to io functions */
	int	(*_close)(void *);
	int	(*_read) (void *, char *, int);
	fpos_t	(*_seek) (void *, fpos_t, int);
	int	(*_write)(void *, const char *, int);

	/* stacked file handles */
	struct	__sFILE *_prevfh;
	char	*_filename;

	/* file extension */
	struct	__sbuf _ext;

	/* separate buffer for long sequences of ungetc() */
	unsigned char *_up;	/* saved _p when _p is doing ungetc data */
	int	_ur;		/* saved _r when _r is counting ungetc data */

	/* tricks to meet minimum requirements even when malloc() fails */
	unsigned char _ubuf[3];	/* guarantee an ungetc() buffer */
	unsigned char _nbuf[1];	/* guarantee a getc() buffer */

	/* separate buffer for fgetln() when line crosses buffer boundary */
	struct	__sbuf _lb;	/* buffer for fgetln() */

	/* Unix stdio files get aligned to block boundaries on fseek() */
	int	_blksize;	/* stat.st_blksize (may be != _bf._size) */
	fpos_t	_offset;	/* current lseek offset */
} FILE;

extern FILE __sF[];

#define	__SLBF	0x0001		/* line buffered */
#define	__SNBF	0x0002		/* unbuffered */
#define	__SRD	0x0004		/* OK to read */
#define	__SWR	0x0008		/* OK to write */
	/* RD and WR are never simultaneously asserted */
#define	__SRW	0x0010		/* open for reading & writing */
#define	__SEOF	0x0020		/* found EOF */
#define	__SERR	0x0040		/* found error */
#define	__SMBF	0x0080		/* _buf is from malloc */
#define	__SAPP	0x0100		/* fdopen()ed in append mode */
#define	__SSTR	0x0200		/* this is an sprintf/snprintf string */
#define	__SOPT	0x0400		/* do fseek() optimization */
#define	__SNPT	0x0800		/* do not do fseek() optimization */
#define	__SOFF	0x1000		/* set iff _offset is in fact correct */
#define	__SMOD	0x2000		/* true => fgetln modified _p text */
#define	__SALC	0x4000		/* allocate string space dynamically */
#define	__SCLN	0x8000		/* fgetln must return copy (clean) */

/*
 * The following three definitions are for ANSI C, which took them
 * from System V, which brilliantly took internal interface macros and
 * made them official arguments to setvbuf(), without renaming them.
 * Hence, these ugly _IOxxx names are *supposed* to appear in user code.
 *
 * Although numbered as their counterparts above, the implementation
 * does not rely on this.
 */
#define	_IOFBF	0		/* setvbuf should set fully buffered */
#define	_IOLBF	1		/* setvbuf should set line buffered */
#define	_IONBF	2		/* setvbuf should set unbuffered */

#define	BUFSIZ	1024		/* size of buffer used by setbuf */
#define	EOF	(-1)

/*
 * FOPEN_MAX is a minimum maximum, and is the number of streams that
 * stdio can provide without attempting to allocate further resources
 * (which could fail).  Do not use this for anything.
 */
				/* must be == _POSIX_STREAM_MAX <limits.h> */
#define	FOPEN_MAX	20	/* must be <= OPEN_MAX <sys/syslimits.h> */
#ifdef WIN32
#undef FILENAME_MAX
#endif
#define	FILENAME_MAX	1024	/* must be <= PATH_MAX <sys/syslimits.h> */

/* Always ensure that these are consistent with <fcntl.h> and <unistd.h>! */
#ifndef SEEK_SET
#define	SEEK_SET	0	/* set file offset to offset */
#endif
#ifndef SEEK_CUR
#define	SEEK_CUR	1	/* set file offset to current plus offset */
#endif
#ifndef SEEK_END
#define	SEEK_END	2	/* set file offset to EOF plus offset */
#endif

#define	stdin	(&__sF[0])
#define	stdout	(&__sF[1])
#define	stderr	(&__sF[2])

/*
 * Functions defined in ANSI C standard.
 */
void	 clearerr(FILE *);
int	 fclose(FILE *);
int	 feof(FILE *);
int	 ferror(FILE *);
int	 fflush(FILE *);
int	 fgetc(FILE *);
int	 fgetpos(FILE * __restrict, fpos_t * __restrict);
char	*fgets(char * __restrict, int, FILE * __restrict);
FILE	*fopen(const char * __restrict , const char * __restrict);
int	 Fprintf(char *path, char *fmt, ...)
#ifdef __GNUC__
	    __attribute__((__format__(__printf__, 2, 3)))
#endif
;
int	 fprintf(FILE * __restrict , const char * __restrict, ...)
#ifdef __GNUC__
	    __attribute__((__format__(__printf__, 2, 3)))
#endif
;
int	 fputc(int, FILE *);
int	 fputs(const char * __restrict, FILE * __restrict);
size_t	 fread(void * __restrict, size_t, size_t, FILE * __restrict);
FILE	*freopen(const char * __restrict, const char * __restrict,
	    FILE * __restrict);
int	 fscanf(FILE * __restrict, const char * __restrict, ...)
#ifdef __GNUC__
	__attribute__((format (__scanf__, 2, 3)))
#endif
;
int	 fseek(FILE *, long, int);
int	 fsetpos(FILE *, const fpos_t *);
long	 ftell(FILE *);
size_t	 fwrite(const void * __restrict, size_t, size_t, FILE * __restrict);
int	 getc(FILE *);
int	 getchar(void);
void	 perror(const char *);
int	 printf(const char * __restrict, ...)
#ifdef __GNUC__
	    __attribute__((__format__(__printf__, 1, 2)))
#endif
;
int	 putc(int, FILE *);
int	 puts(const char *);
int	 remove(const char *);
void	 rewind(FILE *);
int	 scanf(const char * __restrict, ...)
#ifdef __GNUC__
	__attribute__((format (__scanf__, 1, 2)))
#endif
;
void	 setbuf(FILE * __restrict, char * __restrict);
int	 setvbuf(FILE * __restrict, char * __restrict, int, size_t);
int	 sscanf(const char * __restrict, const char * __restrict, ...)
#ifdef __GNUC__
	__attribute__((format (__scanf__, 2, 3)))
#endif
;
FILE	*tmpfile(void);
int	 ungetc(int, FILE *);
int	 vfprintf(FILE * __restrict, const char * __restrict, va_list);
int	 vprintf(const char * __restrict, va_list);

#ifndef __AUDIT__
char	*gets(char *);
int	 sprintf(char * __restrict, const char * __restrict, ...)
#ifdef __GNUC__
	__attribute__((format (__printf__, 2, 3)))
#endif
;
char	*tmpnam(char *);
int	 vsprintf(char * __restrict, const char * __restrict, va_list);
#endif

int	rename(const char *src, const char *dst);

/*
 * Bk extension
 */
int	fpush(FILE **fp, FILE *new);
int	fpop(FILE **fp);
char	*fname(FILE *fp, char *name);

/*
 * IEEE Std 1003.1-90
 */
FILE	*fdopen(int, const char *);
int	 fileno(FILE *);

/*
 * IEEE Std 1003.1c-95, also adopted by X/Open CAE Spec Issue 5 Version 2
 */
void	flockfile(FILE *);
int	ftrylockfile(FILE *);
void	funlockfile(FILE *);
int	getc_unlocked(FILE *);
int	getchar_unlocked(void);
int	putc_unlocked(int, FILE *);

/*
 * Functions defined in POSIX 1003.2 and XPG2 or later.
 */
int	 pclose(FILE *);
FILE	*popen(const char *, const char *);

/*
 * Functions defined in ISO XPG4.2, ISO C99, POSIX 1003.1-2001 or later.
 */
int	 snprintf(char * __restrict, size_t, const char * __restrict, ...)
#ifdef __GNUC__
	    __attribute__((__format__(__printf__, 3, 4)))
#endif
;
int	 vsnprintf(char * __restrict, size_t, const char * __restrict,
	    va_list)
#ifdef __GNUC__
	    __attribute__((__format__(__printf__, 3, 0)))
#endif
;

/*
 * X/Open CAE Specification Issue 5 Version 2
 */
int	 fseeko(FILE *, off_t, int);
off_t	 ftello(FILE *);

/*
 * Routines that are purely local.
 */
#define	FPARSELN_UNESCESC	0x01
#define	FPARSELN_UNESCCONT	0x02
#define	FPARSELN_UNESCCOMM	0x04
#define	FPARSELN_UNESCREST	0x08
#define	FPARSELN_UNESCALL	0x0f

int	 asprintf(char ** __restrict, const char * __restrict, ...)
#ifdef __GNUC__
	    __attribute__((__format__(__printf__, 2, 3)))
#endif
;
char	*fgetln(FILE * __restrict, size_t * __restrict);
char	*fgetline(FILE * __restrict);
char	*fparseln(FILE *, size_t *, size_t *, const char[3], int);
int	 fpurge(FILE *);
void	 setbuffer(FILE *, char *, int);
int	 setlinebuf(FILE *);
int	 vasprintf(char ** __restrict, const char * __restrict ab,
	    va_list)
#ifdef __GNUC__
	    __attribute__((__format__(__printf__, 2, 0)))
#endif
;
int	 vscanf(const char * __restrict, va_list)
#ifdef __GNUC__
	    __attribute__((__format__(__scanf__, 1, 0)))
#endif
;
int	 vfscanf(FILE * __restrict, const char * __restrict,
	    va_list)
#ifdef __GNUC__
	    __attribute__((__format__(__scanf__, 2, 0)))
#endif
;
int	 vsscanf(const char * __restrict, const char * __restrict,
	    va_list)
#ifdef __GNUC__
	    __attribute__((__format__(__scanf__, 2, 0)))
#endif
;
const char *fmtcheck(const char *, const char *)
#ifdef __GNUC__
	    __attribute__((__format_arg__(2)))
#endif
;

/* macro for bk source */

#ifdef __GNUC__
static inline char *
aprintf(const char *fmt, ...)
	__attribute__((__format__(__printf__, 1, 2)));
#endif


static inline char *
aprintf(const char *fmt, ...)
{
	va_list ap;
	char *retval;

	va_start(ap, fmt);
	if (vasprintf(&retval, fmt, ap) < 0) retval = 0;
	va_end(ap);
	return (retval);
}

/*
 * Stdio function-access interface.
 */
FILE	*funopen(const void *,
		int (*)(void *, char *, int),
		int (*)(void *, const char *, int),
		fpos_t (*)(void *, fpos_t, int),
		int (*)(void *));
#define	fropen(cookie, fn) funopen(cookie, fn, 0, 0, 0)
#define	fwopen(cookie, fn) funopen(cookie, 0, fn, 0, 0)

/*
 * Functions internal to the implementation.
 */
int	__srget(FILE *);
int	__swbuf(int, FILE *);

/*
 * The __sfoo macros are here so that we can 
 * define function versions in the C library.
 */
#define	__sgetc(p) (--(p)->_r < 0 ? __srget(p) : (int)(*(p)->_p++))
#if defined(__GNUC__) && defined(__STDC__)
static inline int __sputc(int _c, FILE *_p) {
	if (--_p->_w >= 0 || (_p->_w >= _p->_lbfsize && (char)_c != '\n'))
		return (*_p->_p++ = _c);
	else
		return (__swbuf(_c, _p));
}
#else
/*
 * This has been tuned to generate reasonable code on the vax using pcc.
 */
#define	__sputc(c, p) \
	(--(p)->_w < 0 ? \
		(p)->_w >= (p)->_lbfsize ? \
			(*(p)->_p = (c)), *(p)->_p != '\n' ? \
				(int)*(p)->_p++ : \
				__swbuf('\n', p) : \
			__swbuf((int)(c), p) : \
		(*(p)->_p = (c), (int)*(p)->_p++))
#endif

#define	__sfeof(p)	(((p)->_flags & __SEOF) != 0)
#define	__sferror(p)	(((p)->_flags & __SERR) != 0)
#define	__sclearerr(p)	((void)((p)->_flags &= ~(__SERR|__SEOF)))
#define	__sfileno(p)	((p)->_file)

#ifndef __lint__
#if !defined(_REENTRANT) && !defined(_PTHREADS)
#define	feof(p)		__sfeof(p)
#define	ferror(p)	__sferror(p)
#define	clearerr(p)	__sclearerr(p)

#define	getc(fp)	__sgetc(fp)
#define putc(x, fp)	__sputc(x, fp)
#endif /* !_REENTRANT && !_PTHREADS */
#endif /* __lint__ */

#define	getchar()	getc(stdin)
#define	putchar(x)	putc(x, stdout)

#define	fileno(p)	__sfileno(p)

#define getc_unlocked(fp)	__sgetc(fp)
#define putc_unlocked(x, fp)	__sputc(x, fp)

#define getchar_unlocked()	getc_unlocked(stdin)
#define putchar_unlocked(x)	putc_unlocked(x, stdout)

#endif /* _STDIO_H_ */
