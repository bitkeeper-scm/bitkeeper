/* %W% Copyright (c) 1997-2001 Larry McVoy */
#ifndef	_SCCS_H_
#define	_SCCS_H_

#include "mmap.h"

/*
 * Flags that modify some operation (passed to sccs_*).
 *
 * There are two ranges of values, 0x0000000f and 0xfffffff0.
 * The first is for flags which may be meaningful to all functions.
 * The second is for flags which are function specific.
 * Be VERY CAREFUL to not mix and match.  If I see a DELTA_ in sccs_get()
 * I will be coming after you with a blowtorch.
 */
#define	SILENT		0x00000001	/* do work quietly */
#define	PRINT		0x00000002	/* get/delta/clean [diffs] to stdout */
#define	NEWFILE		0x00000004	/* delta -i: create initial file */
#define	NEWCKSUM	0x00000008	/* Redo checksum */

#define	INIT_avail	0x10000000	/* OLD: map the file read/write */
#define	INIT_NOCKSUM	0x20000000	/* don't do the checksum */
#define	INIT_FIXDTIME	0x40000000	/* use g file mod time as delat time */
#define	INIT_SAVEPROJ	0x80000000	/* project is loaned, do not free it */
#define	INIT_NOSTAT	0x01000000	/* do not look for {p,x,z,c} files */
#define	INIT_HAScFILE	0x02000000	/* has c.file */
#define	INIT_HASgFILE	0x04000000	/* has g.file */
#define	INIT_HASpFILE	0x08000000	/* has p.file */
#define	INIT_HASxFILE	0x00100000	/* has x.file */
#define	INIT_HASzFILE	0x00200000	/* has z.file */
#define	INIT_ONEROOT	0x00400000	/* one root mode i.e not split root */
#define	INIT_NOGCHK	0x00800000	/* do not fail on gfile checks */
#define	INIT_FIXSTIME	0x00010000	/* force sfile mtime < gfile mtime */

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
#define	GET_DTIME	0x00080000	/* gfile get delta's mode time */
#define	GET_NOHASH	0x00001000	/* force regular file, ignore S_HASH */
#define	GET_HASHONLY	0x00002000	/* skip the file */
#define	GET_DIFFS	0x00004000	/* get -D, regular diffs */
#define	GET_BKDIFFS	0x00008000	/* get -DD, BK (rick's) diffs */
#define	GET_HASHDIFFS	0x00000100	/* get -DDD, 0a0 hash style diffs */
#define	GET_SUM		0x00000200	/* used to force dsum in getRegBody */
#define GET_NOREGET	0x00000400	/* get -S: skip gfiles that exist */
#define GET_DIFFTOT	0x00000800	/* hasDiffs() false if !TOT */
#define	GET_FULLPATH	0x00000010	/* like GET_MODNAME but full relative */
#define	GET_HASH	0x00000020	/* force hash file, ignore ~S_HASH */
#define	GET_SEQ		0x00000040	/* get -O: prefix with sequence no */
#define	GET_PREFIX	\
    (GET_REVNUMS|GET_USER|GET_LINENUM|GET_MODNAME|\
     GET_FULLPATH|GET_PREFIXDATE|GET_SEQ)

#define CLEAN_SHUTUP	0x20000000	/* clean -Q: quiet mode */
#define	CLEAN_SKIPPATH	0x40000000	/* ignore path change; for log tree */
#define	CLEAN_CHECKONLY	0x80000000	/* don't delete gfile, just check */

#define	DELTA_AUTO	0x10000000	/* delta -a: auto check-in mode */
#define	DELTA_SAVEGFILE	0x20000000	/* delta -n: save edited gfile */
#define	DELTA_DONTASK	0x40000000	/* don't ask for comments */
#define	DELTA_PATCH	0x80000000	/* delta -R: respect rev */
#define	DELTA_EMPTY	0x01000000	/* initialize with empty file */
#define	DELTA_FORCE	0x02000000	/* delta -f: force a delta */
#define	DELTA_HASH	0x04000000	/* treat as hash (MDBM) file */
#define	DELTA_NOPENDING	0x08000000	/* don't create pending marker */
#define	DELTA_CFILE	0x00100000	/* read cfile and do not prompt */

