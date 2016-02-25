/*
 * Copyright 1999-2006,2009-2010,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"

/*
 * copy a file
 * see: fileLink() if creating a hardlink is OK.
 */
int
fileCopy(char *from, char *to)
{
	char	buf[8192];
	int	n, from_fd = -1, to_fd = -1;
	int	ret = -1;
	struct	stat sb;
	char	tofile[MAXPATH];

	strcpy(tofile, to);	/* 'to' might be read only */
	mkdirf(tofile);
	unlink(tofile);
	if ((from_fd = open(from, 0, 0)) == -1) goto err;
	if (fstat(from_fd, &sb) == -1) goto err;
	if ((to_fd = creat(tofile, sb.st_mode & 0777)) == -1) goto err;
	while ((n = read(from_fd, buf, sizeof(buf))) > 0) {
		if (write(to_fd, buf, n) != n) goto err;
	}
	ret = 0;
err:	if (from_fd > 0) close(from_fd);
	if (to_fd > 0) close(to_fd);
	if (ret) perror(to);
	return (ret);
}


/*
 * a smart wrapper for link().
 * will create destination directories if needed
 * will fall back to fileCopy() if hardlinks fail
 */
int
fileLink(char *from, char *to)
{
	char	tofile[MAXPATH];

	if (link(from, to)) {
		strcpy(tofile, to);	/* 'to' might be read only */
		if (mkdirf(tofile)) {
			perror(tofile);
			return (-1);
		}
		/* fileCopy already calls perror on failure */
		if (link(from, tofile) && fileCopy(from, tofile)) return (-1);
	}
	return (0);
}

/*
 * A wrapper for rename()
 * - will create destination directories if needed
 * - will revert to fileCopy()/unlink if rename() fails
 */
int
fileMove(char *src, char *to)
{
	char	dest[MAXPATH];

	if (rename(src, to)) {
		strcpy(dest, to);	/* 'to' might be read only */
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
