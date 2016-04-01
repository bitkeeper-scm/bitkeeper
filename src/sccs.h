/*
 * Copyright 1997-2016 BitMover, Inc
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

#ifndef	_SCCS_H_
#define	_SCCS_H_

#include "system.h"
#include "diff.h"
#define	PCRE_STATIC		/* for win32 */
#include "pcre.h"

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
char    *tzone(long offset);

#ifdef	WIN32
#define	win32_close(s)	sccs_close(s)
#else
#define	win32_close(s)
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
#define	NEWCKSUM	0x00000008	/* Redo checksum */

#define	INIT_NOWARN	0x10000000	/* don't bitch about failed inits */
#define	INIT_NOCKSUM	0x20000000	/* don't do the checksum */
#define	INIT_FIXDTIME	0x40000000	/* use g file mod time as delat time */
#define	INIT_NOSTAT	0x01000000	/* do not look for {p,x,z,c} files */
#define	INIT_HAScFILE	0x02000000	/* has c.file */
#define	INIT_HASgFILE	0x04000000	/* has g.file */
#define	INIT_HASpFILE	0x08000000	/* has physical p.file */
#define	INIT_HASxFILE	0x00100000	/* has x.file */
#define	INIT_NOGCHK	0x00800000	/* do not fail on gfile checks */
#define	INIT_CHK_STIME	0x00010000	/* check that s.file <= gfile */
#define	INIT_WACKGRAPH	0x00020000	/* we're wacking the graph, no errors */
#define	INIT_MUSTEXIST	0x00040000	/* the sfile must exist or we fail */
#define	INIT_CHKXOR	0x00080000	/* verify crc and xor of sfile */

/* shared across get/diffs/getdiffs */
#define	GET_EDIT	0x10000000	/* get -e: get for editting */
#define	GET_EXPAND	0x20000000	/* expand keywords */
#define	GET_SKIPGET	0x40000000	/* get -g: don't get the file */
#define	GET_NOREMOTE	0x80000000	/* do not go remote for BAM */
#define	GET_ASCII	0x01000000	/* Do not gunzip/uudecode */
#define	GET_SHUTUP	0x02000000	/* quiet on certain errors */
#define	GET_FORCE	0x04000000	/* do it even with errors */
#define	GET_DTIME	0x08000000	/* gfile get delta's mode time */
#define	GET_NOHASH	0x00100000	/* force regular file, ignore S_HASH */
#define	GET_HASHONLY	0x00200000	/* skip the file */
#define	GET_DIFFS	0x00400000	/* get -D, regular diffs */
#define	GET_BKDIFFS	0x00800000	/* get -DD, BK (rick's) diffs */
#define	GET_PERMS	0x00010000	/* extract perms for non gfile */
#define	GET_SUM		0x00020000	/* used to force dsum in getRegBody */
#define	GET_NOREGET	0x00040000	/* get -S: skip gfiles that exist */

#define	GET_PREFIX	\
    (GET_REVNUMS|GET_MD5KEY|GET_USER|GET_LINENUM|GET_MODNAME|\
     GET_RELPATH|GET_PREFIXDATE|GET_SEQ|GET_LINENAME|GET_SERIAL)
#define	GET_ALIGN	0x00002000	/* nicely align prefix output */
#define	GET_REVNUMS	0x00004000	/* get -m: prefix each line with rev */
#define	GET_USER	0x00008000	/* get -u: prefix with user name */
#define	GET_LINENUM	0x00000100	/* get -N: show line numbers */
#define	GET_MODNAME	0x00000200	/* get -n: prefix with %M */
#define	GET_PREFIXDATE	0x00000400	/* get -d: show date */
#define	GET_SERIAL	0x00000800	/* get -aS: prefix with serial */
#define	GET_MD5KEY	0x00000010	/* get -5: prefix line with md5key */
#define	GET_LINENAME	0x00000020	/* get -O: prefix with line name */
#define	GET_SEQ		0x00000040	/* get -0: prefix with sequence no */
#define	GET_RELPATH	0x00000080	/* like GET_MODNAME but full relative */

#define CLEAN_SHUTUP	0x20000000	/* clean -Q: quiet mode */
#define	CLEAN_SKIPPATH	0x40000000	/* ignore path change; for log tree */
#define	CLEAN_CHECKONLY	0x80000000	/* don't delete gfile, just check */

#define	DELTA_AUTO	0x10000000	/* delta -a: auto check-in mode */
#define	DELTA_SAVEGFILE	0x20000000	/* delta -n: save edited gfile */
#define	DELTA_DONTASK	0x40000000	/* don't ask for comments */
#define	DELTA_PATCH	0x80000000	/* delta -R: respect rev */
#define	DELTA_EMPTY	0x01000000	/* initialize with empty file */
#define	DELTA_FORCE	0x02000000	/* delta -f: force a delta */
#define DELTA_CSETMARK	0x04000000	/* delta --csetmark */
#define	DELTA_NOPENDING	0x08000000	/* don't create pending marker */
#define	DELTA_CFILE	0x00100000	/* read cfile and do not prompt */
#define	DELTA_MONOTONIC	0x00200000	/* preserve MONOTONIC flag */
#define	DELTA_TAKEPATCH	0x00400000	/* call sccs_getInit() from takepatch */
#define	DELTA_DB	0x00800000	/* treat as DB file */
#define	DELTA_NEWFILE	0x00010000	/* delta -i: create initial file */

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
#define	ADMIN_NEWTAG	0x00020000	/* restricted tag rules */

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
 * sccs_cat output format; not a bitmap
 */
#define	SCAT_SCCS	0x10000000	/* -i list in the merge nodes */
#define	SCAT_BK		0x20000001	/* use merge edge like parent edge */
#define	SCAT_ACTUAL	0x40000002	/* just dump the file as it is */

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
#define	S_WARNED	0x00000020	/* error message already sent */
#define	S_BADREVS	0x00000080	/* has corrupted revisions */
#define	S_available	0x00000100	/* OLD: make the landing pad big */
#define	S_CSET		0x00000200	/* this is a changeset file */
#define S_MAPPRIVATE	0x00000400	/* hack for hpux */
#define S_READ_ONLY	0x00000800	/* force read only mode */
#define	S_LOCKFILE	0x00001000	/* unlock on free */
#define	S_SET		0x00002000	/* the tree is marked with a set */
#define S_CACHEROOT	0x00004000	/* don't free the root entry */

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
#define	X_DB		0x00002000	/* key value file */
#define	X_NOMERGE	0x00004000	/* treat as binary even if ascii */
					/* flags which can be changed */
#define	X_MONOTONIC	0x00008000	/* file rolls forward only */
#define	X_EOLN_WINDOWS	0x00010000	/* produce \r\n line endings */
/* Note: X_EOLN_UNIX is fake, it does not go on disk */
#define	X_EOLN_UNIX	0x00020000	/* produce \n style endings */
#define	X_MAYCHANGE	(X_RCS | X_YEAR4 | X_SHELL | X_EXPAND1 | \
			X_SCCS | X_EOLN_NATIVE | X_NOMERGE | \
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
 * Bit 4 and 5 are weave format
 * Bit 6 is serialmap using merge edges
 */
#define	E_ALWAYS	0x1000		/* set so encoding is non-zero */

