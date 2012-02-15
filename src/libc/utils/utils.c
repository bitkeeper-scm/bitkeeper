#include "system.h"

#undef	perror

void
my_perror(char *file, int line, char *msg)
{
	char	*p = 0;
	int	save = errno;

	if (p = getenv("_BK_VERSION")) {
		if (strneq(p, "bk-", 3)) p += 3;
		fprintf(stderr, "%s:%d (%s): ", file, line, p);
	} else {
		fprintf(stderr, "%s:%d: ", file, line);
	}
	if (p = strerror(errno)) {
		fprintf(stderr, "%s: %s\n", msg, p);
	} else {
		fprintf(stderr, "%s: errno=%d\n", msg, errno);
	}
	errno = save;
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

/*
 * Give a string containing white space separated tokens, give the
 * next token and its length.
 *
 * - returns pointer after token
 * - no such thing as null token
 *
 * ex:
 * line = CLUDES(s, d);
 * while (p = eachstr(&line, &len)) {
 *	// do stuff to process token
 * }
 */
char *
eachstr(char **linep, int *lenp)
{
	char	*line, *ret = *linep;

	unless (ret) return (0);
	while (*ret && isspace(*ret)) ret++;
	unless (*ret) {
		*linep = 0;
		if (lenp) *lenp = 0;
		return (0);
	}
	for (line = ret; *line && !isspace(*line); line++);
	if (lenp) *lenp = line - ret;
	*linep = line;
	return (ret);
}
