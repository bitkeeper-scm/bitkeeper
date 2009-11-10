#include <unistd.h>
#include "tomcrypt.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#include "oldrsa.h"

/*
 * ---------------------- old tomcrypt support functions ------------------
 */

static int
oldrsa_signpad(u8 *in, int inlen, u8 *out, unsigned long *outlen)
{
	int	x, y;

	if (*outlen < (3 * inlen)) return (CRYPT_BUFFER_OVERFLOW);

	for (y = x = 0; x < inlen; x++) out[y++] = 0xFF;
	for (x = 0; x < inlen; x++) out[y++] = in[x];
	for (x = 0; x < inlen; x++) out[y++] = 0xFF;
	*outlen = 3 * inlen;

	return (CRYPT_OK);
}

static int
oldrsa_signdepad(u8 *in, int inlen, u8 *out, unsigned long *outlen)
{
	int	x;

	if (*outlen < inlen/3) return (CRYPT_BUFFER_OVERFLOW);

	/* check padding bytes */
	for (x = 0; x < inlen/3; x++) {
		if (in[x] != 0xFF || in[x+(inlen/3)+(inlen/3)] != 0xFF) {
			return (CRYPT_ERROR);
		}
	}
	for (x = 0; x < inlen/3; x++) out[x] = in[x+(inlen/3)];
	*outlen = inlen/3;
	return (CRYPT_OK);
}

static int
oldrsa_pad(u8 *in, int inlen, u8 *out, unsigned long *outlen,
    int wprng, prng_state *prng)
{
	int	x;
	u8	buf[2048];

	/* is output big enough? */
	assert(*outlen >= (3 * inlen));

	/* get random padding required */
	assert(prng_is_valid(wprng) == CRYPT_OK);

	/* check inlen */
	assert(inlen <= 512);

	if (prng_descriptor[wprng].read(buf, inlen*2-2, prng) != (inlen*2 -2)) {
		return (CRYPT_ERROR);
	}

	/* pad it like a sandwitch (sp?)
	 *
	 * Looks like 0xFF R1 M R2 0xFF
	 *
	 * Where R1/R2 are random and exactly equal to the length of M
	 * minus one byte.
	 */
	out[0] = 0xFF;
	for (x = 0; x < inlen-1; x++) out[x+1] = buf[x];
	for (x = 0; x < inlen; x++)   out[x+inlen] = in[x];
	for (x = 0; x < inlen-1; x++) out[x+inlen+inlen] = buf[x+inlen-1];
	out[inlen+inlen+inlen-1] = 0xFF;

	/* clear up and return */
	zeromem(buf, sizeof(buf));
	*outlen = inlen*3;
	return (CRYPT_OK);
}

static int
oldrsa_depad(u8 *in, int inlen, u8 *out, unsigned long *outlen)
{
	int	x;

	assert(*outlen >= inlen/3);

	for (x = 0; x < inlen/3; x++) out[x] = in[x+(inlen/3)];
	*outlen = inlen/3;
	return (CRYPT_OK);
}


/* these are smaller routines written by Clay Culver.  They do the
 * same function as the rsa_encrypt/decrypt except that they are used
 * to RSA encrypt/decrypt a single value and not a packet.
 */
