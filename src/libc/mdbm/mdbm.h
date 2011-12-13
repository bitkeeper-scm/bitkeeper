/*
 * mdbm - ndbm work-alike hashed database library based on sdbm which is
 * based on Per-Aake Larson's Dynamic Hashing algorithms.
 * BIT 18 (1978).
 *
 * sdbm Copyright (c) 1991 by Ozan S. Yigit (oz@nexus.yorku.ca)
 *
 * Modifications that:
 *	. Allow 64 bit sized databases,
 *	. used mapped files & allow multi reader/writer access,
 *	. move directory into file, and
 *	. use network byte order for db data structures.
 *	. support fixed size db (with shake function support)
 *	. selectable hash functions
 *	. changed page layout to support data alignment
 *	. support btree pre-split and tree merge/compress
 *	. added a mdbm checker (cf fsck)
 *	. add a statistic/profiler function (for tuning)
 *	. support mdbm_firstkey(), mdbm_nextkey() call.
 * are:
 *	mdbm Copyright (c) 1995, 1996 by Larry McVoy, lm@sgi.com.
 *	mdbm Copyright (c) 1996 by John Schimmel, jes@sgi.com.
 *	mdbm Copyright (c) 1996 by Andrew Chang awc@sgi.com
 *
 * Modification that
 *	support NT/WIN98/WIN95 WIN32 environment
 *	support memory only (non-mmaped) database
 *	support big (4G) pages 
 * are
 *	mdbm Copyright (c) 2001 by Andrew Chang awc@bitmover.com
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
 */
#ifndef	__MDBM_H_
#define	__MDBM_H_
#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/types.h>
#include <assert.h>

/*
 * File format.
 *
 * The database is divided up into m_psize "pages".  The pages do not have
 * to be the same size as the system operating page size, but it is a good
 * idea to make one a multiple of the other.  The database won't create
 * pages smaller that 128 bytes.  Datums must fit on a single page so you
 * probably don't want to change this.	We default to 4K page size if no
 * blocksize is specified at create time.  The page size can't be bigger
 * than 64K because we use 16 bit offsets in the pages.
 *
 * Page types.	The first page is special.  It contains the database header,
 * as well as the first set of directory bits.	The directory bits occupy
 * 8 bytes.The point of putting the directory in
 * the first page was to provide for a very small memory footprint for
 * small databases; this allows this code to be used as a generic hashing
 * package.
 *
 * After the database reaches a size where the directory will no longer fit
 * in the 8 bytes of the first page, the data & directory are grown by
 * doubling its previous size.
 * The directory is copied to the end of data area.  The default page
 * get you to to a terabyte (2^40) database before copying
 * becomes a problem.
 *
 * Page format:
 *	+--------+-----+----+----------+
 *	|  key	 | data	    | key      |
 *	+--------+----------+----------+
 *	|  <---- - - - | data	       |
 *	+--------+-----+----+----------+
 *	|	 F R E E A R E A       |
 *	+--------------+---------------+
 * ino	| n | keyoff | datoff | keyoff |
 *	+------------+--------+--------+
 *
 * We hide our own data in a page by making keyoff == 0.  We do not allow
 * 0 length keys as a result.
 * Suppose we have a 1024 byte page, the header is 16 bytes
 * (i.e. sizeof(MDBM_hdr)),
 * the directory is 4 bytes. (i.e. total is 16 + 4 = 20 bytes )
 *
 * Page 0 looks like
 *	+--------+-----+----+----------+ first entry in page zero
 *	| 16 bytes header (never moves)| has a zero length key.
 *	|  and 4 bytes for directory   | data area has a mdbm header
 *	+--------+-----+----+----------+ and a directory bit array.
 *	|	 F R E E A R E A       |
 *	+--------+-----+----+----------+
 * ino	| 2 | 0 | 20 |		       |
 *	+------------+--------+--------+
 *	     ^^^
 *	    this signifies a zero length key
 */

#define	_SPLTMAX    10	/* maximum allowed splits */
			/* for a single insertion */
/* #define	_INTERNAL	(64*1024-1) */

/*
 * Types.
 */
#ifndef	uchar
# define uchar	unsigned char
#endif
#ifndef	int16
# define int16	short
#endif
#ifndef	uint16
# define uint16 unsigned short
#endif
#ifndef	int32
# define int32	int
#endif
#ifndef	uint32
# define uint32 unsigned int
#endif

/*
 * If your enviromnet support 64 bit int
 * define the following
 */
/* #define	SUPPORT_64BIT  */

#ifdef	SUPPORT_64BIT
#define	big int64
#define	ubig uint64
#else
#define	big int32
#define	ubig uint32
#endif

#ifdef	SUPPORT_64BIT
#ifndef	int64
# ifdef _LONGLONG
#  define int64 long long
# else
#  define int64 __long_long
# endif
#endif
#ifndef	uint64
# ifdef _LONGLONG
#  define uint64	unsigned long long
# else
#  define uint64	unsigned __long_long
# endif
#endif
#endif	/* SUPPORT_64BIT */

