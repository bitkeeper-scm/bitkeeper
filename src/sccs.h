/* %W% Copyright (c) 1997 Larry McVoy */
#ifndef	_SCCS_H_
#define	_SCCS_H_

#include "mmap.h"

/*
 * Flags that modify some operation (passed to sccs_*).
 *
 * There are two ranges of values, 0x000000ff and 0xffffff00.
 * The first is for flags which may be meaningful to all functions.
 * The second is for flags which are function specific.
 * Be VERY CAREFUL to not mix and match.  If I see a DELTA_ in sccs_get()
 * I will be coming after you with a blowtorch.
 */
#define	SILENT		0x00000001	/* do work quietly */
#define	PRINT		0x00000002	/* get/delta/clean [diffs] to stdout */
#define	NEWFILE		0x00000008	/* delta -i: create initial file */
#define	NEWCKSUM	0x00000010	/* Redo checksum */

#define	INIT_MAPWRITE	0x10000000	/* map the file read/write */
#define	INIT_NOCKSUM	0x20000000	/* don't do the checksum */
#define INIT_GTIME	0x40000000	/* use g file mod time as time stamp */
#define	INIT_SAVEPROJ	0x80000000	/* project is loaned, do not free it */

/* shared across get/diffs/getdiffs */
#define	GET_EDIT	0x10000000	/* get -e: get for editting */
#define	GET_EXPAND	0x20000000	/* expand keywords */
#define	GET_REVNUMS	0x40000000	/* get -m: prefix each line with rev */
#define GET_USER	0x80000000	/* get -u: prefix with user name */
#define GET_SKIPGET	0x01000000	/* get -g: don't get the file */
#define	GET_RCSEXPAND	0x02000000	/* do RCS keywords */
#define	GET_ASCII	0x04000000	/* Do not gunzip/uudecode */
#define	GET_LINENUM	0x08000000	/* get -N: show line numbers */
#define	GET_MODNAME	0x00100000	/* get -n: prefix with %M */
#define	GET_PREFIXDATE	0x00200000	/* get -d: show date */
#define GET_PATH	0x00400000	/* use delta (original) path */
#define	GET_SHUTUP	0x00800000	/* quiet on certain errors */
#define	GET_BRANCH	0x00010000	/* force a branch when creating delta */
#define	GET_FORCE	0x00020000	/* do it even with errors */
#define	GET_HEADER	0x00040000	/* diff: print header */
#define	DIFF_HEADER	GET_HEADER
#define	GET_PREFIX	\
	    (GET_REVNUMS|GET_USER|GET_LINENUM|GET_MODNAME|GET_PREFIXDATE)
#define	GET_DTIME	0x00080000	/* gfile get delta's mode time */
#define	GET_NOHASH	0x00001000	/* force regular file, ignore S_HASH */
#define	GET_HASHONLY	0x00002000	/* skip the file */
#define	GET_DIFFS	0x00004000	/* get -D, regular diffs */
#define	GET_BKDIFFS	0x00008000	/* get -DD, BK (rick's) diffs */
#define	GET_HASHDIFFS	0x00000100	/* get -DDD, 0a0 hash style diffs */
#define	GET_SUM		0x00000200	/* used to force dsum in getRegBody */
#define GET_NOREGET	0x00000400	/* get -S: skip gfiles that exist */

#define	CLEAN_UNEDIT	0x10000000	/* clean -u: discard changes */
#define CLEAN_UNLOCK	0x20000000	/* clean -n: just unlock */
#define CLEAN_SHUTUP	0x40000000	/* clean -Q: quite mode */

#define	DELTA_AUTO	0x10000000	/* delta -a: auto check-in mode */
#define	DELTA_SAVEGFILE	0x20000000	/* delta -n: save edited gfile */
#define	DELTA_DONTASK	0x40000000	/* don't ask for comments */
#define	DELTA_PATCH	0x80000000	/* delta -R: respect rev */
#define	DELTA_EMPTY	0x01000000	/* initialize with empty file */
#define	DELTA_FORCE	0x02000000	/* delta -f: force a delta */
#define	DELTA_HASH	0x04000000	/* treat as hash (MDBM) file */

