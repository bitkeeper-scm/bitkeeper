/*
 * Copyright 1999-2016 BitMover, Inc
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

#ifndef _SYSTEM_H
#define _SYSTEM_H

#define	_GNU_SOURCE

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#define	likely(e)	__builtin_expect(!!(e), 1)
#define	unlikely(e)	__builtin_expect(!!(e), 0)

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "string/local_string.h"
#ifdef WIN32
#include "win32.h"
#else
#include "unix.h"
#endif

#include "style.h"
#include "lines.h"
#include "pq.h"
#include "mmap.h"
#include "hash.h"
#include "mdbm/mdbm.h"
#include "zlib/zlib.h"

#define	FREE(x)	do { if (x) { free(x); (x) = 0; } } while (0)
#ifndef	isascii
#define	isascii(x)	(((x) & ~0x7f) == 0)
#endif

#ifndef	INT_MAX
#define	INT_MAX	0x7fffffff
#endif

#define	MAXLINE	2048
#ifndef MAXPATH
#define	MAXPATH	1024
#endif

#ifndef va_copy
/* gcc 2.95 defines __va_copy */
# ifdef __va_copy
# define va_copy(dest, src) __va_copy(dest, src)
# else
# define va_copy(dest, src) ((dest) = (src))
# endif /* __va_copy */
#endif        /* va_copy */

#ifndef offsetof
/* <stddef.h> should define this already */
#define offsetof(st, m) \
    ((size_t) ( (char *)&((st *)(0))->m - (char *)0 ))
#endif

#ifdef	__GNUC__
// from: http://gcc.gnu.org/onlinedocs/gcc-4.7.2/cpp/Common-Predefined-Macros.html#Common-Predefined-Macros

// 4.3.2 => 40320
#define GCC_VERSION (__GNUC__ * 10000 \
			+ __GNUC_MINOR__ * 100 \
			+ __GNUC_PATCHLEVEL__)
#endif

#define BIG_PIPE 4096	/* 16K pipe buffer for win32, ingored on unix */
#define	GOOD_PSIZE	(16<<10)
#define	INVALID		(void *)~0u /* invalid pointer */


#define	isDriveColonPath(p)	(isalpha((p)[0]) && ((p)[1] == ':'))
#define	executable(p)	((access(p, X_OK) == 0) && !isdir(p))

/*
 * ceiling log power 2
 *
 * round x to the next higher power of two
 *
 * from: Hacker's Delight p48
 *   or: http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
 */
#define        clp2(x) \
	({ u32 v = (x)-1; \
	v |= (v >> 1); \
	v |= (v >> 2); \
	v |= (v >> 4); \
	v |= (v >> 8); \
	v |= (v >> 16); \
	(v + 1); })

/* cleanpath.c */
char	*basenm(char *s);
void	cleanPath(char *path, char cleanPath[]);

/* concat_path.c */
void	concat_path(char *buf, char *first, char *second);

/* crc32c.c */
u32	crc32c(u32 crc, const void *chunk, size_t len);

/* data.c */
typedef struct {
	char	*buf;		/* user's data */
	u32	len;		/* length of user's data */
	u32	size;		/* malloc'ed size */
} DATA;

void	data_setSize(DATA *d, u32 size);
void	data_resize(DATA *d, u32 newlen);
void	data_append(DATA *d, void *data, u32 len);
#define	data_appendStr(f, s)       data_append(f, (s), strlen(s))

/* die.c */
#define	die(fmt, args...)  diefn(1, __FILE__, __LINE__, fmt, ##args)
#define	warn(fmt, args...)  diefn(0, __FILE__, __LINE__, fmt, ##args)
void	diefn(int seppuku, char *file, int line, char *fmt, ...)
#ifdef	__GNUC__
	__attribute__((format (__printf__, 4, 5)))
#endif
	;

/* dirname.c */
char	*dirname(char *path);
char	*dirname_alloc(char *path);

/* dirs.c */
char	**getdir(char *dir);
typedef	int	filefn(char *file, char type, void *data);
typedef	int	dirfn(char *file, void *data);

