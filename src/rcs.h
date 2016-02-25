/*
 * Copyright 2000-2002,2006,2016 BitMover, Inc
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

/*
 * Parse just enough of the RCS file to convert them.
 * Did I mention that I really hate RCS?
 */

/*
 * Struct delta - describes a single delta entry.
 */
typedef struct rdelta rdelta;
struct rdelta {
	char	*rev;			/* revision number */
	char	*sccsrev;		/* new revision number */
	char	*sdate;			/* ascii date, 93.07.25.21.14.11 */
	char	*author;		/* user name of delta owner */
	char	*comments;		/* Comment log */
	time_t	date;			/* date - conversion from sdate/zone */
	time_t	dateFudge;		/* make dates go forward */
	rdelta	*parent;		/* parent delta above me */
	rdelta	*kid;			/* next delta on this branch */
	rdelta	*next;			/* all deltas in table order */
	rdelta	*prev;			/* all deltas in reverse table order */
	char	*snext;			/* next as a revision */
	u32	ingraph:1;		/* inserted in the graph */
	u32	printed:1;		/* been here */
	u32	dead:1;			/* file deleted */
};

typedef	struct sym sym;
struct sym {
	char	*name;
	char	*rev;
	sym	*next;
};

/*
 * struct RCS
 */
typedef	struct RCS RCS;
struct RCS {
	rdelta	*tree;		/* the delta tree after mkgraph() */
	rdelta	*table;		/* the delta table list, 1.99 .. 1.0 */
	rdelta	*lastinsert;	/* pointer to the last delta inserted */
	int	n;		/* number of deltas */
	sym	*symbols;	/* symbolic tags */
	char	*defbranch;	/* defbranch, if set */
	char	*text;		/* descriptive text */
	char	*rcsfile;	/* RCS file name */
	char	*workfile;	/* working file name */
	char	*kk;		/* -kk is default; -kb for binary */
	char	*rootkey;	/* root key */
};

typedef struct RCSlist RCSlist;
struct RCSlist {
	RCSlist	*next;
	RCS	*file;
	rdelta	*lastver;  /* The last version added to BK */
};

RCS	*rcs_init(char *file, char *branch);
rdelta	*rcs_findit(const RCS *rcs, char *rev);
void	rcs_free(RCS *r);

#ifdef	RCS_DEBUG
#define	rcsdebug(x)	fprintf x
#else
#define	rcsdebug(x)
#endif
