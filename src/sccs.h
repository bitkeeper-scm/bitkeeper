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
#define	GET_PERMS	0x00000100	/* extract perms for non gfile */
#define	GET_SUM		0x00000200	/* used to force dsum in getRegBody */
#define GET_NOREGET	0x00000400	/* get -S: skip gfiles that exist */
#define	GET_LINENAME	0x00000800	/* get -O: prefix with line name */
#define	GET_RELPATH	0x00000010	/* like GET_MODNAME but full relative */
#define	GET_SKIPGONE	0x00000020	/* ignore gone deltas in HASH */
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
#define	DELTA_TAKEPATCH	0x00400000	/* call sccs_getInit() from takepatch */

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
 * Bit 3 is binary file format
 */
#define	E_ALWAYS	0x1000		/* set so encoding is non-zero */
#define E_DATAENC	0x3
#define E_COMP		0x4

#define	E_ASCII		0		/* no encoding */
#define	E_UUENCODE	1		/* uuenecode it (traditional) */
#define	E_BAM		2		/* store data in BAM pool */
#define	E_GZIP		4		/* gzip the data */
#define	E_BFILE		8		/* use binary sfile format */

#define	HAS_GFILE(s)	((s)->state & S_GFILE)
#define	HAS_PFILE(s)	((s)->state & S_PFILE)
#define	HAS_ZFILE(s)	((s)->state & S_ZFILE)
#define	HAS_SFILE(s)	((s)->state & S_SFILE)
#define	BEEN_WARNED(s)	((s)->state & S_WARNED)
#define	WRITABLE(s)	((s)->mode & 0200)
#define EDITED(s)	((((s)->state&S_EDITED) == S_EDITED) && WRITABLE(s))
#define LOCKED(s)	(((s)->state&S_LOCKED) == S_LOCKED)
#define	ASCII(s)	(((s)->encoding_in & E_DATAENC) == E_ASCII)
#define	BINARY(s)	(((s)->encoding_in & E_DATAENC) != E_ASCII)
#define	BAM(s)		(((s)->encoding_in & E_DATAENC) == E_BAM)
#define	UUENCODE(s)	(((s)->encoding_in & E_DATAENC) == E_UUENCODE)
#define	GZIP(s)		(((s)->encoding_in & E_COMP) == E_GZIP)
#define	GZIP_OUT(s)	(((s)->encoding_out & E_COMP) == E_GZIP)
#define	BFILE(s)	(((s)->encoding_in & E_BFILE) != 0)
#define	BFILE_OUT(s)	(((s)->encoding_out & E_BFILE) != 0)
#define	CSET(s)		((s)->state & S_CSET)
#define	CONFIG(s)	((s)->state & S_CONFIG)
#define	READ_ONLY(s)	((s)->state & S_READ_ONLY)
#define	SET(s)		((s)->state & S_SET)
#define	IMPORT(s)	((s)->state & S_IMPORT)
#define	MK_GONE(s, d)	do {(s)->hasgone = 1; FLAGS(s, d) |= D_GONE;} while (0)
#define	TREE(s)		(1)			// s->tree serial
#define	TABLE(s)	(0 + (s)->tip)		// s->table serial
#define	TABLE_SET(s, v)	((s)->tip = (v))	// s->table serial

#define	GOODSCCS(s)	assert(s); unless (TABLE(s)&&(s)->cksumok) return (-1)
#define	HASGRAPH(s)	(TABLE(s))

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
 * Flags (FLAGS(s, d)) that indicate some state on the delta.
 */
/* flags that are written to disk */
#define	D_INARRAY	0x00000001	/* part of s->slist array */
#define	D_NONEWLINE	0x00000002	/* this delta has no trailing newline */
#define	D_CKSUM		0x00000004	/* delta has checksum */
#define	D_SORTSUM	0x00000008	/* generate a sortSum */
#define	D_META		0x00000010	/* this is a metadata removed delta */
#define	D_SYMBOLS	0x00000020	/* delta has one or more symbols */

#define	D_DANGLING	0x00000080	/* in MONOTONIC file, ahead of chgset */
#define	D_TAG		0x00000100	/* is tag node old d->type=='R' */
#define	D_SYMGRAPH	0x00000200	/* if set, I'm a symbol in the graph */
#define	D_SYMLEAF	0x00000400	/* if set, I'm a symbol with no kids */
					/* Needed for tag conflicts with 2 */
					/* open tips, so maintained always */
#define	D_MODE		0x00000800	/* permissions in MODE(s, d) are valid */
#define	D_CSET		0x00001000	/* this delta is marked in cset file */
#define D_XFLAGS	0x00002000	/* delta has updated file flags */
	/* D_NPARENT	0x00004000 */


/* flags that are in memory only and not written to disk */
#define	D_REMOTE	0x00400000	/* for resolve; from remote repos. */
#define	D_LOCAL		0x00800000	/* for resolve; this is a local delta */
#define	D_ERROR		0x01000000	/* from parseArg() */
#define	D_BADFORM	0x02000000	/* poorly formed rev */
#define	D_BADREV	0x04000000	/* bad parent/child relationship */
#define	D_RED		0x08000000	/* marker used in graph traeversal */
#define	D_GONE		0x10000000	/* this delta is gone, don't print */
#define	D_BLUE		0x20000000	/* when you need two colors */
#define	D_ICKSUM	0x40000000	/* use checksum from init file */
#define	D_SET		0x80000000	/* range.c: marked as part of a set */

