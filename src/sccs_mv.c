/* Copyright (c) 1999 Andrew Chang */
#include "system.h"
#include "sccs.h"
WHATSTR("%W%");

private	char	*getRelativeName(char *, project *proj);
private	void	rmDir(char *);
private	int	mv(char *, char *);
private	int	update_idcache(sccs *s, char *old, char *new);

private char *
mkXfile(char *sfile, char type)
{
	char *p, *tmp;

	tmp = strdup(sfile);
	p = strrchr(tmp, '/');
	p = p ? &p[1]: sfile;
	assert(*p == 's');
	*p = type;
	return (tmp);
}

int
sccs_mv(char *name, char *dest, int isDir, int isDelete)
{
	char 	*p, *q, *t, *destfile, *oldpath, *newpath;
	char	*gfile, *sfile;
	char	buf[1024], commentBuf[MAXPATH*2];
	sccs	*s;
	delta	*d;
	int	error = 0;
	int	flags = SILENT|DELTA_FORCE;

//fprintf(stderr, "sccs_mv(%s, %s, %d, %d)\n", name, dest, isDir, isDelete);
	unless (s = sccs_init(name, INIT_NOCKSUM, 0)) return (1);
	unless (HAS_SFILE(s)) {
		fprintf(stderr, "sccsmv: not an SCCS file: %s\n", name);
		sccs_free(s);
		return (1);
	}

	if (!HAS_PFILE(s) && S_ISREG(s->mode) && IS_WRITABLE(s)) {
		fprintf(stderr,
		    "sccsmv: %s is writable but not edited\n",
		    s->gfile);
		return (1);
	}

	/* XXX - shouldn't this call sccs_clean()? */
	if (HAS_PFILE(s) && !HAS_GFILE(s)){
		unlink(s->pfile);	
		s->state &= ~S_PFILE;
	}

	if (isDir) {
		sprintf(buf, "%s/%s", dest, basenm(s->gfile));
	} else {
		strcpy(buf, dest);
	}
	t = destfile = name2sccs(buf);

	sfile = strdup(sPath(t, 0));
	gfile = sccs2name(t);
	t = 0;

	if (exists(sfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", sfile);
		return (1);
	}
	if (exists(gfile)) {
		fprintf(stderr, "sccsmv: destination %s exists\n", gfile);
		return (1);
	}
	/* close the file before we move it - win32 restriction */
	sccs_close(s);
	oldpath = getRelativeName(name, s->proj);
	newpath = getRelativeName(destfile, s->proj);
	if ((strlen(oldpath) > 14) && strneq(oldpath, "BitKeeper/etc/", 14) ||
	    (strlen(newpath) > 14) && strneq(newpath, "BitKeeper/etc/", 14)) {
			sccs	*sc;
			char 	buf1[MAXKEY], buf2[MAXKEY], *p;
			delta	*d1 = findrev(s, "1.0"), *d2;

			assert(d1);
			sccs_sdelta(s, d1, buf1);

			/* check if it has the changset based root key */
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
			

			if (streq(buf1, buf2)) {
				fprintf(stderr,
					"sccsmv: %s -> %s:\n"
					"moving/removing file in "
					"BitKeeper/etc is ileggal\n",
					oldpath, newpath);
				return (1);
			}
	}
	if (isDelete) {
		sprintf(commentBuf, "Delete: %s", oldpath);
	} else {
		sprintf(commentBuf, "Rename: %s -> %s", oldpath, newpath);
	}

	error = mv(s->sfile, sfile);

	/*
	 * move the d.file
	 */
	p = mkXfile(s->sfile, 'd');
	if (!error && exists(p)) {
		q = mkXfile(sfile, 'd');
		error = mv(p, q);
		free(q);
	}
	free(p);

//fprintf(stderr, "mv(%s, %s) = %d\n", s->sfile, sfile, error);
	if (!error && (error = update_idcache(s, oldpath, newpath))) {
		fprintf(stderr, "Idcache failure\n");
		goto out;
	}
	/* This is kind of bogus, we don't move the sfile back. */
	if (!error && exists(s->gfile)) error = mv(s->gfile, gfile);
	if (HAS_PFILE(s) && !error) {
		error = mv(s->pfile, q = mkXfile(sfile, 'p'));
		free(q);
	}
	if (error) goto out;

	/*
	 * Remove the parent directory of "name",
	 * If it is empty after the moves.
	 * XXX TODO: for split root, check the G tree too..
	 */
	p = strrchr(s->sfile, '/');
	if (p) {
		*p = 0;
		if (emptyDir(s->sfile)) rmDir(s->sfile);
		q = strrchr(s->sfile, '/');
		*p = '/';
		if (q) {
			*q = 0;
			if (emptyDir(s->sfile)) rmDir(s->sfile);
			*q = '/';
		} else {
			if (emptyDir(".")) rmDir(".");
		}
	}

	/*
	 * XXX TODO: we should store the rename comment
	 * somewhere, such as a .comment file ?
	 */
	if (HAS_PFILE(s) && !isDelete) goto out;
	sccs_free(s);
	/* For split root config; We recompute sfile here */
	/* we don't want the sPath() adjustment		  */
	free(sfile);
	sfile = name2sccs(destfile);
	unless (s = sccs_init(sfile, 0, 0)) { error++; goto out; }
	unless (HAS_PFILE(s)) {
		if (sccs_get(s, 0, 0, 0, 0, SILENT|GET_EDIT, "-")) {
			error = 1;
			goto out;
		}
		s = sccs_restart(s);
	}

	d = sccs_parseArg(0, 'C', commentBuf, 0);
	unless (s && d) {
		error = 1;
		goto out;
	}

	if (sccs_delta(s, flags, d, 0, 0, 0) == -1) {
		error = 1;
		goto out;
	}

out:	if (s) sccs_free(s);
	free(newpath);
	free(oldpath);
	free(destfile); free(sfile); free(gfile);
	return (error);
}

/*
 * Update the idcache for this file.
 */
private	int
update_idcache(sccs *s, char *old, char *new)
{
	project	*p;
	char	path[MAXPATH*2];
	char	key[MAXKEY];
	kvpair	kv;
	char	*t;
	FILE	*f;
	MDBM	*idDB;
	extern	char *relativeName(sccs *sc, int withsccs, int mustHaveRmarker);

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
	for (kv = mdbm_first(idDB); kv.key.dsize != 0; kv = mdbm_next(idDB)) {
		fprintf(f, "%s %s\n", kv.key.dptr, kv.val.dptr);
	}
	mdbm_close(idDB);
	fclose(f);
	sprintf(path, "%s/%s", p->root, IDCACHE_LOCK);
	if (sccs_lockfile(path, 16)) {
		fprintf(stderr, "Not updating idcache due to locking.\n");
		fprintf(stderr, "Run \"bk idcache\" to rebuild it.\n");
		return (1);
	}
	sprintf(path, "%s/%s", p->root, IDCACHE);
	unlink(path);
	sprintf(path, "mv %s/%s.new %s/%s", p->root, IDCACHE, p->root, IDCACHE);
	system(path);
	sprintf(path, "%s/%s", p->root, IDCACHE_LOCK);
	unlink(path);
	return (0);
}

private	char *
getRelativeName(char *name, project *proj)
{
	char	*t, *rpath;

	/* TODO: we should cache the root value for faster lookup */
	t = sccs2name(name);
	rpath = strdup(_relativeName(t, 0, 0, 0, proj, 0));
	free(t);
	return rpath;
}

private	int
mv(char *src, char *dest)
{
	debug((stderr, "moving %s -> %s\n", src, dest));
	if (rename(src, dest)) {	/* try mv(1) */

		/* try making the dir and see if that helps */
		mkdirf(dest);
#ifdef WIN32
		/* win32 rename works across devices */
		if (rename(src, dest)) return (1);
#else
		if (rename(src, dest)) { 	/* try mv(1) */
			char	cmd[MAXPATH*2 + 5];
			sprintf(cmd, "/bin/mv %s %s", src, dest);
			if (system(cmd)) return (1);
		}
#endif
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
