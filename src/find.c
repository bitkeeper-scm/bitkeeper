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
private	int	wantdir = 0;

int
find_main(int ac, char **av)
{
	int	i;
	char	**dirs = 0;

	debug_main(av);

	if ((ac > 1) && streq("--help", av[1])) {
		fprintf(stderr, "%s", files_usage);
		exit(0);
	}
	for (i = 1; av[i]; i++) {
		if (streq("-name", av[i])) {
			globs = addLine(globs, strdup(av[++i]));
		} else if (streq("-type", av[i])) {
			++i;
			if (streq("d", av[i])) wantdir = 1;
		} else {
			dirs = addLine(dirs, strdup(av[i]));
		}
	}
	unless (dirs) dirs = addLine(0, strdup("."));
	EACH (dirs) {
		walkdir(dirs[i], do_print, 0);
	}
	freeLines(dirs, free);
	if (globs) freeLines(globs, free);
	return (0);
}

private int
do_print(char *path, struct stat *sb, void *data)
{
	char	*t;
	int	isdir = (S_ISDIR(sb->st_mode) != 0);

	if (strneq(path, "./", 2)) path += 2;
	t = strrchr(path, '/');
	t = t ? (t+1) : path;

	if ((wantdir && isdir) || (!wantdir && !isdir)) {
		unless (globs && !match_globs(t, globs, 0)) {
			printf("%s\n", path);
		}
	}
	return (0);
}
