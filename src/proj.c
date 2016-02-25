/*
 * Copyright 2003-2016 BitMover, Inc
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
#include "bam.h"
#include "cfg.h"

/*
 * This file contains a series of accessor functions for the project
 * struct that caches information about the current repository.  There
 * is a global project struct that always matches the current
 * directory.  This struct is private to proj.c and it used whenever 0
 * is passed to these functions.
 */

private	int	noremap_default = 0;

/*
 * Don't treat chdir() special here.
 */
#undef	chdir
#ifdef	WIN32
#define	chdir	nt_chdir
#endif

struct project {
	char	*root;		/* fullpath root of the project */
	char	*rootkey;	/* Root key of ChangeSet file */
	char	*md5rootkey;	/* MD5 root key of ChangeSet file */
	char	*syncroot;	/* sync root key of ChangeSet file */
	char	*repoID;	/* RepoID */
	char	*comppath;	/* if component, path to root from product */
	MDBM	*config;	/* config DB */
	project	*rparent;	/* if RESYNC, point at enclosing repo */
	project	*product;	/* if Nested, point at the product repo */

	/* per proj cache data */
	int	casefolding;	/* mixed case file system: FOO == foo */
	MDBM	*BAM_idx;	/* BAM index file */
	int	BAM_write;	/* BAM index file opened for write? */
	int	sync;		/* sync/fsync data? */
	int	idxsock;	/* sock to index server for this repo */
	int	noremap;	/* true == old style SCCS dirs */
	char	*tipkey;	/* key/md5key/rev of tip cset rev */
	char	*tipmd5key;
	char	*tiprev;
	hash	*scancomps;	/* prod/BitKeeper/log/scancomps */
	hash	*scandirs;	/* comp/BitKeeper/log/scandirs */

	/* checkout state */
	u32	co;		/* cache of proj_checkout() return */
	MDBM	*coDB;		/* $coDB{rootkey} = e|g|n */
	char	**bp_getFiles;	/* files we need to fetch and get */
	char	**bp_editFiles;	/* files we need to fetch and edit */

	/* external structs */
	p_feat	features;	/* features.c support */

	/* internal state */
	int	refcnt;
	int	preDelta;	/* pre-delta trigger state */
	char	**dirs;		/* list of dirs that map to this project */
};

private char	*find_root(char *dir);

private	int NRECENT = -1;	/* save last 4 in real life, 2 in regressions */

private struct {
	project	*curr;
	char	**recent;	/* addlines of upto NRECENT structs */
	hash	*cache;
	char	cwd[MAXPATH];
} proj;

private project *
projcache_lookup(char *dir)
{
	unless (proj.cache) proj.cache = hash_new(HASH_MEMHASH);
	return (hash_fetchStrPtr(proj.cache, dir));
}

/*
 * The code does a lookup before doing a store, so no need to autocreate
 * hashes like the routine above does.
 */
private void
projcache_store(char *dir, project *p)
{
	unless (hash_insertStrPtr(proj.cache, dir, p)) {
		/* we should never init a new project when we already
		 * have a copy open.
		 */
		assert("dup proj" == 0);
	}
	p->dirs = addLine(p->dirs, proj.cache->kptr);
}

private void
projcache_delete(project *p)
{
	int	i;

	EACH(p->dirs) hash_deleteStr(proj.cache, p->dirs[i]);
	freeLines(p->dirs, 0);
	p->dirs = 0;
}

/*
 * Add proj struct to a list of recently used items.
 * Delete one if we have collected too many.
 */
private void
proj_recent(project *p)
{
	project	*oldp;

	if (NRECENT == -1) NRECENT = getenv("BK_REGRESSION") ? 2 : 4;

	while (nLines(proj.recent) >= NRECENT) {
		/*
		 * don't use the free field in removeLineN because
		 * proj_free() checks the proj.recent cache and fails.
		 */
		oldp = (project *)proj.recent[1];
		removeLineN(proj.recent, 1, 0);
		proj_free(oldp);
	}
	proj.recent = addLine(proj.recent, p);
	++p->refcnt;
}

/*
 * Return a project struct for the project that contains the given
 * directory.
 */
project *
proj_init(char *dir)
{
	project	*ret;
	int	old;
	char	*root;
	char	*t;
	char	*fdir, *cwd;
	char	buf[MAXPATH];

	if (streq(dir, ".") && proj.curr) {
		/* optimize common case */
		ret = proj.curr;
		goto done;
	}
	if (IsFullPath(dir)) {
		fdir = dir;
	} else {
		unless (cwd = proj_cwd()) return (0);
		if (strneq(dir, "./", 2)) dir += 2;
		concat_path(buf, cwd, dir);
		fdir = buf;
	}
	if (ret = projcache_lookup(fdir)) goto done;

	/* missed the cache */
	old = fslayer_enable(0);
	root = find_root(dir);
	fslayer_enable(old);
	unless (root) return (0);

	unless (streq(root, fdir)) {
		/* fdir is not a root, was root in cache? */
		if (ret = projcache_lookup(root)) {
			/* yes, make a new mapping */
			projcache_store(fdir, ret);
			free(root);
			root = 0;
			goto done;
		}
	}
	assert(ret == 0);
	/* Totally new project */
	ret = new(project);
	proj_reset(ret);	/* default values */
	ret->root = root;

	/* remember recent proj structs */
	proj_recent(ret);

	projcache_store(root, ret);
	unless (streq(root, fdir)) projcache_store(fdir, ret);

	if ((t = strrchr(ret->root, '/')) && streq(t, "/RESYNC")) {
		*t = 0;
		ret->rparent = proj_init(ret->root);
		if (ret->rparent && !streq(ret->rparent->root, ret->root)) {
			proj_free(ret->rparent);
			ret->rparent = 0;
		}
		*t = '/';
	}
done:
	++ret->refcnt;
	return (ret);
}

