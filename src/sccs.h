/* %W% Copyright (c) 1997 Larry McVoy */
#ifndef	_SCCS_H_
#define	_SCCS_H_
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "purify.h"
#include "mdbm/mdbm.h"
#ifndef	WIN32
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <setjmp.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <netdb.h>
#include <fcntl.h>
#include "purify.h"
#else
#include "win32hdr.h"
#endif

#define	HAVE_GETDOMAINNAME
#ifdef	PROFILE
#define	private
#define	inline
#else
#define	private static
#endif
#if	defined(ANSIC)
#if	!defined(inline)
#define	inline
#endif

#define	isascii(x)	(((x) & ~0x7f) == 0)
#undef	HAVE_GETDOMAINNAME
extern	char *strdup(char *s);
#endif

/*
 * Local config starts here.
 */
#ifndef	SDIFF
#	define	SDIFF	"/usr/bin/sdiff"
#endif

/*
 * Define this to use GNU diff instead of built in diff.
 * This is currently the right answer, there are problems with the
 * builtin diff.
 */
#define	DIFF	"/usr/bin/diff"

/*
 * define used to improve portability
 * may be redefined in platform specific header
 * ( see re_def.h )
 */
#define	DEV_NULL	"/dev/null"
#define	TMP_PATH	"/tmp"
#define	IsFullPath(f)	((f)[0] == '/')
#define loadNetLib()
#define unLoadNetLib()


/*
 * If you want to use stdio for all I/O, which is slightlu slower but
 * more portable, define this:
#define	USE_STDIO
 */

#define	UMASK(x)	((x) & u_mask)

#if	(!defined(__GNUC__) || defined(DEBUG)) && !defined(inline)
#	define	inline
#endif
#ifdef	DEBUG
#	define	debug(x)	fprintf x
#else
#	define	debug(x)
#	define	debug_main(x)
#endif
#ifdef	DEBUG2
#	define	debug2(x)	fprintf x
#else
#	define	debug2(x)
#endif
#ifdef LINT
#	define	WHATSTR(X)	pid_t	getpid(void)
#else
#	define	WHATSTR(X)	static char *what = X
#endif
#ifndef	PURIFY
#	define	malloc(s)	chk_malloc(s, __FILE__, __LINE__)
#	define	calloc(n, s)	chk_calloc(n, s, __FILE__, __LINE__)
#endif
#define	max(a,b)	((a)>(b)?(a):(b))
#define	min(a,b)	((a)<(b)?(a):(b))
#define	bcopy(s,d,l)	memmove(d,s,l)
#define	bcmp(a,b,l)	memcmp(a,b,l)
#define	bzero(s,l)	memset(s,0,l)
#define	streq(a,b)	!strcmp(a,b)
#define	strneq(a,b,n)	!strncmp(a,b,n)
#define	index(s, c)	strchr(s, c)
#define	rindex(s, c)	strrchr(s, c)
#define	exists(f)	(access(f, F_OK) == 0)
#define	notnull(s)	(s ? s : "")
#define	unless(e)	if (!(e))
#define	verbose(x)	unless (flags&SILENT) fprintf x
#define fnext(buf, in)  fgets(buf, sizeof(buf), in)
#define	unlink(f)	smartUnlink(f)
#define	rename(o, n)	smartRename(o, n)
#ifdef	USE_STDIO
#define next(buf, s)	fnext(buf, s->file)
#define	seekto(s,where)	fseek(s->file, (long)where, SEEK_SET)
#define	tell(s)		ftell(s->file)
#define	eof(s)		feof(s->file)
#define	peekc(c,s)	c = fgetc(s->file); ungetc(c, s->file)
#define	BUF(b)		char b[1024]
#else
#define next(buf, s)	(buf = fastnext(s))
#define	seekto(s,o)	s->where = (s->mmap + o)
#define	tell(s)		(s->where - s->mmap)
#define	eof(s)		(s->where >= s->mmap + s->size)
#define	peekc(c,s)	c = *s->where
#define	BUF(b)		char *b
#endif
#define	EACH(s)		for (i = 1; (s) && (i < (int)(s[0])) && (s[i]); i++)
#define	LPAD_SIZE	70

/*
 * Flags that modify some operation (passed to sccs_*).
 */
