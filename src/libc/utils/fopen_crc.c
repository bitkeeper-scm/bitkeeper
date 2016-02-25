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

/*
 * CRC wrapper.  This is the outermost layer (closest to disk),
 * it lives below VZIP.
 * This layer is cheap, it adds single digit percents to the time to move
 * the data (XXX - verify w/ lmdd)
 *
 * All blocks are the same size where the size is 1 << %3d below
 * file layout:
 *   BLOCK0
 *   BLOCK1
 *   BLOCK2
 *   ...
 *   BLOCKN-1
 *   BLOCKN
 *   XORBLOCK
 *   EOF
 *
 *   BLOCK0 is
 *     "CRc %3d\n"   // block size == 2^bits
 *     		     // data size == 2^bits - 14 (this block only)
 *		     // old 'CRC' files put len here (never shipped)
 *     <data>
 *     <zero padding>// pad (block size - 14 - datalen) only if last block
 *     <u16 datalen> // block size - 14 is max, less if this is the only block
 *     <u32 crc>     // includes CRC header through <datalen>
 *
 *   BLOCK1 through BLOCKN-1 is
 *     <data>
 *     <u16 datalen> // always block size - 6, says more data is coming
 *     <u32 crc>     // covers <data> through <datalen>
 *
 *   BLOCKN is also eof block marker.  Must exist even if datalen == 0 (EOF)
 *     <data frag block>           // leftover data - could be none
 *     <zero padding>              // pad to next blksz boundary - must exist
 *     <u16 datalen>               // length of the last data block (frag)
 *     				   // always in 0 thru block size - 7 range
 *     <u32 crc>     // covers frag .. datalen
 *
 *   XORBLOCK is
 *     <xor data>    // datalen == block size - 4
 *     <u32 crc>     // covers <data> only
 *
 * Notes:
 *  - for <crc> we use crc32c
 *  - <blksz> varies per-file from 64-64k bytes to minimize file size.
 *    The size should be roughly sqrt(6*filesz) ie. 64k blocks are
 *    ideal for 682M files
 *  - The blocks to be crc-ed start at the beginning of the file
 *    and so the first block contains the "CRC %3d\n" header.
 *  - the eof marker block (len < block size - 6) is there so this
 *    can be read in a stream (i.e., we don't know the whole file size)
 *  - The last crc is for the XORBLOCK
 *  - u32 & u16's encoded in Intel byte order
 *
 * LMXXX
 *  - this is not checking errors on fread/fwrite consistently.
 */

typedef struct {
	FILE	*f;		/* file we are reading/writing */
	FILE	*fme;		/* my filehandle */
	u32	read:1;		/* opened in read mode */
	u32	write:1;	/* opened in write mode */
	u32	didwrite:1;	/* made a write() call */
	u32	oldfmt:1;	/* old fmt with len first (readonly) */
	u32	writepartial:1;	/* have written incomplete block to fc->f */
	u32	doXor:1;	/* while reading, compute XOR */
	u32	didseek:1;	/* did a seek (before a read or write) */
	u32	chkxor:1;	/* verify all crc and xor block */
	u32	xorchkd:1;	/* set when we have done all the crc and xor */
	u32	datasz;		/* size of data in blocks1 through N, not BLK0*/
	u8	bits;		/* log2(datasz + PER_BLK) => number of bits */

	off_t	filesz;		/* -1 or all datalen's added up */

	off_t	offset;		/* seek pointer for the caller  */
	off_t	boff;		/* offset to the start of the next fs block */
	off_t	seekoff;	/* where the last seek seeked to (didseek=1) */
	u32	crc;		/* crc of partial data written to stream */

	u8	*rbuf;		/* Last full block read */
	off_t	rbuf_off;	/* file offset of rbuf (always ->boff) */
	int	rlen;		/* len of rbuf */

	u8	*xor;		/* xor's of written data */
} fcrc;

private	int	crcRead(void *cookie, char *buf, int len);
private	int	crcWrite(void *cookie, const char *buf, int len);
private	fpos_t	crcSeek(void *cookie, fpos_t offset, int whence);
private	int	crcClose(void *cookie);

private	int	best_datasz(u64 est_size);
private	int	crcCheckVerify(fcrc *fc);
private int	fileSize(fcrc *fc);

