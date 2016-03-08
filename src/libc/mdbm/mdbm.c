/*
 * mdbm - ndbm work-alike hashed database library based on sdbm which is
 * based on Per-Aake Larson's Dynamic Hashing algorithms.
 * BIT 18 (1978).
 *
 * sdbm Copyright (c) 1991 by Ozan S. Yigit
 *
 * Modifications to use memory mapped files to enable multiple concurrent
 * users:
 *	Copyright (c) 1996 by Larry McVoy, lm@sgi.com.
 *	Copyright (c) 1996 by John Schimmel, jes@sgi.com.
 *	Copyright (c) 1996 by Andrew Chang, awc@sgi.com.
 *
 * Ported to NT WIN32 environment
 *	Copyright (c) 1998 by Andrew Chang, awc@bitmover.com
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * This file is provided AS IS with no warranties of any kind.	The author
 * shall have no liability with respect to the infringement of copyrights,
 * trade secrets or any patents by this file or any part thereof.  In no
 * event will the author be liable for any lost revenue or profits or
 * other special, indirect and consequential damages.
 *
 *			oz@nexus.yorku.ca
 *
 *			Ozan S. Yigit
 *			Dept. of Computing and Communication Svcs.
 *			Steacie Science Library. T103
 *			York University
 *			4700 Keele Street, North York
 *			Ontario M3J 1P3
 *			Canada
 */
#ifdef	SGI
#ifdef	__STDC__
	#pragma weak mdbm_close = _mdbm_close
	#pragma weak mdbm_delete = _mdbm_delete
	#pragma weak mdbm_fetch = _mdbm_fetch
	#pragma weak mdbm_first = _mdbm_first
	#pragma weak mdbm_firstkey = _mdbm_firstkey
	#pragma weak mdbm_next = _mdbm_next
	#pragma weak mdbm_nextkey = _mdbm_nextkey
	#pragma weak mdbm_open = _mdbm_open
	#pragma weak mdbm_store = _mdbm_store
	#pragma weak mdbm_sethash = _mdbm_sethash
	#pragma weak mdbm_pre_split = _mdbm_pre_split
	#pragma weak mdbm_limit_size = _mdbm_limit_size
	#pragma weak mdbm_set_alignment = _mdbm_set_alignment
	#pragma weak mdbm_compress_tree = _mdbm_compress_tree
	#pragma weak mdbm_close_fd = _mdbm_close_fd
	#pragma weak mdbm_truncate = _mdbm_truncate
	#pragma weak mdbm_sync = _mdbm_sync
	#pragma weak mdbm_lock = _mdbm_lock
	#pragma weak mdbm_unlock = _mdbm_unlock
#endif
#endif	/* SGI */

#include "common.h"

static datum   nullitem;
static kvpair  nullkv;

static uint32	magic;
static datum	itemk, itemv;
static int	mflags = MAP_SHARED;
static void	addPageMapEntry(MDBM *db, mpage_t *pg_ent);
static void	freePageMapEntry(MDBM *db, unsigned int pageno);
static void 	resizePmap(MDBM *db, unsigned int new_size);
static ubig	getpage(MDBM *, ubig);
static kvpair	getnext(MDBM *);
static char	*makeroom(MDBM *, ubig, int);
static void	putpair(MDBM *, char *, datum, datum);
static int	_split(struct mdbm *, datum, datum, void *);
static int	_delete(struct mdbm *, datum, datum, void *);
static int	addpage(MDBM *db);
static int	seepair(MDBM *, char *, datum);
static int	see_last_pair(MDBM *, char *, datum);
static int	remap(MDBM *);
static void	split_page(MDBM *, char *, char *,
		    int (*func)(struct mdbm *, datum, datum, void *), void *);
static void	memdb_clean_up(MDBM *db);