#define	ADMIN_FORMAT	0x10000000	/* check file format (admin) */
#define	ADMIN_ASCII	0x20000000	/* check file format (admin) */
#define	ADMIN_TIME	0x40000000	/* warn about time going backwards */
#define	ADMIN_SHUTUP	0x80000000	/* don't be noisy about bad revs */
#define	ADMIN_BK	0x01000000	/* check BitKeeper invariants */
#define	ADMIN_GONE	0x02000000	/* check integrity w/o GONE deltas */
#define	ADMIN_ADD1_0	0x04000000	/* insert a 1.0 delta */
#define	ADMIN_RM1_0	0x08000000	/* remove a 1.0 delta */
#define	ADMIN_OBSCURE	0x00100000	/* remove comments, obscure data */
#define	ADMIN_FORCE	0x00200000	/* use Z lock; for pull/cweave */

#define	ADMIN_CHECKS	(ADMIN_FORMAT|ADMIN_ASCII|ADMIN_TIME|ADMIN_BK)

#define	PRS_META	0x10000000	/* show metadata */
#define	PRS_SYMBOLIC	0x20000000	/* show revs as beta1, etc. Not done */
#define	PRS_PATCH	0x40000000	/* print in patch format */
#define PRS_ALL		0x80000000	/* scan all revs, not just type D */
#define	PRS_GRAFT	0x01000000	/* put the perfile in the patch */
#define	PRS_LF		0x02000000	/* terminate non-empty output with LF */
#define	PRS_LOGGING	0x04000000	/* add logging bit to xflags */
#define	PRS_COMPAT	0x08000000	/* for makepatch -C, send old tags */
#define	PRS_LOGMARK	0x00100000	/* want log marker */

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
#define	SF_NOCSET	0x00000040	/* do not autoexpand cset files */

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
#define	S_RANGE2	0x00001000	/* second call for date|rev range */
#define	S_SET		0x00002000	/* the tree is marked with a set */
#define S_CACHEROOT	0x00004000	/* don't free the root entry */
#define	S_FAKE_1_0	0x00008000	/* the 1.0 delta is a fake */
#define	S_SAVEPROJ	0x00010000	/* do not free the project struct */
#define	S_FORCELOGGING	0x00020000	/* Yuck - force it to logging */
#define S_CONFIG	0x00040000	/* this is a config file */

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
#define	X_SINGLE	0x00000100	/* single user, inherit user/host */
/*	X_DO_NOT_USE	0x00000200	   was used shortly, never reuse */
#define	X_LOGS_ONLY	0x00000400	/* this is a logging repository */
#define	X_EOLN_NATIVE	0x00000800	/* use eoln native to this OS */
#define	X_LONGKEY	0x00001000	/* all keys are long format */
#define	X_KV		0x00002000	/* key value file */
#define	X_NOMERGE	0x00004000	/* treat as binary even if ascii */
					/* flags which can be changed */
#define	X_MONOTONIC	0x00008000	/* file rolls forward only */
#define	X_MAYCHANGE	(X_RCS | X_YEAR4 | X_SHELL | X_EXPAND1 | \
			X_SCCS | X_EOLN_NATIVE | X_KV | X_NOMERGE | X_MONOTONIC)
/* default set of flags when we create a file */
#define	X_DEFAULT	(X_BITKEEPER|X_CSETMARKED|X_EOLN_NATIVE)
#define	X_REQUIRED	(X_BITKEEPER|X_CSETMARKED)

/* bits for the xflags checker - lovely having legacy files, eh? */
#define	XF_DRYRUN	0x0001	/* say what would be done */
#define	XF_STATUS	0x0002	/* just return status, for check/admin -h */

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
#define IS_TEXT(s)	(((s)->encoding & E_DATAENC) == E_ASCII)
#define IS_BINARY(s)	(((s)->encoding & E_DATAENC) == E_UUENCODE)
#define WRITABLE(s)	(IS_WRITABLE(s) && isRegularFile((s)->mode))
#define	CSET(s)		((s)->state & S_CSET)
#define	CONFIG(s)	((s)->state & S_CONFIG)
#define	READ_ONLY(s)	((s)->state & S_READ_ONLY)
#define	SET(s)		((s)->state & S_SET)
#define	MK_GONE(s, d)	(s)->hasgone = 1; (d)->flags |= D_GONE

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
#define	SINGLE(s)	((s)->xflags & X_SINGLE)
#define	LOGS_ONLY(s)	((s)->xflags & X_LOGS_ONLY)
#define	EOLN_NATIVE(s)	((s)->xflags & X_EOLN_NATIVE)
#define	LONGKEY(s)	((s)->xflags & X_LONGKEY)
#define	KV(s)		((s)->xflags & X_KV)
#define	NOMERGE(s)	((s)->xflags & X_NOMERGE)
#define	MONOTONIC(s)	((s)->xflags & X_MONOTONIC)

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
 * Flags for command log
 */