#define	HDRSZ	(8)	/* CRC %3d\n */
#define PER_BLK	(2 + 4)	/* len & crc */

/* crc a block */
#define	CRC(x, buf, len)	crc32c(x, (const u8 *)buf, len)

#define	XORSZ(fc)	((fc)->datasz + 2)	// length(<data> + <len>)

FILE *
fopen_crc(FILE *f, char *mode, u64 est_size, int chkxor)
{
	fcrc	*fc = new(fcrc);
	int	bits;
	char	c;

	T_FS("FILE %p, mode %s, est_size %llu, cookie %p",
	    f, mode, est_size, fc);
	assert(f);
	fc->f = f;
	fc->filesz = -1;
	if (streq(mode, "w")) {
		fc->write = 1;
		fc->bits = best_datasz(est_size);
	} else if (streq(mode, "r") || streq(mode, "r+")) {
		fc->read = 1;
		if (streq(mode, "r+")) fc->write = 1;
		if (fscanf(fc->f, "CR%c %03d\n", &c, &bits) != 2) {
			fprintf(stderr, "bad data\n");
			// XXX it is possible to recover here,
			// we need to guess at the block size
			// until we find blocks/crc's that match.
			fclose(f);
			return (0);
		}
		if (c == 'C') {
			fc->oldfmt = 1;
			assert(!fc->write); /* can't append here */
		} else {
			assert(c == 'c');
		}
		fc->bits = bits;
		rewind(fc->f);
	} else {
		assert(0);
	}
	fc->chkxor = chkxor;
	fc->fme = funopen(fc,
	    fc->read ? crcRead : 0,
	    fc->write ? crcWrite : 0,
	    fc->read ? crcSeek : 0,
	    crcClose);
	fc->datasz = (1 << fc->bits) - PER_BLK;
	assert(fc->datasz < (1 << 16)); /* must fit in 16-bits */
	fc->rbuf = malloc(fc->datasz);
	fc->didseek = 1;	// bootstrap as though a seek 0 was done

	setvbuf(fc->fme, 0, _IOFBF, (1<<fc->bits));
	T_FS("read %d, write %d, datasz %d", fc->read, fc->write, fc->datasz);
	return (fc->fme);
}

/*
 * Seek to the block 'fc->offset' is in (unless we are at end of file
 * and fc->offset occurred in a block with data, then seek to the
 * block after).
 *
 * LMXXX - this is pretty weird because of the HDRSZ stuff.
 */
private void
seekBlock(fcrc *fc)
{
	int	n;		/* num of blocks into file */
	off_t	new_pos = 0;	/* start of next block to be read */

	T_FS("cookie %p, block %lld, data %lld",
	    fc, (long long)fc->boff, (long long)fc->offset);
	if (fc->boff == fc->offset) return;	/* no seek needed */

	// User's idea of offset, not file's idea
	n = (fc->offset + HDRSZ) / fc->datasz;
	if (n) new_pos = (n * fc->datasz) - HDRSZ;
	if (new_pos != fc->boff) {
		fc->boff = new_pos;
		T_FS("new %lld", (long long)fc->boff);

		// Seek to the file's idea of that block start
		fseek(fc->f, (n * (fc->datasz + PER_BLK)), SEEK_SET);
	}
}

/*
 * Read the next block in the stream and write the data to 'buf'.
 * Returns the number of bytes written
 * 'buf' is assumed to be at least fc->datasz bytes.
 * If returning 0 (at EOF), then also process xor block
 * Returns -1 on error
 */
