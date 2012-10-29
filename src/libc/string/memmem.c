#include "string/str.cfg"
#ifdef BK_STR_MEMMEM
#include "local_string.h"

/* Local extention to standard C library, so sue me. */

/* Return the first occurrence of SUB in DATA  */
char *
memmem(char *data, int datalen, char *sub, int sublen)
{
	char	*p;
	char	*end = data + datalen - sublen;

	if (sublen == 0) return (data);
	if (sublen > datalen) return (0);

	for (p = data; p <= end; ++p) {
		if ((*p == *sub) && !memcmp(p + 1, sub + 1, sublen - 1)) {
			return (p);
		}
	}
	return (0);
}
#endif