#define CMD_BYTES	0x00000001	/* log command byte count */
#define CMD_FAST_EXIT	0x00000002	/* exit when done */
#define CMD_WRLOCK	0x00000004	/* write lock */
#define CMD_RDLOCK	0x00000008	/* read lock */
#define CMD_WRUNLOCK	0x00000010	/* write unlock */
#define CMD_RDUNLOCK	0x00000020	/* read unlock */
#define CMD_RETRYLOCK	0x00000040	/* if lock failed, retry */

/*
 * Signal handling.
 */
#define	SIG_IGNORE	0x0001		/* ignore the specified signal */
#define	SIG_DEFAULT	0x0002		/* restore old handler */

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

#define	MAXREV	24	/* 99999.99999.99999.99999 */

#define	OPENLOG_ADDR	"logging@openlogging.org"
#define	OPENLOG_URL	"http://config.openlogging.org:80////LOG_ROOT///"
#define	OPENLOG_BACKUP	"http://config2.openlogging.org:80////LOG_ROOT///"
#define	OPENLOG_HOST	"config.openlogging.org"
#define	OPENLOG_HOST1   "config2.openlogging.org"
#define	OPENLOG_LEASE	"http://lease.openlogging.org:80"
#define	BK_WEBMAIL_URL	"http://webmail.bitkeeper.com:80"
#define	BK_HOSTME_SERVER "hostme.bkbits.net"
#define	WEB_BKD_CGI	"web_bkd"
#define	HOSTME_CGI	"hostme_cgi"
#define	WEB_MAIL_CGI	"web_mail"
#define	LEASE_CGI	"bk_lease"
#define	BK_CONFIG_URL	"http://config.bitkeeper.com:80"
#define	BK_CONFIG_BCKUP	"http://config2.bitkeeper.com:80"
#define	BK_CONFIG_CGI	"bk_config"
#define	SCCSTMP		"SCCS/T.SCCSTMP"
#define	BKDIR		"BitKeeper"
#define	BKTMP		"BitKeeper/tmp"
#define	BKROOT		"BitKeeper/etc"
#define	BKMASTER	"BitKeeper/etc/.master"
#define	GONE		"BitKeeper/etc/gone"
#define	CSETS_IN	"BitKeeper/etc/csets-in"
#define	CSETS_OUT	"BitKeeper/etc/csets-out"
#define	SGONE		"BitKeeper/etc/SCCS/s.gone"
#define	CHANGESET	"SCCS/s.ChangeSet"
#define	GCHANGESET	"ChangeSet"
#define	LOGGING_OK	"BitKeeper/etc/SCCS/s.logging_ok"
#define	GLOGGING_OK	"BitKeeper/etc/logging_ok"
#define	IDCACHE		"BitKeeper/etc/SCCS/x.id_cache"
#define	IDCACHE_LOCK	"BitKeeper/etc/SCCS/z.id_cache"
#define	DFILE		"BitKeeper/etc/SCCS/x.dfile"
#define	WEBMASTER	"BitKeeper/etc/webmaster"
#define	CHECKED		"BitKeeper/log/checked"
#define	REPO_ID		"BitKeeper/log/repo_id"
#define	BKSKIP		".bk_skip"
#define	TMP_MODE	0666
#define	GROUP_MODE	0664

#define	UNKNOWN_USER	"anon"
#define	UNKNOWN_HOST	"nowhere"

#define BK_FREE		0
#define BK_BASIC	1
#define BK_PRO		2
#define BK_BADMODE	999
int	bk_mode(void);