#define	ADMIN_FORMAT	0x10000000	/* check file format (admin) */
#define	ADMIN_ASCII	0x20000000	/* check file format (admin) */
#define	ADMIN_TIME	0x40000000	/* warn about time going backwards */
#define	ADMIN_SHUTUP	0x80000000	/* don't be noisy about bad revs */
#define	ADMIN_BK	0x01000000	/* check BitKeeper invariants */
#define	ADMIN_GONE	0x02000000	/* check integrity w/o GONE deltas */

#define	PRS_META	0x10000000	/* show metadata */
#define	PRS_SYMBOLIC	0x20000000	/* show revs as beta1, etc. Not done */
#define	PRS_PATCH	0x40000000	/* print in patch format */
#define PRS_ALL		0x80000000	/* scan all revs, not just type D */

#define SINFO_TERSE	0x10000000	/* print in terse format: sinfo -t */

/*
 * flags passed to sfileFirst
 */
#define	SF_GFILE	0x00000001	/* gfile should be readable */
#define	SF_WRITE_OK	0x00000002	/* gfile should be writable */
#define	SF_NODIREXPAND	0x00000004	/* don't auto expand directories */
#define	SF_HASREVS	0x00000008	/* sfiles - filename:rev */
#define	SF_SILENT	0x00000010	/* sfiles - don't complain */
#define	SF_DELETES	0x00000020	/* expand files like .del-whatever */

/*
 * Flags (s->state) that indicate the state of a file.  Set up in init.
 */
#define	S_SFILE		0x00000001	/* s->sfile exists as a regular file */
#define	S_GFILE		0x00000002	/* s->gfile exists as a regular file */
#define	S_PFILE		0x00000004	/* SCCS/p.file exists */
#define S_EDITED	(S_SFILE|S_PFILE|S_GFILE)
#define	S_ZFILE		0x00000008	/* SCCS/z.file exists */
#define	S_SOPEN		0x00000010	/* s->sfile is open */
#define	S_WARNED	0x00000020	/* error message already sent */
#define	S_RCS		0x00000040	/* expand RCS keywords */
#define	S_BRANCHOK	0x00000080	/* branching allowed */
#define	S_EXPAND1	0x00000100	/* expand first line of keyowrds only */
#define	S_CHMOD		0x00000200	/* change the file back to 0444 mode */
#define	S_YEAR4		0x00000400	/* print out the year as 4 digits */
#define	S_BADREVS	0x00000800	/* has corrupted revisions */
#define	S_BIGPAD	0x00001000	/* admin -B: make the landing pad big */
#define	S_REINHERIT	0x00002000	/* found stuff in flags, reinherit */
#define	S_BITKEEPER	0x00004000	/* X_BITKEEPER flag */
#define	S_CSET		0x00008000	/* this is a changeset file */
#define	S_NOSCCSDIR	0x00010000	/* this is a s.foo not SCCS/s.foo */
#define S_MAPPRIVATE	0x00020000	/* hack for Samba */
#define S_READ_ONLY	0x00040000	/* force read only mode */
#define	S_RANGE2	0x00080000	/* second call for date|rev range */
#define	S_BAD_DSUM	0x00100000	/* patch checksum mismatch */
#define S_ISSHELL	0x00200000	/* this is a shell script */
#define	S_SET		0x00400000	/* the tree is marked with a set */
#define	S_CSETMARKED	0x00800000	/* X_CSETMARKED match */
#define S_CACHEROOT	0x01000000	/* don't free the root entry */
#define	S_KEY2		0x02000000	/* all keys are version 2 format */
#define	S_HASH		0x04000000	/* this file is an MDBM file */
#define	S_FAKE_1_0	0x08000000	/* the 1.0 delta is a fake */
#define	S_SAVEPROJ	0x10000000	/* do not free the project struct */

#define	KEY_FORMAT2	"BK key2"	/* sym in csets created w/ long keys */

/*
 * Options to sccs_diffs()
 */
