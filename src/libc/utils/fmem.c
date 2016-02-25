/*
 * Copyright 2008-2009,2011-2016 BitMover, Inc
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
 * Theory of operation
 *
 * fmem_open() creates a memory-only file on top of our stdio
 * library.  By replacing the backend read/write/seek calls made by
 * stdio the data is store in memory and not on disk.
 *
 * The data is stored in one flat buffer that is grown as needed as
 * data is written.  When returned to the user that buffer has a null
 * character added to the end that is not included in the length.
 * This allows the data to be treated as a C string.
 *
 * Calls to setvbuf() are sprinkled in the code so that stdio doesn't
 * allocate its own buffer, but reads and written directly from the
 * data.  That means fmemRead & fmemWrite do not copy any data.
 */
typedef struct {
	FILE	*f;		/* backpointer */
	DATA	d;
	size_t	offset;		/* current seek offset */
} FMEM;

#define	MINSZ	128

private	int	fmemRead(void *cookie, char *buf, int len);
private	int	fmemWrite(void *cookie, const char *buf, int len);
private	fpos_t	fmemSeek(void *cookie, fpos_t offset, int whence);
private	int	fmemClose(void *cookie);
private	void	fmemSetvbuf(FMEM *fm);

/*
 * open an in-memory file handle that can be read or written and
 * allows seeks and grows as needed automatically.
 *
 * FILE	*f = fmem();
 * // return the data built up so far w/ trailing null (not in len)
 * // this is not a malloc, if you want your own copy dup it.
 * char	*fmem_peek(FILE *f, off_t *len);
 * // close the FMEM but do not free the data, return it.
 * char *fmem_close(FILE *f, off_t *len);
 * // truncate down (or extend buffer/file) to size bytes
 * // or size+1 (for null) in the fmem case
 * int	ftrunc(file *f, off_t size);
 *
 * FILE *f = fmem();
 *
 * fprintf(f, junk);
 * fprintf(f, junk);
 * p = fmem_close(f);	// do this if you want the buffer, fclose if you don't
 */
FILE *
fmem(void)
{
	FMEM	*fm = new(FMEM);
	FILE	*f;

	f = funopen(fm, fmemRead, fmemWrite, fmemSeek, fmemClose);
	fm->f = f;
	data_resize(&fm->d, 1); /* alloc min buffer */
	fmemSetvbuf(fm);
	f->_flags |= __SCLN;	/* make fgetline() return copy */
	return (f);
}

/*
 * Create a FILE* to provide read-only access to a region of memory
 * The memory is addessed directly by the FILE* so it needs to stay
 * around until fclose() is called.
 */
FILE *
fmem_buf(void *buf, int len)
{
	FMEM	*fm = new(FMEM);
	FILE	*f;

	f = funopen(fm, fmemRead, 0, fmemSeek, fmemClose);
	fm->f = f;
	fm->d.buf = buf;
	unless (len) len = strlen(buf);
	fm->d.len = len;
	fm->d.size = len+1;
	fmemSetvbuf(fm);
	f->_flags |= __SCLN;	/* make fgetline() return copy */
	return (f);
}


/*
 * There is no truncate() function for a FILE *
 * That means we have to code this mess for each user who could
 * be writing to a file or a block of memory.  This packages that.
 */
int
ftrunc(FILE *f, off_t offset)
{
	FMEM	*fm;
	int	rc;

	assert(f);
	if (fflush(f)) return (-1);
	if (f->_close == fmemClose) {
		/* this is a FMEM*, trunc but don't free memory */
		fm = f->_cookie;
		assert(fm);
		if (offset > fm->d.len) { /* zero extend new data */
			data_resize(&fm->d, offset+1); /* room for null */
			memset(fm->d.buf + fm->d.len, 0, offset - fm->d.len);
		}
		fm->d.len = offset;
		if (fm->offset > fm->d.len) fm->offset = fm->d.len;
		fmemSetvbuf(fm);
		rc = 0;
	} else {
		/* Assume it is a normal FILE* */
		rc = ftruncate(fileno(f), offset);
	}
	return (rc);
}

/*
 * Allow an application to directly access the memory backend for an
 * in-memory file.  A null is added after the end of the buffer so
 * data can be considered a C-string.
 */
