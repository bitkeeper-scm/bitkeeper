#include "system.h"
#include "sccs.h"
#include "logging.h"

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
	MDBM	*config;	/* config DB */
	project	*rparent;	/* if RESYNC, point at enclosing repo */

	/* per proj cache data */
	char	*license;
	u32	licensebits;
	u8	leaseok:1;

	/* internal state */
    	int	refcnt;
	dirlist	*dirs;
};

private char	*find_root(char *dir);

private struct {
	project	*curr;
	project	*last;
	HASH	*cache;
} proj;

private project *
projcache_lookup(char *dir)
{
	project	*ret = 0;
	project	**p;

	unless (proj.cache) proj.cache = hash_new();
	if (p = hash_fetch(proj.cache, dir, 0, 0)) ret = *p;
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
	*(project **)hash_alloc(proj.cache, dir, 0, sizeof(p)) = p;
}

private void
projcache_delete(project *p)
{
	dirlist	*dl;

	hash_delete(proj.cache, p->root, 0);
	dl = p->dirs;
	while (dl) {
		dirlist	*tmp = dl;

		hash_delete(proj.cache, dl->dir, 0);

		free(dl->dir);
		dl = dl->next;
		free(tmp);
	}
}

project *
proj_init(char *dir)
{
	project	*ret;
	char	*root;
	char	*t;

	if (ret = projcache_lookup(dir)) goto done;

	/* missed the cache */
	unless (root = find_root(fullname(dir, 0))) return (0);

	unless (streq(root, dir)) {
		/* I wasn't in root, was root it cache? */
		if (ret = projcache_lookup(root)) {
			/* yes, make a new mapping */
			if (IsFullPath(dir)) projcache_store(dir, ret);
			goto done;
		}
	}
	assert(ret == 0);
	/* Totally new project */
	new(ret);
	ret->root = root;

	projcache_store(root, ret);
	if (!streq(root, dir) && IsFullPath(dir)) projcache_store(dir, ret);
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

char *
proj_root(project *p)
{
	unless (p) p = curr_proj();
	unless (p) return (0);
	assert(p->root);
	return (p->root);
}

int
proj_cd2root(void)
{
	if (proj_root(0) && (chdir(proj_root(0)) == 0)) return (0);
	return (-1);
}

char *
proj_relpath(project *p, char *path)
{
	char	*root = proj_root(p);
	int	len;

	assert(root);
	unless (IsFullPath(path)) path = fullname(path, 0);
	len = strlen(root);
	if (pathneq(root, path, len)) {
		assert(path[len] == '/');
		return(strdup(&path[len+1]));
	} else {
		fprintf(stderr, "Path mismatch?: %s <=> %s\n",
			root, path);
		return (0);
	}
}

// remove all other calls to loadConfig ??
MDBM *
proj_config(project *p)
{
	unless (p) p = curr_proj();
	unless (p) return (0);
	unless (p->config) p->config = loadConfig(proj_root(p));
	return (p->config);
}

/*
 * Return the ChangeSet file id.
 */
char	*
proj_rootkey(project *p)
{
	sccs	*sc;
	FILE	*f;
	char	*ret;
	char	rkfile[MAXPATH];
	char	buf[MAXPATH];

	unless (p) p = curr_proj();
	unless (p) return (0);

	/*
	 * Use cached copy if available
	 */
	if (p->rootkey) return (p->rootkey);
	if (p->rparent && (ret = proj_rootkey(p->rparent))) return (ret);

	sprintf(rkfile, "%s/BitKeeper/tmp/ROOTKEY", p->root);
	if (f = fopen(rkfile, "rt")) {
		fnext(buf, f);
		chomp(buf);
		p->rootkey = strdup(buf);
		fnext(buf, f);
		chomp(buf);
		p->md5rootkey = strdup(buf);
		fclose(f);
	} else {
		sprintf(buf, "%s/%s", p->root, CHANGESET);
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

char *
proj_md5rootkey(project *p)
{
	char	*ret;

	unless (p || (p = curr_proj())) return (0);

	unless (p->md5rootkey) proj_rootkey(p);
	if (p->rparent && (ret = proj_md5rootkey(p->rparent))) return (ret);
	return (p->md5rootkey);
}

void
proj_reset(project *p)
{
	kvpair	kv;

	if (p) {
		if (p->rootkey) {
			free(p->rootkey);
			p->rootkey = 0;
			free(p->md5rootkey);
			p->md5rootkey = 0;
		}
		if (p->config) {
			mdbm_close(p->config);
			p->config = 0;
		}
		if (p->license) {
			free(p->license);
			p->license = 0;
		}
		p->licensebits = 0;
		p->leaseok = 0;
	} else {
		kv = hash_first(proj.cache);
		while (kv.key.dptr) {
			proj_reset(*(project **)kv.val.dptr);
			kv = hash_next(proj.cache);
		}
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

int
proj_chdir(char *newdir)
{
	int	ret;

	ret = chdir(newdir);
	unless (ret) {
		if (proj.curr) {
			if (proj.last) proj_free(proj.last);
			proj.last = proj.curr;
			proj.curr = 0;
		}
	}
	return (ret);
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
	ret->root = strdup("/");
	ret->rootkey = strdup("SCCS");
	ret->md5rootkey = strdup("SCCS");
	projcache_store("/", ret);

	return (ret);
}

char *
proj_license(project *p)
{
	/*
	 * If we are outside of any repository then we must get a
	 * licence from the global config or a license server.
	 */
	unless (p || (p = curr_proj())) p = proj_fakenew();

	unless (p->license) p->license = lease_licenseKey(p);
	return (p->license);
}

u32
proj_licensebits(project *p)
{
	/*
	 * If we are outside of any repository then we must get a
	 * licence from the global config or a license server.
	 */
	unless (p || (p = curr_proj())) p = proj_fakenew();

	unless (p->licensebits) p->licensebits = fetchLicenseBits(p);
	return (p->licensebits);
}

int
proj_leaseOK(project *p, int *newok)
{
	unless (p || (p = curr_proj())) return (0);

	if (newok) p->leaseok = *newok;
	return (p->leaseok);
}
