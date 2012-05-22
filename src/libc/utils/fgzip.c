#include "system.h"

/*
 * GZIP wrapper.  This is closer to the application than the CRC layer,
 * CRC wraps this.  This layer is currently used to unwrap and present
 * user data to the application (I suppose someone could layer something
 * more on top but nobody does today).
 *
 * file layout:
 *   "ZIP\n"  || "VIP\n"
 *   BLOCK     #2        # these are in arbitrary order
 *   BLOCK     #1
 *   BLOCK
 *   ...
 *   <u32 zero>  # eof marker
 *   SZBLOCK   #1        # these in file order
 *   SZBLOCK   #2
 *   SZBLOCK
 *   ...
 *   <u32 nblocks>
 *   EOF
 *
 *   BLOCK
 *      <u32 len>	# High bit set if not in linear order
 *      <len bytes of compressed data>
 *
 *   SZBLOCK
 *      <u32 uncompressed len>
 *      <u32 file offset to start of compressed block>
 *
 * Notes:
 *   - VIP is all of the indirect blocks but no compression
 *   - If the high bit is never set, you can just stream this data
 *   - Layout is designed to allow adding data in the middle by
 *     truncating at the SZBLOCK table and appending new data and
 *     tables to the end of the file. (not quite append-only, but
 *     close)
 *   - <compressed data> uses 'zlib nowrap' mode so it has no headers
 *     or checksums
 *   - u32's encoded in Intel byte order
 *   - blocks usually encode 64k of uncompressed data, but may be
 *     shorter.  The compressed data may be slightly longer than 64k.
 *   - fseek() is allowed when reading the file, but only to BLOCK
 *     boundries.  While writing, a BLOCK boundry can be forced by
 *     calling fflush().
 *   - without compression this limits us to 64*1024^4 or 70368744177664 bytes
 */
#define	BLOCKSZ		(64<<10)
#define	MAXZIPBLOCK	(80<<10)

typedef struct {
	u32	usz;		/* uncompressed size */
	u32	off;		/* offset in compressed stream */
} szblock;

typedef struct {
	FILE	*fin;		/* file we are reading/writing */
	u32	write:1;

	off_t	offset;		/* current offset in uncompressed stream */
	u32	zoffset;	/* current offset in compressed stream */

	z_stream z;
	szblock	*szarr;
	szblock *szp;		/* next block when reading */
} fgzip;

private	int	zipRead(void *cookie, char *buf, int len);
private	int	zipWrite(void *cookie, const char *buf, int len);
private	fpos_t	zipSeek(void *cookie, fpos_t offset, int whence);
private	int	zipClose(void *cookie);
private	int	load_szArray(fgzip *fz);

