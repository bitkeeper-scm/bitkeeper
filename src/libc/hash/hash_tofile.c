#include "system.h"

/*
 * Routines to save and restore a hash to and from a file. The format
 * used in the file is the same as in the bugdb.
 */

private	int binaryField(u8 *data, int len);
private	int goodkey(u8 *key, int len);
private	void savekey(hash *h, int base64, char *key, char **val);
private	void writeField(FILE *f, char *key, u8 *data, int len);

/* These are from tomcrypt... */
extern int base64_decode(const unsigned char *in,  unsigned long inlen,
    unsigned char *out, unsigned long *outlen);
extern int base64_encode(const unsigned char *in,  unsigned long inlen,
    unsigned char *out, unsigned long *outlen);

/*
 * write the hash files to a FILE*
 * returns -1 on error, or 0
 */
int
hash_toFile(hash *h, FILE *f)
{
	char	**fieldlist = 0;
	int	i, rc = -1;
	u8	*data;

	/*
	 * Sort the fields and print them
	 */
	EACH_HASH(h) {
		unless (goodkey(h->kptr, h->klen)) goto out;
		fieldlist = addLine(fieldlist, h->kptr);
	}
	sortLines(fieldlist, 0);
	EACH(fieldlist) {
		data = hash_fetchStr(h, fieldlist[i]);
		writeField(f, fieldlist[i], data, h->vlen);
	}
	rc = 0;
out:	freeLines(fieldlist, 0);
	return (rc);
}

/*
 * Read a stream written by the function above and add keys to 'h'
 * overwriting any existing keys.  If h==0, then a new hash is
 * returned.
 */
hash *
hash_fromFile(hash *h, FILE *f)
{
	char	*line;
	char	*key = 0;
	int	base64 = 0;
	char	**val = 0;
	char	*p;
	unsigned long len;
	char	data[256];

	unless (h) h = hash_new(HASH_MEMHASH);
	while (line = fgetline(f)) {
		if ((line[0] == '@') && (line[1] != '@')) {
			if (streq(line, "@END@")) break;
			if (key || val) {
				/* save old key */
				savekey(h, base64, key, val);
				key = 0;
				val = 0;
			}
			base64 =
			    ((p = strchr(line, ' ')) && streq(p, " base64"));
			if (p) {
				unless (base64) {
					fprintf(stderr,
					    "hash_fromFile: bad line '%s'\n",
					    line);
				}
				*p = 0;
			}
			key = strdup(line+1);
		} else {
			if (*line == '@') ++line; /* skip escaped @ */
			if (base64) {
				len = sizeof(data);
				base64_decode(line, strlen(line), data, &len);
				val = data_append(val, data, len, 0);
			} else {
				if (val) str_append(val, "\n", 0);
				val = str_append(val, line, 0);
			}
		}
	}
	if (key || val) savekey(h, base64, key, val);
	return (h);
}


private int
goodkey(u8 *key, int len)
{
	int	i;

	for (i = 0; i < (len-1); i++) {
		unless (isprint(key[i]) && !isspace(key[i])) {
			fprintf(stderr, "%s invalid: %c\n", key, key[i]);
			return (0);
		}
	}
	unless (key[len-1] == 0) {
		fprintf(stderr, "%s, no null\n", key);
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

	for (i = 0; i < (len-1); i++) {
		int	c = data[i];
		unless (isprint(c) || isspace(c)) return (1);
		if (c == '\n') {
			if (i - lastret > 256) return (1);
			lastret = i;
		}
	}
	if (data[len-1] != 0) return (1);
	return (0);
}

private void
writeField(FILE *f, char *key, u8 *data, int len)
{
	unsigned long	inlen, outlen;
	u8	*p;
	char	out[128];

	fputc('@', f);
	fputs(key, f);
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
	} else {
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

private void
savekey(hash *h, int base64, char *key, char **val)
{
	u8	*data;
	int	len;

	unless (key) key = strdup("__HEADER__");
	if (base64) {
		data = data_pullup(&len, val);
	} else {
		data = str_pullup(&len, val);
		++len;
	}
	hash_store(h, key, strlen(key)+1, data, len); /* overwrite existing */
	free(key);
}

int
hashfile_test_main(int ac, char **av)
{
	hash	*h;
	FILE	*f;
	int	rc = 0;
	char	bdata[6] = {'a', 'b', 0, 'c', 'd', 0};

	unless (av[1]) {
		fprintf(stderr, "usage: %s <file>\n", av[0]);
		return (1);
	}
	h = hash_new(HASH_MEMHASH);
	/* create some hash values */
	hash_insertStr(h, "simple", "simple hash_insertStr");
	hash_insert(h, "binary", strlen("binary")+1, bdata, sizeof(bdata));
	hash_insertStr(h, "end", "@END@\n");
	hash_insertStr(h, "marker", "@@");
	hash_insert(h, "strnonl", 8, "strnonl", 7);
	hash_insertStr(h, "multiline", "a\nb\nc\nd\n");
	unless (f = fopen(av[1], "w")) {
		rc = 1;
		goto out;
	}
	if (hash_toFile(h, f)) {
		fprintf(stderr, "Failed to save hash\n");
		rc = 1;
		goto out;
	}
	fclose(f);
	f = 0;
	hash_free(h);
	h = 0;
	/* now print what we got from the file */
	unless (f = fopen(av[1], "r")) {
		rc = 1;
		goto out;
	}
	unless (h = hash_fromFile(0, f)) {
		fprintf(stderr, "Failed to load hash\n");
		goto out;
	}
	fclose(f);
	f = 0;
	unless (hash_fetchStr(h, "simple") &&
	    streq(h->vptr, "simple hash_insertStr")) {
		fprintf(stderr, "simple failed\n");
		rc = 1;
		goto out;
	}
	unless (hash_fetchStr(h, "end") &&
	    streq(h->vptr, "@END@\n")) {
		fprintf(stderr, "end failed\n");
		rc = 1;
		goto out;
	}
	unless (hash_fetchStr(h, "marker") &&
	    streq(h->vptr, "@@")) {
		fprintf(stderr, "marker failed\n");
		rc = 1;
		goto out;
	}
	unless (hash_fetchStr(h, "strnonl") &&
	    strneq(h->vptr, "strnonl", h->vlen)) {
		fprintf(stderr, "strnonl failed\n");
		rc = 1;
		goto out;
	}
	unless (hash_fetchStr(h, "binary") &&
	    (h->vlen == 6) &&
	    !memcmp(bdata, h->vptr, 6)) {
		fprintf(stderr, "binary failed\n");
		rc = 1;
		goto out;
	}
	unless (hash_fetchStr(h, "multiline") &&
	    streq(h->vptr, "a\nb\nc\nd\n")) {
		fprintf(stderr, "multiline failed\n");
		rc = 1;
		goto out;
	}
out:	if (f) fclose(f);
	if (h) hash_free(h);
	return (rc);
}
