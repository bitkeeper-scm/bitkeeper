#include "sccs.h"


/*
 * REPO/dir/SCCS/X.file -> REPO/.bk/dir/SCCS/file,X
 *
 * returns output of isSCCS(rel)
 */
private	int
fullRemapPath(char *buf, project *proj, char *rel)
{
	char	*p;
	int	suffix, ret = 0;
	char    newrel[MAXPATH];

	unless (proj) {
		strcpy(buf, rel);
		return (ret);
	}
	if (ret = isSCCS(rel)) {
		concat_path(newrel, ".bk", rel);

		/* p -> s.foo */
		p = strrchr(newrel, '/') + 1;

		/* we're sometimes called with path/SCCS */
		if (p[1] == '.') {
			/* save the prefix char, it'll become suffix */
			suffix = *p;
			sprintf(p, "%s,%c", p+2, suffix);
		}
	} else {
		strcpy(newrel, rel);
	}
	if (streq(newrel, ".")) {
		strcpy(buf, proj_root(proj));
	} else {
		concat_path(buf, proj_root(proj), newrel);
	}
	return (ret);
}

private void
unremap_name(char *name)
{
	int	l;
	char	prefix;

	l = strlen(name);
	if (l < 3) return;
	unless (name[l - 2] == ',') return;

	prefix = name[l - 1];
	name[l - 2] = 0;
	memmove(name + 2, name, l - 2);
	name[0] = prefix;
	name[1] = '.';
	//name[l] = 0;
}


/*
 * open a file
 *
 * return
 *   -1 failure
 */
int
remap_open(project *proj, char *rel, int flags, mode_t mode)
{
	int	ret;
	char	buf[MAXPATH];

	fullRemapPath(buf, proj, rel);
	ret = open(buf, flags, mode);
	return (ret);
}

int
remap_utime(project *proj, char *rel, const struct utimbuf *utb)
{
	char	buf[MAXPATH];

	fullRemapPath(buf, proj, rel);
	return (utime(buf, utb));
}

int
remap_lstat(project *proj, char *rel, struct stat *sb)
{
	char	buf[MAXPATH];

	fullRemapPath(buf, proj, rel);
	return (lstat(buf, sb));
}

char *
remap_realBasename(project *proj, char *rel, char *realname)
{
	char	*ret;
	int	sccs;
	char	buf[MAXPATH];

	sccs = fullRemapPath(buf, proj, rel);
	ret = realBasename(buf, realname);
	if (sccs) unremap_name(realname);
	return (ret);
}

int
remap_unlink(project *proj, char *rel)
{
	char	buf[MAXPATH];

	fullRemapPath(buf, proj, rel);
	return (unlink(buf));
}

int
remap_rename(project *proj1, char *old, project *proj2, char *new)
{
	int	rc;
	char	buf1[MAXPATH];
	char	buf2[MAXPATH];

	fullRemapPath(buf1, proj1, old);
	fullRemapPath(buf2, proj2, new);
	if (!(rc = rename(buf1, buf2)) &&
	    proj1 && (proj1 == proj2) && !isSCCS(old)) {
		/*
		 * maybe we just did:
		 *    mv old new
		 * add a:
		 *    mv .bk/old .bk/new
		 * just in case.
		 */
		concat_path(buf1, proj_root(proj1), ".bk");
		concat_path(buf1, buf1, old);
		if (isdir(buf1)) {
			concat_path(buf2, proj_root(proj2), ".bk");
			concat_path(buf2, buf2, new);
			mkdirf(buf2);
			rename(buf1, buf2);
		}
	}
	return (rc);
}

int
remap_link(project *proj1, char *old, project *proj2, char *new)
{
	char	buf1[MAXPATH];
	char	buf2[MAXPATH];

	fullRemapPath(buf1, proj1, old);
	fullRemapPath(buf2, proj2, new);
	return (link(buf1, buf2));
}

int
remap_chmod(project *proj, char *rel, mode_t mode)
{
	u8	buf[MAXPATH];

	fullRemapPath(buf, proj, rel);
	return (chmod(buf, mode));
}

int
remap_mkdir(project *proj, char *dir, mode_t mode)
{
	u8	buf[MAXPATH];

	if (fullRemapPath(buf, proj, dir)) {
		mkdirp(buf);
		fullRemapPath(buf, proj, dirname(dir));
		return (mkdirp(buf));
	} else {
		return (mkdir(buf, mode));
	}
}

int
remap_rmdir(project *proj, char *dir)
{
	int	idf = 0;
	char	buf[MAXPATH], buf2[MAXPATH];

	if (!isSCCS(dir) && !streq(dir, ".")) {
		/*
		 * Can't remove a directory with a SCCS subdir, and
		 * need to keep .bk directories in sync
		 */
		sprintf(buf2, "%s/.bk/%s", proj_root(proj), dir);
		if (isdir(buf2)) {
			unless (emptyDir(buf2)) {
				errno = ENOTEMPTY;
				return (-1);
			}
			rmdir(buf2);
		}
	}
	fullRemapPath(buf, proj, dir);

	/*
	 * Being called with "." means we are deleting the whole repository
	 * so make sure we delete .bk as well.
	 */
	if (streq(dir, ".")) {
		concat_path(buf2, buf, ".bk");
		idf = isdir(buf2);
		assert(idf);
		rmdir(buf2);
	}

	return (rmdir(buf));
}

char **
remap_getdir(project *proj, char *dir)
{
	int	i, sccs;
	char	**ret, *t;
	char	**mapdir;
	char	tmp[MAXPATH];
	char	buf[MAXPATH];

	sccs = fullRemapPath(buf, proj, dir);
	ret = getdir(buf);
	if (sccs) {
		EACH(ret) unremap_name(ret[i]);
	} else {
		if (streq(dir, ".")) removeLine(ret, ".bk", free);
		if (proj && getenv("_BK_CREATE_MISSING_DIRS")) {
			sprintf(tmp, "%s/.bk/%s", proj_root(proj), dir);
			mapdir = getdir(tmp);
			/* remove items from mapdir already in 'ret' */
			pruneLines(mapdir, ret, 0, free);
			EACH(mapdir) {
				unless (streq(mapdir[i], "SCCS")) {
					concat_path(tmp, buf, mapdir[i]);
					mkdir(tmp, 0777);
				}
				ret = addLine(ret, mapdir[i]);
			}
			freeLines(mapdir, 0); /* copied all to ret */
			uniqLines(ret, free);
		} else {
			concat_path(tmp, dir, "SCCS");
			fullRemapPath(buf, proj, tmp);
			if (isdir(buf)) {
				t = malloc(7);
				memcpy(t, "SCCS\0d", 7);
				ret = addLine(ret, t);
				uniqLines(ret, free);
			}
		}
	}
	return (ret);
}

int
remap_access(project *proj, char *rel, int mode)
{
	char	buf[MAXPATH];

	fullRemapPath(buf, proj, rel);
	return (access(buf, mode));
}
