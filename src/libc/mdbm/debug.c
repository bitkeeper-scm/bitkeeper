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

#include "common.h"

#define P(p) ((unsigned long)(p))   /* cast pointer to integer for printf */

char *
#ifndef	SGI
pxx(ubig num)
#else
pxx(unsigned long long num)
#endif
{
	static	char	buf[20][8];
	static	int	nextbuf = 0;
	char	*s = buf[nextbuf++];
	char	*tag = "\0KMGTPS";
	double	amt = num;
	int	i;
	char	*t;

	for (i = 0; i < 7 && amt >= 512; i++) {
		amt /= 1024;
	}
	sprintf(s, "%.2f", amt);
	for (t = s; t[1]; t++);
	while (*t == '0') *t-- = 0;
	if (*t == '.') t--;
	t[1] = tag[i]; t[2] = 0;
	if (nextbuf == 7) nextbuf = 0;
	return (s);
}

static int *count;
static ubig byte_used;
static ubig page_used;
static ubig e_count; /* number of entry */

private void
stat_page(MDBM *db, char *pag)
{
	int n;

	if (!(n = NUM(pag))) return;

	count[n/2]++;
	byte_used += OFF(pag);
	e_count += NUM(pag);
	page_used++;
}

private void
chk_page(MDBM *db, char *pag)
{
	idx_t	i, n;
	idx_t	koff = 0, doff;

	if (!_Mdbm_memdb(db)) {
		if ((pag < db->m_db) ||
		    (pag > (db->m_db + (MAX_NPG(db) * PAGE_SIZE(db)))))
			printf("bad page address %ld(0x%lx)\n",
			       P(pag), P(pag));
		if ((int)(TOP(db, pag) - db->m_db) % PAGE_SIZE(db))
			printf("bad page address (algn) %ld(0x%lx)\n",
			       P(pag), P(pag));
	}
	if (NUM(pag) > MAX_FREE(db))
		printf("page (%d): bad n, n=%d\n", PNO(db, pag), NUM(pag));
	if ((OFF(pag) + INO_TOTAL(pag)) > PAGE_SIZE(db))
		printf("page (%d): bad off, off=%d\n", PNO(db, pag), OFF(pag));
	n = NUM(pag);
	for (i = 1; i < n; i += 2) {
		doff = ntoh_idx(INO(pag, i));
		if (doff > MAX_FREE(db)) {
			printf("page (%d) (ent %d): bad doff, doff=%d\n",
			    PNO(db, pag), i, doff);
			continue;
		}
		if ((doff - koff)  <= 0) {
			if ((PAG_ADDR(db, 0) != pag) || (i != 1)) {
				printf("page (%d): bad key size, key size=%d\n",
				    PNO(db, pag), doff - koff);
				continue;
			}
		}
		koff = ntoh_idx(INO(pag, i+1));
		if (koff > MAX_FREE(db)) {
			printf("page (%d)(ent %d) : bad koff, koff=%d\n",
			    PNO(db, pag), (i+1), koff);
			continue;
		}
		if ((koff - doff)  < 0) {
			printf("page (%d): bad val size, val size=%d\n",
			    PNO(db, pag), koff - doff);
			continue;
		}
	}
}

void
dump_page(MDBM *db, char *pag)
{
	datum	key;
	datum	val;
	idx_t	i, n;
	idx_t	koff = 0, doff;
	char	buf[1024];
	char	*p;

	chk_page(db, pag);

	n = NUM(pag);
	p  = TOP(db, pag);

	printf("total number of entry in page(%d): %d(%d)\n",
	    PNO(db, pag), n, n/2);
	for (i = 1; i < n; i += 2) {
		key.dptr = p + koff;
		doff = ntoh_idx(INO(pag, i));
		key.dsize = doff - koff;
		memcpy(buf, key.dptr, key.dsize);
#if	1
		buf[key.dsize] = 0;
		printf("pageno %u: kentry[%d](sz=%d))(off=%d)(ptr=%ld)= %s\n",
		    PNO(db, pag), i, key.dsize, doff, P(key.dptr), buf);
#else
		buf[key.dsize -1] = 0;
		printf("pageno %u: kentry[%d](sz=%d))(off=%d)(ptr=%ld)= %s\n",
		    PNO(db, pag), i, key.dsize, doff, P(key.dptr), buf);
#endif
		val.dptr = p + doff;
		koff = ntoh_idx(INO(pag, i+1));
		val.dsize = koff - doff;
#if	1
		memcpy(buf, val.dptr, val.dsize);
		buf[val.dsize] = 0;
		printf("pageno %u: ventry[%d](sz=%d))(off=%d)(ptr=%ld)= %s\n",
		    PNO(db, pag), (i+1), val.dsize, doff, P(val.dptr), &buf[0]);
#endif
	}
}


