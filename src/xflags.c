/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
WHATSTR("@(#)%K%");

private int xflagsDefault(sccs *s, int cset, int what);
private int xflags(sccs *s, delta *d, int what);
private void pflags(u32 flags);
private int a2xflag(char *flag);

/*
 * xflags - walk the graph looking for xflag updates and make sure
 * they actually happened.
 * We record flag updates with a "Turn on|off <flag> flag" comment
 * and it will be a null delta.
 */
int
xflags_main(int ac, char **av)
{
	sccs	*s = 0;
	int	c, what = 0, ret = 0;
	char	*name;
	project	*proj = 0;

	debug_main(av);
	if (ac > 1 && streq("--help", av[1])) {
usage:		fprintf(stderr, "usage: %s [-ns] [files...]\n", av[0]);
		return (1);
	}
	while ((c = getopt(ac, av, "ns")) != -1) {
		switch (c) {
		    case 'n': what = XF_DRYRUN; break;		/* doc 2.0 */
		    case 's': what = XF_STATUS; break;		/* doc 2.0 */
		    default: goto usage;
		}
	}
	for (name = sfileFirst("xflags", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, INIT_NOCKSUM|INIT_SAVEPROJ, proj);
		unless (s) continue;
		unless (proj) proj = s->proj;
		unless (s->tree) goto next;
		unless (s->bitkeeper) {
			fprintf(stderr, "Not a BitKeeper file: %s\n", s->sfile);
			goto next;
		}
		s->state |= S_READ_ONLY;
		ret |= xflagsDefault(s, CSET(s), what);
		ret |= xflags(s, s->tree, what);
		if (!(what & XF_DRYRUN) && !(s->state & S_READ_ONLY)) {
		    	sccs_resum(s);
		}
next:		sccs_free(s);
	}
	sfileDone();
	if (proj) proj_free(proj);
	return (ret ? 1 : 0);
}

/*
 * Make sure we have something.
 * This simulates the code in delta_table().
 */
private int
xflagsDefault(sccs *s, int cset, int what)
{
	int	xf = s->tree->xflags;
	int	ret = 0;

	unless (xf) {
		unless (what & XF_DRYRUN) s->state &= ~S_READ_ONLY;
		s->tree->flags |= D_XFLAGS;
		s->tree->xflags = X_DEFAULT;
		ret = 1;
	}
	/* for old binaries */
	unless ((xf & X_REQUIRED) == X_REQUIRED) {
		unless (what & XF_DRYRUN) s->state &= ~S_READ_ONLY;
		s->tree->xflags |= X_REQUIRED;
		ret = 1;
	}
	if (cset && ((xf & (X_SCCS|X_RCS)) || !(xf & X_HASH))) {
		unless (what & XF_DRYRUN) s->state &= ~S_READ_ONLY;
		s->tree->xflags &= ~(X_SCCS|X_RCS);
		s->tree->xflags |= X_HASH;
		ret = 1;
	}
	return (ret);
}

/*
 * Recursively walk the tree,
 * look at each delta and make sure the xflags match the comments.
 */
private int
xflags(sccs *s, delta *d, int what)
{
	char	*t;
	int	ret = 0;

	unless (d) return (0);
	if (!d->added && !d->deleted && (d->type == 'D') &&
	    d->comments && (t = d->comments[1]) &&
	    (strneq(t, "Turn on ", 8) || strneq(t, "Turn off ", 9))) {
	    	ret = checkXflags(s, d, what);
	}
	ret |= xflags(s, d->kid, what);
	ret |= xflags(s, d->siblings, what);
	return (ret);
}

