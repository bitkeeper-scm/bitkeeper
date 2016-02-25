/*
 * Copyright 2011-2013,2016 BitMover, Inc
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
#include "lz4/lz4.h"

/*
 * GZIP wrapper.  This is closer to the application than the CRC layer,
 * CRC wraps this.  This layer is currently used to unwrap and present
 * user data to the application (I suppose someone could layer something
 * more on top but nobody does today).
 *
 * file layout:
 *   "ZIP\n"  || "VZ0\n" || "LZ4\n"
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
 *   - <compressed data>
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

typedef	int (cmpfn)(const void *in, int ilen, void *out, int *olen);

typedef struct {
	u32	usz;		/* uncompressed size */
	u32	off;		/* offset in compressed stream */
} szblock;

typedef struct {
	FILE	*fin;		/* file we are reading/writing */
	u32	read:1;		/* opened in read mode */
	u32	write:1;	/* opened in write mode */
	u32	nonlinear:1;	/* read blocks are not sequential */

	off_t	offset;		/* current offset in uncompressed stream */
	u32	zoffset;	/* current offset in compressed stream */

	cmpfn	*compress;	/* compression function */
	cmpfn	*uncompress;	/* uncompression function */

	szblock	*szarr;
	szblock *szp;		/* next block when reading */
} fgzip;

private	int	select_cmpfn(fgzip *fz, char *buf);
private	int	zipRead(void *cookie, char *buf, int len);
private	int	zipWrite(void *cookie, const char *buf, int len);
private	fpos_t	zipSeek(void *cookie, fpos_t offset, int whence);
private	int	zipClose(void *cookie);
private	int	load_szArray(fgzip *fz);

/*
 * "virtual"-zip, gzip with a mapping table to allow seeks or to have
 * the data stored out of order.
 */
FILE *
fopen_vzip(FILE *fin, char *mode)
{
	fgzip	*fz;
	FILE	*f;
	char	*t;
	szblock	*sz;
	char	fmt[5];

	assert(fin);

	fz = new(fgzip);
	T_FS("FILE %p, mode %s, cookie %p", fin, mode, fz);
	fz->fin = fin;
	if (streq(mode, "w")) {
		fz->write = 1;
		if (t = getenv("_BK_VZIP_FMT")) {
			assert(strlen(t) == 3);
			sprintf(fmt, "%s\n", t);
		} else {
			strcpy(fmt, "LZ4\n");
		}
		fputs(fmt, fin);
	} else {
		if (streq(mode, "a")) {
			fz->write = 1;
		} else {
			assert(streq(mode, "r"));
			fz->read = 1;
		}
		rewind(fz->fin);
		unless (fread(fmt, 1, 4, fin) == 4) {
			return (0);
		}
		fmt[4] = 0;
	}
	if (select_cmpfn(fz, fmt)) {
		fprintf(stderr, "unknown file format '%.3s'\n", fmt);
		assert(0);
	}
	f = funopen(fz,
	    fz->read ? zipRead : 0,
	    fz->write ? zipWrite : 0,
	    zipSeek, zipClose);
	fz->zoffset = 4;
	/* I want to see large block accesses */
	setvbuf(f, 0, _IOFBF, BLOCKSZ);

	if (streq(mode, "a")) {
		// read data at end of file
		if (load_szArray(fz) < 0) return (0);
		// rewind to start of data
		if (fseek(fz->fin,
		    -(sizeof(szblock) * nLines(fz->szarr) + (2 * sizeof(u32))),
			SEEK_END)) {
			return (0);
		}
		// update offset to account for existing data
		fz->offset = 0;
		EACHP(fz->szarr, sz) fz->offset += sz->usz;
		fz->zoffset = ftell(fz->fin);
	}
	return (f);
}

/*
 * Use zlib to compress a block of data.
 * uses 'zlib nowrap' mode so it has no headers or checksums
 *
 * Copy data from in->out and sets olen to the final block size.
 */
