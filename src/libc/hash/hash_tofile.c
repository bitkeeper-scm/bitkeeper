/*
 * Copyright 2008-2012,2016 BitMover, Inc
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
 * Routines to save and restore a hash to and from a file. The format
 * used in the file is the same as in the bugdb.
 */

private	int binaryField(u8 *data, int len);
private	int goodkey(u8 *key, int len);
private	void writeField(FILE *f, char *key, u8 *data, int len);

/* These are from tomcrypt... */
extern int base64_decode(const unsigned char *in,  unsigned long inlen,
    unsigned char *out, unsigned long *outlen);
extern int base64_encode(const unsigned char *in,  unsigned long inlen,
    unsigned char *out, unsigned long *outlen);

/*
 * write the hash files to a file named by path.
 * returns -1 on error, or 0
 */
int
hash_toFile(hash *h, char *path)
{
	FILE	*f;
	int	rc = -1;

	if (f = fopen(path, "w")) {
		rc = hash_toStream(h, f);
		fclose(f);
	}
	return (rc);
}

/*
 * write the hash files to a FILE*
 * returns -1 on error, or 0
 */
int
hash_toStream(hash *h, FILE *f)
{
	char	**fieldlist = 0;
	int	i, rc = -1;
	u8	*data;

	assert(h && f);

	/* The empty key goes first without a header or encoding */
	if (hash_fetchStr(h, "")) {
		i = h->vlen;
		if ((i > 0) &&
		    (((u8 *)h->vptr)[i-1] == 0)) {
			--i; /* no trailing null */
		}
		fwrite(h->vptr, 1, i, f);
		fputc('\n', f);
	}

	/*
	 * Sort the fields and print them
	 */
	EACH_HASH(h) {
		if ((h->klen == 1) && (*(u8*)h->kptr == 0)) continue;
		unless (goodkey(h->kptr, h->klen)) goto out;
		fieldlist = addLine(fieldlist, h->kptr);
	}
	sortLines(fieldlist, 0);
	EACH(fieldlist) {
		data = hash_fetchStr(h, fieldlist[i]);
		assert(data);
		writeField(f, fieldlist[i], data, h->vlen);
	}
	rc = 0;
out:	freeLines(fieldlist, 0);
	return (rc);
}

/*
 * Read a file written by the function above and add keys to 'h'
 * overwriting any existing keys.
 * If h==0 and path doesn't exist or is empty, then 0 is returned.
 */
hash *
hash_fromFile(hash *h, char *path)
{
	FILE	*f;

	if (f = fopen(path, "r")) {
		h = hash_fromStream(h, f);
		fclose(f);
	}
	return (h);
}

/*
 * Read a stream written by the function above and add keys to 'h'
 * overwriting any existing keys.
 * The function returns at EOF or when a line with just "@\n" is
 * encountered.  (record separator to put multiple KV's in a file)
 * If h==0, then a new hash is created if any data is found.
 */
hash *
hash_fromStream(hash *h, FILE *f)
{
	char	*line;
	hashpl	state = {0};

	assert(f);
	while (line = fgetline(f)) {
		unless (h) h = hash_new(HASH_MEMHASH);
		if (hash_parseLine(line, h, &state) == 1) break;
	}
	if (h) hash_parseLine(0, h, &state);
	return (h);
}

/*
 * Read a line of data from a DB file and write it to the hash
 * Data gets buffered up locally to so a key is only written after
 * all of the data is seen.
 *
 * Special forms:
 *   hash_parseLine(0, db, data)    At EOF so finish the last key
 *   hash_parseLine(0, 0, data)	    Just free 'data'
 *
 * Return:
 *	1   "@\n" record separator seen
 *     -1   error
 *	0   good line
 */
int
hash_parseLine(char *line, hash *h, hashpl *s)
{
	unless (s->val) s->val = fmem();
	if (!line || ((line[0] == '@') && (line[1] != '@'))) {
		char	*key = s->key;
		u8	*data;
		size_t	len;

		data = fmem_peek(s->val, &len);
		if (key || len) {
			unless (key) key = "";
			if (!s->base64 && len && (data[len-1] == '\n')) --len;
			data[len++] = 0;	/* always add trailing null */
			/* overwrite existing */
			if (h) hash_store(h, key, strlen(key)+1, data, len);
			ftrunc(s->val, 0);
			FREE(s->key);
		}
		unless (line) {    /* at EOF */
			fclose(s->val);
			s->val = 0;
			s->base64 = 0;
			return (0);
		}
		unless (line[1]) return (1); /* @ == end of record */
		len = strlen(line);
		s->base64 = (len > 8) && ends_with(line, " base64");
		if (s->base64) line[len-7] = 0;
		s->key = hash_keydecode(line+1);
		if (s->base64) line[len-7] = ' ';
	} else {
		if (*line == '@') ++line; /* skip escaped @ */
		if (s->base64) {
			long	len;
			char	data[256];

			len = sizeof(data);
			base64_decode(line, strlen(line), data, &len);
			if (len) fwrite(data, 1, len, s->val);
		} else {
			/* compat: ignore null key null val */
			if (*line || s->key) {
				fputs(line, s->val);
				fputc('\n', s->val);
			}
		}
	}
	return (0);
}


