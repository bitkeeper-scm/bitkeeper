#include "system.h"
#include "sccs.h"

#if 0
#define pdbg(x) do { \
			ttyprintf("%5d: ", getpid()); \
			ttyprintf x ; \
		} while (0);
#else
#define	pdbg(x)
#endif

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
	char	*csetrootkey;	/* Root key of ChangeSet file */
	MDBM	*config;	/* config DB */

	/* internal state */
    	int	refcnt;
	project	*next;
	dirlist	*dirs;
};

private char	*find_root(char *dir);

private project	*proj_curr = 0;
private project	*proj_last = 0;
private	project *proj_master_list = 0;
private MDBM	*projcache = 0;

private project *
projcache_lookup(char *dir)
{
	project	*ret = 0;
	datum	k, v;

	unless (projcache) {
		projcache = mdbm_mem();
		pdbg(("mdbm_mem()\n"));
	}
	k.dptr = (char *)dir;
	k.dsize = strlen(dir) + 1;
	v = mdbm_fetch(projcache, k);
	if (v.dsize) {
		assert(v.dsize == sizeof(ret));
		memcpy(&ret, v.dptr, sizeof(ret));
	}
	pdbg(("mdbm_fetch(%s) = %p\n", dir, ret));
	return (ret);
}

private void
projcache_store(char *dir, project *p)
{
	datum	k, v;
	int	status;


	unless (streq(dir, p->root)) {
		dirlist	*dl;

		new(dl);
		dl->dir = strdup(dir);
		dl->next = p->dirs;
		p->dirs = dl;
	}
	k.dptr = (char *)dir;
	k.dsize = strlen(dir) + 1;
	v = mdbm_fetch(projcache, k);
	assert(v.dsize == 0);
	v.dptr = (char *)&p;
	v.dsize = sizeof(p);
	pdbg(("mdbm_store(%s, %p)\n", dir, p));
	status = mdbm_store(projcache, k, v, MDBM_INSERT);
	if (status) {
		pdbg(("mdbm_store: exists\n"));
		assert(0);
	}
}

private void
projcache_delete(project *p)
{
	datum	k;
	int	status;
	dirlist	*dl;

	k.dptr = p->root;
	k.dsize = strlen(p->root) + 1;

	pdbg(("mdbm_delete(%s)\n", p->root));
	status = mdbm_delete(projcache, k);
	assert(status == 0);
	dl = p->dirs;
	while (dl) {
		dirlist	*tmp = dl;

		k.dptr = dl->dir;
		k.dsize = strlen(dl->dir) + 1;

		pdbg(("mdbm_delete(%s)\n", dl->dir));
		status = mdbm_delete(projcache, k);
		assert(status == 0);

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
	pdbg(("proj_init() = %p (ref %d)\n", ret, ret->refcnt));
	return (ret);
}

void
proj_free(project *p)
{
	project	**m;
	assert(p);

	pdbg(("proj_free(%p (ref %d))\n", p, p->refcnt));
	unless (--p->refcnt == 0) return;

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
	char	buf[MAXPATH];

	unless (p) p = curr_proj();
	unless (p) return (0);

	/*
	 * Use cached copy if available
	 */
	unless (p->csetrootkey) {
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
			sccs_free(sc);
			p->csetrootkey = strdup(buf);
		}
	}
	return (p->csetrootkey);
}

void
proj_reset(project *p)
{
	if (p) {
		if (p->csetrootkey) {
			free(p->csetrootkey);
			p->csetrootkey = 0;
		}
		if (p->config) {
			mdbm_close(p->config);
			p->config = 0;
		}
	} else {
		pdbg(("proj_reset(0)\n"));
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

