#include "system.h"
#include "sccs.h"
#include "tomcrypt/mycrypt.h"

extern char *bin;

private	int	make_keypair(int bits, char *secret, char *public);
private	int	signdata(rsa_key *secret);
private	int	validatedata(rsa_key *public, char *sign);
private	int	cryptotest(void);

private void	loadkey(char *file, rsa_key *key);

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
 * -t
 *    # run internal test vectors on the library
 */

private void
usage(void)
{
	system("bk help -s crypto");
	exit(1);
}

int
crypto_main(int ac, char **av)
{
	int	c;
	int	mode = 0;
	int	args;
	int	ret = 1;
	rsa_key	key;

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help crypto");
		return (1);
	}

	while ((c = getopt(ac, av, "istv")) != -1) {
		switch (c) {
		    case 'i': case 's': case 't': case 'v':
			if (mode) usage();
			mode = c;
			break;
		    default:
			usage();
		}
	}
	switch (mode) {
	    case 'i': args = 3; break;
	    case 'v': args = 2; break;
	    default: args = 1; break;
	    case 't': args = 0; break;
	}
	if (ac - optind != args) {
		fprintf(stderr, "ERROR: wrong number of args!\n");
		usage();
	}

	register_cipher(&rijndael_desc);
	register_hash(&md5_desc);
	register_prng(&yarrow_desc);
	register_prng(&sprng_desc);

	switch (mode) {
	    case 'i':
		ret = make_keypair(atoi(av[optind]),
		    av[optind+1], av[optind+2]);
		break;
	    case 's':
		loadkey(av[optind], &key);
		ret = signdata(&key);
		rsa_free(&key);
		break;
	    case 'v':
		loadkey(av[optind], &key);
		ret = validatedata(&key, av[optind+1]);
		rsa_free(&key);
		break;
	    case 't':
		ret = cryptotest();
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
	prng_state	prng;
	char	out[4096];

	if (rng_make_prng(128, find_prng("yarrow"), &prng, 0)) {
 err:		fprintf(stderr, "crypto: %s\n", crypt_error);
		return (1);
	}
	if (rsa_make_key(&prng, find_prng("yarrow"), bits/8, 655337, &key)) {
		goto err;
	}
	size = sizeof(out);
	if (rsa_export(out, &size, PK_PRIVATE_OPTIMIZED, &key)) goto err;
	f = fopen(secret, "wb");
	unless (f) {
		fprintf(stderr, "crypto: can open %s for writing\n", secret);
		return(2);
	}
	fwrite(out, 1, size, f);
	fclose(f);
	size = sizeof(out);
	if (rsa_export(out, &size, PK_PUBLIC, &key)) goto err;
	f = fopen(public, "wb");
	unless (f) {
		fprintf(stderr, "crypto: can open %s for writing\n", public);
		return(2);
	}
	fwrite(out, 1, size, f);
	fclose(f);
	rsa_free(&key);
	return (0);
}

private char *
loadfile(char *file, int *size)
{
	FILE	*f;
	struct	stat	statbuf;
	char	*ret;
	int	len;

	f = fopen(file, "rb");
	unless (f) return (0);

	if (fstat(fileno(f), &statbuf)) {
 err:		fclose(f);
		return (0);
	}
	len = statbuf.st_size;
	ret = malloc(len);
	unless (ret) goto err;
	fread(ret, 1, len, f);
	fclose(f);

	if (size) *size = len;
	return (ret);
}

private void
loadkey(char *file, rsa_key *key)
{
	char	*data;

	data = loadfile(file, 0);
	unless (data) {
		fprintf(stderr, "crypto: cannot load key from %s\n", file);
		exit(1);
	}
	if (rsa_import(data, key) == CRYPT_ERROR) {
		fprintf(stderr, "crypto loadkey: %s\n", crypt_error);
		exit(1);
	}
}

private int
hash_filehandle(int hash, FILE *in, unsigned char *dst)
{
	hash_state	md;
	unsigned char	buf[512];
	int		x;

	if(hash_is_valid(hash) == CRYPT_ERROR) {
		return (CRYPT_ERROR);
	}

	unless (in) {
		crypt_error = "Error reading file in hash_filehandle().";
		return CRYPT_ERROR;
	}
	hash_descriptor[hash].init(&md);
	do {
		x = fread(buf, 1, sizeof(buf), in);
		hash_descriptor[hash].process(&md, buf, x);
	} while (x == sizeof(buf));
	hash_descriptor[hash].done(&md, dst);
#ifdef CLEAN_STACK
	zeromem(buf, sizeof(buf));
#endif
	return CRYPT_OK;
}

