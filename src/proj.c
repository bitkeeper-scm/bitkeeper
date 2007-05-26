#include "system.h"
#include "sccs.h"
#include "logging.h"

/*
 * This file contains a series of accessor functions for the project
 * struct that caches information about the current repository.  There
 * is a global project struct that always matches the current
 * directory.  This struct is private to proj.c and it used whenever 0
 * is passed to these functions.
 */

/*
 * Don't treat chdir() special here.
 */
#undef	chdir
#ifdef	WIN32
#define	chdir	nt_chdir
#endif

typedef struct dirlist dirlist;
struct dirlist {
	char	*dir;
	dirlist	*next;
};

struct project {
	char	*root;		/* fullpath root of the project */
	char	*rootkey;	/* Root key of ChangeSet file */
	char	*md5rootkey;	/* MD5 root key of ChangeSet file */
	char	*repoID;	/* RepoID */
	MDBM	*config;	/* config DB */
	project	*rparent;	/* if RESYNC, point at enclosing repo */

	/* per proj cache data */
	char	*bkl;		/* key - filled from lease_bkl() */
	u32	bklbits;	/* LIC_* from license_bklbits() */
	int	casefolding;	/* mixed case file system: FOO == foo */
	int	leaseok;	/* if set, we've checked and have a lease */
	MDBM	*binpool_idx;	/* binpool index file */
	int	binpool_write;	/* binpool index file opened for write? */

	/* checkout state */
	MDBM	*coDB;		/* $coDB{rootkey} = e|g|n */
	char	**bp_getFiles;	/* files we need to fetch and get */
	char	**bp_editFiles;	/* files we need to fetch and edit */

	/* internal state */
	int	refcnt;
	dirlist	*dirs;
};

private char	*find_root(char *dir);

private struct {
	project	*curr;
	project	*last;
	hash	*cache;
	char	cwd[MAXPATH];
} proj;

private project *
projcache_lookup(char *dir)
{
	project	*ret = 0;
	project	**p;

	unless (proj.cache) proj.cache = hash_new(HASH_MEMHASH);
	if (p = hash_fetchStr(proj.cache, dir)) ret = *p;
	return (ret);
}

private void
projcache_store(char *dir, project *p)
{
	unless (streq(dir, p->root)) {
		dirlist	*dl;

		new(dl);
		dl->dir = strdup(dir);
		dl->next = p->dirs;
		p->dirs = dl;
	}
	hash_store(proj.cache, dir, strlen(dir)+1, &p, sizeof(p));
}

private void
projcache_delete(project *p)
{
	dirlist	*dl;

	hash_deleteStr(proj.cache, p->root);
	dl = p->dirs;
	while (dl) {
		dirlist	*tmp = dl;

		hash_deleteStr(proj.cache, dl->dir);

		free(dl->dir);
		dl = dl->next;
		free(tmp);
	}
}

/*
 * Return a project struct for the project that contains the given
 * directory.
 */
project *
proj_init(char *dir)
{
	project	*ret;
	char	*root;
	char	*t;
	char	*fdir, *cwd;
	char	buf[MAXPATH];

	if (IsFullPath(dir)) {
		fdir = dir;
	} else {
		unless (cwd = proj_cwd()) return (0);
		concat_path(buf, cwd, dir);
		fdir = buf;
	}
	if (ret = projcache_lookup(fdir)) goto done;

	/* missed the cache */
	unless (root = find_root(fullname(dir))) return (0);

	unless (streq(root, fdir)) {
		/* fdir is not a root, was root in cache? */
		if (ret = projcache_lookup(root)) {
			/* yes, make a new mapping */
			projcache_store(fdir, ret);
			goto done;
		}
	}
	assert(ret == 0);
	/* Totally new project */
	new(ret);
	ret->root = root;
	ret->casefolding = -1;
	ret->leaseok = -1;

	projcache_store(root, ret);
	unless (streq(root, fdir)) projcache_store(fdir, ret);
done:
	++ret->refcnt;

	if ((t = strrchr(ret->root, '/')) && streq(t, "/RESYNC")) {
		*t = 0;
		ret->rparent = proj_init(ret->root);
		if (ret->rparent && !streq(ret->rparent->root, ret->root)) {
			proj_free(ret->rparent);
			ret->rparent = 0;
		}
		*t = '/';
	}
	return (ret);
}

