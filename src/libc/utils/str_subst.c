#include "system.h"

/*
 * replace all occurances of 'search' in 'str' with 'replace' writing
 * the result to 'output'.
 *
 * if output=0, return the output in a malloc'ed string
 *
 * inplace edits are OK, if they don't make the string longer.
 */
char *
str_subst(char *str, char *search, char *replace, char *output)
{
	char	*p, *s, *t;
	int	slen, rlen, n;
	char	buf[MAXLINE];	/* limited, oh well. */

	unless (output) output = buf;
	slen = strlen(search);
	rlen = strlen(replace);
	if (str == output) assert(slen >= rlen);
	s = str;
	t = output;
	while (p = strstr(s, search)) {
		/* copy leading text */
		n = p - s;
		strncpy(t, s, n);
		s += n;
		t += n;

		/* make subst */
		strcpy(t, replace);
		t += rlen;
		s += slen;
	}
	/* copy remaining text */
	strcpy(t, s);
	if (output == buf) {
		assert((t - output) + strlen(s) < sizeof(buf));
		output = strdup(output);
	}
	return (output);
}
