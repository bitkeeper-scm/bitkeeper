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
#define	INIT_NOSTAT	0x01000000	/* do not look for {p,x,z,c} files */
#define	INIT_HAScFILE	0x02000000	/* has c.file */
#define	INIT_HASgFILE	0x04000000	/* has g.file */
#define	INIT_HASpFILE	0x08000000	/* has p.file */
#define	INIT_HASxFILE	0x00100000	/* has x.file */
#define	INIT_HASzFILE	0x00200000	/* has z.file */
#define	INIT_ONEROOT	0x00400000	/* one root mode i.e not split root */

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
#define	GET_ALIGN	0x00010000	/* nicely align prefix output */
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
#define GET_DIFFTOT	0x00000800	/* hasDiffs() false if !TOT */

#define	CLEAN_UNEDIT	0x10000000	/* clean -u: discard changes */
#define CLEAN_SHUTUP	0x20000000	/* clean -Q: quiet mode */

#define	DELTA_AUTO	0x10000000	/* delta -a: auto check-in mode */
#define	DELTA_SAVEGFILE	0x20000000	/* delta -n: save edited gfile */
#define	DELTA_DONTASK	0x40000000	/* don't ask for comments */
#define	DELTA_PATCH	0x80000000	/* delta -R: respect rev */
#define	DELTA_EMPTY	0x01000000	/* initialize with empty file */
#define	DELTA_FORCE	0x02000000	/* delta -f: force a delta */
#define	DELTA_HASH	0x04000000	/* treat as hash (MDBM) file */
#define	DELTA_NOPENDING	0x08000000	/* don't create pending marker */

#define	ADMIN_FORMAT	0x10000000	/* check file format (admin) */
#define	ADMIN_ASCII	0x20000000	/* check file format (admin) */
#define	ADMIN_TIME	0x40000000	/* warn about time going backwards */
#define	ADMIN_SHUTUP	0x80000000	/* don't be noisy about bad revs */
#define	ADMIN_BK	0x01000000	/* check BitKeeper invariants */
#define	ADMIN_GONE	0x02000000	/* check integrity w/o GONE deltas */
#define	ADMIN_ADD1_0	0x04000000	/* insert a 1.0 delta */

#define	ADMIN_CHECKS	(ADMIN_FORMAT|ADMIN_ASCII|ADMIN_TIME|ADMIN_BK)

#define	PRS_META	0x10000000	/* show metadata */
#define	PRS_SYMBOLIC	0x20000000	/* show revs as beta1, etc. Not done */
#define	PRS_PATCH	0x40000000	/* print in patch format */
#define PRS_ALL		0x80000000	/* scan all revs, not just type D */
#define	PRS_GRAFT	0x01000000	/* put the perfile in the patch */
#define	PRS_LF		0x02000000	/* terminate non-empty output with LF */
#define	PRS_LFLF	0x04000000	/* terminate non-empty record with LF */
#define	PRS_PLACEHOLDER	0x08000000	/* make a place holder patch */
#define	PRS_COMPAT	0x00100000	/* for makepatch -C, send old tags */

#define SINFO_TERSE	0x10000000	/* print in terse format: sinfo -t */

/*
 * flags passed to sccs_lod()
 */
