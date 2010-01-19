/* %W% Copyright (c) 1997-2001 Larry McVoy */
#ifndef	_SCCS_H_
#define	_SCCS_H_

#include "system.h"
#include "purify.h"

#define	mdbm_mem()	mdbm_open(NULL, 0, 0, GOOD_PSIZE)
#define	EACH_KV(d)	for (kv = mdbm_first(d); \
			    kv.key.dsize; kv = mdbm_next(d))
#define EACH_KEY(d)     for (k = mdbm_firstkey(d); \
                            k.dsize; k = mdbm_nextkey(d))

#define	MAXKEY	(MAXPATH + 512)
#define SPLIT_ROOT	/* enable split_root support */
#define BK_FS		'|' /* Field seperator used in file|rev names */

void	reserveStdFds(void);
struct tm *localtimez(time_t *timep, long *offsetp);

#ifdef	WIN32
#define	win32_close(s)	sccs_close(s)
#define	win32_open(s)	sccs_open(s, 0)
#else
#define	win32_close(s)
#define	win32_open(s)
#endif

#ifndef	NOPROC
void	rmdir_findprocs(void);
int	checking_rmdir(char *dir);
#endif

/*
 * Flags that modify some operation (passed to sccs_*).
 *
 * There are two ranges of values, 0x0000000f and 0xfffffff0.
 * The first is for flags which may be meaningful to all functions.
 * The second is for flags which are function specific.
 * Be VERY CAREFUL to not mix and match.  If I see a DELTA_ in sccs_get()
 * I will be coming after you with a blowtorch.
 * We've hit problems with flags being passed to sccs_init() being used for
 * lower level functions that want different flags.  See WACKGRAPH.
 */
#define	SILENT		0x00000001	/* do work quietly */
#define	PRINT		0x00000002	/* get/delta/clean [diffs] to stdout */
#define	NEWFILE		0x00000004	/* delta -i: create initial file */
#define	NEWCKSUM	0x00000008	/* Redo checksum */

#define	INIT_NOWARN	0x10000000	/* don't bitch about failed inits */
#define	INIT_NOCKSUM	0x20000000	/* don't do the checksum */
#define	INIT_FIXDTIME	0x40000000	/* use g file mod time as delat time */
#define	INIT_NOSTAT	0x01000000	/* do not look for {p,x,z,c} files */
#define	INIT_HAScFILE	0x02000000	/* has c.file */
#define	INIT_HASgFILE	0x04000000	/* has g.file */
#define	INIT_HASpFILE	0x08000000	/* has p.file */
#define	INIT_HASxFILE	0x00100000	/* has x.file */
#define	INIT_HASzFILE	0x00200000	/* has z.file */
#define	INIT_NOGCHK	0x00800000	/* do not fail on gfile checks */
#define	INIT_CHK_STIME	0x00010000	/* check that s.file <= gfile */
#define	INIT_WACKGRAPH	0x00020000	/* we're wacking the graph, no errors */
#define	INIT_MUSTEXIST	0x00040000	/* the sfile must exist or we fail */

/* shared across get/diffs/getdiffs */
#define	GET_EDIT	0x10000000	/* get -e: get for editting */
#define	GET_EXPAND	0x20000000	/* expand keywords */
#define	GET_REVNUMS	0x40000000	/* get -m: prefix each line with rev */
#define GET_USER	0x80000000	/* get -u: prefix with user name */
#define GET_SKIPGET	0x01000000	/* get -g: don't get the file */
#define	GET_NOREMOTE	0x02000000	/* do not go remote for BAM */
#define	GET_ASCII	0x04000000	/* Do not gunzip/uudecode */
#define	GET_LINENUM	0x08000000	/* get -N: show line numbers */
#define	GET_MODNAME	0x00100000	/* get -n: prefix with %M */
#define	GET_PREFIXDATE	0x00200000	/* get -d: show date */
#define	GET_SERIAL	0x00400000	/* serial annotate */
#define	GET_SHUTUP	0x00800000	/* quiet on certain errors */
#define	GET_ALIGN	0x00010000	/* nicely align prefix output */
#define	GET_FORCE	0x00020000	/* do it even with errors */
#define	GET_HEADER	0x00040000	/* diff: print header */
#define	DIFF_HEADER	GET_HEADER
#define	GET_DTIME	0x00080000	/* gfile get delta's mode time */
#define	GET_NOHASH	0x00001000	/* force regular file, ignore S_HASH */
#define	GET_HASHONLY	0x00002000	/* skip the file */
#define	GET_DIFFS	0x00004000	/* get -D, regular diffs */
#define	GET_BKDIFFS	0x00008000	/* get -DD, BK (rick's) diffs */
#define	GET_HASHDIFFS	0x00000100	/* get -DDD, 0a0 hash style diffs */
#define	GET_SUM		0x00000200	/* used to force dsum in getRegBody */
#define GET_NOREGET	0x00000400	/* get -S: skip gfiles that exist */
#define	GET_LINENAME	0x00000800	/* get -O: prefix with line name */
#define	GET_RELPATH	0x00000010	/* like GET_MODNAME but full relative */
#define	GET_HASH	0x00000020	/* force hash file, ignore ~S_HASH */
#define	GET_SEQ		0x00000040	/* sccs_get: prefix with sequence no */
#define	GET_COMMENTS	0x00000080	/* diffs -H: prefix diffs with hist */
#define	DIFF_COMMENTS	GET_COMMENTS
#define	GET_PREFIX	\
    (GET_REVNUMS|GET_USER|GET_LINENUM|GET_MODNAME|\
     GET_RELPATH|GET_PREFIXDATE|GET_SEQ|GET_LINENAME|GET_SERIAL)

#define CLEAN_SHUTUP	0x20000000	/* clean -Q: quiet mode */
#define	CLEAN_SKIPPATH	0x40000000	/* ignore path change; for log tree */
#define	CLEAN_CHECKONLY	0x80000000	/* don't delete gfile, just check */

#define	DELTA_AUTO	0x10000000	/* delta -a: auto check-in mode */
#define	DELTA_SAVEGFILE	0x20000000	/* delta -n: save edited gfile */
#define	DELTA_DONTASK	0x40000000	/* don't ask for comments */
#define	DELTA_PATCH	0x80000000	/* delta -R: respect rev */
#define	DELTA_EMPTY	0x01000000	/* initialize with empty file */
#define	DELTA_FORCE	0x02000000	/* delta -f: force a delta */
/* AVAILABLE		0x04000000	*/
#define	DELTA_NOPENDING	0x08000000	/* don't create pending marker */
#define	DELTA_CFILE	0x00100000	/* read cfile and do not prompt */
#define	DELTA_MONOTONIC	0x00200000	/* preserve MONOTONIC flag */