#define	CNTLA_ESCAPE	'\001'	/* escape character for ^A is also a ^A */
#define	isData(buf)	((buf[0] != '\001') || \
			    ((buf[0] == CNTLA_ESCAPE) && (buf[1] == '\001')))
#define	seekto(s,o)	(s)->where = ((s)->mmap + o)
#define	eof(s)		(((s)->encoding & E_GZIP) ? \
			    zeof() : ((s)->where >= (s)->mmap + (s)->size))
#define	new(p)		p = calloc(1, sizeof(*p))

typedef	unsigned short	ser_t;
typedef	unsigned short	sum_t;
typedef	char		**globv;

#include "liblines.h"

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
 * Fri Nov 29 2002: and it keeps growing.  We need to rethink this a bit, we
 * could lose the mr and ignore fields, we never use those, just turn the
 * file to readonly if we ever see them.  We could also move infrequently
 * used fields to a list hanging off of the sccs struct, i.e., random,
 * csetFile, symlink.  We could lose sdate, we're going to have to
 * eventually when the date is a time_t in the binary s.file.  That's at
 * least 20 bytes.  And move the "type" to bit fields and put it down
 * next to the other ones.  That's 260K for the kernel's cset file.
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
	u32	xflags;			/* timesafe x flags */
	/* In memory only stuff */
	u16	r[4];			/* 1.2.3 -> 1, 2, 3, 0 */
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
	char	type;			/* Delta or removed delta */
} delta;
#define	TAG(d)	((d)->type == 'R')

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

#define	exit(e)	longjmp(exit_buf, (e) + 1000)

#define	READER_LOCK_DIR	"BitKeeper/readers"
#define	WRITER_LOCK_DIR	"BitKeeper/writer"
#define	WRITER_LOCK	"BitKeeper/writer/lock"

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
#define	SCCS_VERSION_COMPAT	3	/* for opull/opush */
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
	u32	state;		/* GFILE/SFILE etc */
	u32	xflags;		/* cache of sccs_top()->xflags */
	mode_t	mode;		/* mode of the gfile */
	off_t	data;		/* offset to data in file */
	delta	*rstart;	/* start of a range (1.1 - oldest) */
	delta	*rstop;		/* end of range (1.5 - youngest) */
	sum_t	 cksum;		/* SCCS chksum */
	sum_t	 dsum;		/* SCCS delta chksum */
	u32	added;		/* lines added by this delta (u32!) */
	u32	deleted;	/* and deleted */
	u32	same;		/* and unchanged */
	off_t	sumOff;		/* offset of the new delta cksum */
	time_t	gtime;		/* gfile modidification time */
	MDBM	*mdbm;		/* If state & S_HASH, put answer here */
	MDBM	*findkeydb;	/* Cache a map of delta key to delta* */
	project	*proj;		/* If in BK mode, pointer to project */
	u16	version;	/* file format version */
	u16	userLen;	/* maximum length of any user name */
	u16	revLen;		/* maximum length of any rev name */
	loc	*locs;		/* for cset data */ 
	u32	iloc;		/* index to element in *loc */ 
	u32	nloc;		/* # of element in *loc */ 
	u32	initFlags;	/* how we were opened */
	u32	cksumok:1;	/* check sum was ok */
	u32	cksumdone:1;	/* check sum was checked */
	u32	grafted:1;	/* file has grafts */
	u32	bad_dsum:1;	/* patch checksum mismatch */
	u32	io_error:1;	/* had an output error, abort */
	u32	io_warned:1;	/* we told them about the error */
	u32	prs_output:1;	/* prs printed something */
	u32	bitkeeper:1;	/* bitkeeper file */
	u32	prs_odd:1;	/* for :ODD: :EVEN: in dspecs */
	u32	unblock:1;	/* sccs_free: only if set */
	u32	hasgone:1;	/* this graph has D_GONE deltas */
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
	u16	type:3;		/* address type, nfs/bk/http/file/ssh/rsh */
	u16	loginshell:1;	/* if set, login shell is the bkd */
	u16	trace:1;	/* for debug, trace send/recv msg */
	u16	isSocket:1;	/* if set, rfd and wfd are sockets */
	u16	badhost:1;	/* if set, hostname lookup failed */
	int	rfd;		/* read fd for the remote channel */
	int	wfd;		/* write fd for the remote channel */
	char	*user;		/* remote user if set */
	char	*host;		/* remote host if set */
	char	*path;		/* pathname (must be set) */
	char 	*cred;		/* user:passwd for proxy authentication */
	int	contentlen;	/* len from http header (recieve only) */
	pid_t	pid;		/* if pipe, pid of the child */
} remote;

