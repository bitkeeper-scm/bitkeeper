/*
 * Copyright 2002-2008,2010-2011,2015-2016 BitMover, Inc
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
#include "sccs.h"
#define	LTC_SOURCE
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"
#include "tomcrypt/oldrsa.h"
#include "cmd.h"
#include "zlib/zlib.h"

private	int	make_keypair(int bits, char *secret, char *public);
private	int	signdata(rsa_key *secret);
private	int	validatedata(rsa_key *public, char *sign);
private	int	encrypt_stream(rsa_key *public, FILE *fin, FILE *fout);
private	int	decrypt_stream(rsa_key	*secret, FILE *fin, FILE *fout);
private void	loadkey(char *file, rsa_key *key);
private	int	symEncrypt(char *key, FILE *fin, FILE *fout);
private	int	symDecrypt(char *key, FILE *fin, FILE *fout);

private	int		wprng = -1;
private	prng_state	*prng;
private	int	use_sha1_hash = 0;
private	int	hex_output = 0;
private	int	rsakey_old = 0;

/*
 * -i bits secret-key public-key
 *    # generate new random keypair and writes the two keys to files
 * -e key < plain > cipher
 *    # encrypt a datastream on stdin and write the data to stdout
 *    # normally the public key is used for this
 * -d key < cipher > plain
 *    # decrypt a datastream on stdin and write the data to stdout
 *    # normally the private key is used for this
 * -s key < data > sign
 *    # read data from stdin and write signature to stdout
 *    # normally private key is used for this
 * -v key sign < data
 *    # read signature from file and data from stdin
 *    # exit status indicates if the data matches signature
 *    # normally public key is used for this
 * -h <data> [<key>]
 *    # hash data with an optional key
 *
 * -E key < plain > cipher
 *    # simple aes symetric encryption of data
 *    # key must be 16 bytes long
 * -D key < plain > cipher
 *    # simple aes symetric decryption of data
 *    # key must be 16 bytes long
 * -S
 *    # use sha1 instead of md5 for -h
 * -X
 *    # print hash results in hex instead of base64 for -h
 * -O
 *    # load RSA key in old format
 */