#define	D_INVALID	((ser_t)~0u)	/* invalid delta */

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
	RET_OK		= 0,
	RET_ERROR	= 1,	/* other error */
	RET_EXISTS	= 2,	/* remote repo exists already */
	RET_CONNECT	= 5,	/* bkd_connect() failed */
	RET_CHDIR	= 6,	/* chdir failure */
	RET_BADREV	= 7,	/* rev not found */
} retrc;

/*
 * Hash behaviour.  Bitmask.
 */
#define	DB_NODUPS       0x01		/* keys must be unique */
#define	DB_KEYSONLY	0x08		/* boolean hashes */
#define	DB_KEYFORMAT	0x20		/* key/value are u@h|path|date|cksum */

/* shortcuts for common formats */
#define	DB_IDCACHE	(0x80|DB_KEYFORMAT|DB_NODUPS)
#define	DB_GONE		(DB_KEYSONLY)

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
#define	NO_DFILE	"BitKeeper/log/no_dfiles"
#define	WEBMASTER	"BitKeeper/etc/webmaster"
#define	CHECKED		"BitKeeper/log/checked"
#define	REPO_ID		"BitKeeper/log/repo_id"
#define	ATTR		"BitKeeper/etc/attr"
#define	SATTR		"BitKeeper/etc/SCCS/s.attr"
#define	BKSKIP		".bk_skip"
#define	GROUP_MODE	0664
#define	BAM_DSPEC	"$if(:BAMHASH:){:BAMHASH: :KEY: :MD5KEY|1.0:}"

/*
 * Constants for running some parallel processes, sfio, checkout,
 * to overcome NFS latency.
 */
#define	PARALLEL_NET		8
#define	PARALLEL_LOCAL		3
#define	PARALLEL_MAX		64

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
	/* linkage */
	ser_t	parent;		/* serial number of parent */
	ser_t	merge;			/* serial number merged into here */
	ser_t	ptag;			/* parent in tag graph */
	ser_t	mtag;			/* merge parent in tag graph */

	/* data */
	u32	added;			/* lines added by this delta (u32!) */
	u32	deleted;		/* and deleted */
	u32	same;			/* and unchanged */
	u32	sum;			/* checksum of gfile */
	u32	sortSum;		/* sum from sortkey */
	u32	date;			/* date - conversion from sdate/zone */
	u32	dateFudge;		/* make dates go forward */
	u32	mode;			/* 0777 style modes */
	u32	xflags;			/* timesafe x flags */
	u32	flags;			/* per delta flags */
	ser_t	r[4];			/* 1.2.3 -> 1, 2, 3, 0 */

	/* unique heap data */
	u32	cludes;			/* include/exclude list */
	u32	comments;		/* delta comments (\n sep string) */
	u32	bamhash;		/* hash of gfile for BAM */
	u32	random;			/* random bits for file ID */

	/* collapsible heap data */
	u32	userhost;		/* user/realuser@host/realhost */
	u32	pathname;		/* pathname to the file */
	u32	sortPath;		/* original pathname for delta */
	u32	zone;			/* 08:00 is time relative to GMT */
	u32 	symlink;		/* sym link target */
 	u32	csetFile;		/* id for ChangeSet file */
} d_t;

#define	XFLAGS(s, d)		((s)->slist[d].xflags)
#define	FLAGS(s, d)		((s)->slist[d].flags)

#define	PARENT(s, d)		(0 + (s)->slist[d].parent)
#define	MERGE(s, d)		(0 + (s)->slist[d].merge)
#define	PTAG(s, d)		(0 + (s)->slist[d].ptag)
#define	MTAG(s, d)		(0 + (s)->slist[d].mtag)
#define	ADDED(s, d)		(0 + (s)->slist[d].added)
#define	DELETED(s, d)		(0 + (s)->slist[d].deleted)
#define	SAME(s, d)		(0 + (s)->slist[d].same)
#define	SUM(s, d)		(0 + (s)->slist[d].sum)
#define	SORTSUM(s, d)		(0 + (s)->slist[d].sortSum)
#define	DATE(s, d)		(0 + (s)->slist[d].date)
#define	DATE_FUDGE(s, d)	(0 + (s)->slist[d].dateFudge)
#define	MODE(s, d)		(0 + (s)->slist[d].mode)
#define	R0(s, d)		(0 + (s)->slist[d].r[0])
#define	R1(s, d)		(0 + (s)->slist[d].r[1])
#define	R2(s, d)		(0 + (s)->slist[d].r[2])
#define	R3(s, d)		(0 + (s)->slist[d].r[3])

#define	PARENT_SET(s, d, v)	((s)->slist[d].parent = (v))
#define	MERGE_SET(s, d, v)	((s)->slist[d].merge = (v))
#define	PTAG_SET(s, d, v)	((s)->slist[d].ptag = (v))
#define	MTAG_SET(s, d, v)	((s)->slist[d].mtag = (v))
#define	ADDED_SET(s, d, v)	((s)->slist[d].added = (v))
#define	DELETED_SET(s, d, v)	((s)->slist[d].deleted = (v))
#define	SAME_SET(s, d, v)	((s)->slist[d].same = (v))
#define	SUM_SET(s, d, v)	((s)->slist[d].sum = (v))
#define	SORTSUM_SET(s, d, v)	((s)->slist[d].sortSum = (v))
#define	DATE_SET(s, d, v)	((s)->slist[d].date = (v))
#define	DATE_FUDGE_SET(s, d, v)	((s)->slist[d].dateFudge = (v))
#define	MODE_SET(s, d, v)	((s)->slist[d].mode = (v))
#define	XFLAGS_SET(s, d, v)	((s)->slist[d].xflags = (v))
#define	FLAGS_SET(s, d, v)	((s)->slist[d].flags = (v))
#define	R0_SET(s, d, v)		((s)->slist[d].r[0] = (v))
#define	R1_SET(s, d, v)		((s)->slist[d].r[1] = (v))
#define	R2_SET(s, d, v)		((s)->slist[d].r[2] = (v))
#define	R3_SET(s, d, v)		((s)->slist[d].r[3] = (v))

