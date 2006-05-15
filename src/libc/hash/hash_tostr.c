#include "system.h"

/*
 * Routines to save and restore a hash to and from a string.  To be
 * useful for other work the format of that string matches RFC3986
 * query strings.
 */

private	int	is_encoded(int c);
private	char	**wrapstr(char **buf, u8 *ptr, int len);
private	char	*unwrapstr(char *data, char **buf, int *size);

char *
hash_toStr(hash *h)
{
	char	**buf = 0;
	char	**data = 0;
	char	*ret;

	EACH_HASH(h) {
		buf = wrapstr(buf, h->kptr, h->klen);
		buf = str_nappend(buf, "=", 1, 0);
		buf = wrapstr(buf, h->vptr, h->vlen);
		data = addLine(data, str_pullup(0, buf));
		buf = 0;
	}
	sortLines(data, 0);	/* sort by keys */
	ret = joinLines("&", data);
	freeLines(data, free);
	return (ret);
}

int
hash_fromStr(hash *h, char *str)
{
	char	*p = str;
	char	*k = 0, *v = 0;
	u32	klen, vlen;

	while (*p) {
		unless (p = unwrapstr(p, &k, &klen)) {
err:			fprintf(stderr,
			    "ERROR: hash_fromStr() can't parse '%s'\n",
			    str);
			if (k) free(k);
			if (v) free(v);
			return (-1);
		}
		unless (*p++ == '=') goto err;
		unless (p = unwrapstr(p, &v, &vlen)) goto err;
		hash_store(h, k, klen, v, vlen);
		if (k) free(k);
		if (v) free(v);
		k = v = 0;
		unless (*p) break;
		unless (*p++ == '&') goto err;
	}
	return (0);
}

/*
 * Encode the data in ptr/len and add it to buf
 * using data_append().
 */
private	char	**
wrapstr(char **buf, u8 *ptr, int len)
{
	char	hex[4];

	while (len > 0) {
		/* suppress trailing null (common) */
		if ((len == 1) && !*ptr) break;

		if (*ptr == ' ') {
			hex[0] = '+';
			buf = str_nappend(buf, hex, 1, 0);
		} else if (is_encoded(*ptr)) {
			sprintf(hex, "%%%02x", *ptr);
			buf = str_nappend(buf, hex, 3, 0);
		} else {
			buf = str_nappend(buf, ptr, 1, 0);
		}
		++ptr;
		--len;
	}
	/* %FF(captials) is a special bk marker for no trailing null */
	if (len == 0) buf = str_nappend(buf, "%FF", 3, 0);
	return (buf);
}

/*
 * unpack a wrapped string from *data and put it in the buffer buf.
 * If successful, returns new pointer to data and set size.
 * Else return 0.
 * Any whitespace in the string a ignored and skipped.
 */
private char *
unwrapstr(char *data, char **buf, int *sizep)
{
	u8	*p = (u8 *)data;
	int	c;
	int	bin = 0;
	char	**lines = 0;
	char	tmp[4];

	while (1) {
		switch (*p) {
		    case '+':
			tmp[0] = ' ';
			lines = data_append(lines, tmp, 1, 0);
			break;
		    case '%':
			if ((p[1] == 'F') && (p[2] == 'F')) {
				bin = 1;
				p += 2;
				break;
			}
			unless (sscanf(p+1, "%2x", &c) == 1) goto err;
			tmp[0] = c;
			lines = data_append(lines, tmp, 1, 0);
			p += 2;
			break;
		    case ' ': case '\n': case '\r': case '\t':
			break;
		    case '&': case '=': case 0:
			unless (bin) { /* add trailing null */
				tmp[0] = 0;
				lines = data_append(lines, tmp, 1, 0);
			}
			*buf = data_pullup(sizep, lines);
			return (p);
		    default:
			lines = data_append(lines, p, 1, 0);
			break;
		}
		p++;
	}
err:
	fprintf(stderr, "ERROR: can't decode %s\n", p);
	return (0);
}

/*
 * Return true if this character should be encoded according to RFC1738
 */
private int
is_encoded(int c)
{
	static	u8	*binchars = 0;

	unless (binchars) {
		int	i;
		char	*p;

		binchars = malloc(256);
		/* all encoded by default */
		for (i = 0; i < 256; i++) binchars[i] = 1;

		/* these don't need encoding */
		for (i = 'A'; i <= 'Z'; i++) binchars[i] = 0;
		for (i = 'a'; i <= 'z'; i++) binchars[i] = 0;
		for (i = '0'; i <= '9'; i++) binchars[i] = 0;
		for (p = "-_.~/@"; *p; p++) binchars[(int)*p] = 0;
	}
	return (binchars[c]);
}

#define	MAXBUF	512

private int
randbuf(char *buf)
{
	int	i, len;

	len = rand() % MAXBUF;

	for (i = 0; i < len; i++) buf[i] = (u8)rand();
	return (len);
}

int
hashstr_test_main(int ac, char **av)
{
	hash	*h1 = hash_new(HASH_MEMHASH);
	hash	*h2 = hash_new(HASH_MEMHASH);
	int	klen, vlen, i;
	char	*p1, *p2;
	char	buf1[MAXBUF];
	char	buf2[MAXBUF];

	srand((int)time(0));

	/* ascii */
	klen = sprintf(buf1, "%s", "hello");
	vlen = sprintf(buf2, "%s", "world");
	hash_store(h1, buf1, klen + 1, buf2, vlen + 1);

	/* null key & val */
	hash_store(h1, buf1, 0, buf1, 0);

	/* blank key & val */
	buf1[0] = 0;
	hash_store(h1, buf1, 1, buf1, 1);

	/* null val */
	klen = randbuf(buf1);
	hash_store(h1, buf1, klen, buf1, 0);

	/* blank val */
	klen = randbuf(buf1);
	buf2[0] = 0;
	hash_store(h1, buf1, klen, buf2, 1);

	/* random data */
	for (i = 0; i < 10; i++) {
		klen = randbuf(buf1);
		vlen = randbuf(buf2);
		hash_store(h1, buf1, klen, buf2, vlen);
	}

	p1 = hash_toStr(h1);
	hash_fromStr(h2, p1);
	p2 = hash_toStr(h2);
	unless (streq(p1, p2)) abort();
	free(p2);
	free(p1);

	EACH_HASH(h2) {
		unless (hash_fetch(h1, h2->kptr, h2->klen)) abort();
		unless ((h1->vlen == h2->vlen) &&
		    !memcmp(h1->vptr, h2->vptr, h1->vlen)) {
			abort();
		}
		if (hash_delete(h1, h2->kptr, h2->klen)) abort();
	}
	if (hash_first(h1)) abort();
	hash_free(h1);
	hash_free(h2);

	/* XXX should probably test some error conditions... */
	return (0);
}
