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
			/* s.foo => foo,s */
			suffix = *p;
			while ((p[0] = p[2])) p++;
			*p++ =',';
			*p++ = suffix;
			*p = 0;
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
remap_linkcount(project *proj, char *rel, struct stat *sb)
{
	char	buf[MAXPATH];

	fullRemapPath(buf, proj, rel);
	return (linkcount(buf, sb));
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
	int	ret;
	char	*t;
	char	buf[MAXPATH], buf2[MAXPATH];

	if (fullRemapPath(buf, proj, dir)) {
		if (ret = rmdir(buf)) return (ret);

		/* remove any empty directories above */
		t = buf + strlen(buf);
		while (1) {
			while (*t != '/') --t;
			*t = 0;
			if (streq(t, "/.bk")) break;
			if (rmdir(buf)) break;
		}
		return (0);
	} else {
		/*
		 * Can't remove a directory with a SCCS subdir, and
		 * need to keep .bk directories in sync
		 */
		sprintf(buf2, "%s/.bk/%s/SCCS", proj_root(proj), dir);
		if (isdir(buf2)) {
			errno = ENOTEMPTY;
			return (-1);
		}
		return (rmdir(buf));
	}
}

struct getdirMerge {
	project	*proj;
	char	*dir;
	char	**add;
};

/*
 * Note: all dotbk entries will either be freed or copied to add.
 * The gfile holds a file or dir from the gfile namespace.
 * The dotbk holds a shadow .bk dir that matches a gfile dir
 */
private int
getdirMerge(void *token, char *gfile, char *dotbk)
{
	struct	getdirMerge *opts = token;
	char	buf[MAXPATH];

	unless (dotbk) return (0);	/* okay for gfile to not match .bk */

	if (dotbk[strlen(dotbk)+1] == 'f') {
		die("file in %s/.bk/%s/%s",
		    proj_root(opts->proj), opts->dir, dotbk);
	}
	if (gfile) {
		if (gfile[strlen(gfile)+1] == 'f') {
			warn("file '%s/%s' masks a directory with history,\n",
			    opts->dir, gfile);
			// XXX - die?  What if more than one?
			die("more details: bk getmsg shadow %s/%s\n",
			    opts->dir, gfile);
		}
		free(dotbk);
	} else {
		/* ret missing something from mapdir */
		unless (streq(dotbk, "SCCS")) {
			sprintf(buf, "%s/%s/%s",
			    proj_root(opts->proj), opts->dir, dotbk);
			mkdir(buf, 0777);
		}
		opts->add = addLine(opts->add, dotbk);
	}
	return (0);
}

char **
remap_getdir(project *proj, char *dir)
{
	int	i, sccs;
	char	**ret;
	char	**mapdir;
	struct	getdirMerge opts;
	char	buf[MAXPATH];

	sccs = fullRemapPath(buf, proj, dir);
	ret = getdir(buf);
	if (sccs) {
		EACH(ret) unremap_name(ret[i]);
	} else if (proj) {
		if (streq(dir, ".")) removeLine(ret, ".bk", free);
		sprintf(buf, "%s/.bk/%s", proj_root(proj), dir);
		mapdir = getdir(buf);

		opts.proj = proj;
		opts.dir = dir;
		opts.add = 0;
		parallelLines(ret, mapdir, 0, getdirMerge, &opts);
		freeLines(mapdir, 0);

		if (opts.add) {
			ret = catLines(ret, opts.add);
			freeLines(opts.add, 0);
			uniqLines(ret, free);
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
