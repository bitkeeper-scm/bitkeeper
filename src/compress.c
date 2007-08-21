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
gzip_done(void)
{
	inflateEnd(&gzip_in);
	deflateEnd(&gzip_out);
}


private u32
gzip_hdr(int rfd)
{
	u16	hlen;

	hlen = 0;
	if (readn(rfd, (char *) &hlen, sizeof(hlen)) != sizeof(hlen)) {
		fprintf(stderr, "BAD gzip hdr\n");
		return (0); /* force EOF */
	}
	return (ntohs(hlen));
}

private int
send_gzip_hdr(int fd,  u16 n)
{
	u16 hlen;

	hlen = htons(n);
	if (writen(fd, (char *) &hlen, sizeof(hlen)) != sizeof(hlen)) {
		if ((errno != EPIPE) || getenv("BK_SHOWPROC")) {
			fprintf(stderr,
			    "gzip: can not write header len %d\n", n);
		}
		exit(1);
	};
	return (sizeof(hlen));
}

/*
 * Compresses the contents of input.
 * Data will be flushed at the end of every call so that each
 * output_buffer can be decompressed independently (but gzip_in the appropriate
 * order since they together form a single compression stream) by the
 * receiver.
 */
int
gzip2fd(char *input, int len, int fd, int hflag)
{
	char	buf[4096];
	int	status, bsize;
	int	bytes = 0, hbytes = 0;
	u16	n;

	unless (len) return (0);

	gzip_out.next_in = input;
	gzip_out.avail_in = len;

	bsize = 4096;
	/* Loop compressing until deflate() returns with avail_out != 0. */
	do {
		/* Set up fixed-size output buffer. */
		gzip_out.next_out = buf;
		//gzip_out.avail_out = sizeof(buf);
		gzip_out.avail_out =  bsize;

		/* Compress as much data into the buffer as possible. */
		if ((status = deflate(&gzip_out, Z_PARTIAL_FLUSH)) == Z_OK) {
			n = bsize - gzip_out.avail_out;
			bytes += n;
			if (hflag) hbytes += send_gzip_hdr(fd, n);
			unless (writen(fd, buf, n) == n) {
				if ((errno != EPIPE) || getenv("BK_SHOWPROC")) {
					perror("write on fd gzip_in gzip2fd");
				}
				exit(1);
			}
		} else {
			fprintf(stderr, "gzip deflate says %d\n", status);
			exit(1);
		}
	} while (gzip_out.avail_out == 0);
	return (bytes + hbytes);
}

/*
 * Uncompresses the input buffer.
 * This must be called for the same size units that the
 * buffer_compress was called, and gzip_in the same order that buffers compressed
 * with that.
 */
int
gunzip2fd(char *input, int len, int fd, int hflag)
{
	char	buf[4096];
	int	n, status, bsize;
	int	bytes = 0;

	bsize = 4096;
	unless (len) return (0);
	gzip_in.next_in = input;
	gzip_in.avail_in = len;
	gzip_in.next_out = buf;
	gzip_in.avail_out = bsize;
	for (;;) {
		if ((status = inflate(&gzip_in, Z_PARTIAL_FLUSH)) == Z_OK) {
			n = bsize - gzip_in.avail_out;
			if (writen(fd, buf, n) != n) {
				perror("write on fd gzip_in gunzip2fd");
				exit(1);
			}
			bytes += n;
			gzip_in.next_out = buf;
			gzip_in.avail_out = bsize;
		} else {
			return (bytes);
		}
	}
}


int
gzip_main(int ac, char **av)
{
	int c, rc, gzip_level = -1;

	unless (av[1]) {
		fprintf(stderr, "usage: %s -z|-u\n", av[0]);
		exit(1);
	}
	while ((c = getopt(ac, av, "z:u")) != -1) { 
		switch (c) {
		    case 'u':	gzip_level = -1; break;
		    case 'z':	gzip_level = atoi(optarg); break;
		    default:  
			    	fprintf(stderr,
					"usage bk _gzip [-z[n] | -u]\n");
			   	return (1);
		}
	}

	if (gzip_level >= 0) {
		rc = gzipAll2fd(0, 1, gzip_level, 0, 0, 1, 0);
	} else {
		rc = gunzipAll2fd(0, 1, 6, 0, 0);
	}
	return (rc);
}