#include "byte_order.h"

//#define BIG_PAGE
#ifdef	BIG_PAGE	/* max page size 4G => 32 bit index */
typedef	uint32	idx_t;	/* type for page index; uint 32 */
#define	MAX_IDX	0xffffffff
#define	hton_idx(i) _htonl(i)
#define	ntoh_idx(i) _ntohl(i)
#else			/* max page size 64K => 16 bit index */
typedef	uint16	idx_t;	/* type for page index; uint 16 */
#define	MAX_IDX	0xffff
#define	hton_idx(i) _htons(i)
#define	ntoh_idx(i) _ntohs(i)
#endif

/*
 * This is the header for the database.	 Keep it small.
 * The values are in network byte order.
 * XXX mh_pad is added to make sure the header size is the same 
 * across all platforms. Some complier allocate padding differently
 */
typedef	struct {
	uint32		mh_magic;	 /* header magic number */
	uchar		mh_npgshift;	 /* # of pages is 1<< this */
	uchar		mh_pshift;	 /* # of pages is 1<< this */
	uchar		mh_max_npgshift; /* for bounded size db */
	uchar		mh_dbflags;
	uint32		mh_lockspace;	 /* a place to stick a lock */
	signed char	mh_minpg_level;	 /* first level a page apear */
	uchar		mh_hashfunc;	 /* see values below */
	uchar		mh_pad[2];	 /* not used */
} MDBM_hdr;		/* MDBM version 1 header */

#ifndef	_DATUM_DEFINED
typedef	struct {
	char	*dptr;
	int	dsize;
} datum;
#endif
typedef	struct {
	datum key;
	datum val;
} kvpair;

typedef	struct page {
	ubig	pageno;
	char	*page_addr;
	struct	page *next;
} mpage_t;

/*
 * In memory handle structure.	Fields are duplicated from the
 * header so that we don't have to byte swap on each access.
 */
struct	mdbm {
	char	    *m_db;	/* database itself */
	char	    *m_dir;	/* directory bits */
	int	    m_fd;	/* database file descriptor */
	int	    m_level;	/* current level */
	int	    m_amask;	/* alignment mask */
	uint32	    m_max_free; /* max free area per page */
	int	    m_toffset;	/* offset to top of page */
	ubig	    m_curbit;	/* XXX - current bit number */
	ubig	    m_last_hash; /* last hash value */
	ubig	    m_pageno;	/* last page fetched */
	uchar	    m_npgshift; /* # of pages is 1<< this */
	uchar	    m_pshift;	/* page size is 1<<this */
	int	    m_kino;	/* last key fetched from m_page */
	int	    m_next;	/* for getnext */
	short	    m_flags;	/* status/error flags, see below */
	int	    (*m_shake)(struct mdbm *, datum, datum, void *);
				/* "shake" function */
	ubig	    (*m_hash)(unsigned char *, int);
				/* hash function */
	char	    *mem_dir;	/* LMXXX - is this right? */
	ubig	    pmapSize;	/* size of the page map hash array */
	mpage_t	    **page_map;	/* hash buckets */
	hash	    *memdb;
};

typedef	struct mdbm MDBM;

#define	_MDBM_RDONLY	0x1	/* data base open read-only */
#define	_MDBM_IOERR	0x2	/* data base I/O error */
#define	_MDBM_LOCKING	0x4	/* lock on read/write */
#define	_MDBM_MEM	0xA	/* memory only db */
#define	_Mdbm_rdonly(db)	((db)->m_flags & _MDBM_RDONLY)
#define	_Exhash(db, item)	(db)->m_hash((uchar *)item.dptr, item.dsize)

#define	_MDBM_ALGN16	0x1
#define	_MDBM_ALGN32	0x3
#define	_MDBM_ALGN64	0x7
#define	_MDBM_PERFECT	0x8	/* perfectly balanced tree */
#define	_Mdbm_perfect(db)	(DB_HDR(db)->mh_dbflags & _MDBM_PERFECT)
#define	_Mdbm_alnmask(db)	((db)->m_amask)
#define	_Mdbm_memdb(db)		((db)->m_flags & _MDBM_MEM)

/*
 * flags to mdbm_store
 */
#define	MDBM_INSERT	0
#define	MDBM_REPLACE	1

/*
** Hash functions
*/
#define	MDBM_HASH_CRC32		0	/* table based 32bit crc */
#define	MDBM_HASH_EJB		1	/* from hsearch */
#define	MDBM_HASH_PHONG		2	/* congruential hash */
#define	MDBM_HASH_OZ		3	/* from sdbm */
#define	MDBM_HASH_TOREK		4	/* from Berkeley db */

/*
 * Page size range supported.  You don't want to make it smaller and
 * you can't make it bigger - the offsets in the page are 16 bits.
 */
#define	MDBM_MINPAGE	128
#define	MDBM_MAXPAGE	(64*1024)
#define	MDBM_PAGESIZ	4096	/* a good default */
#define	MDBM_MIN_PSHIFT 7	/* this must match MDBM_MINPAGE above */