#define LOD_NEW		0x10000000	/* Setup a new LOD on any ChangeSet */
#define LOD_CHECK	0x20000000	/* Check and fix all LOD settings */
#define LOD_NORENAME	0x40000000	/* Skip the renaming part */
#define LOD_RENUMBER	0x80000000	/* Fix possible lod renumber errors */
#define LOD_CREATE	0x01000000	/* Create a new lod on existing */

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
#define S_LOCKED	(S_SFILE|S_PFILE)
#define	S_ZFILE		0x00000008	/* SCCS/z.file exists */
#define	S_SOPEN		0x00000010	/* s->sfile is open */
#define	S_WARNED	0x00000020	/* error message already sent */
#define	S_RCS		0x00000040	/* expand RCS keywords */
#define	S_EOLN_NATIVE	0x00000080	/* use eoln native to this OS */
#define	S_EXPAND1	0x00000100	/* expand first line of keyowrds only */
#define	S_CHMOD		0x00000200	/* change the file back to 0444 mode */
#define	S_YEAR4		0x00000400	/* print out the year as 4 digits */
#define	S_BADREVS	0x00000800	/* has corrupted revisions */
#define	S_BIGPAD	0x00001000	/* admin -B: make the landing pad big */
/*			0x00002000	AVAILABLE */
#define	S_BITKEEPER	0x00004000	/* X_BITKEEPER flag */
#define	S_CSET		0x00008000	/* this is a changeset file */
/* AVALIABLE		0x00010000	*/
#define S_MAPPRIVATE	0x00020000	/* hack for Samba */
#define S_READ_ONLY	0x00040000	/* force read only mode */
#define	S_RANGE2	0x00080000	/* second call for date|rev range */
/*	AVAILABLE	0x00100000	   patch checksum mismatch */
#define S_ISSHELL	0x00200000	/* this is a shell script */
#define	S_SET		0x00400000	/* the tree is marked with a set */
#define	S_CSETMARKED	0x00800000	/* X_CSETMARKED match */
#define S_CACHEROOT	0x01000000	/* don't free the root entry */
#define	S_LONGKEY	0x02000000	/* all keys are version 2 format */
#define	S_HASH		0x04000000	/* this file is an MDBM file */
#define	S_FAKE_1_0	0x08000000	/* the 1.0 delta is a fake */
#define	S_SAVEPROJ	0x10000000	/* do not free the project struct */
#define	S_SCCS		0x20000000	/* expand SCCS keywords */
#define	S_SINGLE	0x40000000	/* inherit user/host */
#define	S_LOGS_ONLY	0x80000000	/* this is a logging repository */
#define S_XFLAGS	(S_RCS|S_YEAR4|S_ISSHELL|S_EXPAND1|S_HASH|\
			 S_SCCS|S_SINGLE|S_EOLN_NATIVE)

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
#define	X_RCS		0x00000002	/* RCS keywords */
#define	X_YEAR4		0x00000004	/* 4 digit years */
#define	X_ISSHELL	0x00000008	/* This is a shell script */
#define	X_EXPAND1	0x00000010	/* Expand first line of keywords only */
#define	X_CSETMARKED	0x00000020	/* ChangeSet boundries are marked */
#define	X_HASH		0x00000040	/* mdbm file */
#define	X_SCCS		0x00000080	/* SCCS keywords */
#define	X_SINGLE	0x00000100	/* single user, inherit user/host */
#if 0
/* Do not re-use this bit until we are sure no production repository use it. */
/* This is a undocumented feature shiped in release pre2-2.0		     */
#define	X_ALWAYS_EDIT	0x00000200	/* stays in edit mode after delta/ci */
#endif
#define	X_LOGS_ONLY	0x00000400	/* this is a logging repository */
#define	X_EOLN_NATIVE	0x00000800	/* use eoln native to this OS */
#define	X_LONGKEY	0x00001000	/* all keys are long format */
					/* flags which can be changed */
#define X_XFLAGS	(X_RCS|X_YEAR4|X_ISSHELL|X_EXPAND1|X_HASH|\
			 X_SCCS|X_SINGLE|X_EOLN_NATIVE)

/*
 * Encoding flags.
 * Bit 0 and 1 are data encoding (bit 1 is reserved for future use)
 * Bit 2 is compression mode (gzip or none)
 */
#define E_DATAENC	0x3
#define E_COMP		0x4

#define	E_ASCII		0		/* no encoding */
#define	E_UUENCODE	1		/* uuenecode it (traditional) */
#define	E_GZIP		4		/* gzip the data */

#define	E_BINARY	E_UUENCODE	/* default binary encoding */

