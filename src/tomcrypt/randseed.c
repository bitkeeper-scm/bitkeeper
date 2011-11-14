#include <unistd.h>
#include "tomcrypt.h"

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

#include "randseed.h"

static	void	rand_setupPrng(u8 *seed, int len);
static	void	mangle(u8 *rand, unsigned int rlen, u8 *buf, int len);

static	int		wprng = -1;
static	prng_state	prng;

#define	SEEDLEN	16

/*
 * Put a 16 bytes random seed in the environment so that subprocesses
 * can have true entropy in their random number generators without
 * having to read /dev/random (slow).
 *
 * Also the current time and pid is added to the data and the whole
 * mess is encrypted.  This allows a subprocess to identify if it was
 * really called from bitkeeper.
 */
void
rand_setSeed(int setpid)
{
	u32     now = (u32)time(0);
	u32	pid = setpid ? (u32)getpid() : 0;
	long	outlen;
	u8	buf[8 + SEEDLEN];
	static u8 out[64]; /* static required for std putenv */
	int	err;

	assert((sizeof(now) + sizeof(pid)) == 8);
	memcpy(buf, &now, 4);
	memcpy(buf + 4, &pid, 4);
	rand_getBytes(buf + 8, SEEDLEN);

	mangle(buf + 8, SEEDLEN, buf, 8);

	strcpy(out, "RANDSEED=");
	outlen = sizeof(out) - strlen(out);
	if ((err = base64_encode(buf, sizeof(buf), out + strlen(out), &outlen))
	    != CRYPT_OK) {
		fprintf(stderr, "rand_setSeed: %s\n", error_to_string(err));
		exit(1);
	}
	putenv(out);
}

/*
 * Look for RANDSEED in the environment and validate that it was
 * written by the rand_setSeed() function from my parent process.  If
 * so return 0 and use the random bytes and entropy for the pseudo
 * random number generator. (If it hasn't been initialized already.)
 */
int
rand_checkSeed(void)
{
	u8	*p;
	int	i;
	long	outlen;
	u32	now;
	u32	pid;
	u8	buf[8 + SEEDLEN + 1];

	assert((sizeof(now) + sizeof(pid)) == 8);
	if (!(p = getenv("RANDSEED"))) return (-1);
	outlen = sizeof(buf);
	if (base64_decode(p, strlen(p), buf, &outlen) != CRYPT_OK) return (-2);
	if (outlen != 8 + SEEDLEN) return (-3);
	mangle(buf + 8, SEEDLEN, buf, 8);
	memcpy(&now, buf, 4);
	memcpy(&pid, buf + 4, 4);

#ifndef WIN32
	if (pid && ((u32)getppid() != pid)) return (-4);
#endif

	/*
	 * Must be checked within 30 seconds of when rand_setSeed()
	 * was run.
	 */
	i = time(0) - now;
	if ((i < -5) || (i > 30)) return (-5);

	rand_setupPrng(buf + 8, SEEDLEN);
	return (0);
}

/*
 * Setup the prng (if needed) and return the tomcrypt pointers to it.
 * This is used by routines that want to call other tomcrypt routines
 * and need a good random source.
 */
int
rand_getPrng(prng_state **p)
{
	rand_setupPrng(0, 0);

	*p = &prng;
	return (wprng);
}

/*
 * Fill 'buf' with 'len' random bytes.
 * This random number generator is fast and strong.
 */
void
rand_getBytes(u8 *buf, unsigned int len)
{
	rand_setupPrng(0, 0);

	memset(buf, 0, len);	/* avoid valgrind warnings */
	if (prng_descriptor[wprng].read(buf, len, &prng) != len) {
		fprintf(stderr, "randBytes: failed to read %d bytes\n", len);
		exit(1);
	}
}

/*
 * Setup a Pseudo random number generator and initialize it a random
 * seed.  If seed is null, then 16 bytes of data are read from
 * /dev/urandom (on unix) or MS CSP (on Windows) This is only done
 * once per process tree.
 *
 * This generator uses yarrow which is a CTR encryption of a random
 * seed using AES.
 *
 * We should switch to Fortuna at some point because Yarrow has been shown
 * to have some vulnerabilities and is no longer recomended for long term
 * use --ob
 */
static void
rand_setupPrng(u8 *seed, int len)
{
	int	ret;
	u8	buf[16];

	if (wprng != -1) return;

	wprng = register_prng(&yarrow_desc);
	assert(prng_is_valid(wprng) == CRYPT_OK);

	ret = prng_descriptor[wprng].start(&prng);
	assert(ret == CRYPT_OK);

	if (!seed) {
		seed = buf;
		len = rng_get_bytes(seed, sizeof(buf), 0);
		assert(len == sizeof(buf));
	}
	ret = prng_descriptor[wprng].add_entropy(seed, len, &prng);
	assert(ret == CRYPT_OK);

	ret = prng_descriptor[wprng].ready(&prng);
	assert(ret == CRYPT_OK);

	zeromem(buf, sizeof(buf));
}

static void
mangle(u8 *rand, unsigned int rlen, u8 *buf, int len)
{
	int	cipher = register_cipher(&rijndael_desc);
	unsigned int	i;
	int	err;
	symmetric_CTR	ctr;
	u8	sym_IV[MAXBLOCKSIZE];
	u8	skey[32];
	const u8 key[] = {
		0x93, 0xa3, 0xab, 0x0a, 0x8c, 0x88, 0xfb, 0x2d,
		0x50, 0x42, 0x15, 0xe5, 0x38, 0x9f, 0x4d, 0x02
	};

	zeromem(sym_IV, sizeof(sym_IV));
	assert(rlen + sizeof(key) == sizeof(skey));
	for (i = 0; i < rlen; i++) skey[i] = rand[i];
	for (; i < sizeof(skey); i++) skey[i] = key[i - rlen];

	if ((err = ctr_start(cipher, sym_IV, skey, sizeof(skey),
	    0, CTR_COUNTER_LITTLE_ENDIAN, &ctr)) != CRYPT_OK) {
		fprintf(stderr, "crypto mangle: %s\n", error_to_string(err));
		exit(1);
	}
	ctr_encrypt(buf, buf, len, &ctr);
}
