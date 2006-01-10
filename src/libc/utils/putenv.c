#include "system.h"

#undef	putenv

/*
 * impliment putenv() but make a copy of each string and only save
 * one copy per variable.
 */
void
safe_putenv(char *fmt, ...)
{
	static	char	**saved = 0;
	char	*old;
	char	*new;
	char	*p;
	int	len;
	int	i;
	va_list	ptr;
	int	rc;
	char	*buf;
	int	size = strlen(fmt) + 64;

	/*
	 * It is hard to write portable varargs code because
	 * we can't depend on va_copy.  So we duplicate the inards
	 * of aprintf() here.
	 */
	while (1) {
		buf = malloc(size);
		va_start(ptr, fmt);
		rc = vsnprintf(buf, size, fmt, ptr);
		va_end(ptr);
		if (rc >= 0 && rc < size - 1) break;
		free(buf);
		if (rc < 0 || rc == size - 1) {
			/*
			 * Older C libraries return -1 to indicate
			 * the buffer was too small.
			 *
			 * On IRIX, it truncates and returns size-1.
			 * We can't assume that that is OK, even
			 * though that might be a perfect fit.  We
			 * always bump up the size and try again.
			 * This can rarely lead to an extra alloc that
			 * we didn't need, but that's tough.
			 */
			size *= 2;
		} else {
			/* In C99 the number of characters needed 
			 * is always returned. 
			 */
			size = rc + 2;	/* extra byte for IRIX */
		}
	}
	/* end of aprintf() */

	new = buf;

	p = strchr(new, '=');
	unless (p) {
		fprintf(stderr, "putenv: can't remove var '%s'\n", new);
		free(new);
		exit(1);
	}
	len = p - new;
	new[len] = 0;
	old = getenv(new);
	new[len++] = '=';	/* include '=' in len */
	++p;	/* p == new value */
	if (old && streq(old, p)) {
		free(new);
		return;	
	}
	putenv(new);
	/* look for an existing copy */
	EACH(saved) {
		if (strneq(saved[i], new, len)) {
			free(saved[i]);
			saved[i] = new;
			return;
		}
	}
	saved = addLine(saved, new);
}