MDBM	*
mdbm_open(char *file, int flags, int mode, int psize)
{
	MDBM	*db;
	struct	stat statbuf;
	debug((stderr, "mdbm_open(%s, 0%o, %o, %d)\n",
	    file, flags, mode, psize));

	assert(sizeof(MDBM_hdr) == 16); /* for cross platform DB portability */

	if (file && !*file) {
		return errno = EINVAL, (MDBM *)0;
	}
	if (!(db = new(MDBM))) {
		return errno = ENOMEM, (MDBM *)0;
	}
	db->m_kino = -1;  /* just in case */

	/*
	 * adjust user flags so that WRONLY becomes RDWR,
	 * as required by this package. Also set our internal
	 * flag for RDONLY if needed.
	 */
	if (flags & O_WRONLY) {
		flags = (flags & ~O_WRONLY) | O_RDWR;
	} else if ((flags & (O_RDWR|O_WRONLY|O_RDONLY)) == O_RDONLY) {
		db->m_flags = _MDBM_RDONLY;
	}

	if (file) {
		if ((stat(file, &statbuf) == -1) && (flags & O_CREAT)) {
			flags |= O_TRUNC;
		}
#ifdef WIN32
		flags |= O_BINARY|O_NOINHERIT;
#endif
		if ((db->m_fd = open(file, flags, mode)) == -1) {
			fprintf(stderr, "Cannot open ");
			perror(file);
			free(db);
			return (MDBM *)0;
		}
	} else {
		/* file == NULL => memory only db, force a create */
		db->m_flags = _MDBM_MEM;
		db->m_fd = -1;
		flags |= (O_RDWR|O_CREAT|O_TRUNC);
		db->memdb = hash_new(HASH_MEMHASH);
		return (db);
	}

	magic = _htonl(_MDBM_MAGIC);
	if ((flags & (O_CREAT|O_TRUNC)) == (O_CREAT|O_TRUNC)) {
		ubig	dbsize;
		char	*pg0; /* page zero */
		int	i;

		if (!psize) {
			psize = MDBM_PAGESIZ; /* go with default size */
		} else if (psize < MDBM_MINPAGE) {
			psize = MDBM_MINPAGE;
		} else if (psize > MDBM_MAXPAGE) {
			psize = MDBM_MAXPAGE;
		}
		for (i = MDBM_MIN_PSHIFT; 1<<i < psize; ++i);
		db->m_pshift = i;
		db->m_toffset = TOP_OFFSET(db);
		db->m_max_free = (uint32) (PAGE_SIZE(db) - INO_SIZE);
		dbsize = DATA_SIZE(0, db->m_pshift);
		if (_Mdbm_memdb(db)) { /* memory only db */
			db->pmapSize = 1;
			db->page_map = new(mpage_t*);
			db->page_map[0] = new(mpage_t);
			db->page_map[0]->page_addr = ALLOC_PAGE(db);
			db->m_db = TOP(db, db->page_map[0]->page_addr);
		} else {
			if (ftruncate(db->m_fd, (off_t) dbsize) == -1 ||
			    (db->m_db = (char *) mmap(0, (size_t) dbsize, PROT,
			    mflags, db->m_fd, 0)) == (char *) MINUS_ONE) {
				(void)close(db->m_fd);
				free((char *) db);
				perror("cannot ftruncate/mmap");
				return (MDBM *) NULL;
			}
		}

		/*
		 * hand craft 1st 2 entries in page 0,
		 * need two INO for key/data pair,
		 * key length of 0 means this is metadata,
		 * data portion has both header & directory.
		 */
		pg0 = PAG_ADDR(db, 0);
		INO(pg0, 0) = hton_idx(2);
		INO(pg0, 1) = 0;
		INO(pg0, 2) = hton_idx(sizeof(MDBM_hdr) + MIN_DSIZE);
		bzero(DB_HDR(db), sizeof(MDBM_hdr) + MIN_DSIZE);
		db->m_dir = TOP(db, pg0) + MIN_DIR_OFFSET(db);
		DB_HDR(db)->mh_magic = magic;
		DB_HDR(db)->mh_max_npgshift = (uchar) 0xff;
		DB_HDR(db)->mh_pshift = db->m_pshift;
		DB_HDR(db)->mh_minpg_level = -1;
		DB_HDR(db)->mh_hashfunc = 0;
		db->m_hash = mdbm_hash0;
		init_lock(((abilock_t *)&(DB_HDR(db)->mh_lockspace)));
		debug((stderr,
		    "after open: db=%x for %d dir=%x dsize=%d psize=%d\n",
		    db->m_db, dbsize, db->m_dir,
		    DIR_SIZE(db->m_npgshift), 1<<db->m_pshift));
		return (db);
	} else if (remap(db)) {
		/*
		** Set the default functions.
		*/
		switch (DB_HDR(db)->mh_hashfunc) {
		    case 0:
			db->m_hash = mdbm_hash0;
			break;
		    case 1:
			db->m_hash = mdbm_hash1;
			break;
		    case 2:
			db->m_hash = mdbm_hash2;
			break;
		    default:
			fprintf(stderr, "unsupported hash function code %d\n",
			    DB_HDR(db)->mh_hashfunc);
			return (MDBM *) NULL;
		}

		return (db);
	}
	(void)close(db->m_fd);
	free((char *) db);
	return (MDBM *) NULL;
}

void
mdbm_close(MDBM *db)
{
	debug((stderr, "mdbm_close\n"));
	if (db == NULL) {
		debug((stderr, "mdbm_close: db	is NULL!!\n"));
		errno = EINVAL;
	} else {
		if (_Mdbm_memdb(db)) memdb_clean_up(db);
		if (db->m_db) {
			if (!_Mdbm_memdb(db)) {
				mdbm_sync(db);
				(void) munmap(db->m_db,
				    DB_SIZE(db->m_npgshift, db->m_pshift));
			}
		}
		if (db->m_fd >= 0) {
			debug((stderr,
			    "mdbm_close: closing fd =%d\n", db->m_fd));
			(void) close(db->m_fd);
		}
		free((char *) db);
	}
}

#if	0
void
mdbm_dump(MDBM *db)
{
	kvpair	kv;
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db))
		printf("%s:%s\n", pk(kv.key), pv(kv.val));
}
#endif

datum
mdbm_fetch(MDBM *db, datum key)
{
	ubig	hash;
	char	*page;
	idx_t	koff, doff;
	register int	i;

	if (db == NULL || bad(key))
		return errno = EINVAL, nullitem;

	if (_Mdbm_memdb(db)) {
		datum	ret;

		ret.dsize = 0;
		ret.dptr = hash_fetch(db->memdb, key.dptr, key.dsize);
		ret.dsize = db->memdb->vlen;
		return (ret);
	}
	if (!_Mdbm_memdb(db) && SIZE_CHANGED(db) && !remap(db))
		return nullitem;
	/*
	 * Hash to a page.
	 */
	hash = _Exhash(db, key);
	page = PAG_ADDR(db, GETPAGE(db, hash));

	if (!INO(page, 0)) return nullitem;

	/* code to match performance of single entry cache in other package */
	if ((hash == db->m_last_hash) && (NUM(page) >= db->m_kino)) {
		koff = ((i = db->m_kino) == -1)? 0 : ntoh_idx(INO(page, i+1));
		XTRACT(db, itemk, i, page, koff, doff);
		if ((key.dsize == itemk.dsize) &&
		    !memcmp(key.dptr, itemk.dptr, itemk.dsize))
			goto found;
	}

	/*
	 * Look through page for given key.
	 */
	if (INO(page, 0) && (i = seepair(db, page, key))) {
		db->m_last_hash = hash;
		db->m_kino = i;

found:
		doff = ntoh_idx(INO(page, i));
		doff = ALGNED(db, doff);
		XTRACT(db, itemv, i+1, page, doff, koff);
		return itemv;
	} else return nullitem;
}

