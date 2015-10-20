#include "system.h"
#include "sccs.h"

/*
 * _find - find regular files and symbolic links
 */
private int	do_print(char *path, char type, void *data);
private	char 	**globs = 0;
private	int	wantdir = 0;

int
find_main(int ac, char **av)
{
	int	i;
	char	**dirs = 0;


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
		walkdir(dirs[i], (walkfns){ .file = do_print}, 0);
	}
	freeLines(dirs, free);
	if (globs) freeLines(globs, free);
	return (0);
}

private int
do_print(char *path, char type, void *data)
{
	char	*t;
	int	isdir = (type == 'd');

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
