/*
 * Copyright 2006-2007,2010-2011,2016 BitMover, Inc
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

#ifndef WIN32
int
hide(char *file, int on_off)
{
	return (0);
}

int
cat(char *file)
{
	int	len, left;
	MMAP	*m = mopen(file, "r");

	unless (m) return (-1);
	fflush(stdout);
	for (left = m->size; left; left -= len) {
		/* <= 0 instead of < 0 is paranoia of infinite loop */
		if ((len = write(1, m->mmap + (m->size-left), left)) <= 0) {
			if (errno == EPIPE) break;
			if (errno == EINTR) continue;
			mclose(m);
			return (-1);
		}
	}
	mclose(m);
	return (0);
}
#else
int
hide(char *file, int on_off)
{
	DWORD attr;

	attr = GetFileAttributes(file);
	if (attr == INVALID_FILE_ATTRIBUTES) return (-1);
	if (on_off) {
		unless (attr & FILE_ATTRIBUTE_HIDDEN) {
			attr |= FILE_ATTRIBUTE_HIDDEN;
			SetFileAttributes(file, attr);
		}
	} else {
		if (attr & FILE_ATTRIBUTE_HIDDEN) {
			attr &= ~FILE_ATTRIBUTE_HIDDEN;
			SetFileAttributes(file, attr);
		}
	}
	return (0);
}

/*
 * We need a win32 version beacuse win32 write interface cannot
 * handle large buffer, do _not_ change this code unless you tested it 
 * on win32. I coded ths once before and someone removed it. - awc
 *
 * XXX TODO move this to the port directory.
 */
int
cat(char *file)
{
	MMAP	*m = mopen(file, "r");
	char	*p;
	int	n;

	unless (m) return (-1);

	p = m->mmap;
	n = m->size;
	while (n) {
		if (n >=  MAXLINE) {
			write(1, p, MAXLINE);
			n -= MAXLINE;
			p += MAXLINE;
		} else {
			write(1, p, n);
			n = 0;
			p = 0;
		}
	};
	mclose(m);
	return (0);
}
#endif

char *
loadfile(char *file, int *size)
{
	FILE	*f;
	struct	stat	statbuf;
	char	*ret;
	int	len;

	f = fopen(file, "r");
	unless (f) return (0);

	if (fstat(fileno(f), &statbuf)) {
 err:		fclose(f);
		return (0);
	}
	len = statbuf.st_size;
	ret = malloc(len+1);
	unless (ret) goto err;
	fread(ret, 1, len, f);
	if (ferror(f)) {
		free(ret);
		goto err;
	}
	fclose(f);
	ret[len] = 0;

	if (size) *size = len;
	return (ret);
}

/*
 * Create a file if it doesn't already exist.
 *
 * NOTE: Unlike touch(1) this doesn't update the timestamp for
 *       existing files.
 */
int
touch(char *file, int mode)
{
	int	fh = open(file, O_CREAT|O_WRONLY, mode);

	if (fh < 0) return (fh);
	return (close(fh));
}

int
sameFiles(char *file1, char *file2)
{
	int	rc = 0;
	int	len;
	FILE	*f1 = 0, *f2 = 0;
	struct	stat sb1, sb2;
	char	buf1[8<<10], buf2[8<<10];

	unless (f1 = fopen(file1, "r")) goto out;
	unless (f2 = fopen(file2, "r")) goto out;
	if (fstat(fileno(f1), &sb1)) goto out;
	if (fstat(fileno(f2), &sb2)) goto out;
	if (sb1.st_size != sb2.st_size) goto out;
	if (!win32() &&
	    (sb1.st_ino == sb2.st_ino) && (sb1.st_dev == sb2.st_dev)) {
		rc = 1;
		goto out;
	}
	while (len = fread(buf1, 1, sizeof(buf1), f1)) {
		unless (len == fread(buf2, 1, sizeof(buf2), f2)) goto out;
		if (memcmp(buf1, buf2, len)) goto out;
	}
	rc = 1;
out:	if (f1) fclose(f1);
	if (f2) fclose(f2);
	return (rc);
}