void
proj_free(project *p)
{
	assert(p);

	assert(p->refcnt > 0);
	unless (--p->refcnt == 0) return;

	if (p->rparent) {
		proj_free(p->rparent);
		p->rparent = 0;
	}
	projcache_delete(p);

	proj_reset(p);
	free(p->root);
	free(p);
}

private char *
find_root(char *dir)
{
	char	*p;
	char	buf[MAXPATH];

	/* This code assumes dir is a full pathname with nothing funny */
	strcpy(buf, dir);
	p = buf + strlen(buf);
	*p = '/';

	/*
	 * Now work backwards up the tree until we find a root marker
	 */
	while (p >= buf) {
		strcpy(++p, BKROOT);
		if (exists(buf))  break;
		if (--p <= buf) {
			/*
			 * if we get here, we hit the beginning
			 * and did not find the root marker
			 */
			return (0);
		}
		/* p -> / in .../foo/SCCS/s.foo.c */
		for (--p; (*p != '/') && (p > buf); p--);
	}
	assert(p >= buf);
	p--;
	*p = 0;
	return (strdup(buf));
}

private project *
curr_proj(void)
{
	unless (proj.curr) proj.curr = proj_init(".");
	return (proj.curr);
}

/*
 * Return the full path to the root directory in the current project.
 * Data is calculated the first time this function in called. And the
 * old data is returned from then on.
 */
char *
proj_root(project *p)
{
	unless (p) p = curr_proj();
	unless (p) return (0);
	assert(p->root);
	return (p->root);
}

/*
 * chdir to the root of the current tree we are in
 */
int
proj_cd2root(void)
{
	char	*root = proj_root(0);

	if (root && (chdir(root) == 0)) {
		strcpy(proj.cwd, root);
		return (0);
	}
	return (-1);
}

/*
 * When given a pathname to a file, this function returns the pathname
 * to the file relative to the current project.  If the file is not under
 * the current project then, NULL is returned.
 * The returned path is allocated with malloc() and the user must free().
 */
char *
proj_relpath(project *p, char *path)
{
	char	*root = proj_root(p);
	int	len;

	assert(root);
	unless (IsFullPath(path)) path = fullname(path);
	len = strlen(root);
	if (pathneq(root, path, len)) {
		if (!path[len]) {
			return (strdup("."));
		} else {
                       assert(path[len] == '/');
                       return(strdup(&path[len+1]));
		}
	} else {
		return (0);
	}
}

/*
 * When given a relative path from the root, turn the path into an absolute
 * path from the root of the file system.
 *
 * Note: returns a malloced pointer that you must not free.  Not safe across
 * multiple calls.
 */
char	*
proj_fullpath(project *p, char *file)
{
	char	*root = proj_root(p);
	static	char *path;

	unless (root) return (0);
	if (path) free(path);	// XXX - if this gets called a lot use a static
	return (path = aprintf("%s/%s", root, file));
}

/*
 * Return a populated MDBM for the config file in the current project.
 */
MDBM *
proj_config(project *p)
{
	unless (p || (p = curr_proj())) p = proj_fakenew();

	if (p->config) return (p->config);
	if (p->rparent) {
		/* If RESYNC doesn't have a config file, then don't use it. */
		p->config = loadConfig(p->root, 1);
		unless (p->config) return (proj_config(p->rparent));
	} else {
		p->config = loadConfig(p->root, 0);
	}
	return (p->config);
}

