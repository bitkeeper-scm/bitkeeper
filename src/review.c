/*
 * Copyright 2003,2007,2010,2016 BitMover, Inc
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

#include "system.h"
#include "sccs.h"
#include "redblack.h"

typedef struct intvl intvl;
struct intvl {
	int	start, end;
};

private RBtree	*intvl_new(void);
private	void	intvl_add(RBtree *range, int start, int end);
private	void	load_existing(char *file);
private	int	mark_annotations(void);

/*
 * data structure with the reviews that are being merged.
 * it is a hash on MD5s pointing at a hash of review tags
 * pointing at a interval tree listing the line numbers.
 */
private	hash	*reviews;

int
reviewmerge_main(int ac, char **av)
{
	FILE	*f;
	hash	*taghash;
	RBtree	*range;
	char	*p;
	int	line;
	intvl	*d;
	int	c;
	int	anno = 0;
	char	*tag;
	char	*exist = 0;
	char	buf[MAXLINE];

	reviews = hash_new(HASH_MEMHASH);

	while ((c = getopt(ac, av, "ae:", 0)) != -1) {
		switch (c) {
		    case 'a': anno = 1; break;
		    case 'e': exist = optarg; break;
		    default: bk_badArg(c, av);
		}
	}
	if (exist) load_existing(exist);

	if (anno) return (mark_annotations());

	tag = av[optind];
	unless (tag) {
		fprintf(stderr, "much supply review tag\n");
		return (1);
	}

	/*
	 * read lines to review from stdin and mark those lines with the
	 * tag 'tag'.
	 */
	while (fnext(buf, stdin)) {
		p = strchr(buf, '.');
		*p++ = 0;
		line = atoi(p);

		unless (taghash = hash_fetchStrPtr(reviews, buf)) {
			taghash = hash_new(HASH_MEMHASH);

			hash_storeStrPtr(reviews, buf, taghash);
		}
		unless (range = hash_fetchStrPtr(taghash, tag)) {
			range = intvl_new();

			hash_storeStrPtr(taghash, tag, range);
		}
		intvl_add(range, line, line);
	}
	if (exist) {
		f = fopen(exist, "w");
	} else {
		f = stdout;
	}
	EACH_HASH(reviews) {
		fprintf(f, "%s", (char *)reviews->kptr);
		taghash = *(hash **)reviews->vptr;
		EACH_HASH(taghash) {
			fprintf(f, " %s", (char *)taghash->kptr);
			range = *(RBtree **)taghash->vptr;
			d = RBtree_first(range);
			while (d) {
				fprintf(f, ",%d", d->start);
				if (d->start != d->end) {
					fprintf(f, "-%d", d->end);
				}
				d = RBtree_next(range, d);
			}
		}
		fprintf(f, "\n");
	}
	if (exist) fclose(f);
	return (0);
}


/*
 * Read an existing review file and load it into the reviews structure.
 *
 * Each line follows this format:
 *    MD5KEY [tag,line,start-end] [tag2,start-end,line3,line4]
 *
 * example:
 *  3b7c2556jFYvzh-kyXSGCUncSpSv9w top,1-11
 *  372395ddmY8v-A6n7h6FHmVblK4G-w old,2,4-5,7,10,12-13,39-41 top,2,4,10,39,41
 *  37e56ba0UUJGcO2HXube7Q6ppQnVvg top,1
 */
private	void
load_existing(char *file)
{
	hash	*taghash;
	RBtree	*range;
	FILE	*f;
	char	*a, *b, *c, *d;
	int	start, end;
	char	buf[MAXLINE];

	unless (f = fopen(file, "r")) return;
	while (fnext(buf, f)) {
		a = strchr(buf, ' ');
		*a++ = 0;

		unless (taghash = hash_fetchStrPtr(reviews, buf)) {
			taghash = hash_new(HASH_MEMHASH);
			hash_storeStrPtr(reviews, buf, taghash);
		}
		/* a points at start of first tag */
		while (a) {
			b = strchr(a, ' ');
			if (b) *b++ = 0; /* b == start of next tag */
			c = strchr(a, ',');
			*c++ = 0; /* c = start of range */

			unless (range = hash_fetchStrPtr(taghash, a)) {
				range = intvl_new();

				hash_storeStrPtr(taghash, a, range);
			}
			while (c) {
				a = strchr(c, ',');
				if (a) *a++ = 0; /* a = start of next range */
				start = atoi(c);
				if (d = strchr(c, '-')) {
					end = atoi(d+1);
				} else {
					end = start;
				}
				intvl_add(range, start, end);
				c = a;
			}
			a = b;
		}
	}
	fclose(f);
}

/*-------------------------------------------------------------------------- */

/* Code to create a iterval tree using the RBtree routines */


/*
 * A comparison routine for building the tree.   Here I say that two ranges
 * are "equal" if they are at least adjecent.  This won't happen in the tree
 * itself and it makes the RBtree_find() return a useful answer.
 */
private int
intvl_compare(void *a, void *b)
{
	intvl	*ap = (intvl *)a;
	intvl	*bp = (intvl *)b;

	if (ap->end < bp->start - 1) return (-1);
	if (ap->start > bp->end + 1) return (1);
	return (0);
}

/*
 * Create a new interval tree
 */
private RBtree *
intvl_new(void)
{
	return (RBtree_new(sizeof(intvl), intvl_compare));
}

/*
 * return true if a value is inside the defined range
 */
private int
intvl_in(RBtree *range, int val)
{
	intvl	new;
	intvl	*d;

	new.start = new.end = val;

	if (d = RBtree_find(range, &new)) {
		if (val >= d->start && val <= d->end) return (1);
	}
	return (0);
}

/*
 * Add a range to the interval tree
 */
private void
intvl_add(RBtree *range, int start, int end)
{
	intvl	new;
	intvl	*d, *prev, *next;

	new.start = start;
	new.end = end;

	unless (d = RBtree_find(range, &new)) {
		/*
		 * if find fails then I know my new range will be a
		 * totally new node in the tree.  See the comparison routine.
		 */
		RBtree_insert(range, &new);
		return;
	}

	/*
	 * Our range at least partially overlaps an existing node in
	 * the tree.  Expand the existing code to cover our range and
	 * merge in any adjecent nodes that are necessary to do this.
	 */
	if (start < d->start) {
		d->start = start;

		while (1) {
			prev = RBtree_prev(range, d);
			if (prev && d->start <= prev->end + 1) {
				/* We overlap the previous node so delete it */
				d->start = prev->start;
				RBtree_delete(range, prev);
			} else {
				break;
			}
		}
	}
	/* same as above for the top end */
	if (end > d->end) {
		d->end = end;

		while (1) {
			next = RBtree_next(range, d);
			if (next && d->end >= next->start - 1) {
				d->end = next->end;
				RBtree_delete(range, next);
			} else {
				break;
			}
		}
	}
}

/*-------------------------------------------------------------------------- */

private int
mark_annotations(void)
{
	hash	*taghash;
	RBtree	*range;
	char	*p;
	int	line;
	int	found;
	char	buf[MAXLINE];

	while (fnext(buf, stdin)) {
		p = strchr(buf, '.');
		*p++ = 0;
		line = atoi(p);

		found = 0;
		taghash = hash_fetchStrPtr(reviews, buf);
		if (taghash) EACH_HASH(taghash) {
			range = *(RBtree **)taghash->vptr;
			if (found = intvl_in(range, line)) break;
		}
		p = strchr(p, '\t');
		printf("%d%s", found, p);
	}
	return (0);
}