private int
readBlock(fcrc *fc, char *buf)
{
	u16	len;
	u32	crc, crc2 = 0;
	int	i;
	int	xblock = 0, xor = 0, bsize = fc->datasz;

	T_FS("cookie %p, block %lld", fc, (long long)fc->boff);
	// Are we at or past xor block?
	if ((fc->filesz >= 0) && (fc->boff > fc->filesz)) {
		// if read xor block already -> EOF
		if (fc->boff > fc->filesz + bsize) {
			T_FS("past xor");
			return (0);
		}
		T_FS("reading xor");
		xblock = 1;
	}

	if (fc->boff == 0) {
		char	hdr[HDRSZ];

		T_FS("reading header");
		fread(hdr, 1, HDRSZ, fc->f);
		crc2 = CRC(crc2, hdr, HDRSZ);
		if (fc->doXor) {
			for (; xor < HDRSZ; xor++) fc->xor[xor] ^= hdr[xor];
		}
		bsize -= HDRSZ;
	}
	if (fc->oldfmt) {
		T_FS("old read format");
		fread(&len, 2, 1, fc->f);
		if (fc->doXor) {
			fc->xor[xor++] ^= ((char *)&len)[0];
			fc->xor[xor++] ^= ((char *)&len)[1];
		}
		crc2 = CRC(crc2, &len, 2);
	}
	fread(buf, 1, bsize, fc->f);
	if (fc->doXor) for (i = 0; i < bsize; i++) fc->xor[xor++] ^= buf[i];
	crc2 = CRC(crc2, buf, bsize);
	unless (fc->oldfmt) {
		fread(&len, 2, 1, fc->f);
		if (fc->doXor) {
			fc->xor[xor++] ^= ((char *)&len)[0];
			fc->xor[xor++] ^= ((char *)&len)[1];
		}
		crc2 = CRC(crc2, &len, 2);
	}
	if (fread(&crc, 4, 1, fc->f) != 1) {
		fprintf(stderr,
		    "crc read: early eof @ %lld\n", (u64)fc->offset);
		errno = EIO;
		return (-1);
	}
	if (crc2 != le32toh(crc)) {
		crcCheckVerify(fc);
		errno = EIO;
		return (-1);
	}
	if (fc->doXor) assert(xor == XORSZ(fc));

	fc->boff += bsize;	// need before the readBlock() recurse
	if (xblock) {
		len = 0;
	} else {
		len = le16toh(len);
		if (len < bsize) {
			if (fc->filesz < 0) {
				fc->filesz = fc->boff - bsize + len;
				T_FS("filesz = %lld", (long long)fc->filesz);
			}
			unless (len) {
				// process xor block
				if (readBlock(fc, buf) < 0) return (-1);
			}
		}
	}
	//fprintf(stderr, "readBlock() ret %d (sz=%d)\n", len, fc->filesz);
	T_FS("= %d", len);
	return (len);
}

private	void
corruptXor(fcrc *fc)
{
	char	*filt = getenv("_BK_BAD_XOR");
	char	*name = fname(fc->f, 0);

	assert(name);
	if (filt && (streq(filt, "ALL") || streq(filt, basenm(name)))) {
		fc->xor[7] ^= 0x8;
	}
}

private	void
readAfterSeek(fcrc *fc)
{
	T_FS("cookie %p", fc);
	assert(!fc->didwrite);
	fc->didseek = 0;

	seekBlock(fc);

	/* if read-only, only xor if reading whole file */
	unless (fc->doXor = (fc->write || !fc->boff)) return;

	fc->seekoff = fc->boff;
	unless (fc->xor) fc->xor = malloc(XORSZ(fc));
	memset(fc->xor, 0, XORSZ(fc));
}

private	int
writeAfterSeek(fcrc *fc)
{
	int	len;

	T_FS("cookie %p", fc);
	assert(!fc->didwrite);
	fc->didseek = 0;
	fc->doXor = 0;

	unless (fc->xor) fc->xor = calloc(1, XORSZ(fc));

	seekBlock(fc);
	assert(!fc->read ? !fc->offset : (fc->seekoff == fc->boff));
	if (fc->boff == fc->offset) return (0);

	/* read rewind write the partial block that remains */
	fc->rbuf_off = fc->boff;
	if ((fc->rlen = readBlock(fc, fc->rbuf)) < 0) return (-1);
	seekBlock(fc);

	len = fc->offset - fc->rbuf_off;
	assert(len <= fc->rlen);	/* no gap when appending data */
	fc->offset = fc->boff;
	T_FS("keep old partial %d", len);
	crcWrite(fc, fc->rbuf, len);
	return (0);
}