#define	DF_DIFF		'd'
#define	DF_SDIFF	's'
#define	DF_CONTEXT	'c'
#define	DF_UNIFIED	'u'
#define	DF_PDIFF	'p'
#define	DF_RCS		'n'
#define	DF_GNU_PATCH	'g'

/*
 * Date handling.
 */
#define	ROUNDUP	1
#define	EXACT	0
#define	ROUNDDOWN -1
#define	DATE(d)		(d->date ? d->date : getDate(d))
#define	CHKDATE(d)	unless (d->date || \
			    streq("70/01/01 00:00:00", d->sdate)) { \
				assert(d->date); \
			}

/*
 * Bits for the x flag in the s.file.
 *
 * Nota bene: these can not change once the product is shipped.  Ever.
 * They are stored on disk.
 */
#define	X_BITKEEPER	0x00000001	/* BitKeeper file, not SCCS */
#define	X_RCSEXPAND	0x00000002	/* RCS keywords */
#define	X_YEAR4		0x00000004	/* 4 digit years */
#define	X_ISSHELL	0x00000008	/* This is a shell script */
#define	X_EXPAND1	0x00000010	/* Expand first line of keywords only */
#define	X_CSETMARKED	0x00000020	/* ChangeSet boundries are marked */
#define	X_HASH		0x00000040	/* mdbm file */

/*
 * Encoding flags.
 */
#define	E_ASCII		0		/* no encoding */
#define	E_UUENCODE	1		/* uuenecode it (traditional) */
#define	E_UUGZIP	2		/* gzip and uuencode */
#define E_DATAENC	(E_UUENCODE | E_UUGZIP)
#define	E_GZIP		4		/* gzip the data */

#define	HAS_GFILE(s)	((s)->state & S_GFILE)
#define	HAS_PFILE(s)	((s)->state & S_PFILE)
#define	HAS_ZFILE(s)	((s)->state & S_ZFILE)
#define	HAS_SFILE(s)	((s)->state & S_SFILE)
#define	BEEN_WARNED(s)	((s)->state & S_WARNED)
#define	IS_WRITABLE(s)	((s)->mode & 0200)
#define IS_EDITED(s)	((((s)->state&S_EDITED) == S_EDITED) && IS_WRITABLE(s))
#define WRITABLE(s)	(IS_WRITABLE(s) && isRegularFile(s->mode))

#define	GOODSCCS(s)	assert(s); unless (s->tree && s->cksumok) return (-1)

/*
 * Flags (d->flags) that indicate some state on the delta.
 */
#define	D_ERROR		0x00000001	/* from parseArg() */
#define	D_NOHOST	0x00000002	/* don't generate a hostname */
#define	D_NOPATH	0x00000004	/* don't generate a pathname */
#define	D_NOZONE	0x00000008	/* don't generate a time zone */
#define	D_NOCOMMENTS	0x00000010	/* don't generate comments */
#define	D_DUPHOST	0x00000020	/* this host pointer is shared */
#define	D_DUPPATH	0x00000040	/* this path pointer is shared */
#define	D_DUPZONE	0x00000080	/* this zone pointer is shared */
#define	D_REMOTE	0x00000100	/* for resolve; from remote repos. */
#define	D_BADFORM	0x00000200	/* poorly formed rev */
#define	D_BADREV	0x00000400	/* bad parent/child relationship */
#define	D_STEAL		0x00000800	/* sccslog is stealing this rev */
#define	D_META		0x00001000	/* this is a metadata removed delta */
#define	D_SYMBOLS	0x00002000	/* delta has one or more symbols */
#define	D_DUPCSETFILE	0x00004000	/* this changesetFile is shared */
#define	D_VISITED	0x00008000	/* and had a nice cup of tea */
#define	D_CKSUM		0x00010000	/* delta has checksum */
#define	D_MERGED	0x00020000	/* set on branch tip which is merged */
#define	D_GONE		0x00800000	/* this delta is gone, don't print */
#define D_ICKSUM	0x01000000	/* use checksum from init file */
#define	D_MODE		0x02000000	/* permissions in d->mode are valid */
#define	D_SET		0x04000000	/* range.c: marked as part of a set */
#define	D_CSET		0x08000000	/* this delta is marked in cset file */
#define D_DUPLINK	0x10000000	/* this symlink pointer is shared */
#define	D_LOCAL		0x20000000	/* for resolve; this is a local delta */
#define D_XFLAGS	0x40000000	/* delta has updated file flags */
#define D_TEXT		0x80000000	/* delta has updated text */