void
proj_free(project *p)
{
	int	i;

	assert(p);

	assert(p->refcnt > 0);
	unless (--p->refcnt == 0) return;

	/* we shouldn't be freeing something that is cached */
	assert(p != proj.curr);
	EACH(proj.recent) assert(p != (project *)proj.recent[i]);

	projcache_delete(p);	/* subtle: See SHORTCUT in proj_reset() */
	proj_reset(p);		/* subtle: do before p->prod due to flush */
	if (p->rparent) {
		proj_free(p->rparent);
		p->rparent = 0;
	}
	if (p->product && (p->product != INVALID) && (p->product != p)) {
		proj_free(p->product);
		p->product = INVALID;
	}
	if (p->coDB) {
		mdbm_close(p->coDB);
		p->coDB = 0;
	}
	free(p->root);
	free(p);
}

/*
 * like fullname() but it also expands if the final component of the
 * pathname is a symlink to a directory
 */
private void
fullname_expand(char *dir, char *buf)
{
	int	i;
	char	sym[MAXPATH];

	while (1) {
		/* convert dir to a full pathname and expand symlinks */
		dir = fullname(dir, buf);
		unless (isSymlnk(dir) && isdir_follow(dir)) return;

		/*
		 * fullname() doesn't expand symlinks in the last
		 * componant so fix that.
		 * While isdir_follow will recurse until not symlink,
		 * this will only go one level at a time, hence the loop.
		 */
		i = readlink(dir, sym, sizeof(sym));
		sym[i] = 0;
		if (IsFullPath(sym)) {
			strcpy(buf, sym);
		} else {
			concat_path(buf, dirname(dir), sym);
		}
	}
}