#define	SILENT		0x00000001	/* do work quietly */
#define	EDIT		0x00000002	/* get -e: get for editting */
#define	EXPAND		0x00000004	/* expand keywords */
#define	REVNUMS		0x00000008	/* get -m: prefix each line with rev */
#define	PRINT		0x00000010	/* get/delta -p: [diffs] to stdout */
#define USER		0x00000020	/* get -u: prefix with user name */
#define SKIPGET		0x00000040	/* get -g: don't get the file */
#define	FORCE		0x00000080	/* delta -f: force a delta */
#define	SAVEGFILE	0x00000100	/* delta -n: save edited gfile */
#define	UNEDIT		0x00000200	/* clean -u: unedit - discard changes */
#define	NEWFILE		0x00000400	/* delta -i: create initial file */
/*					   AVAILABLE */
#define	PATCH		0x00001000	/* mk/tkpatch, delta -R */
#define	NOCKSUM		0x00002000	/* don't do the checksum */
#define	FORCEBRANCH	0x00004000	/* force a branch when creating delta */
#define	CHECKFILE	0x00008000	/* check file format (admin) */
#define	NEWCKSUM	0x00010000	/* Redo checksum */
#define	RCSEXPAND	0x00020000	/* do RCS keywords */
#define	GETDIFFS	0x00040000	/* generate diffs w/ just add/delete */
#define	TOP		0x00080000	/* use the top rev (also mkpatch) */
#define	EMPTY		0x00100000	/* initialize with empty file */
#define	DONTASK		0x00200000	/* don't ask for comments */
#define	FORCEASCII	0x00400000	/* Do not gunzip/uudecode */
#define	MAP_WRITE	0x00800000	/* map the file read/write */
#define	MODNAME		0x01000000	/* get -n: prefix with %M */
#define	PREFIXDATE	0x02000000	/* get -d: show date */
#define	CHECKASCII	0x04000000	/* check file format (admin) */
#define	LINENUM		0x08000000	/* get -N: show line numbers */
#define	VERBOSE		0x10000000	/* when !SILENT isn't enough */
#define SHUTUP		0x20000000	/* when SILENT isn't enough */
#define GTIME		0x40000000	/* use g file mod time as time stamp */

/*
 * Flags (s->state) that indicate the state of a file.  Set up in init.
 * Also used for sccs_files() flags.
 */
#define	SFILE		0x00000001	/* s->sfile exists as a regular file */
#define	GFILE		0x00000002	/* s->gfile exists as a regular file */
#define	PFILE		0x00000004	/* SCCS/p.file exists */
#define	ZFILE		0x00000008	/* SCCS/z.file exists */
#define	WRITE_OK	0x00000010	/* s->gfile is writable */
#define	SOPEN		0x00000020	/* s->sfile is open */
/*			0x00000040	AVAILABLE */
/*			0x00000080	AVAILABLE */
#define	WARNED		0x00000100	/* error message already sent */
#define	KEYWORDS	0x00000200	/* last line gotten has %x% */
#define EDITED		(SFILE|PFILE|GFILE)
#define	RCS		0x00000400	/* expand RCS keywords */
#define	BRANCHOK	0x00000800	/* branching allowed */
#define	NOEXPAND	0x00001000	/* don't auto expand in sccs_files */
#define	CHMOD		0x00002000	/* change the file back to 0444 mode */
#define	YEAR4		0x00004000	/* print out the year as 4 digits */
#define	BADREVS		0x00008000	/* has corrupted revisions */
#define	BIGPAD		0x00010000	/* admin -B: make the landing pad big */
#define	HASREVS		0x00020000	/* sfiles - filename:rev */
#define	REINHERIT	0x00040000	/* found stuff in flags, reinherit */
#define	BITKEEPER	0x00080000	/* X_BITKEEPER flag */
#define	CSET		0x00100000	/* this is a changeset file */
#define	NOSCCSDIR	0x00200000	/* this is a s.foo not SCCS/s.foo */
#define MAPPRIVATE	0x00400000	/* Some winblows hack */
#define	ONE_ZERO	0x00800000	/* initial rev, make it be 1.0 */
#define READ_ONLY	0x01000000	/* force read only mode */
#define Z_LOCKED	0x02000000	/* file is locked */
#define	RANGE2		0x04000000	/* second call for date|rev range */
#define	BAD_DSUM	0x08000000	/* patch checksum mismatch */

/*
 * Options to sccs_diffs()
 */
#define	D_DIFF		'd'
#define	D_SDIFF		's'
#define	D_CONTEXT	'c'
#define	D_UNIFIED	'u'
#define	D_PDIFF		'p'
#define	D_RCS		'n'