private int
goodkey(u8 *key, int len)
{
	if ((len <= 0) || (key[len-1] != 0)) {
		fprintf(stderr, "not C string\n");
		return (0);
	}
	return (1);
}

/*
 * Search for fields that need to be encoded when doing a dbimplode.
 * Currently check for:
 *    Contains non-printable characters
 *    Has a line longer that 256 characters
 */
private int
binaryField(u8 *data, int len)
{
	int	i;
	int	lastret = 0;

	for (i = 0; i < len; i++) {
		int	c = data[i];
		unless (isprint(c) || isspace(c)) return (1);
		if (c == '\n') {
			if ((i - lastret) > 256) return (1);
			lastret = i;
		}
	}
	if ((i - lastret) > 256) return (1);
	return (0);
}

/*
 * A simplified form of webencode() that only encodes the minimal number
 * of characters needed to be correctly recreated with webdecode() and
 * not have a space character so we can search for " base64" at the end of
 * a key.
 * The main point to to make files with long bk keys as hash keys easy
 * to read.
 */
void
hash_keyencode(FILE *out, u8 *ptr)
{
	while (*ptr) {
		switch(*ptr) {
		    case ' ':
			/*
			 * It would be nice to only encode a space if
			 * the key happens to end in " base64", but
			 * the reading code in legacy clients isn't
			 * careful in parsing so we have to encode all
			 * spaces.
			 */
			putc('+', out);
			break;
		    case '%': case '+': case '&': case '=':
		    case '\n':
			fprintf(out, "%%%02x", *ptr);
			break;
		    default:
			putc(*ptr, out);
			break;
		}
		++ptr;
	}
}

char *
hash_keydecode(char *key)
{
	char	*out = 0;

	webdecode(key, &out, 0);
	return (out);
}

private void
writeField(FILE *f, char *key, u8 *data, int len)
{
	unsigned long	inlen, outlen;
	u8	*p;
	char	out[128];

	fputc('@', f);
	/* strip trailing null, all fields should have one */
	if ((len > 0) && (data[len-1] == 0)) --len;
	hash_keyencode(f, key);
	if (binaryField(data, len)) {
		fputs(" base64\n", f);
		while (len) {
			inlen = min(48, len);
			outlen = sizeof(out);
			if (base64_encode(data, inlen, out, &outlen)) {
				fprintf(stderr, "writeField: base64 err\n");
				exit(1);
			}
			fwrite(out, 1, outlen, f);
			fputc('\n', f);
			data += inlen;
			len -= inlen;
		}
	} else if (len) {
		/* data is normal \0 terminated C string */
		fputc('\n', f);
		while (*data) {
			if (*data == '@') fputc('@', f);
			p = data;
			while (*p && (*p++ != '\n'));
			fwrite(data, p - data, 1, f);
			data = p;
		}
	}
	fputc('\n', f);
}

extern	int	hashfile_test_main(int ac, char **av);

int
hashfile_test_main(int ac, char **av)
{
	int	i, c;
	hash	*h = 0;
	char	**keys = 0;
	char	*file;
	char	*key, *val;
	int	mode = 0;
	int	hex = 0;
	FILE	*f;

	while ((c = getopt(ac, av, "nrwX", 0)) != -1) {
		switch (c) {
		    case 'n': mode = c; break;
		    case 'r': mode = c; break;
		    case 'w': mode = c; break;
		    case 'X': hex = 1; break;
		    default: goto usage;
		}
	}
	file = av[optind];
	unless (file && mode) {
usage:		fprintf(stderr, "usage: bk _hashfile_test [-nrwX] file\n");
		return (1);
	}
	switch (mode) {
	    case 'r':
		// read all hashes from av[2], dump to stdout
		unless (f = fopen(file, "r")) return (1);
		while (!feof(f)) {
			unless (h = hash_fromStream(0, f)) goto next;
			EACH_HASH(h) keys = addLine(keys, h->kptr);
			sortLines(keys, 0);
			EACH(keys) {
				printf("%s =>", keys[i]);
				val = hash_fetchStr(h, keys[i]);
				if (hex) {
					for (c = 0; c < h->vlen; c++) {
						printf(" %02x", val[c]);
					}
				} else {
					printf(" %s", val);
				}
				printf("\n");
			}
			freeLines(keys, 0); keys = 0;
			hash_free(h); h = 0;
next:			unless (feof(f)) printf("---\n");
		}
		fclose(f);
		break;
	    case 'w':
		// read hash from av[2], write back out to stdout
		h = hash_fromFile(0, file);
		hash_toStream(h, stdout);
		break;
	    case 'n':
		// create a hash with a non-null-terminated key, write to av[2]
		key = "nonterm";
		h = hash_new(HASH_MEMHASH);
		hash_insert(h, key, 7, "value", 6);
		hash_toFile(h, file);
		break;
	    default: goto usage;
	}
	if (h) hash_free(h);
	return (0);
}
