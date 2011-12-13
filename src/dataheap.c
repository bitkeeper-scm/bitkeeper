#include "sccs.h"

private	void	dumpStats(sccs *s);

//#define PAGING 1

/*
 * Add a null-terminated string to the end of the data heap at s->heap
 * and return a pointer to the offset where this string is stored.
 * This heap will be written to disk when using the binary file
 * format.
 */
u32
sccs_addStr(sccs *s, char *str)
{
	int	off;

	assert(s);
	unless (s->heap.buf) data_append(&s->heap, "", 1);
	unless (str) return (0);
	off = s->heap.len;

	/* can't add a string from the heap to it again (may realloc) */
	assert((str < s->heap.buf) || (str >= s->heap.buf + s->heap.size));

	data_append(&s->heap, str, strlen(str)+1);
	return (off);
}


/*
 * Extend the last item added with sccs_addStr by appending this string to
 * the end of the heap.
 */
void
sccs_appendStr(sccs *s, char *str)
{
	assert(s->heap.buf[s->heap.len] == 0);
	--s->heap.len;		/* remove trailing null */
	data_append(&s->heap, str, strlen(str)+1);
}

/*
 * Like sccs_addStr(), but it first checks a cache of recently added
 * strings and reuses any duplicates found.
 */
u32
sccs_addUniqStr(sccs *s, char *str)
{
	u32	off;
	hash	*h;
	ser_t	d;
	symbol	*sym;

	unless (str) return (0);
	unless (h = s->uniqheap) {
		h = s->uniqheap = hash_new(HASH_MEMHASH);

#define	addField(x) if (x##_INDEX(s, d)) \
    hash_insertStrI32(h, s->heap.buf + x##_INDEX(s, d), x##_INDEX(s, d))

		/* add existing data to heap */
		for (d = TREE(s); d <= TABLE(s); d++) {
			addField(USERHOST);
			addField(PATHNAME);
			addField(SORTPATH);
			addField(ZONE);
			addField(SYMLINK);
			addField(CSETFILE);
		}
#undef addField
		EACHP(s->symlist, sym) {
			unless (sym->symname) continue;
			hash_insertStrI32(h, SYMNAME(s, sym), sym->symname);
		}
	}

	if (hash_insertStrI32(h, str, 0)) {
		/* new string, add to heap */
		off = sccs_addStr(s, h->kptr);
		*(i32 *)h->vptr = off;
	} else {
		/* already exists */
		off = *(i32 *)h->vptr;
	}
	return (off);
}

#define	PAGESZ		(4<<10)
#define	BLOCKSZ		(16<<10)
#define	PAGEUP(a, sz)	((((unsigned long)(a) - 1) | ((sz)-1)) + 1)
#define	PAGEDOWN(a, sz)	((unsigned long)(a) & ~((sz)-1))

struct	MAP {
	u8	*start;
	u8	*end;
	FILE	*f;
	long	off;
	int	byteswap;	/* byteswap u32's in this region */
};

private	char	**maps;

private void
swaparray(void *data, int len)
{
	u32	*p = data;
	int	i;

	assert((len % 4) == 0);
	len /= 4;
	for (i = 0; i < len; i++) p[i] = le32toh(p[i]);
}

#ifdef	PAGING

private void
faulthandler(int sig, siginfo_t *si, void *unused)
{
	MAP	*map;
	int	i;
	int	len;
	u8	*addr;

	addr = si->si_addr;
	EACH_STRUCT(maps, map, i) {
		if ((addr < map->start) || (addr > map->end)) {
			continue;
		}
		addr = (u8*)PAGEDOWN(addr, PAGESZ);
		mprotect(addr, PAGESZ, PROT_READ|PROT_WRITE);
		if (fseek(map->f, map->off + (addr - map->start), SEEK_SET)) {
			perror("fseek");
		}
		len = min(PAGESZ, map->end - addr);
		fread(addr, 1, len, map->f);
		if (map->byteswap) swaparray(addr, len);
		return;
	}
	fprintf(stderr, "No mapping found\n");
	exit(EXIT_FAILURE);
}
#endif

MAP *
datamap(void *start, int len, FILE *f, long off, int byteswap)
{
#ifdef	PAGING
	u8	*sptr = start;
	u8	*eptr = sptr + len;
	MAP	*map;
	struct	sigaction sa;

	if (IS_LITTLE_ENDIAN()) byteswap = 0;

	if (len < 4*PAGESZ) {
		/* too small to mess with */
		fseek(f, off, SEEK_SET);
		fread(start, 1, len, f);
		if (byteswap) swaparray(start, len);
		return (0);
	}

	sptr = (u8*)PAGEUP(start, PAGESZ);
	//fprintf(stderr, "load %p/%x up=%p\n", start, len, sptr);
	if (sptr != start) {
		fseek(f, off, SEEK_SET);
		len = sptr - (u8*)start;
		fread(start, 1, len, f);
		if (byteswap) swaparray(start, len);
	}

	//fprintf(stderr, "prot %p/%lx\n", sptr, eptr-sptr);
	len = eptr-sptr;
	if (mprotect(sptr, PAGEUP(len, PAGESZ), PROT_NONE)) {
		perror("mprotect");
	}
	unless (maps) {
		sa.sa_flags = SA_SIGINFO;
		sigemptyset(&sa.sa_mask);
		sa.sa_sigaction = faulthandler;
		if (sigaction(SIGSEGV, &sa, 0) == -1) {
			perror("sigaction");
		}
	}

	map = new(MAP);
	map->start = sptr;
	map->end = eptr;
	map->f = f;
	map->off = off + (sptr - (u8*)start);
	map->byteswap = byteswap;
	maps = addLine(maps, map);

	return (map);
#else
	if (IS_LITTLE_ENDIAN()) byteswap = 0;
	fseek(f, off, SEEK_SET);
	fread(start, 1, len, f);
	if (byteswap) swaparray(start, len);
	return (0);
#endif
}

void
dataunmap(MAP *map)
{
	int	i;
	MAP	*m;

	EACH_STRUCT(maps, m, i) {
		if (map == m) {
			removeLineN(maps, i, free);
			return;
		}
	}
	assert(0);
}

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

const	char	const *delta_flagNames[] = {
	"INARRAY",		/* 0 */
	"NONEWLINE",		/* 1 */
	"CKSUM",		/* 2 */
	"SORTSUM",		/* 3 */
	"META",			/* 4 */
	"SYMBOLS",		/* 5 */
	0,			/* 6 */
	"DANGLING",		/* 7 */
	"TAG",			/* 8 */
	"SYMGRAPH",		/* 9 */
	"SYMLEAF",		/* 10 */
	"MODE",			/* 11 */
	"CSET",			/* 12 */
	"XFLAGS",		/* 13 */
	"NPARENT",		/* 14 */
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
	c = 0;
	for (i = 1; i < 32; i++) { /* skip INARRAY */
		if (FLAGS(s, d) & (1 << i)) {
			if (c) printf(",");
			c = 1;
			printf("%s", delta_flagNames[i]);
		}
	}
	printf(")\n");

	/* heap print */
#define HPRINT(f) \
    if (f##_INDEX(s, d)) printf(#f ": %s\n", f(s, d))
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
	if (HAS_COMMENTS(s, d)) printf("comments: %s", COMMENTS(s, d));
}

private void
dumpStats(sccs *s)
{
	int	size;
	int	i, off;
	char	*t;
	ser_t	d;
	char	*names[] = {
		"cludes", "comments", "bamhash", "random",
		"userhost", "pathname", "sortPath", "zone",
		"symlink", "csetFile"
	};
	int	htotal[10] = {0};
	hash	*seen = hash_new(HASH_MEMHASH);

	printf("file: %s\n", s->sfile);
	printf("filesize: %7s\n", psize(s->size));
	printf("    heap: %7s\n", psize(s->heap.len));

	for (d = TREE(s); d <= TABLE(s); d++) {
		for (i = 0; i < 10; i++) {
			unless (off = *(&CLUDES_INDEX(s, d) + i)) continue;
			if (hash_insert(seen, &off, sizeof(off), 0, 0)) {
				htotal[i] += strlen(s->heap.buf + off) + 1;
			}
		}
	}
	size = 0;
	for (i = 0; i < 10; i++) {
		unless (htotal[i]) continue;
		printf("%10s: %7s %4.1f%%\n",
		    names[i], psize(htotal[i]),
		    (100.0 * htotal[i]) / s->heap.len);
		size += htotal[i];
	}
	size = s->heap.len - size;
	if (size) {
		printf("%10s: %7s %4.1f%%\n",
		    "unused", psize(size),
		    (100.0 * size) / s->heap.len);
	}

	printf("   table: %7s\n", psize(sizeof(d_t) * (TABLE(s) + 1)));
	if (nLines(s->symlist)) {
		printf(" symlist: %7s\n",
		    psize(nLines(s->symlist) * sizeof(symbol)));
	}
	size = 0;
	sccs_rdweaveInit(s);
	while (t = sccs_nextdata(s)) {
		size += strlen(t)+1;
	}
	sccs_rdweaveDone(s);
	printf("   weave: %7s\n", psize(size));
}