#define	ADMIN_FORMAT	0x10000000	/* check file format (admin) */
/* AVAILABLE		0x20000000	*/
#define	ADMIN_TIME	0x40000000	/* warn about time going backwards */
#define	ADMIN_SHUTUP	0x80000000	/* don't be noisy about bad revs */
#define	ADMIN_BK	0x01000000	/* check BitKeeper invariants */
#define	ADMIN_GONE	0x02000000	/* check integrity w/o GONE deltas */
#define	ADMIN_ADD1_0	0x04000000	/* insert a 1.0 delta */
#define	ADMIN_RM1_0	0x08000000	/* remove a 1.0 delta */
#define	ADMIN_OBSCURE	0x00100000	/* remove comments, obscure data */
#define	ADMIN_FORCE	0x00200000	/* use Z lock; for pull/cweave */
#define	ADMIN_NEWPATH	0x00400000	/* path changed, add a new null delta */
#define	ADMIN_DELETE	0x00800000	/* file deleted, add a new null delta */
#define	ADMIN_RMLICENSE	0x00010000	/* Obscure licenses in repo config */

#define	ADMIN_CHECKS	(ADMIN_FORMAT|ADMIN_TIME|ADMIN_BK)

#define	PRS_FORCE	0x10000000	/* ignore the D_SET/S_SET filter */
#define	PRS_SYMBOLIC	0x20000000	/* show revs as beta1, etc. Not done */
#define	PRS_PATCH	0x40000000	/* print in patch format */
#define PRS_ALL		0x80000000	/* scan all revs, not just type D */
#define	PRS_GRAFT	0x01000000	/* put the perfile in the patch */
#define	PRS_LF		0x02000000	/* terminate non-empty output with LF */
#define	PRS_FASTPATCH	0x04000000	/* print in fast patch format */

#define SINFO_TERSE	0x10000000	/* print in terse format: sinfo -t */

/*
 * flags passed to sfileFirst
 */
#define	SF_GFILE	0x00000001	/* gfile should be readable */
#define	SF_WRITE_OK	0x00000002	/* gfile should be writable */
#define	SF_NODIREXPAND	0x00000004	/* don't auto expand directories */
#define	SF_NOHASREVS	0x00000008	/* don't expect |rev on files */
#define	SF_SILENT	0x00000010	/* sfiles - don't complain */
#define	SF_NOCSET	0x00000020	/* do not autoexpand cset files */

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
#define	S_CHMOD		0x00000040	/* change the file back to 0444 mode */
#define	S_BADREVS	0x00000080	/* has corrupted revisions */
#define	S_available	0x00000100	/* OLD: make the landing pad big */
#define	S_CSET		0x00000200	/* this is a changeset file */
#define S_MAPPRIVATE	0x00000400	/* hack for hpux */
#define S_READ_ONLY	0x00000800	/* force read only mode */
#define	S_SET		0x00002000	/* the tree is marked with a set */
#define S_CACHEROOT	0x00004000	/* don't free the root entry */
#define	S_FAKE_1_0	0x00008000	/* the 1.0 delta is a fake */
#define S_CONFIG	0x00040000	/* this is a config file */
#define S_IMPORT	0x00080000	/* import mode */

#define	KEY_FORMAT2	"BK key2"	/* sym in csets created w/ long keys */

/*
 * Options to sccs_diffs()
 */
#define	DF_DIFF		0x00000001
#define	DF_SDIFF	0x00000002
#define	DF_CONTEXT	0x00000004
#define	DF_UNIFIED	0x00000008
#define	DF_RCS		0x00000010
#define	DF_IFDEF	0x00000020
#define	DF_GNUb		0x00000040
#define	DF_GNUB		0x00000080
#define	DF_GNUp		0x00000100
#define	DF_GNUw		0x00000200
#define	DF_GNUN		0x00000400

/*
 * Date handling.
 */
#define	ROUNDUP	1
#define	EXACT	0
#define	ROUNDDOWN -1
#define	DATE(d)		((d)->date ? (d)->date : getDate(d))
#define	CHKDATE(d)	unless ((d)->date || \
			    streq("70/01/01 00:00:00", (d)->sdate)) { \
				assert((d)->date); \
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
#define	X_SHELL		0x00000008	/* This is a shell script */
#define	X_EXPAND1	0x00000010	/* Expand first line of keywords only */
#define	X_CSETMARKED	0x00000020	/* ChangeSet boundries are marked */
#define	X_HASH		0x00000040	/* mdbm file */
#define	X_SCCS		0x00000080	/* SCCS keywords */
#define	X_SINGLE	0x00000100	/* OLD single user */
/*	X_DO_NOT_USE	0x00000200	   was used shortly, never reuse */
/* old  X_LOGS_ONLY	0x00000400	   this is a logging repository */
#define	X_EOLN_NATIVE	0x00000800	/* use eoln native to this OS */
#define	X_LONGKEY	0x00001000	/* all keys are long format */
#define	X_KV		0x00002000	/* key value file */
#define	X_NOMERGE	0x00004000	/* treat as binary even if ascii */
					/* flags which can be changed */
#define	X_MONOTONIC	0x00008000	/* file rolls forward only */
#define	X_EOLN_WINDOWS	0x00010000	/* produce \r\n line endings */
/* Note: X_EOLN_UNIX is fake, it does not go on disk */
#define	X_EOLN_UNIX	0x00020000	/* produce \n style endings */
#define	X_MAYCHANGE	(X_RCS | X_YEAR4 | X_SHELL | X_EXPAND1 | \
			X_SCCS | X_EOLN_NATIVE | X_KV | X_NOMERGE | \
			X_MONOTONIC | X_EOLN_WINDOWS | X_EOLN_UNIX)
/* default set of flags when we create a file */
#define	X_DEFAULT	(X_BITKEEPER|X_CSETMARKED|X_EOLN_NATIVE)
#define	X_REQUIRED	(X_BITKEEPER|X_CSETMARKED)

/* bits for the xflags checker - lovely having legacy files, eh? */
#define	XF_DRYRUN	0x0001	/* say what would be done */
#define	XF_STATUS	0x0002	/* just return status, for check/admin -h */

/*
 * Encoding flags.
 * Bit 0 and 1 are data encoding
 * Bit 2 is compression mode (gzip or none)
 */
#define E_DATAENC	0x3
#define E_COMP		0x4

#define	E_ASCII		0		/* no encoding */
#define	E_UUENCODE	1		/* uuenecode it (traditional) */
#define	E_BAM		2		/* store data in BAM pool */
#define	E_GZIP		4		/* gzip the data */

#define	HAS_GFILE(s)	((s)->state & S_GFILE)
#define	HAS_PFILE(s)	((s)->state & S_PFILE)
#define	HAS_ZFILE(s)	((s)->state & S_ZFILE)
#define	HAS_SFILE(s)	((s)->state & S_SFILE)
#define	BEEN_WARNED(s)	((s)->state & S_WARNED)
#define	WRITABLE(s)	((s)->mode & 0200)
#define EDITED(s)	((((s)->state&S_EDITED) == S_EDITED) && WRITABLE(s))
#define LOCKED(s)	(((s)->state&S_LOCKED) == S_LOCKED)
#define ASCII(s)	(((s)->encoding & E_DATAENC) == E_ASCII)
#define BINARY(s)	(((s)->encoding & E_DATAENC) != E_ASCII)
#define BAM(s)		(((s)->encoding & E_DATAENC) == E_BAM)
#define UUENCODE(s)	(((s)->encoding & E_DATAENC) == E_UUENCODE)
#define	CSET(s)		((s)->state & S_CSET)
#define	CONFIG(s)	((s)->state & S_CONFIG)
#define	READ_ONLY(s)	((s)->state & S_READ_ONLY)
#define	SET(s)		((s)->state & S_SET)
#define	IMPORT(s)	((s)->state & S_IMPORT)
#define	MK_GONE(s, d)	do {(s)->hasgone = 1; (d)->flags |= D_GONE;} while (0)