FILE *
fgzip_open(FILE *fin, char *mode)
{
	fgzip	*fz;
	FILE	*f;
	char	buf[4];

	if (getenv("_BK_NO_FGZIP")) return (fin);	/* disable */
	unless (fin) return (fin);			/* bad input */

	fz = new(fgzip);
	fz->fin = fin;
	if (streq(mode, "w")) {
		fz->write = 1;
		fputs("ZIP\n", fin);
		/* "nowrap" mode without header or checksums */
		if (deflateInit2(&fz->z, Z_BEST_SPEED, Z_DEFLATED,
			-MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
			perror("init");
			return (0);
		}
		f = funopen(fz, 0, zipWrite, zipSeek, zipClose);
	} else {
		assert(streq(mode, "r"));
		rewind(fz->fin);
		unless ((fread(buf, 1, 4, fin) == 4) &&
		    strneq(buf, "ZIP\n", 4)) {
			free(fz);
			/* not really compressed */
			rewind(fin);
			return (fin);
		}
		/* enable "nowrap" mode with no checksums */
		if (inflateInit2(&fz->z, -MAX_WBITS) != Z_OK) {
			perror("init");
			return (0);
		}
		f = funopen(fz, zipRead, 0, zipSeek, zipClose);
	}
	fz->zoffset = 4;
	/* I want to see large block accesses */
	setvbuf(f, 0, _IOFBF, BLOCKSZ);
	return (f);
}

private int
load_szArray(fgzip *fz)
{
	u32	tmp;
	int	blocks;
	szblock	*sz;

	/* need to load offset table */
	fseek(fz->fin, -sizeof(u32), SEEK_END);
	fread(&tmp, sizeof(u32), 1, fz->fin);
	blocks = le32toh(tmp);

	if (fseek(fz->fin,
	    -((blocks * sizeof(szblock)) + sizeof(u32)), SEEK_END)) {
		fprintf(stderr, "bad szblock\n");
		return (-1);
	}
	fread(growArray(&fz->szarr, blocks),
	    sizeof(szblock), blocks, fz->fin);
	if (0x01020304 != le32toh(0x01020304)) {
		/* byteswap array */
		EACHP(fz->szarr, sz) {
			sz->usz = le32toh(sz->usz);
			sz->off = le32toh(sz->off);
		}
	}
	fz->zoffset = 0;	/* force a seek on next read */
	return (0);
}

/*
 * Given a range of a fgzip file and a desired blocksize, return a
 * list of seek boundries that split the region into blocks.
 *
 * Return 0 on success.
 * If 'fin' was not from fgzip_open(), then leave 'seeks' unmodified.
 */
int
fgzip_findSeek(FILE *fin, long off, int len, u32 pagesz, u32 **lens)
{
	fgzip	*fz;
	szblock	*sz;
	int	i;
	u32	x, blen;

	if (fin->_read != zipRead) return (1);

	fz = fin->_cookie;
	unless (fz->szarr) {
		rewind(fin);
		assert(fz->szarr);
	}
	EACH(fz->szarr) {
		if (off == 0) break;
		sz = &fz->szarr[i];
		off -= sz->usz;
	}
	assert(!off);
	x = off;
	blen = 0;
	EACH_START(i, fz->szarr, i) {
		sz = &fz->szarr[i];
		blen += sz->usz;
		x += sz->usz;
		if ((x < len) && (blen < pagesz)) {
			continue;
		}
		addArray(lens, &blen);
		if (x == len) break;
		if (x > len) return (-1);
		blen = 0;
	}
	return (0);
}

private int
zipRead(void *cookie, char *buf, int len)
{
	fgzip	*fz = cookie;
	int	rc;
	u32	cnt;
	char	data[MAXZIPBLOCK];

again:
	if (fz->szarr) {
		if (!fz->szp || (fz->szp > fz->szarr + nLines(fz->szarr))) {
			/* eof */
			return (0);
		}
		if (fz->zoffset != fz->szp->off) {
			if (fseek(fz->fin, fz->szp->off, SEEK_SET) < 0) {
				perror("fseek");
				return (-1);
			}
			fz->zoffset = fz->szp->off;
		}
		++fz->szp;	   /* next block */
	}

	/* read compressed length */
	fread(&cnt, sizeof(u32), 1, fz->fin);
	cnt = le32toh(cnt);

	if (cnt & 0x80000000) {
		unless (fz->szarr) {
			/*
			 * zipSeek short circuits if no movement, so fake
			 * it out by saying we are somewhere else and then
			 * move to where we are.  This will call
			 * load_szArray which will set zoffset to 0 which
			 * will then seek above when we loop to 'again'.
			 * No block has zoffset of 0 because of ZIP\n header.
			 */
			++fz->offset;
			if (zipSeek(fz, fz->offset-1, SEEK_SET) < 0) {
				perror("fseek");
				return (-1);
			}
			assert(fz->szarr);
			goto again;
		}
		cnt &= ~0x80000000;
	}
	unless (cnt) {
		/* eof */
		return (0);
	}
	assert(cnt < sizeof(data)-1);

	/* read compressed data */
	if (fread(data, 1, cnt, fz->fin) != cnt) {
		perror("fread");
		return (-1);
	}
	data[cnt] = 0;		/* zlib reads this dummy byte*/

	/* uncompress to stdio buffer */
	fz->z.next_out = buf;
	fz->z.avail_out = len;
	fz->z.next_in = data;
	fz->z.avail_in = cnt+1;	/* extra "dummy" byte for gzip */
	rc = inflate(&fz->z, Z_FINISH);
	if (rc != Z_STREAM_END) {
		fprintf(stderr, "inflate failed, rc=%d\n", rc);
		return (-1);
	}
	len -= fz->z.avail_out;

	if (inflateReset(&fz->z) != Z_OK) {
		perror("inflateReset");
		return (-1);
	}
	fz->offset += len;
	fz->zoffset += sizeof(u32) + cnt;
	return (len);		/* we usually return less than requested */
}

private int
zipWrite(void *cookie, const char *buf, int len)
{
	fgzip	*fz = cookie;
	szblock	sz;
	u32	csz;
	u32	tmp;
	char	data[MAXZIPBLOCK];

	sz.usz = len;
	sz.off = fz->zoffset;
	addArray(&fz->szarr, &sz);

	fz->z.next_out = data;
	fz->z.avail_out = sizeof(data);
	fz->z.next_in = (char *)buf;
	fz->z.avail_in = len;
	if (deflate(&fz->z, Z_FINISH) != Z_STREAM_END) {
		perror("deflate");
		return (-1);
	}
	csz = sizeof(data) - fz->z.avail_out;

	if (deflateReset(&fz->z) != Z_OK) {
		perror("deflateRest");
		return (-1);
	}

	tmp = htole32(csz);
	fwrite(&tmp, sizeof(u32), 1, fz->fin);
	if (fwrite(data, 1, csz, fz->fin) != csz) {
		perror("fwrite");
		return (-1);
	}
	fz->offset += len;
	fz->zoffset += 4 + csz;
	return (len);
}

private fpos_t
zipSeek(void *cookie, fpos_t offset, int whence)
{
	fgzip	*fz = cookie;
	szblock	*sz;

	if (whence == SEEK_CUR) {
		assert(offset == 0);
		return (fz->offset);
	}
	assert(!fz->write);
	/* avoid loading table when we don't need to */
	if ((whence == SEEK_SET) && (offset == fz->offset)) return (offset);

	unless (fz->szarr) load_szArray(fz);
	if (whence == SEEK_END) {
		EACHP(fz->szarr, sz) offset += sz->usz;
	} else {
		assert(whence == SEEK_SET);
	}
	if (offset < 0) {
err:		errno = EINVAL;
		return (-1);
	}
	fz->offset = offset;

	/* find offset of desired block */
	sz = 0;
	EACHP(fz->szarr, sz) {
		if (offset == 0) {
			break;
		} else if (offset < 0) {
			fprintf(stderr, "fgzip: fseek to invalid offset %ld\n",
				fz->offset);
			goto err;
		}
		offset -= sz->usz;
	}
	fz->szp = sz;
	return (fz->offset);
}

private	int
zipClose(void *cookie)
{
	fgzip	*fz = cookie;
	szblock	*sz;
	u32	sum;
	int	rc = 0;

	if (fz->write) {
		deflateEnd(&fz->z);

		/* write eof marker */
		sum = 0;
		fwrite(&sum, sizeof(u32), 1, fz->fin);

		/* write sz array */
		EACHP(fz->szarr, sz) {
			sz->usz = htole32(sz->usz);
			sz->off = htole32(sz->off);
		}
		fwrite(fz->szarr + 1, sizeof(szblock), nLines(fz->szarr),
		    fz->fin);

		/* write number of blocks */
		sum = htole32(nLines(fz->szarr));
		fwrite(&sum, sizeof(u32), 1, fz->fin);
	} else {
		inflateEnd(&fz->z);
	}
	if (fclose(fz->fin)) {
		perror("zipClose");
		rc = -1;
	}
	free(fz->szarr);
	free(fz);
	return (rc);
}