/*
 * Signal handling.
 * Caught signals do not restart system calls.
 */
#define	CATCH		0x0001		/* catch the specified signal */
#define	BLOCK		0x0002		/* block the signal */
#define	UNBLOCK		0x0004		/* block the signal */
#define	CHKPENDING	0x0008		/* check for pending signal */
#define	UNCATCH		0x0010		/* restore old handler */
#define	CAUGHT		0x0020		/* has this signal been caught? */
#define	CLEAR		0x0040		/* forget about past catches */

/*
 * Hash behaviour.  Bitmask.
 */
#define DB_NODUPS       1		/* keys must be unique */
#define DB_USEFIRST     2		/* use the first key found */
#define DB_USELAST      4		/* use the last key found */
#define	DB_KEYSONLY	8		/* boolean hashes */

#define	MAXREV	24	/* 99999.99999.99999.99999 */

#define	SCCSTMP		"SCCS/T.SCCSTMP"
#define	BKROOT		"BitKeeper/etc"
#define	GONE		"BitKeeper/etc/gone"
#define	SGONE		"BitKeeper/etc/SCCS/s.gone"
#define	CHANGESET	"SCCS/s.ChangeSet"
#define	IDCACHE		"BitKeeper/etc/SCCS/x.id_cache"
#define	IDCACHE_LOCK	"BitKeeper/etc/SCCS/z.id_cache"
#define	TMP_MODE	0666
#define	GROUP_MODE	0664

#define	UNKNOWN_USER	"anon"

#define	isData(buf)	(buf[0] != '\001')
#define	seekto(s,o)	s->where = (s->mmap + o)
#define	eof(s)		((s->encoding & E_GZIP) ? \
			    zeof() : (s->where >= s->mmap + s->size))
#define	new(p)		p = calloc(1, sizeof(*p))

typedef	unsigned short	ser_t;
typedef	unsigned short	sum_t;
typedef	unsigned int	u32;
typedef	unsigned short	u16;
typedef	unsigned char	u8;

/*
 * Struct delta - describes a single delta entry.
 *
 * If we have a tree like so:
 * 1.1	1.2	1.3
 *      1.1.1.1
 * and we are rev 1.2, then our parent is to 1.1, our kid is 1.3,
 * and our sibling is 1.1.1.1.
 *
 * Note: we don't implement MRs - should we?
 * Fri Apr  9 1999:  this has grown to 124 bytes.  We might want to think
 * about reducing the size.  That means a 5K delta file takes 620K in
 * memory just for the graph.
 *
 * Something else we ought to do: our own allocator/deallocator which allocates
 * enough memory for all the data structures at init time.  That way we can
 * fail the init if there is no memory.
 */
typedef struct delta {
	/* stuff in original SCCS */
	u32	added;			/* lines added by this delta (u32!) */
	u32	deleted;		/* and deleted */
	u32	same;			/* and unchanged */
	char	type;			/* Delta or removed delta */
	char	*rev;			/* revision number */
	char	*sdate;			/* ascii date in local time, i.e.,
					 * 93/07/25 21:14:11 */
	char	*user;			/* user name of delta owner */
	ser_t	serial;			/* serial number of this delta */
	ser_t	pserial;		/* serial number of parent */
	ser_t	*include;		/* include serial #'s */
	ser_t	*exclude;		/* exclude serial #'s */
	ser_t	*ignore;		/* ignore serial #'s */
	char	**mr;			/* MR's are just passed through */
	char	**comments;		/* Comment log */
	/* New stuff in lm's sccs */
	char	**text;			/* descriptive text log */
	char	*hostname;		/* hostname where revision was made */
	char	*pathname;		/* pathname to the file */
	char	*zone;			/* 08:00 is time relative to GMT */
	char	*csetFile;		/* id for ChangeSet file */
	ser_t	merge;			/* serial number merged into here */
	sum_t	sum;			/* checksum of gfile */
	time_t	dateFudge;		/* make dates go forward */
	mode_t	mode;			/* 0777 style modes */
	char 	*symlink;		/* sym link target */
	u32	xflags;			/* x flags */
	/* In memory only stuff */
	u16	r[4];			/* 1.2.3 -> 1, 2, 3, 0 */
	time_t	date;			/* date - conversion from sdate/zone */
	struct	delta *parent;		/* parent delta above me */
	struct	delta *kid;		/* next delta on this branch */
	struct	delta *siblings;	/* pointer to other branches */
	struct	delta *next;		/* all deltas in table order */
	struct	delta *link;		/* link to a matching delta (smoosh) */
	int	flags;			/* per delta flags */
} delta;