#define	ADDR_NFS	0x000	/* host:/path */
#define	ADDR_BK		0x001	/* bk://host:[port]//path */
#define	ADDR_HTTP	0x002	/* http://host:[port]//path */
#define	ADDR_FILE	0x003	/* file://path */
#define	ADDR_SSH	0x004	/*
				 * ssh:[user@]host//path or
				 * ssh:[user@]host:/path
				 */
#define	ADDR_RSH	0x005	/*
				 * rsh:[user@]host//path or
				 * rsh:[user@]host:/path
				 */


/*
 * search interface
 */
typedef struct {
	char    *pattern;	/* what we want to find */
	u8      ignorecase:1;	/* duh */
} search;


int	sccs_admin(sccs *sc, delta *d, u32 flgs, char *encoding, char *compress,
	    admin *f, admin *l, admin *u, admin *s, char *mode, char *txt);
int	sccs_cat(sccs *s, u32 flags, char *printOut);
int	sccs_delta(sccs *s, u32 flags, delta *d, MMAP *init, MMAP *diffs,
		   char **syms);
int	sccs_diffs(sccs *s, char *r1, char *r2, u32 flags, char kind, char *opts, FILE *);
int	sccs_encoding(sccs *s, char *enc, char *comp);
int	sccs_get(sccs *s,
	    char *rev, char *mRev, char *i, char *x, u32 flags, char *out);
int	sccs_hashcount(sccs *s);
int	sccs_clean(sccs *s, u32 flags);
int	sccs_unedit(sccs *s, u32 flags);
int	sccs_info(sccs *s, u32 flags);
int	sccs_prs(sccs *s, u32 flags, int reverse, char *dspec, FILE *out);
int	sccs_prsdelta(sccs *s, delta *d, int flags, const char *dspec, FILE *out);
char	*sccs_prsbuf(sccs *s, delta *d, int flags, const char *dspec);
delta	*sccs_getrev(sccs *s, char *rev, char *date, int roundup);
delta	*sccs_findDelta(sccs *s, delta *d);
sccs	*sccs_init(char *filename, u32 flags, project *proj);
sccs	*sccs_restart(sccs *s);
sccs	*sccs_reopen(sccs *s);
int	sccs_open(sccs *s);
void	sccs_fitCounters(char *buf, int a, int d, int s);
void	sccs_free(sccs *);
void	sccs_freetree(delta *);
void	sccs_close(sccs *);
sccs	*sccs_csetInit(u32 flags, project *proj);
char	**sccs_files(char **, int);
int	sccs_smoosh(char *left, char *right);
delta	*sccs_parseArg(delta *d, char what, char *arg, int defaults);
void	sccs_whynot(char *who, sccs *s);
int	sccs_addSym(sccs *, u32 flags, char *);
void	sccs_ids(sccs *s, u32 flags, FILE *out);
void	sccs_inherit(sccs *s, u32 flags, delta *d);
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
int	sccs_keyunlink(char *key, project *proj, MDBM *idDB, MDBM *dirs);
char	*sccs_impliedList(sccs *s, char *who, char *base, char *rev);
int	sccs_sdelta(sccs *s, delta *, char *);
void	sccs_md5delta(sccs *s, delta *d, char *b64);                            
void	sccs_shortKey(sccs *s, delta *, char *);
int	sccs_resum(sccs *s, delta *d, int diags, int dont);
int	sccs_newchksum(sccs *s);
void	sccs_perfile(sccs *, FILE *);
sccs	*sccs_getperfile(MMAP *, int *);
char	*sccs_gethost(void);
char	*sccs_realhost(void);
int	sccs_getComments(char *, char *, delta *);
int	sccs_getHostName(delta *);
int	sccs_getUserName(delta *);
MDBM    *sccs_keys2mdbm(FILE *f);
void	sfileUnget(void);
char	*sfileNext(void);
char	*sfileRev(void);
char	*sfileFirst(char *cmd, char **Av, int Flags);
void	sfileDone(void);
int	tokens(char *s);
delta	*findrev(sccs *, char *);
delta	*sccs_top(sccs *);
delta	*sccs_findKey(sccs *, char *);
MDBM	*sccs_findKeyDB(sccs *, u32);
int	isKey(char *key);
delta	*sccs_findMD5(sccs *s, char *md5);                              
delta	*sccs_dInit(delta *, char, sccs *, int);
char	*sccs_getuser(void);
void	sccs_resetuser(void);
char	*sccs_realuser(void);
int	sccs_markMeta(sccs *);