#define E_DATAENC	0x3
#define	E_ASCII		0x00		/* no encoding */
#define	E_UUENCODE	0x01		/* uuenecode it (traditional) */
#define	E_BAM		0x02		/* store data in BAM pool */
#define	ASCII(s)	(((s)->encoding_in & E_DATAENC) == E_ASCII)
#define	BINARY(s)	(((s)->encoding_in & E_DATAENC) != E_ASCII)
#define	BAM(s)		(((s)->encoding_in & E_DATAENC) == E_BAM)
#define	UUENCODE(s)	(((s)->encoding_in & E_DATAENC) == E_UUENCODE)

#define E_COMP		0x4
#define	E_GZIP		0x04		/* gzip the data */
#define	GZIP(s)		(((s)->encoding_in & E_COMP) == E_GZIP)
#define	GZIP_OUT(s)	(((s)->encoding_out & E_COMP) == E_GZIP)

#define	E_BK		0x08		/* new binary sfile format */
#define	BKFILE(s)	(((s)->encoding_in & E_BK) != 0)
#define	BKFILE_OUT(s)	(((s)->encoding_out & E_BK) != 0)

#define	E_WEAVE		0x030
#define	E_AWEAVE	0x000		/* ascii weave encoding */
#define	E_BWEAVE2	0x010		/* old binary weave encoding */
#define	E_BWEAVE3	0x020		/* binary weave encoding */
#define	BWEAVE(s)	(((s)->encoding_in & E_WEAVE) != E_AWEAVE)
#define	BWEAVE2(s)	(((s)->encoding_in & E_WEAVE) == E_BWEAVE2)
#define	BWEAVE3(s)	(((s)->encoding_in & E_WEAVE) == E_BWEAVE3)
#define	BWEAVE2_OUT(s)	(((s)->encoding_out & E_WEAVE) == E_BWEAVE2)
#define	BWEAVE_OUT(s)	(((s)->encoding_out & E_WEAVE) != E_AWEAVE)

#define	E_BKMERGE	0x40		/* interpret merge as a graph edge */
#define	BKMERGE(s)	(((s)->encoding_in & E_BKMERGE) != 0)
#define	BKMERGE_OUT(s)	(((s)->encoding_out & E_BKMERGE) != 0)

// mask of bits used for sfile format that map to feature bits
#define	E_FILEFORMAT	(E_BK|E_WEAVE|E_BKMERGE)

#define	HAS_GFILE(s)	((s)->state & S_GFILE)
#define	HAS_PFILE(s)	((s)->state & S_PFILE)
#define	HAS_SFILE(s)	((s)->state & S_SFILE)
#define	BEEN_WARNED(s)	((s)->state & S_WARNED)
#define	WRITABLE(s)	((s)->mode & 0200)
#define EDITED(s)	((((s)->state&S_EDITED) == S_EDITED) && WRITABLE(s))
#define LOCKED(s)	(((s)->state&S_LOCKED) == S_LOCKED)
#define	CSET(s)		((s)->state & S_CSET)
#define	CONFIG(s)	((s)->state & S_CONFIG)
#define	READ_ONLY(s)	((s)->state & S_READ_ONLY)
#define	SET(s)		((s)->state & S_SET)
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
#define	HAS_KEYWORDS(s)	((s)->xflags & (X_RCS|X_SCCS))
#define	EOLN_NATIVE(s)	((s)->xflags & X_EOLN_NATIVE)
#define	DB(s)		((s)->xflags & X_DB)
#define	NOMERGE(s)	((s)->xflags & X_NOMERGE)
#define	MONOTONIC(s)	((s)->xflags & X_MONOTONIC)
#define	EOLN_WINDOWS(s)	((s)->xflags & X_EOLN_WINDOWS)

/*
 * Flags (FLAGS(s, d)) that indicate some state on the delta.
 * When changing also update delta_flagNames in heapdump.c.
 */
/* flags that are written to disk (don't renumber) */
#define	D_INARRAY	0x00000001	/* part of s->slist array */
//#define	D_NONEWLINE	0x00000002	/* no trailing newline */
//#define	D_CKSUM		0x00000004	/* delta has checksum */
//#define	D_SORTSUM	0x00000008	/* generate a sortSum */

// in old bk this is type=='M' plus metadata found on cset comments
// so this is a bk-only delta. So always set for bk tags
// if D_META, then D_TAGS
#define	D_META		0x00000010

// marks that this delta or tag has a symbol in s->symlist.
// The target of both sym->ser and sym->meta_ser get marked with D_SYMBOLS
// tag merges don't always set D_SYMBOLS
#define	D_SYMBOLS	0x00000020

#define	D_DANGLING	0x00000080	/* in MONOTONIC file, ahead of chgset */

// Set on tags that are on a separate graph from the csets (also D_META).
// Also ATT SCCS removed deltas, if no D_META
#define	D_TAG		0x00000100	/* is tag node old d->type=='R' */

// set on all tags created by bk after 2000. These form a separate graph
// using PTAG/MTAG. Also non-tags can have SYMGRAPH|SYMBOLS set if they
// will act as both a commit and a tag.  (bk commit --tag=FOO)
#define	D_SYMGRAPH	0x00000200	/* if set, I'm a symbol in the graph */

// The newest tip of the tag graph will be marked with SYMLEAF
// An older tag graph tip might also have SYMLEAF set after a tag.
// Note: the tag graph many have many open tips after an undo but only
// the newest tip is marked with SYMLEAF
#define	D_SYMLEAF	0x00000400

//#define	D_MODE		0x00000800	/* permissions in MODE(s, d) are valid */
#define	D_CSET		0x00001000	/* this delta is marked in cset file */
	/* unused	0x00002000	*/
	/* unused	0x00004000	*/
#define	D_FIXUPS	0x00008000	/* fixups to tip delta at end of file */

/* flags that are in memory only and not written to disk */
#define	D_GREEN		0x00200000	/* Another coloring flag */
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
 * Exit codes for 'bk fstype' as well as internal uses.
 * List organized to be extensible.
 * Roughly patterned after 'bk repotype'.
 */
#define	FS_UNKNOWN	0
#define	FS_ERROR	1
#define	FS_DISK		2
#define	FS_SSD		3
#define	FS_NFS		4

/*
 * Hash behaviour.  Bitmask, although some of these are
 * mutually exclusive.
 */
#define	DB_NODUPS       0x0001		/* keys must be unique */
#define	DB_KEYSONLY	0x0008		/* boolean hashes */
#define	DB_KEYFORMAT	0x0020		/* key/value are u@h|path|date|cksum */
#define	DB_DB		0x0080		/* db file format */
#define	DB_FIRST	0x0100		/* parsing first line in file */

/* shortcuts for common formats */
#define	DB_IDCACHE	(0x80|DB_KEYFORMAT|DB_NODUPS)
#define	DB_GONE		(DB_KEYSONLY)

#define	MAXREV	24	/* 99999.99999.99999.99999 */
#define	MD5LEN	32	/* storage for a hex-encoded 16-bit md5 checksum */
#define	MD5KEYLEN 30	/* length of :MD5KEY: */

