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

static z_stream gzip_in;
static z_stream gzip_out;
private int gzip();
private int gunzip();

/*
 * Initializes compression; level is compression level from 1 to 9
 * (as gzip_in gzip).
 */

void 
gzip_init(int level)
{
	inflateInit(&gzip_in);
	deflateInit(&gzip_out, level);
}

/* Frees any data structures allocated for compression. */

void 
gzip_done()
{
	inflateEnd(&gzip_in);
	deflateEnd(&gzip_out);
}

/*
 * Compresses the contents of input.
 * Data will be flushed at the end of every call so that each
 * output_buffer can be decompressed independently (but gzip_in the appropriate
 * order since they together form a single compression stream) by the
 * receiver.
 */
int
gzip2fd(char *input, int len, int fd)
{
	char	buf[4096];
	int	n, status;
	int	bytes = 0;

	unless (len) return (0);

	gzip_out.next_in = input;
	gzip_out.avail_in = len;

	/* Loop compressing until deflate() returns with avail_out != 0. */
	do {
		/* Set up fixed-size output buffer. */
		gzip_out.next_out = buf;
		gzip_out.avail_out = sizeof(buf);

		/* Compress as much data into the buffer as possible. */
		if ((status = deflate(&gzip_out, Z_PARTIAL_FLUSH)) == Z_OK) {
			n = sizeof(buf) - gzip_out.avail_out;
			bytes += n;
			unless (write(fd, buf, n) == n) {
				perror("write on fd gzip_in gzip2fd");
				exit(1);
			}
		} else {
			fprintf(stderr, "gzip deflate says %d\n", status);
			exit(1);
		}
	} while (gzip_out.avail_out == 0);
	return (bytes);
}

/*
 * Uncompresses the input buffer.
 * This must be called for the same size units that the
 * buffer_compress was called, and gzip_in the same order that buffers compressed
 * with that.
 */
int
gunzip2fd(char *input, int len, int fd)
{
	char	buf[4096];
	int	n, status;
	int	bytes = 0;

	unless (len) return (0);
	gzip_in.next_in = input;
	gzip_in.avail_in = len;
	gzip_in.next_out = buf;
	gzip_in.avail_out = sizeof(buf);
	for (;;) {
		if ((status = inflate(&gzip_in, Z_PARTIAL_FLUSH)) == Z_OK) {
			n = sizeof(buf) - gzip_in.avail_out;
			if (write(fd, buf, n) != n) {
				perror("write on fd gzip_in gunzip2fd");
				exit(1);
			}
			bytes += n;
			gzip_in.next_out = buf;
			gzip_in.avail_out = sizeof(buf);
		} else {
			return (bytes);
		}
	}
}


int
gzip_main(int ac, char **av)
{
	int c, rc, uflag = 0, gzip_level = 0;

	unless (av[1]) {
		fprintf(stderr, "usage: %s -z|-u\n", av[0]);
		exit(1);
	}
	while ((c = getopt(ac, av, "z:u")) != -1) { 
		switch (c) {
		    case 'u':	uflag; break;
		    case 'z':	gzip_level = atoi(optarg); break;
		    default:  
usage:			    	fprintf(stderr,
					"usage bk _gzip [-z[n] | -u]\n");
			   	return (1);
		}
	}

	if (uflag && gzip_level) goto usage;	
	if (gzip_level) {
		gzip_init(gzip_level); 
		rc = gzip();
	} else {
		gzip_init(6); 
		rc = gunzip();
	}
	return (rc);
}

private int
gzip()
{
	char	buf[4096];
	int	n;

	while ((n = read(0, buf, sizeof(buf))) > 0) {
		gzip2fd(buf, n, 1);
	}
	gzip_done();
	return (0);
}

private int
gunzip()
{
	char	buf[4096];
	int	n;

	while ((n = read(0, buf, sizeof(buf))) > 0) {
		gunzip2fd(buf, n, 1);
	}
	gzip_done();
	return (0);
}
