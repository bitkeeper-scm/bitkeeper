#ifndef _NESTED_H
#define _NESTED_H

/*
 * XXX non nested crud shoved here for the moment for historical reasons
 */

#define	ALIASES	"BitKeeper/etc/aliases"

#define	csetChomp(path)	{			\
	char	*slash = strrchr(path, '/');	\
	assert(slash);				\
	*slash = 0;				\
	}

#define	ALIAS_HERE	0x10000000	/* Require the aliases to be here */
#define	ALIAS_PENDING	0x20000000	/* Include aliases not in a cset */

hash*	alias_hash(char **names, sccs *cset, char *rev, u32 flags);
char*	alias_md5(char *name, sccs *cset, char *rev, u32 flags);

/* db routines called by alias */
hash	*aliasdb_init(char *rev, int pending);
void	aliasdb_free(hash *db);

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

#define	NESTED_PENDING		0x10000000
#define	NESTED_PRODUCT		0x20000000
#define	NESTED_PRODUCTFIRST	0x40000000
#define	NESTED_UNDO		0x80000000
#define	NESTED_DEEPFIRST	0x01000000
#define	NESTED_ALIASDB		0x02000000	/* build the aliasdb */
#define	NESTED_LOOKUP		0x04000000	/* fast lookup db built */

typedef struct nested nested;

typedef struct {
	nested	*n;			// backpointer
	char	*rootkey;		// rootkey of the repo
	char	*deltakey;		// deltakey of repo as of rev
	char	*path;			// actual path: like GFILE, not DPN
					// use accesor to fetch
	int	nlink;			// alias link count
	u32	new:1;			// if set, the undo will remove this
	u32	present:1;		// if set, the repo is actually here
	u32	product:1;		// this is the product
	u32	realpath:1;		// this path is from idDB
} comp;

struct nested {
	char	**comps;	// addlines of pointers to components
	char	*rev;		// new for push and pull, old for undo
				// used by aliasdb for context
	sccs	*cset;		// cache of cset file
	hash	*aliasdb;	// lazy init'd aliasdb
	hash	*compdb;	// lazy init rk lookup of &n->comp[i]
	// bits
	u32	product:1;	// include the product in the list
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
int	nested_filterAlias(nested *n, char **aliases);
nested	*nested_fromStream(nested *n, FILE *in);
int	nested_toStream(nested *n, FILE *out);
int	nested_find(nested *n, char *rootkey);
char	*nested_dir2key(nested *n, char *dir);
void	nested_compFree(void *x);
int	nested_each(int quiet, int ac, char **av);
void	nested_check(void);

char	*comp_path(comp *c);

#endif	// _NESTED_H