static int
_write_access_check(MDBM *db)
{
	if (db == NULL)
		return errno = EINVAL, 1;
	if (_Mdbm_rdonly(db))
		return errno = EPERM, 1;
	if (!_Mdbm_memdb(db) && SIZE_CHANGED(db) && !remap(db))
		return 1;
	return 0;
}

int
mdbm_delete(MDBM *db, datum key)
{
	char	*page;

	if bad(key)
		return errno = EINVAL, -1;
	if (_Mdbm_memdb(db)) {
		hash_delete(db->memdb, key.dptr, key.dsize);
		return (0);
	}
	if  (_write_access_check(db))
		return -1;

	page = PAG_ADDR(db, GETPAGE(db, _Exhash(db, key)));
	if (INO(page, 0) && seepair(db, page, key)) {
		DELPAIR(db, page, key);
		db->m_last_hash = 0;
	}
#ifdef	LATER
	if (_Mdbm_memdb(db)) {
		/* XXX TODO: if resulting page has zero entry */
		/* free the page & its page map entry */
	}
#endif
	return 0;
}

int
mdbm_store(MDBM *db, datum key, datum val, int flags)
{
	register uint32 need;
	register char	*page;
	ubig	hash;
	char	*dbend;

	if bad(key)
		return errno = EINVAL, -1;

	if (_Mdbm_memdb(db)) {
		void	*m;

		if (flags == MDBM_INSERT) {
			unless (m = hash_insert(db->memdb,
				  key.dptr, key.dsize, val.dptr, val.dsize)) {
				return errno = EEXIST, 1;
			}
		} else {
			hash_store(db->memdb,
			    key.dptr, key.dsize, val.dptr, val.dsize);
		}
		return (0);
	}
	if  (_write_access_check(db))
		return -1;

	/*
	 * Existing MDBM data cannot be used to create new entries.
	 * The store may cause the data to be moved around and
	 * invalidate the pointers.
	 */
	dbend = db->m_db + DB_SIZE(db->m_npgshift, db->m_pshift);
	assert((key.dptr < db->m_db) || (key.dptr > dbend));
	assert((val.dptr < db->m_db) || (val.dptr > dbend));

	need = (uint32)(key.dsize + val.dsize +
	    (2 * INO_SIZE) + _Mdbm_alnmask(db));

	hash = _Exhash(db, key);
	page = PAG_ADDR(db, GETPAGE(db, hash));

	/*
	 * if we need to replace, delete the key/data pair first
	 */
	if (INO(page, 0) &&
	    (SEE_LAST_PAIR(db, hash, page, key) || seepair(db, page, key))) {
		if (flags == MDBM_INSERT)
			return errno = EEXIST, 1;
		DELPAIR(db, page, key);
	}
	/*
	 * if we do not have enough room, we have to split.
	 */
	if (need  > FREE_AREA(db, page)) {
		if (need > MAX_FREE(db)) {
			error((stderr, "mdbm_store: too big\n"));
			return errno = EINVAL, -1;
		}
		if ((page = makeroom(db, hash, need)) == (char *) MINUS_ONE) {
			error((stderr, "mdbm_store: no room\n"));
			return ioerr(db), -1;
		}

	}

#ifdef	DEBUG
	if (NUM(page) >= (PAGE_SIZE(db) / INO_SIZE)) {
		    fprintf(stderr, "Bad page %lx (page num = %d) , NUM=%d\n",
			page, PNO(db, page), NUM(page));
				    exit(1);
	}
#endif
	/*
	 * we have enough room or split is successful. insert the key.
	 */
	(void) putpair(db, page, key, val);
	return 0;
}

/*
 * makeroom - make room by splitting the overfull page
 * this routine will attempt to make room for _SPLTMAX times before
 * giving up.
 * returns the page for the key or (char*)-1
 */
static char*
makeroom(MDBM *db, ubig hash, int need)
{
	ubig	npageno,  dbit = 0, hbit = 0, urgency;
	char	*page, *npage, *dest;
	int	smax = _SPLTMAX;

	if (_Mdbm_perfect(db)) {
		register ubig max_pgno;

		/* reset the _MDBM_PERFECT flag */
		DB_HDR(db)->mh_dbflags &= ~_MDBM_PERFECT;
		max_pgno = MAX_NPG(db)-1;
		while (dbit < max_pgno)
			dbit = 2 * dbit + ((hash & (1 << hbit++)) ? 2 : 1);
	} else {
		dbit = db->m_curbit;
		hbit = db->m_level;
	}

	do {
		/* page number of the new page */
		npageno = (hash & MASK(hbit)) | (PLUS_ONE<<hbit);
		if (npageno  >=	 MAX_NPG(db)) {
			if (db->m_npgshift >= DB_HDR(db)->mh_max_npgshift)
				goto do_shake;
			if (!addpage(db))
				return ((char*)MINUS_ONE);
		}
		page = PAG_ADDR(db, (hash & MASK(hbit)));
		npage = PAG_ADDR(db, npageno);
		INO(npage, 0) = 0;

		/* split the current page */
		split_page(db, page, npage, _split, int2p(hbit));
		/* ASSERT(dbit < (PLUS_ONE<<DSHIFT(db->npgshift)) */
		SETDBIT(db->m_dir, dbit);
		dest = ((hash>>hbit) & PLUS_ONE) ? npage : page;
		if (FREE_AREA(db, dest) >= (unsigned int) need)
			return dest;
		dbit = 2 * dbit + ((hash & (1<<hbit++)) ? 2 : 1);
	} while (--smax);
	/*
	 * if we are here, this is real bad news. After _SPLTMAX splits,
	 * we still cannot fit the key. say goodnight.
	 */
#ifdef	BADMESS
	fprintf(stderr, "mdbm: cannot insert after _SPLTMAX attempts.\n");
#endif
	return (char*)MINUS_ONE;

do_shake:
	page = PAG_ADDR(db, (hash & MASK(hbit)));
	for (urgency = 0; urgency < 3; urgency++)	 {
		split_page(db, page, 0, db->m_shake, int2p(urgency));
		if (FREE_AREA(db, page) >= (unsigned int) need)
			return page;
	}
	return (char*)MINUS_ONE;
}

