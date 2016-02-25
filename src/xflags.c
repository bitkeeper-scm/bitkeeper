/*
 * Copyright 2001-2003,2005-2013,2016 BitMover, Inc
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

private int xflagsDefault(sccs *s, int cset, int what);
private int xflags(sccs *s, int what);

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
		ret |= xflags(s, what);
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
	int	xf = XFLAGS(s, TREE(s));
	int	ret = 0;

	unless (xf) {
		unless (what & XF_DRYRUN) s->state &= ~S_READ_ONLY;
		XFLAGS(s, TREE(s)) = X_DEFAULT;
		ret = 1;
	}
	/* for old binaries */
	unless ((xf & X_REQUIRED) == X_REQUIRED) {
		unless (what & XF_DRYRUN) s->state &= ~S_READ_ONLY;
		XFLAGS(s, TREE(s)) |= X_REQUIRED;
		ret = 1;
	}
	if (cset && ((xf & (X_SCCS|X_RCS)) || !(xf & X_HASH))) {
		unless (what & XF_DRYRUN) s->state &= ~S_READ_ONLY;
		XFLAGS(s, TREE(s)) &= ~(X_SCCS|X_RCS);
		XFLAGS(s, TREE(s)) |= X_HASH;
		ret = 1;
	}
	return (ret);
}

/*
 * Recursively walk the tree,
 * look at each delta and make sure the xflags match the comments.
 */
private int
xflags(sccs *s, int what)
{
	int	ret = 0;
	ser_t	d;

	for (d = TREE(s); d <= TABLE(s); d++) {
		ret |= checkXflags(s, d, what);
	}
	return (ret);
}

int
checkXflags(sccs *s, ser_t d, int what)
{
	char	*t, *f;
	u32	old, new, want, added = 0, deleted = 0, *p;
	char	*x, *r;
	char	key[MD5LEN];

	if (d == TREE(s)) {
		unless ((XFLAGS(s, d) & X_REQUIRED) == X_REQUIRED) {
			if (what & XF_STATUS) return (1);
			fprintf(stderr,
			    "%s: missing required flag[s]: ", s->gfile);
			want = ~(XFLAGS(s, d) & X_REQUIRED) & X_REQUIRED;
			fputs(xflags2a(want), stderr);
			fprintf(stderr, "\n");
			return (1);
		}
		return (0);
	}
	x = COMMENTS(s, d);
	while (r = eachline(&x, 0)) {
		if (strneq(r, "Turn on ", 8)) {
			t = r+8;
			p = &added;
		} else if (strneq(r, "Turn off ", 9)) {
			t = r+9;
			p = &deleted;
		} else {
			continue;
		}
		f = t;
		unless (t = strchr(f, ' ')) continue;
		unless (strneq(t, " flag", 5)) continue;
		*t = 0; *p |= a2xflag(f); *t = ' ';
	}
	assert(PARENT(s, d));
	old = XFLAGS(s, PARENT(s, d));
	new = XFLAGS(s, d);
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
	if (DATE(s, TREE(s)) == 864023369) {
		sccs_md5delta(s, TREE(s), key);
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
		fprintf(stderr, "%s|%s ", s->gfile, REV(s, d));
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
	XFLAGS(s, d) = want;
	return (1);
}

char *
xflags2a(u32 flags)
{
	static	char	*ret;

	if (ret) free(ret);
	// we ignore X_LONGKEY
	ret = formatBits(flags & ~X_LONGKEY,
	    X_BITKEEPER, "BITKEEPER",
	    X_RCS, "RCS",
	    X_YEAR4, "YEAR4",
#ifdef X_SHELL
	    X_SHELL, "SHELL",
#endif
	    X_EXPAND1, "EXPAND1",
	    X_CSETMARKED, "CSETMARKED",
	    X_HASH, "HASH",
	    X_SCCS, "SCCS",
	    X_EOLN_NATIVE, "EOLN_NATIVE",
	    X_EOLN_WINDOWS, "EOLN_WINDOWS",
	    X_DB, "DB",
	    X_NOMERGE, "NOMERGE",
	    X_MONOTONIC, "MONOTONIC",
	    0, 0);
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
	if (streq(flag, "NOMERGE")) return (X_NOMERGE);
	if (streq(flag, "MONOTONIC")) return (X_MONOTONIC);
	if (streq(flag, "DB")) return (X_DB);
	fprintf(stderr, "Unknown flag: %s\n", flag);
	return (0);
}
