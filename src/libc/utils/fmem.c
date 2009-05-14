#include "system.h"

/*
 * Theory of operation
 *
 * fmem_open() creates a memory-only file on top of our stdio
 * library.  By replacing the backend read/write/seek calls made by
 * stdio the data is store in memory and not on disk.
 * The functions fmem_(get|peek)buf() allow access to the raw data.
 *
 * The data is stored in one flat buffer that is grown as needed as
 * data is written.  When returned to the user that buffer has a null
 * character added to the end that is not included in the length.
 * This allows the data to be treated as a C string.
 *
 * Calls to setvbuf() are sprinkled in the code so that stdio doesn't
 * allocate its own buffer, but reads and written directly from the
 * data.  That means fmem_read & fmem_write do not copy any data.
 */
typedef struct {
	FILE	*f;		/* backpointer */
	char	*buf;		/* user's data */
	size_t	len;		/* length of user's data */
	size_t	size;		/* malloc'ed size */
	size_t	offset;		/* current seek offset */
} FMEM;

#define	MINSZ	128

private	int	fmem_read(void *cookie, char *buf, int len);
private	int	fmem_write(void *cookie, const char *buf, int len);
private	fpos_t	fmem_seek(void *cookie, fpos_t offset, int whence);
private	int	fmem_close(void *cookie);
private	void	fmem_resize(FMEM *fm, size_t newlen);
private	void	fmem_setvbuf(FMEM *fm);

/*
 * open an in-memory file handle that can be read or written and
 * allows seeks and grows as needed automatically.
 */