delta	*modeArg(delta *d, char *arg);
FILE	*fastPopen(const char*, const char*);
int	fastPclose(FILE*);
char    *fullname(char *, int);
int	fileType(mode_t m);
char	chop(char *s);
void	chomp(char *s);
int	atoi_p(char **p);
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
int	diff(char *lfile, char *rfile, char kind, char *opts, char *out);
int	check_gfile(sccs*, int);
void	platformSpecificInit(char *);
MDBM	*loadDB(char *file, int (*want)(char *), int style);
delta 	*mkOneZero(sccs *s);
typedef	void (*handler)(int);
void	sig_catch(handler);
void	sig_restore(void);
int	sig_ignore(void);
void	sig_default(void);
int	csetIds(sccs *cset, char *rev);
int	csetIds_merge(sccs *cset, char *rev, char *merge);
int	cset_inex(int flags, char *op, char *revs);
void	sccs_fixDates(sccs *);
int	sccs_xflags(delta *d);
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
int	sccs_unlock(sccs *, char);

int	sccs_lockfile(const char *lockfile, int wait, int quiet);
int	sccs_stalelock(const char *lockfile, int discard);
int	sccs_unlockfile(const char *file);
int	sccs_mylock(const char *lockf);
int	sccs_readlockf(const char *file, pid_t *pidp, char **hostp, time_t *tp);

sccs	*sccs_unzip(sccs *s);
sccs	*sccs_gzip(sccs *s);
char	*sccs_utctime(delta *d);
void	sccs_renumber(sccs *s, u32 flags);
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
int 	licenseAccept(int prompt);
int	licenseAcceptOne(int prompt, char *lic);
char	*mkline(char *mmap);
int	mkdirp(char *dir);
int	mkdirf(char *file);
char    *mode2FileType(mode_t m);
int	getline(int in, char *buf, int size);
void	explodeKey(char *key, char *parts[6]);
int	smartUnlink(char *name);
int	smartRename(char *old, char *new);
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
int	sccs_mv(char *, char *, int, int, int, int);
delta	*sccs_gca(sccs *, delta *l, delta *r, char **i, char **x, int best);
char	*_relativeName(char *gName, int isDir, int withsccs,
	    int mustHaveRmarker, int wantRealName, project *proj, char *root);
