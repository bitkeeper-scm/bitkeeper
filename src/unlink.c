#include "system.h"
#include "sccs.h"

unlink_main(int ac, char **av)
{
	char	c;
	int	errors = 0;
	char	buf[MAXPATH];

	while (fgets(buf, sizeof(buf), stdin)) {
		unless ((c = chop(buf)) == '\n') {
			fprintf(stderr, "Bad filename '%s%c'\n", buf, c);
			errors = 1;
			continue;
		}
		if (unlink(buf)) errors = 1;
	}
	return (errors);
}