#define	GOODSCCS(s)	assert(s); unless ((s)->tree&&(s)->cksumok) return (-1)
#define	HASGRAPH(s)	((s)->tree)

#define	BITKEEPER(s)	((s)->bitkeeper)
#define	RCS(s)		((s)->xflags & X_RCS)
#define	YEAR4(s)	((s)->xflags & X_YEAR4)
#define	SHELL(s)	((s)->xflags & X_SHELL)
#define	EXPAND1(s)	((s)->xflags & X_EXPAND1)
#define	CSETMARKED(s)	((s)->xflags & X_CSETMARKED)
#define	HASH(s)		((s)->xflags & X_HASH)
#define	SCCS(s)		((s)->xflags & X_SCCS)
#define	EOLN_NATIVE(s)	((s)->xflags & X_EOLN_NATIVE)
#define	LONGKEY(s)	((s)->xflags & X_LONGKEY)
#define	KV(s)		((s)->xflags & X_KV)
#define	NOMERGE(s)	((s)->xflags & X_NOMERGE)
#define	MONOTONIC(s)	((s)->xflags & X_MONOTONIC)
#define	EOLN_WINDOWS(s)	((s)->xflags & X_EOLN_WINDOWS)

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
#define	D_RED		0x00008000	/* marker used in graph traeversal */
#define	D_CKSUM		0x00010000	/* delta has checksum */
#define	D_MERGED	0x00020000	/* set on branch tip which is merged */
#define	D_GONE		0x00040000	/* this delta is gone, don't print */
#define	D_BLUE		0x00080000	/* when you need two colors */
#define	D_ICKSUM	0x01000000	/* use checksum from init file */
#define	D_MODE		0x02000000	/* permissions in d->mode are valid */
#define	D_SET		0x04000000	/* range.c: marked as part of a set */
#define	D_CSET		0x08000000	/* this delta is marked in cset file */
#define D_DUPLINK	0x10000000	/* this symlink pointer is shared */
#define	D_LOCAL		0x20000000	/* for resolve; this is a local delta */
#define D_XFLAGS	0x40000000	/* delta has updated file flags */
#define D_TEXT		0x80000000	/* delta has updated text */

/*
 * Undo exit status for nothing to undo.
 */
#define	UNDO_ERR	1		/* exitcode for errors */
#define	UNDO_SKIP	2		// exitcode for early exit with no work

/*
 * exit status in the code, some of these values can also be the
 * return code from push_main().
 * Don't renumber these in the future, just add to them.
 */
typedef	enum {
	CLONE_OK = 0,
	CLONE_ERROR = 1,	/* other error */
	CLONE_EXISTS = 2,	/* remote repo exists already */
	CLONE_CONNECT = 5,	/* bkd_connect() failed */
	CLONE_CHDIR = 6,	/* chdir failure */
	CLONE_BADREV = 7,	/* rev not found */
} clonerc;

/*
 * Hash behaviour.  Bitmask.
 */
#define	DB_NODUPS       0x01		/* keys must be unique */
#define	DB_USEFIRST     0x02		/* use the first key found */
#define	DB_USELAST      0x04		/* use the last key found */
#define	DB_KEYSONLY	0x08		/* boolean hashes */
#define	DB_NOBLANKS	0x10		/* keys must have values or skip */
#define	DB_KEYFORMAT	0x20		/* key/value are u@h|path|date|cksum */
#define	DB_CONFIG	0x40		/* config file format */
#define	DB_IDCACHE	(0x80|DB_KEYFORMAT|DB_NODUPS)

#define	MAXREV	24	/* 99999.99999.99999.99999 */
#define	MD5LEN	32	/* really 30: 8 hex time + 22 base-64 MD5 of key */

#define	LEASE_URL	getenv("BK_LEASE_URL")
#define	LEASE_URL2	getenv("BK_LEASE_URL2")
#define	BK_WEBMAIL_URL	getenv("BK_WEBMAIL_URL")
#define	BK_HOSTME_SERVER "hostme.bkbits.net"
#define	BK_CONFIG_URL	getenv("BK_CONFIG_URL")
#define	BK_CONFIG_URL2	getenv("BK_CONFIG_URL2")
#define	BKDIR		"BitKeeper"
#define	BKTMP		"BitKeeper/tmp"
#define	BKROOT		"BitKeeper/etc"
#define	GONE		goneFile()
#define	SGONE		sgoneFile()
#define	COLLAPSED	"BitKeeper/etc/collapsed"
#define	CSETS_IN	"BitKeeper/etc/csets-in"
#define	CSETS_OUT	"BitKeeper/etc/csets-out"
#define	CHANGESET	"SCCS/s.ChangeSet"
#define	CCHANGESET	"SCCS/c.ChangeSet"
#define	GCHANGESET	"ChangeSet"
#define	getIDCACHE(p)	(proj_hasOldSCCS(p) ? \
				"BitKeeper/etc/SCCS/x.id_cache" : \
				"BitKeeper/log/x.id_cache")
#define	IDCACHE		getIDCACHE(0)
#define	DFILE		"BitKeeper/etc/SCCS/x.dfile"
#define	WEBMASTER	"BitKeeper/etc/webmaster"
#define	CHECKED		"BitKeeper/log/checked"
#define	REPO_ID		"BitKeeper/log/repo_id"
#define	BKSKIP		".bk_skip"
#define	GROUP_MODE	0664
#define	BAM_DSPEC	"$if(:BAMHASH:){:BAMHASH: :KEY: :MD5KEY|1.0:}"

#define	MINUTE	(60)
#define	HOUR	(60*MINUTE)
#define	DAY	(24*HOUR)
#define	WEEK	(7*DAY)
#define	YEAR	(365*DAY)	/* no leap */
#define	MONTH	(YEAR/12)	/* average */
#define	DECADE	(10*YEAR)

#define	UNKNOWN_USER	"anon"
#define	UNKNOWN_HOST	"nowhere"

#define	CNTLA_ESCAPE	'\001'	/* escape character for ^A is also a ^A */
#define	isData(buf)	((buf[0] != '\001') || \
			    ((buf[0] == CNTLA_ESCAPE) && (buf[1] == '\001')))

typedef	u32		ser_t;
typedef	unsigned short	sum_t;

