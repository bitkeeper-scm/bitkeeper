/*
 * Copyright 1999-2003,2006,2009-2012,2014-2016 BitMover, Inc
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

#ifndef	_RANGE_H
#define	_RANGE_H

typedef struct range RANGE;
struct	range {
	char	*rstart, *rstop; /* endpoints from command line */
	u32	isdate:1;	 /* contains date range */
	u32	isrev:1;	 /* contains rev range */
};

/* Lower nibble of both used for generic flags */
#define RANGE_ENDPOINTS	0x010	/* just return s->rstart s->rstop */
#define	RANGE_SET	0x020	/* return S_SET */
#define	RANGE_RSTART2	0x040	/* allow s->rstart2 */
#define RANGE_LATTICE	0x080	/* mark lattice range with D_SET */
#define RANGE_LONGEST	0x100	/* mark longest line with D_SET */

#define	WR_EITHER	0x10	/* select if BLUE xor RED */
#define	WR_BOTH 	0x20	/* select if RED and BLUE */
#define	WR_TIP		0x40	/* Callback only on tips */
#define	WR_GCA		0x80	/* Sugar for WR_BOTH | WR_TIP */

/*
 *  1.1 -- 1.2 -- 1.3 -- 1.4 -- 1.5
 *	\                      /
 *	 \                    /
 *	  - 1.1.1.1----------/
 *
 * With RANGE_SET:
 *
 *	-r1.3..1.5
 *	     s->state |= S_SET
 *	     marks 1.1.1.1,1.4,1.5 with D_SET
 *	     sets s->rstart = 1.1.1.1
 *	     sets s->rstop  = 1.5
 *
 *     (rstart/rstop are just the first/last serials where D_SET might be set)
 *
 * With RANGE_ENDPOINTS:
 *
 *	-r1.3..1.5
 *	     sets s->rstart = 1.3
 *	     sets s->rstop = 1.5
 */

int	range_process(char *me, sccs *s, u32 flags, RANGE *rargs);
int	range_addArg(RANGE *rargs, char *arg, int isdate);
int	range_urlArg(RANGE *rargs, char *url, int standalone);

void	range_cset(sccs *s, ser_t d);
ser_t	range_findMerge(sccs *s, ser_t d1, ser_t d2, ser_t **mlist);
time_t	range_cutoff(char *spec);
void	range_markMeta(sccs *s);
int	range_gone(sccs *s, ser_t *dlist, u32 dflags);
void	range_unrange(sccs *s, ser_t *left, ser_t *right, int all);
int	range_lattice(sccs *s, ser_t lower, ser_t upper, int longest);

int	range_walkrevs(sccs *s, ser_t *fromlist, ser_t *tolist, u32 flags,
	    int (*fcn)(sccs *s, ser_t d, void *token), void *token);
int	walkrevs_setFlags(sccs *s, ser_t d, void *token);
int	walkrevs_clrFlags(sccs *s, ser_t d, void *token);

typedef	struct {
	u32	flags;
	sccs	*s;
	ser_t	d;		/* the last 'd' returned from walkrevs() */
	ser_t	last;		/* the oldest serial colored (for cleanup)  */
	int	marked;		/* number of BLUE or RED nodes */
	int	all;		/* set if all deltas in RED */
	u32	mask, want, color; /* state for coloring engine */
} wrdata;

void	walkrevs_setup(wrdata *wr, sccs *s, ser_t *blue, ser_t *red, u32 flags);
ser_t	walkrevs(wrdata *wr);
ser_t	walktagrevs(wrdata *wr);
void	walkrevs_prune(wrdata *wr, ser_t d);
void	walkrevs_done(wrdata *wr);
ser_t	*walkrevs_collect(sccs *s, ser_t *blue, ser_t *red, u32 flags);

#endif