char *
proj_configval(project *p, char *key)
{
	char	*ret;
	MDBM	*db = proj_config(p);

	assert(db);
	unless (ret = mdbm_fetch_str(db, key)) {
		if (streq(key, "clock_skew")) {
			ret = mdbm_fetch_str(db, "trust_window");
		}
		if (streq(key, "bam_server")) {
			ret = mdbm_fetch_str(db, "binpool_server");
		}
	}
	return (ret ? ret : "");
}

int
proj_configbool(project *p, char *key)
{
	char	*val;
	MDBM	*db = proj_config(p);

	assert(db);
	unless (val = mdbm_fetch_str(db, key)) {
		if (streq(key, "bam_hardlinks")) {
			val = mdbm_fetch_str(db, "binpool_hardlinks");
		}
	}
	unless (val) return (0);
	switch(tolower(*val)) {
	    case '0': if (streq(val, "0")) return (0); break;
	    case '1': if (streq(val, "1")) return (1); break;
	    case 'f': if (strieq(val, "false")) return (0); break;
	    case 't': if (strieq(val, "true")) return (1); break;
	    case 'n': if (strieq(val, "no")) return (0); break;
	    case 'y': if (strieq(val, "yes")) return (1); break;
	    case 'o':
		if (strieq(val, "on")) return (1);
		if (strieq(val, "off")) return (0);
		break;
	}
	fprintf(stderr,
	    "WARNING: config key '%s' should be a boolean.\n"
	    "Meaning of '%s' unknown. Assuming false.\n", key, val);
	return (0);
}

/*
 * returns the checkout state for the current repository
 */
int
proj_checkout(project *p)
{
	MDBM	*db;
	char	*s;

	unless (p || (p = curr_proj())) p = proj_fakenew();
	if (proj_isResync(p)) return (CO_NONE);
	db = proj_config(p);
	assert(db);
	unless (s = mdbm_fetch_str(db, "checkout")) return (CO_NONE);
	if (strieq(s, "get")) return (CO_GET);
	if (strieq(s, "edit")) return (CO_EDIT);
	if (strieq(s, "none")) return (CO_NONE);
	if (strieq(s, "last")) return (CO_LAST);
	fprintf(stderr,
	    "WARNING: config key 'checkout' should be a get or edit.\n"
	    "Meaning of '%s' unknown. Assuming none.\n", s);
	return (CO_NONE);
}


/* Return the root key of the ChangeSet file in the current project. */
char	*
proj_rootkey(project *p)
{
	sccs	*sc;
	FILE	*f;
	char	*ret;
	char	rkfile[MAXPATH];
	char	buf[MAXPATH];

	unless (p || (p = curr_proj())) p = proj_fakenew();

	/*
	 * Use cached copy if available
	 */
	if (p->rootkey) return (p->rootkey);
	if (p->rparent && (ret = proj_rootkey(p->rparent))) return (ret);

	concat_path(rkfile, p->root, "/BitKeeper/log/ROOTKEY");
	if (f = fopen(rkfile, "rt")) {
		fnext(buf, f);
		chomp(buf);
		p->rootkey = strdup(buf);
		fnext(buf, f);
		chomp(buf);
		p->md5rootkey = strdup(buf);
		fclose(f);
	} else {
		concat_path(buf, p->root, CHANGESET);
		if (exists(buf)) {
			sc = sccs_init(buf,
			    INIT_NOCKSUM|INIT_NOSTAT|INIT_WACKGRAPH);
			assert(sc->tree);
			sccs_sdelta(sc, sc->tree, buf);
			p->rootkey = strdup(buf);
			sccs_md5delta(sc, sc->tree, buf);
			p->md5rootkey = strdup(buf);
			sccs_free(sc);
			if (f = fopen(rkfile, "wt")) {
				fputs(p->rootkey, f);
				putc('\n', f);
				fputs(p->md5rootkey, f);
				putc('\n', f);
				fclose(f);
			}
		}
	}
	return (p->rootkey);
}

