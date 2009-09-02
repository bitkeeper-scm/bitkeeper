#include "system.h"

/* %K% */

int
fileCopy(char *from, char *to)
{
	char	buf[8192];
	int	n, from_fd, to_fd;
	struct	stat sb;
	char	tofile[MAXPATH];

	strcpy(tofile, to);	/* 'to' might be read only */
	mkdirf(tofile);
	if ((from_fd = open(from, 0, 0)) == -1) {
		perror(from);
		return (-1);
	}
	if (fstat(from_fd, &sb) == -1) {
		perror(from);
		return (-1);
	}
	if ((to_fd = creat(tofile, sb.st_mode & 0777)) == -1) {
		perror(tofile);
		return (-1);
	}
	while ((n = read(from_fd, buf, sizeof(buf))) > 0) {
		if (write(to_fd, buf, n) != n) {
			perror(to);
			return (-1);
		}
	}
	close(from_fd);
	close(to_fd);
	return (0);
}