#include "cmd.h"

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
	ser_t	serial;			/* serial number of this delta */
	ser_t	pserial;		/* serial number of parent */
	ser_t	*include;		/* include serial #'s */
	ser_t	*exclude;		/* exclude serial #'s */
	char	**cmnts;		/* comment offset or lines array */
	/* New stuff in lm's sccs */
	ser_t	ptag;			/* parent in tag graph */
	ser_t	mtag;			/* merge parent in tag graph */
	char	**text;			/* descriptive text log */
	char	*hostname;		/* hostname where revision was made */
	char	*pathname;		/* pathname to the file */
	char	*zone;			/* 08:00 is time relative to GMT */
	char	*csetFile;		/* id for ChangeSet file */
	char	*hash;			/* hash of gfile for BAM */
	char	*random;		/* random bits for file ID */
	ser_t	merge;			/* serial number merged into here */
	sum_t	sum;			/* checksum of gfile */
	time_t	dateFudge;		/* make dates go forward */
	mode_t	mode;			/* 0777 style modes */
	char 	*symlink;		/* sym link target */
	u32	xflags;			/* timesafe x flags */
	/* In memory only stuff */
	ser_t	r[4];			/* 1.2.3 -> 1, 2, 3, 0 */
	time_t	date;			/* date - conversion from sdate/zone */
	struct	delta *parent;		/* parent delta above me */
	struct	delta *kid;		/* next delta on this branch */
	struct	delta *siblings;	/* pointer to other branches */
	struct	delta *next;		/* all deltas in table order */
	u32	flags;			/* per delta flags */
	u32	dangling:1;		/* in MONOTONIC file, ahead of chgset */
	u32	symGraph:1;		/* if set, I'm a symbol in the graph */
	u32	symLeaf:1;		/* if set, I'm a symbol with no kids */
					/* Needed for tag conflicts with 2 */
					/* open tips, so maintained always */
	u32	localcomment:1;		/* comments are stored locally */
	char	type;			/* Delta or removed delta */
} delta;
#define	COMMENTS(d)	((d)->cmnts != 0)
#define	TAG(d)		((d)->type != 'D')
#define	NOFUDGE(d)	(d->date - d->dateFudge)
#define	EACH_COMMENT(s, d) \
			comments_load(s, d); \
			EACH_INDEX(d->cmnts, i)

#define	NEXT(d)		((d)->next)
#define	PARENT(s, d)	((d)->parent)
#define	MERGE(s, d)	sfind((s), (d)->merge)
#define	KID(d)		((d)->kid)
#define	SIBLINGS(d)	((d)->siblings)

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
	char	*symname;		/* STABLE */
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

typedef	struct sccs sccs;

#include "proj.h"

extern	jmp_buf	exit_buf;
extern	char *upgrade_msg;

#define	exit(e)	longjmp(exit_buf, (e) + 1000)

#define	READER_LOCK_DIR	"BitKeeper/readers"
#define	WRITER_LOCK_DIR	"BitKeeper/writer"
#define	WRITER_LOCK	"BitKeeper/writer/lock"
#define	NESTED_WRITER_LOCK "BitKeeper/writer/nested_lock"

#define	LOCK_WR_BUSY	"ERROR-Unable to lock repository for update."
#define	LOCK_RD_BUSY	"ERROR-Can't get read lock on the repository."
#define	LOCK_PERM	"ERROR-Lock fail: possible permission problem."
#define	LOCK_UNKNOWN	"ERROR-Unknown lock error."

#define	LOCKERR_NOREPO		-1
#define	LOCKERR_PERM		-2
#define	LOCKERR_LOST_RACE	-3

/*
 * Bumped whenever we change any file format.
 * 2 - bumped to invalidate old binaries with bad date code.
 * 3 - because random bits can now be on a per delta basis.
 * 4 - added tag graph, X_LOGS_ONLY, DT_PLACEHOLDER & DT_NO_TRANSMIT
 */
#define	SCCS_VERSION		4

/*
 * struct loc - locations
 */
typedef struct loc {
	char	*p;		/* first byte of the data */
	u32	len;		/* think 4GB is big enough? */
	ser_t	serial;
} loc;

/*
 * struct sccs - the delta tree, the data, and associated junk.
 */
struct sccs {
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
	off_t	size;		/* size of mapping */
	FILE	*fh;		/* cached copy of the input file handle */
	FILE	*oldfh;		/* orig fh (no ungzip layer) */
	FILE	*outfh;		/* fh for writing x.file */
	char	*sfile;		/* SCCS/s.foo.c */
	char	*pfile;		/* SCCS/p.foo.c */
	char	*zfile;		/* SCCS/z.foo.c */
	char	*gfile;		/* foo.c */
	char	*symlink;	/* if gfile is a sym link, the destination */
	char	**usersgroups;	/* lm, beth, staff, etc */
	int	encoding;	/* ascii, uuencode, gzip, etc. */
	char	**flags;	/* flags in the middle that we didn't grok */
	char	**text;		/* descriptive text */
	u32	state;		/* GFILE/SFILE etc */
	u32	xflags;		/* cache of sccs_top()->xflags */
	mode_t	mode;		/* mode of the gfile */
	off_t	data;		/* offset to data in file */
	delta	*rstart;	/* start of a range (1.1 - oldest) */
	delta	*rstop;		/* end of range (1.5 - youngest) */
	delta	*remote;	/* sccs_resolveFiles() sets this */
	delta	*local;		/* sccs_resolveFiles() sets this */
	sum_t	 cksum;		/* SCCS chksum */
	sum_t	 dsum;		/* SCCS delta chksum */
	u32	added;		/* lines added by this delta (u32!) */
	u32	deleted;	/* and deleted */
	u32	same;		/* and unchanged */
	off_t	sumOff;		/* offset of the new delta cksum */
	time_t	gtime;		/* gfile modidification time */
	time_t	stime;		/* sfile modidification time */
	MDBM	*mdbm;		/* If state & S_HASH, put answer here */
	MDBM	*findkeydb;	/* Cache a map of delta key to delta* */
	project	*proj;		/* If in BK mode, pointer to project */
	void	*rrevs;		/* If has conflicts, revs in conflict */
				/* Actually is of type "name *" in resolve.h */
	u16	version;	/* file format version */
	u16	userLen;	/* maximum length of any user name */
	u16	revLen;		/* maximum length of any rev name */
	loc	*locs;		/* for cset data */ 
	u32	iloc;		/* index to element in *loc */ 
	u32	nloc;		/* # of element in *loc */ 
	u32	initFlags;	/* how we were opened */
	char	*comppath;	/* used to get correct historic paths in comps*/
	u32	cksumok:1;	/* check sum was ok */
	u32	cksumdone:1;	/* check sum was checked */
	u32	grafted:1;	/* file has grafts */
	u32	bad_dsum:1;	/* patch checksum mismatch */
	u32	io_error:1;	/* had an output error, abort */
	u32	io_warned:1;	/* we told them about the error */
	u32	bitkeeper:1;	/* bitkeeper file */
	u32	prs_output:1;	/* prs printed something */
	u32	prs_odd:1;	/* for :ODD: :EVEN: in dspecs */
	u32	prs_one:1;	/* stop printing after printing the first one */
	u32	prs_join:1;	/* for joining together items in dspecs */
	u32	prs_all:1;	/* including tag deltas in prs output */
	u32	prs_indentC:1;	/* extra space for components in :INDENT: ? */
	u32	unblock:1;	/* sccs_free: only if set */
	u32	hasgone:1;	/* this graph has D_GONE deltas */
	u32	has_nonl:1;	/* set by getRegBody() if a no-NL is seen */
	u32	cachemiss:1;	/* BAM file not found locally */
	u32	bamlink:1;	/* BAM gfile is hardlinked to the sfile */
	u32	used_cfile:1;	/* comments_readcfile found one; for cleanup */
	u32	modified:1;	/* set if we wrote the s.file */
	u32	keydb_long:1;	/* findkeydb contains long keys? */
	u32	keydb_md5:1;	/* findkeydb contains md5 keys? */
	u32	mem_in:1;	/* s->fh is in-memory FILE* */
	u32	mem_out:1;	/* s->outfh is in-memory FILE* */
	u32	file:1;		/* treat as a file in DSPECS */
};

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
 * Timestamp record format
 */