/* Return the root key of the ChangeSet file in the current project. */
char *
proj_md5rootkey(project *p)
{
	char	*ret;

	unless (p || (p = curr_proj())) p = proj_fakenew();

	unless (p->md5rootkey) proj_rootkey(p);
	if (p->rparent && (ret = proj_md5rootkey(p->rparent))) return (ret);
	return (p->md5rootkey);
}

/*
 * Return the repoID if there is one.
 */
char *
proj_repoID(project *p)
{
	char	*repoID, *file;

	unless (p) p = curr_proj();
	unless (p) return (0);
	if (p->rparent) p = p->rparent;
	if (p->repoID) return (p->repoID);

	file = proj_fullpath(p, REPO_ID);
	unless (repoID = loadfile(file, 0)) {
		mk_repoID(p, file);
		repoID = loadfile(file, 0);
	}
	if (repoID) chomp(repoID);
	p->repoID = repoID;
	return (p->repoID);
}

/*
 * Clear any data cached for the current project root.
 * Call this function whenever the current data is made invalid.
 * When passed an explicit project then only that project is cleared.
 * Otherwise, all projects are flushed.
 */
void
proj_reset(project *p)
{
	if (p) {
		if (p->rootkey) {
			free(p->rootkey);
			p->rootkey = 0;
			free(p->md5rootkey);
			p->md5rootkey = 0;
		}
		if (p->repoID) {
			free(p->repoID);
			p->repoID = 0;
		}
		if (p->config) {
			mdbm_close(p->config);
			p->config = 0;
		}
		if (p->bkl) {
			free(p->bkl);
			p->bkl = 0;
		}
		p->bklbits = 0;
		p->leaseok = -1;
		if (p->coDB) mdbm_close(p->coDB);
		p->coDB = 0;
		if (p->binpool_idx) {
			mdbm_close(p->binpool_idx);
			p->binpool_idx = 0;
		}
	} else {
		/*
		 * Warning: This loop will call proj_reset() on the
		 * some projects multiple times.  Currently harmless.
		 */
		EACH_HASH(proj.cache) proj_reset(*(project **)proj.cache->vptr);
		/* free the current project for purify */
		if (proj.curr) {
			proj_free(proj.curr);
			proj.curr = 0;
		}
		if (proj.last) {
			proj_free(proj.last);
			proj.last = 0;
		}
	}
}

/*
 * proj_chdir() is a wrapper for chdir() that also updates
 * the current default project.
 * We map chdir() to this function by default.
 */
int
proj_chdir(char *newdir)
{
	int	ret;

	ret = chdir(newdir);
	unless (ret) {
		unless (getcwd(proj.cwd, sizeof(proj.cwd))) proj.cwd[0] = 0;
		if (proj.curr) {
			if (proj.last) proj_free(proj.last);
			proj.last = proj.curr;
			proj.curr = 0;
		}
	}
	return (ret);
}

/*
 * proj_cwd() just returns a pointer to the current working directory.
 * That information is saved by proj_chdir() so this function is very
 * fast.
 */
char *
proj_cwd(void)
{
	unless (proj.cwd[0]) {
		if (proj_chdir(".")) return (0);
	}
	return (proj.cwd);
}

/*
 * create a fake project struct that is used for files that
 * are outside any repository.  This is used by the lease code.
 */
project *
proj_fakenew(void)
{
	project	*ret;

	if (ret = projcache_lookup("/")) return (ret);
	new(ret);
	ret->root = strdup("/.");
	ret->rootkey = strdup("SCCS");
	ret->md5rootkey = strdup("SCCS");
	projcache_store("/.", ret);

	return (ret);
}

/*
 * return the BKL.... key as a string
 * Will exit(1) on failure, so the function always returns a key.
 */