private void
count_page(MDBM *db, char *pag)
{
	e_count += NUM(pag);
}

/*
 *  recursive tree walk function
 */
private void
walkAllNodes(MDBM *db,
	ubig dbit, ubig pageno, int level, void (*f)(MDBM *, char *))
{
	if (db->memdb) return;
	if (dbit < (MAX_NPG(db) - 1) && GETDBIT(db->m_dir, dbit)) {
		dbit <<= 1;
		walkAllNodes(db, ++dbit, pageno, level+1, f); /* left */
		walkAllNodes(db, ++dbit,
		    (pageno | (PLUS_ONE<<level)), level+1, f); /* right */
	} else {
		(*f)(db, PAG_ADDR(db, pageno));
	}
}

void
mdbm_dump_all_page(MDBM *db)
{
	walkAllNodes(db, 0, 0, 0, dump_page);
}

void
mdbm_chk_all_page(MDBM *db)
{
	walkAllNodes(db, 0, 0, 0, chk_page);
}

void
mdbm_stat_all_page(MDBM *db)
{
	e_count = 0;
	count = calloc(PAGE_SIZE(db)/2, sizeof(int));
	page_used = byte_used = 0;
	walkAllNodes(db, 0, 0, 0, stat_page);

#ifdef	SGI
	printf("MMAP SPACE page efficiency: %lld %%; (%lld/%lld)\n",
	    (ubig) (page_used * HUNDRED / (ubig) MAX_NPG(db)),
	    page_used, (ubig) MAX_NPG(db));
	printf("MMAP SPACE byte efficiency: %lld %%; (%s/%s)\n",
	    (ubig)(byte_used * HUNDRED / ((ubig)MAX_NPG(db) * PAGE_SIZE(db))),
	    pxx(byte_used), pxx((ubig) (MAX_NPG(db) * PAGE_SIZE(db))));
	printf("DISK SPACE efficiency: %lld %%; (%s/%s) bytes used\n",
	    (ubig)(byte_used * HUNDRED / (page_used * (ubig)PAGE_SIZE(db))),
	    pxx(byte_used), pxx((page_used * (ubig)PAGE_SIZE(db))));
#else
	printf("MMAP SPACE page efficiency: %ld %%; (%ld/%ld)\n",
	    P(page_used * HUNDRED / (ubig)MAX_NPG(db)),
	    P(page_used), P(MAX_NPG(db)));
	printf("MMAP SPACE byte efficiency: %ld %%; (%s/%s)\n",
	    P(byte_used * HUNDRED / ((ubig) MAX_NPG(db) * PAGE_SIZE(db))),
	    pxx(byte_used), pxx((ubig) (MAX_NPG(db) * PAGE_SIZE(db))));
	printf("DISK SPACE efficiency: %ld %%; (%s/%s) bytes used\n",
	    P(byte_used * HUNDRED / (page_used * (ubig) PAGE_SIZE(db))),
	    pxx(byte_used), pxx((page_used * (ubig) PAGE_SIZE(db))));
#endif
	if (_Mdbm_memdb(db))  {
		printf("Page map hash table size = %d\n", db->pmapSize);
	}
	e_count -= 2;  /* igonore the header block */
	e_count >>= 1; /* divide by two		   */
#ifdef	SGI
	printf("Total number of entry: %lld\n", e_count);
#else
	printf("Total number of entry: %ld\n", P(e_count));
#endif
	printf("Maximum B-tree level:  %d\n", db->m_npgshift);
	printf("Minimum B-tree level:  %d\n", minimum_level(db));
}

ubig
count_all_page(MDBM *db)
{
	e_count = 0;
	walkAllNodes(db, ZERO, ZERO, 0, count_page);
	e_count -= 2; /* igonore the header block */
	e_count >>= 1; /* divide by two */
	return e_count;
}
