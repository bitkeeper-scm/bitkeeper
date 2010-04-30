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
	MMAP	*m = mopen(file, "r");

	unless (m) return (-1);
	fflush(stdout);
	unless (write(1, m->mmap, m->size) == m->size) {
		mclose(m);
		return (-1);
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
	fclose(f);
	ret[len] = 0;

	if (size) *size = len;
	return (ret);
}

int
touch(char *file, int mode)
{
	int	fh = open(file, O_CREAT|O_WRONLY, mode);

	if (fh < 0) return (fh);
	return (close(fh));
}
