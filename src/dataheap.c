/*
 * Copyright 2011-2016 BitMover, Inc
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

/*
 * Heap repack versions:
 *   1 The original bk-6.0 format and used for all bk-6.x
 *   2 Added the nokey hash to the heap (not released to customers)
 *   3 Unused, version that never hit dev
 *   4 BWEAVE3, weave layout changed (bk-7.0 release)
 *   5 RKTYPES offset in metadata
 */
#define	HEAP_VER	"5"
#define	UNIQHASH_CLEAN	2	/* version 1 did not clean meta hash */
#define	IN_HEAP(s, x)	(((x) >= HEAP(s, 0)) && ((x) < HEAP(s, s->heap.len)))

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
	u32	off;
	char	*old;

	assert(s);
	unless (s->heap.buf) data_append(&s->heap, "", 1);
	unless (str) return (0);
	off = s->heap.len;

	/* can't add a string from the heap to it again (may realloc) */
	assert(!IN_HEAP(s, str));

	old = HEAP(s, 0);
	data_append(&s->heap, str, strlen(str)+1);
	if (CSET(s) && (HEAP(s, 0) != old)) {
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
	assert(*HEAP(s, s->heap.len) == 0);
	--s->heap.len;		/* remove trailing null */
	data_append(&s->heap, str, strlen(str)+1);
}

/*
 * If the data heap contains a pre-calcuated hash table then use that
 * to find strings in the heap.
 */
private u32
findUniq1Str(sccs *s, char *key)
{
	int	j;
	char	*t;
	u32	off;

	unless (s->uniq1init) {
		/* only trust if a reliable version did last repack */
		if ((hash_fetchStrNum(s->heapmeta, "VER") >= UNIQHASH_CLEAN) &&
		    (t = hash_fetchStr(s->heapmeta, "HASH"))) {
			off = strtoul(t, 0, 16);
			j = atoi(hash_fetchStr(s->heapmeta, "HASHBITS"));
			s->uniq1 = nokey_newStatic(off, j);
			t = hash_fetchStr(s->heapmeta, "LEN");
			assert(t);
			s->uniq1off = strtoul(t, 0, 16);
		} else {
			s->uniq1 = 0;
			s->uniq1off = 0;
		}
		s->uniq1init = 1;
	}
	unless (s->uniq1) return (0);
	assert(s->uniq1off);
	return (nokey_lookup(s->uniq1, HEAP(s, 0), key));
}

/*
 * Like sccs_addStr(), but it first checks a cache of recently added
 * strings and reuses any duplicates found.
 */
u32
sccs_addUniqStr(sccs *s, char *str)
{
	u32	off, hlen;
	ser_t	d;
	symbol	*sym;

	unless (str) return (0);
	unless (s->heap.buf) data_append(&s->heap, "", 1);

	/* look in uniq1hash first */
	if (off = findUniq1Str(s, str)) return (off);

	unless (s->uniq2) s->uniq2 = nokey_newAlloc();
	unless (s->uniq2deltas) {
		s->uniq2deltas = 1;

		if (CSET(s)) {
			T_PERF("%s: loading existing uniq2 data", s->gfile);
		}

		/*
		 * Anything less than this is already in uniqhash so
		 * we don't need to index it.
		 */
		hlen = s->uniq1off;

#define	addField(x)			\
	({u32 _idx = x##_INDEX(s, d);	\
	  if (_idx && (_idx >= hlen))	\
		nokey_insert(s->uniq2, HEAP(s, 0), _idx); })

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
			if (sym->symname < hlen) continue;
			nokey_insert(s->uniq2, HEAP(s, 0), sym->symname);
		}
		if (CSET(s)) T_PERF("done uniq2 data");
	}
	unless (off = nokey_lookup(s->uniq2, HEAP(s, 0), str)) {
		if (IN_HEAP(s, str)) {
			/*
			 * Reuse makes sccs_addStr() happy (see assert there).
			 * Partition can use a substring of a heap'd pathname.
			 */
			off = (u32)(str - HEAP(s, 0));
		} else {
			/* new string, add to heap */
			off = sccs_addStr(s, str);
		}
		nokey_insert(s->uniq2, HEAP(s, 0), off);
	}
	return (off);
}

