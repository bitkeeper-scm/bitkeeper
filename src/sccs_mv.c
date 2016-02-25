/*
 * Copyright 1999-2016 BitMover, Inc
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
#include "sccs.h"

private	char	*getRelativeName(char *, project *proj);
private	void	rmDir(char *);

/*
 * Give a pathname to a file or a dir, remove the SCCS dir, the parent,
 * etc, up to the root of the repository.
 */
void
sccs_rmEmptyDirs(char *path)
{
	char	*p = 0;
	char	*q;
	
	unless (isdir(path)) p = strrchr(path, '/');
	do {
		if (p) *p = 0;
		if (!streq(path, "BitKeeper/etc") &&
		    !streq(path, "BitKeeper/deleted") &&
		    emptyDir(path)) {
			rmDir(path);
		} else {
			if (p) *p = '/';
			return;
		}
		q = strrchr(path, '/');
		if (p) *p = '/';
		p = q;
	} while (p);
}

int
sccs_mv(char	*name, char *dest,
	int	isDir, int isDelete, int isUnDelete, int force, MDBM *idDB)
{
	char 	*t, *destfile, *oldpath, *newpath, *rev;
	char	*sname = 0, *gfile = 0, *sfile = 0, *ogfile = 0, *osfile = 0;
	sccs	*s = 0;
	ser_t	d;
	int	error = 0, was_edited = 0, has_diffs = 0;
	int	flags;
	pfile   pf;
	struct	utimbuf	ut;
	char	buf[MAXKEY];

	T_SCCS("(%s, %s, %d, %d, %d)", name, dest, isDir, isDelete,force);
	sname = name2sccs(name);
	unless (s = sccs_init(sname, INIT_NOCKSUM)) {
err:		if (sname) free(sname);
		if (sfile) free(sfile);
		if (gfile) free(gfile);
		if (s) sccs_free(s);
		return (1);
	}
	unless (HAS_SFILE(s)) {
		fprintf(stderr, "sccsmv: not an SCCS file: %s\n", name);
		goto err;
	}

	if (!HAS_PFILE(s) && S_ISREG(s->mode) && WRITABLE(s)) {
		fprintf(stderr,
		    "sccsmv: %s is writable but not edited\n",
		    s->gfile);
		goto err;
	}
	if (CSET(s)) {
		fprintf(stderr, "Will not move ChangeSet file\n");
		goto err;
	}
	if (isDir) {
		concat_path(buf, dest, basenm(s->gfile));
	} else {
		strcpy(buf, dest);
	}
	t = destfile = name2sccs(buf);

	unless (proj_samerepo(sname, destfile, 0)) goto err;

	sfile = strdup(t);
	gfile = sccs2name(t);
	t = 0;

	unless (force) {
		d = sccs_top(s);
		t = PATHNAME(s, d);	/* where file is now */
		if (strneq("BitKeeper/", t, 10) &&
		    !(strneq("BitKeeper/triggers/", t, 19) ||
		      strneq("BitKeeper/deleted/", t, 18))) {
			fprintf(stderr, "Will not move BitKeeper file %s\n",
			    name);
			goto err;
		}
		t = proj_relpath(s->proj, gfile); /* where file is going */
		if (strneq("BitKeeper/", t, 10) &&
		    !(strneq("BitKeeper/triggers/", t, 19) ||
		      strneq("BitKeeper/deleted/", t, 18))) {
			fprintf(stderr, "Will not move to BitKeeper file %s\n",
			    name);
			free(t);
			goto err;
		}
		free(t);
	}

	/* XXX - shouldn't this call sccs_clean()? */
	if (HAS_PFILE(s) && !HAS_GFILE(s)){
		xfile_delete(s->gfile, 'p');
		s->state &= ~S_PFILE;
	}

	if (exists(sfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", sfile);
		goto err;
	}
	if (exists(gfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", gfile);
		goto err;
	}
	oldpath = getRelativeName(sname, s->proj);
	newpath = getRelativeName(destfile, s->proj);

	if (bk_badFilename(s->proj, newpath)) {
		fprintf(stderr, "sccsmv: destination cannot be "
		    "named: %s\n", newpath);
		free(oldpath);
		free(newpath);
		goto err;
	}

	if (HAS_PFILE(s)) {
		was_edited = 1;
		has_diffs = sccs_hasDiffs(s, SILENT, 1);
		if (sccs_read_pfile(s, &pf)) {
			error |= 1;
			fprintf(stderr, "%s: bad pfile\n", s->gfile);
			goto out;
		}
	}

	/* close the file before we move it - win32 restriction */
	sccs_close(s);
	error = sfile_move(s->proj, s->sfile, sfile);

	ogfile = strdup(s->gfile);
	osfile = strdup(s->sfile);
//fprintf(stderr, "mv(%s, %s) = %d\n", s->sfile, sfile, error);
	if (error) goto out;
	/* update idcache */
	sccs_sdelta(s, sccs_ino(s), buf);
	idcache_item(idDB, buf, newpath);
	sccs_free(s);
	/* For split root config; We recompute sfile here */
	/* we don't want the sPath() adjustment		  */
	free(sfile);
	sfile = name2sccs(destfile);
	unless (s = sccs_init(sfile, 0)) { error++; goto out; }

	if (exists(ogfile) && !(error = fileMove(ogfile, gfile))) {
		ut.actime = ut.modtime = time(0);
		utime(gfile, &ut);
	}
	if (isDelete && was_edited) {
		if (has_diffs) {
			flags = SILENT|DELTA_FORCE;

			if (sccs_delta(s, flags, 0, 0, 0, 0) == -1) {
				sccs_whynot("mv", s);
				sccs_free(s);
				return (1);
			}
		}
		unlink(s->gfile);
		xfile_delete(s->gfile, 'p');
		was_edited = 0;
	}
	flags = isDelete ? ADMIN_DELETE : ADMIN_NEWPATH;
	if (sccs_adminFlag(s, flags)) {
		sccs_whynot("mv", s);
		sccs_free(s);
		return (1);
	}
	if (was_edited) {
		/*
		 * Update the p.file, make sure we preserve -i -x
		 */
		free(pf.oldrev);
		pf.oldrev = strdup(pf.newrev);
		rev = NULL; /* Next rev will be relative to TOT */
		sccs_getedit(s, &rev);
		free(pf.newrev);
		pf.newrev = strdup(rev);
		sccs_rewrite_pfile(s, &pf);
		free_pfile(&pf);
	}
	sccs_setStime(s, 0);
	unless (isUnDelete) sccs_rmEmptyDirs(osfile);

out:	if (s) sccs_free(s);
	/* honor checkout modes when running on behalf of unrm */
	if (isUnDelete) {
		s = sccs_init(sfile, INIT_NOCKSUM);
		do_checkout(s, 0, 0);
		sccs_free(s);
	}
	if (ogfile) free(ogfile);
	if (osfile) free(osfile);
	free(newpath);
	free(oldpath);
	free(destfile); free(sfile); free(gfile);
	free(sname);
	return (error);
}

private	char *
getRelativeName(char *name, project *proj)
{
	char	*p, *t, *rpath;

	/* TODO: we should cache the root value for faster lookup */
	t = sccs2name(name);
	if (p = _relativeName(t, 0, 0, 0, proj)) {
		rpath = strdup(p);
	} else {
		rpath = 0;
	}
	free(t);
	return rpath;
}

private	void
rmDir(char *dir)
{
	if (streq(".", dir) || samepath(".", dir)) return;
	debug((stderr, "removing %s\n", dir));
	rmdir(dir);
}