char *
proj_bkl(project *p)
{
	char	*errs = 0;

	/*
	 * If we are outside of any repository then we must get a
	 * licence from the global config or a license server.
	 */
	unless (p || (p = curr_proj())) p = proj_fakenew();

	if (p->bkl) return (p->bkl);
	unless (p->bkl = lease_bkl(p, &errs)) {
		assert(errs);
		lease_printerr(errs);
		exit(1);
	}
	return (p->bkl);
}

/* return the decoded license/option bits (LIC_*) */
u32
proj_bklbits(project *p)
{
	/*
	 * If we are outside of any repository then we must get a
	 * licence from the global config or a license server.
	 */
	unless (p || (p = curr_proj())) p = proj_fakenew();

	unless (p->bklbits) p->bklbits = license_bklbits(proj_bkl(p));
	return (p->bklbits);
}

/*
 * Returns 1 if the repo has already been checked in this write mode
 * and 0 otherwise.  It also saves the mark.
 *
 * This is a helper found used by lease.c to determine if lease have
 * already been verified for this repository.
 */
int
proj_leaseChecked(project *p, int write)
{
	unless (p || (p = curr_proj())) return (0);

	if ((p->leaseok == O_WRONLY) ||
	    ((p->leaseok == O_RDONLY) && (write == O_RDONLY))) {
		return (1);
	}
	p->leaseok = write;
	return (0);
}

int
proj_isCaseFoldingFS(project *p)
{
	char	*t;
	char	s_cset[] = CHANGESET;
	char	buf[MAXPATH];

	unless (p || (p = curr_proj())) return (-1);
	if (p->casefolding != -1) return (p->casefolding);
	if (p->rparent) {
		p->casefolding = proj_isCaseFoldingFS(p->rparent);
	} else {
		t = strrchr(s_cset, '/');
		assert(t && (t[1] == 's'));
		t[1] = 'S';  /* change to upper case */
		concat_path(buf, p->root, s_cset);
		p->casefolding = exists(buf);
	}
	return (p->casefolding);
}

int
proj_samerepo(char *source, char *dest)
{
	int	rc = 0;
	project	*pj1, *pj2;

	unless (pj1 = proj_init(source)) {
		fprintf(stderr,
		    "%s: is not in a BitKeeper repository\n",
		    source);
		return (0);
	}
	unless (pj2 = proj_init(dest)) {
		fprintf(stderr,
		    "%s: is not in a BitKeeper repository\n",
		    dest);
		return (0);
	}
	unless (rc = (pj1 == pj2)) {
		fprintf(stderr,
		    "%s & %s are not in the same BitKeeper repository\n",
		    source, dest);
	}
	proj_free(pj1);
	proj_free(pj2);
	return (rc);
}

int
proj_isResync(project *p)
{
	unless (p) p = curr_proj();

	return (p && p->rparent);
}

/*
 * Record the checkout state of the file so we can restore it
 * after whatever operation we are doing is finished.
 */
void
proj_saveCO(sccs *s)
{
	int	state;
	char	key[MAXKEY];

	assert(s->proj);
	if (CSET(s) || strneq("BitKeeper/", s->gfile, 10)) return;
	sccs_sdelta(s, sccs_ino(s), key);
	if (HAS_PFILE(s)) {
		state = CO_EDIT;
	} else if (HAS_GFILE(s)) {
		state = CO_GET;
	} else {
		state = CO_NONE;
	}
	proj_saveCOkey(s->proj, key, state);
}

void
proj_saveCOkey(project *p, char *key, int co)
{
	char	state[2];

	unless (p) p = curr_proj();
	if (proj_isResync(p)) return;
	unless (p->coDB) p->coDB = mdbm_mem();
	state[0] = co + '0';
	state[1] = 0;
	mdbm_store_str(p->coDB, key, state, MDBM_REPLACE);
}

