#include "system.h"

/*
 * CRC wrapper.  This is the outermost layer (closest to disk),
 * it is wrapped around GZIP.
 *
 * All blocks are the same size where the size is 1 << %3d below (chksz+6)
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
 *     "CHK %3d\n"   // chksz == 2^bits - 6
 *     <u16 len>     //  chksz - 8 at the most, if this is the only block
 *     <data>
 *     <u32 chksum>  // includes CHK header
 *
 *   BLOCK1 .. BLOCKN-1 is
 *     <u16 len>     // always chksz
 *     <data>
 *     <u32 chksum>  // covers <len> and <data>
 *
 *   BLOCKN is also eof block marker - it _must_ be less than full; empty ok
 *     <u16 len>                   // length of the last data block (frag)
 *     <data frag block>           // leftover data - could be none
 *     <zero padding>              // pad to next chksz boundary - must exist
 *     <u32 chksum>
 *
 *   XORBLOCK is
 *     <xor data>  len == chksz + 2, chksum is not part of xor
 *     <u32 chksum>
 *
 * Notes:
 *  - for <chksum> we use adler32 (should use crc32c)
 *  - <chksz> varies per-file from 64-64k bytes to minimize file size.
 *    The size should be roughly sqrt(6*filesz) ie. 64k blocks are
 *    ideal for 682M files
 *  - The blocks to be checksumed start at the beginning of the file
 *    and so the first block contains "CHK\n".
 *  - The last checksum is for the XORBLOCK
 *  - u32 & u16's encoded in Intel byte order
 */

typedef struct {
	FILE	*f;		/* file we are reading/writing */
	FILE	*fme;		/* my filehandle */
	u32	write:1;
	u32	doseek:1;	/* next chkRead should seek first */
	u32	eof:1;		/* wrote eof */
	u32	chksz;		/* size of data in checksum blocks */
	u8	bits;		/* log2(chksz + CHKSZ) => number of bits */

	off_t	filesz;	   /* file size */
	off_t	offset;	   /* current offset into data array */

	u8	*buf;
	u8	*xor;
} fchk;

private	int	chkRead(void *cookie, char *buf, int len);
private	int	chkWrite(void *cookie, const char *buf, int len);
private	fpos_t	chkSeek(void *cookie, fpos_t offset, int whence);
private	int	chkClose(void *cookie);

private	int	best_chksz(int est_size);
private	int	chkCheckVerify(fchk *fc);
private int	fileSize(fchk *fc);

#define	HDRSZ	(8)
#define CHKSZ	(4 + 2)

/* checksum a block */
#define	CHKSUM(x, buf, len)	adler32(x, (const u8 *)buf, len)

FILE *
fchksum_open(FILE *f, char *mode, int est_size)
{
	fchk	*fc = new(fchk);
	int	bits;

	if (getenv("_BK_NO_FCHKSUM")) return (f); /* disable */

	fc->f = f;
	if (streq(mode, "w")) {
		fc->write = 1;
		fc->bits = best_chksz(est_size);
	} else {
		assert(streq(mode, "r"));
		if (fscanf(fc->f, "CHK %03d\n", &bits) != 1) {
			fprintf(stderr, "bad data\n");
			// XXX it is possible to recover here,
			// we need to guess at the block size
			// until we find blocks/crc's that match.
			fclose(f);
			return (0);
		}
		fc->bits = bits;
		rewind(fc->f);
	}
	fc->fme = funopen(fc, chkRead, chkWrite, chkSeek, chkClose);
	fc->chksz = (1 << fc->bits) - CHKSZ;
	fc->buf = malloc(fc->chksz + CHKSZ);
	if (fc->write) {
		fc->xor = calloc(1, fc->chksz+2);
		// start with short block
		setbuffer(fc->fme, fc->buf, fc->chksz - HDRSZ);
	} else {
		setbuffer(fc->fme, fc->buf, fc->chksz);
	}
	return (fc->fme);
}

