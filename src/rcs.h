#include "system.h"
#include "sccs.h"

/*
 * Parse just enough of the RCS file to convert them.
 * Did I mention that I really hate RCS?
 */

/*
 * Struct delta - describes a single delta entry.
 */
typedef struct rdelta {
	char	*rev;			/* revision number */
	char	*sccsrev;		/* new revision number */
	char	*sdate;			/* ascii date, 93.07.25.21.14.11 */
	char	*author;		/* user name of delta owner */
	char	*comments;		/* Comment log */
	time_t	date;			/* date - conversion from sdate/zone */
	time_t	dateFudge;		/* make dates go forward */
	struct	rdelta *parent;		/* parent delta above me */
	struct	rdelta *kid;		/* next delta on this branch */
	struct	rdelta *next;		/* all deltas in table order */
	struct	rdelta *prev;		/* all deltas in reverse table order */
	char	*snext;			/* next as a revision */
	u32	ingraph:1;		/* inserted in the graph */
	u32	printed:1;		/* been here */
	u32	head:1;			/* branch head */
} rdelta;

typedef	struct sym {
	char	*name;
	char	*rev;
	struct	sym *next;
} sym;

/*
 * struct RCS
 */
typedef	struct {
	rdelta	*tree;		/* the delta tree after mkgraph() */
	rdelta	*table;		/* the delta table list, 1.99 .. 1.0 */
	rdelta	*lastinsert;	/* pointer to the last delta inserted */
	int	n;		/* number of deltas */
	sym	*symbols;	/* symbolic tags */
	char	*defbranch;	/* defbranch, if set */
	char	**text;		/* descriptive text */
	char	*file;		/* file name */
	mode_t	mode;		/* mode of the gfile */
} RCS;

RCS	*rcs_init(char *file);
rdelta	*rcs_defbranch(RCS *rcs);
void	rcs_free(RCS *r);
