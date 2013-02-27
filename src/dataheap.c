#include "sccs.h"

/*
 * We require the 3 argument sa_sigaction handler for signals instead of
 * the older 1 argument sa_handler.
 * We also can't support paging on older MacOS because of paging bugs.
 * (10.7 is known to work and 10.4 fail)
 * This currently rules out old versions of macos, netbsd and Windows.
 */
#if defined(SA_SIGINFO) &&						\
	(!defined(__MACH__) || (__MAC_OS_X_VERSION_MIN_REQUIRED >= 1070))
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
	char	*old;

	assert(s);
	unless (s->heap.buf) data_append(&s->heap, "", 1);
	unless (str) return (0);
	off = s->heap.len;

	/* can't add a string from the heap to it again (may realloc) */
	assert((str < s->heap.buf) || (str >= s->heap.buf + s->heap.size));

	old = s->heap.buf;
	data_append(&s->heap, str, strlen(str)+1);
	if (CSET(s) && (s->heap.buf != old)) {
		T_PERF("%s: did realloc() of heap adding %s", s->gfile, str);
	}
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
	unless (h = s->uniqheap) h = s->uniqheap = hash_new(HASH_MEMHASH);
	unless (s->uniqdeltas) {
		s->uniqdeltas = 1;

		if (CSET(s)) {
			T_PERF("%s: loading existing uniqheap data", s->gfile);
		}
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
		if (CSET(s)) T_PERF("done uniqheap data");
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

/*
 * Used for rootkeys in the weave, add the key to the heap and collapse
 * duplicates together.
 */
u32
sccs_addUniqKey(sccs *s, char *key)
{
	hash	*h;
	u32	off, old;

	unless (h = s->uniqheap) h = s->uniqheap = hash_new(HASH_MEMHASH);
	unless (s->uniqkeys) {
		s->uniqkeys = 1;
		T_PERF("%s: loading existing rootkey data", s->gfile);
		/*
		 * This is the first time we have added a rootkey so
		 * we need to walk the list of existing rootkeys and
		 * add them to the uniq hash
		 */
		for (off = RKHEAD(s); off; off = RKNEXT(s, off)) {
			hash_insertStrI32(h, RKSTR(s, off), off + sizeof(off));
		}
		T_PERF("done rootkey data");
	}
	if (hash_insertStrI32(h, key, 0)) {
		/* new string */

		// update rkey list
		off = s->heap.len;
		old = htole32(RKHEAD(s));
		data_append(&s->heap, &old, sizeof(old));
		RKHEAD(s) = off;

		// add string to heap
		off = sccs_addStr(s, h->kptr);
		*(i32 *)h->vptr = off;

		TRACE("add(%s) = %s(%d)", key, s->heap.buf + off, off);
	}
	off = *(i32 *)h->vptr;
	return (off);
}

/*
 * New weave entry for d.  The 'keys' array alternates root and delta
 * keys.  Write _all_ the rootkeys first as the delta keys for a cset
 * need to be written concatenated.  It wouldn't work if rootkeys and
 * delta keys were written interleaved.
 */
void
weave_set(sccs *s, ser_t d, char **keys)
{
	int	i;
	u32	off;
	DATA	w = {0};

	/* All dkeys need to be concatenated; write all rkeys to heap first */
	EACH(keys) {
 		off = htole32(sccs_addUniqKey(s, keys[i]));
 		data_append(&w, &off, sizeof(off));
 		data_append(&w, keys[i+1], strlen(keys[i+1]) + 1);
		++i;
	}
	WEAVE_SET(s, d, s->heap.len);
 	data_append(&s->heap, w.buf, w.len);
 	free(w.buf);
 	off = 0;
 	data_append(&s->heap, &off, sizeof(off));
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

u32	swapsz;			/* size bk swaps data from file */

#ifdef	PAGING

// assumed OS page size
#define        PAGESZ		((long)pagesz)
// round 'a' up to next next multiple of PAGESZ
#define        PAGEUP(a)	((((unsigned long)(a) - 1) | (PAGESZ-1)) + 1)
// round 'a' down to the start of the current page
#define        PAGEDOWN(a)	((u8 *)((unsigned long)(a) & ~(PAGESZ-1)))

struct	MAP {
	FILE	*f;
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
private	u32	pagesz;		/* size of system's page tables */

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
	if (fseek(map->f, map->off, SEEK_SET)) {
		perror("fseek");
		return (-1);
	}
	if (fread(map->start, 1, map->len, map->f) != map->len) {
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
		T_O1("fault in %s load %dK at %.1f%%",
		    map->name,
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
			T_DEBUG("protect start %p", map->startp);
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
			T_DEBUG("protect end %p", map->endp);
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
dataunmap(FILE *f, int keep)
{
	int	i;
	MAP	*m;
	int	cnt = 0;
	char	*name = 0;

	EACH_STRUCT(maps, m, i) {
		if (m->f != f) continue;

		if (keep) {
			map_block(m);
			++cnt;
			name = m->name;
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
	if (cnt) T_PERF("dataunmap loaded %d blocks from %s", cnt, name);
}

#else // no PAGING apis available

private	void *
allocPage(size_t size)
{
	return (malloc(size));
}

void
dataunmap(FILE *f, int keep)
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
datamap(char *name, u32 esize, u32 nmemb,
    FILE *f, long off, int byteswap, int *didpage)
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

	unless (pagesz) {
		pagesz = sysconf(_SC_PAGESIZE);
		/* non-zero power of 2? */
		assert(pagesz && !(pagesz & (pagesz-1)));
	}
#else
	paging = 0;
#endif
	if (getenv("_BK_NO_PAGING")) paging = 0;
	/* NOTE: set swapsz even if not paging: it is used in bin_sortHeap() */
	unless (swapsz) {
		if (p = getenv("_BK_PAGING_PAGESZ")) {
			swapsz = strtoul(p, &p, 10);
			switch(*p) {
			    case 'k': case 'K':
				swapsz *= 1024;
				break;
			    case 'm': case 'M':
				swapsz *= 1024 * 1024;
				break;
			}
		} else {
			swapsz = (256<<10); /* default paging size */
		}
	}
	if (IS_LITTLE_ENDIAN()) byteswap = 0;
	len = nmemb * esize;
	if (!paging ||
	    (!getenv("_BK_FORCE_PAGING") && (len < 4*swapsz))) {
		/* too small to mess with */
		paging = 0;
	}
	if (esize == 1) {
		int	size = 1024;
		while (size < len) size *= 2;
		mstart = paging ? allocPage(size) : malloc(size);
		start = mstart;
	} else {
		mstart = allocArray(nmemb, esize, paging ? allocPage : 0);
		start = mstart + esize;
	}
	unless (paging) {
#ifdef PAGING
nopage:
#endif
		fseek(f, off, SEEK_SET);
		fread(start, 1, len, f);
		if (byteswap) swaparray(start, len);
		return (mstart);
	}
#ifdef PAGING
	*didpage = 1;
	if (mprotect(PAGEDOWN(start),
	    PAGEDOWN(start + len - 1) - PAGEDOWN(start) + PAGESZ,
	    PROT_NONE)) {
		putenv("_BK_NO_PAGING=1");
		paging = 0;
		goto nopage;
	}

	T_DEBUG("map %s %p-%p", name, start, start+len);

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
	if (vzip_findSeek(f, off, len, swapsz, &lens)) {
		/* fgzip failed so we assume it wasn't using fgzip... */
		rlen = len;
		while (rlen > swapsz) {
			addArray(&lens, &swapsz);
			rlen -= swapsz;
		}
		addArray(&lens, &rlen);
	}
	rstart = start;
	EACH(lens) {
		rlen = lens[i];
		assert(rlen > 0);
		map = new(MAP);
		map->f = f;
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

/*
 * Load a 32-bit number that is stored on the heap in little-endian
 * format.  On x86 this is just '*(u32 *)ptr'.  The ptr is not aligned
 * on a 32-bit boundry so on machines that fault with unaligned loads
 * we need to either do a byte-wise load or we need to use the native
 * unaligned load instructions.  Also on big-endian machines we have
 * to byte-swap this number after loading.  Notice the two transforms
 * are independant and a given machine can use either one.
 */
u32
_heap_u32load(void *ptr)
{
	u32	ret;
#ifdef	__GNUC__
	/*
	 * This is an efficient way to tell GCC that a 32-bit value is
	 * stored unaligned and needs to be loaded.  It will used
	 * native unaligned load instructions if they exist in the
	 * current architecture.
	 */
	struct u32_u {
		u32 v;
	} __attribute__((__packed__));

	ret = ((struct u32_u *)ptr)->v;
#else
	/* this is a slow portable way to load an unaligned value */
	memcpy(&ret, ptr, sizeof(ret));
#endif
	return (le32toh(ret));
}
