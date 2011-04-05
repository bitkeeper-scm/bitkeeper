#include "system.h"
#include "sccs.h"
#include "range.h"

private void	prevs(delta *d);
private void	_prevs(delta *d);
private void	puser(char *u);
private void	pd(char *prefix, delta *d);
private void	renumber(sccs *s);
private	delta	*ancestor(sccs *s, delta *d);
private int	flags;
private sccs	*s;
private int	sort;	/* append -timet */
private int	tags;	/* append '*' to tagged revisions */
private	char	*rev;
private	delta	*tree;	/* oldest node we're displaying */

int
lines_main(int ac, char **av)
{
	int	n = 0, c, rc = 1;
	char	*name;
	delta	*e;
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
			prevs(e ? e : s->tree);
		} else if (rargs.rstart) {
			if (range_process("lines", s,
				RANGE_ENDPOINTS, &rargs)) {
				rc = 1;
				goto next;
			}
			unless (s->rstart) goto next;
			e = ancestor(s, s->rstart);
			e->merge = 0;
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
			prevs(s->tree);
		}
		rc = 0;
next:		sccs_free(s);
	}
	sfileDone();
	return (rc);
}

/*
 * Reuse the d->same field to put in a serial number which
 * - starts at 0, not 1
 * - increments only for real deltas, not meta
 */
private void
renumber(sccs *s)
{
	delta	*d;
	int	i;
	int	ser = 0;

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) continue;
		unless (TAG(d)) d->same = ser++;
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
prevs(delta *d)
{
	unless (KID(s, d)) d = PARENT(s, d);
	tree = d;
	pd("", d);
	_prevs(KID(s, d));
	_prevs(SIBLINGS(s, d));
}

private void
_prevs(delta *d)
{
	unless (d && !TAG(d)) return;

	/*
	 * If we are a branch start, then print our parent.
	 */
	if ((d->r[3] == 1) ||
	    ((d->r[0] > 1) && (d->r[1] == 1) && !d->r[2])) {
	    	pd("", PARENT(s, d));
	}

	pd(" ", d);
	if (KID(s, d) && !TAG(KID(s, d))) {
		_prevs(KID(s, d));
	} else {
		printf("\n");
	}
	for (d = SIBLINGS(s, d); d; d = SIBLINGS(s, d)) {
		unless (d->flags & D_RED) _prevs(d);
	}
}

private void
pd(char *prefix, delta *d)
{
	printf("%s%s", prefix, REV(s, d));
	if (flags & GET_USER) {
		putchar('-');
		puser(USER(s, d));
	}
	if (sort) printf("-%u", d->same);
	if (tags && (d->flags & D_SYMBOLS)) putchar('*');
	if (d->flags & D_BADREV) printf("-BAD");
	if (d->merge) {
		delta	*p = MERGE(s, d);

		assert(p);
		if (p->date > tree->date) {
			printf("%c%s", BK_FS, REV(s, p));
			if (flags & GET_USER) {
				putchar('-');
				puser(USER(s, p));
			}
			if (sort) printf("-%u", p->same);
			if (tags && (p->flags & D_SYMBOLS)) putchar('*');
		}
	}
	d->flags |= D_RED;
}

/*
 * For each delta, if it is based on a node earlier than our ancestor,
 * adjust backwards so we get a complete graph.
 */
private delta	*
t(delta *a, delta *d)
{
	delta	*p;

	for (p = d; p->r[2]; p = PARENT(s, p));
	if (!TAG(p) && (p->date < a->date)) a = p;
	if (KID(s, d)) a = t(a, KID(s, d));
	if (SIBLINGS(s, d)) a = t(a, SIBLINGS(s, d));
	return (a);
}

/*
 * Find a common trunk based ancestor for everything from d onward.
 */
private delta *
ancestor(sccs *s, delta *d)
{
	delta	*a;

	/* get back to the trunk */
	for (a = d; a && a->r[2]; a = PARENT(s, a));
	while (TAG(a)) a = PARENT(s, a);
	a = t(a, a);
	return (a);
}