#define	BK_WEBMAIL_URL	getenv("BK_WEBMAIL_URL")
#define	BK_HOSTME_SERVER "hostme.bkbits.net"
#define	BKDIR		"BitKeeper"
#define	BKTMP		"BitKeeper/tmp"
#define	BKROOT		"BitKeeper/etc"
#define	GONE		goneFile()
#define	SGONE		sgoneFile()
#define	COLLAPSED	"BitKeeper/etc/collapsed"
#define	CSETS_IN	"BitKeeper/etc/csets-in"
#define	CSETS_OUT	"BitKeeper/etc/csets-out"
#define	IGNOREPOLY	"BitKeeper/etc/ignore-poly"
#define	CHANGESET	"SCCS/s.ChangeSet"
#define	CHANGESET_H1	"SCCS/1.ChangeSet"
#define	CHANGESET_H2	"SCCS/2.ChangeSet"
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
#define	MAC_PHDIR	"/etc/paths.d"
#define	MAC_PHFILE	"/etc/paths.d/10-BitKeeper"

/*
 * Constants for running some parallel processes, sfio, checkout,
 * to overcome NFS latency.
 */
#define	PARALLEL_MAX		64
#define	READER			1	/* parallel processes mostly reading */
#define	WRITER			2	/* ... writing */

#define	MINUTE	(60)
#define	HOUR	(60*MINUTE)
#define	DAY	(24*HOUR)
#define	WEEK	(7*DAY)
#define	YEAR	(365*DAY)	/* no leap */
#define	MONTH	(YEAR/12)	/* average */
#define	DECADE	(10*YEAR)

#define	UNKNOWN_USER	"anon"
#define	UNKNOWN_HOST	"nowhere"

/*
 * initial values for setup.c to stuff in the nfiles caches
 */
#define	NFILES_SA	5
#define NFILES_COMP	6
#define NFILES_PROD	7

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

typedef struct {
	ser_t	parents[2];		/* serial number of parent,merge */
	u32	date;			/* date - conversion from sdate/zone */
	u32	flags;			/* per delta flags */
} d1_t;			   /* delta walk type */

typedef struct delta {
	/* linkage */
	ser_t	ptags[2];		/* parent,merge in tag graph */

	/* data */
	u32	added;			/* lines added by this delta (u32!) */
	u32	deleted;		/* and deleted */
	u32	same;			/* and unchanged */
	u32	sum;			/* checksum of gfile */
	u32	sortSum;		/* sum from sortkey */
	u32	dateFudge;		/* make dates go forward */
	u32	mode;			/* 0777 style modes */
	u32	xflags;			/* timesafe x flags */
	ser_t	r[4];			/* 1.2.3 -> 1, 2, 3, 0 */

	/* unique heap data */
	u32	cludes;			/* include/exclude list */
	u32	comments;		/* delta comments (\n sep string) */
	u32	bamhash;		/* hash of gfile for BAM.
					 * Also reused for weave in
					 * ChangeSet file. See
					 * WEAVE_INDEX.
					 */
	u32	random;			/* random bits for file ID */

	/* collapsible heap data */
	u32	userhost;		/* user/realuser@host/realhost */
	u32	pathname;		/* pathname to the file */
	u32	sortPath;		/* original pathname for delta */
	u32	zone;			/* 08:00 is time relative to GMT */
	u32 	symlink;		/* sym link target */
 	u32	csetFile;		/* id for ChangeSet file */
} d2_t;

typedef struct rset_df {
	u32	rkoff;
	u32	dkleft1;
	u32	dkleft2;
	u32	dkright;
} rset_df;

typedef struct {
	ser_t	ser;
	u32	rkoff;
	u32	dkoff;
} weave;

#define	XFLAGS(s, d)		((s)->slist2[d].xflags)
#define	FLAGS(s, d)		((s)->slist1[d].flags)

/* Parent must exist for merge, so can terminate on first empty one */
#define	EACH_PARENT(s, d, p, j)	\
	for (j = 0; (p) = (j < 2) ? PARENTS(s, d, j) : 0; ++j)

#define	PARENTS(s, d, j)	(0 + (s)->slist1[d].parents[j])
#define	PARENT(s, d)		PARENTS(s, d, 0)
#define	MERGE(s, d)		PARENTS(s, d, 1)

#define	EACH_PTAG(s, d, p, j)	\
	for (j = 0; (p) = (j < 2) ? PTAGS(s, d, j) : 0; ++j)

#define	PTAGS(s, d, j)		(0 + (s)->slist2[d].ptags[j])
#define	PTAG(s, d)		PTAGS(s, d, 0)
#define	MTAG(s, d)		PTAGS(s, d, 1)

#define	ADDED(s, d)		(0 + (s)->slist2[d].added)
#define	DELETED(s, d)		(0 + (s)->slist2[d].deleted)
#define	SAME(s, d)		(0 + (s)->slist2[d].same)
#define	SUM(s, d)		(0 + (s)->slist2[d].sum)
#define	SORTSUM(s, d)		(0 + (s)->slist2[d].sortSum)
#define	DATE(s, d)		(time_t)(0 + (s)->slist1[d].date)
#define	DATE_FUDGE(s, d)	(time_t)(0 + (s)->slist2[d].dateFudge)
#define	MODE(s, d)		(mode_t)(0 + (s)->slist2[d].mode)
#define	R0(s, d)		(0 + (s)->slist2[d].r[0])
#define	R1(s, d)		(0 + (s)->slist2[d].r[1])
#define	R2(s, d)		(0 + (s)->slist2[d].r[2])
#define	R3(s, d)		(0 + (s)->slist2[d].r[3])

#define	PARENTS_SET(s, d, j, v)	((s)->slist1[d].parents[j] = (v))
#define	PARENT_SET(s, d, v)	PARENTS_SET(s, d, 0, v)
#define	MERGE_SET(s, d, v)	PARENTS_SET(s, d, 1, v)
#define	PTAGS_SET(s, d, j, v)	((s)->slist2[d].ptags[j] = (v))
#define	PTAG_SET(s, d, v)	PTAGS_SET(s, d, 0, v);
#define	MTAG_SET(s, d, v)	PTAGS_SET(s, d, 1, v);
#define	ADDED_SET(s, d, v)	((s)->slist2[d].added = (v))
#define	DELETED_SET(s, d, v)	((s)->slist2[d].deleted = (v))
#define	SAME_SET(s, d, v)	((s)->slist2[d].same = (v))
#define	SUM_SET(s, d, v)	((s)->slist2[d].sum = (v))
#define	SORTSUM_SET(s, d, v)	((s)->slist2[d].sortSum = (v))
#define	DATE_SET(s, d, v)	((s)->slist1[d].date = (v))
#define	DATE_FUDGE_SET(s, d, v)	((s)->slist2[d].dateFudge = (v))
#define	MODE_SET(s, d, v)	((s)->slist2[d].mode = (v))
#define	R0_SET(s, d, v)		((s)->slist2[d].r[0] = (v))
#define	R1_SET(s, d, v)		((s)->slist2[d].r[1] = (v))
#define	R2_SET(s, d, v)		((s)->slist2[d].r[2] = (v))
#define	R3_SET(s, d, v)		((s)->slist2[d].r[3] = (v))

