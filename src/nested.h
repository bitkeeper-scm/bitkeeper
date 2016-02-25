/*
 * Copyright 2008-2013,2016 BitMover, Inc
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

#ifndef _NESTED_H
#define _NESTED_H

/*
 * XXX non nested crud shoved here for the moment for historical reasons
 */

#define	csetChomp(path)	{			\
	char	*slash = strrchr(path, '/');	\
	assert(slash);				\
	*slash = 0;				\
	}

extern	unsigned int turnTransOff;
#define	START_TRANSACTION()						\
	do {								\
		unless (getenv("_BK_TRANSACTION")) {			\
			putenv("_BK_TRANSACTION=1");			\
			turnTransOff = 1;				\
			T_NESTED("Starting %s", "transaction");		\
		} else if (turnTransOff) {				\
			T_NESTED("Starting transaction nest %u", turnTransOff);\
			turnTransOff++;					\
		} else {						\
			T_NESTED("Skipping Starting %s", "transaction");\
		}							\
	} while(0)

#define	STOP_TRANSACTION()						\
	do {								\
		if (turnTransOff == 1)	{				\
			putenv("_BK_TRANSACTION=");			\
			turnTransOff = 0;				\
			T_NESTED("Stopping %s", "transaction");		\
		} else if (turnTransOff) {				\
			turnTransOff--;					\
			T_NESTED("Stopping transaction nest %u", turnTransOff);\
		} else {						\
			T_NESTED("Skipping Stopping %s", "transaction");\
		}							\
	} while(0)

/*
 * End of crud
 */

#define	NESTED_PENDING		0x10000000	/* included pending comps */
#define	NESTED_PULL		0x20000000	/* This is a pull */
#define	NESTED_PRODUCTFIRST	0x40000000	/* c->p first in c->comps */
#define	NESTED_MARKPENDING	0x01000000	/* set c->pending */
#define	NESTED_FIXIDCACHE	0x02000000	/* no error for bad idDB */

#define	NESTED_URLLIST		"BitKeeper/log/urllist"

#define	PRODUCT			"."		/* for titles */

#define	C_PRESENT(c)	({		\
	if ((c)->_present == -1) {	\
		compCheckPresent(c);	\
	}				\
	(c)->_present;			\
})

#define	C_PENDING(c)	({		\
	if ((c)->_pending == -1) {	\
		compMarkPending(c);	\
	}				\
	c->_pending;			\
})

#define	C_DELTAKEY(c)	({		\
	if ((c)->_pending == -1) {	\
		compMarkPending(c);	\
	}				\
	(c)->_deltakey;			\
})

typedef struct nested nested;

typedef struct {
	nested	*n;			// backpointer
	char	*rootkey;		// rootkey of the repo
	char	*_deltakey;		// deltakey of repo as of rev
	char	*lowerkey;		// in pull, local tip
	char	**poly;			// info needed for poly
	char	*path;			// actual path: like GFILE, not DPN

	void	*data;			// scratchpad for applications
	int	_present;		// if set, the repo is actually here
					// -1 means it hasn't been stated
	int	_pending;		// has pending csets not in product
					// -1 means it hasn't been checked
	// bits
	u32	alias:1;		// in the latest alias
	u32	included:1;		// component modified in 'revs'
	u32	inCache:1;		// comp in idcache
	u32	localchanges:1;		// component modified outside 'revs'
	u32	new:1;			// if set, the undo will remove this
	u32	product:1;		// this is the product
	u32	remotePresent:1;	// scratch for remote present bit
	u32	useLowerKey:1;		// tell populate to use lowerkey
	u32	savedGate:1;		// wrote gate to urllist for this comp?
	u32	gca:1;			// seen something in gca
} comp;

struct nested {
	char	**here;		// the contents of the here file
	char	**comps;	// addlines of pointers to components
	char	*oldtip;	// tip before revs (new tip for undo)
	char	*tip;		// newest cset in revs
	sccs	*cset;		// cache of cset file
	project	*proj;
	hash	*aliasdb;	// lazy init'd aliasdb
	hash	*compdb;	// lazy init rk lookup of &n->comp[i]
	comp	*product;	// pointer into comps to the product

	// urllist
	u32	list_loaded:1;	// urllist file loaded
	u32	list_dirty:1;	// urllist data changed
	char	**urls;		// array of urlinfo *'s
	hash	*urlinfo;	// info about urls

	// bits
	u32	alias:1;	// components tagged with c->alias
	u32	pending:1;	// include pending component state
	u32	freecset:1;	// do a sccs_free(cset) in nested_free()
	u32	fix_idDB:1;	// don't complain about idDB problems
};

typedef struct {
	// normalized url, output of remote_unparse() with any leading
	// file:// removed
	char	*url;

	// map comp struct pointers to "1" if remote has the needed
	// component tipkey or "0" if they have the component, but not
	// the tipkey.
	hash	*pcomps;	/* populated components found in this URL */
	u32	checked:1;	/* have we actually connected? */
	u32	checkedGood:1;	/* was URL probe successful this time? */
	u32	noconnect:1;	/* probeURL failed for connection problem */
	u32	from:1;		/* if set, try this one first */
	u32	parent:1;	/* if set, try this one first (or next) */

	// From URLINFO file, extras are ignored
	time_t	time;		/* 1 time of last successful connection */
	int	gate;		/* 2 do we think it's a gate? (checked == 0)
				 * or do we know it's a gate (checked == 1)
				 */
	char	*repoID;	/* 3 */
	char	**extra;	/* extra data we don't parse */
} urlinfo;