#define	HAS_CLUDES(s, d)	((s)->slist[d].cludes != 0)
#define	HAS_COMMENTS(s, d)	((s)->slist[d].comments != 0)
#define	HAS_BAMHASH(s, d)	((s)->slist[d].bamhash != 0)
#define	HAS_RANDOM(s, d)	((s)->slist[d].random != 0)
#define	HAS_USERHOST(s, d)	((s)->slist[d].userhost != 0)
#define	HAS_PATHNAME(s, d)	((s)->slist[d].pathname != 0)
#define	HAS_SORTPATH(s, d)	((s)->slist[d].sortPath != 0)
#define	HAS_ZONE(s, d)		((s)->slist[d].zone != 0)
#define	HAS_SYMLINK(s, d)	((s)->slist[d].symlink != 0)
#define	HAS_CSETFILE(s, d)	((s)->slist[d].csetFile != 0)

#define	CLUDES_INDEX(s, d)	((s)->slist[d].cludes)
#define	COMMENTS_INDEX(s, d)	((s)->slist[d].comments)
#define	BAMHASH_INDEX(s, d)	((s)->slist[d].bamhash)
#define	RANDOM_INDEX(s, d)	((s)->slist[d].random)
#define	USERHOST_INDEX(s, d)	((s)->slist[d].userhost)
#define	PATHNAME_INDEX(s, d)	((s)->slist[d].pathname)
#define	SORTPATH_INDEX(s, d)	((s)->slist[d].sortPath)
#define	ZONE_INDEX(s, d)	((s)->slist[d].zone)
#define	SYMLINK_INDEX(s, d)	((s)->slist[d].symlink)
#define	CSETFILE_INDEX(s, d)	((s)->slist[d].csetFile)

#define	CLUDES_SET(s, d, val)	(CLUDES_INDEX(s, d) = sccs_addStr((s), val))
#define	COMMENTS_SET(s, d, val)	(COMMENTS_INDEX(s, d) = sccs_addStr((s), val))
#define	BAMHASH_SET(s, d, val)	(BAMHASH_INDEX(s, d) = sccs_addStr((s), val))
#define	RANDOM_SET(s, d, val)	(RANDOM_INDEX(s, d) = sccs_addStr((s), val))
#define	USERHOST_SET(s, d, val)	(USERHOST_INDEX(s, d) = sccs_addUniqStr((s), val))
#define	PATHNAME_SET(s, d, val)	(PATHNAME_INDEX(s, d) = sccs_addUniqStr((s), val))
#define	SORTPATH_SET(s, d, val)	(SORTPATH_INDEX(s, d) = sccs_addUniqStr((s), val))
#define	ZONE_SET(s, d, val)	(ZONE_INDEX(s, d) = sccs_addUniqStr((s), val))
#define	SYMLINK_SET(s, d, val)	(SYMLINK_INDEX(s, d) = sccs_addUniqStr((s), val))
#define	CSETFILE_SET(s, d, val)	(CSETFILE_INDEX(s, d) = sccs_addUniqStr((s), val))

/*
 * Extra in-memory only delta information
 */
typedef struct {
	char	*rev;		/* rev string */
} dextra;

#define	EXTRA(s, d)	((s)->extra + d)

#define	TAG(s, d)	(FLAGS(s, d) & D_TAG)
#define	NOFUDGE(s, d)	(DATE(s, d) - DATE_FUDGE(s, d))
#define	DANGLING(s, d)	(FLAGS(s, d) & D_DANGLING)
#define	SYMGRAPH(s, d)	(FLAGS(s, d) & D_SYMGRAPH)
#define	SYMLEAF(s, d)	(FLAGS(s, d) & D_SYMLEAF)
#define	INARRAY(s, d)	(FLAGS(s, d) & D_INARRAY)

#define	KID(s, d)	(s)->kidlist[d].kid
#define	SIBLINGS(s, d)	(s)->kidlist[d].siblings

#define	REV(s, d)	delta_rev(s, d)
#define	CLUDES(s, d)	((s)->heap.buf + CLUDES_INDEX(s, d))
#define	BAMHASH(s, d)	((s)->heap.buf + BAMHASH_INDEX(s, d))
#define	COMMENTS(s, d)	((s)->heap.buf + COMMENTS_INDEX(s, d))
#define	RANDOM(s, d)	((s)->heap.buf + RANDOM_INDEX(s, d))

#define	USER(s, d)	delta_user(s, d)
#define	HOSTNAME(s, d)	delta_host(s, d)
#define	USERHOST(s, d)	((s)->heap.buf + USERHOST_INDEX(s, d))
#define	ZONE(s,d)	((s)->heap.buf + ZONE_INDEX(s, d))
#define	SYMLINK(s,d)	((s)->heap.buf + SYMLINK_INDEX(s, d))
#define	CSETFILE(s,d)	((s)->heap.buf + CSETFILE_INDEX(s, d))
#define	PATHNAME(s,d)	((s)->heap.buf + PATHNAME_INDEX(s, d))
#define	SORTPATH(s,d)	((s)->heap.buf + SORTPATH_INDEX(s, d))