/*
 * the following two routines will break if
 * deletions aren't taken into account. (ndbm bug)
 */
kvpair
mdbm_first(MDBM *db)
{
	debug((stderr, "mdbm_first\n"));
	if (db == NULL)
		return errno = EINVAL, nullkv;

	if (_Mdbm_memdb(db)) {
		kvpair	kv;

		kv.key.dptr = hash_first(db->memdb);
		kv.key.dsize = db->memdb->klen;
		kv.val.dptr = db->memdb->vptr;
		kv.val.dsize = db->memdb->vlen;
		return (kv);
	}

	/*
	 * start at page 0
	 */
	db->m_pageno = 0;
	db->m_next = 1;

	return getnext(db);
}

datum
mdbm_firstkey(MDBM *db)
{
	kvpair kv;

	kv = mdbm_first(db);
	return kv.key;
}

kvpair
mdbm_next(MDBM *db)
{
	if ((db == NULL))
		return errno = EINVAL, nullkv;

	if (_Mdbm_memdb(db)) {
		kvpair	kv;

		kv.key.dptr = hash_next(db->memdb);
		kv.key.dsize = db->memdb->klen;
		kv.val.dptr = db->memdb->vptr;
		kv.val.dsize = db->memdb->vlen;
		return (kv);
	}
	debug((stderr, "mdbm_next page=%d next=%d kino=%d\n",
	    (int)db->m_pageno, db->m_next, db->m_kino));

	if (!_Mdbm_memdb(db) && SIZE_CHANGED(db)) {
		if (!remap(db))
			return nullkv;
		/* a file changing size in the middle is a reset. */
		db->m_pageno = 0;
		db->m_next = 1;
	}

	return getnext(db);
}

datum
mdbm_nextkey(MDBM *db)
{
	kvpair kv;

	kv = mdbm_next(db);
	debug((stderr, "next returns: dsize - %d, val - %s\n",
	    kv.key.dsize, kv.key.dptr));
	return kv.key;
}

static int
_remap(MDBM *db, ubig size)
{

	if (db->m_fd < 0) return (0);

	if (db->m_db) munmap(db->m_db, DB_SIZE(db->m_npgshift, db->m_pshift));
	if ((db->m_db = mmap(0, (size_t) size, PROT, mflags, db->m_fd, 0)) ==
	    (char *) MINUS_ONE) {
		error((stderr, "Can't remap database: %s\n", strerror(errno)));
		return (0);
	}
	if (DB_HDR(db)->mh_magic != magic) {
		errno = EBADF;
		error((stderr, "bad header\n"));
		return (0);
	}
	db->m_toffset = (1<<db->m_pshift) - INO_SIZE;

	/* caller must update m_dir, m_npgshift & m_pshift themself */
	return (1);
}

/*
 * Remap the database - called at open and when size has changed.
 * This needs to work for both readonly & read/write databases.
 * Note: this is always called to grow the mappings, never to shrink them.
 */
static int
remap(MDBM *db)
{
	struct	stat	sbuf;
	ubig	size;

	if (db->m_fd < 0) {
		return (0);
	}

#define	FSIZE(fd) ((fstat((fd), &sbuf) == -1) ? 0 : ((ubig)sbuf.st_size))

	size = FSIZE(db->m_fd);

	if (size < MDBM_MINPAGE) {
		error((stderr, "bad size %u\n", (uint32)size));
		errno = EINVAL;
		return (0);
	}

	if (!_remap(db, size)) return 0;
#ifdef	OLD
	if (DB_HDR(db)->mh_magic != magic) {
		errno = EBADF;
		error((stderr, "bad header\n"));
		return (0);
	}
#endif

	db->m_npgshift = DB_HDR(db)->mh_npgshift;
	db->m_pshift = DB_HDR(db)->mh_pshift;
	db->m_toffset = (1<<db->m_pshift) - INO_SIZE;
	db->m_amask = (DB_HDR(db)->mh_dbflags & 0x07);
	db->m_max_free = (uint32) (PAGE_SIZE(db) - INO_SIZE);

	db->m_dir = GET_DIR(db, db->m_npgshift, db->m_pshift);
#if	0
	debug((stderr, "remapped DB size=%s dsize=%s db=%x dir=%x\n",
	    pxx(size), pxx(DIR_SIZE(db->m_npgshift)), db->m_db, db->m_dir));
#endif
	return (1);
}

/*
 * Add space for a page to the database.
 * Make sure that the directory can address the new page; grow if it can't.
 * Only called for writeable databases.
 * interesting problem - what to do when I/O error?
 */
static int
addpage(MDBM *db)
{
	ubig	new_dbsize;
	char	*newdir;
	uchar	new_npgshift;

	/*
	 * Have to resize, remap, and copy the directory.
	 * Bump up the data & dir sizes by step.
	 */
	new_npgshift = db->m_npgshift + 1; /* double # of page */
	new_dbsize = DB_SIZE(new_npgshift, db->m_pshift);

	if (!_Mdbm_memdb(db) && RE_SIZE(db, new_dbsize)) {
#if	1
		error((stderr, "Can't remap db up to %s: %s\n",
		    pxx((ubig)new_dbsize), strerror(errno)));
#endif
		ioerr(db);
		return (0);
	}
	if (DB_HDR(db)->mh_magic != magic) {
		ioerr(db);
		return (0);
	}

	/* Just in case db->m_dir changed after RE_SIZE() */
	db->m_dir = GET_DIR(db, db->m_npgshift, db->m_pshift);
	if (new_npgshift > (MIN_DSHIFT - DSHIFT_PER_PAGE)) {
		if (_Mdbm_memdb(db)) {
			newdir = malloc(DIR_SIZE(new_npgshift));
		} else {
			newdir =
			    db->m_db + DATA_SIZE(new_npgshift, db->m_pshift);
		}
		debug((stderr, "DIR COPY %s\n", pxx(DIR_SIZE(db->m_npgshift))));
		/* should realy take 64 bit argument, but ok for now */
		memmove(newdir, db->m_dir, (size_t) DIR_SIZE(db->m_npgshift));
		/* init the new portion of directory bit array to zero */
		bzero(newdir+DIR_SIZE(db->m_npgshift),
		    DIR_SIZE(new_npgshift) - DIR_SIZE(db->m_npgshift));
		if (_Mdbm_memdb(db)) {
			free(db->mem_dir);
			db->mem_dir = newdir;
		}
		db->m_dir = newdir;
	}
	DB_HDR(db)->mh_npgshift = db->m_npgshift = new_npgshift;
	return (1);
}

