#ifndef _NESTED_H
#define _NESTED_H

#define	ALIASES	"BitKeeper/etc/aliases"

#define	csetChomp(path)	{			\
	char	*slash = strrchr(path, '/');	\
	assert(slash);				\
	*slash = 0;				\
	}

typedef struct {
	void	**repos;	// addlines of pointers to repo's
	char	*rootkey;	// points at repos[i]->rootkey
	char	*deltakey;	// ditto
	char	*path;		// ditto
	u32	new:1;		// ditto
	u8	present;	// ditto
	int	index;		// used by the iterator
} repos;

typedef	struct {
	sccs	*sc;		// product changeset file if set
	char	*rev;		// if set, we want the deltakeys as of this
	char	**revs;		// if set, limit the components to these csets 
	hash	*aliases;	// if set, limit the comps to these root keys
	u32	product:1;	// include the product in the list
	u32	product_first:1;// default is last in list
	u32	undo:1;		// undo wants the -a inferred from opts.revs
	u32	deepfirst:1;	// sort such that deeply nested comps are first
	u32	pending:1;	// use any pending deltakeys for comps
} eopts;

/*
 * These are like the DELTA_*, INIT_*, .. flags in sccs.h
 * They are used in alias_hash and alias_md5
 */

#define	ALIAS_HERE	0x10000000	/* Require the aliases to be here */
#define	ALIAS_PENDING	0x20000000	/* Include aliases not in a cset */

repos*	ensemble_list(eopts opts);
int	ensemble_find(repos *repos, char *rootkey);
char*	ensemble_dir2key(repos *repos, char *dir);
repos*	ensemble_first(repos *repos);
repos*	ensemble_next(repos *repos);
void	ensemble_free(repos *repos);

int	ensemble_toStream(repos *repos, FILE *f);
repos*	ensemble_fromStream(repos *repos, FILE *f);

int	ensemble_each(int quiet, int ac, char **av);
hash*	alias_hash(char **names, sccs *cset, char *rev, u32 flags);
char*	alias_md5(char *name, sccs *cset, char *rev, u32 flags);
int	isComponent(char *path);
void	ensemble_nestedCheck(void);
int	ensemble_emptyDir(char *dir);
int	ensemble_rmtree(char *dir);
#define	EACH_REPO(c)	for (ensemble_first(c); (c)->index; ensemble_next(c))

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
#endif /* _NESTED_H */