private int
zlib_compress(const void *in, int ilen, void *out, int *olen)
{
	z_stream z = {0};
	int	bz;

	/* "nowrap" mode without header or checksums */
	if (deflateInit2(&z, Z_BEST_SPEED, Z_DEFLATED,
		-MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
		perror("init");
		return (-1);
	}
	z.next_out = out;
	z.avail_out = bz = *olen;
	z.next_in = (char *)in;
	z.avail_in = ilen;
	if (deflate(&z, Z_FINISH) != Z_STREAM_END) {
		perror("deflate");
		deflateEnd(&z);
		return (-1);
	}
	*olen = bz - z.avail_out;
	deflateEnd(&z);
	return (0);
}

/*
 * Use zlib to uncompress a block of data.
 *
 * Copy data from in->out and sets olen to the final block size.
 */
private int
zlib_uncompress(const void *in, int ilen, void *out, int *olen)
{
	z_stream z = {0};
	int	bz;

	/* enable "nowrap" mode with no checksums */
	if (inflateInit2(&z, -MAX_WBITS) != Z_OK) {
		perror("init");
		return (0);
	}
	z.next_out = out;
	z.avail_out = bz = *olen;
	z.next_in = (char *)in;
	z.next_in[ilen] = 0; /* zlib reads this dummy byte */
	z.avail_in = ilen+1; /* extra "dummy" byte of gzip */
	if (inflate(&z, Z_FINISH) != Z_STREAM_END) {
		perror("inflate");
		inflateEnd(&z);
		return (-1);
	}
	*olen = bz - z.avail_out;
	inflateEnd(&z);
	return (0);
}

/*
 * copy block with no compression
 *
 * Copy data from in->out and sets olen to the final block size.
 */
private int
none_compress(const void *in, int ilen, void *out, int *olen)
{
	assert(ilen <= *olen);
	memcpy(out, in, ilen);
	*olen = ilen;
	return (0);
}

/*
 * Use LZ4 to compress a block of data.
 *
 * Copy data from in->out and sets olen to the final block size.
 */
private int
lz4_compress(const void *in, int ilen, void *out, int *olen)
{
	*olen = LZ4_compress_limitedOutput(in, out, ilen, *olen);
	return (*olen == 0);
}

/*
 * Use LZ4 to uncompress a block of data.
 *
 * Copy data from in->out and sets olen to the final block size.
 */
private int
lz4_uncompress(const void *in, int ilen, void *out, int *olen)
{
	*olen = LZ4_uncompress_unknownOutputSize(in, out, ilen, *olen);
	return (*olen < 0);
}

private int
select_cmpfn(fgzip *fz, char *buf)
{
	T_FS("cookie %p, buf %s", fz, buf);
	if (streq(buf, "ZIP\n")) {
		fz->compress = zlib_compress;
		fz->uncompress = zlib_uncompress;
	} else if (streq(buf, "VZ0\n")) {
		fz->compress = fz->uncompress = none_compress;
	} else if (streq(buf, "LZ4\n")) {
		fz->compress = lz4_compress;
		fz->uncompress = lz4_uncompress;
	} else {
		return (-1);
	}
	return (0);
}

private int
load_szArray(fgzip *fz)
{
	u32	tmp;
	int	blocks;
	szblock	*sz;

	T_FS("cookie %p", fz);
	/* need to load offset table */
	if (fseek(fz->fin, -sizeof(u32), SEEK_END)) return (-1);
	if (fread(&tmp, sizeof(u32), 1, fz->fin) != 1) return (-1);
	blocks = le32toh(tmp);

	if (fseek(fz->fin,
	    -((blocks * sizeof(szblock)) + 2 * sizeof(u32)), SEEK_END)) {
		fprintf(stderr, "bad szblock\n");
		return (-1);
	}
	if (fread(&tmp, sizeof(u32), 1, fz->fin) != 1) {
		fprintf(stderr, "early eof\n");
		return (-1);
	}
	assert(!tmp);	/* EOF marker of data in file */
	if (fread(growArray(&fz->szarr, blocks),
	    sizeof(szblock), blocks, fz->fin) != blocks) {
		return (-1);
	}
	unless (IS_LITTLE_ENDIAN()) {
		/* byteswap array */
		EACHP(fz->szarr, sz) {
			sz->usz = le32toh(sz->usz);
			sz->off = le32toh(sz->off);
		}
	}
	/* re-read 'blocks' */
	if (fread(&tmp, sizeof(u32), 1, fz->fin) != 1) return (-1);
	/* verify at EOF (needed if talking to crc layer) */
	if (fgetc(fz->fin) != EOF) {
		fprintf(stderr,
		    "vzip file ends at offset %ld, junk follows\n",
		    ftell(fz->fin) - 1);
		return (-1);
	} else if (ferror(fz->fin)) { /* EOF returned for EOF and error */
		return (-1);
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
vzip_findSeek(FILE *fin, long off, int len, u32 pagesz, u32 **lens)
{
	fgzip	*fz;
	szblock	*sz;
	int	i;
	u32	x, blen;

	if (fin->_read != zipRead) return (1);

	fz = fin->_cookie;
	T_FS("cookie %p, off %ld, len %d, pagesz %d", fz, off, len, pagesz);
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
	u32	cnt;
	size_t	n;
	char	data[MAXZIPBLOCK];

	T_FS("cookie %p, len %d", fz, len);
again:
	if (fz->szarr) {
		if (!fz->szp || (fz->szp > fz->szarr + nLines(fz->szarr))) {
			goto eof;
		}
		if (fz->zoffset != fz->szp->off) {
			if (fseek(fz->fin, fz->szp->off, SEEK_SET) < 0) {
				perror("fseek");
				return (-1);
			}
			fz->zoffset = fz->szp->off;
			fz->nonlinear = (fz->offset != 0);
		}
		++fz->szp;	   /* next block */
	}

	/* read compressed length */
	if ((n = fread(&cnt, sizeof(u32), 1, fz->fin)) != 1) {
		if (!n && fz->nonlinear) return (0); /* for repeating eof's */
		return (-1);
	}
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
eof:		/* eof */
		T_FS("return eof cookie %p, len %d", fz, len);
		unless (fz->nonlinear) {
			fz->nonlinear = 1;
			while (sizeof(data) ==
			    fread(data, 1, sizeof(data), fz->fin)) {
				/* drain */
			}
			if (ferror(fz->fin)) return (-1);
		}
		return (0);
	}
	assert(cnt < sizeof(data)-1);

	/* read compressed data */
	if (fread(data, 1, cnt, fz->fin) != cnt) {
		perror("fread");
		return (-1);
	}
	if (fz->uncompress(data, cnt, buf, &len)) return (-1);
	fz->offset += len;
	fz->zoffset += sizeof(u32) + cnt;
	T_FS("return cookie %p, len %d", fz, len);
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

	T_FS("cookie %p", fz);
	sz.usz = len;
	sz.off = fz->zoffset;
	addArray(&fz->szarr, &sz);

	csz = sizeof(data);
	if (fz->compress(buf, len, data, &csz)) return (-1);
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

	T_FS("cookie %p %lld %d", fz, (long long)offset, whence);
	if (whence == SEEK_CUR) {
		assert(offset == 0);
		return (fz->offset);
	}
	assert(fz->read);
	/* avoid loading table when we don't need to */
	if ((whence == SEEK_SET) && (offset == fz->offset)) return (offset);

	if (whence == SEEK_END) {
		unless (fz->szarr) {
			if (load_szArray(fz) < 0) return (-1);
		}
		EACHP(fz->szarr, sz) offset += sz->usz;
	} else {
		assert(whence == SEEK_SET);
	}
	if (offset < 0) {
		fseeko(fz->fin, offset, SEEK_SET);
		fz->szp = 0;
		FREE(fz->szarr);
err:		errno = EINVAL;
		return (-1);
	}
	fz->offset = offset;

	/* find offset of desired block */
	unless (fz->szarr) {
		if (load_szArray(fz) < 0) return (-1);
	}
	sz = 0;
	EACHP(fz->szarr, sz) {
		if (offset == 0) {
			break;
		} else if (offset < 0) {
			fprintf(stderr, "fgzip: fseek to invalid offset %llu\n",
			  (u64)fz->offset);
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
	int	rc;

	T_FS("cookie %p", fz);
	if (fz->write) {
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
	}
	rc = ferror(fz->fin);
	free(fz->szarr);
	free(fz);
	return (rc);
}
