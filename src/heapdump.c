#include "sccs.h"

private	void	dumpStats(sccs *s);

int
heapdump_main(int ac, char **av)
{
	int	c;
	int	stats = 0;
	char	*name;
	sccs	*s;
	ser_t	d;
	char	*t;
	symbol	*sym;

	while ((c = getopt(ac, av, "s", 0)) != -1) {
		switch (c) {
		    case 's': stats = 1; break;
		    default: bk_badArg(c, av);
		}
	}

	for (name = sfileFirst(av[0], &av[optind], 0);
	     name; name = sfileNext()) {

		s = sccs_init(name, INIT_MUSTEXIST);
		unless (s) continue;

		if (stats) {
			dumpStats(s);
			printf("\n");
			sccs_free(s);
			continue;
		}

		// perfile data
		printf("encoding: %x (", s->encoding_in);
		kw2val(stdout, "ENCODING", strlen("ENCODING"), s, s->tip);
		printf(")\n\n");
		for (d = TABLE(s); d >= TREE(s); d--) {
			delta_print(s, d);
			printf("\n");
		}

		if (s->symlink) printf("symlist:\n");
		EACHP(s->symlist, sym) {
			if (sym->symname) {
				printf("symname: %s\n", SYMNAME(s, sym));
			}
			if (sym->ser) {
				printf("ser: %d (%s)\n",
				    sym->ser, REV(s, sym->ser));
			}
			if (sym->meta_ser) {
				printf("meta_rev: %d (%s)\n",
				    sym->meta_ser,
				    REV(s, sym->meta_ser));
			}
			printf("\n");
		}

		printf("weave:\n");
		sccs_rdweaveInit(s);
		while (t = sccs_nextdata(s)) {
			if (*t == '\001') {
				printf("^A");
				++t;
			}
			printf("%s\n", t);
		}
		sccs_rdweaveDone(s);

		sccs_free(s);
	}
	return (0);
}

const	char	*delta_flagNames[] = {
	"INARRAY",		/* 0 */
	"NONEWLINE",		/* 1 */
	0,			/* 2 */
	0,			/* 3 */
	"META",			/* 4 */
	"SYMBOLS",		/* 5 */
	0,			/* 6 */
	"DANGLING",		/* 7 */
	"TAG",			/* 8 */
	"SYMGRAPH",		/* 9 */
	"SYMLEAF",		/* 10 */
	0,			/* 11 */
	"CSET",			/* 12 */
	0,			/* 13 */
	0,			/* 14 */
	0,			/* 15 */
	0,			/* 16 */
	0,			/* 17 */
	0,			/* 18 */
	0,			/* 19 */
	0,			/* 20 */
	0,			/* 21 */
	0,			/* 22 */
	"REMOTE",		/* 22 */
	"LOCAL",		/* 23 */
	"ERROR",		/* 24 */
	"BADFORM",		/* 25 */
	"BADREV",		/* 26 */
	"RED",			/* 27 */
	"GONE",			/* 28 */
	"BLUE",			/* 29 */
	"ICKSUM",		/* 30 */
	"SET"			/* 31 */
};

/*
 * A print routine that can be used to examine the fields in a delta*
 * This is useful in gdb with 'call delta_print(s, d)
 */