#define	HAS_GFILE(s)	((s)->state & S_GFILE)
#define	HAS_PFILE(s)	((s)->state & S_PFILE)
#define	HAS_ZFILE(s)	((s)->state & S_ZFILE)
#define	HAS_SFILE(s)	((s)->state & S_SFILE)
#define	BEEN_WARNED(s)	((s)->state & S_WARNED)
#define	IS_WRITABLE(s)	((s)->mode & 0200)
#define IS_EDITED(s)	((((s)->state&S_EDITED) == S_EDITED) && IS_WRITABLE(s))
#define IS_LOCKED(s)	(((s)->state&S_LOCKED) == S_LOCKED)
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
#define	D_NONEWLINE	0x00000800	/* this delta has no trailing newline */
#define	D_META		0x00001000	/* this is a metadata removed delta */
#define	D_SYMBOLS	0x00002000	/* delta has one or more symbols */
#define	D_DUPCSETFILE	0x00004000	/* this changesetFile is shared */
#define	D_VISITED	0x00008000	/* and had a nice cup of tea */
#define	D_CKSUM		0x00010000	/* delta has checksum */
#define	D_MERGED	0x00020000	/* set on branch tip which is merged */
#define	D_GONE		0x00040000	/* this delta is gone, don't print */
#define	D_PLACEHOLDER	0x00080000	/* metadata only, no contents */
#define	D_NO_TRANSMIT	0x00100000	/* do not transmit - not used yet */
/*			0x00?00000	   AVAILABLE */
#define	D_ICKSUM	0x01000000	/* use checksum from init file */
#define	D_MODE		0x02000000	/* permissions in d->mode are valid */
#define	D_SET		0x04000000	/* range.c: marked as part of a set */
#define	D_CSET		0x08000000	/* this delta is marked in cset file */
#define D_DUPLINK	0x10000000	/* this symlink pointer is shared */
#define	D_LOCAL		0x20000000	/* for resolve; this is a local delta */
#define D_XFLAGS	0x40000000	/* delta has updated file flags */
#define D_TEXT		0x80000000	/* delta has updated text */
#define D_DT_ALL	(D_PLACEHOLDER|D_NO_TRANSMIT)

/*
 * Bits for the per delta d flag in the delta table. (^AcE d 0x????)
 *
 * Nota bene: these can not change once the product is shipped.  Ever.
 * They are stored on disk.
 */
#define DT_PLACEHOLDER	0x00000001	/* metadata only, no contents */
#define DT_NO_TRANSMIT	0x00000002	/* do not transmit - not used yet */
#define DT_ALL		(DT_PLACEHOLDER|DT_NO_TRANSMIT)

/*
 * Flags for command log
 */
#define CMD_BYTES	0x00000001	/* log command byte count */
#define CMD_FAST_EXIT	0x00000002	/* exit when done */
#define CMD_WRLOCK	0x00000004	/* write lock */
#define CMD_RDLOCK	0x00000008	/* read lock */
#define CMD_WRUNLOCK	0x00000010	/* write unlock */
#define CMD_RDUNLOCK	0x00000020	/* read unlock */

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
#define DB_NODUPS       0x01		/* keys must be unique */
#define DB_USEFIRST     0x02		/* use the first key found */
#define DB_USELAST      0x04		/* use the last key found */
#define	DB_KEYSONLY	0x08		/* boolean hashes */
#define	DB_NOBLANKS	0x10		/* keys must have values or skip */
#define	DB_KEYFORMAT	0x20		/* key/value are u@h|path|date|cksum */

#define	MAXREV	24	/* 99999.99999.99999.99999 */

