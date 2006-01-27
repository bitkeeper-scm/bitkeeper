/*
 * mdbm - ndbm work-alike hashed database library
 * tuning and portability constructs [not nearly enough]
 *
 * Copyright (c) 1991 by Ozan S. Yigit
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

#define	BITS_PER_BYTE		8

#ifdef	SVID
#include <unistd.h>
#endif

#ifdef	BSD42
#define	SEEK_SET	L_SET
#define	memset(s, c, n)		bzero(s, n)	/* only when c is zero */
#define	memcpy(s1, s2, n)	bcopy(s2, s1, n)
#define	memcmp(s1, s2, n)	bcmp(s1, s2, n)
#endif

/*
 * important tuning parms (hah)
 */

#define	SEEDUPS			/* always detect duplicates */
#define	BADMESS			/* generate a message for worst case:
				    cannot make room after SPLTMAX splits */

/*
 * misc
 */
#ifdef	DEBUG
#undef	ERRMSGS
#define	ERRMSGS
#define	debug(x)	fprintf x
#else
#define	debug(x)
#define	sanity(x)
#endif
#ifdef	ERRMSGS
#define	error(x)	fprintf x
#else
#define	error(x)
#endif

#ifdef	WIN32
#define	inline
#endif
