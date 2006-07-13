/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"

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

	while ((c = getopt(ac, av, "ns")) != -1) {
		switch (c) {
		    case 'n': what = XF_DRYRUN; break;		/* doc 2.0 */
		    case 's': what = XF_STATUS; break;		/* doc 2.0 */
		    default:
			fprintf(stderr, "usage: %s [-ns] [files...]\n", av[0]);
			return (1);
		}
	}
	for (name = sfileFirst("xflags", &av[optind], 0);
	    name; name = sfileNext()) {
		s = sccs_init(name, INIT_NOCKSUM);
		unless (s) continue;
		unless (HASGRAPH(s)) goto next;
		unless (s->bitkeeper) {
			fprintf(stderr, "Not a BitKeeper file: %s\n", s->sfile);
			goto next;
		}
		s->state |= S_READ_ONLY;
		ret |= xflagsDefault(s, CSET(s), what);
		ret |= xflags(s, s->tree, what);
		unless ((what & XF_DRYRUN) || (s->state & S_READ_ONLY)) {
		    	sccs_newchksum(s);
		}
next:		sccs_free(s);
	}
	if (sfileDone()) ret = 1;
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
	int	ret = 0;

	unless (d) return (0);
	ret = checkXflags(s, d, what);
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
		} else if (strneq(d->comments[i], "Turn off ", 9)) {
			t = &(d->comments[i][9]);
			p = &deleted;
		} else {
			continue;
		}
		f = t;
		unless (t = strchr(f, ' ')) continue;
		unless (streq(t, " flag")) continue;
		*t = 0; *p |= a2xflag(f); *t = ' ';
	}
	assert(d->parent);
	old = sccs_xflags(d->parent);
	new = sccs_xflags(d);
	want = old | added;
	want &= ~deleted;
	if (new == want) return (0);
	/*
	 * We screwed this one up, the fix is to add some comments to the
	 * 1.1 delta like so: 
	 * Turn off EOLN_NATIVE flag
	 * Turn on EXPAND1 flag
	 * Turn on SCCS flag
	 * but that's too much of a pain.
	 */
	if (streq("src/libc/utils/lines.c", s->gfile)) return (0);

	/* just in case this blows up in the field */
	if (getenv("_BK_NO_XFLAGS_CHECK")) return (0);

	// fprintf(stderr, "\ndelta = %s, %s\n", d->rev, d->comments[1]);
	// fprintf(stderr, "old\t"); pflags(old);
	// fprintf(stderr, "\nnew\t"); pflags(new);
	// fprintf(stderr, "\nwant\t"); pflags(want);
	// fprintf(stderr, "\nadd\t"); pflags(added);
	// fprintf(stderr, "\ndel\t"); pflags(deleted);
	// fprintf(stderr, "\n");
	if (what & XF_STATUS) return (1);
	if (what & XF_DRYRUN) {
		fprintf(stderr, "%s|%s ", s->gfile, d->rev);
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

/* XXX - this should be shared with the prs dspec code */
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
	if (flags & X_EOLN_NATIVE) {
		if (comma) fs(","); fs("EOLN_NATIVE"); comma = 1;
	}
	if (flags & X_EOLN_WINDOWS) {
		if (comma) fs(","); fs("EOLN_WINDOWS"); comma = 1;
	}
	unless (flags & (X_EOLN_NATIVE|X_EOLN_WINDOWS)) {
		if (comma) fs(","); fs("EOLN_UNIX"); comma = 1;
	}
	if (flags & X_LONGKEY) {
		if (comma) fs(","); fs("LONGKEY"); comma = 1;
	}
	if (flags & X_NOMERGE) {
		if (comma) fs(","); fs("NOMERGE"); comma = 1;
	}
	if (flags & X_MONOTONIC) {
		if (comma) fs(","); fs("MONOTONIC"); comma = 1;
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
	if (streq(flag, "EOLN_NATIVE")) return (X_EOLN_NATIVE);
	if (streq(flag, "EOLN_WINDOWS")) return (X_EOLN_WINDOWS);
	if (streq(flag, "LONGKEY")) return (X_LONGKEY);
	if (streq(flag, "NOMERGE")) return (X_NOMERGE);
	if (streq(flag, "MONOTONIC")) return (X_MONOTONIC);
	fprintf(stderr, "Unknown flag: %s\n", flag);
	return (0);
}
