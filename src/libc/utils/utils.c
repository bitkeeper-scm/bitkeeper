#include "system.h"

#undef	perror

void
my_perror(char *file, int line, char *msg)
{
	fprintf(stderr, "%s:%d: ", file, line);
	perror(msg);
}

/*
 * Remove any trailing newline or CR from a string.
 * Returns true if anything stripped.
 */
int
chomp(char *s)
{
	int	any = 0;
	char	*p;

	assert(s);
	p = s + strlen(s);
	while ((p > s) && ((p[-1] == '\n') || (p[-1] == '\r'))) --p, any = 1;
	*p = 0;
	return (any);
}
