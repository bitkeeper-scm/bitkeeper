#include "system.h"
#include "sccs.h"

private	int	print_lstat(const char *fn);

int
lstat_main(int ac, char **av)
{
	char	c;
	int	i, errors = 0;
	char	buf[MAXPATH];

	if (av[1]) {
		for (i = 1; i < ac; i++) {
			errors += print_lstat(av[i]);
		}
		return (errors);
	}
	while (fnext(buf, stdin)) {
		unless ((c = chop(buf)) == '\n') {
			fprintf(stderr, "Bad filename '%s%c'\n", buf, c);
			errors = 1;
			continue;
		}
		errors += print_lstat(buf);
	}
	return (errors);
}

private	int
print_lstat(const char *fn)
{
	struct	stat sb;

	if (lstat(fn, &sb)) {
		return (1);
	} else {
		fprintf(stderr, "%d|%d|%o|%d|%d|%d|%d|%d|%d|%d|%d|%s\n",
		    sb.st_dev, sb.st_ino, sb.st_mode,
		    sb.st_nlink, sb.st_uid, sb.st_gid, sb.st_rdev,
		    sb.st_size, sb.st_atime, sb.st_mtime, sb.st_ctime,
		    fn);
	}
	return (0);
}