typedef struct {
	u32	gfile_mtime;
	u32	gfile_size;
	u32	permissions;
	u32	sfile_mtime;
	u32	sfile_size;
} tsrec;

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
	delta	*d;		/* in cset path, save the corresponding d */
	time_t	order;		/* ordering over the whole list, oldest first */
	u32	local:1;	/* patch is from local file */
	u32	remote:1;	/* patch is from remote file */
	u32	meta:1;		/* delta is metadata */
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
 * 1.4 = one interleaved diff per file in the patch
 */
#define PATCH_CURRENT	"# Patch vers:\t1.3\n"
#define PATCH_FAST	"# Patch vers:\t1.4\n"

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

struct tool
{
	char	*prog;	/* fm3tool */
	char	*alias;	/* fm3 or 0 */
};

/*
 * BK "URL" formats are:
 *	bk://user@host:port/pathname
 *	user@host:pathname
 * In most cases, everything except the pathname is optional.
 */
typedef struct {
	u16	port;		/* remote port if set */
	u16	type;		/* address type, nfs/bk/http/file/ssh/rsh */
	u16	loginshell:1;	/* if set, login shell is the bkd */
	u16	trace:1;	/* for debug, trace send/recv msg */
	u16	progressbar:1;	/* display progressbar for large transfers */
	u16	isSocket:1;	/* if set, rfd and wfd are sockets */
	u16	badhost:1;	/* if set, hostname lookup failed */
	u16	badconnect:1;	/* if set, connect failed */
	u16	withproxy:1;	/* connected via a proxy */
	u16	remote_cmd:1;	/* client wants to run command via bkd */
	u16	need_exdone:1;	/* need call to send_file_extra_done() */
	int	rfd;		/* read fd for the remote channel */
	FILE	*rf;		/* optional stream handle for remote channel */
	int	wfd;		/* write fd for the remote channel */
	int	gzip_in;	/* gzip-level wanted by user*/
	int	gzip;		/* gzip-level to use in protocal */
	char	*user;		/* remote user if set */
	char	*host;		/* remote host if set */
	char	*path;		/* pathname (must be set) */
	char	*httppath;	/* pathname for http URL, see REMOTE_BKDURL */
	char 	*cred;		/* user:passwd for proxy authentication */
	char	*seed;		/* seed saved to validate bkd */
	int	contentlen;	/* len from http header (recieve only) */
	pid_t	pid;		/* if pipe, pid of the child */
	hash	*errs;		/* encode error messages */
	hash	*params;	/* optional url params */
} remote;

#define	ADDR_NFS	0x001	/* host:/path */
#define	ADDR_BK		0x002	/* bk://host:[port]//path */
#define	ADDR_HTTP	0x004	/* http://host:[port]//path */
#define	ADDR_FILE	0x008	/* file://path */
#define	ADDR_SSH	0x010	/*
				 * ssh:[user@]host//path or
				 * ssh:[user@]host:/path
				 */
#define	ADDR_RSH	0x020	/*
				 * rsh:[user@]host//path or
				 * rsh:[user@]host:/path
				 */

int	sccs_admin(sccs *sc, delta *d, u32 flgs, char *compress,
	    admin *f, admin *l, admin *u, admin *s, char *mode, char *txt);
int	sccs_cat(sccs *s, u32 flags, char *printOut);
int	sccs_delta(sccs *s, u32 flags, delta *d, MMAP *init, MMAP *diffs,
		   char **syms);
int	sccs_diffs(sccs *s, char *r1, char *r2, u32 flags, u32 kind, FILE *);
int	sccs_encoding(sccs *s, off_t size, char *enc, char *comp);
int	sccs_get(sccs *s,
	    char *rev, char *mRev, char *i, char *x, u32 flags, char *out);
int	sccs_hashcount(sccs *s);
int	sccs_clean(sccs *s, u32 flags);
int	sccs_unedit(sccs *s, u32 flags);
int	sccs_info(sccs *s, u32 flags);
int	sccs_prs(sccs *s, u32 flags, int reverse, char *dspec, FILE *out);
int	sccs_prsdelta(sccs *s, delta *d, int flags, char *dspec, FILE *out);
char	*sccs_prsbuf(sccs *s, delta *d, int flags, char *dspec);
delta	*sccs_findDate(sccs *s, char *date, int roundup);
int	sccs_patheq(char *file1, char *file2);
delta	*sccs_findDelta(sccs *s, delta *d);
sccs	*sccs_init(char *filename, u32 flags);
sccs	*sccs_restart(sccs *s);
sccs	*sccs_reopen(sccs *s);
int	sccs_open(sccs *s, struct stat *sp);
void	sccs_free(sccs *);
void	sccs_freetree(delta *);
void	sccs_close(sccs *);
int	sccs_csetWrite(sccs *s, char **cweave);
sccs	*sccs_csetInit(u32 flags);
char	**sccs_files(char **, int);
delta	*sccs_parseArg(delta *d, char what, char *arg, int defaults);
void	sccs_whynot(char *who, sccs *s);
void	sccs_ids(sccs *s, u32 flags, FILE *out);
void	sccs_inherit(sccs *s, delta *d);
int	sccs_hasDiffs(sccs *s, u32 flags, int inex);
void	sccs_print(delta *d);
delta	*sccs_getInit(sccs *s, delta *d, MMAP *f, int patch,
		      int *errorp, int *linesp, char ***symsp);
delta	*sccs_ino(sccs *);
int	sccs_userfile(sccs *);
int	sccs_rmdel(sccs *s, delta *d, u32 flags);
int	sccs_stripdel(sccs *s, char *who);
int	stripdel_fixTable(sccs *s, int *pcnt);
int	sccs_getdiffs(sccs *s, char *rev, u32 flags, char *printOut);
int	sccs_patchDiffs(sccs *s, ser_t *patchmap, char *printOut);
void	sccs_pdelta(sccs *s, delta *d, FILE *out);
delta	*sccs_key2delta(sccs *sc, char *key);
int	sccs_keyunlink(char *key, MDBM *idDB, MDBM *dirs, u32 flags);
char	*sccs_impliedList(sccs *s, char *who, char *base, char *rev);
int	sccs_sdelta(sccs *s, delta *, char *);
void	sccs_md5delta(sccs *s, delta *d, char *b64);
void	sccs_key2md5(char *rootkey, char *deltakey, char *b64);
void	sccs_syncRoot(sccs *s, char *key);
delta	*sccs_csetBoundary(sccs *s, delta *);
void	sccs_shortKey(sccs *s, delta *, char *);
int	sccs_resum(sccs *s, delta *d, int diags, int dont);
int	cset_resum(sccs *s, int diags, int fix, int spinners, int takepatch);
char	**cset_mkList(sccs *cset);
int	cset_bykeys(const void *a, const void *b);
int	cset_byserials(const void *a, const void *b);
int	sccs_newchksum(sccs *s);
void	sccs_perfile(sccs *, FILE *);
sccs	*sccs_getperfile(MMAP *, int *);
char	*sccs_gethost(void);
char	*sccs_realhost(void);
char	*sccs_host(void);
int	sccs_getComments(char *, char *, delta *);
int	sccs_badTag(char *, char *, int);
MDBM    *sccs_keys2mdbm(FILE *f);
void	sfileUnget(void);
char	*sfileNext(void);
char	*sfileRev(void);
char	*sfileFirst(char *cmd, char **Av, int Flags);
int	sfileDone(void);
int	sfiles(char *opts);
int	sfilesDied(int killit);
delta	*sccs_findrev(sccs *, char *);
delta	*sccs_top(sccs *);
delta	*sccs_findKey(sccs *, char *);
void	sccs_findKeyUpdate(sccs *s, delta *d);
int	isKey(char *key);
delta	*sccs_findMD5(sccs *s, char *md5);                              
delta	*sccs_dInit(delta *, char, sccs *, int);
char	*sccs_getuser(void);
void	sccs_resetuser(void);
void	sccs_resethost(void);
char	*sccs_realuser(void);
char	*sccs_user(void);

