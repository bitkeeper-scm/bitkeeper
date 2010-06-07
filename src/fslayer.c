#define	FSLAYER_NODEFINES
#include "sccs.h"
#include "libc/fslayer/win_remap.h"

private	project	*findpath(const char *path, char **rel, int sccsonly);


private	int	noloop = 1;
private	FILE	*strace = 0;

#define	STRACE(x) if (strace) fprintf x

/*
 * Allows the user to enable or disable the fslayer filter to the
 * filesystem.
 * Returns the previous enable state of fslayer.
 * To run a section of code without the fslayer:
 *      old = fslayer_enable(0);
 *	... my code ...
 *	fslayer_enable(old);
 */
int
fslayer_enable(int en)
{
	int	old = !noloop;

	unless (strace) strace = efopen("BK_STRACE");
	noloop = !en;
	if (getenv("_BK_FSLAYER_SKIP")) noloop = 1;
	return (old);
}

int
fslayer_open(const char *path, int flags, mode_t mode)
{
	int	ret;
	project	*proj;
	char	*rel;

	if (noloop) return (open(path, flags, mode));
	noloop = 1;
	if (proj = findpath(path, &rel, 1)) {
		ret = remap_open(proj, rel, flags, mode);
		proj_free(proj);
	} else {
		ret = open(path, flags, mode);
	}
	STRACE((strace, "open(%s, %d, %o) = %d\n",
		path, flags, mode, ret));
	noloop = 0;
	return (ret);
}

int
fslayer_close(int fd)
{
	int	ret;

	if (noloop) {
		ret = close(fd);
	} else {
		noloop = 1;
		ret = close(fd);
		STRACE((strace, "close(%d) = %d\n", fd, ret));
		noloop = 0;
	}
	return (ret);
}

ssize_t
fslayer_read(int fd, void *buf, size_t count)
{
	ssize_t	ret;

	if (noloop) {
		ret = read(fd, buf, count);
	} else {
		noloop = 1;
		ret = read(fd, buf, count);
		STRACE((strace, "read(%d, buf, %d) = %d\n", fd, count, ret));
		noloop = 0;
	}
	return (ret);
}

ssize_t
fslayer_write(int fd, const void *buf, size_t count)
{
	ssize_t	ret;

	if (noloop) {
		ret = write(fd, buf, count);
	} else {
		noloop = 1;
		ret = write(fd, buf, count);
		STRACE((strace, "write(%d, buf, %d) = %d\n", fd, count, ret));
		noloop = 0;
	}
	return (ret);
}

