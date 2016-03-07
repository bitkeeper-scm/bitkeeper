/*
 * Copyright 2000-2003,2005-2006,2008-2016 BitMover, Inc
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
#include "resolve.h"


/* global state for converge operations */
typedef struct {
	MDBM	*idDB;
	u32	iflags;		/* sccs_init flags */
	opts	opts;		/* resolve options */
} State;

struct files {
	char	*file;
	char	*opts;
} Files[] = {{ "BitKeeper/etc/aliases", "-hs"},
	     { ATTR, "ATTR"},
	     { "BitKeeper/etc/collapsed", "-s" },
	     { "BitKeeper/etc/gone", "-s" },
	     { "BitKeeper/etc/ignore", "-s" },
	     { IGNOREPOLY, "-s" },
	     { 0, 0 }
};

private	void	converge(State *g, char *gfile, char *opts);
private	void	merge(State *g, char *gfile, char *pathname, char *opts);
private	sccs	*copy_to_resync(State *g, sccs *s);
private	void	free_slot(State *g, sccs *s);

int
isConvergeFile(char *file)
{
	struct files	*cur;

	for (cur = Files; cur->file; cur++) {
		if (streq(cur->file, file)) return (1);
	}
	return (0);
}
/*
 * For certain files in BitKeeper/etc we automerge the contents in
 * takepatch.  Also since these files can be created on the first use
 * we need to make sure that files created in parallel are merged and
 * the oldest file is preserved.  This is so parallel merges will
 * converge on the same answer.
 */
void
converge_hash_files(void)
{
	FILE	*f;
	char	*t;
	int	i;
	char	*bn, *gfile;
	State	*g;

	/* everything in this file is run from the RESYNC dir */
	chdir(ROOT2RESYNC);
	g = new(State);
	g->iflags = INIT_NOCKSUM|INIT_MUSTEXIST|SILENT;

	/*
	 * Find all files in RESYNC that may contain a merge conflict for
	 * the files above and merge them.
	 */
	f = popen("bk gfiles BitKeeper/etc BitKeeper/deleted", "r");
	assert(f);
	while (gfile = fgetline(f))  {
		/* find basename of file with deleted stuff stripped */
		bn = basenm(gfile);
		if (strneq(bn, ".del-", 5)) bn += 5;
		if (t = strchr(bn, '~')) *t = 0;
		for (i = 0; Files[i].file; i++) {
			if (streq(bn, basenm(Files[i].file))) {
				if (t) *t = '~'; /* restore gfile */
				merge(g, gfile, Files[i].file, Files[i].opts);
				break;
			}
		}
	}
	pclose(f);

	/*
	 * Now for each file, check to see if we need to converge multiple
	 * versions.
	 */
	for (i = 0; Files[i].file; i++) {
		converge(g, Files[i].file, Files[i].opts);
	}

	if (g->idDB) {
		idcache_write(0, g->idDB);
		mdbm_close(g->idDB);
	}
	free(g);
	chdir(RESYNC2ROOT);
}

/*
 * Automerge any updates before converging the inodes.
 *
 * as long as 'gfile' was created at the path 'pathname', then
 * merge with 'bk merge -s gfile'
 */