private void
loadRootkeys(sccs *s)
{
	u32	off, ptr, hlen;

	unless (s->uniq2) s->uniq2 = nokey_newAlloc();
	s->uniq2keys = 1;
	T_PERF("%s: loading existing rootkey data", s->gfile);
	assert(s->uniq1init);
	hlen = s->uniq1off;

	/*
	 * This is the first time we have added a rootkey so
	 * we need to walk the list of existing rootkeys and
	 * add them to the uniq hash
	 */
	for (off = s->rkeyHead; off; off = KOFF(s, off)) {
		ptr = off+4;
		if (ptr < hlen) break; /* uniq1 gets from here on */
		nokey_insert(s->uniq2, HEAP(s, 0), ptr);
	}
	T_PERF("done rootkey data");
}

/*
 * Look for a rootkey in the ChangeSet file's weave data and if one is
 * found return the heap offset to where the string is stored.
 * This tells if this ChangeSet file has seen this key as a rootkey
 * in the past.  It may return a false positive and say a key is
 * included in the weave when it isn't actually there.  For example
 * after an undo before the dataheap has been repacked to remove the
 * discarded rootkeys.
 *
 * Ret:
 *   0     == key is certainly not in weave
 *   <num> == key might be in weave and if so, string is at this offset.
 */
u32
sccs_hasRootkey(sccs *s, char *key)
{
	u32	ret;

	/* look in uniqhash first */
	if (ret = findUniq1Str(s, key)) return (ret);

	unless (s->uniq2keys) loadRootkeys(s);
	return (nokey_lookup(s->uniq2, HEAP(s, 0), key));
}

/*
 * Used for rootkeys in the weave, add the key to the heap and collapse
 * duplicates together.
 * Returns an offset to the key on the heap, but the previous 4 bytes
 * is the offset of the rkeyHead linked list.
 */
u32
sccs_addUniqRootkey(sccs *s, char *key)
{
	u32	off, le32;

	if (off = findUniq1Str(s, key)) return (off);
	unless (s->uniq2keys) loadRootkeys(s);

	unless (off = nokey_lookup(s->uniq2, HEAP(s, 0), key)) {
		/* new string, add to heap */

		// update rkey list
		off = s->heap.len;
		le32 = htole32(s->rkeyHead);
		data_append(&s->heap, &le32, 4);
		s->rkeyHead = off;
		// add string to heap
		off = s->heap.len;
		sccs_addStr(s, key);
		nokey_insert(s->uniq2, HEAP(s, 0), off);

		TRACE("add(%s) = %d", key, off);
	}
	return (off);
}

/*
 * Load a hash from a known place in the heap set up by bin_heapRepack()
 */
void
sccs_loadHeapMeta(sccs *s)
{
	char	*t;

	if ((s->heap.len >= 5) && (strneq(HEAP(s, 1), "GEN=", 4))) {
		s->heapmeta = hash_new(HASH_MEMHASH);
		hash_fromStr(s->heapmeta, s->heap.buf+1);

		if ((hash_fetchStrNum(s->heapmeta, "VER") >= UNIQHASH_CLEAN) &&
		    (t = hash_fetchStr(s->heapmeta, "RKTYPES"))) {
			s->rktypeoff.comp = strtoul(t, &t, 16);
			assert(*t == '/');
			s->rktypeoff.bam = strtoul(t+1, 0, 16);
		}
	}
}

/*
 * Convert between BWEAVEv2 and BWEAVEv3 in memory
 */
void
weave_cvt(sccs *s)
{
	ser_t	d;
	u32	off, rkoff, dkoff;
	char	dkey[MAXKEY];

	assert(BWEAVE2(s) != BWEAVE2_OUT(s));
	T_PERF("weave_cvt(%d)", (BWEAVE2(s) ? 2 : 3));
	/*
	 * walk the old binary weave and generate a block in the heap
	 * that will continue the new weave layout.
	 */
	for (d = TABLE(s); d >= TREE(s); d--) {
		unless (off = WEAVE_INDEX(s, d)) continue;
		WEAVE_SET(s, d, s->heap.len);
		while (off = RKDKOFF(s, off, rkoff, dkoff)) {
			unless (dkoff) continue; /* skip last key marker */
			if (BWEAVE2_OUT(s)) {
				/* point rk at nextkey */
				rkoff = htole32(rkoff - 4);
				data_append(&s->heap, &rkoff, 4);

				/*
				 * copy key because a heap realloc
				 * could move it.
				 */
				strcpy(dkey, HEAP(s, dkoff));
				data_append(&s->heap, dkey, strlen(dkey) + 1);
			} else {
				rkoff = htole32(rkoff);
				data_append(&s->heap, &rkoff, 4);
				dkoff = htole32(dkoff);
				data_append(&s->heap, &dkoff, 4);
			}
		}
		rkoff = 0;
		data_append(&s->heap, &rkoff, 4);
	}
	s->encoding_in &= ~(E_BWEAVE2|E_BWEAVE3);
	s->encoding_in |= (BWEAVE2_OUT(s) ? E_BWEAVE2 : E_BWEAVE3);
	T_PERF("done");
}

