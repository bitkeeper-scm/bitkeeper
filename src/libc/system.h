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

#define BIG_PIPE 4096	/* 16K pipe buffer for win32, ingored on unix */
#define	GOOD_PSIZE	(16<<10)

#define	isDriveColonPath(p)	(isalpha((p)[0]) && ((p)[1] == ':'))
#define	executable(p)	((access(p, X_OK) == 0) && !isdir(p))

/* aprintf.c */
char	*aprintf(char *fmt, ...);
void	ttyprintf(char *fmt, ...);

/* cleanpath.c */
char	*basenm(char *s);
void	cleanPath(char *path, char cleanPath[]);

/* concat_path.c */
void	concat_path(char *buf, char *first, char *second);

/* dirname.c */
char	*dirname(char *path);
char	*dirname_alloc(char *path);

/* dirs.c */
char	**getdir(char *);
typedef	int	(*walkfn)(char *file, struct stat *statbuf, void *data);
int	walkdir(char *dir, walkfn fn, void *data);

/* filecopy.c */
int	fileCopy(char *from, char *to);

/* fileinfo.c */
time_t	mtime(char *s);
int	exists(char *s);
int	isdir(char *s);
int	isdir_follow(char *s);
int	isreg(char *s);
int	isSymlnk(char *s);
int	hardlinked(char *a, char *b);
int	writable(char *s);
off_t	fsize(int fd);
off_t	size(char *s);
int	onelink(char *s);

/* fileutils.c */
int	cat(char *file);
char	*loadfile(char *file, int *size);
int	touch(char *file, int mode);

/* findpid.c */
pid_t	findpid(pid_t pid);

/* fullname.c */
char    *fullname(char *path);

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
int	getopt(int ac, char **av, char *opts);
void	getoptReset(void);

/* mkdir.c */
int	mkdirp(char *dir);
int	test_mkdirp(char *dir);
int	mkdirf(char *file);

/* putenv.c */
#define	putenv(s)	safe_putenv("%s", s)
void	safe_putenv(char *fmt, ...);

/* rlimit.c */
void	core(void);

/* rmtree.c */
int	rmtree(char *dir);

/* samepath.c */
int	samepath(char *a, char *b);
int	patheq(char *a, char *b);

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
int	smartMkdir(char *pathname, mode_t mode);

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

/* stdioext.c */
char	*gets_alloc(char *(*fcn)(char *buf, int size, void *arg), void *arg);

/* sys.c */
#define	SYS	(char*)0, 0xdeadbeef	/* this must be the last argument to
					 * all calls to sys/sysio */
int	sys(char *first, ...);
int	sysio(char *in, char *out, char *err, char *first, ...);
void	syserr(const char *postfix);

/* system.c */
#define	system(cmd)	safe_system(cmd)
#define	popen(f, m)	safe_popen(f, m)
#define	pclose(f)	safe_pclose(f)

int	safe_system(char *cmd);
FILE *	safe_popen(char *cmd, char *type);
FILE *	popenvp(char *av[], char *type);
int	safe_pclose(FILE *f);

/* tcp/tcp.c */
int	tcp_server(int port, int quiet);
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

/* tty.c */
#define	isatty		myisatty
int	tty_init(void);
void	tty_done(void);
int	tty_rows(void);
int	tty_cols(void);
int	tty_getchar(void);
void	tty_clear(void);
int	myisatty(int fd);

/* utils.c */
FILE	*efopen(char *env);
void	my_perror(char *, int, char *);
#define	perror(msg)	my_perror(__FILE__, __LINE__, msg)
int	chomp(char *str);

/* webencode.c */
char	**webencode(char **buf, u8 *ptr, int len);
char	*webdecode(char *data, char **buf, int *sizep);

/* which.c */
char	*which(char *prog);

#endif /* _SYSTEM_H */