private void
merge(State *g, char *gfile, char *pathname, char *opts)
{
	char	*sfile = name2sccs(gfile);
	char	*t;
	int	rc;
	sccs	*s;
	resolve	*rs;
	char	rootkey[MAXKEY];

	/* skip files with no conflicts */
	unless (xfile_exists(sfile, 'r')) {	 /* rfile */
		unless (xfile_exists(sfile, 'm')) goto out;
	}

	/* only merge file if it is for the right pathname */
	s = sccs_init(sfile, g->iflags);
	sccs_close(s);
	unless (streq(PATHNAME(s, sccs_ino(s)), pathname)) {
		sccs_free(s);
		goto out;
	}
	sccs_sdelta(s, sccs_ino(s), rootkey);
	rs = resolve_init(&g->opts, s);

	/* resolve rename conflicts */
	if (rs->gnames) {
		unless (g->idDB) g->idDB = loadDB(IDCACHE, 0, DB_IDCACHE);

		if (streq(rs->gnames->remote, pathname)) {
			/* must keep remote copy in place */ 
			t = name2sccs(pathname);
			if (!streq(gfile, pathname) &&
			    (s = sccs_init(t, g->iflags))) {
				/*
				 * Some other file is in the way.
				 * Get rid of it.
				 */
				free_slot(g, s);
				sccs_free(s);
			}
		} else  if (rs->dname) {
			/* try to resolve to "natural" name */
			t = name2sccs(rs->dname);
			if (!streq(gfile, rs->dname) && exists(t)) {
				/*
				 * conflicts with another sfile, oh well
				 * just delete it.
				 */
				free(t);
				t = sccs_rmName(s);
			}
		} else {
			/* resolve conflict by deleting */
			t = sccs_rmName(s);
		}
		move_remote(rs, t); /* fine if this doesn't move */

		/* mark that it moved */
		idcache_item(g->idDB, rootkey, s->gfile);
	}

	/* handle contents conflicts */
	if (rs->revs) {
		/*
		 * Both remote and local have updated the file.
		 * We automerge here, saves trouble later.
		 */
		rc = sys("bk", "get", "-qeM", s->gfile, SYS);
		assert(!rc);
		if (streq(opts, "ATTR")) {
			attr_write(s->gfile);
		} else {
			rc = sysio(0, s->gfile, 0,
			    "bk", "merge", opts, s->gfile, SYS);
			assert(!rc);
		}
		rc = sys("bk", "ci", "-qPyauto-union", s->gfile, SYS);
		assert(!rc);

		/* delete rfile */
		xfile_delete(s->sfile, 'r');
	}
	resolve_free(rs);	/* free 's' too */
out:	free(sfile);
}

/*
 * The goal here is to converge on a single copy of a sfile when we
 * may have had multiple sfiles created in parallel.  So we look for
 * these create/create conflicts and resolve them by picking the
 * oldest sfile and merging in the contents of the other sfile.
 *
 * Note this function is distributed so it assumes both sides of a
 * merge have already been converged, so there is no need to look at
 * any files in the deleted directory.  We have already decided to
 * ignore them some other time.
 */
