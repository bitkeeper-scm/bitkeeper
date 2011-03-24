#ifndef _SYSTEM_H
#define _SYSTEM_H

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#include <assert.h>
#ifdef	PURIFY
#undef	assert
#define	assert(expr) if (!(expr)) { \
			purifyList(__FILE__, __LINE__); \
			(__assert_fail (__STRING(expr), \
                           __FILE__, __LINE__, __ASSERT_FUNCTION), 0); \
		}
#endif
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef WIN32
#include "win32.h"
#else
#include "string.h"
#include "unix.h"
#endif

#include "style.h"
#include "lines.h"
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

#define BIG_PIPE 4096	/* 16K pipe buffer for win32, ingored on unix */
#define	GOOD_PSIZE	(16<<10)
#define	INVALID		(void *)~0u /* invalid pointer */


#define	isDriveColonPath(p)	(isalpha((p)[0]) && ((p)[1] == ':'))
#define	executable(p)	((access(p, X_OK) == 0) && !isdir(p))

/* cleanpath.c */
char	*basenm(char *s);
void	cleanPath(char *path, char cleanPath[]);

/* concat_path.c */
void	concat_path(char *buf, char *first, char *second);

/* data.c */
typedef struct {
	char	*buf;		/* user's data */
	u32	len;		/* length of user's data */
	u32	size;		/* malloc'ed size */
} DATA;

void	data_resize(DATA *d, int newlen);
void	data_append(DATA *d, void *data, int len);
#define	data_appendStr(f, s)       data_append(f, (s), strlen(s))

/* dirname.c */
char	*dirname(char *path);
char	*dirname_alloc(char *path);

/* dirs.c */
#define	getdir(dir)	_getdir(dir, 0)
char	**_getdir(char *dir, struct stat *sb);
typedef	int	(*walkfn)(char *file, struct stat *statbuf, void *data);
int	walkdir(char *dir, walkfn fn, void *data);

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
off_t	fsize(int fd);
off_t	size(char *s);
int	onelink(char *s);

/* fileutils.c */
int	cat(char *file);
char	*loadfile(char *file, int *size);
int	touch(char *file, int mode);
int	hide(char *file, int on_off);
int	sameFiles(char *file1, char *file2);

/* findpid.c */
pid_t	findpid(pid_t pid);

/* fmem.c */
FILE	*fmem_open(void);
char	*fmem_getbuf(FILE *f, size_t *len);
char	*fmem_retbuf(FILE *f, size_t *len);
void	fmem_tests(void);
int	ftrunc(FILE *f, off_t offset);

/* fullname.c */
char    *fullname(char *path, char *out);

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

#define	GETOPT_ERR	256
int	getopt(int ac, char **av, char *opts, longopt *lopts);
void	getoptReset(void);


/* glob.c */

typedef struct {
	char    *pattern;	/* what we want to find */
	u8      ignorecase:1;	/* duh */
	u8	want_glob:1;	/* do a glob based search */
	u8	want_re:1;	/* do a regex based search */
} search;

char**	globdir(char *dir, char *glob);
char	*is_glob(char *glob);
char	*unescape(char *glob);
char*	match_globs(char *string, char **globs, int ignorecase);
int	match_one(char *string, char *glob, int ignorecase);
int	search_either(char *s, search search);
int	search_glob(char *s, search search);
int	search_regex(char *s, search search);
search	search_parse(char *str);

/* mkdir.c */
int	mkdirp(char *dir);
int	test_mkdirp(char *dir);
int	mkdirf(char *file);

/* milli.c */
char *	milli(void);

/* putenv.c */
#define	getenv(s)	safe_getenv(s)
#define	putenv(s)	safe_putenv("%s", s)
void	safe_putenv(char *fmt, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 1, 2)))
#endif
     ;
char	*safe_getenv(char *var);

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
#define	mkdir(d, m)	smartMkdir((char *)d, m)

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
char	*backtick(char *cmd);

/* tcp/tcp.c */
int	tcp_server(char *addr, int port, int quiet);
int	tcp_connect(char *host, int port);
int	tcp_accept(int sock);
void	tcp_ndelay(int sock, int val);
void	tcp_reuse(int sock);
void	tcp_keepalive(int sock);
int	sockport(int s);
char	*sockaddr(int);
char	*hostaddr(char *);
int	tcp_pair(int fds[2]);
char	*peeraddr(int s);
int	issock(int);

/* testcode.c */
void	libc_tests(void);

/* trace.c */
extern	int bk_trace;
void	trace_init(char *prog);
void	trace_msg(char *fmt, char *file, int line, const char *function, ...)
#ifdef __GNUC__
     __attribute__((format (__printf__, 1, 5)))
#endif
	;
void	trace_free(void);

#define	TRACE(format, args...)	\
	do { if (bk_trace) \
		trace_msg(format, __FILE__, __LINE__, __FUNCTION__, ##args); \
	} while (0)

#define	HERE()	TRACE("%s", "")

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

/* webencode.c */
void	webencode(FILE *out, u8 *ptr, int len);
char	*webdecode(char *data, char **buf, int *sizep);

/* which.c */
char	*which(char *prog);

/* zgets.c */
typedef	struct zgetbuf zgetbuf;
typedef	int (*zgets_func)(void *token, u8 **buf);

zgetbuf	*zgets_init(void *start, int len);
zgetbuf	*zgets_initCustom(zgets_func callback, void *token);
char	*zgets(zgetbuf *);
int	zseek(zgetbuf *, int len);
int	zread(zgetbuf *, u8 *buf, int len);
int	zeof(zgetbuf *);
char	*zpeek(zgetbuf *, int len);
int	zgets_done(zgetbuf *);

typedef	struct zputbuf zputbuf;
typedef void (*zputs_func)(void *, u8 *, int);

zputbuf	*zputs_init(zputs_func callback, void *token, int level);
int	zputs(zputbuf *, u8 *data, int len);
int	zputs_done(zputbuf *);


#include "fslayer/fslayer.h"

#endif /* _SYSTEM_H */
