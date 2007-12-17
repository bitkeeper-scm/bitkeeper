#include <stdio.h>

#include "reentrant.h"
#include "local.h"

/*
 * Get an input line.  The returned pointer often (but not always)
 * points into a stdio buffer. fgetline() will return a null terminated
 * C string with any netwlines and carriage returns removed.
 */
char *
fgetline(FILE *fp)
{
	char	*cp;
	size_t	len;

	FLOCKFILE(fp);
	cp = __fgetstr(fp, &len, '\n');
	FUNLOCKFILE(fp);
	unless (cp) return (cp);
	while (len > 0 && (cp[len - 1] == '\r' || cp[len - 1] == '\n')) --len;
	cp[len] = 0;
	return (cp);
}