#define	HAS_CLUDES(s, d)	((s)->slist2[d].cludes != 0)
#define	HAS_COMMENTS(s, d)	((s)->slist2[d].comments != 0)
#define	HAS_BAMHASH(s, d)	(!CSET(s) && ((s)->slist2[d].bamhash != 0))
#define	HAS_WEAVE(s, d)		(CSET(s) && ((s)->slist2[d].bamhash != 0))
#define	HAS_RANDOM(s, d)	((s)->slist2[d].random != 0)
#define	HAS_USERHOST(s, d)	((s)->slist2[d].userhost != 0)
#define	HAS_PATHNAME(s, d)	((s)->slist2[d].pathname != 0)
#define	HAS_SORTPATH(s, d)	((s)->slist2[d].sortPath != 0)
#define	HAS_ZONE(s, d)		((s)->slist2[d].zone != 0)
#define	HAS_SYMLINK(s, d)	((s)->slist2[d].symlink != 0)
#define	HAS_CSETFILE(s, d)	((s)->slist2[d].csetFile != 0)

#define	CLUDES_INDEX(s, d)	((s)->slist2[d].cludes)
#define	COMMENTS_INDEX(s, d)	((s)->slist2[d].comments)
#define	BAMHASH_INDEX(s, d)	((s)->slist2[d].bamhash)
#define	WEAVE_INDEX(s, d)	BAMHASH_INDEX(s, d)
#define	RANDOM_INDEX(s, d)	((s)->slist2[d].random)
#define	USERHOST_INDEX(s, d)	((s)->slist2[d].userhost)
#define	PATHNAME_INDEX(s, d)	((s)->slist2[d].pathname)
#define	SORTPATH_INDEX(s, d)	((s)->slist2[d].sortPath)
#define	ZONE_INDEX(s, d)	((s)->slist2[d].zone)
#define	SYMLINK_INDEX(s, d)	((s)->slist2[d].symlink)
#define	CSETFILE_INDEX(s, d)	((s)->slist2[d].csetFile)

#define	CLUDES_SET(s, d, val)	(CLUDES_INDEX(s, d) = sccs_addStr((s), val))
#define	COMMENTS_SET(s, d, val)	(COMMENTS_INDEX(s, d) = sccs_addStr((s), val))
#define	BAMHASH_SET(s, d, val)	(BAMHASH_INDEX(s, d) = sccs_addStr((s), val))
#define	WEAVE_SET(s, d, val)	(BAMHASH_INDEX(s, d) = val)
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
#define	SYMBOLS(s, d)	(FLAGS(s, d) & D_SYMBOLS)
#define	INARRAY(s, d)	(FLAGS(s, d) & D_INARRAY)

#define	KID(s, d)	(s)->kidlist[d].kid
#define	SIBLINGS(s, d)	(s)->kidlist[d].siblings

#define	REV(s, d)	delta_rev(s, d)
#define	CLUDES(s, d)	((s)->heap.buf + CLUDES_INDEX(s, d))
#define	BAMHASH(s, d)	((s)->heap.buf + BAMHASH_INDEX(s, d))
#define	WEAVE(s, d)	BAMHASH(s, d)
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
 * Rap on symbols wrt deltas.
 * Both symbols can occur exactly once per delta where delta is a node in
 * the graph.  When a delta is created, it might get a symbol.
 * Later, it might get another one.  These later ones are implemented as
 * metadata deltas (aka removed deltas) which point up to the real delta.
 * So it is an invariant: one node in the graph, one symbol.
 *
 * The sym field in the delta are for initialization and are extracted
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
#include "xfile.h"

extern	jmp_buf	exit_buf;
extern	char *upgrade_msg;

void	bk_exit(const char *file, int line, int ret) __attribute__((noreturn));
#define	exit(e)	bk_exit(__FILE__, __LINE__, e)

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
	FILE	*dF;		/* fmem with diffs */
	ser_t	serial;
} loc;

typedef	struct {
	ser_t	kid;
	ser_t	siblings;
} KIDS;

typedef struct nokey nokey;

/*
 * struct sccs - the delta tree, the data, and associated junk.
 */
struct sccs {
	ser_t	tip;		/* the delta table list, 1.99 .. 1.0 */
	d1_t	*slist1;	/* array of delta1 structs */
	d2_t	*slist2;	/* array of delta2 structs */
	dextra	*extra;		/* array of extra delta info */
	symbol	*symlist;	/* array of symbols, oldest first */
	KIDS	*kidlist;	/* optional kid/sibling data */
	hash	*symhash;	/* list of active tags pointing at delta */
	char	*defbranch;	/* defbranch, if set */

	/* struct heap / BKFILE (in essence) */
	off_t	size;		/* size of sfile at sccs_init time */
	DATA	heap;		/* all strings in delta structs */
	u32	heap_loadsz;	/* size of heap at load time */
	u32	heapsz1;	/* size of SCCS/1.ChangeSet */
	hash	*heapmeta;	/* metadata from start of heap */
	struct	{
	   u32	comp;		/* component rootkey end of table */
	   u32	bam;		/* bam file rootkey end of table */
	} rktypeoff;		/* offset to rkeys in heap */
	nokey	*uniq1;		/* uniq hash in heap1 */
	u32	uniq1off;	/* uniq1 hash covers objects <off in heap */
	nokey	*uniq2;		/* remember uniq objects not in uniq1 */
	u32	rkeyHead;	/* head of linked list of rootkeys in heap */
	u32	*mg_symname;	/* symbol list use by mkgraph() */
	FILE	*pagefh;	/* fh for paging from sfile */
	FILE	*heapfh[3];	/* fh for paging dataheap[12] (0 unused) */
	/* Plus some bit fields at the end of this struct - search /heap */