private int
crcRead(void *cookie, char *buf, int len)
{
	fcrc	*fc = cookie;
	int	ret = 0;
	int	n;

	T_FS("cookie %p, buf %p, len %d", fc, buf, len);
	//fprintf(stderr, "%p: crcRead len=%d\n", fc->fme, len);
	assert(!fc->didwrite);

	if (fc->didseek) {
		readAfterSeek(fc);
		/* readBlock state machine needs cache to be cleared */
		fc->rlen = 0;
	}

	while (1) {
		if (fc->rlen) {		/* have saved copy of previous read? */
			long	off = (fc->offset - fc->rbuf_off);

			if ((off >= 0) && (off < fc->rlen)) {
				/* buf[0] is inside saved buffer */
				n = min(len, fc->rlen - off);
				T_FS("same block %p %d", fc, n);
				memcpy(buf, fc->rbuf + off, n);
				buf += n;
				fc->offset += n;
				len -= n;
				ret += n;
			}
		}
		/* optimize block boundary & block size with direct read */
		if ((fc->offset == fc->boff) && (len >= fc->datasz)) {
			T_FS("direct");
			unless (n = readBlock(fc, buf)) break;
			if (n < 0) return (-1);
			fc->offset += n;
			ret += n;
			len -= n;
		}
		if (ret) break;

		fc->rbuf_off = fc->boff;
		unless (fc->rlen = readBlock(fc, fc->rbuf)) break;
		if (fc->rlen < 0) return (-1);
	}
	if (!ret && fc->doXor && !fc->seekoff) {	/* xor'd whole file */
		fc->xorchkd = 1;	/* ran the check */
		for (n = 0; n < XORSZ(fc); n++) {
			if (fc->xor[n] &&
			    !getenv("_BK_BAD_XOR") && !getenv("_BK_XOR_OK")) {
				T_FS("bad xor [%d] %x", n, fc->xor[n]);
				errno = EIO;
				fprintf(stderr,
				    "%s: non-zero xor in byte %d\n",
				    fname(fc->f, 0), n);
				return (-1);
			}
		}
		T_FS("xor passed! %s", fname(fc->f, 0));
	}
	T_FS("= %d", ret);
	return (ret);
}

/*
 * Receive block writes from the buffer layer above this layer.
 * These writes should always be full aligned blocks.  At the end of the
 * file we may see a short block.
 */
private int
crcWrite(void *cookie, const char *buf, int len)
{
	fcrc	*fc = cookie;
	int	i, n, c;
	u32	crc;
	u16	olen;
	int	bsize;
	int	ret = len;
	char	hdr[HDRSZ+1];

	T_FS("cookie %p, buf %p, len %d", fc, buf, len);
	if (fc->didseek) {
		if (writeAfterSeek(fc) < 0) return (-1);
	}

	fc->didwrite = 1;

	//fprintf(stderr, "%p: crcWrite len=%d off=%ld\n", fc->fme, len, fc->offset);
	/*
	 * the writepartial is pedantic - a crcWrite(len = 0) will
	 * write the header, but offset will still be 0.  So detect
	 * that case by looking at writepartial
	 */
	if ((fc->offset == 0) && !fc->writepartial) {
		T_FS("header");
		i = sprintf(hdr,  "CRc %03d\n", fc->bits);
		assert(i == HDRSZ);
		fc->crc = CRC(fc->crc, hdr, HDRSZ);
		fwrite(hdr, 1, HDRSZ, fc->f);
		for (i = 0; i < HDRSZ; i++) fc->xor[i] ^= hdr[i];
		fc->writepartial = 1;
	}
	bsize = fc->datasz;
	c = fc->offset - fc->boff;
	if (fc->boff == 0) {
		bsize -= HDRSZ;
		c += HDRSZ;
	}
	while (len) {
		n = min(len, bsize - (fc->offset - fc->boff));
		fc->crc = CRC(fc->crc, buf, n);
		//assert(n + c <= bsize);
		for (i = 0; i < n; i++) fc->xor[c++] ^= buf[i];
		fwrite(buf, 1, n, fc->f);
		buf += n;
		len -= n;
		fc->offset += n;
		if ((fc->offset - fc->boff) >= bsize) {
			T_FS("crcWrite block");
			fc->boff += bsize;
			assert(fc->offset == fc->boff);
			// completed a block
			//fprintf(stderr, "%p: crcWrite endblock @ %ld\n", fc->fme, fc->offset);
			olen = htole16(bsize);
			fc->crc = CRC(fc->crc, &olen, 2);
			fc->xor[c++] ^= (bsize & 0xff);
			putc(bsize & 0xff, fc->f);
			fc->xor[c++] ^= (bsize >> 8);
			putc(bsize >> 8, fc->f);
			crc = htole32(fc->crc);
			fwrite(&crc, 4, 1, fc->f);

			fc->crc = 0;
			fc->writepartial = 0;
			assert(c == XORSZ(fc));
			c = 0;
			bsize = fc->datasz;
		} else {
			fc->writepartial = 1;
		}
	}
	return (ret);
}