/*
 * All important binary tree traversal.
 */
static ubig
getpage(MDBM *db, ubig hash)
{
	register char *dir;
	register ubig dbit = 0, h, max_dbit;
	register int	hbit = 0;

	h = hash;

#define	BYPASS_TREE_WALK_IF_POSSIBLE
#ifdef	BYPASS_TREE_WALK_IF_POSSIBLE
	if (DB_HDR(db)->mh_minpg_level == 0)
		goto done;

	if (DB_HDR(db)->mh_minpg_level >= 1) {
		max_dbit = MASK(DB_HDR(db)->mh_minpg_level) - 1;
		while (dbit < max_dbit)
			dbit = (dbit << 1) + ((h & (1 << hbit++)) ? 2 : 1);
	}
#endif
	if (MAX_NPG(db) == 1)
		goto done;
	max_dbit = MAX_NPG(db) - 1;
	dir = db->m_dir;
	while (dbit < max_dbit && GETDBIT(dir, dbit))
		dbit = (dbit << 1) + ((h & (1 << hbit++)) ? 2 : 1);

done:
	db->m_curbit = dbit;
	db->m_level = hbit;
	return (h & ((1 << hbit) - 1));
}

/*
 * getnext - get the next key in the page, and if done with
 * the page, try the next page in sequence
 */
static	kvpair
getnext(MDBM *db)
{
	int n;
	idx_t	koff, doff;
	char	*page;
	kvpair kv;
	int i;

	while (db->m_pageno <= (MAX_NPG(db) - 1)) {
		/* XXX the following line could be optimized further */
		if (db->m_pageno != GETPAGE(db, db->m_pageno)) {
			db->m_pageno++;
			continue; /* page not allocated , skip */
		}
		page = PAG_ADDR(db, db->m_pageno);
		n = ntoh_idx(INO(page, 0));
		debug((stderr, "getnext page=%u max=%u n=%u next=%u\n",
		    (uint32)db->m_pageno, MAX_NPG(db)-1, n, db->m_next));
next_entry:
		if (n > db->m_next) {
			i = db->m_next;
			koff = (i == 1) ? 0 : ntoh_idx(INO(page, i-1));
			XTRACT(db, itemk, i, page, koff, doff);
			if (!itemk.dsize) {
				db->m_next += 2;
				goto next_entry; /* ignore header */
			}
			kv.key = itemk;
			doff = ntoh_idx(INO(page, i));
			doff = ALGNED(db, doff);
			XTRACT(db, itemv, i+1, page, doff, koff);
			db->m_next += 2;
			kv.val = itemv;
			return (kv);
		}

		/*
		 * we either run out, or there is nothing on this page..
		 * try the next one...
		 */
		(db->m_pageno)++;
		db->m_next = 1;
	}
	return nullkv;
}

/*
 * page-level routines
 */
static void
putpair(MDBM *db, char *pag, datum key, datum val)
{
	register idx_t	 n, off;
	register char	*t;

	n = NUM(pag);
	off = OFF(pag); /* enter the key first */
	t = TOP(db, pag);
	(void) memmove(t + off, key.dptr, key.dsize);
	off += key.dsize;
	INO(pag, n + 1) = hton_idx(off);
	off = ALGNED(db, off);
	(void) memmove(t + off, val.dptr, val.dsize);
	off += val.dsize;
	INO(pag, n + 2) = hton_idx(off);
	INO(pag, 0) = hton_idx((idx_t) (n + 2)); /* adjust item count */
}

static int
see_last_pair(MDBM *db, char *pag, datum key)
{
	idx_t koff, doff;
	int i = db->m_kino;

	koff = (i == -1)? 0 : ntoh_idx(INO(pag, i-1));
	XTRACT(db, itemk, i, pag, koff, doff);
	if ((key.dsize == itemk.dsize) &&
	    !memcmp(key.dptr, itemk.dptr, itemk.dsize)) {
		return i;
	} else {
		return 0;
	}
}
/*
 * search for the key in the page.
 * return offset index in the range 0 < i < n.
 * return 0 if not found.
 */
static int
seepair(MDBM *db, char *pag, datum key)
{
	register idx_t	 i, n;
	idx_t	 koff = 0, doff;

	n = ntoh_idx(INO(pag, 0));

	for (i = 1; i < n; i += 2) {
		XTRACT(db, itemk, i, pag, koff, doff);
		if ((key.dsize == itemk.dsize) &&
		    !memcmp(itemk.dptr, key.dptr, itemk.dsize)) {
			return i;
		}
		koff = ntoh_idx(INO(pag, i+1));
	}
	return 0;
}

/* ARGSUSED */
static int
_split(struct mdbm  *db, datum key, datum val, void *param)
{
	return ((_Exhash(db, key) >> p2int(param)) & PLUS_ONE);
}