	FILE	*fh;		/* current input file handle (may be stacked) */
	FILE	*outfh;		/* fh for writing x.file (may be stacked) */
	char	*sfile;		/* SCCS/s.foo.c */
	char	*fullsfile;	/* full pathname to sfile */
	char	*gfile;		/* foo.c */
	char	*symlink;	/* if gfile is a sym link, the destination */
	char	**usersgroups;	/* lm, beth, staff, etc */
	int	encoding_in;	/* ascii, uuencode, gzip, etc. */
	int	encoding_out;	/* encoding to write sfile */
	char	**flags;	/* flags in the middle that we didn't grok */
	char	**text;		/* descriptive text */
	u32	state;		/* GFILE/SFILE etc */
	u32	xflags;		/* cache of sccs_top()->xflags */
	u32	lines;		/* how many lines in the latest get_reg() */
	mode_t	mode;		/* mode of the gfile */
	off_t	data;		/* offset to data in file */
	ser_t	rstart;	/* start of a range (1.1 - oldest) */
	ser_t	rstart2;	/* handle merge range: rstart,rstart2..rstop */
	ser_t	rstop;		/* end of range (1.5 - youngest) */
	ser_t	remote;	/* sccs_resolveFiles() sets this */
	ser_t	local;		/* sccs_resolveFiles() sets this */
	ser_t	*remap;		/* scompress remap old ser to new ser */
	ser_t	w_d;		/* current d for cset_rdweavePair() */
	ser_t	w_cur;		/* locator in non BWEAVE cset file */
	u32	w_off;		/* next weave line for sccs_nextdata() */
	char	*w_buf;		/* buf for weave line in sccs_nextdata() */
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
	ser_t	mdbm_ser;	/* Which rev of mdbm was saved */
	MDBM	*goneDB;	/* GoneDB used in the get_reg() setup */
	MDBM	*idDB;		/* id cache used in the get_reg() setup */
	char	**fastsum;	/* pointers to data about weave block sums */
	hash	*fastsumhash;	/* the storage of the data being pointed to */
	project	*proj;		/* If in BK mode, pointer to project */
	ser_t	whodel;		/* reference rev when doing who deleted */
	void	*rrevs;		/* If has conflicts, revs in conflict */
				/* Actually is of type "name *" in resolve.h */
	u16	version;	/* file format version */
	u16	userLen;	/* maximum length of any user name */
	u16	revLen;		/* maximum length of any rev name */
	loc	*locs;		/* for cset data */ 
	u32	iloc;		/* index to element in *loc */ 
	u32	nloc;		/* # of element in *loc */ 
	u32	initFlags;	/* how we were opened */
	char	*comppath;	/* used by changes for historic paths for comps*/
	hash	*saveStr;	/* saved strings from sccs_saveStr() */
	u32	prs_nrevs;	/* stop after printing the first n revs */
	u32	cksumok:1;	/* check sum was ok */
	u32	cksumdone:1;	/* check sum was checked */
	u32	grafted:1;	/* file has grafts */
	u32	bad_dsum:1;	/* patch checksum mismatch */
	u32	io_error:1;	/* had an output error, abort */
	u32	io_warned:1;	/* we told them about the error */
	u32	bitkeeper:1;	/* bitkeeper file */
	u32	prs_output:1;	/* prs printed something */
	u32	prs_odd:1;	/* for :ODD: :EVEN: in dspecs */
	u32	prs_join:1;	/* for joining together items in dspecs */
	u32	prs_all:1;	/* including tag deltas in prs output */
	u32	prs_indentC:1;	/* extra space for components in :INDENT: ? */
	u32	hasgone:1;	/* this graph has D_GONE deltas */
	u32	has_nonl:1;	/* set by getRegBody() if a no-NL is seen */
	u32	cachemiss:1;	/* BAM file not found locally */
	u32	bamlink:1;	/* BAM gfile is hardlinked to the sfile */
	u32	used_cfile:1;	/* comments_readcfile found one; for cleanup */
	u32	modified:1;	/* set if we wrote the s.file */
	u32	mem_out:1;	/* s->outfh is in-memory FILE* */
	u32	file:1;		/* treat as a file in DSPECS */
	u32	rdweaveEOF:1;	/* at EOF in rdWEAVE */
	u32	rdweave:1;	/* currently reading weave */
	u32	wrweave:1;	/* currently writing weave */
	u32	ckwrap:1;	/* running with fopen_cksum */
	u32	w_reverse:1;	/* csetPair walk in reverse order */
	/* heap releated bit fields */
	u32	uniq1init:1;	/* we have looked for uniq1 in heap? */
	u32	uniq2keys:1;	/* rkeys loaded in uniq2 */
	u32	uniq2deltas:1;	/* deltas loaded in uniq2 */
}; /* struct sccs */

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
	u32	magic:1;	/* no pfile, so data is faked */
	u32	formatErr:1;	/* Is the pfile formatted wrong? */
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
	char	*initFile;	/* RESYNC/BitKeeper/init-1, only if !initMem */
	DATA	initMem;	/* block of init data */
	char	*diffFile;	/* RESYNC/BitKeeper/diff-1, only if !diffMem */
	FILE	*diffMem;	/* points into mmapped patch */
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
#define	CLOCK_DRIFT	(2*DAY)

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
	int	contentlen;	/* len from http header (recieve only) */
	pid_t	pid;		/* if pipe, pid of the child */
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

typedef struct {
	int	flags;			/* flags (transitional) */
	int	adds, dels, mods;	/* lines added/deleted/modified */
	u32	bin_files:1;		/* Binary files differ */
	char	*out_define;		/* diff -D */
	pcre	*pattern;		/* compiled pattern for diff -p */
	int	context;		/* context for unified output
					 * (-1 means 0) see delta comments */
	u32	out_sdiff;		/* sdiff output (value is # of cols) */
	u32	ignore_all_ws:1;	/* ignore all whitespace */
	u32	ignore_ws_chg:1;	/* ignore changes in white space */
	u32	minimal:1;		/* find minimal diffs */
	u32	strip_trailing_cr:1;	/* remove trailing \r and \n */
	u32	ignore_trailing_cr:1;	/* ignore trailing \r and \n (bk) */
	u32	new_is_null:1;		/* treat non-existent files as new */
	u32	out_unified:1;		/* print unified diffs */
	u32	out_show_c_func:1;	/* print C function (diff -p) */
	u32	out_rcs:1;		/* output RCS diffs */
	u32	out_print_hunks:1;	/* just print the hunks */
	u32	out_header:1;		/* print bitkeeper header (doDiff())*/
	u32	out_comments:1;		/* print comments */
	u32	out_diffstat:1;		/* print diffstat output */
} df_opt;

#if	defined(__x86_64__) || defined(__i386__)
#define	HEAP_U32LOAD(ptr)	(*(u32 *)(ptr))
#else
#define	HEAP_U32LOAD(ptr)	_heap_u32load(ptr)
#endif
u32	_heap_u32load(void *ptr);

#define	HEAP(s, off)	((s)->heap.buf + (off))
#define	KOFF(s, off)	(HEAP_U32LOAD(HEAP(s, off)))

/* while (off = RKDKOFF(s, off, rkoff, dkoff)) {} */
#define	RKDKOFF(s, off, rkoff, dkoff)					\
	({	u32 _ret;						\
		unless ((rkoff) = KOFF(s, (off))) {			\
			_ret = 0;					\
		} else if (BWEAVE2(s)) {				\
			(rkoff) += 4;					\
			(dkoff) = (off) + 4;				\
			_ret = (off) + 4 + strlen(HEAP(s, dkoff)) + 1;	\
		} else {						\
			(dkoff) = KOFF(s, (off)+4);			\
			_ret = (off) + 8;				\
		}							\
		_ret;							\
	})

int	sccs_admin(sccs *sc, ser_t d, u32 flgs,
	    admin *f, admin *l, admin *u, admin *s, char *mode, char *txt);
int	sccs_adminFlag(sccs *sc, u32 flags);
int	sccs_cat(sccs *s, u32 flags, FILE *out);
char	*sccs_scat(sccs *s, int format, size_t *len);
int	sccs_delta(sccs *s, u32 flags, ser_t d, FILE *init, FILE *diffs,
		   char **syms);
int	sccs_diffs(sccs *s, char *r1, char *r2, df_opt *dop, FILE *)
	__attribute__((nonnull (4)))
;
int	sccs_encoding(sccs *s, off_t size, char *enc);
int	sccs_get(sccs *s, char *rev, char *mRev, char *i, char *x,
    u32 flags, char *outfile, FILE *out);