void
delta_print(sccs *s, ser_t d)
{
	int	i, c;
	u32	mask;

	// XXX: Not supposed to be able to be unused 
	unless (FLAGS(s, d)) {
		printf("serial %d unused\n", d);
		return;
	}

	printf("serial: %d (%s)\n", d, REV(s, d));

	/* serial print */
#define SPRINT(f) if (f(s, d)) \
		printf(#f ": %d (%s)\n", \
		f(s, d), REV(s, f(s, d)))
	SPRINT(PARENT);
	SPRINT(MERGE);
	SPRINT(PTAG);
	SPRINT(MTAG);
#undef	SPRINT

	printf("a/d/s: %d/%d/%d\n", ADDED(s, d), DELETED(s, d), SAME(s, d));
	printf("sum: %u\n", SUM(s, d));

	printf("date: %u", (u32)DATE(s, d));
	if (DATE_FUDGE(s, d)) printf("-%d", (u32)DATE_FUDGE(s, d));
	printf(" (%s)\n", delta_sdate(s, d));

	if (MODE(s, d)) printf("mode: %o\n", MODE(s, d));
	if (XFLAGS(s, d)) {
		printf("xflags: %x (%s)\n", XFLAGS(s, d), xflags2a(XFLAGS(s, d)));
	}
	printf("flags: %x (", FLAGS(s, d));
	c = FLAGS(s, d) & ~D_INARRAY; /* skip INARRAY */
	for (i = 0; c; i++) {
		mask = 1 << i;
		unless (c & mask) continue;
		if (delta_flagNames[i]) {
			printf("%s", delta_flagNames[i]);
		} else {
			printf("0x%x", mask);
		}
		c &= ~mask;
		if (c) printf(",");
	}
	printf(")\n");

	/* heap print */
#define HPRINT(f) \
    if (HAS_##f(s, d)) printf(#f ": %s\n", f(s, d))
	HPRINT(CLUDES);
	HPRINT(BAMHASH);
	HPRINT(RANDOM);
	HPRINT(USERHOST);
	HPRINT(PATHNAME);
	HPRINT(SORTPATH);
	HPRINT(ZONE);
	HPRINT(SYMLINK);
	HPRINT(CSETFILE);
#undef	HPRINT
	if (HAS_WEAVE(s, d)) printf("weave: yes\n");
	if (HAS_COMMENTS(s, d)) printf("comments: %s", COMMENTS(s, d));
}

private void
dumpStats(sccs *s)
{
	int	size;
	int	i, len, cnt;
	u32	off, off2;
	char	*t;
	ser_t	d;
	char	*names[] = {
		"cludes", "comments", "bamhash", "random",
		"userhost", "pathname", "sortPath", "zone",
		"symlink", "csetFile"
	};
	int	htotal[10] = {0};
	int	hcnt[10] = {0};
	const	char *wfmt = "%10s: %7s %4.1f%% %6d %5.1f\n";
	hash	*seen = hash_new(HASH_MEMHASH);

	printf("file: %s\n", s->sfile);
	printf("filesize: %7s\n", psize(s->size));
	printf("    heap: %7s\n", psize(s->heap.len));

	if (BWEAVE(s)) names[2] = "weave";

	for (d = TREE(s); d <= TABLE(s); d++) {
		for (i = 0; i < 10; i++) {
			unless (off = *(&CLUDES_INDEX(s, d) + i)) continue;
			if ((i == 2) && BWEAVE(s)) {
				/* encoded cset weave */
				while (RKOFF(s, off)) {
					off2 = NEXTKEY(s, off);
					htotal[i] += (off2 - off);
					off = off2;
				}
				htotal[i] += 4;
				++hcnt[i];
				continue;
			}
			if (hash_insert(seen, &off, sizeof(off), 0, 0)) {
				htotal[i] += strlen(s->heap.buf + off) + 1;
				++hcnt[i];
			}
		}
	}
	size = s->heap.len;
	for (i = 0; i < 10; i++) {
		unless (htotal[i]) continue;
		printf(wfmt,
		    names[i], psize(htotal[i]),
		    (100.0 * htotal[i]) / s->heap.len,
		    hcnt[i], (double)htotal[i]/hcnt[i]);
		size -= htotal[i];
	}

	cnt = len = 0;
	EACH(s->symlist) {
		unless (off = s->symlist[i].symname) continue;
		if (hash_insert(seen, &off, sizeof(off), 0, 0)) {
			++cnt;
			len += strlen(s->heap.buf + off) + 1;
		}
	}
	printf(wfmt,
	    "symnames", psize(len),  (100.0 * len) / s->heap.len,
	    cnt, (double)len/cnt);
	size -= len;

	if (BWEAVE(s)) {
		cnt = len = 0;
		for (off = s->rkeyHead; off; off = RKNEXT(s, off)) {
			++cnt;
			off2 = NEXTKEY(s, off);
			len += (off2 - off);
		}
		printf(wfmt,
		    "rootkeys", psize(len),  (100.0 * len) / s->heap.len,
		    cnt, (double)len/cnt);
		size -= len;
	}
	if (size) {
		printf("%10s: %7s %4.1f%%\n",
		    "unused", psize(size),
		    (100.0 * size) / s->heap.len);
	}
	printf("  table1: %7s\n", psize(sizeof(d1_t) * (TABLE(s) + 1)));
	printf("  table2: %7s\n", psize(sizeof(d2_t) * (TABLE(s) + 1)));
	if (nLines(s->symlist)) {
		printf(" symlist: %7s\n",
		    psize(nLines(s->symlist) * sizeof(symbol)));
	}
	size = 0;
	unless (BWEAVE(s)) {
		sccs_rdweaveInit(s);
		while (t = sccs_nextdata(s)) {
			size += strlen(t)+1;
		}
		sccs_rdweaveDone(s);
		printf("   weave: %7s\n", psize(size));
	}
}