FILE *
fmem_open(void)
{
	FMEM	*fm = new(FMEM);
	FILE	*f;

	f = funopen(fm, fmem_read, fmem_write, fmem_seek, fmem_close);
	fm->f = f;
	fmem_resize(fm, MINSZ);	/* alloc min buffer */
	fmem_setvbuf(fm);
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

	TRACE("ftrunc(%p, %d)", f, (int)offset);

	fflush(f);
	if (f->_close == fmem_close) {
		/* this is a FMEM*, trunc but don't free memory */
		fm = f->_cookie;
		assert(fm);
		fmem_resize(fm, offset);
		fm->len = offset;
		if (fm->offset > fm->len) fm->offset = fm->len;
		fmem_setvbuf(fm);
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
fmem_getbuf(FILE *f, size_t *len)
{
	FMEM	*fm;

	fm = f->_cookie;
	assert(fm);
	/* discard/flush any currently buffered data */
	fflush(f);
	if (len) *len = fm->len; /* optionally return len */
	fmem_resize(fm, fm->len+1); /* alloc buf if null with room for null */
	fm->buf[fm->len] = 0;	/* force trailing null (not in len) */
	fmem_setvbuf(fm);
	return (fm->buf);
}

/*
 * Return ownership of the malloced data to the application and
 * truncate the file to zero size again.
 */
char *
fmem_retbuf(FILE *f, size_t *len)
{
	FMEM	*fm;
	char	*ret;

	fm = f->_cookie;
	assert(fm);
	/* discard/flush any currently buffered data */
	fflush(f);
	if (len) *len = fm->len;	   /* optionally return size */
	ret = realloc(fm->buf, fm->len+1); /* shrink buffer */
	ret[fm->len] = 0;	/* force trailing null (not in len) */
	/* truncate */
	fm->buf = 0;
	fm->len = fm->size = fm->offset = 0;
	/*
	 * NOTE: It is OK to leave fm->buf==0 here.  If the user
	 * chooses to write again then stdio will allocate it's own
	 * buffer and fmem_write() will allocate a new buf and copy
	 * and free stdio's buffer
	 */
	return (ret);
}

private void
fmem_setvbuf(FMEM *fm)
{
	char	*buf = fm->buf + fm->offset;
	size_t	len = fm->size - fm->offset;

	setvbuf(fm->f, buf, _IOFBF, len);
}

/* grow buffer as needed */
private void
fmem_resize(FMEM *fm, size_t newlen)
{
	size_t	size = fm->size;

	if (size < newlen) {
		unless (size) size = MINSZ;
		while (size < newlen) size *= 2;

		/* buf is uninitialized */
		if (fm->buf) {
			fm->buf = realloc(fm->buf, size);
		} else {
			fm->buf = malloc(size);
		}
		fm->size = size;
	}
}

/*
 * Called by stdio to read one chunk of data.  Because stdio will
 * already be setup to read from fm->buf directly it is usually only
 * called twice.  Once to read all data and again to return 0
 * indicating that we are at EOF.
 */
private int
fmem_read(void *cookie, char *buf, int len)
{
	FMEM	*fm = cookie;
	int	newlen;

	assert(fm);
	newlen = len;
	if (newlen + fm->offset > fm->len) newlen = fm->len - fm->offset;
	len = newlen;
	assert((buf == fm->buf + fm->offset) || (len == 0));
	assert(len >= 0);
	fm->offset += len;
	return (len);
}

/*
 * Called by stdio to write a chunk of data.  Usually it is writing
 * fm->buf so we don't need to copy any data, but a large write can
 * bypass stdio's buffering so we may need to copy data from the
 * user's buffer.
 *
 * Before returning we call setvbuf() so stdio will start collecting
 * the next chunk of data in the right place.  We make sure that
 * remaining buffer is not empty so new data can be written.
 */
private int
fmem_write(void *cookie, const char *buf, int len)
{
	FMEM	*fm = cookie;
	size_t	newoff;

	assert(fm);
	newoff = fm->offset + len;
	if (buf == fm->buf + fm->offset) {
		assert(newoff <= fm->size);
	} else {
		/* stdio can bypass the buffer for large writes */
		if (newoff > fm->size) fmem_resize(fm, newoff + MINSZ);
		memcpy(fm->buf + fm->offset, buf, len);
	}
	fm->offset = newoff;
	if (fm->offset > fm->len) fm->len = fm->offset;
	/* need some space for next write */
	if (fm->size - fm->offset < MINSZ) fmem_resize(fm, fm->len + MINSZ);
	fmem_setvbuf(fm);
	return (len);
}

/*
 * Called by stdio.
 */
private fpos_t
fmem_seek(void *cookie, fpos_t offset, int whence)
{
	FMEM	*fm = cookie;

	assert(fm);
	switch (whence) {
	    case SEEK_SET: break;
	    case SEEK_CUR: offset += fm->offset; break;
	    case SEEK_END: offset += fm->len; break;
	    default: assert(0);
	}
	assert(offset >= 0);
	if (offset >= fm->len) {
		fmem_resize(fm, offset + MINSZ);
		fm->len = offset;
	}
	if (fm->offset != offset) {
		/*
		 * don't call setvbuf() if they are just calling
		 * ftell().  That has the side effect of flushing data.
		 */
		fm->offset = offset;
		fmem_setvbuf(fm);
	}
	return (offset);
}

private int
fmem_close(void *cookie)
{
	FMEM	*fm = cookie;

	assert(fm);
	if (fm->buf) free(fm->buf);
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
	f = fmem_open();
	assert(f);
	for (i = 0; i < 3000; i++) {
		assert(ftell(f) == i);
		c = '0' + (i % 10);
		rc = fputc(c, f);
		assert(rc == c);
		p = fmem_getbuf(f, &len);
		assert(p);
		assert(len == i + 1);
		for (c = 0; c <= i; c++) {
			assert(p[c] == ('0' + (c % 10)));
		}
		assert(p[c] == 0);
	}
	rc = fclose(f);
	assert(rc == 0);

	/* write two cars at a time */
	f = fmem_open();
	assert(f);
	for (i = 0; i < 2000; i++) {
		assert(ftell(f) == 2*i);
		buf[0] = 'f';
		buf[1] = '0' + (i % 10);
		rc = fwrite(buf, 1, 2, f);
		assert(rc == 2);
		p = fmem_getbuf(f, &len);
		assert(p);
		assert(len == 2*i + 2);
		for (c = 0; c <= i; c++) {
			assert(p[2*c] == 'f');
			assert(p[2*c+1] == ('0' + (c % 10)));
		}
		assert(p[2*c] == 0);
	}
	p = fmem_retbuf(f, &len);
	assert(len == 2*i);
	for (c = 0; c < i; c++) {
		assert(p[2*c] == 'f');
		assert(p[2*c+1] == ('0' + (c % 10)));
	}
	assert(p[2*c] == 0);
	assert(ftell(f) == 0);
	free(p);
	p = fmem_retbuf(f, &len);
	assert(streq(p, ""));
	assert(len == 0);
	free(p);
	p = fmem_getbuf(f, &len);
	assert(streq(p, ""));
	assert(len == 0);
	fputc('h', f);
	p = fmem_getbuf(f, &len);
	assert(len == 1);
	assert(streq(p, "h"));
	rc = fclose(f);
	assert(rc == 0);
}