private int
restoreCO(sccs *s)
{
	char	*state;
	int	co;
	int	getFlags = 0;
	char	key[MAXKEY];

	unless (s && s->proj) return (0);
	if (proj_isResync(s->proj)) return (0);
	if (CSET(s) || strneq("BitKeeper/", s->gfile, 10)) return (0);

	/* Let's just be sure we're up to date */
	if (check_gfile(s, 0)) return (-1);

	sccs_sdelta(s, sccs_ino(s), key);
	if (state = mdbm_fetch_str(s->proj->coDB, key)) {
		co = (*state - '0');
	} else {
		/* Use the repo defaults for new files */
		co = proj_checkout(s->proj);
	}
	switch (co) {
	    case CO_EDIT: unless (HAS_PFILE(s)) getFlags = GET_EDIT; break;
	    case CO_GET: unless (HAS_GFILE(s)) getFlags = GET_EXPAND; break;
	    case CO_NONE: break;
	    case CO_LAST: break;
	    default: assert(0);
	}
	unless (getFlags) return (0);
	unless (sccs_get(s, 0, 0, 0, 0, SILENT|GET_NOREMOTE|getFlags, "-")) {
		return (0);
	}
	if (s->cachemiss) {
		if (getFlags & GET_EDIT) {
			s->proj->bp_getFiles =
			    addLine(s->proj->bp_getFiles, strdup(s->gfile));
		} else {
			s->proj->bp_editFiles =
			    addLine(s->proj->bp_editFiles, strdup(s->gfile));
		}
		return (0);
	}
	return (-1);
}

/*
 * After the operations is finished we look at every file that
 * we might have touched and restore the gfile if needed.
 */
int
proj_restoreAllCO(project *p, MDBM *idDB)
{
	sccs	*s;
	kvpair	kv;
	int	i, errs = 0, freeid = 0;
	char	*t;
	FILE	*f;

	unless (p) p = curr_proj();

	if (proj_isResync(p)) return (0);
	unless (idDB) {
		t = aprintf("%s/%s", proj_root(p), IDCACHE);
		idDB = loadDB(t, 0, DB_IDCACHE);
		free(t);
		unless (idDB) {
			perror("idcache");
			exit(1);
		}
		freeid = 1;

	}
	EACH_KV(p->coDB) {
		s = sccs_keyinit(kv.key.dptr, INIT_NOCKSUM|SILENT, idDB);
		unless (s) continue;
		assert(p == s->proj);
		if (restoreCO(s)) errs++;
		sccs_free(s);
	}
	mdbm_close(p->coDB);
	p->coDB = 0;
	if (freeid) mdbm_close(idDB);

	/* Best effort binpool get/edit */
	if (p->bp_getFiles) {
		// XXX - what about hardlinks for binpool?
		f = popen("bk get -q -", "w");
		EACH(p->bp_getFiles) fprintf(f, "%s\n", p->bp_getFiles[i]);
		(void)pclose(f);
		freeLines(p->bp_getFiles, free);
		p->bp_getFiles = 0;
	}
	if (p->bp_editFiles) {
		// XXX - what about hardlinks for binpool?
		f = popen("bk edit -q -", "w");
		EACH(p->bp_editFiles) fprintf(f, "%s\n", p->bp_editFiles[i]);
		(void)pclose(f);
		freeLines(p->bp_editFiles, free);
		p->bp_editFiles = 0;
	}
	return (errs);
}

MDBM *
proj_binpoolIDX(project *p, int write)
{
	char	idx[MAXPATH];

	unless (p || (p = curr_proj())) return (0);

	if (p->rparent) p = p->rparent;

	if (p->binpool_idx) {
		if (write && !p->binpool_write) {
			mdbm_close(p->binpool_idx);
			p->binpool_idx = 0;
		} else {
			return (p->binpool_idx);
		}
	}
	/* open a new binpool */
	concat_path(idx, p->root, "BitKeeper/binpool/index.db");
	if (write || exists(idx)) {
		p->binpool_idx = mdbm_open(idx,
		    write ? O_RDWR|O_CREAT : O_RDONLY, 0666, 8192);
		p->binpool_write = write;
	}
	return (p->binpool_idx);
}