delta	*modeArg(delta *d, char *arg);
int	fileType(mode_t m);
char	chop(char *s);
int	atoi_p(char **p);
char	*p2str(void *p);
int	sccs_filetype(char *name);
int	isValidHost(char *h);
int	isValidUser(char *u);
int	readable(char *f);
char	*sccs2name(char *);
char	*name2sccs(char *);
int	diff(char *lfile, char *rfile, u32 kind, char *out);
int	check_gfile(sccs*, int);
char	*lock_dir(void);
void	platformSpecificInit(char *);
MDBM	*loadDB(char *file, int (*want)(char *), int style);
delta 	*mkOneZero(sccs *s);
int	isCsetFile(char *);
int	csetIds(sccs *cset, char *rev);
int	csetIds_merge(sccs *cset, char *rev, char *merge);
int	cset_inex(int flags, char *op, char *revs);
void	sccs_fixDates(sccs *);
int	sccs_xflags(sccs *s, delta *d);
char	*xflags2a(u32 flags);
u32	a2xflag(char *str);
void	sccs_mkroot(char *root);
int	sccs_parent_revs(sccs *s, char *rev, char **revP, char **revM);
char	*sccs_setpathname(sccs *s);
delta	*sccs_next(sccs *s, delta *d);
int	sccs_reCache(int quiet);
int	sccs_meta(char *m,sccs *s, delta *parent, MMAP *initFile, int fixDates);
int	sccs_findtips(sccs *s, delta **a, delta **b);
int	sccs_resolveFiles(sccs *s);
sccs	*sccs_keyinit(project *proj, char *key, u32 flags, MDBM *idDB);
delta	*sfind(sccs *s, ser_t ser);
void	sfind_update(sccs *s, delta *d, ser_t oldser);
int	sccs_lock(sccs *, char);	/* respects repo locks */
int	sccs_unlock(sccs *, char);

int	sccs_lockfile(char *lockfile, int wait, int quiet);
int	sccs_stalelock(char *lockfile, int discard);
int	sccs_unlockfile(char *file);
int	sccs_mylock(char *lockf);
int	sccs_readlockf(char *file, pid_t *pidp, char **hostp, time_t *tp);

sccs	*sccs_unzip(sccs *s);
sccs	*sccs_gzip(sccs *s);
char	*sccs_utctime(delta *d);
void	sccs_kidlink(sccs *s, delta *d);
void	sccs_renumber(sccs *s, u32 flags);
char 	*sccs_iskeylong(char *key);
int	linelen(char *s);
char	*licenses_accepted(void);
int	eula_known(char *lic);
int	eula_accept(int prompt, char *lic);
char	*eula_name(void);
char	*eula_type(u32 bits);
char	*mkline(char *mmap);
char    *mode2FileType(mode_t m);
#define	getline bk_getline
int	getline(int in, char *buf, int size);
void	explodeKey(char *key, char *parts[6]);
void	free_pfile(pfile *pf);
int	sccs_read_pfile(char *who, sccs *s, pfile *pf);
int	sccs_rewrite_pfile(sccs *s, pfile *pf);
int	sccs_isleaf(sccs *s, delta *d);
int	emptyDir(char *dir);
int	sameFiles(char *file1, char *file2);
int	gone(char *key, MDBM *db);
int	sccs_mv(char *, char *, int, int, int, int);
delta	*sccs_gca(sccs *, delta *l, delta *r, char **i, char **x);
char	*_relativeName(char *gName, int isDir,
	    int mustHaveRmarker, int wantRealName, project *proj);
char	*findBin(void);
int 	prompt(char *msg, char *buf);
void	parse_url(char *url, char *host, char *path);
char	*sccs_Xfile(sccs *s, char type);
FILE	*sccs_startWrite(sccs *s);
int	sccs_finishWrite(sccs *s, FILE **f);
void	sccs_abortWrite(sccs *s, FILE **f);
int	unique(char *key);
char	*uniq_keysHome(void);
int	uniq_lock(void);
int	uniq_unlock(void);
int	uniq_open(void);
time_t	uniq_drift(void);
int	uniq_update(char *key, time_t t);
int	uniq_close(void);
time_t	sccs_date2time(char *date, char *zone);
pid_t	smtpmail(char **to, char *subject, char *file);
int	connect_srv(char *srv, int port, int trace);
int	get(char *path, int flags, char *output);
int	gethelp(char *helptxt, char *help_name, char *bkarg, char *prefix, FILE *f);
void	status(int verbose, FILE *out);
void	notify(void);
char	*package_name(void);
int	bkusers(sccs *s, char *prefix, FILE *out);
int	sfiles_glob(char *glob);
int	readn(int from, char *buf, int size);
void	send_request(int fd, char * request, int len);
int	writen(int to, void *buf, int size);
int	fd2file(int fd, char *file);
int	repository_downgrade(project *p);
int	repository_locked(project *p);
int	repository_mine(project *p, char type);
int	repository_lockers(project *p);
int	repository_rdlock(project *p);
int	repository_rdunlock(project *p, int all);
void	repository_unlock(project *p, int all);
int	repository_wrlock(project *p);
int	repository_wrunlock(project *p, int all);
int	repository_hasLocks(project *p, char *dir);
void	repository_lockcleanup(project *p);
int	repo_nfiles(sccs *cset);
int	comments_save(char *s);
int	comments_savefile(char *s);
int	comments_got(void);
void	comments_done(void);
delta	*comments_get(delta *d);
void	comments_writefile(char *file);
int	comments_checkStr(u8 *s);
void	host_done(void);
delta	*host_get(delta *);
void	user_done(void);
delta	*user_get(delta *);
char	*shell(void);
int	bk_sfiles(char *opts, int ac, char **av);
int	outc(char c);
void	error(const char *fmt, ...);
MDBM	*loadConfig(project *p, int forcelocal);
int	ascii(char *file);
char	*sccs_rmName(sccs *s);
int	sccs_rm(char *name, int force);
void	sccs_rmEmptyDirs(char *path);
void	do_prsdelta(char *file, char *rev, int flags, char *dspec, FILE *out);
char 	**get_http_proxy(char *host);
int	confirm(char *msg);
int	csetCreate(sccs *cset, int flags, char *files, char **syms);
int	cset_setup(int flags);
char	*separator(char *);
int	trigger(char *cmd, char *when);
void	cmdlog_start(char **av, int bkd_mode);
void	cmdlog_addnote(char *key, char *val);
int	cmdlog_end(int ret, int bkd_mode);
int	write_log(char *file, int rotate, char *format, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 3, 4)))
#endif
	;