/*
 * New weave entry for d.  The 'keys' array alternates root and delta
 * keys. We write the keys to the heap and then the weave entry that
 * refers to the keys.  Note: rootkeys use a different API because
 * they are deduplicatied.
 */
void
weave_set(sccs *s, ser_t d, char **keys)
{
	int	i, mark;
	char	*rkey, *dkey;
	u32	rkoff, dkoff;
	u32	added = 0;
	DATA	w = {0};

	/* write keys to heap first and save weave */
	EACH(keys) {
		rkey = keys[i];
		dkey = keys[++i];
		++added;
		rkoff = sccs_addUniqRootkey(s, rkey);
		if (mark = (*dkey == '|')) ++dkey;
		if (BWEAVE2(s)) {
			rkoff = htole32(rkoff - 4);
			data_append(&w, &rkoff, 4);
			data_append(&w, dkey, strlen(dkey)+1);
		} else {
			rkoff = htole32(rkoff);
			data_append(&w, &rkoff, 4);
			dkoff = htole32(sccs_addStr(s, dkey));
			data_append(&w, &dkoff, 4);
			if (mark) {	/* last key marker */
				data_append(&w, &rkoff, 4);
				dkoff = 0;
				data_append(&w, &dkoff, 4);
			}
		}
	}
	WEAVE_SET(s, d, s->heap.len);
	data_append(&s->heap, w.buf, w.len);
	free(w.buf);
	rkoff = 0;
	data_append(&s->heap, &rkoff, 4);
	ADDED_SET(s, d, added);
	DELETED_SET(s, d, 0);

	/* Normal non-1.0 non-TAG csets have same==1 */
	if ((d == TREE(s) && !added) || TAG(s, d)) {
		SAME_SET(s, d, 0);
	} else {
		SAME_SET(s, d, 1);
	}
}

/*
 * If we notice after the fact that a delta key in the cset weave
 * has the wrong end marker, this changes it.
 * add==1, add the marker, else remove it
 */
void
weave_updateMarker(sccs *s, ser_t d, u32 rk, int add)
{
	DATA	w = {0};
	u32	woff, rkoff, dkoff, le32;
	int	sawrkey = 0;

	unless (BWEAVE3(s)) return;

	woff = WEAVE_INDEX(s, d);
	while (woff = RKDKOFF(s, woff, rkoff, dkoff)) {
		le32 = htole32(rkoff);
		data_append(&w, &le32, 4);
		le32 = htole32(dkoff);
		data_append(&w, &le32, 4);
		if (rkoff == rk) {
			assert(dkoff);
			if (add) {
				/*
				 * if this delta was already marked
				 * then the code that called this has
				 * a bug.
				 */
				assert(KOFF(s, woff) != rk);

				/* add marker */
				le32 = htole32(rkoff);
				data_append(&w, &le32, 4);
				le32 = 0;
				data_append(&w, &le32, 4);
			} else {
				/* make sure we had marker */
				assert(KOFF(s, woff) == rk);
				assert(KOFF(s, woff+4) == 0);

				woff += 8; /* skip the marker */
			}
			sawrkey = 1;
		}
	}
	assert(sawrkey);
	WEAVE_SET(s, d, s->heap.len);
	data_append(&s->heap, w.buf, w.len);
	free(w.buf);
	le32 = 0;
	data_append(&s->heap, &le32, 4);
}

/*
 * Dump a cset weave file out: format is from cset_mkList() ...
 */