/*
 * Rap on lod/symbols wrt deltas.
 * Both symbols can occur exactly once per delta where delta is a node in
 * the graph.  When a delta is created, it might get a symbol and/or a lod.
 * Later, it might get another one.  These later ones are implemented as
 * metadata deltas (aka removed deltas) which point up to the real delta.
 * So it is an invariant: one node in the graph, one symbol and/or one lod.
 *
 * The sym/lod fields in the delta are for initialization and are extracted
 * and put into the appropriate lists once the delta is entered in the graph.
 *
 */

/*
 * Symbolic names for revisions.
 * Note that these are fully specified revisions only.
 */
typedef	struct symbol {			/* symbolic tags */
	struct	symbol *next;		/* s->symbols sorted on date list */
	char	*name;			/* STABLE */
	char	*rev;			/* 1.32 */
	delta	*d;			/* delta associated with this one */
					/* only for absolute revs, not LOD */
	delta	*metad;			/* where the symbol lives on disk */
} symbol;

/*
 * See slib.c:allocstate() for explanation.
 */
#define	SFREE	1
#define	SLIST	0
typedef struct serial {
	struct	serial *next;		/* forward link with offset 0 */
	ser_t	serial;			/* # we're working on */
	char	type;			/* 'I' or 'E' */
} serlist;

#define	S_INC	1
#define	S_EXCL	2
typedef struct {
	ser_t	ser;			/* serial number that set ... */
	short	what;			/* ... this to S_INC|S_EXCL */
} ielist;

/*
 * Rap on project roots.  In BitKeeper, lots of stuff wants to know
 * where the project root is relative to where we are.  We need to
 * use the ->root field for this, remembering to null it whenever
 * we change directories.
 *
 * We also need to wack commands that work in loops to reuse the root
 * pointer across sccs_init()s.  This means that loops should extract
 * the root pointer from the sccs pointer, null the sccs pointer, and
 * then put the root pointer into the next sccs pointer.  This means that
 * sccs_init() should *NOT* go find that root directory, it should do it
 * lazily in sccs_root().  That gives us a chance to pass it in.
 */
typedef struct {
	int	flags;		/* PROJ_* */
	char	*root;		/* to the root of the project */
} project;

#define	PROJ_RESYNC	0x00000001	/* Locked by resync */

/*
 * struct sccs - the delta tree, the data, and associated junk.
 */