/*
 * Determine the size of the data in this file.
 */
private int
fileSize(fcrc *fc)
{
	int	n;
	int	bsize = fc->datasz + PER_BLK;
	long	size;

	T_FS("cookie %p", fc);
	fc->doXor = 0;

	/* find size of file */
	if (fseek(fc->f, 0, SEEK_END) || ((size = ftell(fc->f)) < 0)) {
		perror("crcSeek");
		return (-1);
	}
	assert((size % bsize) == 0);
	n = size / bsize;
	assert(n >= 2);

	n -= 2;
	fseek(fc->f, n*bsize, SEEK_SET);
	fc->boff = n ? n * fc->datasz - HDRSZ : 0;
	fc->rbuf_off = fc->boff;
	fc->rlen = readBlock(fc, fc->rbuf);
	if (fc->rlen < 0) return (-1);
	T_FS("= %lld", (long long)fc->filesz);
	if (fc->filesz < 0) return (-1);
	assert(fc->filesz >= 0);
	return (0);
}

private fpos_t
crcSeek(void *cookie, fpos_t offset, int whence)
{
	fcrc	*fc = cookie;

	T_FS("cookie %p, offset %lld, whence %d",
	    cookie, (long long)offset, whence);
	if (whence == SEEK_CUR) {
		assert(offset == 0);
		return (fc->offset);
	}
	fc->didseek = 1;
	assert(fc->read);
	assert(!fc->didwrite);

	/* avoid finding file size when we don't need to */
	if ((whence == SEEK_SET) && (offset <= fc->offset)) goto done;

	/*
	 * This function needs to have the semantics of lseek(2) so we
	 * need the file size to check for errors.
	 *
	 * In the SEEK_SET case it is possible to avoid calling
	 * fileSize() and reading the last block of the file, but this
	 * code chooses simplicity instead.  With this optimization
	 * SEEK_SET with offset > 0 will need to do an ftell() to get
	 * the file size and only if we are seeking within the last
	 * block do we need to read a block.
	 */
	if (fc->filesz < 0) {
		if (fileSize(fc) < 0) return (-1);
	}
	if (whence == SEEK_END) {
		offset += fc->filesz;
	} else {
		assert(whence == SEEK_SET);
	}
	/*
	 * XXX: legal to seek past EOF in lseek(2), but clamp here?
	 * better as an assert or failure?
	 */
	if (offset >= fc->filesz) offset = fc->filesz;

done:
	if (offset < 0) {
		fc->filesz = -1;
		fc->rlen = 0;
		errno = EINVAL;
		return (-1);
	}
	fc->offset = offset;
	//fprintf(stderr, "%p: crcSeek ret=%ld\n", fc->fme, fc->offset);
	T_FS("= %lld", (long long)fc->offset);
	return (fc->offset);
}