typedef struct {
	filefn	*file;	/* called on each inode in a dir (file, link, dir) */
	dirfn	*dir;	/* called after all inodes in a dir are done */
	dirfn	*tail;	/* called after all inodes in the subtree are done */
} walkfns;
int walkdir(char *dir, walkfns fn, void *token);

/* efopen.c */
FILE	*efopen(char *env);
int	 efprintf(char *env, char *fmt, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 2, 3)))
#endif
;

/* fileops.c */
int	fileCopy(char *from, char *to);
int	fileLink(char *from, char *to);
int	fileMove(char *from, char *to);

/* fileinfo.c */
time_t	mtime(char *s);
int	exists(char *s);
int	isdir(char *s);
int	isdir_follow(char *s);
int	isreg(char *s);
int	isSymlnk(char *s);
int	hardlinked(char *a, char *b);
int	perms(char *s);
int	writable(char *s);
int	writableReg(char *s);
off_t	fsize(int fd);
off_t	size(char *s);

/* fileutils.c */
int	cat(char *file);
char	*loadfile(char *file, int *size);
int	touch(char *file, int mode);
int	hide(char *file, int on_off);
int	sameFiles(char *file1, char *file2);

/* findpid.c */
pid_t	findpid(pid_t pid);

/* fmem.c */
FILE	*fmem(void);
FILE	*fmem_buf(void *mem, int len);
char	*fmem_peek(FILE *f, size_t *len);
char	*fmem_dup(FILE *f, size_t *len);
char	*fmem_close(FILE *f, size_t *len);
void	fmem_tests(void);
int	ftrunc(FILE *f, off_t offset);

/* fopen_cksum.c */
FILE	*fopen_cksum(FILE *f, char *mode, u16 *cksump);

/* fopen_crc.c */
FILE	*fopen_crc(FILE *f, char *mode, u64 est_size, int chkxor);

/* fopen_vzip.c */
FILE	*fopen_vzip(FILE *fin, char *mode);
int	vzip_findSeek(FILE *fin, long off, int len, u32 pagesz, u32 **lens);

/* fopen_zip.c */
FILE	*fopen_zip(FILE *f, char *mode, ...);

/* fullname.c */
char    *fullLink(char *path, char *out, int followLink);
#define	fullname(path, out)	fullLink(path, out, 1)

/* getnull.c */
char	*getNull(void);

/* getopt.c */
#define getopt	mygetopt
#define optind	myoptind
#define optarg	myoptarg
#define	opterr	myopterr
#define	optopt	myoptopt

extern	int	optind;
extern	int	opterr;
extern	int	optopt;
extern	char	*optarg;

typedef struct {
	char	*name;		/* name w args ex: "url:" */
	int	ret;		/* return value from getopt */
} longopt;

#define	GETOPT_ERR	256	/* bad option */
#define	GETOPT_NOARG	257	/* missing argument */
int	getopt(int ac, char **av, char *opts, longopt *lopts);
void	getoptReset(void);
void	getoptConsumed(int n);


/* glob.c */
char	*is_glob(char *glob);
char*	match_globs(char *string, char **globs, int ignorecase);
int	match_one(char *string, char *glob, int ignorecase);

/* mkdir.c */
int	mkdirp(char *dir);
int	test_mkdirp(char *dir);
int	mkdirf(char *file);

/* milli.c */
char	*milli(void);
u32	milli_gap(void);

/* putenv.c */
#define	getenv(s)	safe_getenv(s)
#define	putenv(s)	safe_putenv("%s", s)
void	safe_putenv(char *fmt, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 1, 2)))
#endif
     ;
char	*safe_getenv(char *var);

/* readn.c */
int	readn(int from, void *buf, int size);

/* realbasename.c */
char*	realBasename(const char *path, char *realname);

/* relpath.c */
char	*relpath(const char *base, const char *path);

/* rlimit.c */
void	core(void);

/* rmtree.c */
int	rmtree(char *dir);