off_t	get_byte_count(void);
void	save_byte_count(unsigned int byte_count);
char	*getHomeDir(void);
char	*getDotBk(void);
char	*age(time_t secs, char *space);
char	*time2date(time_t tt);
char	*sccs_zone(time_t tt);
MDBM	*sccs_tagConflicts(sccs *s);
int	sccs_tagMerge(sccs *s, delta *d, char *tag);
int	sccs_tagleaves(sccs *, delta **, delta **);
ser_t	*sccs_set(sccs *, delta *, char *iLst, char *xLst);
int	sccs_graph(sccs *s, delta *d, ser_t *map, char **inc, char **exc);
int	sccs_isPending(char *gfile);
int	stripdel_setMeta(sccs *s, int stripBranches, int *count);

int     http_connect(remote *r);
int	http_send(remote *, char *msg, size_t len, size_t ex, char *ua);
int	http_fetch(remote *r, char *file);
char	*bktmp(char *buf, const char *template);
void	bktmpenv(void);
char	*bktmpdir(char *buf, const char *template);
char	*bktmp_local(char *buf, const char *template);
void	bktmpcleanup(void);
int	smallTree(int threshold);
char	*strdup_tochar(const char *s, int c);
void	enableFastPendingScan(void);
char	*isHostColonPath(char *);
int	gui_useDisplay(void);
int	gui_useAqua(void);
char	*gui_displayName(void);
char	*savefile(char *dir, char *prefix, char *pathname);
void	has_proj(char *who);
char	*globalroot(void);
void	sccs_touch(sccs *s);
int	setlevel(int);
void	sccs_rmUncommitted(int quiet, FILE *chkfiles);
void	rmEmptyDirs(int quiet);    
int	after(int quiet, char *rev);
int	consistency(int quiet);
int	diff_gfile(sccs *s, pfile *pf, int expandKeyWord, char *tmpfile);
char	*getCSetFile(project *p);
int	spawn_cmd(int flag, char **av);
pid_t	mkpager(void);
int	getRealName(char *path, MDBM *db, char *realname);
int	addsym(sccs *s, delta *d, delta *metad, int, char*, char*);
int	delta_table(sccs *s, FILE *out, int willfix);
int	walksfiles(char *dir, walkfn fn, void *data);
delta	*getSymlnkCksumDelta(sccs *s, delta *d);
hash	*generateTimestampDB(project *p);
int	timeMatch(project *proj, char *gfile, char *sfile, hash *timestamps);
void	dumpTimestampDB(project *p, hash *db);
void	updateTimestampDB(sccs *s, hash *timestamps, int diff);
struct tm *utc2tm(time_t t);
int	sccs_setStime(sccs *s, time_t newest);
int	isLocalHost(char *h);
void	ids(void);
void	http_hdr(void);
int	check_rsh(char *remsh);
void	sccs_color(sccs *s, delta *d);
int	out(char *buf);
int	getlevel(void);
delta	*cset_insert(sccs *s, MMAP *iF, MMAP *dF, delta *parent, int fast);
int	cset_write(sccs *s, int spinners, int fast);
sccs	*cset_fixLinuxKernelChecksum(sccs *s);
int	cweave_init(sccs *s, int extras);
int	isNullFile(char *rev, char *file);
unsigned long	ns_sock_host2ip(char *host, int trace);
unsigned long	host2ip(char *host, int trace);
int	fileTypeOk(mode_t m);
int	sccs_tagLeaf(sccs *s, delta *d, delta *md, char *tag);
int	sccs_scompress(sccs *s, int flags);
int	mkBkRootIcon(char *path);
int	unmkBkRootIcon(char *path);
void	sccs_tagcolor(sccs *s, delta *d);
int	checkXflags(sccs *s, delta *d, int what);
void	metaUnionResync1(void);
void	metaUnionResync2(void);
int	sccs_istagkey(char *key);
char	*testdate(time_t t);
void	putroot(char *where);
int	uuencode(FILE *in, FILE *out);
int	uudecode(FILE *in, FILE *out);
void	sccs_unmkroot(char *path);
int	sccs_needSwap(sccs *s, delta *p, delta *m);
void	sccs_reDup(sccs *s);
void	sccs_adjustSet(sccs *sc, sccs *scb, delta *d);
int	chk_host(void);
int	chk_user(void);
int	chk_nlbug(sccs *s);
int	chk_permissions(void);
int	fix_gmode(sccs *s, int gflags);
int	do_checkout(sccs *s);
int	unsafe_path(char *s);
char	**getTriggers(char **lines, char *dir, char *prefix);
void	comments_cleancfile(sccs *s);
int	comments_readcfile(sccs *s, int prompt, delta *d);
int	comments_prompt(char *file);
void	saveEnviroment(char *patch);
void	restoreEnviroment(char *patch);
int	run_check(int quiet, char *partial, char *opts, int *did_partial);
int	full_check(void);
char	*key2path(char *key, MDBM *idDB);
int	check_licensesig(char *key, char *sign, int version);
char	*hashstr(char *str, int len);
char	*hashstream(int fd);
char	*secure_hashstr(char *str, int len, char *key);

#define	KEY_LEASE		0
#define	KEY_BK_AUTH_HMAC	1
#define	KEY_LCONFIG		2
#define	KEY_UPGRADE		3
#define	KEY_SIGNEDFILE		4
#define	KEY_SEED		5
#define	KEY_EULA		6
char	*makestring(int keynum);

void	delete_cset_cache(char *rootpath, int save);
void	notice(char *key, char *arg, char *type);
void	save_log_markers(void);
void	update_log_markers(int verbose);
delta	*sccs_getedit(sccs *s, char **revp);
void	line2av(char *cmd, char **av);
void	smerge_saveseq(u32 seq);
void	mk_repoID(project *proj, char *repoid);
void	fromTo(char *op, remote *r, remote *l);
u32	adler32_file(char *filename);
char	*findDotFile(char *old, char *new, char *buf);
char	*platform(void);
char	*pager(void);
int	bkmail(char *url, char **to, char *subject, char *file);
void	bkversion(FILE *f);
int	sane(int, int);
int	global_locked(void);
typedef	struct ticker ticker;
#define	PROGRESS_SPIN	0
#define	PROGRESS_MINI	1
#define	PROGRESS_BAR	2
ticker	*progress_start(int style, u64 max);
void	progress(ticker *t, u64 n);
void	progress_done(ticker *t, char *msg);
char	*signed_loadFile(char *filename);
int	signed_saveFile(char *filename, char *data);
void	bk_preSpawnHook(int flags, char *av[]);
int	upgrade_decrypt(FILE *fin, FILE *fout);
int	crypto_symEncrypt(char *key, FILE *fin, FILE *fout);
int	crypto_symDecrypt(char *key, FILE *fin, FILE *fout);
int	inskeys(char *image, char *keys);
void	lockfile_cleanup(void);