/*
 * Date handling.
 */
#define	ROUNDUP	1
#define	EXACT	0
#define	ROUNDDOWN -1
#define	DATE(d)	(d->date ? d->date : getDate(d))

/*
 * Bits for the x flag in the s.file.
 *
 * Nota bene: these can not change once the product is shipped.  Ever.
 * They are stored on disk.
 */
#define	X_BITKEEPER	0x00000001	/* BitKeeper file, not SCCS */
#define	X_RCSEXPAND	0x00000002	/* RCS keywords */
#define	X_YEAR4		0x00000004	/* 4 digit years */

/*
 * Encoding flags.
 */
#define	E_ASCII		0		/* no encoding */
#define	E_UUENCODE	1		/* uuenecode it (traditional) */
#define	E_UUGZIP	2		/* gzip and uuencode */

#define	HAS_GFILE(s)	((s)->state & GFILE)
#define	HAS_PFILE(s)	((s)->state & PFILE)
#define	HAS_ZFILE(s)	((s)->state & ZFILE)
#define	HAS_SFILE(s)	((s)->state & SFILE)
#define	BEEN_WARNED(s)	((s)->state & WARNED)
#define	IS_WRITABLE(s)	((s)->mode & 0200)
#define IS_EDITED(s)	((((s)->state & EDITED) == EDITED) && IS_WRITABLE(s))

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
#define	D_BRANCH	0x00000100	/* this delta wants to be the root
					 * of a branch, not a child of the
					 * parent's LOD.  */
#define	D_BADFORM	0x00000200	/* poorly formed rev */
#define	D_BADREV	0x00000400	/* bad parent/child relationship */
#define	D_STEAL		0x00000800	/* sccslog is stealing this rev */
#define	D_META		0x00001000	/* this is a metadata removed delta */
#define	D_SYMBOLS	0x00002000	/* delta has one or more symbols */
#define	D_DUPCSETFILE	0x00004000	/* this changesetFile is shared */
#define	D_VISITED	0x00008000	/* and had a nice cup of tea */
#define	D_CKSUM		0x00010000	/* delta has checksum */
#define	D_MERGED	0x00020000	/* some delta has merged this one */
#define	D_LODZERO	0x00040000	/* a .0 delta of an LOD */
#define	D_LODHEAD	0x00080000	/* a .1 delta of an LOD */
#define	D_LODCONT	0x00100000	/* LOD continuation down a branch */
#define	D_DUPLOD	0x00200000	/* shared ->lod */
#define	D_LODSTR	0x00400000	/* lod pointer is a string: getInit() */
#define	D_GONE		0x00800000	/* this delta is gone, don't print */
#define D_ICKSUM	0x01000000	/* use checksum from init file */
#define	D_MODE		0x02000000	/* permissions in d->mode are valid */

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

#define	MAXREV	24	/* 99999.99999.99999.99999 */

#define	SCCSTMP		"SCCS/T.SCCSTMP"	/* XXX - .SCCS for Linus? */
#define	BKROOT		"BitKeeper/etc"
#define	CHANGESET	"SCCS/s.ChangeSet"	/* Ditto */
#define	PENDING		"SCCS/x.pending_cache"	/* Ditto */
#define	ID		"SCCS/x.id_cache"	/* Ditto */

#define	ROOT_USER	"root"
#define	UNKNOWN_USER	"anon"

#define	isData(buf) (buf[0] != '\001')

typedef	unsigned short	ser_t;
typedef	unsigned short	sum_t;
typedef	unsigned short	u16;

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
 * Fri Apr  9 1999:  his has grown to 124 bytes.  We might want to think
 * about reducing the size.  That means a 5K delta file takes 620K in 
 * memory just for the graph.
 *
 * Something else we ought to do: our own allocator/deallocator which allocates
 * enough memory for all the data structures at init time.  That way we can
 * fail the init if there is no memory.
 */