void	rcs(char *cmd, int argc, char **argv);
char	*findBin(void);
project	*chk_proj_init(sccs *s, char *file, int line);
MDBM	*proj_config(project *p);
void	proj_free(project *p);
int 	prompt(char *msg, char *buf);
void	parse_url(char *url, char *host, char *path);
char	*sccs_Xfile(sccs *s, char type);
int	unique(char *key);
int	uniq_lock(void);
int	uniq_unlock(void);
int	uniq_open(void);
time_t	uniq_drift(void);
int	uniq_update(char *key, time_t t);
int	uniq_close(void);
time_t	sccs_date2time(char *date, char *zone);
void	cd2root(void);
pid_t	mail(char *to, char *subject, char *file);
int	connect_srv(char *srv, int port, int trace);
int	get(char *path, int flags, char *output);
int	gethelp(char *helptxt, char *help_name, char *bkarg, char *prefix, FILE *f);
void	status(int verbose, FILE *out);
void	notify(void);
char	*package_name(void);
int	bkusers(int countOnly, int raw, char *prefix, FILE *out);
globv	read_globs(FILE *f, globv oldglobs);
int	match_one(char *string, char *glob, int ignorecase);
char	*match_globs(char *string, globv globs, int ignorecase);
void	free_globs(globv globs);
int	searchMatch(char *s, search search);
search	searchParse(char *str);
char	*prog2path(char *prog);
void	remark(int quiet);
int	readn(int from, char *buf, int size);
void	send_request(int fd, char * request, int len);
int	writen(int to, char *buf, int size);
long	almostUnique(int harder);
int	repository_downgrade(void);
int	repository_locked(project *p);
int	repository_mine(char type);
int	repository_lockers(project *p);
int	repository_rdlock(void);
int	repository_rdunlock(int all);
void	repository_unlock(int all);
int	repository_wrlock(void);
int	repository_wrunlock(int all);
int	repository_hasLocks(char *root, char *dir);
void	repository_lockcleanup(void);
void	comments_save(char *s);
void	comments_savefile(char *s);
int	comments_got(void);
void	comments_done(void);
delta	*comments_get(delta *d);
void	comments_writefile(char *file);
void	host_done(void);
delta	*host_get(delta *, int);
void	user_done(void);
delta	*user_get(delta *, int);
char	*shell(void);
void	names_init(void);
int	names_rename(char *old_spath, char *new_spath, u32 flags);
void	names_cleanup(u32 flags);
int	bk_sfiles(char *opts, int ac, char **av);
int	outc(char c);
MDBM	*loadConfig(char *root);
int	ascii(char *file);
char	*sccs_rmName(sccs *s, int useCommonDir);
int	sccs_rm(char *name, char *del_name, int useCommonDir, int force);
void	sccs_rmEmptyDirs(char *path);
void	do_prsdelta(char *file, char *rev, int flags, char *dspec, FILE *out);
char 	**get_http_proxy(void);
int	confirm(char *msg);
int	csetCreate(sccs *cset, int flags, char *files, char **syms);
int	cset_setup(int flags, int ask);
off_t	fsize(int fd);
char	*separator(char *);
int	trigger(char *cmd, char *when);
void	cmdlog_start(char **av, int want_http_hdr);
void	cmdlog_addnote(char *key, char *val);
int	cmdlog_end(int ret);
int	write_log(char *root, char *file, int rotate, char *format, ...);
off_t	get_byte_count(void);
void	save_byte_count(unsigned int byte_count);
int	cat(char *file);
char	*getHomeDir(void);
char	*getBkDir(void);
char	*age(time_t secs, char *space);
	/* this must be the last argument to all calls to sys/sysio */
#define	SYS	(char*)0, 0xdeadbeef
int	sys(char *first, ...);
int	sysio(char *in, char *out, char *err, char *first, ...);
void	syserr(const char *postfix);
char	*sccs_zone(time_t tt);
MDBM	*sccs_tagConflicts(sccs *s);
void	sccs_tagMerge(sccs *s, delta *d, char *tag);
int	sccs_tagleaves(sccs *, delta **, delta **);
ser_t	*sccs_set(sccs *, delta *, char *iLst, char *xLst);

int     http_connect(remote *r, char *cgi_script);
int     http_send(remote *, char *, size_t, size_t, char *, char *); 
char *	user_preference(char *what);
char	*bktmp(char *buf, const char *template);
char	*bktmp_local(char *buf, const char *template);
void	bktmpcleanup(void);
char	*getRealCwd(char *, size_t);
int	smallTree(int threshold);
MDBM	*csetDiff(MDBM *, int);
char	*aprintf(char *fmt, ...);
char	*vaprintf(const char *fmt, va_list ptr);
void	ttyprintf(char *fmt, ...);
void	enableFastPendingScan(void);
char	*isHostColonPath(char *);
int	hasGUIsupport(void);
char	*GUI_display(void);
char	*savefile(char *dir, char *prefix, char *pathname);
void	has_proj(char *who);
int	mv(char*, char *);
char	*rootkey(char *buf);
char	*globalroot(void);
void	sccs_touch(sccs *s);
int	setlevel(int);
void	sccs_rmUncommitted(int quiet, FILE *chkfiles);
void	rmEmptyDirs(int quiet);    
int	after(int quiet, char *rev);
int	diff_gfile(sccs *s, pfile *pf, int expandKeyWord, char *tmpfile);
char	*getCSetFile(project *p);
int	spawn_cmd(int flag, char **av);
pid_t	mkpager(void);
int	getRealName(char *path, MDBM *db, char *realname);
int	addsym(sccs *s, delta *d, delta *metad, int, char*, char*);
int	delta_table(sccs *s, FILE *out, int willfix);
char	**getdir(char *);
typedef	int	(*walkfn)(char *file, struct stat *statbuf, void *data);
int	walkdir(char *dir, walkfn fn, void *data);
char	*getParent(void);
delta	*getSymlnkCksumDelta(sccs *s, delta *d);
struct tm
        *utc2tm(time_t t);
