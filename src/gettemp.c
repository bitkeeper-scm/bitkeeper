#include "system.h"
#include "sccs.h"

/*
 * Create the tmp file, using our tmp directory.
 * usage: gettemp(buf, "cset");
 */
int
gettemp(char *buf, const char *tmpl)
{
	int	fd;

	assert(tmpl[0] != '/');
	sprintf(buf, "%s/%sXXXXXX", TMP_PATH, tmpl);
	fd = mkstemp(buf);
	if (fd != -1) {
		close(fd);
		return (0);
	}
	perror("mkstemp");
	return (-1);
}
