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
			TRACE("Starting %s", "transaction");		\
		} else if (turnTransOff) {				\
			TRACE("Starting transaction nest %u", turnTransOff); \
			turnTransOff++;					\
		} else {						\
			TRACE("Skipping Starting %s", "transaction");	\
		}							\
	} while(0)

#define	STOP_TRANSACTION()						\
	do {								\
		if (turnTransOff == 1)	{				\
			putenv("_BK_TRANSACTION=");			\
			turnTransOff = 0;				\
			TRACE("Stopping %s", "transaction");		\
		} else if (turnTransOff) {				\
			turnTransOff--;					\
			TRACE("Stopping transaction nest %u", turnTransOff); \
		} else {						\
			TRACE("Skipping Stopping %s", "transaction");	\
		}							\
	} while(0)

/*
 * End of crud
 */

#define	NESTED_PENDING		0x10000000	/* included pending comps */
#define	NESTED_PRODUCTFIRST	0x40000000
#define	NESTED_UNDO		0x80000000
#define	NESTED_DEEPFIRST	0x01000000
#define	NESTED_LOOKUP		0x04000000	/* fast lookup db built */

typedef struct nested nested;

typedef struct {
	nested	*n;			// backpointer
	char	*rootkey;		// rootkey of the repo
	char	*deltakey;		// deltakey of repo as of rev
	char	*lowerkey;		// In a revs, lower deltakey in range
	char	*path;			// actual path: like GFILE, not DPN
					// use accesor to fetch
	int	nlink;			// alias link count (deprecated)

	// bits
	u32	alias:1;		// in the latest alias
	u32	included:1;		// component modified in 'revs'
	u32	new:1;			// if set, the undo will remove this
	u32	present:1;		// if set, the repo is actually here
	u32	product:1;		// this is the product
	u32	remotePresent:1;	// scratch for remote present bit
	u32	realpath:1;		// this path is from idDB
} comp;

struct nested {
	char	**comps;	// addlines of pointers to components
	char	*oldtip;	// tip before revs (new tip for undo)
	char	*tip;		// newest cset in revs
	sccs	*cset;		// cache of cset file
	hash	*aliasdb;	// lazy init'd aliasdb
	hash	*compdb;	// lazy init rk lookup of &n->comp[i]
	comp	*product;	// pointer into comps to the product
	// bits
	u32	alias:1;	// nlink counts set in components
	u32	revs:1;		// revs was used, so list may not be full
	u32	product_first:1;// default is last in list
	u32	undo:1;		// undo wants the -a inferred from opts.revs
	u32	deepfirst:1;	// sort such that deeply nested comps are first
	u32	pending:1;	// include pending component state
	u32	freecset:1;	// do a sccs_free(cset) in nested_free()
};

/*
 * XXX: who frees the revs list? Is this a pass off or ?
 * Same with cset?
 * One model is if caller passes in, caller frees.
 */
int	isComponent(char *path);

nested	*nested_init(sccs *cset, char *rev, char **revs, u32 flags);
void	nested_free(nested *n);
int	nested_filterAlias(nested *n, hash *aliasdb, char **aliases);
comp	*nested_findKey(nested *n, char *rootkey);
comp	*nested_findMD5(nested *n, char *md5rootkey);
comp	*nested_findDir(nested *n, char *dir);
char	*nested_dir2key(nested *n, char *dir);
void	nested_compFree(void *x);
int	nested_each(int quiet, int ac, char **av);
void	nested_check(void);
int	nested_emptyDir(char *dir);
int	nested_rmtree(char *dir);

char	*comp_path(comp *c);

/* alias.h */

#define	ALIASES	"BitKeeper/etc/aliases"

hash	*aliasdb_init(nested *n, project *p, char *rev, int pending);
char	**aliasdb_expand(nested *n, hash *aliasdb, char **aliases);
char	**aliasdb_expandOne(nested *n, hash *aliasdb, char *alias);
void	aliasdb_free(hash *db);
int	aliasdb_chkAliases(nested *n, hash *aliasdb,
	    char ***paliases, char *cwd);
char	**aliases_here(project *p);

#endif	// _NESTED_H
