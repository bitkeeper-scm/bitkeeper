#include "system.h"
#include "sccs.h"

int
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

int
link_main(int ac, char **av)
{
	char	c;
	int	errors = 0;
	char	buf[MAXPATH];
	char	new[MAXPATH];

	unless (av[1] && isdir(av[1])) {
		fprintf(stderr, "Usage: %s directory\n", av[0]);
		exit(1);
	}
	while (fgets(buf, sizeof(buf), stdin)) {
		unless ((c = chop(buf)) == '\n') {
			fprintf(stderr, "Bad filename '%s%c'\n", buf, c);
			errors = 1;
			continue;
		}
		sprintf(new, "%s/%s", av[1], basenm(buf));
		if (link(buf, new)) {
			perror(new);
			errors = 1;
		}
	}
	return (errors);
}

int
exists_main(int ac, char **av)
{
	char	c;
	int	errors = 0;
	char	buf[MAXPATH];

	while (fgets(buf, sizeof(buf), stdin)) {
		unless ((c = chop(buf)) == '\n') {
			fprintf(stderr, "Bad filename '%s%c'\n", buf, c);
			exit(2);
		}
		if (exists(buf)) {
			printf("%s\n", buf);
			exit(0);
		}
	}
	exit(1);
}