void	align_diffs(u8 *vec, int n, int (*compare)(int a, int b),
int	(*is_whitespace)(int i));
void	close_gaps(u8 *vec, int n, int (*compare)(int a, int b));
int	diff_algor(int m, int n, u8 *lchg, u8 *rchg,
int	(*compare)(int a, int b));
int	diffline(char *left, char *right);
typedef	void (*set_pfunc)(sccs *, delta *);
ser_t	*set_get(sccs *s, char *rev);
void	set_list(sccs *s, char *rev, set_pfunc p);
void	set_member(sccs *s, char *rev, ser_t *map, set_pfunc p);
void	set_diff(sccs *s, ser_t *a, ser_t *b, set_pfunc p);
void	set_and(sccs *s, ser_t *a, ser_t *b, set_pfunc p);
void	set_or(sccs *s, ser_t *a, ser_t *b, set_pfunc p);
void	set_xor(sccs *s, ser_t *a, ser_t *b, set_pfunc p);
void	set_set(sccs *s, char *rev, set_pfunc p);
int	saveStdin(char *tmpfile);
char	**parent_pullp(void);
char	**parent_pushp(void);
char	**parent_allp(void);
int	restore_backup(char *backup_sfio, int overwrite);
char	*parent_normalize(char *);
u32	crc(char *s);
int	annotate_args(int flags, char *args);
void	platformInit(char **av);
int	sccs_csetPatchWeave(sccs *s, FILE *f);
int	sccs_fastWeave(sccs *s, ser_t *weavemap, char **patchmap,
	    MMAP *fastpatch, FILE *out);
void	sccs_clearbits(sccs *s, u32 flags);
MDBM	*loadkv(char *file);
char	**getParkComment(int *err);
int	launch_wish(char *script, char **av);
void	converge_hash_files(void);
int	getMsg(char *msg_name, char *bkarg, char b, FILE *outf);
int	getMsg2(char *msg_name, char *arg, char *arg2, char b, FILE *outf);
int	getMsgP(char *msg_name, char *bkarg, char *prefix, char b, FILE *outf);
int	getMsgv(char *msg_name, char **bkarg, char *prefix, char b, FILE *outf);
void	randomBits(char *buf);
int	almostUnique(void);
int	uninstall(char *path, int upgrade);
int	remote_bk(int quiet, int ac, char **av);
void	dspec_eval(FILE * out, sccs *s, delta *d, char *start);
void	dspec_printline(sccs *s, FILE *out);
void	dspec_printeach(sccs *s, FILE *out);
int	kw2val(FILE *out, char *kw, int len, sccs *s, delta *d);
void	show_s(sccs *s, FILE *out, char *data, int len);
void	show_d(sccs *s, FILE *out, char *format, int num);
void	comments_append(delta *d, char *line);
char	**comments_load(sccs *s, delta *d);
void	comments_free(delta *d);
void	gdb_backtrace(void);
char	*bp_lookup(sccs *s, delta *d);
delta	*bp_fdelta(sccs *s, delta *d);
int	bp_fetch(sccs *s, delta *din);
int	bp_fetchData(void);
int	bp_fetchkeys(char *me, project *p, int quiet, char **keys, u64 todo);
int	bp_get(sccs *s, delta *d, u32 flags, char *out);
int	bp_delta(sccs *s, delta *d);
int	bp_diff(sccs *s, delta *d, char *gfile);
int	bp_updateServer(char *range, char *list, int quiet);
int	bp_sharedServer(void);
char	*bp_serverURL(char *out);
char	*bp_serverID(char *out, int notme);
char	*bp_serverURL2ID(char *url);
void	bp_setBAMserver(char *path, char *url, char *repoid);
int	bp_hasBAM(void);
u32	send_BAM_sfio(FILE *wf, char *bp_keys, u64 bpsz, int gzip);
int	bkd_BAM_part3(remote *r, char **env, int quiet, char *range);
int	bp_sendkeys(FILE *f, char *range, u64 *bytes, int gzip);
int	detach(int quiet);
int	zgets_hread(void *token, u8 **buf);
int	zgets_hfread(void *token, u8 **buf);
void	zputs_hfwrite(void *token, u8 *data, int len);
char	*psize(u64 bytes);
u64	scansize(char *bytes);
void	idcache_update(char *filelist);
int	idcache_write(project *p, MDBM *idDB);
void	cset_savetip(sccs *s, int force);
void	symGraph(sccs *s, delta *d);
void	clearCsets(sccs *s);
void	sccs_rdweaveInit(sccs *s);
char	*short_random(char *str, int len);
char	*sccs_nextdata(sccs *s);
int	sccs_rdweaveDone(sccs *s);
int	hasLocalWork(char *gfile);
char	*goneFile(void);
char	*sgoneFile(void);
int	keycmp(const void *k1, const void *k2);
int	key_sort(const void *a, const void *b);
int	startmenu_list(u32, char *);
int	startmenu_rm(u32, char *);
int	startmenu_get(u32, char *path);
int	startmenu_set(u32, char *, char *, char *, char *);
char	*bkmenupath(int, int);
void	repos_update(sccs *cset);
char	*bk_searchFile(char *base);
void	dspec_collapse(char **dspec, char **begin, char **end);
void	fslayer_cleanup(void);
void	updatePending(sccs *s);
int	isSCCS(const char *path);
int	fslayer_enable(int en);
int	doidx_quit(project *proj);
int	doidx_remap(project *proj, char *rel, char **file);
int	doidx_utime(project *proj, char *rel, const struct utimbuf *utb);
int	doidx_lstat(project *proj, char *rel, struct stat *sb);
int	doidx_unlink(project *proj, char *rel);
int	doidx_rename(project *proj1, char *old, project *proj2, char *new);
int	doidx_link(project *proj1, char *old, project *proj2, char *new);
int	doidx_chmod(project *proj, char *rel, mode_t mode);
int	doidx_mkdir(project *proj, char *dir, mode_t mode);
int	doidx_rmdir(project *proj, char *dir);
char	**doidx_getdir(project *proj, char *dir);
char	*doidx_realBasename(project *proj, char *rel, char *realname);
int	doidx_access(project *proj, char *file, int mode);
int	bk_urlArg(char ***urls, char *arg);

#ifdef	WIN32
void	notifier_changed(char *fullpath);
void	notifier_flush(void);
#else
#define	notifier_changed(x)
#define	notifier_flush()
#endif

extern	char	*editor;
extern	char	*bin;
extern	char	*BitKeeper;
extern	time_t	licenseEnd;
extern	int	spawn_tcl;
extern	char	*start_cwd;
extern	char	*bk_vers;
extern	char	*bk_utc;
extern	char	*bk_tag;
extern	char	*bk_time;
extern	char	*bk_platform;
extern	time_t	bk_build_timet;
extern	char	*bk_build_user;
extern	char	*bk_build_dir;
extern	int	test_release;
extern	char	*prog;

#define	componentKey(k) (strstr(k, "/ChangeSet|") != (char*)0)
#define	changesetKey(k) (strstr(k, "|ChangeSet|") != (char*)0)

#endif	/* _SCCS_H_ */