int
gzipAll2fd(int rfd, int wfd, int level, int *in, int *out,
						int hflag, int verbose)
{
	int	n, i, bsize;
	char 	buf[4096];
	char    *spin = "|/-\\";

	bsize = 4096;
	gzip_init(level);
	setmode(rfd, _O_BINARY);
	setmode(wfd, _O_BINARY);
	if (getenv("BK_NOTTY")) verbose = 0;
	while ((n = readn(rfd, buf, bsize)) > 0) { /* must use readn() here */
		if (in) *in += n;
		i = gzip2fd(buf, n, wfd, hflag);
		if (out) *out += i;
		if (verbose) fprintf(stderr, "%c\b", spin[i++ % 4]);

	}
	gzip_done();
	if (hflag) {
		i = send_gzip_hdr(wfd, 0); /* send EOF */
		if (out) *out += i;
	}
	return (0);
}

int
gunzipAll2fd(int rfd, int wfd, int level, int *in, int *out)
{
	u32 	hlen;
	char	buf[4096];
	int	i;
	int	moved = 0, offset = 0;

	if (getenv("BK_REGRESSION")) {
		char	*p = getenv("BK_DIE_OFFSET");
	
		if (p) offset = atoi(p);
	}

	gzip_init(level);
	setmode(rfd, _O_BINARY);
	setmode(wfd, _O_BINARY);
	while (1) {
		hlen = gzip_hdr(rfd); 
		unless (hlen)  break;
		if (in) *in += hlen;
		unless (readn(rfd, buf, hlen) == hlen) {
			fprintf( stderr, "I/O error 1\n");
			break;
		}
		i = gunzip2fd(buf, hlen, wfd, 1);
		moved += i;
		if (offset && (moved >= offset)) exit(1);
	}
	if (out) *out = moved;
	gzip_done();
	return (0);
}


/*
 * callback used by zgets to read data from a file.  Adds the gzip headers
 * used by the read of this file.
 */
int
zgets_hread(void *token, u8 **buf)
{
	int	fd = p2int(token);
	int	len;
	static	char *data = 0;

	len = gzip_hdr(fd);
	assert(len <= BSIZE);
	if (buf) {
		unless (data) data = malloc(BSIZE);
		*buf = data;
		return (readn(fd, data, len));
	} else {
		/* called from zgets_done */
		if (len != 0) {
			ttyprintf("failed to find trailer\n");
		}
		if (data) {
			free(data);
			data = 0;
		}
		return (0);
	}
}

/*
 * callback used by zgets to read data from a file.  Adds the gzip headers
 * used by the read of this file.
 */
int
zgets_hfread(void *token, u8 **buf)
{
	FILE	*f = token;
	u16	hlen;
	int	len;
	static	char *data = 0;

	if (fread((char *) &hlen, 1, sizeof(hlen), f) != sizeof(hlen)) {
		fprintf(stderr, "BAD gzip hdr\n");
		len = 0;
	} else {
		len = ntohs(hlen);
	}
	assert(len <= BSIZE);
	if (buf) {
		unless (data) data = malloc(BSIZE);
		*buf = data;
		return (fread(data, 1, len, f));
	} else {
		/* called from zgets_done */
		if (len != 0) {
			ttyprintf("failed to find trailer\n");
		}
		if (data) {
			free(data);
			data = 0;
		}
		return (0);
	}
}

/*
 * A callback to be used with zputs that encodes these gzip headers
 */
void
zputs_hwrite(void *token, u8 *data, int len)
{
	int	fd = p2int(token);

	send_gzip_hdr(fd, len);
	if (len) writen(fd, data, len);
}

