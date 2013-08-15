#include "system.h"
#include "stdio.h"
#include "errno.h"

/*
 * Replace a file handle with a new one that is layered on top.
 * The old handle is remembered and closed automatically in fclose().
 */
int
fpush(FILE **fp, FILE *new)
{
	if (!new) return (-1);
	assert(*fp != new);	/* avoid loop */
	assert(!new->_prevfh);
	new->_prevfh = *fp;
	new->_filename = (*fp)->_filename;
	*fp = new;
	return (0);
}


/*
 * For a file handle stacked with fpush(), this calls fclose() on
 * only the top find handle and returns the one underneath.
 * Returns the output of fclose().
 */
int
fpop(FILE **fp)
{
	int	ret;
	FILE	*prev;

	if (*fp) {
		prev = (*fp)->_prevfh;

		(*fp)->_prevfh = 0;
		(*fp)->_filename = 0;
		ret = fclose(*fp);
		*fp = prev;
	} else {
		errno = EINVAL;
		ret = -1;
	}
	return (ret);
}

char *
fname(FILE *fp, char *name)
{
	if (name && !fp->_filename) fp->_filename = strdup(name);
	return (fp->_filename);
}
