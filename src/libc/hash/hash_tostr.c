/*
 * Copyright 2006,2011-2012,2016 BitMover, Inc
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
 * Routines to save and restore a hash to and from a string.  To be
 * useful for other work the format of that string matches RFC3986
 * query strings.
 */

char *
hash_toStr(hash *h)
{
	char	**data = 0;
	char	*ret;
	FILE	*f = fmem();

	EACH_HASH(h) {
		webencode(f, h->kptr, h->klen);
		putc('=', f);
		webencode(f, h->vptr, h->vlen);
		data = addLine(data, fmem_dup(f, 0));
		ftrunc(f, 0);
	}
	fclose(f);
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
		unless (p = webdecode(p, &k, &klen)) {
err:			fprintf(stderr,
			    "ERROR: hash_fromStr() can't parse '%s'\n",
			    str);
			if (k) free(k);
			if (v) free(v);
			return (-1);
		}
		unless (*p++ == '=') goto err;
		unless (p = webdecode(p, &v, &vlen)) goto err;
		hash_store(h, k, klen, v, vlen);
		if (k) free(k);
		if (v) free(v);
		k = v = 0;
		unless (*p) break;
		unless (*p++ == '&') goto err;
	}
	return (0);
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

extern	int	hashstr_test_main(int ac, char **av);

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