private char *
find_root(char *dir)
{
	char	*p, *first;
	project	*proj;
	char	buf[MAXPATH];

	fullname_expand(dir, buf);

	/* This code assumes dir is a full pathname with nothing funny */
	p = buf + strlen(buf);
	if ((p > buf) && (p[-1] == '/')) *--p = 0;	/* D:// into D:/ */

	/*
	 * Find the last slash in pathname where we will look for a
	 * repository. This means a repository cannot be at the root
	 * of the filesystem, but it avoids problems with the NFS
	 * automounter.
	 *
	 *   /nfs/repo
	 *       ^
	 *   f:/repo
	 *     ^
	 */
	if (win32() && (buf[1] == ':')) {
		unless (first = strchr(buf, '/')) return (0);
	} else {
		unless (*buf == '/') return (0);
		if (getenv("BK_REPO_IN_ROOT")) {
			/* hack to support repository at fs root */
			first = buf;
		} else {
			unless (first = strchr(buf+1, '/')) return (0);
		}
	}

	/*
	 * Now work backwards up the tree until we find a root marker
	 */
	while (1) {
		strcpy(p, "/" BKROOT);
		if (isdir(buf)) {
			/* found repo */
			*p = 0;
			if (((p - buf) > 4) && streq(p-4, "/.bk")) return (0);
			return (strdup(buf));
		}
		for (p--; *p != '/'; p--); /* previous / */
		if (p == first) return (0);
		*p = 0;
		if (proj = projcache_lookup(buf)) return (strdup(proj->root));
	}
	/* NOTREACHED */
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
 * chdir to the root of the product
 * returns an error in non-nested repositories
 */
int
proj_cd2product(void)
{
	project	*p;

	unless (p = curr_proj()) return (-1);
	if (p->rparent) p = p->rparent;
	if (p = proj_product(p)) return (proj_chdir(proj_root(p)));
	return (-1);
}

/*
 * When given a pathname to a file, this function returns the pathname
 * to the file relative to the current project.  If the file is not under
 * the current project then, NULL is returned.
 * The returned path is allocated with malloc() and the user must free().
 */
char *
proj_relpath(project *p, char *in_path)
{
	char	*root = proj_root(p);
	int	len;
	char	path[MAXPATH];

	assert(root);
	fullname(in_path, path);
	T_PROJ("in=%s, path=%s", in_path, path);
	len = strlen(root);
	if (pathneq(root, path, len)) {
		if (!path[len]) {
			return (strdup("."));
		} else {
			assert(path[len] == '/');
			return(strdup(&path[len+1]));
		}
	} else {
		/* special case, we're in / */
		if (streq(root, "/.")) return (strdup("."));
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
		p->config = loadConfig(p, 1);
		unless (p->config) return (proj_config(p->rparent));
	} else {
		p->config = loadConfig(p, 0);
	}
	return (p->config);
}

/*
 * returns the checkout state for the current repository
 */
int
proj_checkout(project *p)
{
	char	*s;
	int	bits;

	unless (p || (p = curr_proj())) return (CO_NONE|CO_BAM_NONE);
	if (proj_isResync(p)) return (CO_NONE|CO_BAM_NONE);
	if (p->co) return (p->co);
	s = cfg_str(p, CFG_CHECKOUT);
	assert(s);		/* must have default */
	if (strieq(s, "get")) bits = CO_GET|CO_BAM_GET;
	if (strieq(s, "edit")) bits = CO_EDIT|CO_BAM_EDIT;
	if (strieq(s, "last")) bits = CO_LAST|CO_BAM_LAST;
	if (strieq(s, "none")) bits = CO_NONE|CO_BAM_NONE;
	unless (bits) {
		fprintf(stderr,
		    "WARNING: checkout: should be get|edit|last|none.\n"
		    "Meaning of '%s' unknown. Assuming edit.\n", s);
		bits = CO_EDIT|CO_BAM_EDIT;
	}
	if (s = cfg_str(p, CFG_BAM_CHECKOUT)) {
		bits &= 0xf;
		if (strieq(s, "get")) bits |= CO_BAM_GET;
		if (strieq(s, "edit")) bits |= CO_BAM_EDIT;
		if (strieq(s, "last")) bits |= CO_BAM_LAST;
		if (strieq(s, "none")) bits |= CO_BAM_NONE;
		unless (bits & 0xf0) {
			fprintf(stderr,
			    "WARNING: BAM_checkout: "
			    "should be get|edit|last|none.\n"
			    "Meaning of '%s' unknown. Assuming none.\n", s);
			bits |= CO_BAM_NONE;
		}
	}
	return (p->co = bits);
}


/* Return the root key of the ChangeSet file in the current project. */
char	*
proj_rootkey(project *p)
{
	sccs	*sc;
	FILE	*f;
	char	*ret, *t;
	char	file[MAXPATH];
	char	buf[MAXPATH];

	unless (p || (p = curr_proj())) p = proj_fakenew();

	/*
	 * Use cached copy if available
	 */
	if (p->rootkey) return (p->rootkey);
	if (p->rparent && (ret = proj_rootkey(p->rparent))) return (ret);

	/* clear existing values */
	if (p->rootkey)    { free(p->rootkey);    p->rootkey = 0; }
	if (p->md5rootkey) { free(p->md5rootkey); p->md5rootkey = 0; }
	if (p->syncroot)   { free(p->syncroot);   p->syncroot = 0; }

	/* load values from cache */
	concat_path(file, p->root, "/BitKeeper/log/ROOTKEY");
	if (f = fopen(file, "rt")) {
		if (t = fgetline(f)) p->rootkey = strdup(t);
		if (t = fgetline(f)) p->md5rootkey = strdup(t);
		if (t = fgetline(f)) p->syncroot = strdup(t);
		fclose(f);
	}

	if (p->rootkey && p->md5rootkey && p->syncroot) return (p->rootkey);

	/* cache invalid, regenerate values */
	concat_path(buf, p->root, CHANGESET);
	if (exists(buf)) {
		sc = sccs_init(buf, INIT_NOCKSUM|INIT_NOSTAT|INIT_WACKGRAPH);
		assert(HASGRAPH(sc));
		sccs_sdelta(sc, TREE(sc), buf);
		if (p->rootkey) free(p->rootkey);
		p->rootkey = strdup(buf);
		sccs_md5delta(sc, TREE(sc), buf);
		if (p->md5rootkey) free(p->md5rootkey);
		p->md5rootkey = strdup(buf);
		sccs_syncRoot(sc, buf);
		if (p->syncroot) free(p->syncroot);
		p->syncroot = strdup(buf);
		sccs_free(sc);
		concat_path(file, p->root, "/BitKeeper/log/ROOTKEY");
		if (f = fopen(file, "wt")) {
			fputs(p->rootkey, f);
			putc('\n', f);
			fputs(p->md5rootkey, f);
			putc('\n', f);
			fputs(p->syncroot, f);
			putc('\n', f);
			fclose(f);
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

	if (p->rparent && (ret = proj_md5rootkey(p->rparent))) return (ret);
	unless (p->md5rootkey) proj_rootkey(p);
	return (p->md5rootkey);
}

/* Return the sync root key of the ChangeSet file in the current project. */
char *
proj_syncroot(project *p)
{
	char	*ret;

	unless (p || (p = curr_proj())) p = proj_fakenew();

	if (p->rparent && (ret = proj_syncroot(p->rparent))) return (ret);
	unless (p->syncroot) proj_rootkey(p);
	return (p->syncroot);
}

/*
 * Return the product's project* if this repo is nested
 * and returns 0 otherwise
 *
 * The returned product is part of 'p' so it shouldn't be freed,
 * and it only valid while 'p' is still active.
 */
project *
proj_product(project *p)
{
	project	*proj, *prod;
	char	buf[MAXPATH];

	unless (p || (p = curr_proj())) return (0);
	if (p->product == INVALID) {
		/* find product root */
		p->product = 0;
		concat_path(buf, p->root, "BitKeeper/log/PRODUCT");
		if (exists(buf)) {
			p->product = p;	/* we're our own product */
		} else if (proj_comppath(p)) {
			/* return proj_product of the repo above this one */
			strcpy(buf, p->root);
			if (proj = proj_init(dirname(buf))) {
				assert(proj != p);
				if (prod = proj_product(proj)) {
					p->product = proj_init(proj_root(prod));
				}
				proj_free(proj);
			}
		}
	}
	return (p->product);
}

/*
 * Return the project* for any product at or above the current repository.
 * Unlike proj_product this one still works when the current repo is
 * standalone.
 *
 * Note: this is not setting the refcnt on the returned project* so it
 *       is relying on the recent projects cache.  Be careful.
 */
project *
proj_findProduct(project *p)
{
	project	*proj, *prod;
	char	buf[MAXPATH];

	unless (p || (p = curr_proj())) return (0);
	if (prod = proj_product(p)) return (prod);

	/* return proj_findProduct of the repo above this one */
	strcpy(buf, p->root);
	if (proj = proj_init(dirname(buf))) {
		assert(proj != p);
		prod = proj_findProduct(proj);
		proj_free(proj);
		return (prod);
	}
	return (0);
}


/*
 * Return the path to the component, if any.
 */
char *
proj_comppath(project *p)
{
	FILE	*f;
	char	*t;
	char	file[MAXPATH];

	unless (p || (p = curr_proj())) return (0);

	if (p->rparent) p = p->rparent;

	/* returned cached value if possible */
	if (p->comppath) goto out;

	concat_path(file, p->root, "/BitKeeper/log/COMPONENT");
	if (f = fopen(file, "rt")) {
		if (t = fgetline(f)) p->comppath = strdup(t);
		fclose(f);
	}
	unless (p->comppath) p->comppath = strdup("");
out:	return (p->comppath[0] ? p->comppath : 0);
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
	project	*p2;
	hash	*h;
	char	**recent;

	/* free the cwd project */
	if (proj.curr && (!p || (p == proj.curr))) {
		/*
		 * proj_free() checks proj caches like proj.curr so
		 * proj.curr must be zero to avoid assert
		 */
		p2 = proj.curr;
		proj.curr = 0;
		proj_free(p2);
	}
	if (p) {
		proj_flush(p);
		FREE(p->rootkey);
		FREE(p->md5rootkey);
		FREE(p->comppath);
		FREE(p->repoID);
		FREE(p->tipkey);
		FREE(p->tipmd5key);
		FREE(p->tiprev);
		if (p->config) {
			mdbm_close(p->config);
			p->config = 0;
		}
		p->casefolding = -1;
		p->co = 0;
		p->sync = -1;
		p->noremap = -1;
		p->preDelta = -1;
		p->features.bits = 0;
		if (p->BAM_idx) {
			mdbm_close(p->BAM_idx);
			p->BAM_idx = 0;
		}
		if (p->idxsock) {
			closesocket(p->idxsock);
			p->idxsock = 0;
		}
		if (p->scancomps) {
			hash_free(p->scancomps);
			p->scancomps = 0;
		}
		if (p->scandirs) {
			hash_free(p->scandirs);
			p->scandirs = 0;
		}
		unless (p->product) p->product = INVALID;

		/*
		 * delete all but primary dir mapping
		 * SHORTCUT: proj_free() calls projcache_delete() which
		 * will clear p->dirs.  It is the only case in which
		 * p->dirs will be empty with the desire being: don't
		 * put something into it which needs to be freed.
		 */
		if (p->dirs) {
			projcache_delete(p);
			projcache_store(p->root, p);
		}
	} else {
		/*
		 * proj_free() checks to see that freed item isn't cached.
		 * Mark the proj.recent cache as empty before freeing.
		 */
		recent = proj.recent;
		proj.recent = 0;
		freeLines(recent, (void(*)(void *))proj_free);

		/*
		 * only proj structs with external references will
		 * remain
		 */

		/* use a hash to reset each unique proj struct */
		h = hash_new(HASH_MEMHASH);
		EACH_HASH(proj.cache) {
			hash_insert(h, proj.cache->vptr, sizeof(project *),
			    0, 0);
		}
		EACH_HASH(h) proj_reset(*(project **)h->kptr);
		hash_free(h);
	}
}

/*
 * write out modified state that is cached in the proj struct
 */
void
proj_flush(project *p)
{
	if (p) {
		if (p->scancomps || p->scandirs) {
			proj_dirstate(p, 0, 0, -1);
			assert(!p->scancomps && !p->scandirs);
		}
	} else {
		EACH_HASH(proj.cache) {
			proj_flush(*(project **)proj.cache->vptr);
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
	project	*p;

	ret = chdir(newdir);
	unless (ret) {
		unless (getcwd(proj.cwd, sizeof(proj.cwd))) proj.cwd[0] = 0;
		if (proj.curr) {
			p = proj.curr;
			proj.curr = 0;
			proj_free(p);
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
		unless (getcwd(proj.cwd, sizeof(proj.cwd))) proj.cwd[0] = 0;
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

	if (ret = projcache_lookup("/.")) return (ret);
	ret = new(project);
	ret->root = strdup("/.");
	ret->rootkey = strdup("SCCS");
	ret->md5rootkey = strdup("SCCS");
	projcache_store("/.", ret);

	return (ret);
}


int
proj_isCaseFoldingFS(project *p)
{
	char	*t;
	char	s_cset[] = CHANGESET;
	char	buf[MAXPATH];

	unless (p || (p = curr_proj())) return (-1);
	if (p->rparent) p = p->rparent;
	if (p->casefolding != -1) return (p->casefolding);
	t = strrchr(s_cset, '/');
	assert(t && (t[1] == 's'));
	t[1] = 'S';  /* change to upper case */
	concat_path(buf, p->root, s_cset);
	p->casefolding = exists(buf);
	return (p->casefolding);
}

int
proj_samerepo(char *source, char *dest, int quiet)
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
		unless (quiet) {
			fprintf(stderr,
			    "%s & %s are not in the same "
			    "BitKeeper repository\n", source, dest);
		}
	}
	proj_free(pj1);
	proj_free(pj2);
	return (rc);
}

project *
proj_isResync(project *p)
{
	unless (p || (p = curr_proj())) return (0);
	return (p->rparent);
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
	char	*path;
	char	state[2];

	unless (p) p = curr_proj();
	if (proj_isResync(p)) return;
	path = strchr(key, '|') + 1;
	if (strneq(path, "ChangeSet|", 10)) return; /* no co on ChangeSet */
	unless (p->coDB) p->coDB = mdbm_mem();
	state[0] = co + '0';
	state[1] = 0;
	mdbm_store_str(p->coDB, key, state, MDBM_REPLACE);
}

private int
restoreCO(sccs *s, int co, int dtime)
{
	int	getFlags = dtime ? GET_DTIME : 0;

	if (CSET(s) || strneq("BitKeeper/", s->gfile, 10)) return (0);

	/* Let's just be sure we're up to date */
	if (check_gfile(s, 0)) return (-1);

	switch (co) {
	    case CO_EDIT:
		unless (HAS_PFILE(s)) getFlags |= GET_EDIT; break;
	    case CO_GET:
		unless (HAS_GFILE(s)) getFlags |= GET_EXPAND; break;
	    default:
		fprintf(stderr, "co=0x%x\n", co);
		assert(0);
	}
	unless (getFlags) return (0);
	unless (sccs_get(s, 0, 0, 0, 0, SILENT|GET_NOREMOTE|getFlags,
	    s->gfile, 0)) {
		return (0);
	}
	if (s->cachemiss) {
		if (getFlags & GET_EDIT) {
			s->proj->bp_editFiles =
			    addLine(s->proj->bp_editFiles, strdup(s->gfile));
		} else {
			s->proj->bp_getFiles =
			    addLine(s->proj->bp_getFiles, strdup(s->gfile));
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
proj_restoreAllCO(project *p, MDBM *idDB, ticker *tick, int dtime)
{
	sccs	*s;
	kvpair	kv;
	int	i, errs = 0, freeid = 0, co;
	char	*t;
	FILE	*f;

	unless (p) p = curr_proj();

	if (proj_isResync(p)) return (0);
	unless (idDB) {
		t = aprintf("%s/%s", proj_root(p), getIDCACHE(p));
		idDB = loadDB(t, 0, DB_IDCACHE);
		free(t);
		unless (idDB) {
			perror("idcache");
			exit(1);
		}
		freeid = 1;

	}
	EACH_KV(p->coDB) {
		if (tick) progress(tick, tick->cur+1);
		co = (kv.val.dptr[0] - '0');
		assert(!(co & (CO_BAM_GET|CO_BAM_EDIT)));
		if (co & (CO_GET|CO_EDIT)) {
			s = sccs_keyinit(p, kv.key.dptr,
			    INIT_NOCKSUM|SILENT, idDB);
			unless (s) continue;
			assert(p == s->proj);
			if (restoreCO(s, co, dtime)) errs++;
			sccs_free(s);
		}
	}
	mdbm_close(p->coDB);
	p->coDB = 0;
	if (freeid) mdbm_close(idDB);

	/* Best effort BAM get/edit */
	if (p->bp_getFiles) {
		// XXX - what about hardlinks for BAM?
		f = popen("bk get -q -", "w");
		EACH(p->bp_getFiles) fprintf(f, "%s\n", p->bp_getFiles[i]);
		if (pclose(f)) errs++;
		freeLines(p->bp_getFiles, free);
		p->bp_getFiles = 0;
	}
	if (p->bp_editFiles) {
		// XXX - what about hardlinks for BAM?
		f = popen("bk edit -q -", "w");
		EACH(p->bp_editFiles) fprintf(f, "%s\n", p->bp_editFiles[i]);
		if (pclose(f)) errs++;
		freeLines(p->bp_editFiles, free);
		p->bp_editFiles = 0;
	}
	return (errs);
}

MDBM *
proj_BAMindex(project *p, int write)
{
	char	idx[MAXPATH];

	unless (p || (p = curr_proj())) return (0);

	if (p->BAM_idx) {
		if (write && !p->BAM_write) {
			mdbm_close(p->BAM_idx);
			p->BAM_idx = 0;
		} else {
			return (p->BAM_idx);
		}
	}
	/* open a new BAM pool */
	bp_dataroot(p, idx);
	concat_path(idx, idx, BAM_DB);
	if (write || exists(idx)) {
		p->BAM_idx = mdbm_open(idx,
		    write ? O_RDWR|O_CREAT : O_RDONLY, 0666, 8192);
		p->BAM_write = write;
	}
	return (p->BAM_idx);
}

/*
 * Should bk call fsync() after writing sfiles and sync() after applying
 * new changes in resolve?
 *
 * The default is 'no' unless 'sync:yes' is found in the config.
 *
 * The old 'nosync' config is ignored.
 */
int
proj_sync(project *p)
{
	unless (p || (p = curr_proj())) return (0);

	if (p->rparent) return (0); /* no syncs in RESYNC */

	if (p->sync == -1) p->sync = cfg_bool(p, CFG_SYNC);
	return (p->sync);
}

/*
 * Returns true if p is a product repository.
 * Do not use this to see if there is a product, use proj_product() for that.
 * This interface is to specifically see if p is a product.
 */
int
proj_isProduct(project *p)
{
	project	*prod;

	unless (p || (p = curr_proj())) return (0);
	if (p->rparent) p = p->rparent;
	return ((prod = proj_product(p)) && (prod == p));
}

/*
 * Returns true if p is a component repository.
 * Note: Just being under a product is not enough.  It needs to have the
 *       COMPONENT marker file setup.
 */
int
proj_isComponent(project *p)
{
	unless (p || (p = curr_proj())) return (0);
	if (p->rparent) p = p->rparent;
	return (proj_comppath(p) != 0);
}

int
proj_idxsock(project *p)
{
	FILE	*f;
	int	c;
	char	*s, *t;
	int	first = 1;
	char	buf[MAXPATH];

	unless (p || (p = curr_proj())) return (0);

	if (p->idxsock) return (p->idxsock);

	concat_path(buf, p->root, ".bk/INDEX");
	unless (f = fopen(buf, "r")) {
again:		putenv("_BK_FSLAYER_SKIP=1");
		c = sys("bk", "indexsvr", proj_root(p), SYS);
		putenv("_BK_FSLAYER_SKIP=");
		unless (!c ||
		    (WIFEXITED(c) && WEXITSTATUS(c) == 1)) {
			fprintf(stderr, "bk indexsvr failed\n");
			exit(1);
		}
		f = fopen(buf, "r");
		assert(f);
	}
	// read from env?
	if ((t = fgetline(f)) && (s = strchr(t, ':'))) {
		*s++ = 0;
		p->idxsock = tcp_connect(t, atoi(s));
	} else {
		p->idxsock = -4;
	}
	fclose(f);
	if (first && (p->idxsock < 0)) {
		first = 0;
		goto again;
	}
	// need to verify
	assert(p->idxsock > 0);
	return (p->idxsock);
}

/*
 * Return true if this repo uses the old directory remapping.
 * Funky rules:
 *  root/SCCS exists -> non-remapped
 *  root/.bk/SCCS exists -> remapped
 *  both exists? -> try to fix problem
 *  is product remapped? -> do the same
 *
 * The rationale for the funk is to make new components (coming in via
 * populate, say) behave like the product.  The funk above is to protect
 * against screwing up existing components in a mixed remapped/non-remapped
 * ensemble.
 *
 * The fall through default (for a new repo) is to do the remapping.
 */
int
proj_hasOldSCCS(project *p)
{
	project	*p2;
	int	en;
	int	oldsccs, newsccs;
	char	buf[MAXPATH];

	unless (p || (p = curr_proj())) return (1);

	if (p->noremap != -1) return (p->noremap);

	if (p->rparent) {
		p->noremap = proj_hasOldSCCS(p->rparent);
		return (p->noremap);
	}

	en = fslayer_enable(0);
	/* See: Funky rules above */
	concat_path(buf, p->root, ".bk/SCCS");
	newsccs = isdir(buf);
	concat_path(buf, p->root, "SCCS");
	oldsccs = isdir(buf);

	if (oldsccs) {
		if (newsccs) {
			/* both exist? */
			if (!rmdir(buf)) {
				/* empty from 4.6, no problem */
				p->noremap = 0;
			} else {
				fprintf(stderr, "error: both %s/SCCS "
				    "and .../.bk/SCCS exist\n", p->root);
				exit(1);
			}
		} else {
			p->noremap = 1;
		}
	} else {
		if (newsccs) {
			p->noremap = 0;
		} else {
			/* Neither exist, find default
			 * Called before clone is a component so use
			 * proj_findProduct()
			 */
			if ((p2 = proj_findProduct(p)) && (p != p2)) {
				p->noremap = proj_hasOldSCCS(p2);
			} else {
				p->noremap = noremap_default;
			}
		}
	}
	fslayer_enable(en);
	return (p->noremap);
}

int
proj_remapDefault(int doremap)
{
	int	ret = !noremap_default;

	noremap_default = !doremap;
	return (ret);
}

/*
 * Are there any pre-delta triggers associated with this project?
 * This is a cached response to that question, since delta can be
 * run on many files.
 */
int
proj_hasDeltaTriggers(project *p)
{
	char	here[MAXPATH];

	unless (p || (p = curr_proj())) return (0);

	if (p->preDelta == -1) {
		/* we never run pre-delta triggers in RESYNC */
		if (proj_isResync(p)) return (p->preDelta = 0);

		strcpy(here, proj_cwd());
		if (proj_chdir(p->root)) {
			perror("to trigger check");
			exit (1);
		}
		p->preDelta = hasTriggers("delta", "pre");
		if (proj_chdir(here)) {
			perror("from trigger check");
			exit (1);
		}
	}
	return (p->preDelta);
}

/*
 * Keep most recent cset2rev caches
 */
private void
pruneCsetCache(project *p, char *new)
{
	char	**files;
	char	**keep = 0;
	int	i;
	char	*s, *t;
	struct	stat sb;
	char	buf[MAXPATH];

	/* delete old caches here, keep newest */
	sprintf(buf, "%s/BitKeeper/tmp", p->root);
	files = getdir(buf);
	t = buf + strlen(buf);
	*t++ = '/';
	EACH (files) {
		if (streq(files[i], new)) continue; /* keep one just created */
		if (strneq(files[i], "csetkeycache.", 13) ||
		    strneq(files[i], "csetcache.", 10)) {
			strcpy(t, files[i]);

			/*
			 * Collect all the existing cache files and
			 * save them with the current timestamp.
			 *
			 * XXX We may want to open the non-key caches
			 *     to see if they are still valid, but I decided
			 *     strict time order is fine for now.
			 */
			lstat(buf, &sb);
			/* invert time to get newest first */
			s = aprintf("%08x %s", (u32)~sb.st_atime, buf);
			keep = addLine(keep, s);
		}
	}
	freeLines(files, free);
	sortLines(keep, 0);
	EACH(keep) {
		/* keep the latest 4 cache files */
		if (i > 4) {
			t = strchr(keep[i], ' ');
			unlink(t+1);
		}
	}
	freeLines(keep, free);
}


/*
 * Given a csetrev and a file rootkey, return the file deltakey that
 * was active when that cset was created.
 *
 * The data is cached in BitKeeper/tmp for fast access.
 */
char *
proj_cset2key(project *p, char *csetrev, char *rootkey)
{
	char	*mpath;
	char	*mtmp = 0;
	MDBM	*m = 0;
	char	*deltakey = 0;
	int	key;
	char	*x;
	char	buf[MAXLINE];

	unless (p || (p = curr_proj())) return (0);

	key = isKey(csetrev);
	mpath = aprintf("%s/BitKeeper/tmp/cset%scache.%x",
	    p->root,
	    key ? "key" : "",
	    crc32c(0, csetrev, strlen(csetrev)));

	if (exists(mpath)) m = mdbm_open(mpath, O_RDONLY, 0600, 0);
	/* validate it still matches rev */
	if (m &&
	    (!(x = mdbm_fetch_str(m, "REV")) || !streq(x, csetrev))) {
		mdbm_close(m);
		m = 0;
	}
	if (m && !key) {
		/* validate it still matches cset file */
		char	*x;

		if (!(x = mdbm_fetch_str(m, "TIPKEY")) ||
		    !streq(x, proj_tipkey(p))) {
			mdbm_close(m);
			m = 0;
		}
	}
	unless (m) {
		sccs	*sc;
		ser_t	d;
		rset_df	*data, *item;

		/* fetch MDBM from ChangeSet */
		concat_path(buf, p->root, CHANGESET);
		unless (sc = sccs_init(buf, SILENT|INIT_NOCKSUM)) goto ret;
		d = sccs_findrev(sc, csetrev);
		unless (d) {
			sccs_free(sc);
			goto ret; /* bad cset rev */
		}
		data = rset_diff(sc, 0, 0, d, 0);
		unless (data) {
			sccs_free(sc);
			goto ret;
		}

		pruneCsetCache(p, basenm(mpath));	/* save newest */

		/* write new MDBM */
		mtmp = aprintf("%s.tmp.%u", mpath, getpid());
		m = mdbm_open(mtmp, O_RDWR|O_CREAT|O_TRUNC, 0666, 0);
		unless (m) {
			FREE(mtmp);
			sccs_free(sc);
			free(data);
			goto ret;
		}
		EACHP(data, item) {
			mdbm_store_str(m, HEAP(sc, item->rkoff),
			    HEAP(sc, item->dkright), MDBM_INSERT);
		}
		free(data);
		sccs_free(sc);
		mdbm_store_str(m, "TIPKEY", proj_tipkey(p), MDBM_REPLACE);
		mdbm_store_str(m, "REV", csetrev, MDBM_REPLACE);
	}
	deltakey = mdbm_fetch_str(m, rootkey);
	if (deltakey) {
		deltakey = strdup(deltakey);
	} else {
		/*
		 * We found the cset but this rookey didn't exist
		 * then.  Have @REV expand to 1.0 in this case.
		 */
		deltakey = strdup(rootkey);
	}
	mdbm_close(m);
 ret:
	if (mtmp) {
		if (m) rename(mtmp, mpath);
		free(mtmp);

	}
	if (mpath) free(mpath);
	return (deltakey);
}

char *
proj_tipkey(project *p)
{
	sccs	*s;
	char	**lines;
	struct	stat sb;
	char	buf[MAXPATH];

	unless (p || (p = curr_proj())) return (0);

	if (p->tipkey) return (p->tipkey);

	/* read TIP file */
	concat_path(buf, p->root, "BitKeeper/log/TIP");
	lines = file2Lines(0, buf);

	concat_path(buf, p->root, CHANGESET);

	/*
	 * History of TIP file:
	 * 1 line - md5key (4.x)
	 * 3 lines - md5key, deltakey, rev (bk-5.0-beta3)
	 * 5 lines - md5key, deltakey, rev, mtime, size (bk-5.4.1)
	 *
	 * We don't compare the ChangeSet file timestamp because the
	 * TIP file is transferred on clone, but sfio doesn't set the
	 * s.ChangeSet timestamp to match the time on the remote side.
	 */
	sb.st_size = 0;
	if ((nLines(lines) < 5) ||
	    lstat(buf, &sb) ||
	    /* (sb.st_mtime != strtoul(lines[4], 0, 0)) || */
	    (sb.st_size != strtoul(lines[5], 0, 0))) {
		/* regenerate TIP file */
		T_SCCS("regen TIP %s n=%d", p->root, nLines(lines));

		/*
		 * In regressions we should never have to recreate the
		 * TIP file because the cached file size is wrong.
		 * This indicates we are not maintaining this cache
		 * correctly.  This can happen in real life when
		 * switching between different versions of bk.
		 */
		if (sb.st_size && getenv("_BK_REGRESSION")) assert(0);

		// should only happen when talking to older bks
		if (s = sccs_init(buf, SILENT|INIT_NOCKSUM|INIT_MUSTEXIST)) {
			cset_savetip(s);
			sccs_free(s);
			concat_path(buf, p->root, "BitKeeper/log/TIP");
			freeLines(lines, free);
			lines = file2Lines(0, buf);
		} else {
			/* Missing ChangeSet file, just keep the old data */
		}
		if (nLines(lines) < 3) goto out; // too old
	}
	p->tipmd5key = lines[1];
	lines[1] = 0;
	p->tipkey = lines[2];
	lines[2] = 0;
	p->tiprev = lines[3];
	lines[3] = 0;
out:	freeLines(lines, free);
	return (p->tipkey);

}

char *
proj_tiprev(project *p)
{
	unless (p || (p = curr_proj())) return (0);

	unless (p->tiprev) proj_tipkey(p);
	return (p->tiprev);
}

char *
proj_tipmd5key(project *p)
{
	unless (p || (p = curr_proj())) return (0);

	unless (p->tipmd5key) proj_tipkey(p);
	return (p->tipmd5key);
}

private u32
scanToBits(char *t)
{
	u32	ret = 0;

	unless (t && *t) return (ret);
	while (*t) {
		switch (*t) {
		    case 'e': ret |= DS_EDITED; break;
		    case 'p': ret |= DS_PENDING; break;
		}
		t++;
	}
	return (ret);
}

private void
scanFromBits(u32 bits, char *out)
{
	if (bits & DS_PENDING) *out++ = 'p';
	if (bits & DS_EDITED) *out++ = 'e';
	*out = 0;
}

/*
 * Update a scandir or scancomps file with changes.
 * We might be doing multiple updates in parallel so we need to do
 * a read/modify/write operation under a lock.
 * returns -1 if a problem occurred.
 */
private int
scanfWrite(char *file, hash *new, char **keys)
{
	int	i, rc;
	char	*k, *v;
	hash	*old;
	char	lock[MAXPATH], tmpf[MAXPATH];

	sprintf(lock, "%s.lock", file);
	if (sccs_lockfile(lock, 16, 0)) {
		fprintf(stderr, "Not updating %s due to locking.\n", file);
		return (-1);
	}
	old = hash_fromFile(hash_new(HASH_MEMHASH), file);
	EACH(keys) {
		k = keys[i];
		assert(!streq(k, "DIRTY"));
		if (v = hash_fetchStr(new, k)) {
			hash_storeStr(old, k, v);
		} else {
			hash_deleteStr(old, k);
		}
	}
	sprintf(tmpf, "%s.tmp", file);
	rc = hash_toFile(old, tmpf) || rename(tmpf, file);
	hash_free(old);
	if (rc) {
		perror(tmpf);
		unlink(tmpf);
		sccs_unlockfile(lock);
		return (-1);
	}
	if (sccs_unlockfile(lock)) return (-1);
	return (0);
}

/*
 * helper function for proj_dirstate() below.
 *
 * it loads the hash file at 'file' into *h and updates 'dir'
 */
private void
scanfUpdate(project *p, char *file, hash **h, char *dir, u32 state, int set)
{
	u32	ostate, nstate;
	char	***dirty, **keys;
	char	buf[16];

	if (set == -1) {
		/* flush cache */
		if (*h) {
			if (keys = hash_fetchStrPtr(*h, "DIRTY")) {
				scanfWrite(file, *h, keys);
				freeLines(keys, free);
			}
			hash_free(*h);
			*h = 0;
		}
	} else {
		unless (*h) *h = hash_fromFile(hash_new(HASH_MEMHASH), file);
		if (set && (ostate = scanToBits(hash_fetchStr(*h, "*")))) {
			/*
			 * if we already have "*" for the same bits, then
			 * we don't need a new entry
			 */
			if ((ostate | state) == ostate) return;
		}
		ostate = scanToBits(hash_fetchStr(*h, dir));
		nstate = (ostate & ~state) | (set * state);
		if (ostate != nstate) {

			hash_insertStrPtr(*h, "DIRTY", 0);
			dirty = (*h)->vptr;
			*dirty = addLine(*dirty, strdup(dir));

			if (nstate) {
				scanFromBits(nstate, buf);
				hash_storeStr(*h, dir, buf);
			} else {
				hash_deleteStr(*h, dir);
			}
		}
	}
}

/*
 * Remember modified/pending state for a directory
 *
 * - if dir == "*", then we are talking about all dirs in component
 * - state is a mask and more than one state can be changed
 * - set==-1 means flush any saved state
 */
void
proj_dirstate(project *p, char *dir, u32 state, int set)
{
	project	*prod;
	char	*file;
	char	*t;
	char	**dirs;
	int	i;
	int	isStar;

	unless (p || (p = curr_proj())) return;
	unless (features_test(p, FEAT_SCANDIRS)) {
		assert(!(p->scancomps || p->scandirs));
		return;
	}

	/*
	 * Changes in RESYNC won't leave files pending or modified for
	 * citool to need to look at them.
	 */
	if (proj_isResync(p)) return;

	isStar = (dir && streq(dir, "*"));
	if ((set || isStar) && (prod = proj_product(p))) {
		/* nested so handle scancomps file */

		file = proj_fullpath(prod, "BitKeeper/log/scancomps");
		t = (p == prod) ? "." : proj_comppath(p);
		scanfUpdate(prod, file, &prod->scancomps, t, state, set);
	}

	/* now update the local file */
	file = proj_fullpath(p, "BitKeeper/log/scandirs");
	if (isStar) {
		assert(state);
		dirs = 0;
		EACH_HASH(p->scandirs) {
			if (streq(p->scandirs->kptr, "DIRTY")) continue;
			dirs = addLine(dirs, p->scandirs->kptr);
		}
		/* clear bits for all existing dirs */
		EACH(dirs) {
			scanfUpdate(p, file, &p->scandirs, dirs[i], state, 0);
		}
		freeLines(dirs, 0);
		/* and set "*" entry ... */
		if (set) goto set;
	} else {
set:		scanfUpdate(p, file, &p->scandirs, dir, state, set);
	}
}

/*
 * Returns the list of directories that might have any of the 'state'
 * bits set.  Returns 0, if we don't know or not enabled.
 */
char **
proj_scanDirs(project *p, u32 state)
{
	char	**ret;
	char	*file;

	unless (p || (p = curr_proj())) return (0);
	unless (features_test(p, FEAT_SCANDIRS)) return (0);
	unless (p->scandirs) {
		file = proj_fullpath(p, "BitKeeper/log/scandirs");
		p->scandirs =  hash_fromFile(hash_new(HASH_MEMHASH), file);
	}
	/* any overlap with * then we scan everything */
	if (state & scanToBits(hash_fetchStr(p->scandirs, "*"))) return (0);

	ret = allocLines(16);  // must return non-zero
	EACH_HASH(p->scandirs) {
		if (state & scanToBits(p->scandirs->vptr)) {
			ret = addLine(ret, p->scandirs->kptr);
		}
	}
	sortLines(ret, 0);
	return (ret);
}

/*
 * Returns the list of components that might have the 'state' bits
 * set.
 * Returns 0, if we don't know or not enabled.
 */
char **
proj_scanComps(project *p, u32 state)
{
	char	**ret;
	char	*file;

	unless (p || (p = curr_proj())) return (0);
	unless (features_test(p, FEAT_SCANDIRS)) return (0);
	unless (p = proj_product(p)) return (0);

	unless (p->scancomps) {
		file = proj_fullpath(p, "BitKeeper/log/scancomps");
		p->scancomps =  hash_fromFile(hash_new(HASH_MEMHASH), file);
	}
	ret = allocLines(4);
	EACH_HASH(p->scancomps) {
		if (state & scanToBits(p->scancomps->vptr)) {
			ret = addLine(ret, p->scancomps->kptr);
		}
	}
	sortLines(ret, 0);
	return (ret);
}

p_feat *
proj_features(project *p)
{
	unless (p || (p = curr_proj())) return (0);

	return (&p->features);
}
