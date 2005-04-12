#include "system.h"
#include "sccs.h"
#include "tomcrypt/mycrypt.h"
#include "tomcrypt/randseed.h"
#include "cmd.h"

extern char *bin;

private	int	make_keypair(int bits, char *secret, char *public);
private	int	signdata(rsa_key *secret);
private	int	validatedata(rsa_key *public, char *sign);
private	int	cryptotest(void);
private	int	encrypt_stream(rsa_key *public, FILE *fin, FILE *fout);
private	int	decrypt_stream(rsa_key	*secret, FILE *fin, FILE *fout);
private void	loadkey(char *file, rsa_key *key);

private	int		wprng = -1;
private	prng_state	*prng;

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
 * -h <data> [<key>]
 *    # hash data with an optional key
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
	int	args, optargs;
	int	ret = 1;
	char	*hash;
	rsa_key	key;

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help crypto");
		return (1);
	}

	while ((c = getopt(ac, av, "dehistv")) != -1) {
		switch (c) {
		    case 'h': case 'i': case 's': case 't': case 'v':
		    case 'e': case 'd':
			if (mode) usage();
			mode = c;
			break;
		    default:
			usage();
		}
	}
	optargs = 0;
	switch (mode) {
	    case 'i': args = 3; break;
	    case 'v': args = 2; break;
	    default: args = 1; break;
	    case 't': args = 0; break;
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
	    case 't':
		ret = cryptotest();
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

	if (rsa_make_key(prng, wprng, bits/8, 655337, &key)) {
err:		fprintf(stderr, "crypto: %s\n", crypt_error);
		return (1);
	}
	size = sizeof(out);
	if (rsa_export(out, &size, PK_PRIVATE_OPTIMIZED, &key)) goto err;
	f = fopen(secret, "w");
	unless (f) {
		fprintf(stderr, "crypto: can open %s for writing\n", secret);
		return(2);
	}
	fwrite(out, 1, size, f);
	fclose(f);
	size = sizeof(out);
	if (rsa_export(out, &size, PK_PUBLIC, &key)) goto err;
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
	if (rsa_signpad(rsa_in, hashlen, rsa_out, &x, wprng, prng)) goto err;

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

private char *
publickey(int version)
{
	char	*key;
	unsigned long keylen;
	int	i, len;
	int	tmp;
	char	*coded;

	if (version <= 5) {
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~U
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~0V
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~|
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~I
~~~~~~~~~~t
	} else {
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~/
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~U
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~4
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~G
~~~~~~~~~~t
	}

	coded = strdup(coded);
	len = strlen(coded);
	for (i = 0; i < len/2; i++) {
		tmp = coded[i];
		coded[i] = coded[len-i-1];
		coded[len-i-1] = tmp;
	}
	keylen = len;
	key = malloc(len);
	base64_decode(coded, len, key, &keylen);
	free(coded);
	return (key);
}

int
check_licensesig(char *key, char *sign, int version)
{
	char	signbin[256];
	unsigned long	outlen;
	rsa_key	rsakey;
	char	*pubkey = publickey(version);
	int	ret;
	int	stat;

	register_hash(&md5_desc);

	ret = rsa_import(pubkey, &rsakey);
	free(pubkey);
	if (ret == CRYPT_ERROR) {
		fprintf(stderr, "crypto rsa_import: %s\n", crypt_error);
		exit(1);
	}

	outlen = sizeof(signbin);
	if (base64_decode(sign, strlen(sign), signbin, &outlen)) return (-1);
	if (rsa_verify(signbin, key, strlen(key), &stat, &rsakey)) return (-1);
	rsa_free(&rsakey);
	return (stat ? 0 : 1);
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
		setmode(fileno(stdout), _O_BINARY);
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
		setmode(fileno(stdin), _O_BINARY);
		while (len = fread(buf, 1, 48, stdin)) {
			outlen = sizeof(out);
			if (base64_encode(buf, len, out, &outlen)) goto err;
			fwrite(out, 1, outlen, stdout);
			putchar('\n');
		}
	}
	return (0);
}

