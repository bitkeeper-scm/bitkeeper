#include "system.h"
#include "sccs.h"

/*
 * Don't treat chdir() special here.
 */
#undef	chdir

typedef struct dirlist dirlist;
struct dirlist {
	char	*dir;
	dirlist	*next;
};

struct project {
	char	*root;		/* fullpath root of the project */
	char	*csetrootkey;	/* Root key of ChangeSet file */
	char	*csetmd5rootkey;/* MD5 root key of ChangeSet file */
	MDBM	*config;	/* config DB */
	HASH	*hash;		/* misc data, stored per tree */
	project	*parent;	/* set if this is a RESYNC proj */

	/* internal state */
    	int	refcnt;
	project	*next;
	dirlist	*dirs;
};

private char	*find_root(char *dir);

private project	*proj_curr = 0;
private project	*proj_last = 0;
private	project *proj_master_list = 0;
private HASH	*projcache = 0;

private project *
projcache_lookup(char *dir)
{
	project	*ret = 0;
	project	**p;

	unless (projcache) projcache = hash_new();
	if (p = hash_fetch(projcache, dir, 0, 0)) ret = *p;
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
	*(project **)hash_alloc(projcache, dir, 0, sizeof(p)) = p;
}

private void
projcache_delete(project *p)
{
	dirlist	*dl;

	hash_delete(projcache, p->root, 0);
	dl = p->dirs;
	while (dl) {
		dirlist	*tmp = dl;

		hash_delete(projcache, dl->dir, 0);

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
	ret->next = proj_master_list;
	proj_master_list = ret;

	projcache_store(root, ret);
	if (!streq(root, dir) && IsFullPath(dir)) projcache_store(dir, ret);
done:
	++ret->refcnt;

	if ((t = strrchr(ret->root, '/')) && streq(t, "/RESYNC")) {
		*t = 0;
		ret->parent = proj_init(ret->root);
		if (ret->parent && !streq(ret->parent->root, ret->root)) {
			proj_free(ret->parent);
			ret->parent = 0;
		}
		*t = '/';
	}
	return (ret);
}

void
proj_free(project *p)
{
	project	**m;
	assert(p);

	unless (--p->refcnt == 0) return;

	if (p->parent) {
		proj_free(p->parent);
		p->parent = 0;
	}
	projcache_delete(p);

	proj_reset(p);
	free(p->root);
	/* unlink p from master list */
	m = &proj_master_list;
	while (*m && *m != p) {
		m = &(*m)->next;
	}
	if (*m) *m = p->next;
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
	unless (proj_curr) proj_curr = proj_init(".");
	return (proj_curr);
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
proj_csetrootkey(project *p)
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
	if (p->csetrootkey) return (p->csetrootkey);
	if (p->parent && (ret = proj_csetrootkey(p->parent))) return (ret);

	sprintf(rkfile, "%s/BitKeeper/tmp/ROOTKEY", p->root);
	if (f = fopen(rkfile, "rt")) {
		fnext(buf, f);
		chomp(buf);
		p->csetrootkey = strdup(buf);
		fnext(buf, f);
		chomp(buf);
		p->csetmd5rootkey = strdup(buf);
		fclose(f);
	} else {
		sprintf(buf, "%s/%s", p->root, CHANGESET);
		if (exists(buf)) {
			/*
			 * XXX This is still doing too much work to
			 * get the root key We don't need to init the
			 * whole graph
			 */
			sc = sccs_init(buf, INIT_NOCKSUM|INIT_NOSTAT);
			assert(sc->tree);
			sccs_sdelta(sc, sc->tree, buf);
			p->csetrootkey = strdup(buf);
			sccs_md5delta(sc, sc->tree, buf);
			p->csetmd5rootkey = strdup(buf);
			sccs_free(sc);
			if (f = fopen(rkfile, "wt")) {
				fputs(p->csetrootkey, f);
				putc('\n', f);
				fputs(p->csetmd5rootkey, f);
				putc('\n', f);
				fclose(f);
			}
		}
	}
	return (p->csetrootkey);
}

char *
proj_csetmd5rootkey(project *p)
{
	char	*ret;

	unless (p || (p = curr_proj())) return (0);

	unless (p->csetmd5rootkey) proj_csetrootkey(p);
	if (p->parent && (ret = proj_csetmd5rootkey(p->parent))) return (ret);
	return (p->csetmd5rootkey);
}

HASH *
proj_hash(project *p)
{
	unless (p || (p = curr_proj())) return (0);

	unless (p->hash) p->hash = hash_new();
	return (p->hash);
}

void
proj_reset(project *p)
{
	if (p) {
		if (p->csetrootkey) {
			free(p->csetrootkey);
			p->csetrootkey = 0;
			free(p->csetmd5rootkey);
			p->csetmd5rootkey = 0;
		}
		if (p->config) {
			mdbm_close(p->config);
			p->config = 0;
		}
		if (p->hash) {
			hash_free(p->hash);
			p->hash = 0;
		}
	} else {
		p = proj_master_list;
		while (p) {
			proj_reset(p);
			p = p->next;
		}
		/* free the current project for purify */
		if (proj_curr) {
			proj_free(proj_curr);
			proj_curr = 0;
		}
		if (proj_last) {
			proj_free(proj_last);
			proj_last = 0;
		}
	}
}

int
proj_chdir(char *newdir)
{
	int	ret;

	ret = chdir(newdir);
	unless (ret) {
		if (proj_curr) {
			if (proj_last) proj_free(proj_last);
			proj_last = proj_curr;
			proj_curr = 0;
		}
	}
	return (ret);
}

