/*
 * Copyright 1999-2001,2006,2016 BitMover, Inc
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

//#define	MEMORY_ONLY /* XXX */
#define	ERRMSGS

#include "tune.h"	/* has to be before mdbm.h */
#define	__mdbm_c
#include "system.h"

#ifndef	SUPPORT_64BIT
/* for win32 and any 32 bit environment */
#define	MINUS_ONE 	-1L
#define	PLUS_ONE  	1UL
#define	ZERO	  	0L
#define	HUNDRED	  	100L
#else
#define	MINUS_ONE 	-1LL
#define	PLUS_ONE  	1ULL
#define	ZERO	  	0LL
#define	HUNDRED	  	100LL
#endif
/*
 * useful macros - XXX - useful to dbu.c as well, make them accessable
 */
#define	bad(x)			((x).dptr == NULL || (x).dsize <= 0)
#ifdef	ERRMSGS
#define	ioerr(db)		((db)->m_flags |= _MDBM_IOERR), \
				    fprintf(stderr, "ioerr at %s %d\n", \
				    __FILE__, __LINE__)
#else
#define	ioerr(db)		((db)->m_flags |= _MDBM_IOERR)
#endif
#define	INO_SIZE		(sizeof (idx_t))
#define	MAX_NPG(db)		/* max # of page */ \
				(PLUS_ONE<<((db)->m_npgshift))
#define	HASHALL(db)		(MAX_NPG(db) -1)
#define	GETPAGE(db, h)		((_Mdbm_perfect(db) ? \
				    (HASHALL(db) & (h)) : getpage(db, (h))))
#define	PAG_ADDR(db, page_no)	mdbm_page_addr(db, page_no)
#define	SETDBIT(dir, dbit)	{dir[(dbit)>>3] |= ('\01' << ((dbit) & 0x07)); }
#define	UNSETDBIT(dir, dbit)	{dir[(dbit)>>3] &= (~('\01'<<((dbit) & 0x07)));}
#define	GETDBIT(dir, dbit)	(dir[(dbit) >> 3 ] & ('\01' << ((dbit) & 0x07)))
#define	PNO(db, p)		page_number(db, p)
#define	PROT			((db)->m_flags&_MDBM_RDONLY ? \
				    PROT_READ : PROT_READ|PROT_WRITE)
#define	MIN_DIR_OFFSET(db)	(sizeof(MDBM_hdr))
#undef PAGE_SIZE
#define	PAGE_SIZE(db)		(1UL<<db->m_pshift)
#define	MIN_DSHIFT		2  /* minimum dir size ==  1<< this */
#define	MIN_DSIZE		(1<<MIN_DSHIFT)
#define	DSHIFT_PER_PAGE		(-3)	/* 8 page per byte */
#define	DSHIFT(npgshift)	(((npgshift) > (MIN_DSHIFT - DSHIFT_PER_PAGE))\
				    ?  ((npgshift) + DSHIFT_PER_PAGE) \
				    : MIN_DSHIFT)
#define	DIR_SIZE(npgshift)	(PLUS_ONE<<DSHIFT(npgshift))
#define	DATA_SIZE(npgsh, psh)	(PLUS_ONE<<((npgsh) + (psh)))
#define	DB_SIZE(npgsh, psh)	((npgsh <= (MIN_DSHIFT - DSHIFT_PER_PAGE)) ? \
				    DATA_SIZE(npgsh, psh): \
				    DATA_SIZE(npgsh, psh) + DIR_SIZE(npgsh))
#define	GET_DIR(db, npgsh, psh) get_dir(db, npgsh, psh)
#define	M_SIZE_PTR(db)		((uchar *) &(db->m_npgshift))
#define	MH_SIZE_PTR(db)		((uchar *) &(DB_HDR(db)->mh_npgshift))
#define	SIZE_CHANGED(db)	(*M_SIZE_PTR(db) != *MH_SIZE_PTR(db))
#define	MASK(level)		((PLUS_ONE<<(level)) -1)
#define	RE_SIZE(db, newsize)	((ftruncate(db->m_fd, (off_t) newsize) == -1) \
				|| !_remap(db, newsize))
#define	INO(pag, n)		(*((idx_t *)(pag) - (n)))
#define	TOP(db, pg)		(pg - (db)->m_toffset)
#define	XTRACT(db, item, i, pag, off1, off2) \
				{(item).dptr = TOP(db, pag) + (off1); (off2) = \
				    ntoh_idx(INO(pag, i)); \
				    (item).dsize = (off2)- (off1);}
#define	DB_HDR(db)		((MDBM_hdr *)((db)->m_db))
#define	DELPAIR(db, pag, key)	{ datum target; target = key; \
				    split_page(db, pag, 0, \
				    _delete, (void *) (&target)); }
#define	INO_TOTAL(pag)		((NUM(pag) + 1) * INO_SIZE)
#define	MAX_FREE(db)		((db)->m_max_free)
#define	NUM(pag)		(ntoh_idx(INO(pag, 0)))
#define	OFF(pag)		(ntoh_idx(INO(pag, NUM(pag))))
#define	ALGNED(db, off)		((_Mdbm_alnmask(db) & (off)) ? \
				    (~_Mdbm_alnmask(db) & (off)) + \
				    _Mdbm_alnmask(db) + 1 : off)
#define	FREE_AREA(db, pag)	(MAX_FREE(db) - (NUM(pag) * INO_SIZE) - \
				    OFF(pag))
#define	SEE_LAST_PAIR(db, hash, pag, key) \
				((((hash) == (db)->m_last_hash) && \
				    (NUM(pag) >= -((db)->m_kino))) ? \
				    see_last_pair(db, pag, key) : 0)


#define	TOP_OFFSET(db)		 (PAGE_SIZE(db) - INO_SIZE)
#define	MDBM_HASHDEPTH 4
#define	ALLOC_PAGE(db)		mdbm_alloc_page(db)
#define	FREE_PAGE(db, pg_addr)	(free(pg_addr - db->m_toffset))