private	int
signdata(rsa_key *key)
{
	int	wprng = find_prng("sprng");
	int	hash = find_hash("md5");
	unsigned long	hashlen, rsa_size, x, y;
	unsigned char	rsa_in[4096], rsa_out[4096];
	unsigned char	out[4096];

	/* type of key? */
	if (key->type != PK_PRIVATE && key->type != PK_PRIVATE_OPTIMIZED) {
		fprintf(stderr, "crypto: Cannot sign with public key\n");
		return (1);
	}

	/* are the parameters valid? */
	assert (!prng_is_valid(wprng) && !hash_is_valid(hash));

	/* hash it */
	hashlen = hash_descriptor[hash].hashsize;
	if (hash_filehandle(hash, stdin, rsa_in)) {
 err:		fprintf(stderr, "crypto: %s\n", crypt_error);
		return (1);
	}

	/* pad it */
	x = sizeof(rsa_in);
	if (rsa_signpad(rsa_in, hashlen, rsa_out, &x, wprng, 0)) goto err;

	/* sign it */
	rsa_size = sizeof(rsa_in);
	if (rsa_exptmod(rsa_out, x, rsa_in, &rsa_size, PK_PRIVATE, key)) {
		goto err;
	}

	/* check size */
	if (sizeof(out) < (8+rsa_size)) {
		crypt_error = "Buffer Overrun in rsa_sign().";
		goto err;
	}

	/* now lets output the message */
	y = PACKET_SIZE;
	out[y++] = hash_descriptor[hash].ID;

	/* output the len */
	STORE32L(rsa_size, (out+y));
	y += 4;

	/* store the signature */
	for (x = 0; x < rsa_size; x++, y++) {
		out[y] = rsa_in[x];
	}

	/* store header */
	packet_store_header(out, PACKET_SECT_RSA, PACKET_SUB_SIGNED, y);

	fwrite(out, 1, y, stdout);

	/* clean up */
	zeromem(rsa_in, sizeof(rsa_in));
	zeromem(rsa_out, sizeof(rsa_out));
	zeromem(out, sizeof(out));

	return (0);
}

private	int
validatedata(rsa_key *key, char *signfile)
{
	unsigned char	*sig;
	unsigned long	hashlen, rsa_size, x, y, z;
	int		hash;
	int		ret;
	unsigned char	rsa_in[4096], rsa_out[4096];

	sig = loadfile(signfile, 0);
	unless (sig) {
		crypt_error = "Cannot load signature";
 err:	        fprintf(stderr, "crypto: %s\n", crypt_error);
		exit(1);
	}

	/* verify header */
	if (packet_valid_header(sig, PACKET_SECT_RSA, PACKET_SUB_SIGNED)) {
		crypt_error = "Invalid header for input in rsa_verify().";
		goto err;
	}

	/* grab cipher name */
	y = PACKET_SIZE;
	hash = find_hash_id(sig[y++]);
	if (hash == -1) {
		crypt_error = "Invalid hash ID for rsa_verify().";
		goto err;
	}
	hashlen = hash_descriptor[hash].hashsize;

	/* get the len */
	LOAD32L(rsa_size, (sig+y));
	y += 4;

	/* load the signature */
	for (x = 0; x < rsa_size; x++, y++) {
		rsa_in[x] = sig[y];
	}

	/* exptmod it */
	x = sizeof(rsa_in);
	if (rsa_exptmod(rsa_in, rsa_size, rsa_out, &x, PK_PUBLIC, key)) {
		crypt_error = "rsa_exptmod() failed";
		goto err;
	}

	/* depad it */
	z = sizeof(rsa_in);
	if (rsa_signdepad(rsa_out, x, rsa_in, &z)) {
		crypt_error = "rsa_signdepad() failed";
		goto err;
	}

	/* check? */
	hash_filehandle(hash, stdin, rsa_out);
	if ((z == hashlen) && (!memcmp(rsa_in, rsa_out, hashlen))) {
		ret = 0;
	} else {
		ret = 1;
	}

	zeromem(rsa_in, sizeof(rsa_in));
	zeromem(rsa_out, sizeof(rsa_out));

	return (ret);
}

int
check_licensesig(char *key, char *sign)
{
	char	signbin[256];
	unsigned long	outlen;
	rsa_key	rsakey;
	char	*pubkey;
	int	stat;

	register_hash(&md5_desc);

	pubkey = aprintf("%s/bklicense.pub", bin);
	loadkey(pubkey, &rsakey);

	outlen = sizeof(signbin);
	if (base64_decode(sign, strlen(sign), signbin, &outlen)) {
 err:		fprintf(stderr, "licsign: %s\n", crypt_error);
		return (-1);
	}

	if (rsa_verify(signbin, key, strlen(key), &stat, &rsakey)) goto err;
	return (stat ? 0 : -1);
}

private	int
cryptotest(void)
{
	extern void	store_tests(void);
	extern void	cipher_tests(void);
	extern void	hash_tests(void);
	extern void	ctr_tests(void);
	extern void	rng_tests(void);
	extern void	test_prime(void);
	extern void	rsa_test(void);
	extern void	pad_test(void);
	extern void	base64_test(void);
	extern void	time_hash(void);

	store_tests();
	cipher_tests();
	hash_tests();

	ctr_tests();

	rng_tests();
	test_prime();
	rsa_test();
	pad_test();

	base64_test();

	time_hash();

	return 0;
}

int
base64_main(int ac, char **av)
{
	int	c;
	int	unpack = 0;
	long	len, outlen;
	char	buf[4096], out[4096];

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help base64");
		return (1);
	}

	while ((c = getopt(ac, av, "d")) != -1) {
		switch (c) {
		    case 'd': unpack = 1; break;
		    default:
			system("bk help -s base64");
			exit(1);
		}
	}

	if (unpack) {
		while (fgets(buf, sizeof(buf), stdin)) {
			len = strlen(buf);
			outlen = sizeof(out);
			if (base64_decode(buf, len, out, &outlen)) {
 err:				fprintf(stderr, "base64: %s\n", crypt_error);
				return (1);
			}
			fwrite(out, 1, outlen, stdout);
		}
	} else {
		while (len = fread(buf, 1, 48, stdin)) {
			outlen = sizeof(out);
			if (base64_encode(buf, len, out, &outlen)) goto err;
			fwrite(out, 1, outlen, stdout);
			putchar('\n');
		}
	}
	return (0);
}