int	sccs_hashcount(sccs *s);
int	sccs_clean(sccs *s, u32 flags);
int	sccs_unedit(sccs *s, u32 flags);
int	sccs_info(sccs *s, u32 flags);
int	sccs_prs(sccs *s, u32 flags, int reverse, char *dspec, FILE *out);
int	sccs_prsdelta(sccs *s, ser_t d, int flags, char *dspec, FILE *out);
char	*sccs_prsbuf(sccs *s, ser_t d, int flags, char *dspec);
ser_t	sccs_findDate(sccs *s, char *date, int roundup);
ser_t	sccs_date2delta(sccs *s, time_t date);
int	sccs_patheq(char *file1, char *file2);
ser_t	sccs_findDelta(sccs *s, ser_t d);
sccs	*sccs_init(char *filename, u32 flags);
sccs	*sccs_restart(sccs *s);
sccs	*sccs_reopen(sccs *s);
int	sccs_open(sccs *s);
int	sccs_free(sccs *);
ser_t	sccs_newdelta(sccs *s);
void	sccs_freedelta(sccs *s, ser_t d);
ser_t	sccs_insertdelta(sccs *s, ser_t d, ser_t serial);
void	sccs_close(sccs *);
void	sccs_writeHere(sccs *s, char *new);
sccs	*sccs_csetInit(u32 flags);
char	**sccs_files(char **, int);
ser_t	sccs_parseArg(sccs *s, ser_t d, char what, char *arg, int defaults);
void	sccs_whynot(char *who, sccs *s);
void	sccs_ids(sccs *s, u32 flags, FILE *out);
void	sccs_inherit(sccs *s, ser_t d);
int	sccs_hasDiffs(sccs *s, u32 flags, int inex);
ser_t	sccs_getInit(sccs *s, ser_t d, FILE *f, u32 flags,
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
void	sccs_key2md5(char *deltakey, char *b64);
void	sccs_setPath(sccs *s, ser_t d, char *newpath);
int	sccs_syncRoot(sccs *s, char *key);
ser_t	sccs_csetBoundary(sccs *s, ser_t, u32 flags);
int	poly_pull(int got_patch, char *mergefile);
void	poly_range(sccs *s, ser_t d, char *pkey);
char	**poly_save(char **list, sccs *cset, ser_t d, char *ckey, int side);
int	poly_r2c(sccs *cset, ser_t d, char ***pcsets);
int	sccs_shortKey(sccs *s, ser_t, char *);
int	sccs_resum(sccs *s, ser_t d, int diags, int dont);
int	cset_resum(sccs *s, int diags, int fix, int spinners, int takepatch);
weave	*cset_mkList(sccs *cset);
int	sccs_newchksum(sccs *s);
ser_t	sccs_getCksumDelta(sccs *s, ser_t d);
int	serial_sortrev(const void *a, const void *b);
void	sccs_perfile(sccs *s, FILE *out, int patch);
int	sccs_getperfile(sccs *, FILE *, int *);
char	*sccs_gethost(void);
char	*sccs_realhost(void);
char	*sccs_host(void);
char	**sccs_getComments(char *prompt);
int	sccs_badTag(char *tag, u32 flags);
MDBM    *sccs_keys2mdbm(FILE *f);
void	sfileUnget(void);
char	*sfileNext(void);
char	*sfileRev(void);
char	*sfileFirst(char *cmd, char **Av, int Flags);
int	sfileDone(void);
int	sfiles(char **av);
char	*sfiles_local(char *rev, char *opts);
int	sfilesDied(int wait, int killit);
ser_t	sccs_findrev(sccs *, char *);
ser_t	sccs_top(sccs *);
ser_t	sccs_findKey(sccs *, char *);
ser_t	sccs_findSortKey(sccs *s, char *sortkey);
int	isKey(char *key);
ser_t	sccs_findMD5(sccs *s, char *md5);
ser_t	sccs_dInit(ser_t, char, sccs *, int);
char	*sccs_getuser(void);
void	sccs_resetuser(void);
void	sccs_resethost(void);
char	*sccs_realuser(void);
char	*sccs_user(void);

int	bin_needHeapRepack(sccs *s);
void	bin_heapRepack(sccs *s);

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
char	*sccs2name(char *);
char	*name2sccs(char *);
int	diff(char *lfile, char *rfile, u32 kind, char *out);
int	check_gfile(sccs*, int);
void	platformSpecificInit(char *);
MDBM	*loadDB(char *file, int (*want)(char *), int style);
ser_t 	mkOneZero(sccs *s);
int	isCsetFile(char *);
int	cset_inex(int flags, char *op, char *revs);
void	sccs_fixDates(sccs *);
char	*xflags2a(u32 flags);
u32	a2xflag(char *str);
void	sccs_mkroot(char *root);
int	sccs_parent_revs(sccs *s, char *rev, char **revP, char **revM);
char	*sccs_setpathname(sccs *s);
ser_t	sccs_prev(sccs *s, ser_t d);
ser_t	sccs_next(sccs *s, ser_t d);
int	sccs_reCache(int quiet);
int	sccs_findtips(sccs *s, ser_t *a, ser_t *b);
int	sccs_resolveFiles(sccs *s, int merge);
sccs	*sccs_keyinit(project *proj, char *key, u32 flags, MDBM *idDB);
sccs	*sccs_keyinitAndCache(
	    project *proj, char *key, u32 flags, MDBM *sDB, MDBM *idDB);

int	sccs_lockfile(char *lockfile, int wait, int quiet);
int	sccs_stalelock(char *lockfile, int discard);
int	sccs_unlockfile(char *file);
int	sccs_mylock(char *lockf);
int	sccs_readlockf(char *file, pid_t *pidp, char **hostp, time_t *tp);

char	*sccs_utctime(sccs *s, ser_t d, char *buf);
int	delta_strftime(char *out, int sz, char *fmt, sccs *s, ser_t d);
char	*delta_sdate(sccs *s, ser_t d);
ser_t	sccs_kid(sccs *s, ser_t d);
void	sccs_mkKidList(sccs *s);
void	sccs_renumber(sccs *s, u32 flags);
int	linelen(char *s);
char	*mkline(char *mmap);
char    *mode2FileType(mode_t m);
#define	getline bk_getline
int	getline(int in, char *buf, int size);
void	explodeKey(char *key, char *parts[6]);
void	free_pfile(pfile *pf);
int	sccs_read_pfile(sccs *s, pfile *pf);
int	sccs_rewrite_pfile(sccs *s, pfile *pf);
int	sccs_isleaf(sccs *s, ser_t d);
int	emptyDir(char *dir);
int	gone(char *key, MDBM *db);
int	sccs_mv(char *, char *, int, int, int, int, MDBM *idcache);
ser_t	sccs_gca(sccs *, ser_t l, ser_t r, char **i, char **x);
char	*_relativeName(char *gName, int isDir,
	    int mustHaveRmarker, int wantRealName, project *proj);
char	*findBin(void);
int 	prompt(char *msg, char *buf);
void	parse_url(char *url, char *host, char *path);
int	parallel(char *path, int write);
int	fstype(char *path);
int	cpus(void);
char	*sccs_saveStr(sccs *s, char *str);
char	*sccs_Xfile(sccs *s, char type);
FILE	*sccs_startWrite(sccs *s);
int	sccs_finishWrite(sccs *s);
void	sccs_abortWrite(sccs *s);
int	uniq_adjust(sccs *s, ser_t d);
char	*uniq_dbdir(void);
int	uniqdb_req(char *msg, int msglen, char *resp, size_t *resplen);
time_t	sccs_date2time(char *date, char *zone);
pid_t	smtpmail(char **to, char *subject, char *file);
int	connect_srv(char *srv, int port, int trace);
int	get(char *path, int flags);
int	gethelp(char *helptxt, char *help_name, char *bkarg, char *prefix, FILE *f);
void	notify(void);
char	*package_name(void);
int	bkusers(sccs *s, char *prefix, FILE *out);
int	sfiles_glob(char *glob);
void	send_request(int fd, char * request, int len);
int	writen(int to, void *buf, int size);
int	fd2file(int fd, char *file);
int	repository_downgrade(project *p);
int	repository_locked(project *p);
int	repository_mine(project *p, char type);
int	repository_lockers(project *p);
int	repository_rdlock(project *p);
int	repository_rdunlock(project *p, int all);
void	repository_rdunlockf(project *p, char *file);
void	repository_unlock(project *p, int all);
int	repository_wrlock(project *p);
int	repository_wrunlock(project *p, int all);
int	repository_hasLocks(project *p, char *dir);
void	repository_lockcleanup(project *p);
u32	repo_nfiles(project *proj, filecnt *nf);
void	repo_nfilesUpdate(filecnt *nf);
int	comments_save(char *s);
int	comments_savefile(char *s);
int	comments_got(void);
void	comments_done(void);
char	**comments_return(char *prompt);
ser_t	comments_get(char *gfile, char *rev, sccs *s, ser_t d);
void	comments_writefile(char *file);
int	comments_checkStr(u8 *s, int len);
int	bk_sfiles(char *opts, int ac, char **av);
int	outc(char c);
void	error(const char *fmt, ...);
MDBM	*loadConfig(project *p, int forcelocal);
int	ascii(char *file);
char	*sccs_rmName(sccs *s);
char	*key2rmName(char *rootkey);
int	sccs_rm(char *name, int force, MDBM *idcache);
void	sccs_rmEmptyDirs(char *path);
void	do_prsdelta(char *file, char *rev, int flags, char *dspec, FILE *out);
char 	**get_http_proxy(char *host);
int	confirm(char *msg);
int	cset_setup(int flags);
char	*separator(char *);
int	trigger(char *cmd, char *when);
void	trigger_setQuiet(int yes);
void	cmdlog_start(char **av, int bkd_mode);
void	cmdlog_addnote(const char *key, const char *val);
int	cmdlog_end(int ret, int bkd_mode);
void	cmdlog_lock(int flags);
void	cmdlog_unlock(int flags);
void	callstack_push(int remote);
void	callstack_pop(void);
int	write_log(char *file, char *format, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 2, 3)))
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
symbol	*sccs_walkTags(symbol *sym, sccs *s, ser_t d, int active, int meta);
u8	*sccs_set(sccs *s, ser_t d, ser_t m, char *iLst, char *xLst);
int	sccs_graph(sccs *s, ser_t d, u8 *map, char **inc, char **exc);
int	sccs_setCludes(sccs *sc, ser_t d, char *iLst, char *xLst);
int	sccs_isPending(char *gfile);
int	isReachable(sccs *s, ser_t start, ser_t stop);
int	stripdel_setMeta(sccs *s, int stripBranches, int *count);

