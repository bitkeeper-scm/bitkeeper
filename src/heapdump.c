/*
 * Copyright 2013-2016 BitMover, Inc
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

#include "sccs.h"

private	void	dumpStats(sccs *s);
private	void	dumpEof(sccs *s);

int
heapdump_main(int ac, char **av)
{
	int	c;
	int	stats = 0, eof = 0;
	char	*name;
	sccs	*s;
	ser_t	d, lastd;
	u32	rkoff, dkoff;
	char	*t;
	symbol	*sym;

	while ((c = getopt(ac, av, "es", 0)) != -1) {
		switch (c) {
		    case 'e': eof = 1; break;
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
		if (eof) {
			dumpEof(s);
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
		if (CSET(s)) {
			lastd = 0;
			while (d = cset_rdweavePair(s, 0, &rkoff, &dkoff)) {
				if (d != lastd) {
					if (lastd) printf("^AE %d\n", lastd);
					printf("^AI %d\n", d);
					lastd = d;
				}
				printf("%s ", HEAP(s, rkoff));
				printf("%s\n", dkoff ? HEAP(s, dkoff) : "END");
			}
			if (lastd) printf("^AE %d\n", lastd);
		} else {
			while (t = sccs_nextdata(s)) {
				if (*t == '\001') {
					printf("^A");
					++t;
				}
				printf("%s\n", t);
			}
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
	u32	tsize;
	u32	i, len, cnt;
	u32	off, rkoff, dkoff;
	char	*t;
	FILE	*f;
	ser_t	d;
	char	*names[] = {
		"cludes", "comments", "bamhash", "random",
		"userhost", "pathname", "sortPath", "zone",
		"symlink", "csetFile", "deltakeys"
	};
#define	N_BAMHASH	2
#define	N_WEAVE		2
#define	N_DELTAKEYS	10
#define	N_NUMNAMES	(sizeof(names) / sizeof(names[0]))
	int	numNames = N_NUMNAMES;
	int	htotal[N_NUMNAMES] = {0};
	int	hcnt[N_NUMNAMES] = {0};
	const	char *wfmt = "%10s: %7s %4.1f%% %6d %5.1f\n";
	hash	*seen = hash_new(HASH_MEMHASH);

	unless (f = s->fh) f = s->pagefh;
	assert(f);
	fseek(f, 0, SEEK_END);
	printf("%s\n", s->sfile);
	printf("sfile: %s", psize(s->size));
	if (llabs(s->size - ftell(f)) > 100) {
		printf("->%s", psize(ftell(f)));
	} else if (s->encoding_in & E_GZIP) {
		printf(" (gzip)");
	}
	printf("\n");
	if (s->heapsz1) {
		assert(CSET(s));
		printf("heap1: %s->%s\n",
		    psize(size(sccs_Xfile(s, '1'))), psize(s->heapsz1));
		if (i = size(sccs_Xfile(s, '2'))) {
			printf("heap2: %s->%s\n",
			    psize(i), psize(s->heap_loadsz - s->heapsz1));
		}
	} else {
		printf("heap: %5s (contained in sfile)\n", psize(s->heap.len));
	}

	if (CSET(s)) names[N_BAMHASH] = "weave";  /* shared with bamhash */
	for (d = TREE(s); d <= TABLE(s); d++) {
		for (i = 0; i < numNames; i++) {
			if (i == N_DELTAKEYS) continue;
			unless (off = *(&CLUDES_INDEX(s, d) + i)) continue;
			if ((i == N_WEAVE) && CSET(s)) {
				/* encoded cset weave */
				while (off = RKDKOFF(s, off, rkoff, dkoff)) {
					if (dkoff) {
						htotal[N_DELTAKEYS] +=
						    strlen(HEAP(s, dkoff))+1;
						++hcnt[N_DELTAKEYS];
					}
					htotal[i] += 4;
					unless (BWEAVE2(s)) htotal[i] += 4;
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
	tsize = s->heap.len;
	for (i = 0; i < numNames; i++) {
		unless (htotal[i]) continue;
		printf(wfmt,
		    names[i],					// name
		    psize(htotal[i]),				// size
		    (100.0 * htotal[i]) / s->heap.len,		// percent
		    hcnt[i],					// # items
		    (double)htotal[i]/hcnt[i]);			// avg size/item
		tsize -= htotal[i];
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
	tsize -= len;

	if (CSET(s)) {
		cnt = len = 0;
		for (off = s->rkeyHead; off; off = KOFF(s, off)) {
			++cnt;
			len += strlen(HEAP(s, off+4)) + 1 + 4;
		}
		if (cnt) len += 4;
		printf(wfmt,
		    "rootkeys", psize(len),  (100.0 * len) / s->heap.len,
		    cnt, (double)len/cnt);
		tsize -= len;
	}
	if (t = hash_fetchStr(s->heapmeta, "HASHBITS")) {
		len = sizeof(u32) * (1 << atoi(t));
		printf("%10s: %7s %4.1f%%\n",
		    "uniqhash", psize(len),  (100.0 * len) / s->heap.len);
		tsize -= len;
	}
	if (tsize) {
		fflush(stdout);
		assert(tsize > 0);
		printf("%10s: %7s %4.1f%%\n",
		    "unused", psize(tsize),
		    (100.0 * tsize) / s->heap.len);
	}
	printf("  table1: %7s\n", psize(sizeof(d1_t) * (TABLE(s) + 1)));
	printf("  table2: %7s\n", psize(sizeof(d2_t) * (TABLE(s) + 1)));
	if (nLines(s->symlist)) {
		printf(" symlist: %7s\n",
		    psize(nLines(s->symlist) * sizeof(symbol)));
	}
	tsize = 0;
	unless (CSET(s)) {
		sccs_rdweaveInit(s);
		while (t = sccs_nextdata(s)) {
			tsize += strlen(t)+1;
		}
		sccs_rdweaveDone(s);
		printf("   weave: %7s\n", psize(tsize));
	}
}

/*
 * Poly can put the same rootkey deltakey into multiple csets,
 * so we need to include cset key to disambiguate.
 *
 * But when wanting to isolate that component in the ouput using
 * grep, we need to list delta key, as comp name isn't in rootkey.
 *
 * And lastly, we include rootkey for item, as it makes it easier
 * to track between output, what lines up with what.
 */
private	void
dumpEof(sccs *s)
{
	ser_t	d;
	u32	rkoff, dkoff, prev = 0;
	char	cset[MAXKEY];

	if (!s || !CSET(s)) return;

	sccs_rdweaveInit(s);
	while (d = cset_rdweavePair(s, 0, &rkoff, &dkoff)) {
		if (dkoff) {
			prev = dkoff;
			continue;
		}
		sccs_sdelta(s, d, cset);
		printf("%s %s %s\n", HEAP(s, rkoff), HEAP(s, prev), cset);
	}
	sccs_rdweaveDone(s);
}
