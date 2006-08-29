#include "system.h"

private	int	rmtreewalk(char *file, struct stat *sb, void *data);

int
rmtree(char *dir)
{
	char	**dirs = 0;
	int	i;
	int	ret;

	ret = walkdir(dir, rmtreewalk, &dirs);
	unless (ret) {
		reverseLines(dirs);
		EACH (dirs) {
			if (rmdir(dirs[i])) {
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
rmtreewalk(char *file, struct stat *sb, void *data)
{
	char	***dirs = data;

	if (S_ISDIR(sb->st_mode)) {
		*dirs = addLine(*dirs, strdup(file));
	} else {
		if (unlink(file)) {
			fprintf(stderr, "unlink(%s) failed\n", file);
			return (1);
		}
	}
	return (0);
}
