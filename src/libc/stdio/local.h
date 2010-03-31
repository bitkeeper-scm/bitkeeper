/*	$NetBSD: local.h,v 1.19 2005/02/09 21:35:47 kleink Exp $	*/

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
 *	@(#)local.h	8.3 (Berkeley) 7/3/94
 */

#include "system.h"
#include <limits.h>
#ifdef	WIN32
#include <stdint.h>
#endif

#include "fileext.h"

/* "fix" remapping of fclose from bk's system.h */
#undef	fclose
#define	fclose	bk_fclose

/* same for fopen on win32 */
#undef	fopen
#define	fopen	bk_fopen

#define	STDIO_BLKSIZE	(8<<10)

#ifndef	DEFFILEMODE
#define	DEFFILEMODE 0666
#endif

#ifndef	EFTYPE
#define	EFTYPE ENOSYS
#endif

/* attempt to find the alignment of the FILE struct */
#ifndef	ALIGNBYTES
#define	ALIGNBYTES	(((sizeof(off_t) > sizeof(void *)) ? \
			 sizeof(off_t) : sizeof(void *)) - 1)
#endif
#ifndef	ALIGN
#define	ALIGN(p)	(((unsigned long)(p) + ALIGNBYTES) &~ ALIGNBYTES)
#endif

#ifndef	INTMAX_MAX
#define	intmax_t	long int
#define	uintmax_t	unsigned long int
#define	INTMAX_MAX	9223372036854775807L
#define	intptr_t	long
#endif
#ifndef	UINTPTR_MAX
#define	uintptr_t	unsigned long int
#endif

#ifndef NULL
#define	NULL	0
#endif

#define	__UNCONST(s)	((unsigned char *)s)

/*
 * Information local to this implementation of stdio,
 * in particular, macros and private variables.
 */

extern int	__sflush(FILE *);
extern FILE	*__sfp(void);
extern int	__srefill(FILE *);
extern int	__sread(void *, char *, int);
extern int	__swrite(void *, char const *, int);
extern fpos_t	__sseek(void *, fpos_t, int);
extern int	__sclose(void *);
extern void	__sinit(void);
#ifdef	NOTBK
extern void	_cleanup(void);
extern void	(*__cleanup)(void);
#else
extern void	__atexit_cleanup(void);
#endif
extern void	__smakebuf(FILE *);
extern int	__swhatbuf(FILE *, size_t *, int *);
extern int	_fwalk(int (*)(FILE *));
extern char	*_mktemp(char *);
extern int	__swsetup(FILE *);
extern int	__sflags(const char *, int *);
extern int	__svfscanf(FILE * __restrict, const char * __restrict,
		    va_list)
		    __attribute__((__format__(__scanf__, 2, 0)));
extern int	__svfscanf_unlocked(FILE * __restrict, const char * __restrict,
		    va_list)
		    __attribute__((__format__(__scanf__, 2, 0)));
extern int	__vfprintf_unlocked(FILE * __restrict, const char * __restrict,
		    va_list);


extern int	__sdidinit;

extern int	__gettemp(char *, int *, int);

extern char	*__fgetstr(FILE * __restrict, size_t * __restrict, int);
extern int	 __slbexpand(FILE *, size_t);

/*
 * Return true iff the given FILE cannot be written now.
 */
#define	cantwrite(fp) \
	((((fp)->_flags & __SWR) == 0 || (fp)->_bf._base == NULL) && \
	 __swsetup(fp))

/*
 * Test whether the given stdio file has an active ungetc buffer;
 * release such a buffer, without restoring ordinary unread data.
 */
#define	HASUB(fp) (_UB(fp)._base != NULL)
#define	FREEUB(fp) { \
	if (_UB(fp)._base != (fp)->_ubuf) \
		free((char *)_UB(fp)._base); \
	_UB(fp)._base = NULL; \
}

/*
 * test for an fgetln() buffer.
 */
#define	HASLB(fp) ((fp)->_lb._base != NULL)
#define	FREELB(fp) { \
	free((char *)(fp)->_lb._base); \
	(fp)->_lb._base = NULL; \
}

extern void __flockfile_internal(FILE *, int);
extern void __funlockfile_internal(FILE *, int);
