#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

/*
 * _find - find regular files and symbolic links
 */
private	char *files_usage = "\n\
usage: _find [dir...] [-name bk_glob_pattern]\n\
\n";
private int	do_print(char *path, struct stat *sb, void *data);
private	char 	**globs = 0;

int
find_main(int ac, char **av)
{
	int	i;

	debug_main(av);

	if ((ac > 1) && streq("--help", av[1])) {
		fprintf(stderr, "%s", files_usage);
		exit(0);
	}
	if ((ac > 3) && streq("-name", av[ac - 2])) {
		globs = addLine(0, strdup(av[ac - 1]));
		av[ac - 2] = 0;
	}
	unless (av[1]) {
		walkdir(".", do_print, 0);
	} else {
		for (i = 1; av[i]; ++i) {
			walkdir(av[i], do_print, 0);
		}
	}
	if (globs) freeLines(globs, free);
	exit(0);
}

private int
do_print(char *path, struct stat *sb, void *data)
{
	char	*t;

	if (strneq(path, "./", 2)) path += 2;
	t = strrchr(path, '/');
	t = t ? (t+1) : path;

	unless (S_ISDIR(sb->st_mode)) {
		unless (globs && !match_globs(t, globs, 0)) {
			printf("%s\n", path);
		}
	}
	return (0);
}