typedef	struct sccs {
	delta	*tree;		/* the delta tree after mkgraph() */
	delta	*table;		/* the delta table list, 1.99 .. 1.0 */
	delta	*lastinsert;	/* pointer to the last delta inserted */
	delta	*meta;		/* deltas in the meta data list */
	symbol	*symbols;	/* symbolic tags sorted most recent to least */
	symbol	*symTail;	/* last symbol, for tail inserts */
	char	*defbranch;	/* defbranch, if set */
	int	numdeltas;	/* number of entries in the graph */
	int	nextserial;	/* next unused serial # */
				/* due to gaps, those two may not be the same */
	delta	**ser2delta;	/* indexed by serial, returns delta */
	int	ser2dsize;	/* just to be sure */
	char	*mmap;		/* mapped file */
	char	*where;		/* where we are in the mapped file */
	off_t	size;		/* size of mapping */
	char	*landingpad;	/* some space for symbols */
	int	fd;		/* cached copy of the file descriptor */
	char	*sfile;		/* SCCS/s.foo.c */
	char	*pfile;		/* SCCS/p.foo.c */
	char	*zfile;		/* SCCS/z.foo.c */
	char	*gfile;		/* foo.c */
	char	*symlink;	/* if gfile is a sym link, the destination */
	char	**usersgroups;	/* lm, beth, staff, etc */
	int	encoding;	/* ascii, uuencode, gzip, etc. */
	char	**flags;	/* flags in the middle that we didn't grok */
	char	**text;		/* descriptive text */
	int	state;		/* GFILE/SFILE etc */
	char	*random;	/* random bits for file ID */
	mode_t	mode;		/* mode of the gfile */
	off_t	data;		/* offset to data in file */
	delta	*rstart;	/* start of a range (1.1 - oldest) */
	delta	*rstop;		/* end of range (1.5 - youngest) */
	sum_t	 cksum;		/* SCCS chksum */
	sum_t	 dsum;		/* SCCS delta chksum */
	off_t	sumOff;		/* offset of the new delta cksum */
	time_t	gtime;		/* gfile modidification time */
	MDBM	*mdbm;		/* If state & S_HASH, put answer here */
	project	*proj;		/* If in BK mode, pointer to project */
	unsigned int cksumok:1;	/* check sum was ok */
} sccs;

typedef struct {
	int	flags;		/* ADD|DEL|FORCE */
	char	*thing;		/* whatever */
} admin;
#define	A_SZ		20	/* up to 20 adds/deletes of a single type */
#define	A_ADD		0x0001
#define	A_DEL		0x0002

/*
 * Passed back from read_pfile().
 */
typedef struct {
	char	oldrev[MAXREV];		/* XXX - needs to be malloced */
	char	newrev[MAXREV];		/* XXX - needs to be malloced */
	char	sccsrev[MAXREV];
	char	*user;			/* malloced - caller frees */
	char	date[20];
	char	*iLst;			/* malloced - caller frees */
	char	*xLst;			/* malloced - caller frees */
	char	*mRev;			/* malloced - caller frees */
} pfile;

/*
 * RESYNC directory layout.
 *
 * This directory is created at the same level as the working directory
 * where the ChangeSet file lives.
 * It is a sparse copy of the destination, containing only files which have
 * been patched.
 * The patch file itself lives in PENDING, which also exists at the same
 * level as the RESYNC directory.
 * Inside the RESYNC directory is a subdirectory, BitKeeper.  In that, we
 * find:
 * RESYNC/BitKeeper/patch - a file containing the pathname to the
 *	patch file in PENDING
 * RESYNC/BitKeeper/pid - the process id of the tkpatch or resolve program.
 * RESYNC/BitKeeper/init-%d - the init file for one delta of the patch
 * RESYNC/BitKeeper/diff-%d - the diff file for one delta of the patch
 * ------------------------------------------------------------------------
 * patch list - this list will contain all the deltas up to a GCA
 * from both the local file and the patch.
 *
 * Operations on this list take place in the RESYNC directory, which
 * is in the top directory of the project.
 *
 * This entire list and everything in it is malloced, and needs to be
 * freed.  As part of freeing it up, the init/diff files must be
 * unlinked.
 */
typedef struct patch {
	char	*localFile;	/* sccs/SCCS/s.get.c */
	char	*resyncFile;	/* RESYNC/sccs/SCCS/s.get.c */
	char	*pid;		/* unique key of parent */
				/* NULL if the new delta is the root. */
	char	*me;		/* unique key of this delta */
	char	*initFile;	/* RESYNC/BitKeeper/init-1, only if !initMmap */
	MMAP	*initMmap;	/* points into mmapped patch */
	char	*diffFile;	/* RESYNC/BitKeeper/diff-1, only if !diffMmap */
	MMAP	*diffMmap;	/* points into mmapped patch */
	time_t	order;		/* ordering over the whole list, oldest first */
	int	flags;		/* local/remote /etc */
	struct	patch *next;	/* guess */
} patch;