int
checkXflags(sccs *s, delta *d, int what)
{
	char	*t, *f;
	u32	old, new, want, added = 0, deleted = 0, *p;
	int	i;

	if (d == s->tree) {
		unless ((d->xflags & X_REQUIRED) == X_REQUIRED) {
			if (what & XF_STATUS) return (1);
			fprintf(stderr,
			    "%s: missing required flag[s]: ", s->gfile);
			want = ~(d->xflags & X_REQUIRED) & X_REQUIRED;
			pflags(want);
			fprintf(stderr, "\n");
			return (1);
		}
		return (0);
	}
	EACH(d->comments) {
		if (strneq(d->comments[i], "Turn on ", 8)) {
			t = &(d->comments[i][8]);
			p = &added;
		} else {
			t = &(d->comments[i][9]);
			p = &deleted;
		}
		f = t;
		unless (t = strchr(f, ' ')) return (0);
		unless (streq(t, " flag")) return (0);
		*t = 0; *p |= a2xflag(f); *t = ' ';
	}
	assert(d->parent);
	old = sccs_xflags(d->parent);
	new = sccs_xflags(d);
	want = old | added;
	want &= ~deleted;
	if (new == want) return (0);
	if (what & XF_STATUS) return (1);
	if (what & XF_DRYRUN) {
		fprintf(stderr, "%s@@%s ", s->gfile, d->rev);
		if (new & ~want) {
			fprintf(stderr, "should not have ");
			pflags(new & ~want);
		}
		if (want & ~new) {
			if (new & ~want) fprintf(stderr, ", ");
			fprintf(stderr, "should have ");
			pflags(want & ~new);
		}
		fprintf(stderr, " flag\n");
		return (1);
	}
	s->state &= ~S_READ_ONLY;
	d->flags |= D_XFLAGS;
	d->xflags = want;
	return (1);
}

private void
pflags(u32 flags)
{
	int	comma = 0;

#define	fs(s)	fputs(s, stderr)
	if (flags & X_BITKEEPER) {
		if (comma) fs(","); fs("BITKEEPER"); comma = 1;
	}
	if (flags & X_RCS) {
		if (comma) fs(","); fs("RCS"); comma = 1;
	}
	if (flags & X_YEAR4) {
		if (comma) fs(","); fs("YEAR4"); comma = 1;
	}
#ifdef X_SHELL
	if (flags & X_SHELL) {
		if (comma) fs(","); fs("SHELL"); comma = 1;
	}
#endif
	if (flags & X_EXPAND1) {
		if (comma) fs(","); fs("EXPAND1"); comma = 1;
	}
	if (flags & X_CSETMARKED) {
		if (comma) fs(","); fs("CSETMARKED"); comma = 1;
	}
	if (flags & X_HASH) {
		if (comma) fs(","); fs("HASH"); comma = 1;
	}
	if (flags & X_SCCS) {
		if (comma) fs(","); fs("SCCS"); comma = 1;
	}
	if (flags & X_SINGLE) {
		if (comma) fs(","); fs("SINGLE"); comma = 1;
	}
	if (flags & X_LOGS_ONLY) {
		if (comma) fs(","); fs("LOGS_ONLY"); comma = 1;
	}
	if (flags & X_EOLN_NATIVE) {
		if (comma) fs(","); fs("EOLN_NATIVE"); comma = 1;
	}
	if (flags & X_LONGKEY) {
		if (comma) fs(","); fs("LONGKEY"); comma = 1;
	}
	unless (comma) fs("<NONE>");
}


private int
a2xflag(char *flag)
{
	if (streq(flag, "BITKEEPER")) return (X_BITKEEPER);
	if (streq(flag, "RCS")) return (X_RCS);
	if (streq(flag, "YEAR4")) return (X_YEAR4);
#ifdef X_SHELL
	if (streq(flag, "SHELL")) return (X_SHELL);
#endif
	if (streq(flag, "EXPAND1")) return (X_EXPAND1);
	if (streq(flag, "CSETMARKED")) return (X_CSETMARKED);
	if (streq(flag, "HASH")) return (X_HASH);
	if (streq(flag, "SCCS")) return (X_SCCS);
	if (streq(flag, "SINGLE")) return (X_SINGLE);
	if (streq(flag, "LOGS_ONLY")) return (X_LOGS_ONLY);
	if (streq(flag, "EOLN_NATIVE")) return (X_EOLN_NATIVE);
	if (streq(flag, "LONGKEY")) return (X_LONGKEY);
	fprintf(stderr, "Unknown flag: %s\n", flag);
	return (0);
}