int
crypto_main(int ac, char **av)
{
	int	c;
	int	mode = 0;
	int	args, optargs;
	int	ret = 1;
	char	*hash;
	rsa_key	key;

	while ((c = getopt(ac, av, "dDeEhiOsSvX", 0)) != -1) {
		switch (c) {
		    case 'h': case 'i': case 's': case 'v':
		    case 'e': case 'd': case 'E': case 'D':
			if (mode) usage();
			mode = c;
			break;
		    case 'S': use_sha1_hash = 1; break;
		    case 'X': hex_output = 1; break;
		    case 'O': rsakey_old = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	optargs = 0;
	switch (mode) {
	    case 'i': args = 3; break;
	    case 'v': args = 2; break;
	    default: args = 1; break;
	    case 'h': args = 1; optargs = 1; break;
	}
	if (ac - optind < args || ac - optind > args + optargs) {
		fprintf(stderr, "ERROR: wrong number of args!\n");
		usage();
	}

	setmode(0, _O_BINARY);
	register_cipher(&rijndael_desc);
	register_hash(&md5_desc);
	wprng = rand_getPrng(&prng);

	switch (mode) {
	    case 'i':
		ret = make_keypair(atoi(av[optind]),
		    av[optind+1], av[optind+2]);
		break;
	    case 'e': case 'd': case 's': case 'v':
		loadkey(av[optind], &key);
		switch (mode) {
		    case 'e': ret = encrypt_stream(&key, stdin, stdout); break;
		    case 'd': ret = decrypt_stream(&key, stdin, stdout); break;
		    case 's': ret = signdata(&key); break;
		    case 'v': ret = validatedata(&key, av[optind+1]); break;
		}
		rsa_free(&key);
		break;
	    case 'E':
		if (strlen(av[optind]) == 16) {
			ret = symEncrypt(av[optind], stdin, stdout);
		} else {
			fprintf(stderr,
			    "ERROR: key must be exactly 16 bytes\n");
			ret = 1;
		}
		break;
	    case 'D':
		if (strlen(av[optind]) == 16) {
			ret = symDecrypt(av[optind], stdin, stdout);
		} else {
			fprintf(stderr,
			    "ERROR: key must be exactly 16 bytes\n");
			ret = 1;
		}
		break;
	    case 'h':
		if (av[optind+1]) {
			hash = secure_hashstr(av[optind], strlen(av[optind]),
			    av[optind+1]);
		} else {
			hash = hashstr(av[optind], strlen(av[optind]));
		}
		puts(hash);
		free(hash);
		ret = 0;
		break;
	    default:
		usage();
	}
	return (ret);
}

private	int
make_keypair(int bits, char *secret, char *public)
{
	rsa_key	key;
	unsigned long	size;
	FILE	*f;
	char	out[4096];
	int	err;

	if (err = rsa_make_key(prng, wprng, bits/8, 655337, &key)) {
		fprintf(stderr, "crypto: %s\n", error_to_string(err));
		return (1);
	}
	size = sizeof(out);
	if (err = rsa_export(out, &size, PK_PRIVATE, &key)) {
		fprintf(stderr, "crypto export private key: %s\n",
		    error_to_string(err));
		return (1);
	}
	f = fopen(secret, "w");
	unless (f) {
		fprintf(stderr, "crypto: can open %s for writing\n", secret);
		return(2);
	}
	fwrite(out, 1, size, f);
	fclose(f);
	size = sizeof(out);
	if (err = rsa_export(out, &size, PK_PUBLIC, &key)) {
		fprintf(stderr, "crypto exporting public key: %s\n",
		    error_to_string(err));
		return (1);
	}
	f = fopen(public, "w");
	unless (f) {
		fprintf(stderr, "crypto: can open %s for writing\n", public);
		return(2);
	}
	fwrite(out, 1, size, f);
	fclose(f);
	rsa_free(&key);
	return (0);
}

private void
loadkey(char *file, rsa_key *key)
{
	u8	*data;
	int	err, fsize;

	data = loadfile(file, &fsize);
	unless (data) {
		fprintf(stderr, "crypto: cannot load key from %s\n", file);
		exit(1);
	}
	if (rsakey_old) {
		err = oldrsa_import(data, key);
	} else {
		err = rsa_import(data, fsize, key);
	}
	if (err) {
		fprintf(stderr, "crypto loadkey: %s\n", error_to_string(err));
		exit(1);
	}
	free(data);
}

private	int
signdata(rsa_key *key)
{
	int	hash = find_hash("md5");
	unsigned long	hashlen, x;
	int	err;
	u8	hbuf[32], out[4096];

	/* are the parameters valid? */
	assert (hash_is_valid(hash) == CRYPT_OK);

	/* hash it */
	hashlen = sizeof(hbuf);
	if (err = hash_fd(hash, 0, hbuf, &hashlen)) {
err:		fprintf(stderr, "crypto: %s\n", error_to_string(err));
		return (1);
	}

	x = sizeof(out);
	if (err = oldrsa_sign_hash(hbuf, hashlen, out, &x,
		hash_descriptor[hash].ID, key)) goto err;
	fwrite(out, 1, x, stdout);

	return (0);
}

private	int
validatedata(rsa_key *key, char *signfile)
{
	int		hash = register_hash(&md5_desc);
	u8		*sig;
	int		siglen;
	unsigned long	hashlen;
	int		stat, err;
	u8		hbuf[32];

	sig = loadfile(signfile, &siglen);
	unless (sig && (siglen > 16)) {
		fprintf(stderr, "crypto: unable to load signature\n");
		return (1);
	}
	hashlen = sizeof(hbuf);
	if (err = hash_fd(hash, 0, hbuf, &hashlen)) {
error:		fprintf(stderr, "crypto: %s\n", error_to_string(err));
		return (1);
	}
	if (err = oldrsa_verify_hash(sig, &siglen, hbuf, hashlen,
		&stat, hash_descriptor[hash].ID, key)) goto error;
	return (stat ? 0 : 1);
}

int
base64_main(int ac, char **av)
{
	int	c, err;
	int	unpack = 0;
	long	len, outlen;
	char	buf[4096], out[4096];

	while ((c = getopt(ac, av, "d", 0)) != -1) {
		switch (c) {
		    case 'd': unpack = 1; break;
		    default: bk_badArg(c, av);
		}
	}

	if (unpack) {
		setmode(fileno(stdout), _O_BINARY);
		while (fgets(buf, sizeof(buf), stdin)) {
			len = strlen(buf);
			outlen = sizeof(out);
			if (err = base64_decode(buf, len, out, &outlen)) {
err:				fprintf(stderr, "base64: %s\n",
				    error_to_string(err));
				return (1);
			}
			fwrite(out, 1, outlen, stdout);
		}
	} else {
		setmode(fileno(stdin), _O_BINARY);
		while (len = fread(buf, 1, 48, stdin)) {
			outlen = sizeof(out);
			if (err = base64_encode(buf, len, out, &outlen)) {
				goto err;
			}
			fwrite(out, 1, outlen, stdout);
			putchar('\n');
		}
	}
	return (0);
}

/* this seems to have been removed from libtomcrypt,
 * it seems to me it should really be a tomcrypt API
 */
private int
hmac_filehandle(int hash, FILE *in,
    const unsigned char *key, unsigned long keylen,
    unsigned char *out, unsigned long *outlen)
{
	hmac_state	hmac;
	unsigned char	buf[512];
	size_t		x;
	int		err;

	if (err = hash_is_valid(hash)) {
		return (err);
	}

	if (err = hmac_init(&hmac, hash, key, keylen)) {
		return (err);
	}

	while ((x = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (err = hmac_process(&hmac, buf, (unsigned long)x)) {
			return (err);
		}
	}

	if (err = hmac_done(&hmac, out, outlen)) {
		return (err);
	}
	zeromem(buf, sizeof(buf));
	return (CRYPT_OK);
}

char *
secure_hashstr(char *str, int len, char *key)
{
	int	hash = register_hash(use_sha1_hash ? &sha1_desc : &md5_desc);
	unsigned long md5len, b64len;
	char	*p;
	int	n;
	char	md5[32];
	char	b64[64];

	md5len = sizeof(md5);
	if (key && (len == 1) && streq(str, "-")) {
		if (hmac_filehandle(hash, stdin,
		    key, strlen(key), md5, &md5len)) {
			return (0);
		}
	} else if (key) {
		if (hmac_memory(hash, key, strlen(key),
			str, len, md5, &md5len)) return (0);
	} else if ((len == 1) && streq(str, "-")) {
		if (hash_fd(hash, 0, md5, &md5len)) return (0);
	} else {
		if (hash_memory(hash, str, len, md5, &md5len)) return (0);
	}
	b64len = sizeof(b64);
	if (hex_output) {
		assert(sizeof(b64) > md5len*2+1);
		for (n = 0; n < md5len; n++) {
			sprintf(b64 + 2*n,
			    "%1x%x", (md5[n] >> 4) & 0xf, md5[n] & 0xf);
		}
	} else {
		if (base64_encode(md5, md5len, b64, &b64len)) return (0);
		for (p = b64; *p; p++) {
			if (*p == '/') *p = '-';	/* dash */
			if (*p == '+') *p = '_';	/* underscore */
			if (*p == '=') {
				*p = 0;
				break;
			}
		}
	}
	return (strdup(b64));
}

char *
hashstr(char *str, int len)
{
	return (secure_hashstr(str, len, 0));
}

char *
hashstream(int fd)
{
	int	hash = register_hash(&md5_desc);
	unsigned long md5len, b64len;
	char	*p;
	char	md5[32];
	char	b64[32];

	md5len = sizeof(md5);
	if (hash_fd(hash, fd, md5, &md5len)) return (0);
	b64len = sizeof(b64);
	if (base64_encode(md5, md5len, b64, &b64len)) return (0);
	for (p = b64; *p; p++) {
		if (*p == '/') *p = '-';	/* dash */
		if (*p == '+') *p = '_';	/* underscore */
		if (*p == '=') {
			*p = 0;
			break;
		}
	}
	return (strdup(b64));
}

private int
encrypt_stream(rsa_key *key, FILE *fin, FILE *fout)
{
	int	cipher = register_cipher(&rijndael_desc);
	long	inlen, outlen, blklen;
	int	i, err;
	symmetric_CTR	ctr;
	u8	sym_IV[MAXBLOCKSIZE];
	u8	skey[32];
	u8	in[4096], out[4096];

	/* generate random session key */
	i = 32;
	cipher_descriptor[cipher].keysize(&i);
	inlen = i;

	blklen = cipher_descriptor[cipher].block_length;
	rand_getBytes(skey, inlen);
	rand_getBytes(sym_IV, blklen);

	outlen = sizeof(out);
	if (err = oldrsa_encrypt_key(skey, inlen, out, &outlen,
	    prng, wprng, key)) {
err:		fprintf(stderr, "crypto encrypt: %s\n", error_to_string(err));
		return (1);
	}
	fwrite(out, 1, outlen, fout);
	fwrite(sym_IV, 1, blklen, fout);

	/* bulk encrypt */
	if (err = ctr_start(cipher, sym_IV, skey, inlen, 0,
	    CTR_COUNTER_LITTLE_ENDIAN, &ctr)) {
		goto err;
	}

	while ((i = fread(in, 1, sizeof(in), fin)) > 0) {
		ctr_encrypt(in, out, i, &ctr);
		fwrite(out, 1, i, fout);
	}
	return (0);
}

private int
decrypt_stream(rsa_key *key, FILE *fin, FILE *fout)
{
	int	cipher = register_cipher(&rijndael_desc);
	unsigned long	inlen, outlen, blklen;
	int	i, err;
	symmetric_CTR	ctr;
	u8	sym_IV[MAXBLOCKSIZE];
	u8	skey[32];
	u8	in[4096], out[4096];

	i = inlen = fread(in, 1, sizeof(in), fin);
	outlen = sizeof(out);
	if (err = oldrsa_decrypt_key(in, &inlen, out, &outlen, key)) {
err:		fprintf(stderr, "crypto decrypt: %s\n", error_to_string(err));
		return (1);
	}
	memcpy(skey, out, outlen);

	blklen = cipher_descriptor[cipher].block_length;
	assert(inlen + blklen < i);
	memcpy(sym_IV, in + inlen, blklen);

	/* bulk encrypt */
	if (err = ctr_start(cipher, sym_IV, skey, outlen, 0,
		    CTR_COUNTER_LITTLE_ENDIAN, &ctr)) {
		goto err;
	}
	i -= inlen + blklen;
	ctr_decrypt(in + inlen + blklen, out, i, &ctr);
	fwrite(out, 1, i, fout);

	while ((i = fread(in, 1, sizeof(in), fin)) > 0) {
		ctr_decrypt(in, out, i, &ctr);
		fwrite(out, 1, i, fout);
	}
	return (0);
}

/* key contains 16 bytes of data */
private int
symEncrypt(char *key, FILE *fin, FILE *fout)
{
	int	cipher = register_cipher(&rijndael_desc);
	long	blklen;
	int	i;
	symmetric_CFB	cfb;
	u8	sym_IV[MAXBLOCKSIZE];
	u8	buf[4096];

	blklen = cipher_descriptor[cipher].block_length;
	assert(blklen == 16);  // aes
	memset(sym_IV, 0, blklen);

	cfb_start(cipher, sym_IV, key, 16, 0, &cfb);

	while ((i = fread(buf, 1, sizeof(buf), fin)) > 0) {
		cfb_encrypt(buf, buf, i, &cfb);
		fwrite(buf, 1, i, fout);
	}
	return (0);
}

private int
symDecrypt(char *key, FILE *fin, FILE *fout)
{
	int	cipher = register_cipher(&rijndael_desc);
	long	blklen;
	int	i;
	symmetric_CFB	cfb;
	u8	sym_IV[MAXBLOCKSIZE];
	u8	buf[4096];

	blklen = cipher_descriptor[cipher].block_length;
	assert(blklen == 16);  // aes
	memset(sym_IV, 0, blklen);

	cfb_start(cipher, sym_IV, key, 16, 0, &cfb);

	while ((i = fread(buf, 1, sizeof(buf), fin)) > 0) {
		cfb_decrypt(buf, buf, i, &cfb);
		fwrite(buf, 1, i, fout);
	}
	return (0);
}


/*
 * only setup the special key in the environment for restricted commands
 */
void
bk_preSpawnHook(int flags, char *av[])
{
	if (spawn_tcl || streq(av[0], "bk")) {
		/* bk calling itself */
		rand_setSeed((flags & _P_DETACH) ? 0 : 1);
	} else {
		/*
		 * calling other commands, need to clear since win32
		 * can't validate the ppid pointer.
		 */
		putenv("RANDSEED=");
	}

	/*
	 * Flush anything cached in proj structs before running
	 * another process.
	 */
	proj_flush(0);
}

/*
 * A little helper funtion to find hash conflicts for BAM tests
 * Ex: These 3 files are conflicts: 155 236 317
 */
int
findhashdup_main(int ac, char **av)
{
	hash	*h;
	int	cnt = 0, max;
	u32	i, len, a32;
	char	buf[64];

	if (!av[1] || av[2]) return (1);
	max = atoi(av[1]);
	h = hash_new(HASH_MEMHASH);

	for (i = 0; ; i++) {
		len = sprintf(buf, "%d\n", i);
		a32 = adler32(0, buf, len);

		unless (hash_insert(h, &a32, 4, buf, len)) {
			printf("dup %.*s && %.*s == %08x\n",
			    len-1, buf, h->vlen-1, (char *)h->vptr,
			    *(u32 *)h->kptr);
			if (++cnt >= max) return (0);
		}
	}
	return (0);
}

/*
 * given a filename return
 * HASH/base
 */
char *
file_fanout(char *base)
{
	int	hash = register_hash(&md5_desc);
	unsigned long md5len;
	char	md5[32];

	/* make sure it really is a basename */
	assert(!strchr(base, '/'));

	md5len = sizeof(md5);
	hash_memory(hash, base, strlen(base), md5, &md5len);

	return (aprintf("%1x%1x/%s",
		((md5[0] >> 4) & 0xf), (md5[0] & 0xf),
		base));
}