#define	OPENLOG_HOME	"bitmover.com"
#define	OPENLOG_URL	"http://www.openlogging.org:80///LOG_ROOT///" 
#define	OPENLOG_IP	"http://208.184.147.196:80///LOG_ROOT///" 
#define	BK_WEBMAIL_URL	"http://www.bitkeeper.com:80"
#define	WEB_BKD_CGI	"web_bkd"
#define	WEB_MAIL_CGI	"web_mail"
#define	SCCSTMP		"SCCS/T.SCCSTMP"
#define	BKTMP		"BitKeeper/tmp"
#define	BKROOT		"BitKeeper/etc"
#define BKMASTER	"BitKeeper/etc/.master"
#define	GONE		"BitKeeper/etc/gone"
#define CSETS_IN	"BitKeeper/etc/csets-in"
#define CSETS_OUT	"BitKeeper/etc/csets-out"
#define	SGONE		"BitKeeper/etc/SCCS/s.gone"
#define	CHANGESET	"SCCS/s.ChangeSet"
#define	GCHANGESET	"ChangeSet"
#define	LOGGING_OK	"BitKeeper/etc/SCCS/s.logging_ok"
#define	GLOGGING_OK	"BitKeeper/etc/logging_ok"
#define	IDCACHE		"BitKeeper/etc/SCCS/x.id_cache"
#define	IDCACHE_LOCK	"BitKeeper/etc/SCCS/z.id_cache"
#define	TMP_MODE	0666
#define	GROUP_MODE	0664

#define	UNKNOWN_USER	"anon"
#define	UNKNOWN_HOST	"nowhere"

#define BK_FREE		0
#define BK_BASIC	1
#define BK_PRO		2

#define BKOPT_WEB	0x0001
#define BKOPT_ALL	0xffff

#define	CNTLA_ESCAPE	'\001'	/* escape character for ^A is also a ^A */
#define	isData(buf)	((buf[0] != '\001') || \
			    ((buf[0] == CNTLA_ESCAPE) && (buf[1] == '\001')))
#define	seekto(s,o)	s->where = (s->mmap + o)
#define	eof(s)		((s->encoding & E_GZIP) ? \
			    zeof() : (s->where >= s->mmap + s->size))
#define	new(p)		p = calloc(1, sizeof(*p))

typedef	unsigned short	ser_t;
typedef	unsigned short	sum_t;
typedef	unsigned int	u32;
typedef	unsigned short	u16;
typedef	unsigned char	u8;
typedef	char		**globv;

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
	char	*rev;			/* revision number */
	char	*sdate;			/* ascii date in local time, i.e.,
					 * 93/07/25 21:14:11 */
	char	*user;			/* user name of delta owner */
	char	type;			/* Delta or removed delta */
	ser_t	serial;			/* serial number of this delta */
	ser_t	pserial;		/* serial number of parent */
	ser_t	*include;		/* include serial #'s */
	ser_t	*exclude;		/* exclude serial #'s */
	ser_t	*ignore;		/* ignore serial #'s */
	char	**mr;			/* MR's are just passed through */
	char	**comments;		/* Comment log */
	/* New stuff in lm's sccs */
	ser_t	ptag;			/* parent in tag graph */
	ser_t	mtag;			/* merge parent in tag graph */
	char	**text;			/* descriptive text log */
	char	*hostname;		/* hostname where revision was made */
	char	*pathname;		/* pathname to the file */
	char	*zone;			/* 08:00 is time relative to GMT */
	char	*csetFile;		/* id for ChangeSet file */
	char	*random;		/* random bits for file ID */
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
	u32	flags;			/* per delta flags */
	u32	symLeaf:1;		/* if set, I'm a symbol with no kids */
	u32	symGraph:1;		/* if set, I'm a symbol in the graph */
	u32	published:1;	
	u32	ptype:1;	
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
	u32	left:1;			/* present in left branch */
	u32	right:1;		/* present in right branch */
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

/*
 * Map used by things like serial map,
 * used to calculate from graph to set
 */
#define	S_INC	1
#define	S_EXCL	2
#define	S_PAR	4

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
	char	*csetFile;	/* Root key of ChangeSet file */
	MDBM	*config;	/* config DB */
} project;

extern	project	*bk_proj;	/* bk.c sets this up */
extern	jmp_buf	exit_buf;
extern	char *upgrade_msg;

#define	exit(e)	longjmp(exit_buf, e + 1000)