/*
 * Patch file format strings.
 */
#define PATCH_CURRENT	"# Patch vers:\t0.7\n"
#define PATCH_NOSUM	"# Patch vers:\t0.5\n"

/*
 * Internal to takepatch
 */
#define	PATCH_LOCAL	0x0001	/* patch is from local file */
#define	PATCH_REMOTE	0x0002	/* patch is from remote file */
#define	PATCH_META	0x0004	/* delta is metadata */

/*
 * The amount of clock drift we handle when generating keys.
 */
#define	CLOCK_DRIFT	(2*24*60*60)

int	sccs_admin(sccs *sc, delta *d, u32 flgs, char *encoding, char *compress,
	    admin *f, admin *l, admin *u, admin *s, char *mode, char *txt);
int	sccs_cat(sccs *s, u32 flags, char *printOut);
int	sccs_delta(sccs *s, u32 flags, delta *d, MMAP *init, MMAP *diffs,
		   char **syms);
int	sccs_diffs(sccs *s, char *r1, char *r2,
		u32 flags, char kind, FILE *, char *not_used1, char *not_used2);
int	new_sccs_diffs(sccs *s, char *r1, char *r2,
		u32 flags, char kind, FILE *, char *lLabel, char *rLabel);
int	sccs_encoding(sccs *s, char *enc, char *comp);
int	sccs_get(sccs *s,
	    char *rev, char *mRev, char *i, char *x, u32 flags, char *out);
int	sccs_clean(sccs *s, u32 flags);
int	sccs_info(sccs *s, u32 flags);
int	sccs_prs(sccs *s, u32 flags, int reverse, char *dspec, FILE *out);
void	sccs_prsdelta(sccs *s, delta *d, int flags, const char *dspec, FILE *out);
delta	*sccs_getrev(sccs *s, char *rev, char *date, int roundup);
delta	*sccs_findDelta(sccs *s, delta *d);
sccs	*sccs_init(char *filename, u32 flags, project *proj);
sccs	*sccs_restart(sccs *s);
void	sccs_free(sccs *);
void	sccs_freetree(delta *);
void	sccs_close(sccs *);
char	**sccs_files(char **, int);
u16	sccs_nextlod(sccs *s);
int	sccs_smoosh(char *left, char *right);
delta	*sccs_parseArg(delta *d, char what, char *arg, int defaults);
void	sccs_whynot(char *who, sccs *s);
int	sccs_addSym(sccs *, u32 flags, char *);
void	sccs_ids(sccs *s, u32 flags, FILE *out);
int	sccs_hasDiffs(sccs *s, u32 flags);
void	sccs_print(delta *d);
delta	*sccs_getInit(sccs *s, delta *d, MMAP *f, int patch,
		      int *errorp, int *linesp, char ***symsp);
delta	*sccs_ino(sccs *);
int	sccs_rmdel(sccs *s, delta *d, u32 flags);
int	sccs_stripdel(sccs *s, char *who);
int	sccs_getdiffs(sccs *s, char *rev, u32 flags, char *printOut);
void	sccs_pdelta(sccs *s, delta *d, FILE *out);
char	*sccs_root(sccs *s);
int	sccs_cd2root(sccs *, char *optional_root);
delta	*sccs_key2delta(sccs *sc, char *key);
char	*sccs_impliedList(sccs *s, char *who, char *base, char *rev);
int	sccs_sdelta(sccs *s, delta *, char *);
void	sccs_perfile(sccs *, FILE *);
sccs	*sccs_getperfile(MMAP *, int *);
char	*sccs_gethost(void);
int	sccs_getComments(char *, char *, delta *);
int	sccs_getHostName(char *, char *, delta *);
int	sccs_getUserName(char *, char *, delta *);
void	sfileUnget(void);
char	*sfileNext(void);
char	*sfileRev(void);
char	*sfileFirst(char *cmd, char **Av, int Flags);
void	sfileDone(void);
void	rangeReset(sccs *sc);
int	rangeAdd(sccs *sc, char *rev, char *date);
int	tokens(char *s);
delta	*findrev(sccs *, char *);
delta	*sccs_findKey(sccs *, char *);
delta	*sccs_dInit(delta *, char, sccs *, int);
char	*sccs_gethost(void);
char	*sccs_getuser(void);
int	sccs_markMeta(sccs *);

