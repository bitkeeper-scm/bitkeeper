/*
 * compress.c: derived from a file of the same name by:
 * 
 *	Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *	All rights reserved.  Created: Wed Oct 25 22:12:46 1995 ylo
 *
 * Local changes are (c) 2000 Larry McVoy.
 * %W% %K%
 */

#include "bkd.h"

static z_stream in;
static z_stream out;

/*
 * Initializes compression; level is compression level from 1 to 9
 * (as in gzip).
 */

void 
gzip_init(int level)
{
	inflateInit(&in);
	deflateInit(&out, level);
}

/* Frees any data structures allocated for compression. */

void 
gzip_done()
{
	inflateEnd(&in);
	deflateEnd(&out);
}

/*
 * Compresses the contents of input.
 * Data will be flushed at the end of every call so that each
 * output_buffer can be decompressed independently (but in the appropriate
 * order since they together form a single compression stream) by the
 * receiver.
 */
void 
gzip2fd(char *input, int len, int fd)
{
	char	buf[4096];
	int	n, status;

	unless (len) return;

	out.next_in = input;
	out.avail_in = len;

	/* Loop compressing until deflate() returns with avail_out != 0. */
	do {
		/* Set up fixed-size output buffer. */
		out.next_out = buf;
		out.avail_out = sizeof(buf);

		/* Compress as much data into the buffer as possible. */
		if ((status = deflate(&out, Z_PARTIAL_FLUSH)) == Z_OK) {
			n = sizeof(buf) - out.avail_out;
			unless (write(fd, buf, n) == n) {
				perror("write on fd in gzip2fd");
				exit(1);
			}
		} else {
			fprintf(stderr, "gzip deflate says %d\n", status);
			exit(1);
		}
	} while (out.avail_out == 0);
}

/*
 * Uncompresses the input buffer.
 * This must be called for the same size units that the
 * buffer_compress was called, and in the same order that buffers compressed
 * with that.
 */
int
gunzip2fd(char *input, int len, int fd)
{
	char	buf[4096];
	int	n, status;
	int	bytes = 0;

	unless (len) return (0);
	in.next_in = input;
	in.avail_in = len;
	in.next_out = buf;
	in.avail_out = sizeof(buf);
	for (;;) {
		if ((status = inflate(&in, Z_PARTIAL_FLUSH)) == Z_OK) {
			n = sizeof(buf) - in.avail_out;
			if (write(fd, buf, n) != n) {
				perror("write on fd in gunzip2fd");
				exit(1);
			}
			bytes += n;
			in.next_out = buf;
			in.avail_out = sizeof(buf);
		} else {
			return (bytes);
		}
	}
}

#ifdef MAIN
main(int ac, char **av)
{
	unless (av[1]) {
		fprintf(stderr, "usage: %s -z|-u\n", av[0]);
		exit(1);
	}
	gzip_init(6);
	if (streq(av[1], "-z")) {
		gzip();
	} else {
		gunzip();
	}
}

gzip()
{
	char	buf[8192];
	int	n;

	while ((n = read(0, buf, sizeof(buf))) > 0) {
		gzip2fd(buf, n, 1);
	}
	gzip_done();
}

gunzip()
{
	char	buf[8192];
	int	n;

	while ((n = read(0, buf, sizeof(buf))) > 0) {
		gunzip2fd(buf, n, 1);
	}
	gzip_done();
}
#endif
