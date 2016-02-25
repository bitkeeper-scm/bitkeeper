/*
 * Copyright 2006-2007,2015-2016 BitMover, Inc
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


/*
 * Encode the data in ptr/len and write to stdio filehandle
 *
 * If you want to encode a string using webencode(), you should pass
 * it strlen(str)+1 for the length, otherwise you'll get a %FF on the
 * end.  This is because webencode() needs to be binary safe, since
 * it's used by hash_toStr().
 */
void
webencode(FILE *out, u8 *ptr, int len)
{
	while (len > 0) {
		/* suppress trailing null (common) */
		if ((len == 1) && !*ptr) break;

		if (*ptr == ' ') {
			putc('+', out);
		} else if (is_encoded(*ptr)) {
			fprintf(out, "%%%02x", *ptr);
		} else {
			putc(*ptr, out);
		}
		++ptr;
		--len;
	}
	/* %FF(captials) is a special bk marker for no trailing null */
	if (len == 0) fputs("%FF", out);
}

/*
 * unpack a wrapped string from *data and put it in the buffer buf.
 * The string ends on the first '&' '=' or '\0'.
 * If successful, returns new pointer to data and sets size.
 * Else return 0.
 */
char *
webdecode(char *data, char **buf, int *sizep)
{
	char	*p = data;
	char	*t;
	char	*ret;
	int	c;
	int	bin = 0;

	assert(buf);
	ret = t = malloc(strcspn(data, "=&") + 1);
	while (1) {
		switch (*p) {
		    case '+':
			*t++ = ' ';
			break;
		    case '%':
			if ((p[1] == 'F') && (p[2] == 'F')) {
				bin = 1;
				p += 2;
				break;
			}
			unless (sscanf(p+1, "%2x", &c) == 1) goto err;
			*t++ = c;
			p += 2;
			break;
		    case '&': case '=': case 0:
			unless (bin) *t++ = 0; /* add trailing null */
			if (sizep) *sizep = (t - ret);
			*buf = ret;
			return (p);
		    default:
			*t++ = *p;
			break;
		}
		p++;
	}
err:
	fprintf(stderr, "ERROR: can't decode %s\n", p);
	return (0);
}