/*
 * Magic number which is also a version number.	  If the database format
 * changes then we need another magic number.
 */
#define	_MDBM_MAGIC 0x01023962


/*
 * mdbm interface
 */
extern	MDBM	*mdbm_mem_open(int, int);
extern	MDBM	*mdbm_open(char *, int, int, int);
extern	void	mdbm_close(MDBM *);
extern	datum	mdbm_fetch(MDBM *, datum);
extern	char 	*mdbm_fetch_str(MDBM *, const char *);
extern	int	mdbm_delete(MDBM *, datum);
extern	int	mdbm_delete_str(MDBM *, const char *);
extern	int	mdbm_store(MDBM *, datum, datum, int);
extern	int	mdbm_store_str(MDBM *, const char *, const char *, int);
extern	kvpair	mdbm_first(MDBM *);
extern	datum	mdbm_firstkey(MDBM *);
extern	kvpair	mdbm_next(MDBM *);
extern	datum	mdbm_nextkey(MDBM *);
extern	int	mdbm_pre_split(MDBM *, ubig);
extern	int	mdbm_set_alignment(MDBM *, int);
extern	int	mdbm_limit_size(MDBM *, ubig,
		    int (*func)(struct mdbm *, datum, datum, void *));
extern	void	mdbm_compress_tree(MDBM *);
extern	void	mdbm_stat_all_page(MDBM *);
extern	void	mdbm_chk_all_page(MDBM *);
extern	void	mdbm_dump_all_page(MDBM *);
extern	void	mdbm_sync(MDBM *);
extern	void	mdbm_close_fd(MDBM *);
extern	void	mdbm_truncate(MDBM *);
extern	int	mdbm_lock(MDBM *);
extern	int	mdbm_unlock(MDBM *);
extern	int	mdbm_sethash(MDBM *, int);

#define	mdbm_isEmpty(db)	(mdbm_firstkey(db).dptr == 0)

/* for debug */
extern	ubig	count_all_page(MDBM *db);

/*
 * optional - may be user supplied.
 */
extern	ubig	mdbm_hash(unsigned char *, int);
extern	ubig	mdbm_hash0(unsigned char *, int);
extern	ubig	mdbm_hash1(unsigned char *, int);
extern	ubig	mdbm_hash2(unsigned char *, int);
extern	ubig	mdbm_hash3(unsigned char *, int);
extern	ubig	mdbm_hash4(unsigned char *, int);

/*
 * Internal interfaces for libmdbm.
 */
#ifdef	__mdbm_c
extern char	* pxx(ubig);
extern void	dump_page(MDBM *, char *);
extern uchar	minimum_level(MDBM *);
extern big	page_number(MDBM *, char *);
extern char	*get_dir(MDBM *, int, int);

char * mdbm_page_addr(MDBM *, ubig);
char * mdbm_alloc_page(MDBM *);

#ifdef	WIN32
#include "win32/mman.h"
#include "win32/misc.h"
#endif

#ifndef	SGI
/* define locking stuff to null */
#define	init_lock(x)
#define	spin_lock(x)
#define	release_lock(x)
#endif

#endif	/* __mdbm_c */
/*
 * End of of Internal interfaces for libmdbm
 */



#ifndef	NULL
# define	NULL	0L
#endif

/*
 * Compatibility defines for NDBM API
 */
#ifdef	NDBM_COMPAT
#define	DBM			MDBM
#define	DBM_INSERT		MDBM_INSERT
#define	DBM_REPLACE		MDBM_REPLACE
#define	dbm_open(a, b, c)	mdbm_open(a, b, c, 0)
#define	dbm_close(a)		mdbm_close(a)
#define	dbm_fetch(a, b)		mdbm_fetch(a, b)
#define	dbm_delete(a, b)	mdbm_delete(a, b)
#define	dbm_store(a, b, c, d)	mdbm_store(a, b, c, d)
#define	dbm_firstkey(a)		((datum)mdbm_firstkey(a))
#define	dbm_nextkey(a)		((datum)mdbm_nextkey(a))
#endif	/* NDBM_COMPAT */

/*
 * Compatibility defines for old DBM API
 */
#ifdef	DBM_COMPAT
MDBM *mdbmdb;
#define	dbminit(a)		((ubig)(mdbmdb = mdbm_open(a, 2, 0, 0, 0)))
#define	dbmclose()		mdbm_close(mdbmdb);
#define	fetch(a)		(mdbm_fetch(mdbmdb, a))
#define	store(a, b)		(mdbm_store(mdbmdb, a, b, MDBM_REPLACE))
#define	delete(a)		(mdbm_delete(mdbmdb, a))
#define	firstkey()		((datum)mdbm_firstkey(mdbmdb))
#define	nextkey()		((datum)mdbm_nextkey(mdbmdb))
#endif	/* DBM_COMPAT */

#ifdef	LINT
#	define	WHATSTR(X)	pid_t	getpid(void)
#else
#	define	WHATSTR(X)	static const char what[] = X
#endif
#ifdef	__cplusplus
}
#endif
#endif	/* __MDBM_H_ */