int     http_connect(remote *r);
int	http_send(remote *, char *msg, size_t len, size_t ex, char *ua);
int	http_fetch(remote *r, char *file);
char	*_bktmp(char *file, int line, char *buf);
char	*_bktmp_dir(char *file, int line, char *buf);
char	*_bktmp_local(char *file, int line, char *buf);
#define	bktmp(buf) _bktmp(__FILE__, __LINE__, buf)
#define	bktmp_dir(buf) _bktmp_dir(__FILE__, __LINE__, buf)
#define	bktmp_local(buf) _bktmp_local(__FILE__, __LINE__, buf)
void	bktmpenv(void);
void	bktmpcleanup(void);
int	smallTree(int threshold);
char	*strnonldup(char *s);
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
void	rmEmptyDirs(hash *empty);
int	after(int quiet, int verbose, char *rev);
int	consistency(int quiet);
int	diff_gfile(sccs *s, pfile *pf, int expandKeyWord, char *tmpfile);
char	*getCSetFile(project *p);
int	spawn_cmd(int flag, char **av);
pid_t	mkpager(void);
int	getRealName(char *path, MDBM *db, char *realname);
int	addsym(sccs *s, ser_t metad, int graph, char *tag);
int	delta_table(sccs *s, int willfix);
int	walksfiles(char *dir, filefn *fn, void *data);
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
ser_t	cset_insert(sccs *s, FILE *iF, FILE *dF, ser_t parent, int fast);
int	cset_write(sccs *s, int spinners, int fast);
sccs	*cset_fixLinuxKernelChecksum(sccs *s);
int	cweave_init(sccs *s, int extras);
void	weave_set(sccs *s, ser_t d, char **keys);
void	weave_cvt(sccs *s);
void	weave_replace(sccs *s, weave *cweave);
void	weave_updateMarker(sccs *s, ser_t d, u32 rk, int add);
int	isNullFile(char *rev, char *file);
int	weave_iscomp(sccs *s, u32 rkoff);
int	weave_isBAM(sccs *s, u32 rkoff);
u32	rset_checksum(sccs *cset, ser_t d, ser_t base);
rset_df	*rset_diff(sccs *cset,
    ser_t left, ser_t left2, ser_t right, int showgone);
unsigned long	ns_sock_host2ip(char *host, int trace);
unsigned long	host2ip(char *host, int trace);
int	fileTypeOk(mode_t m);
int	sccs_tagLeaf(sccs *s, ser_t d, ser_t md, char *tag);
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
int	sccs_needSwap(sccs *s, ser_t p, ser_t m, int warn);
int	chk_host(void);
int	chk_user(void);
int	chk_nlbug(sccs *s);
int	chk_permissions(void);
int	fix_gmode(sccs *s, int gflags);
int	do_checkout(sccs *s, u32 getFlags, char ***bamFiles);
int	unsafe_path(char *s);
int	hasTriggers(char *cmd, char *when);
void	comments_cleancfile(sccs *s);
int	comments_readcfile(sccs *s, int prompt, ser_t d);
int	comments_prompt(char *file);
void	saveEnviroment(char *patch);
void	restoreEnviroment(char *patch);
int	run_check(int quiet, int verbose, char **flist, char *opts, int *did_partial);
int	full_check(void);
int	cset_needRepack(void);
char	*key2path(char *key, MDBM *idDB, MDBM *done, MDBM **m2k);
char	*hashstr(char *str, int len);
char	*hashstream(int fd);
char	*secure_hashstr(char *str, int len, char *key);
int	isNetworkFS(char *path);
void	log_rotate(char *path);
void	sccs_saveNum(FILE *f, int num, int sign);
int	sccs_eachNum(char **linep, int *signp);

int	attach_name(sccs *cset, char *name, int setmarks);
void	notice(char *key, char *arg, char *type);
ser_t	sccs_getedit(sccs *s, char **revp);
void	line2av(char *cmd, char **av);
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
void	bk_preSpawnHook(int flags, char *av[]);
void	lockfile_cleanup(void);