private int
chkRead(void *cookie, char *buf, int len)
{
	fchk	*fc = cookie;
	u32	sum, sum2;
	u16	blen;
	int	n, off = 0;
	int	bsize = fc->chksz;

	if (fc->offset < fc->chksz - HDRSZ) bsize -= HDRSZ;
	assert(len >= bsize);

	if (fc->eof) return (0);

	if (fc->doseek) {
		fc->doseek = 0;
		off = fc->offset;
		if (fc->offset < fc->chksz - HDRSZ) {
			fc->offset = 0;
			fseek(fc->f, 0, SEEK_SET);
		} else {
			/* n == number of full blocks to skip */
			n = (fc->offset + HDRSZ) / fc->chksz;
			fc->offset = n * fc->chksz - HDRSZ;
			off -= fc->offset;
			fseek(fc->f, (fc->chksz + CHKSZ) * n, SEEK_SET);
		}
		/* 'off' is now sub-block offset */
	}

	sum2 = 0;
	if (fc->offset == 0) {
		fread(buf, 1, HDRSZ, fc->f);
		sum2 = CHKSUM(sum2, buf, HDRSZ);
	}
	fread(&blen, 2, 1, fc->f);
	sum2 = CHKSUM(sum2, &blen, 2);
	fread(buf, 1, bsize, fc->f);
	sum2 = CHKSUM(sum2, buf, bsize);
	n = fread(&sum, 4, 1, fc->f);
	if ((n != 1) || (le32toh(sum) != sum2)) {
		chkCheckVerify(fc);
		exit(1);
	}
	len = le16toh(blen);    /* don't use until after doing checksum */
	if (len < bsize) {
		fc->eof = 1;
	} else {
		assert(len == bsize);
	}
	fc->offset += len;

	if (off) {
		assert(off < len); // unless bounds checking above is altered
		len -= off;
		memmove(buf, buf + off, len);
	}
	return (len);
}

/*
 * Receive block writes from the buffer layer above this layer.
 * These writes should always be full aligned blocks.  At the end of the
 * file we may see a short block.
 */
private int
chkWrite(void *cookie, const char *buf, int len)
{
	fchk	*fc = cookie;
	int	i;
	int	c = 0;
	u32	sum;
	u16	olen;
	u8	*zero;
	int	bsize = fc->chksz;
	char	hdr[16];

	sum = 0;
	if (fc->offset == 0) {
		bsize -= HDRSZ;
		sprintf(hdr,  "CHK %03d\n", fc->bits);
		sum = CHKSUM(sum, hdr, HDRSZ);
		fwrite(hdr, 1, HDRSZ, fc->f);
		for (i = 0; i < HDRSZ; i++) fc->xor[c++] ^= hdr[i];
	}
	olen = htole16(len);
	sum = CHKSUM(sum, &olen, 2);
	sum = CHKSUM(sum, buf, len);
	if (len < bsize) {
		fc->eof = 1;
		zero = calloc(1, bsize - len);
		sum = CHKSUM(sum, zero, bsize - len);
		free(zero);
	} else {
		assert(len == bsize);
	}

	fc->xor[c++] ^= (len & 0xff);
	fc->xor[c++] ^= (len >> 8);
	for (i = 0; i < len; i++) fc->xor[c++] ^= buf[i];

	fwrite(&olen, 2, 1, fc->f);
	fwrite(buf, 1, len, fc->f);
	if (len < bsize) fseek(fc->f, bsize - len, SEEK_CUR);
	sum = htole32(sum);
	fwrite(&sum, 4, 1, fc->f);

	// later blocks are longer
	if (fc->offset == 0) setbuffer(fc->fme, fc->buf, fc->chksz);
	fc->offset += len;
	return (len);
}

/*
 * Determine the size of the data in this file.
 */
private int
fileSize(fchk *fc)
{
	int	n, cnt;
	u16	len;
	u32	sum;
	long	size;

	/* find size of file */
	if (fseek(fc->f, 0, SEEK_END) || ((size = ftell(fc->f)) < 0)) {
		perror("chkSeek");
		exit(1);
	}
	assert((size % (fc->chksz + CHKSZ)) == 0);
	// number of data blocks to skip
	n = (int)(size / (fc->chksz + CHKSZ)) - 2;
	assert(n >= 0);

	/* just need to read the len on last block */
	fseek(fc->f, n*(fc->chksz + CHKSZ), SEEK_SET);
	cnt = fread(fc->buf, 1, fc->chksz + CHKSZ, fc->f);
	sum = CHKSUM(0, fc->buf, fc->chksz+2);
	if ((cnt != fc->chksz + CHKSZ) ||
	    (sum != le32toh(*(u32 *)(fc->buf + fc->chksz + 2)))) {
		chkCheckVerify(fc);
		exit(1);
	}
	len = le16toh(*(u16 *)(fc->buf + (n ? 0 : HDRSZ)));
	return (n ? n*fc->chksz + len - HDRSZ : len);
}