int
oldrsa_encrypt_key(u8 *inkey, int inlen, u8 *outkey, unsigned long *outlen,
    prng_state *prng, int wprng, rsa_key *key)
{
	unsigned long	x, y, rsa_size, err;
	u8		rsa_in[4096], rsa_out[4096];

	/* rsa_pad the symmetric key */
	y = sizeof(rsa_in);
	if (err = oldrsa_pad(inkey, inlen, rsa_in, &y, wprng, prng)) {
		return (err);
	}

	/* rsa encrypt it */
	rsa_size = sizeof(rsa_out);
	if (err = rsa_exptmod(rsa_in, y, rsa_out, &rsa_size, PK_PUBLIC, key)) {
		return (err);
	}

	/* check size */
	if (*outlen < (7+rsa_size)) return (CRYPT_BUFFER_OVERFLOW);

	/* now lets make the header */
	y = 8;		/* PACKET_SIZE */

	/* store the size of the RSA value */
	STORE32L(rsa_size, (outkey+y));
	y += 4;

	/* store the rsa value */
	for (x = 0; x < rsa_size; x++, y++) outkey[y] = rsa_out[x];

	/* store header */
	outkey[0] = 0x62;
	outkey[1] = 0;
	outkey[2] = 0;		/* PACKET_SECT_RSA */
	outkey[3] = 3;		/* PACKET_SUB_ENC_KEY */
	STORE32L(y, outkey+4);

	/* clean up */
	zeromem(rsa_in, sizeof(rsa_in));
	zeromem(rsa_out, sizeof(rsa_out));
	*outlen = y;
	return (CRYPT_OK);
}

int
oldrsa_decrypt_key(u8 *in, unsigned long *len,
		   u8 *outkey, unsigned long *keylen,
		   rsa_key *key)
{
	unsigned long	x, y, z, i, rsa_size, err;
	u8		sym_key[MAXBLOCKSIZE], rsa_in[4096], rsa_out[4096];

	/* right key type? */
	assert(key->type == PK_PRIVATE);

	/* check the header */
	//if (packet_valid_header((unsigned char *)in, PACKET_SECT_RSA, PACKET_SUB_ENC_KEY) == CRYPT_ERROR) {

	/* grab length of the rsa key */
	y = 8;
	LOAD32L(rsa_size, (in+y));
	y += 4;
	if (y + rsa_size > *len) return (CRYPT_BUFFER_OVERFLOW);
	*len = y + rsa_size;

	/* read it in */
	for (x = 0; x < rsa_size; x++, y++) rsa_in[x] = in[y];

	/* decrypt it */
	x = sizeof(rsa_out);
	err = rsa_exptmod(rsa_in, rsa_size, rsa_out, &x, PK_PRIVATE, key);
	if (err) return (err);

	/* depad it  96==magic */
	z = sizeof(sym_key);
	if (err = oldrsa_depad(rsa_out+x-96, 96, sym_key, &z)) return (err);

	/* check size */
	if (*keylen < z) return (CRYPT_BUFFER_OVERFLOW);

	for (i = 0; i < z; i++) outkey[i] = sym_key[i];

	/* clean up */
	zeromem(sym_key, sizeof(sym_key));
	zeromem(rsa_in, sizeof(rsa_in));
	zeromem(rsa_out, sizeof(rsa_out));
	*keylen = z;
	return (CRYPT_OK);
}

int
oldrsa_sign_hash(u8 *hash, int hashlen, u8 *out, unsigned long *outlen,
    int hashid, rsa_key *key)
{
	unsigned long	rsa_size, x, y;
	int		err;
	u8		rsa_in[4096], rsa_out[4096];

	/* type of key? */
	if (key->type != PK_PRIVATE) return (CRYPT_PK_NOT_PRIVATE);

	/* pad it */
	x = sizeof(rsa_in);
	if (err = oldrsa_signpad(hash, hashlen, rsa_out, &x)) return (err);

	/* sign it */
	rsa_size = sizeof(rsa_in);
	if (err = rsa_exptmod(rsa_out, x, rsa_in,&rsa_size, PK_PRIVATE, key)) {
		return (err);
	}

	/* check size */
	if (*outlen < (8+1+4+rsa_size)) return (CRYPT_BUFFER_OVERFLOW);

	y = 8;

	out[y++] = hashid;

	/* store the length */
	STORE32L(rsa_size, (out+y));
	y += 4;

	/* store the signature */
	for (x = 0; x < rsa_size; x++, y++) {
		out[y] = rsa_in[x];
	}

	/* store header */
	out[0] = 0x62;
	out[1] = 0;
	out[2] = 0;		/* PACKET_SEC_RSA */
	out[3] = 2;		/* PACKET_SUB_SIGNED */
	STORE32L(y, (out+4));

	/* clean up */
	zeromem(rsa_in, sizeof(rsa_in));
	zeromem(rsa_out, sizeof(rsa_out));
	*outlen = y;
	return (CRYPT_OK);
}