int	diff_cleanOpts(df_opt *opts);
int	diff_files(char *file1, char *file2, df_opt *opts, char *out);
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
int	annotate_args(int flags, char *args);
void	platformInit(char **av);
int	sccs_fastWeave(sccs *s);
int	sccs_slowWeave(sccs *s);
void	sccs_clearbits(sccs *s, u32 flags);
MDBM	*db_load(char *gfile, sccs *s, char *rev, MDBM *m);
int	db_sort(char *gfile_in, char *gfile_out);
int	db_store(char *gfile, MDBM *m);
char	**getParkComment(int *err);
int	launch_wish(char *script, char **av);
void	converge_hash_files(void);
int	isConvergeFile(char *file);
int	getMsg(char *msg_name, char *bkarg, char b, FILE *outf);
int	getMsg2(char *msg_name, char *arg, char *arg2, char b, FILE *outf);
int	getMsgP(char *msg_name, char *bkarg, char *prefix, char b, FILE *outf);
int	getMsgv(char *msg_name, char **bkarg, char *prefix, char b, FILE *outf);
void	randomBits(char *buf);
sum_t	almostUnique(void);
int	uninstall(char *path, int upgrade);
void	delete_onReboot(char *path);
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
int	bp_get(sccs *s, ser_t d, u32 flags, char *outf, FILE *out);
int	bp_delta(sccs *s, ser_t d);
int	bp_diff(sccs *s, ser_t d, char *gfile);
int	bp_updateServer(char *range, char *list, int quiet);
int	bp_sharedServer(void);
char	*bp_serverURL(char *out);
char	*bp_serverID(char *out, int notme);
char	*bp_serverURL2ID(char *url);
void	bp_setBAMserver(char *path, char *url, char *repoid);
int	bp_hasBAM(void);
u32	send_BAM_sfio(FILE *wf, char *bp_keys, u64 bpsz, int gzip, int quiet, int verbose);
int	bkd_BAM_part3(remote *r, char **env, int quiet, char *range);
int	bp_sendkeys(FILE *f, char *range, u64 *bytes, int gzip);
int	detach(int quiet, int verbose);
char	*psize(u64 bytes);
u64	scansize(char *bytes);
void	idcache_update(char **files);
int	idcache_item(MDBM *idDB, char *rkey, char *path);
int	idcache_write(project *p, MDBM *idDB);
void	cset_savetip(sccs *s);
void	cset_updatetip(void);
void	clearCsets(sccs *s);
void	sccs_rdweaveInit(sccs *s);
char	*sccs_nextdata(sccs *s);

#define	RWP_DSET	0x00000001 /* only walk D_SET deltas */
#define	RWP_ONE		0x00000002 /* stop at end of current delta */
ser_t	cset_rdweavePair(sccs *s, u32 flags, u32 *rkoff, u32 *dkoff);
void	cset_firstPair(sccs *s, ser_t first);
void	cset_firstPairReverse(sccs *s, ser_t first);
int	sccs_rdweaveDone(sccs *s);
FILE	*sccs_wrweaveInit(sccs *s);
FILE	*sccs_wrweaveDone(sccs *s);
int	hasLocalWork(char *gfile);
char	*goneFile(void);
char	*sgoneFile(void);
int	keycmp(const void *k1, const void *k2);
int	keycmp_nopath(char *k1, char *k2);
int	key_sort(const void *a, const void *b);
int	earlier(sccs *s, ser_t a, ser_t b);
#ifdef	WIN32
int	startmenu_list(u32, char *);
int	startmenu_rm(u32, char *);
int	startmenu_get(u32, char *path);
int	startmenu_set(u32, char *, char *, char *, char *, char *);
void	startmenu_install(char *dest);
void	startmenu_uninstall(FILE *log);
char	*bkmenupath(u32 user, int create, int isthere);
#else
int	__startmenu_generic(void);
void	*__startmenu_generic_ptr(void);
#define	startmenu_list(...)		__startmenu_generic()
#define	startmenu_rm(...)		__startmenu_generic()
#define	startmenu_get(...)		__startmenu_generic()
#define	startmenu_set(...)		__startmenu_generic()
#define	startmenu_install(...)		__startmenu_generic()
#define	startmenu_uninstall(...)	__startmenu_generic()
#define	bkmenupath(...)			__startmenu_generic_ptr()
#endif
void	repos_update(project *proj);
char	*bk_searchFile(char *base);
void	dspec_collapse(char **dspec, char **begin, char **end);
void	fslayer_cleanup(void);
void	updatePending(sccs *s);
#define	IS_FILE	1
#define	IS_DIR	2
int	isSCCS(const char *path);
int	fslayer_enable(int en);
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
char	*file_fanout(char *file);
int	upgrade_latestVersion(char *new_vers, char *new_utc);
void	upgrade_maybeNag(char *out);
int	attr_update(void);
int	attr_write(char *file);
int	bk_badFilename(project *p, char *name);

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
u32	sccs_addUniqRootkey(sccs *s, char *key);
u32	sccs_hasRootkey(sccs *s, char *key);
void	sccs_loadHeapMeta(sccs *s);
typedef	struct MAP MAP;
void	*dataAlloc(u32 esize, u32 nmemb);
void	datamap(char *name, void *start, size_t len,
    FILE *f, long off, int byteswap, int *didpage);
void	dataunmap(FILE *f, int keep);
FILE	*fopen_bkfile(char *file, char *mode, u64 size, int chkxor);
FILE	*fdopen_bkfile(FILE *f, char *mode, u64 size, int chkxor);
int	undoLimit(sccs *cs, char **limit);

#define	RGCA_ALL	0x1000
#define	RGCA_STANDALONE	0x2000
#define	RGCA_ONLYONE	0x4000
int	repogca(char **urls, char *dspec, u32 flags, FILE *out);

u64	maxrss(void);
char	*formatBits(u32 bits, ...);
int	bk_gzipLevel(void);

nokey	*nokey_newStatic(u32 data, u32 bits);
nokey	*nokey_newAlloc(void);
u32	nokey_log2size(nokey *h);
u32	*nokey_data(nokey *h);
void	nokey_free(nokey *h);
u32	nokey_lookup(nokey *h, char *heap, char *key);
void	nokey_insert(nokey *h, char *heap, u32 key);

typedef struct {
	pcre	*re;		/* handle to compiled regex */
	char    *pattern;	/* what we want to find */
	u8      ignorecase:1;	/* duh */
	u8	want_glob:1;	/* do a glob based search */
	u8	want_re:1;	/* do a regex based search */
} search;

int	search_either(char *s, search search);
int	search_glob(char *s, search search);
int	search_regex(char *s, search search);
search	search_parse(char *str);
void	search_free(search search);

extern	char	*editor;
extern	char	*bin;
extern	char	*BitKeeper;
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
extern	u32	swapsz;		/* paging blocksize */

#define	componentKey(k) (strstr(k, "/ChangeSet|") != (char*)0)
#define	changesetKey(k) (strstr(k, "|ChangeSet|") != (char*)0)
#define	BAMkey(k) ({ \
	char	*_p; ((_p = strrchr(k, '|')) && strneq(_p+1, "B:", 2)); })

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
#define	CMD_COMPAT_NOSI		0x00000200	/* compat, no server info */
#define	CMD_IGNORE_RESYNC	0x00000400	/* ignore resync lock */
#define	CMD_BKD_CMD		0x00002000	/* command comes from bkd.c */
#define	CMD_NOLOG		0x00004000	/* don't log command */

#define	LOGVER			1		/* dflt idx into log_versions */

#endif	/* _SCCS_H_ */
