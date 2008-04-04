#define	FSLAYER_NODEFINES
#include "sccs.h"

#define	NEG1	(void *)~0u

private	int	noloop;
private	FILE	*strace = NEG1;

private	inline int
do_trace(void)
{
	if (strace == NEG1) strace = efopen("BK_STRACE");
	return (strace != 0);
}

#define	TRACE(x) if (do_trace()) fprintf x

int
fslayer_open(const char *path, int flags, mode_t mode)
{
	int	ret;

	ret = open(path, flags, mode);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "open(%s, %d, %o) = %d\n",
		    path, flags, mode, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_close(int fd)
{
	int	ret;

	ret = close(fd);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "close(%d) = %d\n", fd, ret));
		noloop = 0;
	}
	return (ret);
}

ssize_t
fslayer_read(int fd, void *buf, size_t count)
{
	ssize_t	ret;

	ret = read(fd, buf, count);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "read(%d, buf, %d) = %d\n", fd, count, ret));
		noloop = 0;
	}
	return (ret);
}

ssize_t
fslayer_write(int fd, const void *buf, size_t count)
{
	ssize_t	ret;

	ret = write(fd, buf, count);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "write(%d, buf, %d) = %d\n", fd, count, ret));
		noloop = 0;
	}
	return (ret);
}

off_t
fslayer_lseek(int fildes, off_t offset, int whence)
{
	off_t	ret;

	ret = lseek(fildes, offset, whence);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "lseek(%d, %d, %d) = %d\n",
		    fildes, offset, whence, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_lstat(const char *path, struct stat *buf)
{
	int	ret;

	ret = lstat(path, buf);
	unless (noloop) {
		noloop = 1;
		if (do_trace()) {
			fprintf(strace, "lstat(%s, {", path);
			unless (ret) {
				fprintf(strace, "mode=%o", buf->st_mode);
				if (S_ISREG(buf->st_mode)) {
					fprintf(strace, ", size=%d",
					    buf->st_size);
				}
			}
			fprintf(strace, "}) = %d\n", ret);
		}
		noloop = 0;
	}
	return (ret);
}

int
fslayer_stat(const char *path, struct stat *buf)
{
	int	ret;

	ret = stat(path, buf);
	unless (noloop) {
		noloop = 1;
		if (do_trace()) {
			fprintf(strace, "stat(%s, {", path);
			unless (ret) {
				fprintf(strace, "mode=%o", buf->st_mode);
				if (S_ISREG(buf->st_mode)) {
					fprintf(strace, ", size=%d",
					    buf->st_size);
				}
			}
			fprintf(strace, "}) = %d\n", ret);
		}
		noloop = 0;
	}
	return (ret);
}

int
fslayer_unlink(const char *path)
{
	int	ret;

	ret = unlink(path);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "unlink(%s) = %d\n", path, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_rename(const char *old, const char *new)
{
	int	ret;

	ret = rename(old, new);
	unless (noloop) {
		noloop  = 1;
		TRACE((strace, "rename(%s, %s) = %d\n", old, new, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_chmod(const char *path, mode_t mode)
{
	int	ret;

	ret = chmod(path, mode);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "chmod(%s, %o) = %d\n", path, mode, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_link(const char *old, const char *new)
{
	int	ret;

	ret = link(old, new);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "link(%s, %s) = %d\n", old, new, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_symlink(const char *old, const char *new)
{
	int	ret;

	ret = symlink(old, new);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "symlink(%s, %s) = %d\n", old, new, ret));
		noloop = 0;
	}
	return (ret);
}

char **
fslayer__getdir(char *dir, struct stat *sb)
{
	char	**ret;

	ret = _getdir(dir, sb);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "_getdir(%s, sb) = list\n", dir));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_access(const char *path, int mode)
{
	int	ret;

	ret = access(path, mode);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "access(%s, %d) = %d\n", path, mode, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_utime(const char *path, const struct utimbuf *buf)
{
	int	ret;

	ret = utime(path, buf);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "utime(%s, buf) = %d\n", path, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_mkdir(const char *path, mode_t mode)
{
	int	ret;

	ret = mkdir(path, mode);
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "mkdir(%s, %o) = %d\n", path, mode, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_rmdir(const char *dir)
{
	int	ret;

#ifndef	NOPROC
	ret = checking_rmdir((char *)dir);
#else
	ret = rmdir(dir);
#endif
	unless (noloop) {
		noloop = 1;
		TRACE((strace, "rmdir(%s) = %d\n", dir, ret));
		noloop = 0;
	}
	return (ret);
}
