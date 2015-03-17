/*	$NetBSD: fread.c,v 1.15 2003/01/18 11:29:53 thorpej Exp $	*/

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
static char sccsid[] = "@(#)fread.c	8.2 (Berkeley) 12/11/93";
#else
__RCSID("$NetBSD: fread.c,v 1.15 2003/01/18 11:29:53 thorpej Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "reentrant.h"
#include "local.h"

size_t
fread(buf, size, count, fp)
	void *buf;
	size_t size, count;
	FILE *fp;
{
	size_t resid;
	unsigned char *p;
	int r, eof;
	size_t total;

	assert(fp != NULL);
	/*
	 * The ANSI standard requires a return value of 0 for a count
	 * or a size of 0.  Whilst ANSI imposes no such requirements on
	 * fwrite, the SUSv2 does.
	 */
	if ((resid = count * size) == 0)
		return (0);

	assert(buf != NULL);

	FLOCKFILE(fp);
	if (fp->_r < 0)
		fp->_r = 0;
	total = resid;
	p = buf;
	while (resid > (r = fp->_r)) {
		if (p != fp->_p) {
			memcpy((void *)p, (void *)fp->_p, (size_t)r);
			fp->_p += r;
		}
		/* fp->_r = 0 ... done in __srefill */
		p += r;
		resid -= r;
		/*
		 * If the user is requesting enough data that we will
		 * consume the entire buffer, then just write to the
		 * destination buffer directly.
		 *
		 * We can't do this in RW mode because __srefill() may
		 * need to flush the old buffer when switching from
		 * writing to reading.
		 */
		if ((fp->_flags & __SRD) && !HASUB(fp) &&
		    (fp->_bf._size > 0) && (resid >= fp->_bf._size)) {
			char *obuf = fp->_bf._base;
			size_t osize = fp->_bf._size;
			fp->_bf._base = p;
			/* keep reads aligned by bufsize */
			fp->_bf._size = osize * (min((1<<30), resid) / osize);
			eof = __srefill(fp);
			fp->_bf._base = obuf;
			fp->_bf._size = osize;
		} else {
			eof = __srefill(fp);
		}
		if (eof) {
			/* no more input: return partial result */
			fp->_p = fp->_bf._base;
			FUNLOCKFILE(fp);
			return ((total - resid) / size);
		}
	}
	if (p != fp->_p) {
		memcpy((void *)p, (void *)fp->_p, resid);
		fp->_p += resid;
	} else {
		fp->_p = fp->_bf._base;
	}
	fp->_r -= resid;
	FUNLOCKFILE(fp);
	return (count);
}