void
weave_replace(sccs *s, weave *cweave)
{
	u32	off;
	weave	*item;
	ser_t	d = 0;
	hash	*first = hash_new(HASH_U32HASH, sizeof(u32), sizeof(u32));
	char	key[MAXKEY];

	T_SCCS("file=%s", s->gfile);
	// yeah we duplicate all the weave table
	for (d = TREE(s); d <= TABLE(s); d++) WEAVE_SET(s, d, 0);
	unless (BWEAVE2(s)) {
		/* compute rootkey termination - first in reverse */
		EACHP_REVERSE(cweave, item) {
			hash_insertU32U32(first, item->rkoff, item->ser);
		}
	}
	d = 0;
	EACHP(cweave, item) {
		if (d != item->ser) {
			if (d) {	/* terminate if prev entry */
				off = 0;
				data_append(&s->heap, &off, 4);
			}
			d = item->ser;
			WEAVE_SET(s, d, s->heap.len);
		}
		if (BWEAVE2(s)) {
			off = htole32(item->rkoff-4);
			data_append(&s->heap, &off, 4);
			/* Caution: heap could realloc; pass in copy */
			strcpy(key, HEAP(s, item->dkoff));
			data_append(&s->heap, key, strlen(key) + 1);
		} else {
			off = htole32(item->rkoff);
			data_append(&s->heap, &off, 4);
			off = htole32(item->dkoff);
			data_append(&s->heap, &off, 4);
			if (hash_fetchU32U32(first, item->rkoff) == item->ser) {
				/* mark termination */
				off = htole32(item->rkoff);
				data_append(&s->heap, &off, 4);
				off = 0;
				data_append(&s->heap, &off, 4);
			}
		}
	}
	if (d) {
		/* if we made an entry, terminate it */
		off = 0;
		data_append(&s->heap, &off, 4);
	}
	hash_free(first);
}

/*
 * Returns 1 if rkoff points at a component rootkey
 */
int
weave_iscomp(sccs *s, u32 rkoff)
{
	if (s->rktypeoff.comp && (rkoff < s->heapsz1)) {
		return (rkoff < s->rktypeoff.comp);
	}
	return (changesetKey(HEAP(s, rkoff)));
}

/* returns 1 if rkoff is a BAM file */
int
weave_isBAM(sccs *s, u32 rkoff)
{
	char	*rkey;

	if (s->rktypeoff.bam && (rkoff < s->heapsz1)) {
		return ((rkoff < s->rktypeoff.bam) &&
		        (rkoff >= s->rktypeoff.comp));
	}
	rkey = HEAP(s, rkoff);
	/* BAM files can be identified with B: at start of random bits */
	unless (BAMkey(rkey)) return (0);

	/* component keys can also have "B:" in randbits */
	if (changesetKey(rkey)) return (0);

	return (1);
}

