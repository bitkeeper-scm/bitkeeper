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

/*
 * Give a string containing multiple newline separated lines, return each
 * line one-by-line.
 *
 * - return any junk after last \n
 * - an empty line must end in a \n
 *
 * ex:
 * line = COMMENTS(s, d);
 * while (p = eachline(&line, &len)) {
 *	// do stuff
 * }
 */
char *
eachline(char **linep, int *lenp)
{
	char	*line, *ret = *linep;
	int	len;

	unless (*ret) return (0);
	for (line = ret; *line && (*line != '\n'); line++);
	if (lenp) {
		len = line - ret;
		while ((len > 0) && (ret[len-1] == '\r')) --len;
		*lenp = len;
	}
	if (*line == '\n') line++;
	*linep = line;
	return (ret);
}