/* ARGSUSED */
static int
_delete(struct mdbm  *db, datum key, datum val, void *param)
{
		/* this will break if sizeof(int) < sizeof(pointer) */
#define	TARGET ((datum *) param)
	if ((TARGET->dsize == key.dsize) &&
	    !memcmp(TARGET->dptr, key.dptr, key.dsize)) {
		TARGET->dsize = -1; /* so we return 0 on later call */
		return 1;
	} else {
		return 0;
	}
}

/*
 * Take all the items in pag and split them between pag & new.
 * If "new" is null, then we just "shake/delete" out "unwanted" entry
 * This is also used as a "merge" function for mdbm_compress_tree()
 */
static void
split_page(MDBM *db, char *pag,
	char *new, int (*move_it)(MDBM *, datum, datum, void *), void *param)
{
#define	NO_HOLE MAX_IDX /* MAX_IDX => no hole/slot exist yet */ 
	register idx_t	i, n;
	idx_t	s;
	idx_t	doff, koff, fh = NO_HOLE; /* location of first hole */

	n = ntoh_idx(INO(pag, 0)); /* caller must insure n != 0 */
	s = (pag == PAG_ADDR(db, 0))? 3 : 1; /* ignore header in page 0 */
	koff = (s == 1) ? 0 : ntoh_idx(INO(pag, 2));

	for (i = s; i < n; i += 2) {
		XTRACT(db, itemk, i, pag, koff, doff);
		doff = ALGNED(db, doff);
		XTRACT(db, itemv, i+1, pag, doff, koff);
		if ((*move_it)(db, itemk, itemv, param)) {
			if (new) putpair(db, new, itemk, itemv);
			if (fh == NO_HOLE) {
				fh = INO(pag, 0) =
				    hton_idx((idx_t)  (i-1));
			}
		} else if (fh != NO_HOLE) {
			putpair(db, pag, itemk, itemv);
		}
	}
/*
	debug((stderr, "split([%d] -> [%d]%u [%d]%u)\n",
	    n, ntoh_idx(*((idx_t*)pag)), PNO(db, pag),
	    ntoh_idx(*((idx_t *)new)), PNO(new)));
*/
}

int
mdbm_pre_split(MDBM *db, ubig n)
{
	register int i;
	ubig dbsize, m, q;
	ubig pageno;

	if (db->memdb) return (0); /* skip for memory dbs */

	/* if used, this function must be called immediately */
	/* after dbm_open() */
	for (i = 0, m = 1; m < n; i++, m <<= 1);
	if (i > DB_HDR(db)->mh_max_npgshift)
		i = DB_HDR(db)->mh_max_npgshift;
	dbsize = DB_SIZE(i, db->m_pshift);
	if (!_Mdbm_memdb(db)) {
		/* extend the file and remap */
		if (RE_SIZE(db, dbsize)) {
			ioerr(db);
			return (-1);
		}
		for (pageno = 1; pageno < m; pageno++) {
			/* zero out the entry count */
			INO(PAG_ADDR(db, pageno), 0) = 0;
		}
	}


	/* update dir[] to pre-split the tree */
	DB_HDR(db)->mh_minpg_level =
	    DB_HDR(db)->mh_npgshift = db->m_npgshift = i;
	/* tag this as a "perfectly balanced tree" */
	DB_HDR(db)->mh_dbflags |= _MDBM_PERFECT;

	if (_Mdbm_memdb(db)) db->mem_dir = malloc(DIR_SIZE(i));
	db->m_dir = GET_DIR(db, i, db->m_pshift);
	q  = --m / BITS_PER_BYTE;
	i  = m % BITS_PER_BYTE;
	if (q > 0) memset(db->m_dir, 0xff, q);
	if (i > 0) db->m_dir[q] = 0; /* zero out unused bits */
	while (i > 0)
		db->m_dir[q] |= (1 << --i);
	if (_Mdbm_memdb(db)) {
		/* now we resize the page map */
		/* be generous, alloc one page map hash bucket per page */
		resizePmap(db, MAX_NPG(db));
	}

	return (0);
}

int
mdbm_limit_size(MDBM *db, ubig max_page,
	int (*shake)(MDBM *, datum, datum, void *))
{
	uchar i;

	if (_Mdbm_rdonly(db)) {
		error((stderr, "can not limit size of read only db\n"));
		return errno = EINVAL, -1;
	}
	if (!shake) {
		error((stderr,
		    "mdbm_limit_size: must supply shake function\n"));
		return errno = EINVAL, -1;
	}
	db->m_shake = shake;
	for (i = 0; (PLUS_ONE<<i) < max_page; ++i);
	if (db->m_npgshift > i) {
		error((stderr, "%s %s\n",
		    "mdbm_limit_size: database is already ",
		    "bigger then requested size limit\n"));
		return errno = EINVAL, -1;
	}
	DB_HDR(db)->mh_max_npgshift = (uchar) i;
	return (0);
}

int
mdbm_set_alignment(MDBM *db, int amask) {
	if (_Mdbm_rdonly(db)) {
		error((stderr, "can not set alignment of read only db\n"));
		return errno = EINVAL, -1;
	}
	if (db->memdb) return (0);
	db->m_amask = (amask & 0x07);
	DB_HDR(db)->mh_dbflags |= (amask & 0x07);
	return 0;
}


/* ARGSUSED */
static int
_merge(struct mdbm * db, datum key, datum val, void *param)
{ return 1; }

#define	MERGE_PAGE(db, left, right)	split_page(db, right, left, _merge, 0)


static int
is_directory(MDBM *db, ubig dbit)
{
	if (dbit < (MAX_NPG(db) - 1) && GETDBIT(db->m_dir, dbit))
		return 1;
	else	return 0;
}

#if	1
static uchar
find_min_level(MDBM *db, ubig dbit, int level)
{
	uchar min_level, r_level;
#define	LEFT_CHLD  ((dbit<<1)+1)
#define	RIGHT_CHLD ((dbit<<1)+2)

	if (!is_directory(db, LEFT_CHLD) || !is_directory(db, RIGHT_CHLD))
		return	level +1;

	min_level = find_min_level(db, LEFT_CHLD, level+1);
	r_level	  = find_min_level(db, RIGHT_CHLD, level+1);
	if (r_level < min_level) min_level = r_level;
	return min_level;
}