#define	READER_LOCK_DIR	"BitKeeper/readers"
#define	WRITER_LOCK_DIR	"BitKeeper/writer"
#define	WRITER_LOCK	"BitKeeper/writer/lock"

#define	LOCK_WR_BUSY	"ERROR-Unable to lock repository for update."
#define	LOCK_RD_BUSY	"ERROR-Can't get read lock on the repository."
#define	LOCK_PERM	"ERROR-Lock fail: possible permission problem."
#define	LOCK_UNKNOWN	"ERROR-Unknown lock error."

/*
 * Bumped whenever we change any file format.
 * XXX - this isn't timesafe.  It's not clear it wants to be, it's a file
 * format thing, not a repository thing.  It makes for BK itself, that's all.
 *
 * 2 - bumped to invalidate old binaries with bad date code.
 * 3 - because random bits can now be on a per delta basis.
 * 4 - added X_LOGS_ONLY, DT_PLACEHOLDER & DT_NO_TRANSMIT flags
 */
#define	SCCS_VERSION_COMPAT	3	/* for opull/opush */
#define	SCCS_VERSION		4

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
	char	*spathname;	/* current spathname in view or not in view */
	char	**usersgroups;	/* lm, beth, staff, etc */
	int	encoding;	/* ascii, uuencode, gzip, etc. */
	char	**flags;	/* flags in the middle that we didn't grok */
	char	**text;		/* descriptive text */
	int	state;		/* GFILE/SFILE etc */
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
	int	version;	/* file format version */
	int	userLen;	/* maximum length of any user name */
	int	revLen;		/* maximum length of any rev name */
	u32	cksumok:1;	/* check sum was ok */
	u32	cksumdone:1;	/* check sum was checked */
	u32	grafted:1;	/* file has grafts */
	u32	bad_dsum:1;	/* patch checksum mismatch */
	u32	io_error:1;	/* had an output error, abort */
	u32	io_warned:1;	/* we told them about the error */
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
 * How to go from the RESYNC dir to the root dir and back again.
 * Lots of code does not use these but should so we can move
 * RESYNC to BitKeeper/RESYNC and not pollute the namespace.
 */
#define	RESYNC2ROOT	".."	  /* chdir(RESYNC2ROOT) gets you to root */
#define	ROOT2RESYNC	"RESYNC"  /* chdir(ROOT2RESYNC) gets you back */
#define	PENDING2ROOT	".."	  /* chdir(PENDING2ROOT) gets you to root */
#define	ROOT2PENDING	"PENDING" /* chdir(ROOT2PENDING) gets you back */

/*
 * Patch file format strings.
 *
 * 0.8 = 0x flags and V version.
 * 0.9 = Positive termination.
 * 1.0 = state machine in takepatch
 * 1.1 = state machine in adler32
 * 1.2 = Changed random bits to be per delta;
 *	 Add grafted file support.
 * 1.3 = add logging patch type and tag graph.
 */
#define	PATCH_COMPAT	"# Patch vers:\t1.2\n"
#define PATCH_CURRENT	"# Patch vers:\t1.3\n"

#define PATCH_LOGGING	"# Patch type:\tLOGGING\n"
#define PATCH_REGULAR	"# Patch type:\tREGULAR\n"

#define	BK_RELEASE	"2.O"	/* this is lame, we need a sccs keyword */

/*
 * Patch envelops for adler32.
 */
#define	PATCH_DIFFS	"\001 Diff start\n"
#define	PATCH_PATCH	"\001 Patch start\n"
#define	PATCH_END	"\001 End\n"
#define	PATCH_ABORT	"\001 Patch abort\n"
#define	PATCH_OK	"\001 Patch OK\n"

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

/*
 * command struct for bk front end
 */
struct command
{
        char *name;
        int (*func)(int, char **);
};      

/*
 * BK "URL" formats are:
 *	bk://user@host:port/pathname
 *	user@host:pathname
 * In most cases, everything except the pathname is optional.
 */