/*
 * Macros to get at an optional hidden string after the null
 */
#define	HIDDEN(p)		((p) + strlen(p) + 1)
#define	HIDDEN_BUILD(a, b)	aprintf("%s%c%s", (a), 0, (b))
#define	HIDDEN_DUP(p)		HIDDEN_BUILD(p, HIDDEN(p))

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
	u32	symname;		/* STABLE */
	ser_t	ser;			/* delta associated with this one */
					/* only for absolute revs, not LOD */
	ser_t	meta_ser;		/* where the symbol lives on disk */
} symbol;

#define	SYMNAME(s, sym)	((s)->heap.buf + (sym)->symname)

/*
 * Map used by things like serial map,
 * used to calculate from graph to set
 */
#define	S_INC	1
#define	S_EXCL	2
#define	S_PAR	4

typedef	struct sccs sccs;

#include "proj.h"
#include "bk-features.h"

extern	jmp_buf	exit_buf;
extern	char *upgrade_msg;

#define	exit(e)	longjmp(exit_buf, (e) + 1000)

#define	READER_LOCK_DIR	"BitKeeper/readers"
#define	WRITER_LOCK_DIR	"BitKeeper/writer"
#define	WRITER_LOCK	"BitKeeper/writer/lock"
#define	NESTED_WRITER_LOCK "BitKeeper/writer/nested_lock"
#define	NESTED_MUTEX	"BitKeeper/tmp/nl_mutex"

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

typedef	struct {
	ser_t	kid;
	ser_t	siblings;
} KIDS;

/*
 * struct sccs - the delta tree, the data, and associated junk.
 */