uchar
minimum_level(MDBM *db)
{
	if (!is_directory(db, 0))
		return 0;
	return find_min_level(db, ZERO, 0);
}
#endif

/*
 *  recursive tree merge function
 */
static uchar
mergeAllBranches(MDBM *db, ubig dbit, ubig pageno, int level)
{
	uchar max_level;
#define	LEFT_CHLD  ((dbit<<1)+1)
#define	RIGHT_CHLD ((dbit<<1)+2)

	max_level = level + 1;
	if (is_directory(db, LEFT_CHLD))
		max_level = mergeAllBranches(db, LEFT_CHLD, pageno, level+1);
	if (is_directory(db, RIGHT_CHLD)) {
		uchar r_level;
		r_level = mergeAllBranches(db,
		    RIGHT_CHLD, (pageno | (PLUS_ONE<<level)), level+1);
		if (r_level > max_level) max_level = r_level;
	}
	if (!is_directory(db, LEFT_CHLD) && !is_directory(db, RIGHT_CHLD)) {
		char	*lpag = PAG_ADDR(db, pageno);
		char	*rpag = PAG_ADDR(db, (pageno | (PLUS_ONE<<level)));
		/* check if entries can fix on one page */
		if ((ALGNED(db, OFF(lpag)) + OFF(rpag) +
		    ((NUM(lpag) + NUM(rpag) + 1) * INO_SIZE)) >
		    (unsigned int) PAGE_SIZE(db)) {
			goto done;
		}
		/* merge rpag into lpag */
		if (NUM(rpag))
			MERGE_PAGE(db, lpag, rpag);

		if (_Mdbm_memdb(db)) {
			/* XXX TODO:
			 * re-size page map hash table after tree compression
			 */
			freePageMapEntry(db, pageno | (PLUS_ONE<<level));
			FREE_PAGE(db, rpag);
		}

		UNSETDBIT(db->m_dir, dbit);
		DB_HDR(db)->mh_dbflags &= ~_MDBM_PERFECT;

		return level;
	}
done:
	return max_level;
}

void
mdbm_compress_tree(MDBM *db)
{
	uchar new_npgshift;
	ubig new_dbsize;

	if (!is_directory(db, 0)) return;

	new_npgshift = mergeAllBranches(db, 0, 0, 0);
	/* done the recursive tree merge, now fix dir[] & mmap size */
	if (new_npgshift < db->m_npgshift) {
		new_dbsize = DB_SIZE(new_npgshift, db->m_pshift);
		if (db->m_npgshift > (MIN_DSHIFT - DSHIFT_PER_PAGE)){
			memmove(GET_DIR(db, new_npgshift, db->m_pshift),
			    db->m_dir, (size_t) DIR_SIZE(new_npgshift));
			/* XXX TODO:
			 * need to zero out unused part of directory array
			 */
		}

		if (!_Mdbm_memdb(db)) {
			/* munmap() before we truncate */
			munmap(db->m_db, DB_SIZE(db->m_npgshift, db->m_pshift));
			if (RE_SIZE(db, new_dbsize)) {
				ioerr(db);
				return;
			}
		}

		DB_HDR(db)->mh_npgshift = db->m_npgshift = new_npgshift;
		db->m_dir = GET_DIR(db, db->m_npgshift, db->m_pshift);
	}
	DB_HDR(db)->mh_minpg_level =  minimum_level(db);
	/* min level == max level => perfect tree */
	if (db->m_npgshift == DB_HDR(db)->mh_minpg_level)
		DB_HDR(db)->mh_dbflags |= _MDBM_PERFECT;
}

// XXX - shouldn't we take a flag to say sync/async?
void
mdbm_sync(MDBM *db)
{
	debug((stderr, "mdbm_sync\n"));
	if (db == NULL) {
		errno = EINVAL;
	} else {
		if (db->m_db) {
			if (!_Mdbm_rdonly(db) && !_Mdbm_memdb(db)) {
				msync(db->m_db, (size_t) DB_SIZE(db->m_npgshift,
				    db->m_pshift), MS_ASYNC);
			}
		}
	}
}

void
mdbm_close_fd(MDBM *db)
{
#ifdef	linux
	errno = EINVAL;
#else
	if (db->m_fd >= 0) {
		close(db->m_fd);
		db->m_fd = -1;
	}
#endif
}

void
mdbm_truncate(MDBM *db)
{
	ubig	dbsize, shift;
	char	*pg0; /* page zero */
	int	i, rc;

	if (db->m_db) {
		if (db->m_fd >= 0) {
			rc = ftruncate(db->m_fd, sizeof(MDBM_hdr));
			assert(!rc);

			dbsize = DATA_SIZE(0, db->m_pshift);
			rc = ftruncate(db->m_fd, (off_t) dbsize);
			assert(!rc);

			db->m_toffset = (1<<db->m_pshift) - 2;
			db->m_max_free = (uint32) (PAGE_SIZE(db) - INO_SIZE);

			/*
			 * Hand craft 1st two entries.
			 * Reserve two entries for header,
			 * null key for header.
			 */
			pg0 = PAG_ADDR(db, 0);
			INO(pg0, 0) = _htons(2);
			INO(pg0, 1) = 0;
			INO(pg0, 2) = _htons(sizeof(MDBM_hdr) + MIN_DSIZE);

			db->m_dir = TOP(db, pg0) + MIN_DIR_OFFSET(db);

			DB_HDR(db)->mh_pshift = db->m_pshift;
			DB_HDR(db)->mh_minpg_level = -1;

			if (DB_HDR(db)->mh_dbflags & _MDBM_PERFECT) {
				for (i = 0, shift = DB_HDR(db)->mh_max_npgshift;
				    shift > 0; i++, shift <<= 1);
				mdbm_pre_split(db, i);
			}
		}
	}
}

