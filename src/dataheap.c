#include "sccs.h"

private	void	dumpStats(sccs *s);

/*
 * We require the 3 argument sa_sigaction handler for signals instead of
 * the older 1 argument sa_handler.
 * We also can't support paging on older MacOS because of paging bugs.
 * (10.7 is known to work and 10.4 fail)
 * This currently rules out mac, netbsd and Windows.
 */
#if defined(SA_SIGINFO) && !defined(__MACH__)
#define PAGING 1
#endif

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

private void
swaparray(void *data, int len)
{
	u32	*p = data;
	int	i;

	assert((len % 4) == 0);
	len /= 4;
	for (i = 0; i < len; i++) p[i] = le32toh(p[i]);
}

/* default paging size */
#define	       BLOCKSZ		(256<<10)

#ifdef	PAGING

// assumed OS page size
#define        PAGESZ		(pagesz)
// round 'a' up to next next multiple of PAGESZ
#define        PAGEUP(a)	((((unsigned long)(a) - 1) | (PAGESZ-1)) + 1)
// round 'a' down to the start of the current page
#define        PAGEDOWN(a)	((u8 *)((unsigned long)(a) & ~(PAGESZ-1)))

struct	MAP {
	sccs	*s;
	char	*name;
	u8	*bstart;	/* start of whole block */
	int	blen;		/* length of whole block */
	u8	*start;		/* first byte in range */
	int	len;		/* length of range */
	long	off;		/* offset in file of 'start' */
	int	byteswap;	/* byteswap u32's in this region */

	/* page pointers point to first byte in page */
	u8	*startp;	/* page containing first byte in range */
	u8	*endp;		/* page containing last byte in range */
};
private	MAP	**maps;
private	long	pagesz;

private	void *
allocPage(size_t size)
{
	return (valloc(PAGEUP(size)));
}

private int
map_block(MAP *map)
{
	if (mprotect(map->startp, map->endp - map->startp + PAGESZ,
		PROT_READ|PROT_WRITE)) {
		perror("mprotect");
		exit(1);
	}
	if (fseek(map->s->pagefh, map->off, SEEK_SET)) {
		perror("fseek");
		return (-1);
	}
	if (fread(map->start, 1, map->len, map->s->pagefh) != map->len) {
		perror("fread");
		return (-1);
	}
	if (map->byteswap) swaparray(map->start, map->len);
	return (0);
}

#ifdef WIN32

mprotect becomes:
	u32 oldprotect;
	VirtualProtect(sptr, len, PAGE_NO_ACCESS, &oldprotect);

sigaction becomes:
	AddVectoredExecptionHandler
#endif

// MAP only
private int
MAPcmp(const void *va, const void *vb)
{
	MAP	*a = *(MAP **)va;
	MAP	*b = *(MAP **)vb;

	if (a->start < b->start) {
		return (-1);
	} else if (a->start > b->start) {
		return (1);
	} else {
		return (0);
	}
}

// MAP only
private void
faulthandler(int sig, siginfo_t *si, void *unused)
{
	MAP	*map;
	int	i;
	u8	*addr;		/* the page where the fault occurred */
	int	found = 0;

	addr = PAGEDOWN(si->si_addr);
	EACH_STRUCT(maps, map, i) {
		if (addr < map->startp) break;
		if (addr > map->endp) continue;

		found = 1;
		if (map_block(map)) {
			perror("map_block");
			exit(1);
		}
		TRACE("fault in %s-%s load %dK at %.1f%%",
		    map->s->gfile, map->name,
		    map->len / 1024,
		    (100.0 * (addr - map->bstart))/map->blen);
		if ((i > 1) &&
		    (maps[i-1]->endp == map->startp)) {
			/*
			 * Two regions share a page, but we couldn't
			 * have hit that page of overlap or the
			 * previous block would be mapped.  Reprotect
			 * the overlap block so the other region won't
			 * be missed.
			 */
			assert(addr != map->startp);
			TRACE("protect start %p", map->startp);
			if (mprotect(map->startp, PAGESZ, PROT_NONE)) {
				perror("mprotect");
				exit(1);
			}
		}
		if ((i < nLines(maps)) &&
		    (maps[i+1]->startp == map->endp) &&
		    (addr != map->endp)) {
			/* region doesn't end on a page boundry and the next
			 * region uses the rest of the end page and the fault
			 * didn't occurr on that last page.
			 *
			 * Here we load the whole map, but leave the last page
			 * protected.  We will unprotect that last page when
			 * the next map gets loaded.
			 */
			TRACE("protect end %p", map->endp);
			if (mprotect(map->endp, PAGESZ, PROT_NONE)) {
				perror("mprotect");
				exit(1);
			}
		}
		removeArrayN(maps, i);
		--i;
	}
	unless (found) {
		fprintf(stderr, "seg fault to addr %p\n", si->si_addr);
		TRACE("seg fault to addr %p", si->si_addr);
		abort();
	}
}

/*
 * Unprotect all the memory regions for a given sccs*
 *
 * if keep==1 then map in any remaining data
 */