private void
swaparray(void *data, u32 len)
{
	u32	*p = data;
	u32	i;

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
	u32	blen;		/* length of whole block */
	u8	*start;		/* first byte in range */
	u32	len;		/* length of range */
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
		return (-1);
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
	u32	i;
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
	u32	i;
	MAP	*m;
	u32	cnt = 0;
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
 * Allocate memory, either as a block of bytes (element size 1)
 * or as a bk array.  In both cases, align allocation on a block boundary.
 */
void *
dataAlloc(u32 esize, u32 nmemb)
{
	void	*ret;
#ifdef	PAGING
	unless (pagesz) {
		pagesz = sysconf(_SC_PAGESIZE);
		/* non-zero power of 2? */
		assert(pagesz && !(pagesz & (pagesz-1)));
	}
#endif
	if (esize == 1) {
		size_t	size;

		if (nmemb >= (1 << 30)) {
			size = nmemb;
		} else {
			size = 1024;
			while (size <= nmemb) size *= 2;
		}
		ret = allocPage(size);
	} else {
		ret = allocArray(nmemb, esize, allocPage);
	}
	assert(ret);
	return (ret);
}

/*
 * Load 'len' bytes at 'addr' from 'off' of s->sfile.
 * If the data range is large then the data will be mapped into memory
 * but only loaded on demand as needed.
 */
void
datamap(char *name, void *addr, size_t len,
    FILE *f, long off, int byteswap, int *didpage)
{
	u8	*start = addr;
	int	paging = 1;
	char	*p;
#ifdef	PAGING
	MAP	*map;
	u32	*lens = 0;
	u8	*rstart;
	u32	rlen;
	u32	i;

	struct	sigaction sa;
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
	if (!paging ||
	    (!getenv("_BK_FORCE_PAGING") && (len < 4*swapsz))) {
		/* too small to mess with */
		paging = 0;
	}
	unless (paging) {
#ifdef PAGING
nopage:
#endif
		if (fseek(f, off, SEEK_SET) ||
		    (fread(start, 1, len, f) != len)) {
			perror("datamap read");
		}
		if (byteswap) swaparray(start, len);
		return;
	}
#ifdef PAGING
	*didpage = 1;
	assert(pagesz);
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

/*
 * Look at the current file and decide if the heap should be resorted.
 */
int
bin_needHeapRepack(sccs *s)
{
	char	*t;
	u32	heapsz_orig;

	/* only makes since for BK sfiles */
	unless (BKFILE(s)) return (0);

	/* csets have more restrictions */
	if (CSET(s)) {
		/* no repacking in RESYNC */
		if (proj_isResync(s->proj)) return (0);

		/* no repacking if we have a RESYNC (which shares the heap) */
		if (isdir(proj_fullpath(s->proj, ROOT2RESYNC))) return (0);

		/* Also for comp csets that are copied to prod RESYNC */
		if (proj_isComponent(s->proj) &&
		    isdir(proj_fullpath(proj_product(s->proj), ROOT2RESYNC))) {
			return (0);
		}
	}

	/* testing API */
	if (getenv("_BK_HEAP_NOREPACK")) return (0);

	/* always repack if forced */
	if (getenv("_BK_FORCE_REPACK")) return (1);

	/* repack if no pack (this creates it) */
	unless (s->heapmeta) return (1);

	/* always repack if wrong version */
	unless ((t = hash_fetchStr(s->heapmeta, "VER"))
	    &&  streq(t, HEAP_VER)) {
		return (1);
	}

	/* read old heap len */
	heapsz_orig = strtoul(hash_fetchStr(s->heapmeta, "LEN"), 0, 16);
	assert(heapsz_orig <= s->heap.len);

	/* if new stuff has grown larger than 10% of heap */
	if ((s->heap.len - heapsz_orig) > s->heap.len/10) return (1);

	/* default is no */
	return (0);
}

typedef	struct {
	u32	newrk;
	u32	oldestdk;
} Pair;

/*
 * reorder the data heap in memory to attempt to reduce the number of
 * pages that are needed for common operations.
 * v2 - bk-6.0 header
 *   0 - first byte is null string
 *   hash - keys = {GEN, LEN, TIP, VER} (hash string sorted alphabetically)
 *   s->rkeyHead = heap offset gets stored in on disk init table
 *   (this can contain keys from later revs if downgrading)
 * v3 and a version that started with %GEN= were never shipped
 * v4 post 6.0 header
 *   0 - first byte is null string
 *   hash - keys = {GEN, HASH, HASHBITS, LEN, TIP, VER}
 *	new are HASH and HASHBITS, which only work if VER >= UNIQHASH_CLEAN
 *   s->rkeyHead = heap offset gets stored in on disk init table
 * v5 bk-7.0.x
 *   Add RKTYPES and rootkeys sorted by types
 *
 * HEAP LAYOUT
 *   meta hash
 *   one 'page' of data for the top csets (about 500)
 *   All remaining data for the following fields in table order
 *     RANDOM
 *     PATHNAME
 *     SORTPATH
 *     ZONE
 *     SYMLINK
 *     CSETFILE
 *     USER@HOST
 *     INCLUDES/EXCLUDES
 *     BAMHASH
 *     All tag names
 *     COMMENTS
 *
 *   For BWEAVE3 ChangeSet files:
 *     non-tip deltakeys in weave order
 *     deltakeys in tip cset in weave order
 *     component rootkeys
 *     BAM rootkeys
 *     remaining file rootkeys in weave order
 *     actual weave data
 *
 *   nokey hash of unique data
 */
void
bin_heapRepack(sccs *s)
{
	u32	i, old;
	ser_t	d, ds;
	u32	size, len, off;
	u32	rkoff, dkoff;
	char	*rkey, *dkey;
	u32	uhash;
	u32	metalen;
	Pair	*pair;
	u32	bits;
	DATA	oldheap;
	DATA	tipkeys = {0};	/* deltakey tipkey array */
	DATA	*dkeys;
	u32	*rkeys = 0;	/* list of rkoff's in old heap */
	u32	tipkeys_start;
	u32	*weave = 0;	/* array of u32's containing weave */
	hash	*meta;
	char	*t;
	int	first_time = (s->heapmeta == 0);
	char	buf[MAXLINE];

/* Local versions of sccs.h macros for a copy of the heap */
#define	OLDHEAP(x)	(oldheap.buf + (x))
#define	OLDKOFF(x)	HEAP_U32LOAD(OLDHEAP(x))

	// safety net and for testing
	if (getenv("_BK_HEAP_NOREPACK")) return;

	/* read old metadata */
	meta = hash_new(HASH_MEMHASH);
	/* old generational number */
	hash_storeStr(meta, "GEN", "0");
	hash_free(s->heapmeta);
	s->heapmeta = meta;

	/* remember the tip when we last repacked */
	if (HASGRAPH(s)) {
		sccs_sdelta(s, sccs_top(s), buf);
		hash_storeStr(meta, "TIP", buf);
	}

	/*
	 * placeholders for the heap size and hash offset after the
	 * last repack
	 */
	sprintf(buf, "0x%08x", 0);
	hash_storeStr(meta, "LEN", buf);
	hash_storeStr(meta, "HASH", buf);
	if (CSET(s)) {
		sprintf(buf, "0x%08x/0x%08x", 0, 0);
		hash_storeStr(meta, "RKTYPES", buf);
	}
	sprintf(buf, "%02d", 0);
	hash_storeStr(meta, "HASHBITS", buf);

	/* version */
	hash_storeStr(meta, "VER", HEAP_VER);

	/* clean old data */
	unless (s->heap.len) data_append(&s->heap, "", 1);
	oldheap = s->heap;
	memset(&s->heap, 0, sizeof(s->heap));
	nokey_free(s->uniq1);
	s->uniq1 = 0;
	s->uniq1init = 1;	/* keep empty */
	s->uniq1off = 0;
	nokey_free(s->uniq2);
	s->uniq2 = nokey_newAlloc();
	s->uniq2deltas = 1;	/* don't reload */
	s->uniq2keys = 1;	/* don't reload */
	s->rkeyHead = 0;

	/* write header to heap */
	t = hash_toStr(meta);
	metalen = strlen(t);
	sccs_addStr(s, t);
	free(t);

	/* debug mode to test for signed integer overflow */
	if (CSET(s) && (t = getenv("_BK_HEAP_ADDJUNK"))) {
		off = strtoul(t, 0, 0);
		data_resize(&s->heap, s->heap.len + off);
		memset(s->heap.buf + s->heap.len, 0, off);
		s->heap.len += off;
	}

#define FIELD(x) \
	if (old = s->slist2[d].x) \
	    s->slist2[d].x = sccs_addStr(s, OLDHEAP(old))
#define UFIELD(x) \
	if (old = s->slist2[d].x) \
	    s->slist2[d].x = sccs_addUniqStr(s, OLDHEAP(old))

	/* put one paging block of deltas together (about 500) */
	for (d = TABLE(s); d >= TREE(s); d--) {
		if (s->heap.len > swapsz) break; // sz from dataheap.c
		FIELD(cludes);
		unless (CSET(s)) FIELD(bamhash);
		FIELD(random);
		UFIELD(userhost);
		UFIELD(pathname);
		UFIELD(sortPath);
		UFIELD(zone);
		UFIELD(symlink);
		UFIELD(csetFile);
		FIELD(comments);
	}
	ds = d;
	/* put the rest sorted by field (stuff for keysync first) */
	for (d = ds; d >= TREE(s); d--) FIELD(random);
	for (d = ds; d >= TREE(s); d--) UFIELD(pathname);
	for (d = ds; d >= TREE(s); d--) UFIELD(sortPath);
	for (d = ds; d >= TREE(s); d--) UFIELD(zone);
	for (d = ds; d >= TREE(s); d--) UFIELD(symlink);
	for (d = ds; d >= TREE(s); d--) UFIELD(csetFile);
	for (d = ds; d >= TREE(s); d--) UFIELD(userhost);
	for (d = ds; d >= TREE(s); d--) FIELD(cludes);
	unless (CSET(s)) {
		for (d = ds; d >= TREE(s); d--) FIELD(bamhash);
	}

	/* add symbols */
	EACH(s->symlist) {
		s->symlist[i].symname =
		    sccs_addUniqStr(s, OLDHEAP(s->symlist[i].symname));
	}

	/* then comments */
	for (d = ds; d >= TREE(s); d--) FIELD(comments);

#undef FIELD
#undef UFIELD

	/* finally the cset content */
	if (BWEAVE2(s)) {
		/*
		 * for BWEAVEv2 just dump the full weave in order
		 */
		for (d = TABLE(s); d >= TREE(s); d--) {
			char	**w = 0;

			unless (off = WEAVE_INDEX(s, d)) continue;
			while (rkoff = OLDKOFF(off)) {
				rkoff += 4;
				w = addLine(w, OLDHEAP(rkoff));
				w = addLine(w, (t = OLDHEAP(off + 4)));
				off += 4 + strlen(t) + 1;
			}
			weave_set(s, d, w);
			freeLines(w, 0);
		}
	} else if (CSET(s)) {
		hash	*rkh =
			    hash_new(HASH_U32HASH, sizeof(u32), sizeof(Pair));
		Pair	p = {0};
		hash	*tiphash;
		u32	*comprk = 0, *bamrk = 0;

		/*
		 * walk the weave oldest to newest to identify the
		 * oldest dkey for each rkey
		 */
		for (d = TREE(s); d <= TABLE(s); d++) {
			unless (off = WEAVE_INDEX(s, d)) continue;
			while (rkoff = OLDKOFF(off)) {
				if (((p.oldestdk = OLDKOFF(off+4)) != 0) &&
				    hash_insert(rkh, &rkoff, sizeof(rkoff),
					&p, sizeof(p))) {

					rkey = OLDHEAP(rkoff);
					if (changesetKey(rkey)) {
						addArray(&comprk, &rkoff);
					} else if (BAMkey(rkey)) {
						/* BAM file */
						addArray(&bamrk, &rkoff);
					}
				}
				off += 8;
			}
		}

		/*
		 * walk weave and save rootkeys to heap, buffer
		 * deltakeys and generate weave data.  The weave data
		 * will need to be renumbered before writing out.  The
		 * offsets to the weave in the delta table also need
		 * to get offset once we know where they start. (The
		 * +1 prevents us from using 0.)
		 */
		for (d = TABLE(s); d >= TREE(s); d--) {
			unless (off = WEAVE_INDEX(s, d)) continue;
			WEAVE_SET(s, d, 4*nLines(weave)+1);
			while (rkoff = OLDKOFF(off)) {
				dkoff = OLDKOFF(off+4);
				unless (dkoff) {/* skip old last key marker */
					off += 8;
					continue;
				}
				dkey = OLDHEAP(dkoff);
				pair = hash_fetch(rkh, &rkoff, sizeof(rkoff));
				assert(pair);
				unless (pair->newrk) {
					addArray(&rkeys, &rkoff);
					pair->newrk = nLines(rkeys);
					dkeys = &tipkeys;
				} else {
					/* mark dkoff in weave as 'rest' */
					dkeys = &s->heap;
				}
				assert(pair->newrk);
				/* build up the weave table */
				addArray(&weave, &pair->newrk);
				len = dkeys->len + 1;
				addArray(&weave, &len);
				data_append(dkeys, dkey, strlen(dkey)+1);
				if (dkoff == pair->oldestdk) {
					/* mark oldest delta */
					addArray(&weave, &pair->newrk);
					len = 0;
					addArray(&weave, &len);
				}
				off += 8;
				assert(off < oldheap.len);
			}
			addArray(&weave, &rkoff);	/* null terminate */
		}
		hash_free(rkh);

		/* write tip delta keys array to heap */
		tipkeys_start = s->heap.len;
		data_append(&s->heap, tipkeys.buf, tipkeys.len);
		free(tipkeys.buf);

		/* write rootkeys to heap */
		EACH(comprk) {
			rkey = OLDHEAP(comprk[i]);
			sccs_addUniqRootkey(s, rkey);
		}
		free(comprk);
		/* Save the last offset of component rootkeys */
		s->rktypeoff.comp = s->heap.len;

		/* Now put BAM rootkeys on heap and save that */
		EACH(bamrk) {
			rkey = OLDHEAP(bamrk[i]);
			sccs_addUniqRootkey(s, rkey);
		}
		free(bamrk);
		s->rktypeoff.bam = s->heap.len;
		sprintf(buf, "0x%08x/0x%08x",
		    s->rktypeoff.comp, s->rktypeoff.bam);
		hash_storeStr(meta, "RKTYPES", buf);

		/* now all remaining rootkeys (and update offsets) */
		EACH(rkeys) {
			rkey = OLDHEAP(rkeys[i]);
			rkeys[i] = sccs_addUniqRootkey(s, rkey);
		}

		/* update offset to weave */
		for (d = TABLE(s); d >= TREE(s); d--) {
			if (off = WEAVE_INDEX(s, d)) {
				WEAVE_SET(s, d, off + s->heap.len-1);
			}
		}

		/* update offsets in weave */
		tiphash = hash_new(HASH_U32HASH, sizeof(u32), sizeof(u32));
		EACH(weave) {
			unless (rkoff = weave[i]) {
				continue; /* ignore list term */
			}
			weave[i] = htole32(rkeys[rkoff]);
			unless (dkoff = weave[++i]) {
				continue; /* ignore last key marker */
			}
			dkoff -= 1; /* remove offset */
			if (hash_insertU32U32(tiphash, rkoff, 0)) {
				/* a tipkey so use second array */
				dkoff += tipkeys_start;
			}
			weave[i] = htole32(dkoff);
		}
		hash_free(tiphash);
		free(rkeys);

		/* write weave to heap */
		data_append(&s->heap, &weave[1], nLines(weave)*4);
		free(weave);
		weave = 0;
	}

	/*
	 * Now add a hash of unique data in the heap. This is a simple array
	 * looked up by the crc32c of the key. It is open addressing with a
	 * linear reprobe so we will make the array larger than the key space.
	 * XXX i could skip this section if the number of keys is small
	 *     it would save space in small files
	 */
	if (i = ((s->heap.len + 3) & ~3) - s->heap.len) {
		/* next 4-byte boundry, not needed but helps perf */
		memset(HEAP(s, s->heap.len), 0, i);
		s->heap.len += i;
	}
	uhash = s->heap.len;
	bits = nokey_log2size(s->uniq2);
	len = (1 << bits);
	size = len * 4;
	s->heap.len += size;
	data_resize(&s->heap, s->heap.len);
	memcpy(HEAP(s, uhash), nokey_data(s->uniq2), size);

	/* now update the header at the top of the heap */
	sprintf(buf, "0x%08x", uhash);
	hash_storeStr(meta, "HASH", buf);
	sprintf(buf, "%02d", bits);
	hash_storeStr(meta, "HASHBITS", buf);
	sprintf(buf, "0x%08x", s->heap.len);
	hash_storeStr(meta, "LEN", buf);
	t = hash_toStr(meta);
	assert(strlen(t) == metalen);
	assert(strneq(t, "GEN=", 4));
	strcpy(HEAP(s, 1), t);
	free(t);

	/* clear caches */
	s->uniq1init = 0;
	nokey_free(s->uniq2);
	s->uniq2 = 0;
	s->uniq2deltas = 0;
	s->uniq2keys = 0;

#undef	OLDHEAP
#undef	OLDKOFF

	/* now free the old data, have to unmap first */
	if (BWEAVE(s)) {
		for (i = 1; i <= 2; i++) {
			if (s->heapfh[i]) {
				// unprotect all memory, but leave blank
				dataunmap(s->heapfh[i], 0);
				fclose(s->heapfh[i]);
				s->heapfh[i] = 0;
			}
		}
	} else {
		if (s->pagefh) {
			// need to keep all non-heap sfile data
			dataunmap(s->pagefh, 1);
			fclose(s->pagefh);
			s->pagefh = 0;
		}
	}
	free(oldheap.buf);

	s->heap_loadsz = 0;	/* flag heap to be rewritten */
	unless (first_time) {
		T_PERF("pack %d -> %d [%s]",
		    oldheap.len, s->heap.len, s->sfile);
	}
}

/*
 * Adds the CRC/VZIP stdio layers like we use for BK sfiles.
 *
 * mode can be "r", "w" or "a".
 * size is the estimated file size and is only used for "w"
 *
 * the layers are stacked so a fclose() will free everything.
 *
 * Returns 0 on failure.
 */
FILE *
fdopen_bkfile(FILE *f, char *mode, u64 size, int chkxor)
{
	assert(mode[1] == 0);
	if (fpush(&f,
	   fopen_crc(f, (mode[0] == 'a') ? "r+" : mode, size, chkxor))) {
		perror(mode);
err:		fclose(f);
		return (0);
	}
	if (fpush(&f, fopen_vzip(f, mode))) {
		perror(mode);
		goto err;
	}
	return (f);
}

/*
 * Open a file on disk with the CRC/VZIP stdio layers like we use
 * for BK sfiles.
 *
 * mode can be "r", "w" or "a".
 * size is the estimated file size and is only used for "w"
 *
 * Returns 0 on failure.
 */
FILE *
fopen_bkfile(char *file, char *mode, u64 size, int chkxor)
{
	FILE	*f;

	unless (f = fopen(file, (mode[0] == 'a') ? "r+": mode)) return (0);
	return (fdopen_bkfile(f, mode, size, chkxor));
}