private void
converge(State *g, char *gfile, char *opts)
{
	sccs	*skeep, *srm, *s;
	int	rc;
	resolve	*rs;
	char	*sfile = name2sccs(gfile);
	char	key_keep[MAXKEY];
	char	key_rm[MAXKEY];
	char	buf[MAXPATH];
	char	tmp[MAXPATH];

	concat_path(buf, RESYNC2ROOT, sfile);	/* ../sfile */
	unless (exists(sfile) && exists(buf)) {
		/* no conflict, we are done */
		free(sfile);
		return;
	}

	skeep = sccs_init(buf, INIT_MUSTEXIST);
	assert(skeep);
	sccs_sdelta(skeep, sccs_ino(skeep), key_keep);
	srm = sccs_init(sfile, INIT_MUSTEXIST);
	assert(srm);
	sccs_sdelta(srm, sccs_ino(srm), key_rm);

	if (streq(key_keep, key_rm)) {
		/* same rootkey, no conflict */
		sccs_free(skeep);
		sccs_free(srm);
		free(sfile);
		return;
	}
	sccs_clean(skeep, SILENT);
	sccs_clean(srm, SILENT);

	/* pick the older sfile */
	if ((DATE(srm, sccs_ino(srm)) < DATE(skeep, sccs_ino(skeep))) ||
	    ((DATE(srm, sccs_ino(srm)) == DATE(skeep, sccs_ino(skeep))) &&
		(strcmp(key_rm, key_keep) < 0))) {
		s = skeep;
		skeep = srm;
		srm = s;
		sccs_sdelta(skeep, sccs_ino(skeep), key_keep);
		sccs_sdelta(srm, sccs_ino(srm), key_rm);
	}
	unless (g->idDB) g->idDB = loadDB(IDCACHE, 0, DB_IDCACHE);

	/* copy both sfiles to RESYNC */
	skeep = copy_to_resync(g, skeep);
	srm = copy_to_resync(g, srm);

	/* get contents of old version */
	bktmp(tmp);
	rc = sccs_get(srm, "+", 0, 0, 0, SILENT, tmp, 0);
	assert(!rc);
	sccs_free(srm);

	/* Check if something is in the way (other than skeep) */
	if (s = sccs_init(sfile, g->iflags)) {
		sccs_sdelta(s, sccs_ino(s), buf);
		unless (streq(buf, key_keep)) free_slot(g, s);
		sccs_free(s);
	}

	/* if skeep is not in the right place move it */
	unless (samepath(skeep->gfile, gfile)) {
		rs = resolve_init(&g->opts, skeep);
		assert(!rs->revs);
		assert(!rs->snames);
		move_remote(rs, sfile);
		rs->s = 0;
		idcache_item(g->idDB, key_keep, gfile);
		resolve_free(rs);
	}
	sccs_free(skeep);	/* done with skeep */
	free(sfile);

	if (streq(opts, "ATTR")) {
		/*
		 * We don't need to record these converge merges in
		 * the attribute file.  Just pick the oldest and
		 * delete the other file.
		 */
		unlink(tmp);
		return;
	}

	/* Add other files contents as branch of 1.0 */
	rc = sys("bk", "_get", "-egqr1.0", gfile, SYS);
	assert(!rc);
	fileMove(tmp , gfile);		/* srm contents saved above */
	rc = sys("bk", "ci", "-qdPyconverge", gfile, SYS);
	assert(!rc);

	/*
	 * merge in new tip
	 *
	 * The get -M may fail if the ci above doesn't create a new
	 * delta, because the old file was empty.  In that case there is
	 * nothing to union.
	 */
	if (!sys("bk", "get", "-qeM", gfile, SYS)) {
		if (streq(opts, "ATTR")) {
			attr_write(gfile);
		} else {
			rc = sysio(0, gfile, 0,
			    "bk", "merge", opts, gfile, SYS);
			assert(!rc);
		}
		rc = sys("bk", "ci", "-qPyauto-union-files", gfile, SYS);
		assert(!rc);
	}
}

/*
 * Assume we are in the repository root and move the sfile
 * for the given sccs* to the RESYNC directory.
 */
private sccs *
copy_to_resync(State *g, sccs *s)
{
	resolve	*rs;
	sccs	*snew;
	char	*rmName;
	char	rootkey[MAXKEY];

	/* already in RESYNC? */
	if (proj_isResync(s->proj)) return (s);

	/* Is it already there? */
	sccs_sdelta(s, sccs_ino(s), rootkey);

	if (snew = sccs_keyinit(0, rootkey, g->iflags, g->idDB)) {
		/* Found this rootkey in RESYNC already */
		sccs_free(s);
		return (snew);
	}

	/* so we need copy to RESYNC and then move to the deleted dir */
	fileCopy(s->sfile, "BitKeeper/etc/SCCS/s.converge-tmp");
	sccs_free(s);
	s = sccs_init("BitKeeper/etc/SCCS/s.converge-tmp", g->iflags);
	rs = resolve_init(&g->opts, s);
	assert(!rs->revs);
	assert(!rs->snames);
	rmName = sccs_rmName(s);
	move_remote(rs, rmName);
	resolve_free(rs);
	s = sccs_init(rmName, g->iflags);
	idcache_item(g->idDB, rootkey, s->gfile);
	free(rmName);
	return(s);
}

/* just delete the sfile to free up its current location */
private void
free_slot(State *g, sccs *s)
{
	resolve	*rs;
	char	*rmName, *t;
	char	key[MAXKEY];

	rs = resolve_init(&g->opts, s);
	sccs_sdelta(s, sccs_ino(s), key);
	rmName = sccs_rmName(s);
	move_remote(rs, rmName);
	t = sccs2name(rmName);
	free(rmName);
	idcache_item(g->idDB, key, t);
	free(t);
	rs->s = 0;
	resolve_free(rs);
}