struct sccs {
	ser_t	tip;		/* the delta table list, 1.99 .. 1.0 */
	d_t	*slist;		/* array of delta structs */
	dextra	*extra;		/* array of extra delta info */
	symbol	*symlist;	/* array of symbols, oldest first */
	KIDS	*kidlist;	/* optional kid/sibling data */
	char	*defbranch;	/* defbranch, if set */
	int	numdeltas;	/* number of entries in the graph */
	off_t	size;		/* size of mapping */
	DATA	heap;		/* all strings in delta structs */
	hash	*uniqheap;	/* help collapse unique strings in hash */
	u32	*mg_symname;	/* symbol list use by mkgraph() */
	char	**mapping;
	FILE	*fh;		/* cached copy of the input file handle */
	FILE	*oldfh;		/* orig fh (no ungzip layer) */
	FILE	*outfh;		/* fh for writing x.file */
	char	*sfile;		/* SCCS/s.foo.c */
	char	*pfile;		/* SCCS/p.foo.c */
	char	*zfile;		/* SCCS/z.foo.c */
	char	*gfile;		/* foo.c */
	char	*symlink;	/* if gfile is a sym link, the destination */
	char	**usersgroups;	/* lm, beth, staff, etc */
	int	encoding_in;	/* ascii, uuencode, gzip, etc. */
	int	encoding_out;	/* encoding to write sfile */
	char	**flags;	/* flags in the middle that we didn't grok */
	char	**text;		/* descriptive text */
	u32	state;		/* GFILE/SFILE etc */
	u32	xflags;		/* cache of sccs_top()->xflags */
	mode_t	mode;		/* mode of the gfile */
	off_t	data;		/* offset to data in file */
	ser_t	rstart;	/* start of a range (1.1 - oldest) */
	ser_t	rstop;		/* end of range (1.5 - youngest) */
	ser_t	remote;	/* sccs_resolveFiles() sets this */
	ser_t	local;		/* sccs_resolveFiles() sets this */
	sum_t	 cksum;		/* SCCS chksum */
	sum_t	 dsum;		/* SCCS delta chksum */
	u32	added;		/* lines added by this delta (u32!) */
	u32	deleted;	/* and deleted */
	u32	same;		/* and unchanged */
	long	sumOff;		/* offset of the new delta cksum */
	long	adddelOff;	/* offset to tip add/delete/unchanged */
	time_t	gtime;		/* gfile modidification time */
	time_t	stime;		/* sfile modidification time */
	MDBM	*mdbm;		/* If state & S_HASH, put answer here */
	MDBM	*goneDB;	/* GoneDB used in the get_reg() setup */
	MDBM	*idDB;		/* id cache used in the get_reg() setup */
	u32	*fastsum;	/* Cache a lines array of the weave sums */
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
	u32	keydb_nopath:1;	/* don't compare path in sccs_findKey() */
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
 * All strings are malloced and cleared by calling free_pfile(&pf);
 */
typedef struct {
	char	*oldrev;	/* old tip rev */
	char	*newrev;	/* rev to be created */
	char	*iLst;		/* include revs for delta */
	char	*xLst;		/* exclude revs for delta */
	char	*mRev;		/* merge rev for delta */
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
	char	*sortkey;	/* sortable key of this delta, if different */
	char	*initFile;	/* RESYNC/BitKeeper/init-1, only if !initMmap */
	MMAP	*initMmap;	/* points into mmapped patch */
	char	*diffFile;	/* RESYNC/BitKeeper/diff-1, only if !diffMmap */
	MMAP	*diffMmap;	/* points into mmapped patch */
	ser_t	serial;		/* in cset path, save the corresponding ser */
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
#define PATCH_FEATURES	"# Patch features:\t"

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
	u16	notUrl:1;	/* addr was a file path (had no URL scheme) */
	u16	noLocalRepo:1;	/* local repo not related to connection */
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

/* For repo_nfiles() et al. */
typedef struct {
	u32	tot;	/* tot # files in repo */
	u32	usr;	/* # user files (not under BitKeeper/) */
} filecnt;

int	sccs_admin(sccs *sc, ser_t d, u32 flgs,
	    admin *f, admin *l, admin *u, admin *s, char *mode, char *txt);
int	sccs_adminFlag(sccs *sc, u32 flags);
int	sccs_cat(sccs *s, u32 flags, char *printOut);
int	sccs_delta(sccs *s, u32 flags, ser_t d, MMAP *init, MMAP *diffs,
		   char **syms);
int	sccs_diffs(sccs *s, char *r1, char *r2, u32 flags, u32 kind, FILE *);
int	sccs_encoding(sccs *s, off_t size, char *enc);
int	sccs_get(sccs *s,
	    char *rev, char *mRev, char *i, char *x, u32 flags, char *out);
int	sccs_hashcount(sccs *s);
int	sccs_clean(sccs *s, u32 flags);
int	sccs_unedit(sccs *s, u32 flags);
int	sccs_info(sccs *s, u32 flags);
int	sccs_prs(sccs *s, u32 flags, int reverse, char *dspec, FILE *out);
int	sccs_prsdelta(sccs *s, ser_t d, int flags, char *dspec, FILE *out);
char	*sccs_prsbuf(sccs *s, ser_t d, int flags, char *dspec);
ser_t	sccs_findDate(sccs *s, char *date, int roundup);
int	sccs_patheq(char *file1, char *file2);
ser_t	sccs_findDelta(sccs *s, ser_t d);
sccs	*sccs_init(char *filename, u32 flags);
sccs	*sccs_restart(sccs *s);
sccs	*sccs_reopen(sccs *s);
int	sccs_open(sccs *s, struct stat *sp);
void	sccs_free(sccs *);
ser_t	sccs_newdelta(sccs *s);
void	sccs_freedelta(sccs *s, ser_t d);
ser_t	sccs_insertdelta(sccs *s, ser_t d, ser_t serial);
void	sccs_close(sccs *);
int	sccs_csetWrite(sccs *s, char **cweave);
sccs	*sccs_csetInit(u32 flags);
char	**sccs_files(char **, int);
ser_t	sccs_parseArg(sccs *s, ser_t d, char what, char *arg, int defaults);
void	sccs_whynot(char *who, sccs *s);
void	sccs_ids(sccs *s, u32 flags, FILE *out);
void	sccs_inherit(sccs *s, ser_t d);
int	sccs_hasDiffs(sccs *s, u32 flags, int inex);
ser_t	sccs_getInit(sccs *s, ser_t d, MMAP *f, u32 flags,
		      int *errorp, int *linesp, char ***symsp);
ser_t	sccs_ino(sccs *);
int	sccs_userfile(sccs *);
int	sccs_metafile(char *file);
int	sccs_rmdel(sccs *s, ser_t d, u32 flags);
int	sccs_stripdel(sccs *s, char *who);
int	stripdel_fixTable(sccs *s, int *pcnt);
int	sccs_getdiffs(sccs *s, char *rev, u32 flags, char *printOut);
int	sccs_patchDiffs(sccs *s, ser_t *patchmap, char *printOut);
void	sccs_pdelta(sccs *s, ser_t d, FILE *out);
ser_t	sccs_key2delta(sccs *sc, char *key);
int	sccs_keyunlink(char *key, MDBM *idDB, MDBM *dirs, u32 flags);
char	*sccs_impliedList(sccs *s, char *who, char *base, char *rev);
int	sccs_sdelta(sccs *s, ser_t, char *);
void	sccs_md5delta(sccs *s, ser_t d, char *b64);
void	sccs_sortkey(sccs *s, ser_t d, char *buf);
void	sccs_key2md5(char *rootkey, char *deltakey, char *b64);
void	sccs_setPath(sccs *s, ser_t d, char *newpath);
void	sccs_syncRoot(sccs *s, char *key);
ser_t	sccs_csetBoundary(sccs *s, ser_t);
void	sccs_shortKey(sccs *s, ser_t, char *);
int	sccs_resum(sccs *s, ser_t d, int diags, int dont);
int	cset_resum(sccs *s, int diags, int fix, int spinners, int takepatch);
char	**cset_mkList(sccs *cset);
int	cset_bykeys(const void *a, const void *b);
int	cset_byserials(const void *a, const void *b);
int	sccs_newchksum(sccs *s);
ser_t	*addSerial(ser_t *space, ser_t s);
void	sccs_perfile(sccs *, FILE *);
sccs	*sccs_getperfile(sccs *, MMAP *, int *);
char	*sccs_gethost(void);
char	*sccs_realhost(void);
char	*sccs_host(void);
char	**sccs_getComments(char *prompt);
int	sccs_badTag(char *, char *, int);
MDBM    *sccs_keys2mdbm(FILE *f);
void	sfileUnget(void);
char	*sfileNext(void);
char	*sfileRev(void);
char	*sfileFirst(char *cmd, char **Av, int Flags);
int	sfileDone(void);
int	sfiles(char **av);
int	sfilesDied(int killit);
ser_t	sccs_findrev(sccs *, char *);
ser_t	sccs_top(sccs *);
ser_t	sccs_findKey(sccs *, char *);
int	isKey(char *key);
ser_t	sccs_findMD5(sccs *s, char *md5);
ser_t	sccs_dInit(ser_t, char, sccs *, int);
char	*sccs_getuser(void);
void	sccs_resetuser(void);
void	sccs_resethost(void);
char	*sccs_realuser(void);
char	*sccs_user(void);

char	*delta_rev(sccs *s, ser_t d);
char	*delta_user(sccs *s, ser_t d);
char	*delta_host(sccs *s, ser_t d);
void	delta_print(sccs *s, ser_t d);

ser_t	modeArg(sccs *s, ser_t d, char *arg);
int	fileType(mode_t m);
char	chop(char *s);
void	touch_checked(void);
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
ser_t 	mkOneZero(sccs *s);
int	isCsetFile(char *);
int	cset_inex(int flags, char *op, char *revs);
void	sccs_fixDates(sccs *);
int	sccs_xflags(sccs *s, ser_t d);
char	*xflags2a(u32 flags);
u32	a2xflag(char *str);
void	sccs_mkroot(char *root);
int	sccs_parent_revs(sccs *s, char *rev, char **revP, char **revM);
char	*sccs_setpathname(sccs *s);
ser_t	sccs_prev(sccs *s, ser_t d);
ser_t	sccs_next(sccs *s, ser_t d);
int	sccs_reCache(int quiet);
int	sccs_findtips(sccs *s, ser_t *a, ser_t *b);
int	sccs_resolveFiles(sccs *s);
sccs	*sccs_keyinit(project *proj, char *key, u32 flags, MDBM *idDB);
int	sccs_lock(sccs *, char);	/* respects repo locks */
void	sccs_unlock(sccs *, char);

int	sccs_lockfile(char *lockfile, int wait, int quiet);
int	sccs_stalelock(char *lockfile, int discard);
int	sccs_unlockfile(char *file);
int	sccs_mylock(char *lockf);
int	sccs_readlockf(char *file, pid_t *pidp, char **hostp, time_t *tp);

char	*sccs_utctime(sccs *s, ser_t d);
int	delta_strftime(char *out, int sz, char *fmt, sccs *s, ser_t d);
char	*delta_sdate(sccs *s, ser_t d);
ser_t	sccs_kid(sccs *s, ser_t d);
void	sccs_mkKidList(sccs *s);
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
int	sccs_isleaf(sccs *s, ser_t d);
int	emptyDir(char *dir);
int	gone(char *key, MDBM *db);
int	sccs_mv(char *, char *, int, int, int, int);
ser_t	sccs_gca(sccs *, ser_t l, ser_t r, char **i, char **x);
char	*_relativeName(char *gName, int isDir,
	    int mustHaveRmarker, int wantRealName, project *proj);
char	*findBin(void);
int 	prompt(char *msg, char *buf);
void	parse_url(char *url, char *host, char *path);
int	parallel(char *path);
char	*sccs_Xfile(sccs *s, char type);
FILE	*sccs_startWrite(sccs *s);
int	sccs_finishWrite(sccs *s, FILE **f);
void	sccs_abortWrite(sccs *s, FILE **f);
int	uniq_adjust(sccs *s, ser_t d);
char	*uniq_keysHome(void);
int	uniq_lock(void);
int	uniq_unlock(void);
int	uniq_open(void);
time_t	uniq_drift(void);
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
int	repo_nfiles(project *proj, filecnt *nf);
void	repo_nfilesUpdate(filecnt *nf);
int	comments_save(char *s);
int	comments_savefile(char *s);
int	comments_got(void);
void	comments_done(void);
char	**comments_return(char *prompt);
ser_t	comments_get(char *gfile, char *rev, sccs *s, ser_t d);
void	comments_writefile(char *file);
int	comments_checkStr(u8 *s, int len);
char	*shell(void);
int	bk_sfiles(char *opts, int ac, char **av);
int	outc(char c);
void	error(const char *fmt, ...);
MDBM	*loadConfig(project *p, int forcelocal);
int	ascii(char *file);
char	*sccs_rmName(sccs *s);
char	*key2rmName(char *rootkey);
int	sccs_rm(char *name, int force);
void	sccs_rmEmptyDirs(char *path);
void	do_prsdelta(char *file, char *rev, int flags, char *dspec, FILE *out);
char 	**get_http_proxy(char *host);
int	confirm(char *msg);
int	csetCreate(sccs *cset, int flags, char *files, char **syms);
int	cset_setup(int flags);
char	*separator(char *);
int	trigger(char *cmd, char *when);
void	trigger_setQuiet(int yes);
void	cmdlog_start(char **av, int bkd_mode);
void	cmdlog_addnote(char *key, char *val);
int	cmdlog_end(int ret, int bkd_mode);
void	cmdlog_lock(int flags);
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
int	sccs_tagMerge(sccs *s, ser_t d, char *tag);
int	sccs_tagleaves(sccs *, ser_t *, ser_t *);
u8	*sccs_set(sccs *, ser_t, char *iLst, char *xLst);
int	sccs_graph(sccs *s, ser_t d, u8 *map, char **inc, char **exc);
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
void	sccs_rmUncommitted(int quiet, char ***chkfiles);
void	rmEmptyDirs(int quiet);
int	after(int quiet, int verbose, char *rev);
int	consistency(int quiet);
int	diff_gfile(sccs *s, pfile *pf, int expandKeyWord, char *tmpfile);
char	*getCSetFile(project *p);
int	spawn_cmd(int flag, char **av);
pid_t	mkpager(void);
int	getRealName(char *path, MDBM *db, char *realname);
int	addsym(sccs *s, ser_t metad, int graph, char *tag);
int	delta_table(sccs *s, FILE *out, int willfix);
int	walksfiles(char *dir, walkfn fn, void *data);
ser_t	getSymlnkCksumDelta(sccs *s, ser_t d);
hash	*generateTimestampDB(project *p);
int	timeMatch(project *proj, char *gfile, char *sfile, hash *timestamps);
void	dumpTimestampDB(project *p, hash *db);
void	updateTimestampDB(sccs *s, hash *timestamps, int diff);
struct tm *utc2tm(time_t t);
int	sccs_setStime(sccs *s, time_t newest);
void	ids(void);
void	http_hdr(void);
int	check_rsh(char *remsh);
void	sccs_color(sccs *s, ser_t d);
int	out(char *buf);
int	getlevel(void);
ser_t	cset_insert(sccs *s, MMAP *iF, MMAP *dF, ser_t parent, int fast);
int	cset_write(sccs *s, int spinners, int fast);
sccs	*cset_fixLinuxKernelChecksum(sccs *s);
int	cweave_init(sccs *s, int extras);
int	isNullFile(char *rev, char *file);
unsigned long	ns_sock_host2ip(char *host, int trace);
unsigned long	host2ip(char *host, int trace);
int	fileTypeOk(mode_t m);
int	sccs_tagLeaf(sccs *s, ser_t d, ser_t md, char *tag);
int	sccs_scompress(sccs *s, int flags);
int	mkBkRootIcon(char *path);
int	unmkBkRootIcon(char *path);
void	sccs_tagcolor(sccs *s, ser_t d);
int	checkXflags(sccs *s, ser_t d, int what);
void	metaUnionResync1(void);
void	metaUnionResync2(void);
int	sccs_istagkey(char *key);
char	*testdate(time_t t);
void	putroot(char *where);
int	uuencode(FILE *in, FILE *out);
int	uudecode(FILE *in, FILE *out);
void	sccs_unmkroot(char *path);
int	sccs_needSwap(sccs *s, ser_t p, ser_t m);
int	chk_host(void);
int	chk_user(void);
int	chk_nlbug(sccs *s);
int	chk_permissions(void);
int	fix_gmode(sccs *s, int gflags);
int	do_checkout(sccs *s);
int	unsafe_path(char *s);
int	hasTriggers(char *cmd, char *when);
void	comments_cleancfile(sccs *s);
int	comments_readcfile(sccs *s, int prompt, ser_t d);
int	comments_prompt(char *file);
void	saveEnviroment(char *patch);
void	restoreEnviroment(char *patch);
int	run_check(int quiet, char **flist, char *opts, int *did_partial);
int	full_check(void);
char	*key2path(char *key, MDBM *idDB, MDBM **m2k);
int	check_licensesig(char *key, char *sign, int version);
char	*hashstr(char *str, int len);
char	*hashstream(int fd);
char	*secure_hashstr(char *str, int len, char *key);
int	isNetworkFS(char *path);
void	sccs_saveNum(FILE *f, int num, int sign);
int	sccs_eachNum(char **linep, int *signp);

#define	KEY_LEASE		0
#define	KEY_BK_AUTH_HMAC	1
#define	KEY_LCONFIG		2
#define	KEY_UPGRADE		3
#define	KEY_SIGNEDFILE		4
#define	KEY_SEED		5
#define	KEY_EULA		6
char	*makestring(int keynum);

int	attach_name(sccs *cset, char *name, int setmarks);
void	notice(char *key, char *arg, char *type);
void	save_log_markers(void);
void	update_log_markers(int verbose);
ser_t	sccs_getedit(sccs *s, char **revp);
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
typedef	void (*set_pfunc)(sccs *, ser_t);
u8	*set_get(sccs *s, char *rev);
void	set_list(sccs *s, char *rev, set_pfunc p);
void	set_member(sccs *s, char *rev, u8 *map, set_pfunc p);
void	set_diff(sccs *s, u8 *a, u8 *b, set_pfunc p);
void	set_and(sccs *s, u8 *a, u8 *b, set_pfunc p);
void	set_or(sccs *s, u8 *a, u8 *b, set_pfunc p);
void	set_xor(sccs *s, u8 *a, u8 *b, set_pfunc p);
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
int	sccs_fastWeave(sccs *s, ser_t *weavemap, ser_t *patchmap,
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
sum_t	almostUnique(void);
int	uninstall(char *path, int upgrade);
int	remote_bk(int quiet, int ac, char **av);
void	dspec_eval(FILE * out, sccs *s, ser_t d, char *start);
void	dspec_printline(sccs *s, FILE *out);
void	dspec_printeach(sccs *s, FILE *out);
int	kw2val(FILE *out, char *kw, int len, sccs *s, ser_t d);
void	show_s(sccs *s, FILE *out, char *data, int len);
void	show_d(sccs *s, FILE *out, char *format, int num);
void	comments_set(sccs *s, ser_t d, char **comments);
void	gdb_backtrace(void);
char	*bp_lookup(sccs *s, ser_t d);
ser_t	bp_fdelta(sccs *s, ser_t d);
int	bp_fetch(sccs *s, ser_t din);
int	bp_fetchData(void);
int	bp_fetchkeys(char *me, project *p, int quiet, char **keys, u64 todo);
int	bp_get(sccs *s, ser_t d, u32 flags, char *out);
int	bp_delta(sccs *s, ser_t d);
int	bp_diff(sccs *s, ser_t d, char *gfile);
int	bp_updateServer(char *range, char *list, int quiet);
int	bp_sharedServer(void);
char	*bp_serverURL(char *out);
char	*bp_serverID(char *out, int notme);
char	*bp_serverURL2ID(char *url);
void	bp_setBAMserver(char *path, char *url, char *repoid);
int	bp_hasBAM(void);
u32	send_BAM_sfio(FILE *wf, char *bp_keys, u64 bpsz, int gzip, int quiet);
int	bkd_BAM_part3(remote *r, char **env, int quiet, char *range);
int	bp_sendkeys(FILE *f, char *range, u64 *bytes, int gzip);
int	detach(int quiet, int verbose);
int	zgets_hread(void *token, u8 **buf);
int	zgets_hfread(void *token, u8 **buf);
void	zputs_hfwrite(void *token, u8 *data, int len);
sum_t	fputdata(sccs *s, u8 *buf, FILE *out);
char	*psize(u64 bytes);
u64	scansize(char *bytes);
void	idcache_update(char **files);
int	idcache_write(project *p, MDBM *idDB);
void	cset_savetip(sccs *s);
void	clearCsets(sccs *s);
void	sccs_rdweaveInit(sccs *s);
char	*short_random(char *str, int len);
char	*sccs_nextdata(sccs *s);
int	sccs_rdweaveDone(sccs *s);
int	hasLocalWork(char *gfile);
char	*goneFile(void);
char	*sgoneFile(void);
int	keycmp(const void *k1, const void *k2);
int	keycmp_nopath(char *k1, char *k2);
int	key_sort(const void *a, const void *b);
int	earlier(sccs *s, ser_t a, ser_t b);
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
int	remap_open(project *proj, char *rel, int flags, mode_t mode);
int	remap_utime(project *proj, char *rel, const struct utimbuf *utb);
int	remap_lstat(project *proj, char *rel, struct stat *sb);
int	remap_unlink(project *proj, char *rel);
int	remap_rename(project *proj1, char *old, project *proj2, char *new);
int	remap_link(project *proj1, char *old, project *proj2, char *new);
int	remap_chmod(project *proj, char *rel, mode_t mode);
int	remap_mkdir(project *proj, char *dir, mode_t mode);
int	remap_rmdir(project *proj, char *dir);
char	**remap_getdir(project *proj, char *dir);
char	*remap_realBasename(project *proj, char *rel, char *realname);
int	remap_access(project *proj, char *file, int mode);
int	bk_urlArg(char ***urls, char *arg);
char	**bk_saveArg(char **nav, char **av, int c);
void	bk_badArg(int c, char **av)
#ifdef __GNUC__
	__attribute__((noreturn))
#endif
;
int	bk_nested2root(int standalone);
void	usage(void)
#ifdef __GNUC__
	__attribute__((noreturn))
#endif
;
int	bk_notLicensed(project *p, u32 bits, int quiet);
char	*file_fanout(char *file);
void	upgrade_maybeNag(char *out);
int	attr_update(void);
int	attr_write(char *file);
int	bk_badFilename(char *name);

#ifdef	WIN32
void	notifier_changed(char *fullpath);
void	notifier_flush(void);
#else
#define	notifier_changed(x)
#define	notifier_flush()
#endif

int	sccs_defRootlog(sccs *cset);
void	bk_setConfig(char *key, char *val);
u32	sccs_addStr(sccs *s, char *str);
void	sccs_appendStr(sccs *s, char *str);
u32	sccs_addUniqStr(sccs *s, char *str);
typedef	struct MAP MAP;
MAP	*datamap(void *start, int len, FILE *f, long off);
void	dataunmap(MAP *map);


#define	RGCA_ALL	0x1000
#define	RGCA_STANDALONE	0x2000
int	repogca(char **urls, char *dspec, u32 flags, FILE *out);

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
extern	char	*title;
extern	char	*log_versions;

#define	componentKey(k) (strstr(k, "/ChangeSet|") != (char*)0)
#define	changesetKey(k) (strstr(k, "|ChangeSet|") != (char*)0)

/*
 * Locking flags for cmdlog_start() and cmdlog_lock()
 */
#define	CMD_BYTES		0x00000001	/* log command byte count */
#define	CMD_WRLOCK		0x00000002	/* write lock */
#define	CMD_RDLOCK		0x00000004	/* read lock */
#define	CMD_REPOLOG		0x00000008	/* log in repolog */
#define	CMD_QUIT		0x00000010	/* mark quit command */
#define	CMD_NOREPO		0x00000020	/* don't assume in repo */
#define	CMD_NESTED_WRLOCK	0x00000040	/* nested write lock */
#define	CMD_NESTED_RDLOCK	0x00000080	/* nested read lock */
#define	CMD_SAMELOCK		0x00000100	/* grab a repolock that matches
						 * the nested lock we have */
#define	CMD_COMPAT_NOSI		0x00000200	/* compat, no server info */
#define	CMD_IGNORE_RESYNC	0x00000400	/* ignore resync lock */
#define	CMD_RDUNLOCK		0x00001000	/* unlock a previous READ */
#define	CMD_BKD_CMD		0x00002000	/* command comes from bkd.c */

#define	LOGVER			0		/* dflt idx into log_versions */

#endif	/* _SCCS_H_ */
