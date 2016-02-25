/*
 * Copyright 2009-2010,2013-2016 BitMover, Inc
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

#include "sccs.h"
#include "fsfuncs.h"


/*
 * REPO/dir/SCCS/X.file -> REPO/.bk/dir/SCCS/file,X
 *
 * returns output of isSCCS(rel)
 */
private	int
fullRemapPath(char *buf, project *proj, char *rel)
{
	char	*p;
	int	suffix, ret;
	char    newrel[MAXPATH];

	unless (proj) {
		strcpy(buf, rel);
		return (0);
	}
	assert(!strneq(rel, ".bk/", 4));
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
 * Open a file in a SCCS directory.
 * return -1 if that file doesn't exist
 */
private int
remap_open(project *proj, char *rel, int flags, mode_t mode)
{
	int	sccs;
	int	ret;
	char	buf[MAXPATH];

	sccs = fullRemapPath(buf, proj, rel);
	assert(sccs);
	ret = open(buf, flags, mode);
	return (ret);
}

private int
remap_linkcount(project *proj, char *rel, struct stat *sb)
{
	char	buf[MAXPATH];

	fullRemapPath(buf, proj, rel);
	return (linkcount(buf, sb));
}

/*
 * Only called for files in SCCS directories
 */
private int
remap_lstat(project *proj, char *rel, struct stat *sb)
{
	int	sccs;
	char	buf[MAXPATH];

	sccs = fullRemapPath(buf, proj, rel);
	assert(sccs);
	return (lstat(buf, sb));
}

private char *
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

/*
 * remove something from a SCCS directory
 */
private int
remap_unlink(project *proj, char *rel)
{
	int	sccs;
	char	buf[MAXPATH];

	sccs = fullRemapPath(buf, proj, rel);
	assert(sccs);
	return (unlink(buf));
}

private int
remap_rename(project *proj1, char *old, project *proj2, char *new)
{
	int	s1;
	int	rc;
	char	buf1[MAXPATH];
	char	buf2[MAXPATH];

	s1 = fullRemapPath(buf1, proj1, old);
	fullRemapPath(buf2, proj2, new);

	if (!(rc = rename(buf1, buf2)) &&
	    proj1 && (proj1 == proj2) && !s1) {
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

private int
remap_link(project *proj1, char *old, project *proj2, char *new)
{
	char	buf1[MAXPATH];
	char	buf2[MAXPATH];

	fullRemapPath(buf1, proj1, old);
	fullRemapPath(buf2, proj2, new);
	return (link(buf1, buf2));
}

/*
 * Called for xxx/SCCS directories only
 */
private int
remap_mkdir(project *p, char *rel)
{
	int	sccs, ret;
	char	buf[MAXPATH];

	assert(p);
	/*
	 * We auto create directories as needed to create SCCS, but
	 * first we have to verify the that the matching gfile
	 * directory exists
	 */
	concat_path(buf, proj_root(p), rel);
	dirname(buf);		/* strip SCCS */
	unless (isdir(buf)) {
		errno = ENOENT;
		return (-1);
	}
	sccs = fullRemapPath(buf, p, rel);
	assert(sccs == 2);
	ret = mkdirp(buf);
	return (ret);
}

/*
 * Called for xxx/SCCS directories only
 * we need to prune up to root as needed
 */
private int
remap_rmdir(project *proj, char *dir)
{
	char	*t;
	int	sccs;
	int	ret;
	char	buf[MAXPATH];

	sccs = fullRemapPath(buf, proj, dir);
	assert(sccs == 2);
	if (ret = rmdir(buf)) return (ret);

	/* remove any empty directories above */
	t = buf + strlen(buf);
	while (1) {
		while (*t != '/') --t;
		if (streq(t, "/.bk")) break;
		*t = 0;
		if (rmdir(buf)) break;
	}
	return (0);
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

	if (gfile) {
		/* both gfile and dotbk contain the same file */
		if ((gfile[strlen(gfile)+1] == 'f') &&
		    (dotbk[strlen(dotbk)+1] == 'd')) {
			warn("file '%s/%s' masks a directory with history,\n",
			    opts->dir, gfile);
			// XXX - die?  What if more than one?
			die("more details: bk getmsg shadow %s/%s\n",
			    opts->dir, gfile);
		}
		free(dotbk);
	} else {
		/* dotbk contains something not in gfile space */
		if (streq(dotbk, "SCCS")) {
			opts->add = addLine(opts->add, dotbk);
		} else if ((dotbk[strlen(dotbk)+1] == 'd') ||
		    (sprintf(buf, "%s/.bk/%s/%s",
			proj_root(opts->proj), opts->dir, dotbk) &&
		     isdir(buf))) {
			/* repair missing directory in gfile space */
			sprintf(buf, "%s/%s/%s",
			    proj_root(opts->proj), opts->dir, dotbk);
			mkdir(buf, 0777);
			opts->add = addLine(opts->add, dotbk);
		} else {
			/* we ignore files, but don't expect them */
			free(dotbk);
		}
	}
	return (0);
}

/*
 * Add directory information from the index.  In SCCS then everything
 * is from index, outside the index just adds additional information.
 */
private char **
remap_getdir(project *p, char *dir)
{
	int	sccs, i;
	char	**ret, **mapdir;
	struct	getdirMerge opts;
	char	buf[MAXPATH];

	sccs = fullRemapPath(buf, p, dir);
	ret = getdir(buf);
	if (sccs) {
		EACH(ret) unremap_name(ret[i]);
	} else {
		if (streq(dir, ".")) removeLine(ret, ".bk", free);
		opts.proj = p;
		opts.dir = dir;
		opts.add = 0;

		sprintf(buf, "%s/.bk/%s", proj_root(p), dir);
		if (mapdir = getdir(buf)) {
			parallelLines(ret, mapdir, 0, getdirMerge, &opts);
			freeLines(mapdir, 0); /* mapdir items freed in getdirMerge */
		}
		if (opts.add) {
			ret = catLines(ret, opts.add);
			freeLines(opts.add, 0);
			uniqLines(ret, free);
		}
	}
	return (ret);
}

/*
 * used for "xxx/SCCS" only
 */
private int
remap_isdir(project *p, char *rel)
{
	int	sccs;
	char	ret[MAXPATH];

	sccs = fullRemapPath(ret, p, rel);
	assert(sccs == 2);
	return (isdir(ret));
}

private int
remap_access(project *proj, char *rel, int mode)
{
	char	buf[MAXPATH];

	fullRemapPath(buf, proj, rel);
	return (access(buf, mode));
}

fsfuncs remap_funcs = {
	._open = remap_open,
	._lstat = remap_lstat,
	._isdir = remap_isdir,
	._linkcount = remap_linkcount,
	._getdir = remap_getdir,
	._link = remap_link,
	._realBasename = remap_realBasename,
	._unlink = remap_unlink,
	._rename = remap_rename,
	._mkdir = remap_mkdir,
	._rmdir = remap_rmdir,
	._access = remap_access,
};
