/* zgets.c
 * Copyright (C) 1995-1998 Jean-loup Gailly.
 * Copyright (C) 1999 Larry McVoy - same conditions as original author.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* %W% */

#include "system.h"

#define ZBUFSIZ (4<<10)		/* see note in compress.c */

struct zgetbuf	{
	z_stream	z;		/* zlib data */
	zgets_func	callback;	/* to get more input */
	void		*token;
	char		*buf;		/* out buffer (size ZBUFSIZ) */
	int		eof;		/* seen eof? */
	int		err;		/* seen err? */
	char		*next;		/* next output to be read */
	int		left;		/* size of unread data after next */
	char		*line;		/* used by zgets to hold line */
};

private	int	zgets_fileread(void *token, u8 **buf);

/*
 * Setup to read a stream of compressed data.  The data to be
 * decompressed is returned from the callback function a block at a
 * time.
 *
 * Minimal callback:
 *    int
 *    zgets_callback(void *token u8 **buf)
 *    {
 *         if (buf) {
 *             *buf = START;
 *             return (LENGTH);
 *         }
 *         return (0);
 *    }
 *
 * If callback is null, then assume token is a FILE *.
 */
zgetbuf	*
zgets_initCustom(zgets_func callback, void *token)
{
	zgetbuf	*in;

	in = new(zgetbuf);
	in->buf = malloc(ZBUFSIZ);
	in->callback = callback ? callback : zgets_fileread;
	in->token = token;
	if (inflateInit(&in->z) != Z_OK) {
		free(in->buf);
		free(in);
		return (0);
	}
	return (in);
}

/*
 * callback used by zgets to read data from a file handle.
 */
private int
zgets_fileread(void *token, u8 **buf)
{
	FILE	*f = (FILE *)token;
	static	char *data = 0;

	if (buf) {
		unless (data) data = malloc(ZBUFSIZ);
		*buf = data;
		return (fread(data, 1, ZBUFSIZ, f));
	} else {
		/* called from zgets_done */
		if (data) {
			free(data);
			data = 0;
		}
		return (0);
	}
}

/*
 * Define a simple flat array callback for the simple case where we are
 * just decompressing from a buffer in memory.  Then zgets_init() gets
 * called like this:
 *
 *   handle = zgets_init(zgets_flatCallback, zgets_flatData(buf, len));
 */
private int
zgets_flatCallback(void *token, u8 **buf) {
	void	**data = token;

	if (buf) {
		*buf = data[0];
		return (p2int(data[1]));
	} else {
		/* called from zgets_done() */
		free(data);
		return (0);
	}
}

private void *
zgets_flatData(void *start, int len)
{
	void	**data = malloc(2 * sizeof(void *));

	data[0] = start;
	data[1] = int2p(len);

	return (data);
}

/*
 * a simpler version of zgets_init() that handles the common case of
 * decompression from a buffer in memory.
 */
zgetbuf	*
zgets_init(void *start, int len)
{
	return (zgets_initCustom(zgets_flatCallback,
				 zgets_flatData(start, len)));
}


/*
 * decompress data into a buffer and return the number of bytes
 * written.
 */
static int
zfillbuf(zgetbuf *in, u8 *buf, int len)
{
	int	err;

	in->z.next_out = buf;
	in->z.avail_out = len;
	while (in->z.avail_out) {
		unless (in->z.avail_in) {
			in->z.avail_in =
				in->callback(in->token, &in->z.next_in);
		}
		unless (in->z.avail_in) break;
		err = inflate(&in->z, Z_NO_FLUSH);
		if (err == Z_STREAM_END) {
			in->eof = 1;
			break;
		} else if (err != Z_OK) {
			unless (in->err) in->err = err;
			break;
		}
	}
	return (len - in->z.avail_out);
}

/* fill in->buf with data and update in->next and in->left */
static int
zfill(zgetbuf *in)
{
	if (in->eof || in->err) return (0);
	if (in->left) memmove(in->buf, in->next, in->left);
	in->left += zfillbuf(in, in->buf + in->left, ZBUFSIZ - in->left);
	in->next = in->buf;
	return (in->left);
}