char *
fmem_peek(FILE *f, size_t *len)
{
	FMEM	*fm;

	fm = f->_cookie;
	assert(fm);
	/* discard/flush any currently buffered data */
	unless (fflush(f)) {   /* fails if readonly */
		/* make sure we have room for the null */
		data_resize(&fm->d, fm->d.len+1);
		fm->d.buf[fm->d.len] = 0;/* force trailing null (not in len) */
		fmemSetvbuf(fm);
	}
	if (len) *len = fm->d.len; /* optionally return len */
	return (fm->d.buf);
}

/*
 * Return malloced copy of data in buffer.
 */
char *
fmem_dup(FILE *f, size_t *len)
{
	FMEM	*fm;
	char	*ret;

	fm = f->_cookie;
	assert(fm);
	/* discard/flush any currently buffered data */
	fflush(f);
	if (len) *len = fm->d.len;	   /* optionally return size */
	ret = malloc(fm->d.len+1);
	memcpy(ret, fm->d.buf, fm->d.len);
	ret[fm->d.len] = 0;	/* force trailing null (not in len) */
	return (ret);
}

/*
 * Close the fmem file and return the data it contained.
 */
char *
fmem_close(FILE *f, size_t *len)
{
	FMEM	*fm;
	char	*ret;

	unless (f->_write) {
		errno = EBADF;
		fclose(f);
		return (0);
	}
	fm = f->_cookie;
	assert(fm);
	/* discard/flush any currently buffered data */
	fflush(f);
	if (len) *len = fm->d.len;	   /* optionally return size */
	ret = realloc(fm->d.buf, fm->d.len+1); /* shrink buffer */
	ret[fm->d.len] = 0;	/* force trailing null (not in len) */
	/* prevent _close() from freeing buffer */
	fm->d.buf = 0;
	fm->d.len = fm->d.size = fm->offset = 0;
	fclose(f);
	return (ret);
}

private void
fmemSetvbuf(FMEM *fm)
{
	char	*buf = fm->d.buf + fm->offset;
	size_t	len = fm->d.size - fm->offset;

	setvbuf(fm->f, buf, _IOFBF, len);
}

/*
 * Called by stdio to read one chunk of data.  Because stdio will
 * already be setup to read from fm->d.buf directly it is usually only
 * called twice.  Once to read all data and again to return 0
 * indicating that we are at EOF.
 */
private int
fmemRead(void *cookie, char *buf, int len)
{
	FMEM	*fm = cookie;
	char	*ptr;
	int	newlen;

	assert(fm);
	newlen = len;
	if (newlen + fm->offset > fm->d.len) newlen = fm->d.len - fm->offset;
	len = newlen;
	unless (len) return (0);

	assert(len >= 0);
	ptr = fm->d.buf + fm->offset;

	/*
	 * Normally this read() call doesn't do anything because the
	 * backing store _is_ the stdio buffer.  But if fread()
	 * bypasses directly to user's buffer, then this read will
	 * need to copy the data to the user.
	 */
	if (buf != ptr) memcpy(buf, ptr, len);
	fm->offset += len;
	return (len);
}

/*
 * Called by stdio to write a chunk of data.  Usually it is writing
 * fm->d.buf so we don't need to copy any data, but a large write can
 * bypass stdio's buffering so we may need to copy data from the
 * user's buffer.
 *
 * Before returning we call setvbuf() so stdio will start collecting
 * the next chunk of data in the right place.  We make sure that
 * remaining buffer is not empty so new data can be written.
 */
private int
fmemWrite(void *cookie, const char *buf, int len)
{
	FMEM	*fm = cookie;
	size_t	newoff;

	assert(fm);
	unless (fm->f->_write) {
		errno = EBADF;
		return (-1);
	}
	newoff = fm->offset + len;
	if (buf == fm->d.buf + fm->offset) {
		assert(newoff <= fm->d.size);
	} else {
		/* stdio can bypass the buffer for large writes */
		if (newoff > fm->d.size) data_resize(&fm->d, newoff + MINSZ);
		memcpy(fm->d.buf + fm->offset, buf, len);
	}
	fm->offset = newoff;
	if (fm->offset > fm->d.len) fm->d.len = fm->offset;
	/* need some space for next write */
	if (fm->d.size - fm->offset < MINSZ) {
		data_resize(&fm->d, fm->d.len + MINSZ);
	}
	fmemSetvbuf(fm);
	return (len);
}