char *
secure_hashstr(char *str, int len, char *key)
{
	int	hash = register_hash(&md5_desc);
	unsigned long md5len, b64len;
	char	*p;
	char	md5[32];
	char	b64[32];

	if ((len == 1) && streq(str, "-")) {
		if (hmac_filehandle(hash, stdin, key, strlen(key), md5)) {
			return (0);
		}
	} else {
		if (hmac_memory(hash, key, strlen(key),
			str, len, md5)) return (0);
	}
	b64len = sizeof(b64);
	md5len = hash_descriptor[hash].hashsize;
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

char *
hashstr(char *str, int len)
{
	int	hash = register_hash(&md5_desc);
	unsigned long md5len, b64len;
	char	*p;
	char	md5[32];
	char	b64[32];

	if ((len == 1) && streq(str, "-")) {
		if (hash_filehandle(hash, stdin, md5)) return (0);
	} else {
		if (hash_memory(hash, str, len, md5)) return (0);
	}
	b64len = sizeof(b64);
	md5len = hash_descriptor[hash].hashsize;
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

char *
signed_loadFile(char *filename)
{
	int	len;
	char	*p;
	char	*hash;
	char	*data = loadfile(filename, &len);

	unless (data && len > 0) return (0);
	p = data + len - 1;
	*p = 0;
	while ((p > data) && (*p != '\n')) --p;
	*p++ = 0;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~d
	unless (streq(hash, p)) {
		free(data);
		data = 0;
	}
	free(hash);
	return (data);
}

int
signed_saveFile(char *filename, char *data)
{
	FILE	*f;
	char	*tmpf;
	char	*hash;

	tmpf = aprintf("%s.%u", filename, getpid());
	unless (f = fopen(tmpf, "w")) {
		return (-1);
	}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~0U
	fprintf(f, "%s\n%s\n", data, hash);
	fclose(f);
	free(hash);
	rename(tmpf, filename);
	unlink(tmpf);
	free(tmpf);
	return (0);
}

private int
encrypt_stream(rsa_key *key, FILE *fin, FILE *fout)
{
	int	cipher = register_cipher(&rijndael_desc);
	long	inlen, outlen, blklen;
	int	i;
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
	if (rsa_encrypt_key(skey, inlen, out, &outlen, prng, wprng, key)) {
err:		fprintf(stderr, "crypto encrypt: %s\n", crypt_error);
		return (1);
	}
	fwrite(out, 1, outlen, fout);
	fwrite(sym_IV, 1, blklen, fout);

	/* bulk encrypt */
	if (ctr_start(cipher, sym_IV, skey, inlen, 0, &ctr)) {
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
	long	inlen, outlen, blklen;
	int	i;
	symmetric_CTR	ctr;
	u8	sym_IV[MAXBLOCKSIZE];
	u8	skey[32];
	u8	in[4096], out[4096];

	i = fread(in, 1, sizeof(in), fin);

	inlen = i;
	outlen = sizeof(out);
	if (rsa_decrypt_key(in, &inlen, out, &outlen, key)) {
err:		fprintf(stderr, "crypto decrypt: %s\n", crypt_error);
		return (1);
	}
	memcpy(skey, out, outlen);

	blklen = cipher_descriptor[cipher].block_length;
	assert(inlen + blklen < i);
	memcpy(sym_IV, in + inlen, blklen);

	/* bulk encrypt */
	if (ctr_start(cipher, sym_IV, skey, outlen, 0, &ctr)) {
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

private char *
upgrade_secretkey(void)
{
	char	*key;
	unsigned long keylen;
	int	i, len;
	int	tmp;
	/*
	 * generate like this:
	 *    bk edit bkupgrade.key
	 *    ./bk crypto -i 1024 bkupgrade.key.sec bkupgrade.key
	 *    ./key2code.pl bkupgrade.key.sec
	 */
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~0]
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~+
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~0S
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~$
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~t
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~J
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~o
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~0T
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~0k
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~k
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~0
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~z
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~0`
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~E
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~`
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~0O
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*
~~~~~~~~~~~~~~~~~~~~~~~~0`

	coded = strdup(coded);
	len = strlen(coded);
	for (i = 0; i < len/2; i++) {
		tmp = coded[i];
		coded[i] = coded[len-i-1];
		coded[len-i-1] = tmp;
	}
	keylen = len;
	key = malloc(keylen);
	base64_decode(coded, len, key, &keylen);
	free(coded);
	return (key);
}

int
upgrade_decrypt(char *infile, char *outfile)
{
	FILE	*fin, *fout;
	rsa_key	rsakey;
	int	ret;
	char	*seckey;

	unless (fin = fopen(infile, "r")) return (1);
	unless (fout = fopen(outfile, "w")) {
		fclose(fin);
		return (1);
	}
	seckey = upgrade_secretkey();
	ret = rsa_import(seckey, &rsakey);
	free(seckey);
	if (ret) {
		fprintf(stderr, "crypto rsa_import: %s\n", crypt_error);
		exit(1);
	}
	decrypt_stream(&rsakey, fin, fout);
	fclose(fin);
	fclose(fout);
	return (0);
}

/*
 * only setup the special key in the environment for restricted commands
 */
void
bk_preSpawnHook(int flags, char *const av[])
{
	rand_setSeed((flags & _P_DETACH) ? 0 : 1);
}