int
zeof(zgetbuf *in)
{
	return (!in->left && !zfill(in));
}

/*
 * Return a pointer to the buffer to see at least 'len' characters
 * ahead. Returns NULL if that many characters can't be found.
 */
char *
zpeek(zgetbuf *in, int len)
{
	assert(len < ZBUFSIZ);
	if (in->left < len) zfill(in);
	if (in->left < len) {
		assert(in->eof);
		return (0);
	}
	return (in->next);
}

/*
 * Seek ahead 'len' characters
 */
int
zseek(zgetbuf *in, int len)
{
	while (len > in->left) {
		len -= in->left;
		in->left = 0;
		unless (zfill(in)) return (-1); /* hit EOF */
	}
	in->next += len;
	in->left -= len;
	return (0);
}

/*
 * Return one line of text from the compressed stream.  The line is
 * null terminated with any newlines stripped.
 */
char	*
zgets(zgetbuf *in)
{
	char	*ret;
	char	**data = 0;
	int	didfill = 0;
	int	i;
	u32	nl;

	if (in->line) {
		free(in->line);
		in->line = 0;
	}

	/*
	 * XXX Caution -- Wayne optimized code --
	 * in the didfill block, zfill() resets in->next
	 * and loops back knowing that 'i' offset will still work
	 * because any remaining data was copied to the beginning
	 * of the buffer.
	 */
	i = 0;
again:
	if (zeof(in)) return (0);
	while(i < in->left) if (in->next[i++] == '\n') break;
	if ((i > 0) && (in->next[i-1] == '\n')) {
		in->next[i-1] = 0; /* chop newline */
		ret = in->next;
		in->next += i;
		in->left -= i;
		return (ret);
	}
	unless (didfill) {
		zfill(in);
		didfill = 1;
		goto again;
	}
	/*
	 * line is bigger than ZBUFSIZ !
	 * Non-Wayne unoptimized for ease of lesser folks reading ability.
	 */
	data = data_append(data, in->next, in->left, 0);
	in->left = 0;
	nl = 0;
	while (!nl && zfill(in)) {
		i = 0;
		while (i < in->left) {
			if (in->next[i++] == '\n') {
				nl = 1;
				break;
			}
		}
		data = data_append(data, in->next, i-nl, 0); /* chop newline */
		in->next += i;
		in->left -= i;
	}
	return (in->line = str_pullup(0, data));
}

/*
 * Read a block of data from the compressed stream and write it to the
 * buffer.  Returns number of characters read or -1 on error.
 */
int
zread(zgetbuf *in, u8 *buf, int len)
{
	int	cnt;
	u8	*p = buf;

	for (;;) {
		if (in->left) {
			cnt = min(in->left, len);
			memcpy(p, in->next, cnt);
			p += cnt;
			len -= cnt;
			in->next += cnt;
			in->left -= cnt;
		}
		/*
		 * Arbitrarily decide that requests larger than 1024
		 * should be decompressed directly into the user's
		 * buffer rather than being put it in the zgetbuf and
		 * copied out.  It is a trade off between an extra
		 * data copy and running the compress code on a short
		 * block.
		 * 1024 is the stdio buf size
		 */
		if (len >= 1024) {
			assert(in->left == 0);
			cnt = zfillbuf(in, p, len);
			p += cnt;
			len -= cnt;
		} else if (len > 0) {
			cnt = zfill(in);
		} else {
			break;	/* done */
		}
		unless (cnt) break; /* at EOF */
	}
	if (in->err) return (-1);
	return (p - buf);
}

int
zgets_done(zgetbuf *in)
{
	int	err;

	err = inflateEnd(&in->z);
	if (err != Z_OK && !in->err) in->err = err;
	err = in->err;
	if (err) fprintf(stderr, "zgets: decompression failure %d\n", err);
	free(in->buf);
	/* signal end to callback */
	if (in->callback(in->token, 0) < 0) err = -1;
	free(in);
	return (err);
}

