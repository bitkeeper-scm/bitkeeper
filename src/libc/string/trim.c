#include "system.h"

/*
 * Trim leading/trailing white space.  Ours is a little different than
 * some in that it shifts left so that you either get back the same
 * address or NULL.  Nicer for free().
 *
 * If you want trimdup() that is #define trimdup(s) trim(strdup(s))
 */
char *
trim(char *buf)
{
	char	*s, *t;
	char	*trailing = 0;	/* just past last non-space char */

	unless (buf) return (0);
	s = t = buf;		/* src and dest */
	while (*s && isspace(*s)) s++;
	while (1) {
		if (t != s) *t = *s;
		unless (*s) break;
		t++;
		unless (isspace(*s)) trailing = t;
		s++;
	}
	if (trailing) *trailing = 0;
	return (buf);
}