/*
 * Called by stdio.
 */
private fpos_t
fmemSeek(void *cookie, fpos_t offset, int whence)
{
	FMEM	*fm = cookie;

	assert(fm);
	switch (whence) {
	    case SEEK_SET: break;
	    case SEEK_CUR: offset += fm->offset; break;
	    case SEEK_END: offset += fm->d.len; break;
	    default: assert(0);
	}
	assert(offset >= 0);
	if (fm->offset != offset) {
		/*
		 * don't call setvbuf() if they are just calling
		 * ftell().  That has the side effect of flushing data.
		 */
		fm->offset = offset;
		if (offset >= fm->d.len) {
			unless (fm->f->_write) {
				errno = EBADF;
				return (-1);
			}
			data_resize(&fm->d, offset + MINSZ);
			fm->d.len = offset;
			fmemSetvbuf(fm);
		}
	}
	return (offset);
}

private int
fmemClose(void *cookie)
{
	FMEM	*fm = cookie;

	assert(fm);
	if (fm->d.buf && fm->f->_write) free(fm->d.buf);
	free(fm);
	return (0);
}

/*
 *
 */
void
fmem_tests(void)
{
	FILE	*f;
	int	i, c, rc;
	size_t	len;
	char	*p;
	char	buf[4096];

	/* write dynamic memory char at a time */
	f = fmem();
	assert(f);
	for (i = 0; i < 3000; i++) {
		assert(ftell(f) == i);
		c = '0' + (i % 10);
		rc = fputc(c, f);
		assert(rc == c);
		p = fmem_peek(f, &len);
		assert(p);
		assert(len == i + 1);
		for (c = 0; c <= i; c++) {
			assert(p[c] == ('0' + (c % 10)));
		}
		assert(p[c] == 0);
	}
	rc = fclose(f);
	assert(rc == 0);

	/* write two chars at a time */
	f = fmem();
	assert(f);
	for (i = 0; i < 2000; i++) {
		assert(ftell(f) == 2*i);
		buf[0] = 'f';
		buf[1] = '0' + (i % 10);
		rc = fwrite(buf, 1, 2, f);
		assert(rc == 2);
		p = fmem_peek(f, &len);
		assert(p);
		assert(len == 2*i + 2);
		for (c = 0; c <= i; c++) {
			assert(p[2*c] == 'f');
			assert(p[2*c+1] == ('0' + (c % 10)));
		}
		assert(p[2*c] == 0);
	}
	p = fmem_dup(f, &len);
	ftrunc(f, 0);
	assert(len == 2*i);
	assert(ftell(f) == 0);
	fprintf(f, "this shouldn't touch the previous buffer");
	for (c = 0; c < i; c++) {
		assert(p[2*c] == 'f');
		assert(p[2*c+1] == ('0' + (c % 10)));
	}
	assert(p[2*c] == 0);
	free(p);

	p = fmem_dup(f, &len);
	ftrunc(f, 0);
	assert(streq(p, "this shouldn't touch the previous buffer"));
	free(p);

	p = fmem_dup(f, &len);
	ftrunc(f, 0);
	assert(streq(p, ""));
	assert(len == 0);
	free(p);

	p = fmem_peek(f, &len);
	assert(streq(p, ""));
	assert(len == 0);
	fputc('h', f);

	p = fmem_peek(f, &len);
	assert(len == 1);
	assert(streq(p, "h"));
	rc = fclose(f);
	assert(rc == 0);

	/* write; rewind; read; trunc, repeat */
	f = fmem();
	fputs("x\n", f);
	rewind(f);
	p = fgetline(f);
	assert(*p == 'x');
	p = fgetline(f);
	assert(p == 0);
	ftrunc(f, 0);
	fputs("m\n", f);
	rewind(f);
	p = fgetline(f);
	assert(*p == 'm');
	p = fgetline(f);
	assert(p == 0);
	rc = fclose(f);
	assert(rc == 0);

	strcpy(buf, "this is a test");
	f = fmem_buf(buf, 0);
	while (fgetc(f) != EOF);
	rc = fseek(f, 0, SEEK_SET);
	assert(rc == 0);
	rc = fclose(f);
	assert(rc == 0);
}