off_t
fslayer_lseek(int fildes, off_t offset, int whence)
{
	off_t	ret;

	if (noloop) {
		ret = lseek(fildes, offset, whence);
	} else {
		noloop = 1;
		ret = lseek(fildes, offset, whence);
		STRACE((strace, "lseek(%d, %d, %d) = %d\n",
		    fildes, offset, whence, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_lstat(const char *path, struct stat *buf)
{
	int	ret;
	char	*rel;
	project	*proj;

	if (noloop) {
		ret = lstat(path, buf);
	} else {
		noloop = 1;
		if (proj = findpath(path, &rel, 1)) {
			ret = remap_lstat(proj, rel, buf);
			proj_free(proj);
		} else {
			ret = lstat(path, buf);
		}
		if (strace) {
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
	project	*proj;
	char	*rel;

	if (noloop) {
		ret = stat(path, buf);
	} else {
		noloop = 1;
		if (proj = findpath(path, &rel, 1)) {
			ret = remap_lstat(proj, rel, buf);
			proj_free(proj);
		} else {
			ret = stat(path, buf);
		}
		if (strace) {
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
fslayer_fstat(int fd, struct stat *buf)
{
	int	ret;

	if (noloop) {
		ret = fstat(fd, buf);
	} else {
		noloop = 1;
		ret = fstat(fd, buf);
		if (strace) {
			fprintf(strace, "fstat(%d, {", fd);
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
	project	*proj;
	char	*rel;

	if (noloop) {
		ret = unlink(path);
	} else {
		noloop = 1;
		if (proj = findpath(path, &rel, 1)) {
			ret = remap_unlink(proj, rel);
			proj_free(proj);
		} else {
			ret = unlink(path);
		}
		STRACE((strace, "unlink(%s) = %d\n", path, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_rename(const char *old, const char *new)
{
	int	ret;
	project	*proj1 = 0;
	project	*proj2 = 0;
	char	*rel1, *rel2;

	if (noloop) {
		ret = rename(old, new);
	} else {
		noloop  = 1;
		proj1 = findpath(old, &rel1, 0);
		if (rel1) rel1 = strdup(rel1);
		proj2 = findpath(new, &rel2, 0);

		ret = remap_rename(proj1, rel1, proj2, rel2);
		if (rel1) free(rel1);
		if (proj1) proj_free(proj1);
		if (proj2) proj_free(proj2);
		STRACE((strace, "rename(%s, %s) = %d\n", old, new, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_chmod(const char *path, mode_t mode)
{
	int	ret;
	project	*proj;
	char	*rel;

	if (noloop) {
		ret = chmod(path, mode);
	} else {
		noloop = 1;
		if (proj = findpath(path, &rel, 1)) {
			ret = remap_chmod(proj, rel, mode);
			proj_free(proj);
		} else {
			ret = chmod(path, mode);
		}
		STRACE((strace, "chmod(%s, %o) = %d\n", path, mode, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_link(const char *old, const char *new)
{
	int	ret;
	project	*proj1 = 0;
	project	*proj2 = 0;
	char	*rel1, *rel2;

	if (noloop) {
		ret = link(old, new);
	} else {
		noloop = 1;
		proj1 = findpath(old, &rel1, 0);
		if (rel1) rel1 = strdup(rel1);
		proj2 = findpath(new, &rel2, 0);

		ret = remap_link(proj1, rel1, proj2, rel2);
		if (rel1) free(rel1);
		if (proj1) proj_free(proj1);
		if (proj2) proj_free(proj2);
		STRACE((strace, "link(%s, %s) = %d\n", old, new, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_symlink(const char *old, const char *new)
{
	int	ret;

	if (noloop) {
		ret = symlink(old, new);
	} else {
		noloop = 1;
		assert(!isSCCS(new)); /* no SYMLINKS in SCCS dirs */
		ret = symlink(old, new);
		STRACE((strace, "symlink(%s, %s) = %d\n", old, new, ret));
		noloop = 0;
	}
	return (ret);
}

char **
fslayer__getdir(char *dir, struct stat *sb)
{
	char	**ret;
	project	*proj;
	char	*rel;

	if (noloop) {
		ret = _getdir(dir, sb);
	} else {
		noloop = 1;
		if (proj = findpath(dir, &rel, 0)) {
			ret = remap_getdir(proj, rel);
			proj_free(proj);
		} else {
			ret = _getdir(dir, sb);
		}
		STRACE((strace, "_getdir(%s, sb) = list\n", dir));
		noloop = 0;
	}
	return (ret);
}

char *
fslayer_realBasename(const char *path, char *realname)
{
	char	*rel, *ret = 0;
	project	*proj;

	if (noloop) {
		ret = realBasename(path, realname);
	} else {
		noloop = 1;
		if (proj = findpath(path, &rel, 1)) {
 			ret = remap_realBasename(proj, rel, realname);
			proj_free(proj);
		} else {
			ret = realBasename(path, realname);
		}
		noloop = 0;
	}
	return (ret);
}

int
fslayer_access(const char *path, int mode)
{
	int	ret;
	project	*proj;
	char	*rel;

	if (noloop) {
		ret = access(path, mode);
	} else {
		noloop = 1;

		if (proj = findpath(path, &rel, 1)) {
			ret = remap_access(proj, rel, mode);
			proj_free(proj);
		} else {
			ret = access(path, mode);
		}
		STRACE((strace, "access(%s, %d) = %d\n", path, mode, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_utime(const char *path, const struct utimbuf *buf)
{
	int	ret;
	project	*proj;
	char	*rel;

	if (noloop) {
		ret = utime(path, buf);
	} else {
		noloop = 1;
		if (proj = findpath(path, &rel, 1)) {
			ret = remap_utime(proj, rel, buf);
			proj_free(proj);
		} else {
			ret = utime(path, buf);
		}
		STRACE((strace, "utime(%s, buf) = %d\n", path, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_mkdir(const char *path, mode_t mode)
{
	int	ret;
	project	*proj;
	char	*rel;

	if (noloop) {
		ret = mkdir(path, mode);
	} else {
		noloop = 1;
		if (proj = findpath(path, &rel, 0)) {
			ret = remap_mkdir(proj, rel, mode);
			proj_free(proj);
		} else {
			ret = mkdir(path, mode);
		}
		STRACE((strace, "mkdir(%s, %o) = %d\n", path, mode, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_rmdir(const char *dir)
{
	int	ret;
	project	*proj;
	char	*rel;

	if (noloop) {
		ret = rmdir(dir);
	} else {
		noloop = 1;

		if (proj = findpath(dir, &rel, 0)) {
			ret = remap_rmdir(proj, rel);
			proj_free(proj);
		} else {
#ifndef	NOPROC
			ret = checking_rmdir((char *)dir);
#else
			ret = rmdir(dir);
#endif
		}
		STRACE((strace, "rmdir(%s) = %d\n", dir, ret));
		noloop = 0;
	}
	return (ret);
}

/*
 * Called from main() to cleanup
 */
void
fslayer_cleanup(void)
{
	if (strace && (strace != INVALID)) {
		fclose(strace);
		strace = 0;
	}
}

/* returns 1 pathname is file in SCCS dir
 * returns 2 if path is SCCS dir
 * returns 0 otherwise
 *
 * examples:
 *	0 .
 *	0 file
 *	0 dir
 *	2 SCCS
 *	1 SCCS/file
 *	1 SCCS/dir  (we assume no dirs under SCCS)
 *	2 path/SCCS
 *	1 path/SCCS/file
 */
int
isSCCS(const char *path)
{
	const char	*p, *slash;
	int	cnt;

	/* p = end of path, slash = last / */
	slash = 0;
	for (p = path; *p; p++) {
		if (*p == '/') slash = p;
	}
	unless (slash) slash = path - 1;
	cnt = slash-path;
	if ((cnt == 4) && strneq(path, "SCCS", 4)) return (1);
	if ((cnt > 4) && strneq(slash-5, "/SCCS", 5)) return (1);
	if (((p - slash) == 5) && streq(slash+1, "SCCS")) return (2);
	return (0);
}

/*
 * Given a pathname return the project* containing that file
 * and the relative path from the repo root to this file.
 * Or return 0 (no proj) and the path name that was passed in unfiltered.
 *
 * sccsonly means don't bother if isn't inside a SCCS subdirectory.
 * This acts like no proj for all non SCCS paths.
 */
private project *
findpath(const char *path, char **relp, int sccsonly)
{
	static	char	buf[MAXPATH];

	char	*rel;
	project	*proj;
	char	*dn;		/* directory to pass to proj_init() */
	int	c = isSCCS(path);
	char	pbuf[MAXPATH];

	strcpy(pbuf, path);
	dn = dirname(pbuf);
	if (c == 2) {
		/* dir/SCCS dn=dir*/
	} else if (c) {
		/* dir/SCCS/file	dn=dir */
		dn = dirname(dn);
	} else if (sccsonly) {
noproj:		*relp = (char *)path;
		return (0);
	} else if (isdir((char *)path)) {
		dn = (char *)path;
	}
	unless (proj = proj_init(dn)) goto noproj;
	if (proj_hasOldSCCS(proj)) {
		proj_free(proj);
		goto noproj;
	}
	rel = proj_relpath(proj, (char *)path);
	unless (rel) {
		fprintf(stderr, "error with file mapping\n"
		    "dir %s proj %s cwd %s\n",
		    path, proj_root(proj), proj_cwd());
		exit(103);
	}
	assert(rel);
	strcpy(buf, rel);
	*relp = buf;
	free(rel);
	return (proj);
}