/* rmrepo.c */
int	rmIfRepo(char *dir);

/* samepath.c */
int	samepath(char *a, char *b);
int	patheq(char *a, char *b);
int	paths_overlap(char *a, char *b);

/* sig.c */
#define	SIG_IGNORE	0x0001		/* ignore the specified signal */
#define	SIG_DEFAULT	0x0002		/* restore old handler */

typedef	void (*handler)(int);
void	sig_catch(handler);
void	sig_restore(void);
int	sig_ignore(void);
void	sig_default(void);

/* smartrename.c */
int	smartUnlink(char *name);
int	smartRename(char *old, char *new);
int	smartMkdir(char *dir, mode_t mode);

/* spawn.c */
#ifndef WIN32
#define	_P_WAIT		0 	/* for spawnvp() */
#define	_P_NOWAIT 	1	/* for spawnvp() */
#define	_P_DETACH 	2	/* for spawnvp() */
#define	P_WAIT		_P_WAIT
#define	P_NOWAIT 	_P_NOWAIT
#define	P_DETACH 	_P_DETACH
#endif

#define	spawnvp bk_spawnvp

extern void	(*spawn_preHook)(int flags, char *av[]);
pid_t	bk_spawnvp(int flags, char *cmdname, char *av[]);
pid_t	spawnvp_ex(int flags, char *cmdname, char *av[]);
pid_t	spawnvpio(int *fd0, int *fd1, int *fd2, char *av[]);
int	spawn_filterPipeline(char **cmds);

/* stackdump.c */
char	*stackdump(void);

/* str_subst.c */
char	*str_subst(char *str, char *search, char *replace, char *output);

/* sys.c */
#define	SYS	(char*)0, 0xdeadbeef	/* this must be the last argument to
					 * all calls to sys/sysio */
int	sys(char *first, ...);
int	sysio(char *in, char *out, char *err, char *first, ...);
void	syserr(const char *postfix);

/* system.c */
#define	SYSRET(status)				\
	({ int rc = (status);			\
	   if (rc) rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 255; \
	   rc; })

#define	system(cmd)	safe_system(cmd)
#define	systemf		safe_systemf
#define	popen(f, m)	safe_popen(f, m)
#define	pclose(f)	safe_pclose(f)
#undef	fclose
#define	fclose(f)	safe_fclose(f)

int	safe_system(char *cmd);
int	safe_systemf(char *fmt, ...)
#ifdef __GNUC__
	    __attribute__((__format__(__printf__, 1, 2)))
#endif
;
FILE *	safe_popen(char *cmd, char *type);
FILE *	popenvp(char *av[], char *type);
int	safe_pclose(FILE *f);
int	safe_fclose(FILE *f);
char	*backtick(char *cmd, int *status);
char	*shell(void);

/* tcp/tcp.c */
int	tcp_server(char *addr, int port, int quiet);
int	tcp_connect(char *host, int port);
int	tcp_accept(int sock);
void	tcp_ndelay(int sock, int val);
void	tcp_reuse(int sock);
void	tcp_keepalive(int sock);
int	udp_server(char *addr, int port, int quiet);
int	udp_connect(char *host, int port);
int	readable(int fd, int sec);
int	sockport(int s);
char	*sockaddr(int);
int	isLocalHost(char *h);
char	*hostaddr(char *);
int	tcp_pair(int fds[2]);
char	*peeraddr(int s);
int	issock(int);

/* testcode.c */
void	libc_tests(void);

/* trace.c */
extern	int bk_trace;
int	indent(void);
void	trace_init(char *prog);
void	trace_msg(char *file, int line,
    const char *function, u32 bits, char *fmt, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 5, 6)))
#endif
	;
int	trace_this(char *file, int line, const char *function, u32 bits);
void	trace_free(void);

/*
 * the env var gets split and turned into these.  We check this selector
 * first, it's much faster than a bunch of glob matches.
 *
 * T_TMP traces shouldn't be checked in, they are there as a way for you
 * to pepper the code with those and find your problem; then clean them up.
 */