void
dataunmap(sccs *s, int keep)
{
	int	i;
	MAP	*m;

	unless (s->pagefh) return;
	EACH_STRUCT(maps, m, i) {
		if (m->s != s) continue;

		if (keep) {
			map_block(m);
		} else {
			if (mprotect(m->startp,
				m->endp - m->startp + PAGESZ,
				PROT_READ|PROT_WRITE)) {
				perror("mprotect");
				exit(1);
			}
		}
		free(m);
		removeArrayN(maps, i);
		--i;
	}
	unless (nLines(maps)) {
		freeLines((char **)maps, 0);
		maps = 0;
	}
}

#else // no PAGING apis available

private	void *
allocPage(size_t size)
{
	return (malloc(size));
}

void
dataunmap(sccs *s, int keep)
{
	return;
}
#endif

/*
 * Allocate memory and load 'nmemb' blocks of 'esize' from 'off' of
 * s->sfile.
 *
 * If the data range is large then the data will be mapped into memory
 * but only loaded on demand as needed.
 */
void *
datamap(sccs *s, char *name, u32 esize, u32 nmemb, long off, int byteswap)
{
	u8	*start;		/* address of data from file */
	u8	*mstart;	/* address to be returned */
	u32	len;
	int	paging = 1;
	char	*p;
#ifdef	PAGING
	MAP	*map;
	u32	*lens = 0;
	u8	*rstart;
	u32	rlen;
	int	i;

	struct	sigaction sa;

	unless (pagesz) pagesz = sysconf(_SC_PAGESIZE);
	assert(pagesz && !(pagesz & (pagesz-1))); /* non-zero power of 2 */
#else
	paging = 0;
#endif
	if (getenv("_BK_NO_PAGING")) paging = 0;
	/* NOTE: set pagesz even if not paging: it is used in bin_sortHeap() */
	if (p = getenv("_BK_PAGING_PAGESZ")) {
		s->pagesz = strtoul(p, &p, 10);
		switch(*p) {
		    case 'k': case 'K':
			s->pagesz *= 1024;
			break;
		    case 'm': case 'M':
			s->pagesz *= 1024 * 1024;
			break;
		}
	} else {
		s->pagesz = BLOCKSZ;
	}
	if (IS_LITTLE_ENDIAN()) byteswap = 0;
	len = nmemb * esize;
	if (!paging ||
	    (!getenv("_BK_FORCE_PAGING") && (len < 4*s->pagesz))) {
		/* too small to mess with */
		paging = 0;
	}
	if (esize == 1) {
		mstart = paging ? allocPage(len) : malloc(len);
		start = mstart;
	} else {
		mstart = allocArray(nmemb, esize, paging ? allocPage : 0);
		start = mstart + esize;
	}
	unless (paging) {
nopage:		fseek(s->fh, off, SEEK_SET);
		fread(start, 1, len, s->fh);
		if (byteswap) swaparray(start, len);
		return (mstart);
	}
#ifdef PAGING

	if (mprotect(PAGEDOWN(start),
	    PAGEDOWN(start + len - 1) - PAGEDOWN(start) + PAGESZ,
	    PROT_NONE)) {
		putenv("_BK_NO_PAGING=1");
		paging = 0;
		goto nopage;
	}

	/*
	 * save s->fh in s->pagefh, and eventually clear s->fh (bin_mkgraph)
	 * so slib.c will open new filehandle for weave
	 */
	if (s->pagefh) {
		assert(s->pagefh == s->fh);
	} else {
		s->pagefh = s->fh;
	}

	TRACE("map %s:%s %p-%p", s->gfile, name, start, start+len);

	unless (maps) {
		sa.sa_flags = SA_SIGINFO;
		sigemptyset(&sa.sa_mask);
		sa.sa_sigaction = faulthandler;
		if (sigaction(SIGBUS, &sa, 0) == -1) {
			perror("sigaction");
		}
		if (sigaction(SIGSEGV, &sa, 0) == -1) {
			perror("sigaction");
		}
	}
	if (fgzip_findSeek(s->pagefh, off, len, s->pagesz, &lens)) {
		/* fgzip failed so we assume it wasn't using fgzip... */
		rlen = len;
		while (rlen > s->pagesz) {
			addArray(&lens, &s->pagesz);
			rlen -= s->pagesz;
		}
		addArray(&lens, &rlen);
	}
	rstart = start;
	EACH(lens) {
		rlen = lens[i];
		assert(rlen > 0);
		map = new(MAP);
		map->s = s;
		map->name = name;
		map->bstart = start;
		map->blen = len;
		map->start = rstart;
		map->len = rlen;
		map->startp = PAGEDOWN(rstart);
		map->endp = PAGEDOWN(rstart + rlen - 1);
		map->off = off;
		map->byteswap = byteswap;
		addArray(&maps, &map);

		rstart += rlen;
		off += rlen;
 	}
	free(lens);
	sortArray(maps, MAPcmp); /* sort by start address */
#endif
	return (mstart);
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
	int	i, off, sym;
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
	sym = 0;
	EACH(s->symlist) {
		unless (off = s->symlist[i].symname) continue;
		if (hash_insert(seen, &off, sizeof(off), 0, 0)) {
			sym += strlen(s->heap.buf + off) + 1;
		}
	}
	printf("%10s: %7s %4.1f%%\n",
	    "symnames", psize(sym),  (100.0 * sym) / s->heap.len);
	size -= sym;
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
	sccs_rdweaveInit(s);
	while (t = sccs_nextdata(s)) {
		size += strlen(t)+1;
	}
	sccs_rdweaveDone(s);
	printf("   weave: %7s\n", psize(size));
}