private void	zputs_filewrite(void *token, u8 *data, int len);

struct zputbuf	{
	z_stream	z;
	char		*outbuf, *inbuf;
	int		err;
	zputs_func	callback;
	void		*token;
};

zputbuf *
zputs_init(zputs_func callback, void *token, int level)
{
	zputbuf	*out;

	out = new(zputbuf);
	out->outbuf = malloc(ZBUFSIZ);
	out->z.next_out = out->outbuf;
	out->z.avail_out = ZBUFSIZ;
	out->inbuf = malloc(ZBUFSIZ);
	out->z.next_in = out->inbuf;
	out->z.avail_in = 0;
	out->callback = callback ? callback : zputs_filewrite;
	out->token = token;
	if (deflateInit(&out->z, (level == -1) ? 4 : level) != Z_OK) {
		free(out->inbuf);
		free(out->outbuf);
		free(out);
		return (0);
	}
	return (out);
}

private void
zflush(zputbuf *out)
{
	int	err;

	while (out->z.avail_in) {
		if (!out->z.avail_out) {
			out->callback(out->token, out->outbuf, ZBUFSIZ);
			out->z.avail_out = ZBUFSIZ;
			out->z.next_out = out->outbuf;
		}
		if (err = deflate(&out->z, Z_NO_FLUSH)) {
			fprintf(stderr,
				"zputs: compression failure %d\n", err);
			unless (out->err) out->err = err;
			break;
		}
	}
	out->z.next_in = out->inbuf;
}

int
zputs(zputbuf *out, u8 *data, int len)
{
	int	i;

	i = ZBUFSIZ - out->z.avail_in; /* space left in buf */
	if ((len >= 1024) && (len > i)) {
		if (out->z.avail_in) zflush(out);
		out->z.next_in = data;
		out->z.avail_in = len;
		zflush(out);
	} else {
		if (len < i) i = len;
		memcpy(out->z.next_in + out->z.avail_in, data, i);
		out->z.avail_in += i;
		len -= i;
		if (len) {
			zflush(out);
			memcpy(out->z.next_in, data + i, len);
			out->z.avail_in = len;
		}
	}
	return (out->err);
}

int
zputs_done(zputbuf *out)
{
	int	err = Z_OK;
	int	len;

	/* Clear output buffer.  */
	for (;;) {
		err = deflate(&out->z, Z_PARTIAL_FLUSH);
		if (err != Z_OK && err != Z_BUF_ERROR) {
			fprintf(stderr,
				"zputs_done: compression failure %d\n", err);
			goto out;
		}
		if (len = ZBUFSIZ - out->z.avail_out) {
			out->callback(out->token, out->outbuf, len);
			out->z.avail_out = ZBUFSIZ;
			out->z.next_out = out->outbuf;
		} else {
			/* until no data left in or out... */
			if (out->z.avail_in == 0) break;
		}

	}
	/* write trailer */
	err = deflate(&out->z, Z_FINISH);
	if (err != Z_STREAM_END) {
		fprintf(stderr,
		    "zputs_done: finsish failure %d\n", err);
		goto out;
	}
	if (len = ZBUFSIZ - out->z.avail_out) {
		assert(len < ZBUFSIZ);	/* should be partial */
		out->callback(out->token, out->outbuf, len);
		out->z.avail_out = ZBUFSIZ;
		out->z.next_out = out->outbuf;
	}

	/* signal end to callback */
	out->callback(out->token, 0, 0);

	err = deflateEnd(&out->z);
	if (err != Z_OK) {
		fprintf(stderr,
		    "zputs_done: compression failure end %d\n", err);
		goto out;
	}

out:
	if (out->err) err = out->err;
	free(out->inbuf);
	free(out->outbuf);
	free(out);
	return (err);
}

private void
zputs_filewrite(void *token, u8 *data, int len)
{
	FILE	*f = (FILE *)token;

	if (len) fwrite(data, len, 1, f);
}