typedef struct {
	u16	port;		/* remote port if set */
	u16	loginshell:1;	/* if set, login shell is the bkd */
	u16	httpd:1;	/* if set, httpd is the bkd */
	u16	trace:1;	/* for debug, trace send/recv msg */
	u16	isSocket:1;	/* if set, rfd and wfd is socket */
	int	rfd;		/* read fd for the remote channel */
	int	wfd;		/* write fd for the remote channel */
	char	*user;		/* remote user if set */
	char	*host;		/* remote host if set */
	char	*path;		/* pathname (must be set) */
} remote;


int	sccs_admin(sccs *sc, delta *d, u32 flgs, char *encoding, char *compress,
	    admin *f, admin *l, admin *u, admin *s, char *mode, char *txt);
int	sccs_cat(sccs *s, u32 flags, char *printOut);
int	sccs_delta(sccs *s, u32 flags, delta *d, MMAP *init, MMAP *diffs,
		   char **syms);
int	sccs_diffs(sccs *s, char *r1, char *r2, u32 flags, char kind, FILE *);
int	sccs_encoding(sccs *s, char *enc, char *comp);
int	sccs_get(sccs *s,
	    char *rev, char *mRev, char *i, char *x, u32 flags, char *out);
int	sccs_clean(sccs *s, u32 flags);
int	sccs_info(sccs *s, u32 flags);
int	sccs_prs(sccs *s, u32 flags, int reverse, char *dspec, FILE *out);
int	sccs_prsdelta(sccs *s, delta *d, int flags, const char *dspec, FILE *out);
delta	*sccs_getrev(sccs *s, char *rev, char *date, int roundup);
delta	*sccs_findDelta(sccs *s, delta *d);
sccs	*sccs_init(char *filename, u32 flags, project *proj);
sccs	*sccs_restart(sccs *s);
void	sccs_free(sccs *);
void	sccs_freetree(delta *);
void	sccs_close(sccs *);
sccs	*sccs_csetInit(u32 flags, project *proj);
char	**sccs_files(char **, int);
ser_t	sccs_nextlod(sccs *s);
int	sccs_smoosh(char *left, char *right);
delta	*sccs_parseArg(delta *d, char what, char *arg, int defaults);
void	sccs_whynot(char *who, sccs *s);
int	sccs_addSym(sccs *, u32 flags, char *);
void	sccs_ids(sccs *s, u32 flags, FILE *out);
int	sccs_hasDiffs(sccs *s, u32 flags, int inex);
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
void	sccs_shortKey(sccs *s, delta *, char *);
void	sccs_perfile(sccs *, FILE *);
sccs	*sccs_getperfile(MMAP *, int *);
char	*sccs_gethost(void);
int	sccs_getComments(char *, char *, delta *);
int	sccs_getHostName(delta *);
int	sccs_getUserName(delta *);
MDBM    *sccs_keys2mdbm(FILE *f);
void	sfileUnget(void);
char	*sfileNext(void);
char	*sfileRev(void);
char	*sfileFirst(char *cmd, char **Av, int Flags);
void	sfileDone(void);
void	rangeReset(sccs *sc);
int	rangeAdd(sccs *sc, char *rev, char *date);
int	tokens(char *s);
delta	*findrev(sccs *, char *);
delta	*sccs_top(sccs *);
delta	*sccs_findKey(sccs *, char *);
delta	*sccs_dInit(delta *, char, sccs *, int);
char	*sccs_gethost(void);
char	*sccs_realhost(void);
char	*sccs_getuser(void);
char	*sccs_realuser(void);
int	sccs_markMeta(sccs *);

