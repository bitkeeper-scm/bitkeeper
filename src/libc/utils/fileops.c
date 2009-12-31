#include "system.h"

/* %K% */

/*
 * copy a file
 * see: fileLink() if creating a hardlink is OK.
 */
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


/*
 * a smart wrapper for link().
 * will create destination directories if needed
 * will fall back to fileCopy() if hardlinks fail
 */
int
fileLink(char *from, char *to)
{
	if (link(from, to)) {
		if (mkdirf(to)) {
			perror(to);
			return (-1);
		}
		/* fileCopy already calls perror on failure */
		if (link(from, to) && fileCopy(from, to)) return (-1);
	}
	return (0);
}

/*
 * A wrapper for rename()
 * - will create destination directories if needed
 * - will revert to fileCopy()/unlink if rename() fails
 */
int
fileMove(char *src, char *dest)
{
	if (rename(src, dest)) {
		/* try making the dir and see if that helps */
		mkdirf(dest);
		if (rename(src, dest)) {
			if (fileCopy(src, dest)) return (-1);
			if (unlink(src)) {
				perror(src);
				return (-1);
			}
		}
	}
	return (0);
}
