/*
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
