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

#include "sccs.h"
#include <tomcrypt.h>
#include "randseed.h"

private	int	use_sha1_hash = 0;
private	int	hex_output = 0;

/*
 * -h <data> [<key>]
 *    # hash data with an optional key
 *
 * -S
 *    # use sha1 instead of md5 for -h
 * -X
 *    # print hash results in hex instead of base64 for -h
 */

int
crypto_main(int ac, char **av)
{
	int	c;
	int	mode = 0;
	int	args, optargs;
	int	ret = 1;
	char	*hash;

	while ((c = getopt(ac, av, "hSX", 0)) != -1) {
		switch (c) {
		    case 'h':
			if (mode) usage();
			mode = c;
			break;
		    case 'S': use_sha1_hash = 1; break;
		    case 'X': hex_output = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	optargs = 0;
	switch (mode) {
	    default: args = 1; break;
	    case 'h': args = 1; optargs = 0; break;
	}
	if (ac - optind < args || ac - optind > args + optargs) {
		fprintf(stderr, "ERROR: wrong number of args!\n");
		usage();
	}

	setmode(0, _O_BINARY);
	register_hash(&md5_desc);

	switch (mode) {
	    case 'h':
		hash = hashstr(av[optind], strlen(av[optind]));
		puts(hash);
		free(hash);
		ret = 0;
		break;
	    default:
		usage();
	}
	return (ret);
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

private int
hash_fd(int hash, int fd, unsigned char *out, unsigned long *outlen)
{
    hash_state md;
    unsigned char buf[8192];
    size_t x;
    int err;

    if ((err = hash_is_valid(hash)) != CRYPT_OK) {
        return err;
    }

    if (*outlen < hash_descriptor[hash].hashsize) {
       *outlen = hash_descriptor[hash].hashsize;
       return CRYPT_BUFFER_OVERFLOW;
    }
    if ((err = hash_descriptor[hash].init(&md)) != CRYPT_OK) {
       return err;
    }

    *outlen = hash_descriptor[hash].hashsize;
    while ((x = read(fd, buf, sizeof(buf))) > 0) {
        if ((err = hash_descriptor[hash].process(&md, buf, x)) != CRYPT_OK) {
           return err;
        }
    }
    err = hash_descriptor[hash].done(&md, out);

    return err;
}

char *
hashstr(char *str, int len)
{
	int	hash = register_hash(use_sha1_hash ? &sha1_desc : &md5_desc);
	unsigned long md5len, b64len;
	char	*p;
	int	n;
	char	md5[32];
	char	b64[64];

	md5len = sizeof(md5);
	if ((len == 1) && streq(str, "-")) {
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