/*
 * This is the subset of the clone opts we pass down to nested_populate.
 */
typedef struct {
	u32	debug:1;	// -d: debug mode
	u32	no_lclone:1;	// --no-hardlinks: don't hard link the files
	u32	quiet:1;	// -q: quiet
	u32	verbose:1;	// -v: verbose
	u32	force:1;	// -f: force unpopulate with local diffs
	u32	noURLprobe:1;	// unpopulate doesn't look for other urls
	u32	runcheck:1;	// follow up with a partial check of prod
	u32	leaveHERE:1;	// do not update the HERE file with our actions
	int	parallel;	// -j%d
	int	comps;		// number of comps we worked on

	/* internal state */

	/* copy of nested_populated() args */
	nested	*n;
	char	*lasturl;		/* last URL printed */
} popts;

/*
 * For getting info about locks
 */
typedef struct {
	char	*nlid;		/* nested lock id */
	int	stale;		/* whether said lock id is stale or not */
} nlock;

int	isComponent(char *path);

nested	*nested_init(sccs *cset, char *rev, char **revs, u32 flags);
void	nested_free(nested *n);
int	nested_aliases(nested *n, char *rev, char ***aliases, char *cwd,
	    int pending);
comp	*nested_findKey(nested *n, char *rootkey);
comp	*nested_findMD5(nested *n, char *md5rootkey);
comp	*nested_findDir(nested *n, char *dir, int exact);
char	*nested_dir2key(nested *n, char *dir);
void	nested_compFree(void *x);
int	nested_each(int quiet, char **av, char **aliases);
void	nested_check(void);
int	nested_emptyDir(nested *n, char *dir);
int	nested_rmcomp(nested *n, comp *c);
char	**nested_here(project *p);
char	**nested_fixHere(char **aliases);
void	nested_writeHere(nested *n);
char	**nested_complist(nested *n, project *p);
char	**modified_pending(u32 flags);

/* alias.h */

#define	ALIASES		"BitKeeper/etc/aliases"
#define	SALIASES	"BitKeeper/etc/SCCS/s.aliases"
#define	COMPLIST	"BitKeeper/log/COMPS"

hash	*aliasdb_init(nested *n,
    project *p, char *rev, int pending, int no_diffs);
int	aliasdb_tag(nested *n, hash *aliasdb, char **aliases);
char	**aliasdb_expandOne(nested *n, hash *aliasdb, char *alias);
void	aliasdb_free(hash *db);
int	aliasdb_chkAliases(nested *n, hash *aliasdb,
	    char ***paliases, char *cwd);
int	aliasdb_caret(char **aliases);
char	**alias_coverMissing(nested *n, char **missing, char **aliases);

/* urlinfo.c */
void	urlinfo_load(nested *n, remote *base);
void	urlinfo_buildArray(nested *n);
void	urlinfo_urlArgs(nested *n, char **urls);

void	urlinfo_addURL(nested *n, comp *c, char *url);
void	urlinfo_rmURL(nested *n, comp *c, char *url);

int	urlinfo_probeURL(nested *n, char *url, FILE *out);

void	urlinfo_setFromEnv(nested *n, char *url);
int	urlinfo_write(nested *n);
void	urlinfo_free(nested *n);
void	urlinfo_flushCache(nested *n);

/* clone.c */
char	**clone_defaultAlias(nested *n);

/* here_check.c */
int	here_check_main(int ac, char **av);

/* locking.c */

typedef enum {
	NL_OK,
	NL_NOT_NESTED,
	NL_NOT_PRODUCT,
	NL_ALREADY_LOCKED,
	NL_LOCK_FILE_NOT_FOUND,
	NL_MISMATCH,
	NL_COULD_NOT_LOCK_RESYNC,
	NL_COULD_NOT_LOCK_NOT_MINE,
	NL_COULD_NOT_UNLOCK,
	NL_INVALID_LOCK_STRING,
	NL_ABORT_FAILED,
	NL_COULD_NOT_GET_MUTEX
} nle_t;			/* nl_errno */
extern	nle_t	nl_errno;

char	*nested_wrlock(project *p);
char	*nested_rdlock(project *p);
int	nested_mine(project *p, char *t, int write);
int	nested_unlock(project *p, char *t);
int	nested_forceUnlock(project *p, int kind);
int	nested_abort(project *p, char *t);
char	*nested_errmsg(void);
/* nested_lockers returns nlock *'s in the addLines */
char	**nested_lockers(project *p, int listStale, int removeStale);
int	nested_printLockers(project *p,
    int listStale, int removeStale, FILE *out);
void	nested_updateIdcache(project *comp);
int	nested_isPortal(project *comp);
int	nested_isGate(project *comp);
int	nested_makeComponent(char *dir);
void	freeNlock(void *nl);
void	compCheckPresent(comp *c);
void	compMarkPending(comp *c);


/* populate.c */
int	nested_populate(nested *n, popts *ops);

#define	URLLIST_GATEONLY	0x40	/* only gates */
#define	URLLIST_NOERRORS	0x80 	/* don't print errors to stderr */
char	*urllist_find(nested *n, comp *cp, int flags, int *idx);

#endif	// _NESTED_H
