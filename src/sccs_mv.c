/* Copyright (c) 1999 Andrew Chang */
#include "system.h"
#include "sccs.h"

private	char	*getRelativeName(char *, project *proj);
private	void	rmDir(char *);
private	int	update_idcache(sccs *s, char *old, char *new);
private char	**xfileList(char *sfile);

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
sccs_mv(char	*name,
	char	*dest, int isDir, int isDelete, int isUnDelete, int force)
{
	char 	*t, *destfile, *oldpath, *newpath, *rev;
	char	*sname = 0, *gfile = 0, *sfile = 0, *ogfile = 0, *osfile = 0;
	char	**xlist = NULL;
	sccs	*s = 0;
	delta	*d;
	int	error = 0, was_edited = 0, has_diffs = 0;
	int	i, flags;
	pfile   pf;
	struct	utimbuf	ut;
	char	buf[1024];

//ttyprintf("sccs_mv(%s, %s, %d, %d, %d)\n", name, dest, isDir, isDelete,force);
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

	unless (proj_samerepo(sname, destfile)) goto err;

	sfile = strdup(t);
	gfile = sccs2name(t);
	t = 0;

	unless (force) {
		d = sccs_top(s);
		t = d->pathname;	/* where file is now */
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
		unlink(s->pfile);
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

	if (bk_badFilename(newpath)) {
		fprintf(stderr, "sccsmv: destination cannot be "
		    "named: %s\n", newpath);
		free(oldpath);
		free(newpath);
		goto err;
	}

	if (HAS_PFILE(s)) {
		was_edited = 1;
		has_diffs = sccs_hasDiffs(s, SILENT, 1);
		if (sccs_read_pfile("mvdir", s, &pf)) {
			error |= 1;
			fprintf(stderr, "%s: bad pfile\n", s->gfile);
			goto out;
		}
	}

	/* close the file before we move it - win32 restriction */
	sccs_close(s);
	error = fileMove(s->sfile, sfile);

	ogfile = strdup(s->gfile);
	osfile = strdup(s->sfile);
//fprintf(stderr, "mv(%s, %s) = %d\n", s->sfile, sfile, error);
	if (!error && (error = update_idcache(s, oldpath, newpath))) {
		fprintf(stderr, "Idcache failure\n");
		goto out;
	}
	if (error) goto out;
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
	
	/*
	 * Clean up or move the helper file
	 * a) c.file[@rev] 
	 * b) d.file
	 * c) p.file
	 */
	xlist = xfileList(osfile);
	EACH(xlist) {
		char	nbuf[MAXPATH];
		char	*obase, *ndir, *nbase, *n_path, *rev;
		char	prefix;

		obase = basenm(xlist[i]);
		prefix =obase[0];
		/* Warn if not a c.file or a d.file */
		unless (strchr("cdp", prefix)) {
			fprintf(stderr,
			    "Warning: unexpected helper file: %s, skipped\n",
			    xlist[i]);
			continue;
		}
		if (isDelete && (prefix != 'd')) {
			unlink(xlist[i]);
		} else {
			strcpy(nbuf, sfile);
			nbase = basenm(sfile);
			ndir = dirname(nbuf);
			/*
			 * subsitude old dir with new dir
			 * subsitude old base name with new base name
			 * if c.file@rev, glue on the @rev part
			 */
			rev = NULL;
			if (prefix == 'c') rev = strrchr(obase, '@');
			n_path = aprintf("%s/%c.%s%s",
				ndir, prefix, &nbase[2], rev ? rev : "");
			fileMove(xlist[i], n_path);
			free(n_path);
		}
	}
	freeLines(xlist, free);

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
		unlink(s->pfile);
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
	if (ogfile) free(ogfile);
	if (osfile) free(osfile);
	free(newpath);
	free(oldpath);
	free(destfile); free(sfile); free(gfile);
	free(sname);
	return (error);
}

/*
 * Update the idcache for this file.
 */
private	int
update_idcache(sccs *s, char *old, char *new)
{
	char	*root;
	int	rc;
	char	*t;
	MDBM	*idDB;
	char	path[MAXPATH*2];
	char	key[MAXKEY];

	unless (root = proj_root(s->proj)) {
		fprintf(stderr,
		    "can't find package root, idcache not updated\n");
		return (1);
	}

	/*
	 * This code ripped off from sfiles -r.
	 */
again:	
	sprintf(path, "%s/%s", root, getIDCACHE(s->proj));
	unless (idDB = loadDB(path, 0, DB_IDCACHE)) {
		fprintf(stderr, "Creating new idcache.\n");
		idDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	}
	sccs_sdelta(s, sccs_ino(s), key);
	if ((t = mdbm_fetch_str(idDB, key)) &&
	    !streq(t, old) && !streq(t, new)) {
		t = name2sccs(t);
		sprintf(path, "%s/%s", root, t);
		unless (exists(path)) {
			fprintf(stderr,
			    "Out of date idcache detected, updating...\n");
			mdbm_close(idDB);
			sccs_reCache(0);
			goto again;
		}
		fprintf(stderr, "Key %s exists for %s\n", key, t);
		mdbm_close(idDB);
		return (1);
	}
	mdbm_store_str(idDB, key, new, MDBM_REPLACE);
	rc = idcache_write(s->proj, idDB);
	mdbm_close(idDB);
	return (rc);
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

/*
 * Get a list of c.file[@rev] files
 * XXX - this should NOT be done like this, there should be a generic API
 * for managing the c.files.
 */
private char **
xfileList(char *sfile)
{
	char 	*dir, *p, *q, *xname, **d;
	char	**xlist = NULL;
	int	i;
	char	buf[MAXPATH];

	strcpy(buf, sfile);
	dir =  dirname(buf);
	p = strrchr(sfile, '/');
	assert(p);
	assert(p[1] == 's');
	xname = &p[2];
	unless (d = getdir(dir)) return (0);
	EACH (d) {
		if (q = strrchr(d[i], '@')) *q = 0;
		if (streq(xname, &(d[i][1]))) {
			if (q) *q = '@';
			xlist = addLine(xlist, aprintf("%s/%s", dir, d[i]));
		}
	}
	freeLines(d, free);
	return (xlist);
}
