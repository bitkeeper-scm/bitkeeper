#include "system.h"

typedef struct {
	u32		write:1;	 /* opened for writing */
	u32		eof:1;		 /* saw eof while reading */
	u32		header:1;	 /* 2-byte len for each block */
	u32		doCksum:1;	 /* calc checksums */
	u32		unbuffer:1;	 /* use read() */
	u32		gotNextLen:1;	 /* bit for header state machine */
	u16		nextlen;
	FILE		*f;		 /* base data stream */
	z_stream	z;
	u16		*cksump;	 /* update checksum here */
	u16		cksum;		 /* current chksum */
	int		*incntp, *outcntp;
	int		incnt, outcnt;
	int		obuflen;
	u8		*obuf;		 /* local storage */
} zbuf;

private	int	zRead(void *cookie, char *buf, int len);
private	int	zWrite(void *cookie, const char *buf, int len);
private	int	zClose(void *cookie);
private	int	zCloseWrite(zbuf *zf);
private	int	doFill(zbuf *zf);
private	void	writeBlock(zbuf *zf);

/*
 * Like fopen() but reads and writes zips files
 *
 * mode can be one of "rw#hcu"
 *
 * Some options include extra function arguments that they appear
 * in the same order as the option letters.
 *
 * code extra_args		meaning
 * r				read
 * w	int level		write
 * #	int *in, int *out	return bytes read/written
 * c	u16 *cksum		count bk cksum of output
 * h				use 2 byte len 'header' between blocks
 * u				unbuffered (only with rh)
 *
 * For "w#", the parameters are level, in, out
 * For "#w", the parameters are in, out, level
 */
FILE *
fopen_zip(FILE *f, char *mode, ...)
{
	zbuf	*zf;
	int	level = -1;
	int	write = -1;
	va_list	ap;

	setmode(fileno(f), _O_BINARY);
	zf = new(zbuf);
	zf->f = f;
	va_start(ap, mode);
	for (; *mode; mode++) {
		switch(*mode) {
		    case 'r':
			write = 0;
			break;
		    case 'w':
			write = 1;
			level = va_arg(ap, int);
			break;
		    case '#':
			zf->incntp = va_arg(ap, int*);
			zf->outcntp = va_arg(ap, int*);
			break;
		    case 'c':
			zf->doCksum = 1;
			zf->cksump = va_arg(ap, u16 *);
			break;
		    case 'h':
			zf->header = 1;
			break;
		    case 'u':
			zf->unbuffer = 1;
			break;
		}
	}
	if (zf->unbuffer) assert(zf->header);
	assert(write != -1);
	if (write) {
		zf->obuflen = 16<<10;
		zf->obuf = malloc(zf->obuflen);
		zf->write = 1;
		zf->z.next_out = zf->obuf;
		zf->z.avail_out = zf->obuflen;
		if (deflateInit(&zf->z, (level == -1) ? Z_BEST_SPEED : level)) {
			perror("fopen_zip");
			return (0);
		}
		f = funopen(zf, 0, zWrite, 0, zClose);
	} else {
		if (inflateInit(&zf->z) != Z_OK) {
			perror("inflateInit");
			return (0);
		}
		zf->obuflen = 32<<10;
		zf->obuf = malloc(zf->obuflen);
		f = funopen(zf, zRead, 0, 0, zClose);
	}
	return (f);
}

private int
zRead(void *cookie, char *buf, int len)
{
	zbuf	*zf = cookie;
	int	cnt, err;

	if (zf->eof) return (0);

	zf->z.next_out = buf;
	zf->z.avail_out = len;
	while (zf->z.avail_out) {
		unless (zf->z.avail_in) {
			zf->z.next_in = zf->obuf;
			zf->z.avail_in = doFill(zf);
		}
		unless (zf->z.avail_in) {
			/*
			 * We shouldn't see this because the previous block
			 * should unpack to a Z_STREAM_END marker.  Versions
			 * of bk before 4.1 didn't include a stream marker
			 * so may die here
			 */
			fprintf(stderr, "zRead: error premature EOF\n");
			return (-1);
		}
		err = inflate(&zf->z, Z_NO_FLUSH);
		if (err == Z_STREAM_END) {
			if (zf->header) {
				if (zf->z.avail_in) {
					fprintf(stderr,
					    "zRead: not all input consumed\n");
					return (-1);
				}
				zf->z.avail_in = doFill(zf);
				if (zf->z.avail_in) {
					fprintf(stderr,
					    "zRead: no eof marker\n");
					return (-1);
				}
			}
			zf->eof = 1;
			break;
		} else if (err != Z_OK) {
			TRACE("err=%d", err);
			return (-1);
		}
	}
	cnt = len - zf->z.avail_out;
	TRACE("cnt=%d", cnt);
	zf->outcnt += cnt;
	return (cnt);
}

