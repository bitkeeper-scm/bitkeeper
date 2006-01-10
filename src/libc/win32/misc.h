/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_MISC_H_
#define	_MISC_H_
#include <stdio.h>

#define private static
#define NBUF_SIZE 1024
#ifdef	DEBUG
#	define	debug(x)	fprintf x
#else
#	define	debug(x)
#endif

#define	SIGKILL	-999 /* not supported */
#define	SIGPIPE	-999 /* not supported */
#define	SIGQUIT	-999 /* not supported */
#define	SIGCHLD -999 /* not supported */
/*
 * Win32 does not have SIGALRM, so we are "stealing" SIGABRT as SIGALRM
 */
#define	SIGALRM	SIGABRT 

extern void make_fd_inheritable(int);
extern void make_fd_uninheritable(int);
extern char *getlogin(void);

const char *nt_tmpdir(void);
extern int  nt_is_full_path_name(char *);
extern int ftruncate(int, size_t);
extern int getdomainname(char *buf, size_t len);
extern pid_t _spawnvp_ex(int flag, const char *cmdname, char *const av[], int fix_quote);
extern int mkstemp(char *template1);
extern void nt_loadWinSock(void);
extern int pipe(int fd[2], int pipe_size);
extern void nt_sleep(int i);
extern int fileBusy(char *fname);
extern int _isExecutable(char *f);
extern int kill(pid_t, int);
extern void usleep(unsigned long);
extern int sigcaught(int);
extern int hasConsole(void);

int	setShutDownPrivilege(void);
int	do_reboot(char *msg);
int	win_unsupported(void);
int	isWin2000(void);
int	alarm(int seconds);
int	link(char *from, char *to);
int	fsync (int fd);

#endif /* _MISC_H_ */