delta	*modeArg(delta *d, char *arg);
FILE	*fastPopen(const char*, const char*);
int	fastPclose(FILE*);
char    *fullname(char *, int);
int	fileType(mode_t m);
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
int	check_gfile(sccs*, int);
void	platformSpecificInit(char *);
MDBM	*loadDB(char *file, int (*want)(char *), int style);
delta 	*mkOneZero(sccs *s);
#ifndef ANSIC
int     sig(int, int);
#endif
int	sigcaught(int sig);
int	csetIds(sccs *cset, char *rev);
int	csetIds_merge(sccs *cset, char *rev, char *merge);
int	cset_inex(int flags, char *op, char *revs);
void	sccs_fixDates(sccs *);
int	sccs_getxflags(delta *d);
void	sccs_mkroot(char *root);
char	*sccs_nivPath(sccs *s);
int	sccs_parent_revs(sccs *s, char *rev, char **revP, char **revM);
char	*sccs_setpathname(sccs *s);
char	*sPath(char *name, int isDir);
delta	*sccs_next(sccs *s, delta *d);
int	sccs_reCache(int quiet);
int	sccs_meta(sccs *s, delta *parent, MMAP *initFile, int fixDates);
int	sccs_resolveFiles(sccs *s);
sccs	*sccs_keyinit(char *key, u32 flags, project *p, MDBM *idDB);
delta	*sfind(sccs *s, ser_t ser);
int	sccs_lock(sccs *, char);	/* respects repo locks */
int	sccs_lockfile(char *lockfile, int tries);	/* works in NFS */
int	sccs_unlock(sccs *, char);
char	*sccs_utctime(delta *d);
int	sccs_setlod(char *rev, u32 flags);
void	sccs_renumber(sccs *s, ser_t nextlod, ser_t thislod, MDBM *lodDb,
	    char *base, u32 flags);
char 	*sccs_iskeylong(char *key);
#ifdef	PURIFY_FILES
MMAP	*purify_mopen(char *file, char *mode, char *, int);
void	purify_mclose(MMAP *, char *, int);
#else
MMAP	*mopen(char *file, char *mode); void	mclose(MMAP *);
#endif
char	*mnext(MMAP *);
int	mcmp(MMAP *, char *);
int	mpeekc(MMAP *);
void	mseekto(MMAP *m, off_t off);
off_t	mtell(MMAP *m);
size_t	msize(MMAP *m);
MMAP	*mrange(char *start, char *stop, char *mode);
int	linelen(char *s);
void 	license();
char	*mkline(char *mmap);
int	mkdirp(char *dir);
int	mkdirf(char *file);
char    *mode2FileType(mode_t m);
int	gettemp(char *file, const char *template);
int	getline(int in, char *buf, int size);
void	explodeKey(char *key, char *parts[6]);
int	smartUnlink(char *name);
int	smartRename(char *old, char *new);
void	concat_path(char *buf, char *first, char *second);
void	free_pfile(pfile *pf);
int	sccs_read_pfile(char *who, sccs *s, pfile *pf);
int	sccs_rewrite_pfile(sccs *s, pfile *pf);
int	sccs_isleaf(sccs *s, delta *d);
int	exists(char *file);
int	emptyDir(char *dir);
char	*dirname(char *path);
int	fileCopy(char *from, char *to);
off_t	size(char *s);
int	sameFiles(char *file1, char *file2);
int	gone(char *key, MDBM *db);
int	sccs_mv(char *name, char *dest, int isDir, int isDelete);
delta	*sccs_gca(sccs *, delta *l, delta *r, char **i, char **x, int best);
char	*_relativeName(char *gName, int isDir,
		int withsccs, int mustHaveRmarker, project *proj, char *root);