int
mdbm_lock(MDBM *db)
{
	debug((stderr, "mdbm_lock\n"));
	if (! (db->m_flags & _MDBM_RDONLY)) {
		spin_lock((abilock_t *)&(DB_HDR(db)->mh_lockspace));
	}
	return 1;
}

int
mdbm_unlock(MDBM *db)
{
	debug((stderr, "mdbm_unlock\n"));
	if (! (db->m_flags & _MDBM_RDONLY)) {
		release_lock((abilock_t *)&(DB_HDR(db)->mh_lockspace));
	}
	return 1;
}

int
mdbm_sethash(MDBM *db, int number)
{
	switch (number) {
	    case 0:
		db->m_hash = mdbm_hash0;
		break;
	    case 1:
		db->m_hash = mdbm_hash1;
		break;
	    case 2:
		db->m_hash = mdbm_hash2;
		break;
	    default:
		return 0;
	}
	if (DB_HDR(db)) DB_HDR(db)->mh_hashfunc = number;
	return 1;
}


static void addPageMapEntry(MDBM *db, mpage_t *pg_ent)
{
	mpage_t *s;
	int depth;

	/* caller is responsible to zero out pg_ent->next */
	s = db->page_map[pg_ent->pageno % db->pmapSize];
	if (!s) {
		db->page_map[pg_ent->pageno % db->pmapSize] = pg_ent;
		return;
	}
	for (depth = 1; s->next; depth++,  s = s->next);
	s->next = pg_ent;
}

static void freePageMapEntry(MDBM *db, ubig pageno)
{
	mpage_t	*s;
	s = db->page_map[pageno % db->pmapSize];
	if (!s) return;
	if (s->pageno == pageno) {
		db->page_map[pageno % db->pmapSize] = s->next;
		free(s);
		return;
	}
	while (s->next && s->next->pageno != pageno) s = s->next;
	if (s->next) {
		mpage_t *t;
		t = s->next;
		s->next = s->next->next;
		free(t);
	}
}


/* given a page number, return the page address */
/* allocation page if necessary			*/
char * mdbm_page_addr(MDBM *db, ubig pageno)
{
	mpage_t *s, *p;
	int depth;

	if (!_Mdbm_memdb(db)) {
		return ((db)->m_db + ((pageno+1) << db->m_pshift) - INO_SIZE);
	}
	p = db->page_map[pageno % db->pmapSize];
	while (p) {
		if (p->pageno == pageno)
			return p->page_addr;
		p = p->next;
	}

	/* if we get here, pageno is not allocated yet, so allocate one */
	p = malloc(sizeof (mpage_t));
	p->pageno = pageno;
	p->page_addr = ALLOC_PAGE(db);
	INO(p->page_addr, 0) = 0; /* zero out the entry count */
	p->next = NULL;

	/* now we append the new page entry to the list */
	s = db->page_map[pageno % db->pmapSize];
	depth = 0;
	if (!s) {
		db->page_map[pageno % db->pmapSize] = p;
	} else {
		for (depth = 1; s->next; depth++, s = s->next);
		s->next = p;
	}

	if (depth >= MDBM_HASHDEPTH)
		resizePmap(db, db->pmapSize << 1);
	return p->page_addr;
}

static void resizePmap(MDBM *db, ubig new_size)
{
	mpage_t **old_page_map, *s;
	ubig old_pmapSize;
	ubig i;

	/* XX TODO: lock page map for multi-threaded access support */
	old_pmapSize = db->pmapSize;
	db->pmapSize = new_size;

	old_page_map = db->page_map;
	db->page_map = calloc(db->pmapSize, sizeof (mpage_t *));
	for (i = 0; i < old_pmapSize; i++) {
		s = old_page_map[i];
		while (s) {
			mpage_t *q;
			q = s->next;
			s->next = NULL;
			addPageMapEntry(db, s);  /* add pg_ent to new slot */
			s = q;
		}
	}
	free(old_page_map);
}

static void memdb_clean_up(MDBM *db)
{
	assert(db->memdb);
	hash_free(db->memdb);
}


char * mdbm_alloc_page(MDBM *db)
{
	char	*addr;

	if ((addr = malloc(PAGE_SIZE(db))) == NULL)
		return ioerr(db), NULL;
	return (addr + db->m_toffset);
}


big
page_number(MDBM *db, char *p)
{
	if (_Mdbm_memdb(db)) {
		return (-1);
	} else {
		return (
		    (uint32)(((p) - (db)->m_db - PAGE_SIZE(db) + 2)
			    >> (db)->m_pshift));
	}
}


char *get_dir(MDBM *db, int npgsh, int psh)
{
	if  ((npgsh) <= (MIN_DSHIFT - DSHIFT_PER_PAGE)) {
		return	((db)->m_db + MIN_DIR_OFFSET(db));
	}

	if (_Mdbm_memdb(db)) {
		return db->mem_dir;
	} else {
		return ((db)->m_db + DATA_SIZE(npgsh, psh));
	}
}


int
mdbm_store_str(MDBM *db, const char *key, const char *val, int flag)
{
	datum k, v;

	k.dptr = (char *) key; k.dsize = strlen(key) + 1;
	v.dptr = (char *) val; v.dsize = strlen(val) + 1;
	return (mdbm_store(db, k, v, flag));
}

int
mdbm_delete_str(MDBM *db, const char *key)
{
	datum k;

	k.dptr = (char *) key; k.dsize = strlen(key) + 1;
	return (mdbm_delete(db, k));
}

char *
mdbm_fetch_str(MDBM *db, const char *key)
{
	datum k;
	k.dptr = (char *) key; k.dsize = strlen(key) + 1;
	return (mdbm_fetch(db, k).dptr);
}

