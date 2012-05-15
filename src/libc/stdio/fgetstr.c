/*	$NetBSD: fgetstr.c,v 1.3 2005/05/14 23:51:02 christos Exp $	*/

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
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)fgetline.c	8.1 (Berkeley) 6/4/93";
#else
__RCSID("$NetBSD: fgetstr.c,v 1.3 2005/05/14 23:51:02 christos Exp $");
#endif
#endif /* LIBC_SCCS and not lint */


#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "reentrant.h"
#include "local.h"

/*
 * Get an input line.  The returned pointer often (but not always)
 * points into a stdio buffer.  Fgetline does not alter the text of
 * the returned line (which is thus not a C string because it will
 * not necessarily end with '\0'), but does allow callers to modify
 * it if they wish.  Thus, we set __SMOD in case the caller does.
 */
char *
__fgetstr(fp, lenp, sep)
	FILE *fp;
	size_t *lenp;
	int sep;
{
	unsigned char *p;
	size_t len;

	assert(fp != NULL);

	/* make sure there is input */
	if (fp->_r <= 0 && __srefill(fp)) {
		if (lenp) *lenp = 0;
		return (NULL);
	}

	/* look for a newline in the input */
	if (!(fp->_flags & __SCLN) &&
	    (p = memchr((void *)fp->_p, sep, (size_t)fp->_r)) != NULL) {
		char *ret;

		/*
		 * Found one.  Flag buffer as modified to keep fseek from
		 * `optimising' a backward seek, in case the user stomps on
		 * the text.
		 */
		p++;		/* advance over it */
		ret = (char *)fp->_p;
		len = p - fp->_p;
		if (lenp) *lenp = len;
		fp->_flags |= __SMOD;
		fp->_r -= len;
		fp->_p = p;
		return (ret);
	}

	/*
	 * We have to copy the current buffered data to the line buffer.
	 * As a bonus, though, we can leave off the __SMOD.
	 */
	len = 0;
	p = fp->_lb._base;
	do {
		/*
		 * Make sure there is room for more bytes.  Copy data from
		 * file buffer to line buffer, refill file and look for
		 * newline.  The loop stops only when we find a newline.
		 */
		if (fp->_r == 0)
			if (__srefill(fp))
				break;	/* EOF or error: return partial line */
		if (len + 2 > fp->_lb._size) {
			/* include room for null */
			fp->_lb._size =
			    fp->_lb._size ? 2*fp->_lb._size : 128;
			fp->_lb._base = realloc(fp->_lb._base, fp->_lb._size);
			p = fp->_lb._base;
			assert(p);
		}
		p[len] = *fp->_p++;
		--fp->_r;
	} while (p[len++] != sep);
	p[len] = 0;
	if (lenp) *lenp = len;
	return (p);
}
