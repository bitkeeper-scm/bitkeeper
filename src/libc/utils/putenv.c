#include "system.h"

#undef	putenv
#undef	getenv

/*
 * We "clear" variables by doing putenv("VAR=").
 * So we want variables that exist in the environment but have no value
 * to appear to not exist at all.
 */
char *
safe_getenv(char *var)
{
	char	*ret;

	ret = getenv(var);
	if (ret && !*ret) ret = 0;
	return (ret);
}

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

	va_start(ptr, fmt);
	rc = vasprintf(&new, fmt, ptr);
	va_end(ptr);
	assert(rc >= 0);

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