typedef struct delta {
	/* stuff in original SCCS */
	u16	added;			/* lines added by this delta */
	u16	deleted;		/* and deleted */
	u16	same;			/* and unchanged */
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
	char	*hostname;		/* hostname where revision was made */
	char	*pathname;		/* pathname to the file */
	char	*zone;			/* 08:00 is time relative to GMT */
	char	*cset;			/* id for delta in the ChangeSet file */
	char	*csetFile;		/* id for ChangeSet file itself */
	ser_t	merge;			/* serial number merged into here */
	sum_t	sum;			/* checksum of gfile */
	time_t	dateFudge;		/* make dates go forward */
	mode_t	mode;			/* 0777 style modes */
	/* In memory only stuff */
	u16	r[4];			/* 1.2.3 -> 1, 2, 3, 0 */
	u16	lodr[3];		/* Same as above for LODs */
	time_t	date;			/* date - conversion from sdate/zone */
	char	*sym;			/* used only for getInit(), see above */
	struct	lod *lod;		/* used for getInit() and later */
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
 * Lod update:
 * If the delta has D_LODZERO set then it is a parent LOD, i.e., it is the .0
 * node for an LOD.  There can be multiple LOD's rooted at one parent, so
 * we have to search the lod list if we need to find them.  The d* in the lod
 * points at this delta if there are no deltas in the LOD yet.
 * If the delta has D_LODHEAD set, it is the first delta in the LOD and the
 * lod pointer is valid.  The d* in the lod points at this delta.
 * If the delta has D_LODCONT set, it is a continuation of the LOD which has
 * gone around a branch.  The lod pointer is valid but is a dup.
 * If the delta has D_DUPLOD set, it is some delta in the LOD, not the head
 * delta.  The lod pointer is valid, but it is a dup.
 *
 * Takepatch implications: this means that all deltas in a LOD need to print
 * out their LOD name so that the right thing happens when we smoosh.  And
 * in dinsert() if the lod is set, we need to make sure that it isn't a dup
 * and if so make it be set up like a dup.
 */

/*
 * Symbolic lines of development.
 * Like symbols only they apply to an indefinite number of revisions.
 * The same LOD name can occur multiple times, once per each branch in the
 * LOD.  To get to the tip of an LOD, walk this list, take the last LOD
 * which matches, and go to the top of that.
 * This means that the LODS are in the list oldest .. newest.
 */
typedef	struct lod {		
	struct	lod	*next;		/* list of all heads of LODs, this
					 * list is unique in the name field. */
	char	*name;			/* the LOD name */
	ser_t	*heads;			/* the .1 ser and each branch head */
	delta	*d;			/* the .0 rev of the LOD */
} lod;

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
	struct	serial *next;		/* forward & */
	struct	serial *prev;		/* ... back links when allocated */
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

/*
 * struct sccs - the delta tree, the data, and associated junk.
 */
typedef	struct sccs {
	delta	*tree;		/* the delta tree after mkgraph() */
	delta	*table;		/* the delta table list, newest..oldest */
	delta	*lastinsert;	/* pointer to the last delta inserted */
	delta	*meta;		/* deltas in the meta data list */
	symbol	*symbols;	/* symbolic tags sorted most recent to least */
	symbol	*symTail;	/* last symbol, for tail inserts */
	lod	*lods;		/* all lines of development */
	char	*defbranch;	/* defbranch, if set */
	int	numdeltas;	/* number of entries in the graph */
	delta	**ser2delta;	/* indexed by serial, returns delta */
	int	ser2dsize;	/* just to be sure */
#ifdef	USE_STDIO
	FILE	*file;		/* open to ... */
#else
	char	*mmap;		/* mapped file */
	char	*where;		/* where we are in the mapped file */
	off_t	size;		/* size of mapping */
	char	*landingpad;	/* some space for symbols */
#endif
	int	fd;		/* cached copy of the file descriptor */
	char	*sfile;		/* SCCS/s.foo.c */
	char	*pfile;		/* SCCS/p.foo.c */
	char	*zfile;		/* SCCS/z.foo.c */
	char	*gfile;		/* foo.c */
	char	*root;		/* to the root of the project; optional */
	char	**usersgroups;	/* lm, beth, staff, etc */
	int	encoding;	/* ascii, uuencode, gzip, etc. */
	char	**flags;	/* flags in the middle that we didn't grok */
	char	**text;		/* descriptive text */
	int	state;		/* GFILE/SFILE etc */
	mode_t	mode;		/* mode of the gfile */
	off_t	data;		/* offset to data in file */
	int	nextserial;	/* next unused serial # */
	delta	*rstart;	/* start of a range (1.1 - oldest) */
	delta	*rstop;		/* end of range (1.5 - youngest) */
	sum_t	 cksum;		/* SCCS chksum */
	sum_t	 dsum;		/* SCCS delta chksum */
	off_t	sumOff;		/* offset of the new delta cksum */
	time_t	gtime;		/* gfile modidification time */
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
	lod	*l;			/* if set, this the lod for new d */
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
	char	*initFile;	/* RESYNC/BitKeeper/init-1 */
	char	*diffFile;	/* RESYNC/BitKeeper/diff-1 */
	time_t	order;		/* ordering over the whole list, oldest first */
	int	flags;		/* local/remote /etc */
	sccs	*init;		/* if new file, then init from here */
	struct	patch *next;	/* guess */
} patch;

#define	PATCH_VERSION	"# BitKeeper Patch version 0.2\n"

#define	PATCH_LOCAL	0x0001	/* patch is from local file */
#define	PATCH_REMOTE	0x0002	/* patch is from remote file */
#define	PATCH_META	0x0004	/* delta is metadata */

/* getopt stuff */
#define getopt	mygetopt
#define optind	myoptind
#define optarg	myoptarg
#define	opterr	myopterr
#define	optopt	myoptopt
extern	int	optind;
extern	int	opterr;
extern	int	optopt;
extern	char	*optarg;
int	getopt(int ac, char **av, char *opts);

int	sccs_admin(sccs *sc, int flgs,
	    admin *f, admin *l, admin *u, admin *s, char *txt);
int	sccs_checkin(sccs *s, int flags, delta *d);
int	sccs_delta(sccs *s, int flags, delta *d, FILE *init, FILE *diffs);
int	sccs_diffs(sccs *s, char *r1, char *r2, int flags, char kind, FILE *);
int	sccs_get(sccs *s,
	    char *rev, char *mRev, char *i, char *x, int flags, char *out);
int	sccs_clean(sccs *s, int flags);
int	sccs_info(sccs *s, int flags);
int	sccs_prs(sccs *s, int flags, char *dspec, FILE *out);
delta	*sccs_getrev(sccs *s, char *rev, char *date, int roundup);
delta	*sccs_findDelta(sccs *s, delta *d);
sccs	*sccs_init(char *filename, int flags);
sccs	*sccs_restart(sccs *s);
void	sccs_free(sccs *);
void	sccs_freetree(delta *);
char	**sccs_files(char **, int);
void	sccs_freefiles();
int	sccs_smoosh(char *left, char *right);
delta	*sccs_parseArg(delta *d, char what, char *arg, int defaults);
void	sccs_whynot(char *who, sccs *s);
int	sccs_addSym(sccs *, int, char *);
void	sccs_ids(sccs *s, int flags, FILE *out);
int	sccs_hasDiffs(sccs *s, int flags);
void	sccs_print(delta *d);
delta	*sccs_getInit(sccs *s,
		delta *d, FILE *f, int patch, int *errorp, int *linesp);
delta	*sccs_ino(sccs *);
int	sccs_rmdel(sccs *s, char *rev, int destroy, int flags);
int	sccs_getdiffs(sccs *s, char *rev, int flags, char *printOut);
void	sccs_pdelta(delta *d, FILE *out);
char	*sccs_root(sccs *, char *optional_root);
int	sccs_cd2root(sccs *, char *optional_root);
delta	*sccs_key2delta(sccs *sc, char *key);
char	*sccs_impliedList(sccs *s, char *who, char *base, char *rev);
void	sccs_sdelta(char *, delta *);
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
char	*getuser(void);

delta	*modeArg(delta *d, char *arg);
FILE	*fastPopen(const char*, const char*);
int	fastPclose(FILE*);
char	chop(register char *s);
int	is_sccs(char *name);
int	isdir(char *s);
int	isreg(char *s);
int	readable(char *f);
int	writable(char *f);
int	executable(char *f);
char	*basenm(char *);
char	*sccs2name(char *);
char	*name2sccs(char *);
int	diff(char *lfile, char *rfile, char kind, char *out);
char	**addLine(char **space, char *line);
int	roundType(char *r);
sccs	*check_gfile(sccs*, int);
void	*chk_malloc(size_t s, char *, int);
void	*chk_calloc(size_t n, size_t s, char *, int);
void	platformSpecificInit(char *, int);
MDBM	*loadDB(char *file, int (*want)(char *));
MDBM	*csetIds(sccs *cset, char *rev, int all);
void	sccs_fixDates(sccs *);
void	sccs_mkroot(char *root);
#ifdef	WIN32
/*
 * Most of the WIN32 stuff is defined in re_def.h
 * this #include should be the last line in this file
 * re_def.h will redefine some of the decleration above
 * (for more info, see comments in uwtlib/wapi_intf.c)
 */
#include "re_def.h"
#endif
#endif	/* _SCCS_H_ */
