/* Copyright (c) 1999 Andrew Chang */
#include "system.h"
#include "sccs.h"
WHATSTR("%Z%%K%");

private	char	*getRelativeName(char *, project *proj);
private	void	rmDir(char *);
private	int	update_idcache(sccs *s, char *old, char *new);
private char	**xfileList(char *sfile);

/*
 * Return TRUE if s has cset derived root key
 */
int
sccs_hasCsetDerivedKey(sccs *s)
{
	sccs	*sc;
	char 	buf1[MAXKEY], buf2[MAXKEY], *p;
	delta	*d1, *d2;

	d1 = findrev(s, "1.0");
	assert(d1);
	sccs_sdelta(s, d1, buf1);

	sprintf(buf2, "%s/%s", s->proj->root, CHANGESET);
	sc = sccs_init(buf2, INIT_SAVEPROJ, s->proj);
	assert(sc);
	d2 = findrev(sc, "1.0");
	assert(d2);
	p = d2->pathname;
	d2->pathname = d1->pathname;
	sccs_sdelta(sc, d2, buf2);
	d2->pathname = p;
	sccs_free(sc);

	return (streq(buf1, buf2));
}

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
sccs_mv(char *name,
	char *dest, int isDir, int isDelete, int isUnDelete, int force)
{
	char 	*t, *destfile, *oldpath, *newpath, *rev;
	char	*sname = 0, *gfile = 0, *sfile = 0, *ogfile = 0, *osfile = 0;
	char	**xlist = NULL;
	char	buf[1024];
	sccs	*s = 0;
	int	error = 0, was_edited = 0, has_diffs = 0;
	int	flags = SILENT|DELTA_FORCE;
	int	i;
	time_t	gtime;
	pfile   pf;

//ttyprintf("sccs_mv(%s, %s, %d, %d, %d)\n", name, dest, isDir, isDelete,force);
	sname = name2sccs(name);
	unless (s = sccs_init(sname, INIT_NOCKSUM|INIT_FIXSTIME, 0)) {
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

	if (!HAS_PFILE(s) && S_ISREG(s->mode) && IS_WRITABLE(s)) {
		fprintf(stderr,
		    "sccsmv: %s is writable but not edited\n",
		    s->gfile);
		goto err;
	}
	if (CSET(s) ||
	    (strneq("BitKeeper/", s->tree->pathname, 10) && !force)) {
		fprintf(stderr, "Will not move BitKeeper file %s\n", name);
		goto err;
	}

	/* XXX - shouldn't this call sccs_clean()? */
	if (HAS_PFILE(s) && !HAS_GFILE(s)){
		unlink(s->pfile);	
		s->state &= ~S_PFILE;
	}

	if (isDir) {
		concat_path(buf, dest, basenm(s->gfile));
	} else {
		strcpy(buf, dest);
	}
	t = destfile = name2sccs(buf);

	sfile = strdup(sPath(t, 0));
	gfile = sccs2name(t);
	t = 0;

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

	if (HAS_PFILE(s)) {
		was_edited = 1;
		has_diffs = sccs_hasDiffs(s, flags, 1);
		if (sccs_read_pfile("mvdir", s, &pf)) {
			error |= 1;
			fprintf(stderr, "%s: bad pfile\n", s->gfile);
			goto out;
		}
	}

	/* close the file before we move it - win32 restriction */
	sccs_close(s);
	error = mv(s->sfile, sfile);

	ogfile = strdup(s->gfile);
	osfile = strdup(s->sfile);
//fprintf(stderr, "mv(%s, %s) = %d\n", s->sfile, sfile, error);
	if (!error && (error = update_idcache(s, oldpath, newpath))) {
		fprintf(stderr, "Idcache failure\n");
		goto out;
	}
	if (error) goto out;
	gtime = s->gtime; /* save this, we need it later */
	sccs_free(s);
	/* For split root config; We recompute sfile here */
	/* we don't want the sPath() adjustment		  */
	free(sfile);
	sfile = name2sccs(destfile);
	unless (s = sccs_init(sfile, 0, 0)) { error++; goto out; }

	if (exists(ogfile)) error = mv(ogfile, gfile);
	
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
			mv(xlist[i], n_path);
			free(n_path);
		}
	}
	freeLines(xlist, free);

	if (isDelete && was_edited) {
		if (has_diffs) {
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
	if (sccs_admin(s, 0, flags, 0, 0, 0, 0, 0, 0, 0, 0)) {
		sccs_whynot("mv", s);
		sccs_free(s);
		return (1);
	}
	if (was_edited) {

		/*
		 * Update the p.file, make sure we preserve -i -x
		 */
		strcpy(pf.oldrev, pf.newrev);
		rev = NULL; /* Next rev will be relative to TOT */
		getedit(s, &rev);
		strcpy(pf.newrev, rev);
		sccs_rewrite_pfile(s, &pf);
		free_pfile(&pf);
	}
	s->gtime = gtime;
	fix_stime(s);
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

private	u32	id_sum;

private void
idsum(u8 *s)
{
	while (*s) id_sum += *s++;
}

/*
 * Update the idcache for this file.
 */
private	int
update_idcache(sccs *s, char *old, char *new)
{
	project	*p;
	char	path[MAXPATH*2];
	char	path2[MAXPATH];
	char	key[MAXKEY];
	kvpair	kv;
	char	*t;
	FILE	*f;
	MDBM	*idDB;

	unless ((p = s->proj) || (p = proj_init(s))) {
		fprintf(stderr,
		    "can't find package root, idcache not updated\n");
		s->proj = p;
		return (1);
	}

	/*
	 * This code ripped off from sfiles -r.
	 */
again:	
	sprintf(path, "%s/%s", p->root, IDCACHE);
	unless (idDB = loadDB(path, 0, DB_KEYFORMAT|DB_NODUPS)) {
		fprintf(stderr, "Creating new idcache.\n");
		idDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	}
	sccs_sdelta(s, sccs_ino(s), key);
	if ((t = mdbm_fetch_str(idDB, key)) &&
	    !streq(t, old) && !streq(t, new)) {
		t = name2sccs(t);
		sprintf(path, "%s/%s", p->root, t);
		unless (exists(path)) {
			fprintf(stderr,
			    "Out of date idcache detected, updating...\n");
			mdbm_close(idDB);
			system("bk idcache");
			goto again;
		}
		fprintf(stderr, "Key %s exists for %s\n", key, t);
		mdbm_close(idDB);
		return (1);
	}
	mdbm_store_str(idDB, key, new, MDBM_REPLACE);
	sprintf(path, "%s/%s.new", p->root, IDCACHE);
	unless (f = fopen(path, "w")) {
		perror(path);
		mdbm_close(idDB);
		return (1);
	}
	fprintf(f,
"# This is a BitKeeper cache file.\n\
# If you suspect that the file is corrupted, simply remove it and \n\
# and it will be rebuilt as needed.\n\
# The format of the file is <ID> <PATHNAME>\n\
# The file is used for performance during makepatch/takepatch commands.\n");
	id_sum = 0;
	for (kv = mdbm_first(idDB); kv.key.dsize != 0; kv = mdbm_next(idDB)) {
		fprintf(f, "%s %s\n", kv.key.dptr, kv.val.dptr);
		idsum(kv.key.dptr);
		idsum(kv.val.dptr);
		idsum(" \n");
	}
	mdbm_close(idDB);
	fprintf(f, "#$sum$ %u\n", id_sum);
	fclose(f);
	sprintf(path, "%s/%s", p->root, IDCACHE_LOCK);
	if (sccs_lockfile(path, 16, 0)) {
		fprintf(stderr, "Not updating idcache due to locking.\n");
		fprintf(stderr, "Run \"bk idcache\" to rebuild it.\n");
		return (1);
	}
	sprintf(path, "%s/%s", p->root, IDCACHE);
	unlink(path);
	sprintf(path2, "%s/%s.new", p->root, IDCACHE);
	sprintf(path, "%s/%s", p->root, IDCACHE);
	sys("mv", path2, path, SYS);
	sprintf(path, "%s/%s", p->root, IDCACHE_LOCK);
	sccs_unlockfile(path);
	sprintf(path, "%s/%s", p->root, IDCACHE);
	chmod(path, GROUP_MODE);
	return (0);
}

private	char *
getRelativeName(char *name, project *proj)
{
	char	*t, *rpath;

	/* TODO: we should cache the root value for faster lookup */
	t = sccs2name(name);
	rpath = strdup(_relativeName(t, 0, 0, 0, 0, proj, 0));
	free(t);
	return rpath;
}

/*
 * XXX TODO move this to the port directory
 */
int
mv(char *src, char *dest)
{
	debug((stderr, "moving %s -> %s\n", src, dest));
	if (rename(src, dest)) {
		/* try making the dir and see if that helps */
		mkdirf(dest);
		if (rename(src, dest)) {
#ifndef WIN32
			if (fileCopy(src, dest)) return (1);
			if (unlink(src)) return (1);
#else
			return (1);
#endif
		}
	}
	return (0);
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
 */
private char **
xfileList(char *sfile)
{
	char	buf[MAXPATH];
	char 	*dir, *p, *xname;
	char	**xlist = NULL;
	DIR	*dh;
	struct  dirent *e;

	strcpy(buf, sfile);
	dir =  dirname(buf);
	p = strrchr(sfile, '/');
	assert(p);
	assert(p[1] == 's');
	xname = &p[2];

	dh = opendir(dir);
	unless (dh) return (0);
	while ((e = readdir(dh)) != NULL) {
		char *q;

		q = strrchr(e->d_name, '@');
		if (q) *q = 0;
		if (streq(xname, &(e->d_name[1]))) {
			if (q) *q = '@';
			xlist = addLine(xlist,
					aprintf("%s/%s", dir, e->d_name));
		}
	}
	closedir(dh);
	return (xlist);
}