void	fix_stime(sccs *s);
int	isLocalHost(char *h);
void	do_cmds(void);
void	core(void);
void	ids(void);
void	requestWebLicense(void);
void	http_hdr(int full);
pid_t	bkd_tcp_connect(remote *r);
int	check_rsh(char *remsh);
int	smartMkdir(char *pathname, mode_t mode);
void	sccs_color(sccs *s, delta *d);
int	out(char *buf);
int	getlevel(void);
int	isSymlnk(char *s);
delta	*cset_insert(sccs *s, MMAP *iF, MMAP *dF, char *parentKey);
int	cset_map(sccs *s, int extras);
int	cset_write(sccs *s);
int	cset_diffs(sccs *s, ser_t ser);
int	cweave_init(sccs *s, int extras);
int	isNullFile(char *rev, char *file);
unsigned long	ns_sock_host2ip(char *host, int trace);
unsigned long	host2ip(char *host, int trace);
int	onelink(char *s);
int	isEffectiveDir(char *s);
int	fileTypeOk(mode_t m);
void	sccs_tagLeaf(sccs *s, delta *d, delta *md, char *tag);
int	sccs_scompress(sccs *s, int flags);
int	hasRootFile(char *gRoot, char *sRoot);
int	mkBkRootIcon(char *path);
int	unmkBkRootIcon(char *path);
char	*fast_getcwd(char *buf, int len);
void	sccs_tagcolor(sccs *s, delta *d);
int	checkXflags(sccs *s, delta *d, int what);
void	metaUnionResync1(void);
void	metaUnionResync2(void);
int	sccs_istagkey(char *key);
char	*testdate(time_t t);
void	putroot(char *where);
int	runable(char *file);
int	uuencode(FILE *in, FILE *out);
int	uudecode(FILE *in, FILE *out);
void	sccs_unmkroot(char *path);
int	sccs_needSwap(delta *p, delta *m);
void	sccs_reDup(sccs *s);
void	sccs_adjustSet(sccs *sc, sccs *scb, delta *d);
int	chk_host(void);
int	chk_user(void);
int	fix_gmode(sccs *s, int gflags);
int	do_checkout(sccs *s);
int	unsafe_path(char *s);
char	**getTriggers(char *dir, char *prefix);
void	comments_cleancfile(char *file);
int	comments_readcfile(sccs *s, int prompt, delta *d);
int	comments_prompt(char *file);
void	saveEnviroment(char *patch);
void	restoreEnviroment(char *patch);
int	run_check(char *partial, int fix, int quiet);
char	*key2path(char *key, MDBM *idDB);
int	check_licensesig(char *key, char *sign);
char	*hashstr(char *str);
char	*secure_hashstr(char *str, char *key);
void	delete_cset_cache(char *rootpath, int save);
int	nFiles(void);
void	notice(char *key, char *arg, char *type);
pid_t	findpid(pid_t pid);
void	save_log_markers(void);
void	update_log_markers(int verbose);
int	isCaseFoldingFS(char *root);
void	line2av(char *cmd, char **av);
void	smerge_saveseq(u32 seq);
char	*loadfile(char *file, int *size);
char	*repo_id(void);
void	fromTo(char *op, remote *r, remote *l);
u32	adler32_file(char *filename);
char	*findDotFile(char *old, char *new, char *buf);

void	align_diffs(u8 *vec, int n, int (*compare)(int a, int b),
    int (*is_whitespace)(int i));
void	close_gaps(u8 *vec, int n, int (*compare)(int a, int b));
int	diff_algor(int m, int n, u8 *lchg, u8 *rchg,
    int (*compare)(int a, int b));
int   diffline(char *left, char *right);

extern char *bk_vers;
extern char *bk_utc;
extern char *bk_time;

int	getMsg(char *msg_name, char *bkarg, char *prefix, char b, FILE *outf);
#endif	/* _SCCS_H_ */
