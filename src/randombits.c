#include "sccs.h"
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"

void
randomBits(char *buf)
{
    	u32	a, b;

	rand_getBytes((void *)&a, 4);
	rand_getBytes((void *)&b, 4);
	sprintf(buf, "%x%x", a, b);
}


int	in_rcs_import = 0;

/*
 * Return an at most 5 digit !0 integer.
 *
 * Used for:
 *    - 1.0 deltas
 *    - null deltas in files
 */
sum_t
almostUnique(void)
{
        u32     val;
	char	*p;

	if (p = getenv("BK_RANDOM")) {
		/* get 20 bits worth */
		sscanf(p, "%5x", &val);
		val %= 100000;		/* low 5 digits */
		return (val);
	}
	if (in_rcs_import) return (0);
	do {
		rand_getBytes((void *)&val, 4);
		val %= 100000;		/* low 5 digits */
	} while (!val);
        return (val);
}