#define	TR_DEFAULT	0x00000001	// "default", on by default
#define	TR_FS_LAYER	0x00000002	// "fs", on if selected
#define	TR_PERF		0x00000004	// "perf", on if selected
#define	TR_IF		0x00000008	// "if", on if selected
#define	TR_DEBUG	0x00000010	// "debug", on if selected
#define	TR_LOCK		0x00000020	// "lock", on if selected
#define	TR_NESTED	0x00000040	// "nested", on if selected
#define	TR_TMP		0x00000080	// "tmp", on if selected
#define	TR_O1		0x00000100	// "o1" (paging), on if selected
#define	TR_PROJ		0x00000200	// "proj", on if selected
#define	TR_SHIP		0x00000400	// "ship", ships to customers
#define	TR_CMD		0x00000800	// log command start/stop
#define	TR_SCCS		0x00001000	// most of the sccs_* interfaces
#define	TR_INIT		0x00002000	// show sccs_init's
/* Hey!  You!  Adding a flag!  Make it match in trace_init() */
#define	TR_ALL		0xffffffff	// "all", easy way to select everything

#ifdef	USE_TRACE
#define	TRACE(args...)	\
	do { if (unlikely(bk_trace))					    \
	    trace_msg(__FILE__, __LINE__, __FUNCTION__, TR_DEFAULT, ##args);\
	} while (0)
#define	T_NUM(args...)	\
	do { if (unlikely(bk_trace))				     \
		trace_msg(__FILE__, __LINE__, __FUNCTION__, ##args); \
	} while (0)
#define	IF_TRACE							\
	if (unlikely(bk_trace) && trace_this(__FILE__, __LINE__, __FUNCTION__, TR_IF))
#else
#define	TRACE(args...) {}
#define	T_NUM(args...) {}
#define	IF_TRACE	if (0)
#endif

#define	T_FS(args...)		T_NUM(TR_FS_LAYER, ##args)
#define	T_PERF(args...)		T_NUM(TR_PERF, ##args)
#define	T_DEBUG(args...)	T_NUM(TR_DEBUG, ##args)
#define	T_LOCK(args...)		T_NUM(TR_LOCK, ##args)
#define	T_NESTED(args...)	T_NUM(TR_NESTED, ##args)
#define	T_TMP(args...)		T_NUM(TR_TMP, ##args)
#define	T_O1(args...)		T_NUM(TR_O1, ##args)
#define	T_PROJ(args...)		T_NUM(TR_PROJ, ##args)
#define	T_SCCS(args...)		T_NUM(TR_SCCS, ##args)
#define	T_INIT(args...)		T_NUM(TR_INIT, ##args)
#define	T_CMD(args...) \
	do { if (unlikely(bk_trace))					   \
		trace_msg(__FILE__,__LINE__, __FUNCTION__, TR_CMD, ##args);\
	} while (0)
#define	T_SHIP(args...) \
	do { if (unlikely(bk_trace))					    \
		trace_msg(__FILE__,__LINE__, __FUNCTION__, TR_SHIP, ##args);\
	} while (0)

/* tty.c */
#define	isatty		myisatty
int	tty_init(void);
void	tty_done(void);
int	tty_rows(void);
int	tty_cols(void);
int	tty_getchar(void);
void	tty_clear(void);
int	myisatty(int fd);

/* ttyprintf.c */
void	ttyprintf(char *fmt, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 1, 2)))
#endif
;

/* utils.c */
void	my_perror(char *, int, char *);
#define	perror(msg)	my_perror(__FILE__, __LINE__, msg)
int	chomp(char *str);
char	*eachline(char **linep, int *lenp);
char	*eachstr(char **linep, int *lenp);

/* webencode.c */
void	webencode(FILE *out, u8 *ptr, int len);
char	*webdecode(char *data, char **buf, int *sizep);

/* which.c */
char	*which(char *prog);


#include "fslayer/fslayer.h"

#endif /* _SYSTEM_H */