delta	*modeArg(delta *d, char *arg);
FILE	*fastPopen(const char*, const char*);
int	fastPclose(FILE*);
char    *fullname(char *, int);
char	chop(register char *s);
int	sccs_filetype(char *name);
void	concat_path(char *, char *, char *);
void	cleanPath(char *path, char cleanPath[]);
int	isdir(char *s);
int	isreg(char *s);
int	isValidHost(char *h);
int	isValidUser(char *u);
int	readable(char *f);
void 	randomBits(char *);
int	samepath(char *a, char *b);
int	writable(char *f);
int	executable(char *f);
char	*basenm(char *);
char	*sccs2name(char *);
char	*name2sccs(char *);
int	diff(char *lfile, char *rfile, char kind, char *out);
char	**addLine(char **space, char *line);
void	freeLines(char **space);
int	roundType(char *r);
int	check_gfile(sccs*, int);
void	platformSpecificInit(char *);
MDBM	*loadDB(char *file, int (*want)(char *), int style);
delta 	*mkOneZero(sccs *s);
#ifndef ANSIC
int     sig(int, int);
#endif
int	csetIds(sccs *cset, char *rev);
void	sccs_fixDates(sccs *);
void	sccs_mkroot(char *root);
char	*sPath(char *name, int isDir);
delta	*sccs_next(sccs *s, delta *d);
int	sccs_reCache(void);
int	sccs_meta(sccs *s, delta *parent, MMAP *initFile);
int	sccs_resolveFiles(sccs *s);
sccs	*sccs_keyinit(char *key, u32 flags, project *p, MDBM *idDB);
delta	*sfind(sccs *s, ser_t ser);
int	sccs_lock(sccs *, char);
int	sccs_unlock(sccs *, char);
char 	*sccs_iskeylong(char *key);
#ifdef	PURIFY_FILES
MMAP	*purify_mopen(char *file, char *mode, char *, int);
void	purify_mclose(MMAP *, char *, int);
#else
MMAP	*mopen(char *file, char *mode);
void	mclose(MMAP *);
#endif
char	*mnext(MMAP *);
int	mpeekc(MMAP *);
void	mseekto(MMAP *m, off_t off);
off_t	mtell(MMAP *m);
MMAP	*mrange(char *start, char *stop, char *mode);
int	linelen(char *s);
char	*mkline(char *mmap);
int	mkdirp(char *dir);
int	mkdirf(char *file);
int	gettemp(char *file, const char *template);
void	explodeKey(char *key, char *parts[6]);
int	smartUnlink(char *name);
int	smartRename(char *old, char *new);
void	concat_path(char *buf, char *first, char *second);
void	free_pfile(pfile *pf);
delta	*sccs_kid(sccs *s, delta *d);  /* In range.c */
int	sccs_morekids(delta *d, int bk_mode);
int	exists(char *file);
int	emptyDir(char *dir);
char	*dirname(char *path);
int	fileCopy(char *from, char *to);
off_t	size(char *s);
int	gone(char *key, MDBM *db);
int	sccs_mv(char *name, char *dest, int isDir, int isDelete);
delta	*sccs_gca(sccs *, delta *l, delta *r, int best);
char	*_relativeName(char *gName,
	    int isDir, int withsccs, int mustHaveRmarker, char *root);
void	rcs(char *cmd, int argc, char **argv);
char	*findBin();
int	uniq_lock(void);
int	uniq_unlock(void);
int	uniq_open(void);
time_t	uniq_time(char *key);
time_t	uniq_drift();
int	uniq_update(char *key, time_t t);
int	uniq_root(char *key);
int	uniq_close(void);

typedef	char **globv;
globv	read_globs(FILE *f, globv oldglobs);
char	*match_globs(char *string, globv globs);
void	free_globs(globv globs);

#endif	/* _SCCS_H_ */
