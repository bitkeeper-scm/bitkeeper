/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"

private int xflagsDefault(sccs *s, int cset, int what);
private int xflags(sccs *s, delta *d, int what);

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

	while ((c = getopt(ac, av, "ns", 0)) != -1) {
		switch (c) {
		    case 'n': what = XF_DRYRUN; break;		/* doc 2.0 */
		    case 's': what = XF_STATUS; break;		/* doc 2.0 */
		    default: bk_badArg(c, av);
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
	ret |= xflags(s, KID(d), what);
	ret |= xflags(s, SIBLINGS(d), what);
	return (ret);
}

int
checkXflags(sccs *s, delta *d, int what)
{
	char	*t, *f;
	u32	old, new, want, added = 0, deleted = 0, *p;
	int	i;
	char	key[MD5LEN];

	if (d == s->tree) {
		unless ((d->xflags & X_REQUIRED) == X_REQUIRED) {
			if (what & XF_STATUS) return (1);
			fprintf(stderr,
			    "%s: missing required flag[s]: ", s->gfile);
			want = ~(d->xflags & X_REQUIRED) & X_REQUIRED;
			fputs(xflags2a(want), stderr);
			fprintf(stderr, "\n");
			return (1);
		}
		return (0);
	}
	EACH_COMMENT(s, d) {
		if (strneq(d->cmnts[i], "Turn on ", 8)) {
			t = &(d->cmnts[i][8]);
			p = &added;
		} else if (strneq(d->cmnts[i], "Turn off ", 9)) {
			t = &(d->cmnts[i][9]);
			p = &deleted;
		} else {
			continue;
		}
		f = t;
		unless (t = strchr(f, ' ')) continue;
		unless (streq(t, " flag")) continue;
		*t = 0; *p |= a2xflag(f); *t = ' ';
	}
	assert(d->pserial);
	old = sccs_xflags(s, PARENT(s, d));
	new = sccs_xflags(s, d);
	want = old | added;
	want &= ~deleted;
	if (new == want) return (0);
	/*
	 * We screwed lines.c up, the fix is to add some comments to the
	 * 1.1 delta like so: 
	 * Turn off EOLN_NATIVE flag
	 * Turn on EXPAND1 flag
	 * Turn on SCCS flag
	 * but that's too much of a pain.
	 * XXX: this is fixed by rmshortkeys -- so safe to pull when
	 * all repos have been converted.
	 */
	if (streq(s->tree->sdate, "97/05/18 16:29:28")) {
		sccs_md5delta(s, s->tree, key);
		if (streq("337f90d8qZwQGPzUrQ-3E6KGSH4k4g", key)) return (0);
	}

	/* just in case this blows up in the field */
	if (getenv("_BK_NO_XFLAGS_CHECK")) return (0);

	// fprintf(stderr, "\ndelta = %s, %s\n", d->rev, d->cmnts[1]);
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
			fputs(xflags2a(new & ~want), stderr);
		}
		if (want & ~new) {
			if (new & ~want) fprintf(stderr, ", ");
			fprintf(stderr, "should have ");
			fputs(xflags2a(want & ~new), stderr);
		}
		fprintf(stderr, " flag\n");
		return (1);
	}
	s->state &= ~S_READ_ONLY;
	d->flags |= D_XFLAGS;
	d->xflags = want;
	return (1);
}

char *
xflags2a(u32 flags)
{
	static	char	*ret;
	char	**list = 0;

	if (ret) free(ret);

	if (flags & X_BITKEEPER) {
		list = addLine(list, "BITKEEPER");
	}
	if (flags & X_RCS) {
		list = addLine(list, "RCS");
	}
	if (flags & X_YEAR4) {
		list = addLine(list, "YEAR4");
	}
#ifdef X_SHELL
	if (flags & X_SHELL) {
		list = addLine(list, "SHELL");
	}
#endif
	if (flags & X_EXPAND1) {
		list = addLine(list, "EXPAND1");
	}
	if (flags & X_CSETMARKED) {
		list = addLine(list, "CSETMARKED");
	}
	if (flags & X_HASH) {
		list = addLine(list, "HASH");
	}
	if (flags & X_SCCS) {
		list = addLine(list, "SCCS");
	}
	if (flags & X_EOLN_NATIVE) {
		list = addLine(list, "EOLN_NATIVE");
	}
	if (flags & X_EOLN_WINDOWS) {
		list = addLine(list, "EOLN_WINDOWS");
	}
	if (flags & X_LONGKEY) {
		list = addLine(list, "LONGKEY");
	}
	if (flags & X_KV) {
		list = addLine(list, "KV");
	}
	if (flags & X_NOMERGE) {
		list = addLine(list, "NOMERGE");
	}
	if (flags & X_MONOTONIC) {
		list = addLine(list, "MONOTONIC");
	}
	ret = joinLines(",", list);
	freeLines(list, 0);
	return (ret);
}


u32
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
	if (streq(flag, "EOLN_UNIX")) return (X_EOLN_UNIX);
	if (streq(flag, "EOLN_WINDOWS")) return (X_EOLN_WINDOWS);
	if (streq(flag, "LONGKEY")) return (X_LONGKEY);
	if (streq(flag, "NOMERGE")) return (X_NOMERGE);
	if (streq(flag, "MONOTONIC")) return (X_MONOTONIC);
	fprintf(stderr, "Unknown flag: %s\n", flag);
	return (0);
}