void	rcs(char *cmd, int argc, char **argv);
char	*findBin();
project	*chk_proj_init(sccs *s, char *file, int line);
void	proj_free(project *p);
int 	prompt(char *msg, char *buf);
void	parse_url(char *url, char *host, char *path);
char	*sccs_Xfile(sccs *s, char type);
int	unique(char *key);
int	uniq_lock(void);
int	uniq_unlock(void);
int	uniq_open(void);
time_t	uniq_drift();
int	uniq_update(char *key, time_t t);
int	uniq_close(void);
time_t	sccs_date2time(char *date, char *zone);
void	cd2root();
pid_t	mail(char *to, char *subject, char *file);
void	logChangeSet(int, char *rev, int quiet);
char	*getlog(char *u, int q);
int	setlog(char *u);
int	connect_srv(char *srv, int port, int trace);
int	checkLog(int quiet, int resync);
int	get(char *path, int flags, char *output);
int	gethelp(char *helptxt, char *help_name, char *bkarg, char *prefix, FILE *f);
int	is_open_logging(char *logaddr);
void	status(int verbose, FILE *out);
void	notify();
char	*logAddr();
char	*package_name();
int	bkusers(int countOnly, int raw, char *prefix, FILE *out);
globv	read_globs(FILE *f, globv oldglobs);
int	match_one(char *string, char *glob);
char	*match_globs(char *string, globv globs);
void	free_globs(globv globs);
char	*prog2path(char *prog);
void	remark(int quiet);
int	readn(int from, char *buf, int size);
void	sendConfig(char *addr, char *rev);
void	send_request(int fd, char * request, int len);
int	writen(int to, char *buf, int size);
char	chop(register char *s);
int	mkdirp(char *dir);
int	mkdirf(char *file);
long	almostUnique(int harder);
int	repository_locked(project *p);
int	repository_lockers(project *p);
int	repository_locker(char type, pid_t pid, char *host);
int	repository_cleanLocks(project *p, int r, int w, int force, int verbose);
int	repository_rdlock(void);
int	repository_wrlock(void);
int	repository_rdunlock(int force);
int	repository_wrunlock(int force);
int	isValidLock(char, pid_t, char *);
void	comments_save(char *s);
int	comments_got(void);
void	comments_done(void);
delta	*comments_get(delta *d);
void	host_done();
delta	*host_get(delta *);
void	user_done();
delta	*user_get(delta *);
char	*shell();
struct	lod;
typedef struct lod lod_t;
lod_t	*lod_init(sccs *cset, char *lodname, u32 flags, char *who);
void	lod_free(lod_t *l);
int	lod_setlod(lod_t *l, sccs *s, u32 flags);
void	names_init(void);
int	names_rename(char *old_spath, char *new_spath, u32 flags);
void	names_cleanup(u32 flags);
int	bk_sfiles(char *opts, int ac, char **av);
int	outc(char c);
MDBM	*loadConfig(char *root, int convert);
int	ascii(char *file);
char	*sccs_rmName(sccs *s, int useCommonDir);
int	sccs_rm(char *name, char *del_name, int useCommonDir);
int	mkconfig(FILE *out);
int	config2logging(char *root);
int	logging(char *user, MDBM *configDB, MDBM *okDB);
void	do_prsdelta(char *file, char *rev, int flags, char *dspec, FILE *out);
char 	**get_http_proxy();
int	confirm(char *msg);
int	setlod_main(int ac, char **av);
MDBM *	loadOK();
void	config(FILE *f);
int	ok_commit(int l, int alreadyAsked);
int	cset_setup(int flags);
off_t	fsize(int fd);
char	*separator(char *);
int	trigger(char **av, char *when);
int	cmdlog_start(char **av, int want_http_hdr);
int	cmdlog_end(int ret, int flags);
off_t	get_byte_count();
void	save_byte_count(unsigned int byte_count);
int	bk_mode();
int	cat(char *file);
char	*bk_model(char *buf, int len);
char	*getHomeDir();
char	*age(time_t secs, char *space);
void	sortLines(char **);
	/* this must be the last argument to all calls to sys/sysio */
#define	SYS	(char*)0, 0xdeadbeef
int	sys(char *first, ...);
int	sysio(char *in, char *out, char *err, char *first, ...);
char	*sccs_zone();
MDBM	*sccs_tagConflicts(sccs *s);

int     http_connect(remote *r, char *cgi_script);
int     http_send(remote *, char *, size_t, size_t, char *, char *); 
char *	user_preference(char *what, char buf[MAXPATH]);
int	bktemp(char *buf);
char	*bktmpfile();	/* return a char* to a just created temp file */
void	updLogMarker(int ptype);
char	*getRealCwd(char *, size_t);
#endif	/* _SCCS_H_ */