private fpos_t
chkSeek(void *cookie, fpos_t offset, int whence)
{
	fchk	*fc = cookie;

	if (whence == SEEK_CUR) {
		assert(offset == 0);
		return (fc->offset);
	}
	assert(!fc->write);

	/* avoid finding file size when we don't need to */
	if (whence == SEEK_SET) {
		if (offset == fc->offset) return (offset);
		unless (offset) goto done; /* rewind */
	}

	/*
	 * This function needs to have the semantics of lseek(2) so we
	 * need the file size of check for errors.
	 *
	 * In the SEEK_SET case it is possible to avoid calling
	 * fileSize() and reading the last block of the file, but this
	 * code chooses simplicity instead.  With this optimization
	 * SEEK_SET with offset > 0 will need to do an ftell() to get
	 * the file size and only if we are seeking within the last
	 * block do we need to read a block.
	 */
	unless (fc->filesz) fc->filesz = fileSize(fc);
	if (whence == SEEK_END) {
		offset += fc->filesz;
	} else {
		assert(whence == SEEK_SET);
	}
	if (offset < 0) {
		errno = EINVAL;
		return (-1);
	}
	if (offset >= fc->filesz) {
		fc->offset = offset;
		fc->eof = 1;
		return (0);
	}
done:	fc->offset = offset;
	fc->doseek = 1;
	fc->eof = 0;
	return (fc->offset);
}

private	int
chkClose(void *cookie)
{
	fchk	*fc = cookie;
	u32	sum, sum2;
	int	rc = 0;
	int	n;

	if (fc->write) {
		/* add EOF block */
		unless (fc->eof) chkWrite(cookie, fc->buf, 0);
		fwrite(fc->xor, 1, fc->chksz+2, fc->f);
		sum = htole32(CHKSUM(0, fc->xor, fc->chksz+2));
		fwrite(&sum, 4, 1, fc->f);
	} else if (fc->eof) {
		/* we have read to end of file so validate last chksum */
		fread(fc->buf, 1, fc->chksz+2, fc->f);
		sum2 = CHKSUM(0, fc->buf, fc->chksz+2);
		n = fread(&sum, 4, 1, fc->f);
		if ((n != 1) || (le32toh(sum) != sum2)) {
			chkCheckVerify(fc);
			exit(1);
		}
	}
	if (rc = (ferror(fc->f) || fclose(fc->f))) {
		perror("close fchksum");
	}
	free(fc->buf);
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
best_chksz(int est_size)
{
	int	best = 0;
	int	bestsz = 0;
	int	nblocks;
	int	out, sz;
	int	bits;

	for (bits = 6; bits <= 16; bits++) {
		sz = 1 << bits;
		nblocks = est_size / (sz - CHKSZ) + 1;

		out = nblocks * CHKSZ + sz;
		if (!best || (out < bestsz)) {
			best = bits;
			bestsz = out;
		} /* else break; // once we find it we are done, yes? */

	}
	return (best);
}

private int
chkCheckVerify(fchk *fc)
{
	int	i, c, bsize;
	u32	sum, sum2;
	int	errors = 0;
	int	first_error = 0;
	int	n;
	struct	stat sb;


	/* find size of file, could use fseek/ftell if that layers better */
	if (fstat(fileno(fc->f), &sb)) {
		perror("chkSeek");
		exit(1);
	}
	assert((sb.st_size % (fc->chksz + CHKSZ)) == 0);
	// number of data blocks and xor block in file
	n = (int)(sb.st_size / (fc->chksz + CHKSZ));

	if (fseek(fc->f, 0, SEEK_SET)) {
		fprintf(stderr, "seek failed, can't repair\n");
		return (-1);
	}
	fc->xor = calloc(1, fc->chksz+2);

	bsize = fc->chksz + 2;
	for (c = 0; c < n; c++) {
		fread(fc->buf, 1, bsize, fc->f);
		sum2 = CHKSUM(0, fc->buf, bsize);
		i = fread(&sum, 4, 1, fc->f);
		if ((i != 1) || (le32toh(sum) != sum2)) {
			++errors;
			first_error = c*(fc->chksz + CHKSZ);
			fprintf(stderr,
			    "checksum error block %d(sz=%d) %x vs %x\n",
			    c, bsize, sum, sum2);
		} else {
			for (i = 0; i < bsize; i++) fc->xor[i] ^= fc->buf[i];
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
		fread(fc->buf, 1, bsize, fc->f);
		for (i = 0; i < bsize; i++) {
			if (fc->buf[i] != fc->xor[i]) {
				fprintf(stderr,
				    "offset %d was %x should be %x\n",
				    first_error+i,
				    fc->buf[i], fc->xor[i]);
			}
		}
	}
	return (-1);
}