int
oldrsa_verify_hash(u8 *sig, int *siglen, u8 *hash, int hashlen, int *stat,
    int hashid, rsa_key *key)
{
	int		err;
	unsigned long	rsa_size, x, y, z;
	u8		rsa_in[4096], rsa_out[4096];

	/* always be incorrect by default */
	*stat = 0;

	y = 8;			/* skip header */

	/* verify hashid */
	if (sig[y++] != hashid) return (CRYPT_INVALID_PACKET);

	/* get the len */
	LOAD32L(rsa_size, (sig+y));
	y += 4;

	/* load the signature */
	for (x = 0; x < rsa_size; x++, y++) rsa_in[x] = sig[y];

	/* exptmod it */
	x = sizeof(rsa_out);
	if (err = rsa_exptmod(rsa_in, rsa_size, rsa_out, &x, PK_PUBLIC, key)) {
		return (err);
	}
	z = sizeof(rsa_in);
	err = oldrsa_signdepad(rsa_out+x-(3*hashlen), 3*hashlen, rsa_in, &z);
	if (err) return (err);

	/* check? */
	if ((z == hashlen) && (!memcmp(rsa_in, hash, hashlen))) {
		*stat = 1;
	}
	*siglen = y;

	zeromem(rsa_in, sizeof(rsa_in));
	zeromem(rsa_out, sizeof(rsa_out));

	return (CRYPT_OK);
}

static u8 *
input_bignum(void *num, u8 *in)
{
	unsigned long	x;

	/* load value */
	LOAD32L(x, in);
	in += 4;

	/* sanity check... */
	if (x > 1024) return (0);

	/* load it */
	if (mp_read_unsigned_bin(num, in, x)) return (0);
	return (in+x);
}

int
oldrsa_import(u8 *in, rsa_key *key)
{
	int	err, type;
	void	*junk;

	/* init key */
	if (err = mp_init_multi(&key->e, &key->d, &key->N, &key->dQ,
		&key->dP, &key->qP, &key->p, &key->q, NULL)) {
		return err;
	}

	in += 8;		/* skip header */

	/* get key type */
	type = *in++;

	switch (type) {
	    case 2:
	    case 0: key->type = PK_PRIVATE; break;
	    case 1: key->type = PK_PUBLIC; break;
	    default: assert(0);
	}

	/* load the modulus  */
	if (!(in = input_bignum(key->N, in))) goto error2;

	/* load public exponent */
	if (!(in = input_bignum(key->e, in))) goto error2;

	if (key->type == PK_PRIVATE) {
		if (!(in = input_bignum(key->d, in))) goto error2;
	}

	if (type == 2) {	/* PRIVATE_OPTIMZED */
		if (!(in = input_bignum(key->dQ, in))) goto error2;
		if (!(in = input_bignum(key->dP, in))) goto error2;
		mp_init(&junk);
		if (!(in = input_bignum(junk, in))) goto error2;/* pQ */
		if (!(in = input_bignum(junk, in))) goto error2;/* qP */
		mp_clear(junk);
		if (!(in = input_bignum(key->p, in))) goto error2;
		if (!(in = input_bignum(key->q, in))) goto error2;
		mp_invmod(key->q, key->p, key->qP);
	}
	return (CRYPT_OK);
error2:
	mp_clear_multi(&key->e, &key->d, &key->N, &key->dQ,
		&key->dP, &key->qP, &key->p, &key->q, NULL);
	return CRYPT_ERROR;
}