private	int
crcClose(void *cookie)
{
	fcrc	*fc = cookie;
	u32	crc;
	u16	olen;
	int	n, len;
	int	rc = -1;

	T_FS("crcClose %p", fc);
	//fprintf(stderr, "%p: crcClose()\n", fc->fme);
	if (fc->write) {
		/* in case crcWrite not called, make sure header exists */
		crcWrite(fc, fc->rbuf, 0);
		/* add EOF block of null data */
		len = fc->offset - fc->boff;
		n = fc->datasz - len;
		unless (fc->boff) n -= HDRSZ;
		memset(fc->rbuf, 0, n);
		crc = fc->crc;
		crc = CRC(crc, fc->rbuf, n);
		fwrite(fc->rbuf, 1, n, fc->f);
		fc->xor[fc->datasz+0] ^= (len & 0xff);
		fc->xor[fc->datasz+1] ^= (len >> 8);
		olen = htole16(len);
		crc = CRC(crc, &olen, 2);
		fwrite(&olen, 2, 1, fc->f);
		crc = htole32(crc);
		fwrite(&crc, 4, 1, fc->f);
		/* end up with writing xor data out */
		corruptXor(fc);
		fwrite(fc->xor, 1, XORSZ(fc), fc->f);
		crc = htole32(CRC(0, fc->xor, XORSZ(fc)));
		fwrite(&crc, 4, 1, fc->f);
	} else if (fc->chkxor && !fc->xorchkd) {
		char	buf[fc->datasz];

		T_FS("xor close %s", fname(fc->f, 0));
		if (crcSeek(fc, 0, SEEK_SET)) goto err;
		fc->filesz = -1;	/* read whole file; even if changed */
		while ((n = crcRead(fc, buf, fc->datasz)) > 0) /* eat data */;
		if (n < 0) goto err;
		assert(fc->xorchkd);
	}
	rc = 0;
err:
	free(fc->rbuf);
	free(fc->xor);
	free(fc);
 	return (rc);
}

/*
 * find the best check block size based on the estimated size of the
 * resulting file
 *
 * The answer is the nearest power of two to: sqrt(6*est_size)
 */
private int
best_datasz(u64 est_size)
{
	int	best = 0;
	int	bestsz = 0;
	int	nblocks;
	int	out, sz;
	int	bits;

	T_FS("est_size %llu", est_size);
	for (bits = 6; bits <= 16; bits++) {
		sz = 1 << bits;
		nblocks = est_size / (sz - PER_BLK) + 1;

		out = nblocks * PER_BLK + sz;
		if (!best || (out < bestsz)) {
			best = bits;
			bestsz = out;
		} /* else break; // once we find it we are done, yes? */

	}
	T_FS("= %d", best);
	return (best);
}

private int
crcCheckVerify(fcrc *fc)
{
	int	i, c, bsize = XORSZ(fc);
	u32	crc, crc2;
	int	errors = 0;
	int	first_error = 0;
	int	n;
	long	size;
	u8	buf[bsize];

	T_FS("cookie %p", fc);
	fprintf(stderr,
	    "crc error found in %s\nscanning file\n", fname(fc->f, 0));

	/* find size of file */
	if (fseek(fc->f, 0, SEEK_END) || ((size = ftell(fc->f)) < 0)) {
		perror("crcSeek");
		exit(1);
	}
	assert((size % (fc->datasz + PER_BLK)) == 0);
	// number of data blocks and xor block in file
	n = (int)(size / (fc->datasz + PER_BLK));

	if (fseek(fc->f, 0, SEEK_SET)) {
		fprintf(stderr, "seek failed, can't repair\n");
		return (-1);
	}
	unless (fc->xor) fc->xor = malloc(bsize);
	memset(fc->xor, 0, bsize);

	for (c = 0; c < n; c++) {
		fread(buf, 1, bsize, fc->f);
		crc2 = CRC(0, buf, bsize);
		i = fread(&crc, 4, 1, fc->f);
		if ((i != 1) || (le32toh(crc) != crc2)) {
			++errors;
			first_error = c*(fc->datasz + PER_BLK);
			fprintf(stderr,
			    "crc error block %d(sz=%d) %x vs %x\n",
			    c, bsize, crc, crc2);
		} else {
			for (i = 0; i < bsize; i++) fc->xor[i] ^= buf[i];
		}
	}
	unless (errors) {
		fprintf(stderr, "no problems found?\n");
		return (0);
	}
	if (errors > 1) {
		fprintf(stderr, "too many errors to correct\n");
	} else {
		fprintf(stderr, "correctable error found\n");
		fseek(fc->f, first_error, SEEK_SET);
		fread(buf, 1, bsize, fc->f);
		for (i = 0; i < bsize; i++) {
			if (buf[i] != fc->xor[i]) {
				fprintf(stderr,
				    "offset %d was %x should be %x\n",
				    first_error+i,
				    buf[i], fc->xor[i]);
			}
		}
	}
	return (-1);
}