private int
zWrite(void *cookie, const char *buf, int len)
{
	zbuf	*zf = cookie;
	int	err;

	assert(len > 0);
	zf->z.next_in = (char *)buf;
	zf->z.avail_in = len;
	do {
		if (err = deflate(&zf->z, Z_NO_FLUSH)) {
			fprintf(stderr, "zWrite: compression failure %d\n",
			    err);
			return (-1);
		}
		if (zf->z.avail_out == 0) {
			writeBlock(zf);
			zf->z.next_out = zf->obuf;
			zf->z.avail_out = zf->obuflen;
		}
	} while (zf->z.avail_in > 0);
	zf->incnt += len;
	return (len);
}

private	int
zClose(void *cookie)
{
	zbuf	*zf = cookie;
	int	rc;

	if (zf->write) {
		rc = zCloseWrite(zf);
	} else {
		rc = inflateEnd(&zf->z);
	}
	if (zf->incntp) *zf->incntp += zf->incnt;
	if (zf->outcntp) *zf->outcntp += zf->outcnt;
	if (zf->cksump) *zf->cksump += zf->cksum;
	free(zf->obuf);
	free(zf);
	return (rc);
}

private int
zCloseWrite(zbuf *zf)
{
	int	err;

	assert(zf->z.avail_in == 0);
	do {
		err = deflate(&zf->z, Z_FINISH);
		if (err != Z_STREAM_END && err != Z_OK) {
			fprintf(stderr,
			    "zClose: finish failure %d\n", err);
			return (-1);
		}
		writeBlock(zf);
		zf->z.next_out = zf->obuf;
		zf->z.avail_out = zf->obuflen;
	} while (err != Z_STREAM_END);
	if (zf->header) {
		putc(0, zf->f);
		putc(0, zf->f);
		zf->outcnt += 2;
	}
	fflush(zf->f);
	if (deflateEnd(&zf->z)) {
		fprintf(stderr,
		    "zClose: compression failure end %d\n", err);
		return (-1);
	}
	return (0);
}

private int
doFill(zbuf *zf)
{
	u16	olen = 0;
	int	cnt = 0;

	if (zf->header) {
		/*
		 * We pipeline length headers so we only need one
		 * read() per block.
		 */
		if (zf->gotNextLen) {
			olen = zf->nextlen;
		} else {
			zf->gotNextLen = 1;
			if (zf->unbuffer) {
				cnt = readn(fileno(zf->f), &olen, 2);
			} else {
				cnt = fread(&olen, 1, 2, zf->f);
			}
			zf->incnt += cnt;
			if (cnt != 2) {
				perror("doFill");
				return (0);
			}
			olen = ntohs(olen);
		}
		TRACE("olen=%d", olen);
		assert(olen <= zf->obuflen-2);
		if (olen) {
			if (zf->unbuffer) {
				cnt = readn(fileno(zf->f), zf->obuf, olen+2);
			} else {
				cnt = fread(zf->obuf, 1, olen+2, zf->f);
			}
			zf->incnt += cnt;
			if (cnt != olen+2) {
				perror("doFill");
				return (0);
			}
			cnt -= 2;
			memcpy(&olen, zf->obuf+olen, 2);
			zf->nextlen = ntohs(olen);
		} else {
			cnt = 0;
			zf->eof = 1;
		}
	} else {
		cnt = fread(zf->obuf, 1, zf->obuflen, zf->f);
		zf->incnt += cnt;
	}
	return (cnt);
}

private void
writeBlock(zbuf *zf)
{
	int	olen;

	if ((olen = zf->obuflen - zf->z.avail_out) > 0) {
		if (zf->header) {
			putc(olen >> 8, zf->f);
			putc(olen & 0xff, zf->f);
			zf->outcnt += 2;
		}
		if (zf->doCksum) {
			u32	sum = 0;
			int	i;

			if (zf->header) {
				sum += (olen >> 8) + (olen & 0xff);
			}
			for (i = 0; i < olen; sum += zf->obuf[i++]);
			zf->cksum += (u16)sum;
		}
		fwrite(zf->obuf, 1, olen, zf->f);
		zf->outcnt += olen;
	}
}
