/*
 * Copyright 1999-2003,2006,2009-2011,2016 BitMover, Inc
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
#include "range.h"

private void	prevs(ser_t d);
private void	_prevs(ser_t d);
private void	puser(char *u);
private void	pd(char *prefix, ser_t d);
private void	renumber(sccs *s);
private	ser_t	ancestor(sccs *s, ser_t d);
private int	flags;
private sccs	*s;
private int	sort;	/* append -timet */
private int	tags;	/* append '*' to tagged revisions */
private	char	*rev;
private	ser_t	tree;	/* oldest node we're displaying */

int
lines_main(int ac, char **av)
{
	int	n = 0, c, rc = 1;
	char	*name;
	ser_t	e;
	RANGE	rargs = {0};

	while ((c = getopt(ac, av, "c;n;ur;R;tT", 0)) != -1) {
		switch (c) {
		    case 'R':	/* old form, allows no .. */
		    	unless (strstr(optarg, "..")) {
				optarg = aprintf("%s..", optarg);
			}
		    case 'c':
		    	if (range_addArg(&rargs, optarg, 1)) usage();
			break;
		    case 'u':
			flags |= GET_USER;
			break;
		    case 't': sort = 1; 
			break;
		    case 'T': tags = 1;
			break;
		    case 'n':
			n = atoi(optarg); 
			break;
		    case 'r':
			rev = optarg; 
			break;
		    default: bk_badArg(c, av);
		}
	}

	unless (av[optind]) usage();
	name = sfileFirst("lines", &av[optind], 0);
	if (sfileNext() || !name) usage();

	if (name && (s = sccs_init(name, INIT_NOCKSUM)) && HASGRAPH(s)) {
		renumber(s);
		sccs_mkKidList(s);
		if (n) {
			for (c = n, e = sccs_top(s); e && c--; e = PARENT(s, e));
			if (e) e = ancestor(s, e);
			prevs(e ? e : TREE(s));
		} else if (rargs.rstart) {
			if (range_process("lines", s,
				RANGE_ENDPOINTS, &rargs)) {
				rc = 1;
				goto next;
			}
			unless (s->rstart) goto next;
			e = ancestor(s, s->rstart);
			MERGE_SET(s, e, 0);
			prevs(e);
		} else if (rev) {
			e = sccs_findrev(s, rev);
			unless (e) {
				fprintf(stderr, "bad rev %s\n", rev);
				rc = 1;
				goto next;
			}
			printf("%s", REV(s, e));
			if (flags & GET_USER) {
				putchar('-');
				puser(USER(s, e));
			}
			printf("\n");
		} else {
			prevs(TREE(s));
		}
		rc = 0;
next:		sccs_free(s);
	}
	sfileDone();
	return (rc);
}

/*
 * Reuse the SAME(s, d) field to put in a serial number which
 * - starts at 0, not 1
 * - increments only for real deltas, not meta
 */
private void
renumber(sccs *s)
{
	ser_t	d;
	int	ser = 0;

	for (d = TREE(s); d <= TABLE(s); d++) {
		unless (TAG(s, d)) SAME_SET(s, d, ser++);
	}
}

private void
puser(char *u)
{
	/* 1.0 nodes don't always have a user */
    	unless (u) {
		printf("NONE");
		return;
	}
	do {
		if (*u == '/') break;
		unless (*u == '@') putchar(*u);
	} while (*++u);
}

private void
prevs(ser_t d)
{
	unless (KID(s, d)) d = PARENT(s, d);
	tree = d;
	pd("", d);
	_prevs(KID(s, d));
	_prevs(SIBLINGS(s, d));
}

private void
_prevs(ser_t d)
{
	unless (d && !TAG(s, d)) return;

	/*
	 * If we are a branch start, then print our parent.
	 */
	if ((R3(s, d) == 1) ||
	    ((R0(s, d) > 1) && (R1(s, d) == 1) && !R2(s, d))) {
	    	pd("", PARENT(s, d));
	}

	pd(" ", d);
	if (KID(s, d) && !TAG(s, KID(s, d))) {
		_prevs(KID(s, d));
	} else {
		printf("\n");
	}
	for (d = SIBLINGS(s, d); d; d = SIBLINGS(s, d)) {
		unless (FLAGS(s, d) & D_RED) _prevs(d);
	}
}

private void
pd(char *prefix, ser_t d)
{
	printf("%s%s", prefix, REV(s, d));
	if (flags & GET_USER) {
		putchar('-');
		puser(USER(s, d));
	}
	if (sort) printf("-%u", SAME(s, d));
	if (tags && (FLAGS(s, d) & D_SYMBOLS)) putchar('*');
	if (FLAGS(s, d) & D_BADREV) printf("-BAD");
	if (MERGE(s, d)) {
		ser_t	p = MERGE(s, d);

		assert(p);
		if (DATE(s, p) > DATE(s, tree)) {
			printf("%c%s", BK_FS, REV(s, p));
			if (flags & GET_USER) {
				putchar('-');
				puser(USER(s, p));
			}
			if (sort) printf("-%u", SAME(s, p));
			if (tags && (FLAGS(s, p) & D_SYMBOLS)) putchar('*');
		}
	}
	FLAGS(s, d) |= D_RED;
}

/*
 * For each delta, if it is based on a node earlier than our ancestor,
 * adjust backwards so we get a complete graph.
 */
private ser_t
t(ser_t a, ser_t d)
{
	ser_t	p;

	for (p = d; R2(s, p); p = PARENT(s, p));
	if (!TAG(s, p) && (DATE(s, p) < DATE(s, a))) a = p;
	if (KID(s, d)) a = t(a, KID(s, d));
	if (SIBLINGS(s, d)) a = t(a, SIBLINGS(s, d));
	return (a);
}

/*
 * Find a common trunk based ancestor for everything from d onward.
 */
private ser_t
ancestor(sccs *s, ser_t d)
{
	ser_t	a;

	/* get back to the trunk */
	for (a = d; a && R2(s, a); a = PARENT(s, a));
	while (TAG(s, a)) a = PARENT(s, a);
	a = t(a, a);
	return (a);
}
