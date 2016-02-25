/*
 * Copyright 2008-2011,2013-2016 BitMover, Inc
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

#define	FSLAYER_NODEFINES
#include "sccs.h"
#include "fsfuncs.h"
#ifdef	WIN32
#include "libc/fslayer/win_remap.h"
#else
#define	rename(a, b)	smartRename(a, b)
#define	unlink(a)	smartUnlink(a)
#endif

typedef struct {
	fsfuncs	*funcs;
	project	*proj;		/* repo containing files */
	char	*rel;		/* path from repo root */
	int	isSCCS;		/* IS_FILE in SCCS or IS_DIR if .../SCCS */
} Fp;

#define	FP_SCCS	1		/* fail if path is not SCCS */
#define	FP_DIR	2		/* assume path is a directory */
#define	FP_FILE	3		/* assume path is a file */
private	int	fp_find(const char *path, Fp *fp, int mode);
inline	private	void	fp_free(Fp *fp);

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
	Fp	fp;

	if (noloop) return (open(path, flags, mode));
	noloop = 1;

	if (fp_find(path, &fp, FP_SCCS)) {
		ret = fp.funcs->_open(fp.proj, fp.rel, flags, mode);
		fp_free(&fp);
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
		STRACE((strace, "read(%d, buf, %d) = %d\n",
			fd, (int)count, (int)ret));
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
		STRACE((strace, "write(%d, buf, %d) = %d\n",
			fd, (int)count, (int)ret));
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
		    fildes, (int)offset, whence, (int)ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_linkcount(const char *path, struct stat *buf)
{
	int	ret;
	Fp	fp;

	if (noloop) {
		ret = linkcount(path, buf);
	} else {
		noloop = 1;
		if (fp_find(path, &fp, FP_SCCS)) {
			ret = fp.funcs->_linkcount(fp.proj, fp.rel, buf);
			fp_free(&fp);
		} else {
			ret = linkcount(path, buf);
		}
		if (strace) {
			fprintf(strace, "linkcount(%s) = %d", path, ret);
		}
		noloop = 0;
	}
	return (ret);
}

int
fslayer_lstat(const char *path, struct stat *buf)
{
	int	ret;
	Fp	fp;

	if (noloop) {
		ret = lstat(path, buf);
	} else {
		noloop = 1;
		if (fp_find(path, &fp, FP_SCCS)) {
			/*
			 * For blob we only need to set:
			 *   st_mode = S_IFREG | 0444;
			 *   st_nlink = 1
			 *   st_size = xxx
			 */
			ret = fp.funcs->_lstat(fp.proj, fp.rel, buf);
			fp_free(&fp);
		} else {
			ret = lstat(path, buf);
		}
		if (strace) {
			fprintf(strace, "lstat(%s, {", path);
			unless (ret) {
				fprintf(strace, "mode=%o", buf->st_mode);
				if (S_ISREG(buf->st_mode)) {
					fprintf(strace, ", size=%d",
					    (int)buf->st_size);
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
	Fp	fp;

	if (noloop) {
		ret = stat(path, buf);
	} else {
		noloop = 1;
		if (fp_find(path, &fp, FP_SCCS)) {
			ret = fp.funcs->_lstat(fp.proj, fp.rel, buf);
			fp_free(&fp);
		} else {
			ret = stat(path, buf);
		}
		if (strace) {
			fprintf(strace, "stat(%s, {", path);
			unless (ret) {
				fprintf(strace, "mode=%o", buf->st_mode);
				if (S_ISREG(buf->st_mode)) {
					fprintf(strace, ", size=%d",
					    (int)buf->st_size);
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
					    (int)buf->st_size);
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
	Fp	fp;

	if (noloop) {
		ret = unlink((char *)path);
	} else {
		noloop = 1;
		if (fp_find(path, &fp, FP_SCCS)) {
			ret = fp.funcs->_unlink(fp.proj, fp.rel);
			fp_free(&fp);
		} else {
			ret = unlink((char *)path);
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
	Fp	fp1, fp2;

	if (noloop) {
		ret = rename((char *)old, (char *)new);
	} else {
		noloop  = 1;

		fp_find(old, &fp1, FP_FILE);
		fp_find(new, &fp2, FP_FILE);

		/* we never rename SCCS dirs */
		assert((fp1.isSCCS != IS_DIR) && (fp2.isSCCS != IS_DIR));

		/*
		 * This one is a bit of a pain.
		 * We really only need to call a fs plugin for renames
		 * of files in SCCS directories or directory renames.
		 * But unless we add an extra lstat() we don't know if
		 * a rename("a/b", "a/c") is renaming a file or directory
		 * so these are also used.
		 * Currently we never rename between repos (other than RESYNC)
		 * so there assert() is safe.
		 */
		if (fp1.proj && fp2.proj) {
			assert(fp1.funcs == fp2.funcs);
			ret = fp1.funcs->_rename(fp1.proj, fp1.rel,
			    fp2.proj, fp2.rel);
		} else {
			ret = rename((char *)old, (char *)new);
		}
		fp_free(&fp1);
		fp_free(&fp2);
		STRACE((strace, "rename(%s, %s) = %d\n", old, new, ret));
		noloop = 0;
	}
	return (ret);
}

int
fslayer_chmod(const char *path, mode_t mode)
{
	int	ret;

	if (noloop) {
		ret = chmod(path, mode);
	} else {
		noloop = 1;
		if (isSCCS(path)) {
			ret = 0;  // XXX just ignore
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
	Fp	fp1, fp2;

	if (noloop) {
		ret = link(old, new);
	} else {
		noloop = 1;

		fp_find(old, &fp1, FP_FILE);
		fp_find(new, &fp2, FP_FILE);

		/* we never link SCCS dirs */
		assert((fp1.isSCCS != IS_DIR) && (fp2.isSCCS != IS_DIR));

		/*
		 * Here we kind of have a hack.
		 * It is possible to hardlink SCCS files between two
		 * different repositories that are stored in a different
		 * format.  However we can only pick one helper function.
		 * At the moment we assume the two projects either match
		 * or one side is an old SCCS repo.
		 * Blob won't hardlink SCCS files so this seem OK for now.
		 */
		if (fp1.proj && fp2.proj) assert(fp1.funcs == fp2.funcs);
		if (fp1.proj || fp2.proj) {
			fsfuncs	*f = fp1.funcs;

			unless (fp1.proj) {
				f = fp2.funcs;
				fp1.rel = strdup(old);
			}
			unless (fp2.proj) fp2.rel = strdup(new);
			ret = f->_link(fp1.proj, fp1.rel, fp2.proj, fp2.rel);
		} else {
			ret = link(old, new);
		}
		fp_free(&fp1);
		fp_free(&fp2);
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
fslayer_getdir(char *dir)
{
	char	**ret;
	Fp	fp;

	if (noloop) {
		ret = getdir(dir);
	} else {
		noloop = 1;

		if (fp_find(dir, &fp, FP_DIR)) {
			ret = fp.funcs->_getdir(fp.proj, fp.rel);
			fp_free(&fp);
		} else {
			ret = getdir(dir);
		}
		STRACE((strace, "getdir(%s) = ", dir));
		if (ret) {
			STRACE((strace, "list{%d}\n", nLines(ret)));
		} else {
			STRACE((strace, "0\n"));
		}
		noloop = 0;
	}
	return (ret);
}

char *
fslayer_realBasename(const char *path, char *realname)
{
	char	*ret = 0;
	Fp	fp;

	if (noloop) {
		ret = realBasename(path, realname);
	} else {
		noloop = 1;
		if (fp_find(path, &fp, FP_SCCS)) {
			ret = fp.funcs->_realBasename(fp.proj, fp.rel, realname);
			fp_free(&fp);
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
	Fp	fp;

	if (noloop) {
		ret = access(path, mode);
	} else {
		noloop = 1;

		if (fp_find(path, &fp, FP_SCCS)) {
			ret = fp.funcs->_access(fp.proj, fp.rel, mode);
			fp_free(&fp);
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
	Fp	fp;

	if (noloop) {
		ret = utime(path, buf);
	} else {
		noloop = 1;
		if (fp_find(path, &fp, FP_SCCS)) {
			assert(0); /* shouldn't happen */
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
	Fp	fp;

	if (noloop) {
		ret = smartMkdir((char *)path, mode);
	} else {
		noloop = 1;
		if ((isSCCS(path) == IS_DIR) &&
		    fp_find(path, &fp, FP_SCCS)) {
			ret = fp.funcs->_mkdir(fp.proj, fp.rel);
			fp_free(&fp);
		} else {
			ret = smartMkdir((char *)path, mode);
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
	Fp	fp;
	char	buf[MAXPATH];

	if (noloop) {
		ret = rmdir(dir);
	} else {
		noloop = 1;

		if (fp_find(dir, &fp, FP_DIR)) {
			if (fp.isSCCS == IS_DIR) {
				ret = fp.funcs->_rmdir(fp.proj, fp.rel);
			} else {
				/* Can't remove a directory with a
				 * SCCS subdir */
				concat_path(buf, fp.rel, "SCCS");
				if (fp.funcs->_isdir(fp.proj, buf)) {
					errno = ENOTEMPTY;
					ret = -1;
				} else {
#ifndef	NOPROC
					ret = checking_rmdir((char *)dir);
#else
					ret = rmdir(dir);
#endif
				}
			}
			fp_free(&fp);
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
 * The return feeds the walkdir callback:
 * 0 - nothing happened
 * -1 - prune this from walk
 * -2 - dump this whole getdir that this dir was part of (not used here)
 * > 0 - error
 */
int
fslayer_rmIfRepo(char *dir)
{
	int	ret = 0;
	char	buf[MAXPATH];

	if (noloop) return (0);
	concat_path(buf, dir, BKROOT);
	if (exists(buf)) {
		noloop = 1;
		ret = rmtree(dir);
		noloop = 0;
		unless (ret) ret = -1; 	// prune from walk, as this was removed.
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
	if ((cnt == 4) && strneq(path, "SCCS", 4)) return (IS_FILE);
	if ((cnt > 4) && strneq(slash-5, "/SCCS", 5)) return (IS_FILE);
	if (((p - slash) == 5) && streq(slash+1, "SCCS")) return (IS_DIR);
	return (0);
}

/*
 * Given a pathname return the project* containing that file
 * and the relative path from the repo root to this file.
 * Or return 0 (no proj) and the path name that was passed in unfiltered.
 */
private int
fp_find(const char *path, Fp *fp, int mode)
{
	char	*dn;		/* directory to pass to proj_init() */
	int	c = isSCCS(path);
	char	pbuf[MAXPATH];

	fp->isSCCS = c;
	strcpy(pbuf, path);
	dn = dirname(pbuf);
	if (c == IS_DIR) {
		/* dir/SCCS dn=dir*/
	} else if (c) {
		/* dir/SCCS/file	dn=dir */
		dn = dirname(dn);
	} else if (mode == FP_SCCS) {
noproj:		fp->proj = 0;
		fp->rel = 0;
		fp->funcs = 0;
		return (0);
	} else if (mode == FP_DIR) {
		dn = (char *)path;
	}
	unless (fp->proj = proj_init(dn)) goto noproj;
	if (proj_hasOldSCCS(fp->proj)) {
		proj_free(fp->proj);
		goto noproj;
	}
	strcpy(pbuf, path);
	fp->rel = proj_relpath(fp->proj, pbuf);
	unless (fp->rel) {
		fprintf(stderr, "error with file mapping\n"
		    "dir %s proj %s cwd %s\n",
		    path, proj_root(fp->proj), proj_cwd());
		exit(103);
	}
	fp->funcs = &remap_funcs;
	return (1);
}

inline private void
fp_free(Fp *fp)
{
	if (fp->proj) proj_free(fp->proj);
	if (fp->rel) free(fp->rel);
}
