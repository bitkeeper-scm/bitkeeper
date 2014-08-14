#include "system.h"

private	int	rmtreewalk(char *file, char type, void *data);

int
rmtree(char *dir)
{
	char	**dirs = 0;
	int	i;
	int	ret;

	unless (exists(dir)) return (0);
	ret = walkdir(dir, rmtreewalk, &dirs);
	unless (ret) {
		reverseLines(dirs);
		EACH (dirs) {
			if (rmdir(dirs[i])) {
				perror(dirs[i]);
				ret = 1;
				break;
			}
		}
	}
	freeLines(dirs, free);
	if (ret) fprintf(stderr, "rmtree(%s) failed.\n", dir);
	return (ret);
}

private int
rmtreewalk(char *file, char type, void *data)
{
	char	***dirs = data;
	int	ret;
	struct	stat sb;

	if (type == 'd') {
		if (ret = rmIfRepo(file)) return (ret);
		if (!lstat(file, &sb) && ((sb.st_mode & 0700) != 0700)) {
			chmod(file, sb.st_mode | 0700);
		}
		*dirs = addLine(*dirs, strdup(file));
	} else {
		if (unlink(file) && (errno != ENOENT)) {
			perror(file);
			return (1);
		}
	}
	return (0);
}
