/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_RE_MAP_DECL_H_
#define	_RE_MAP_DECL_H_

/* file name translation functions */
#define nt2bmfname(ofn, nfn) _switch_char(ofn, nfn, '\\', '/')
#define bm2ntfname(ofn, nfn) _switch_char(ofn, nfn, '/', '\\')
char*	_switch_char(const char *, char [], char, char);

/* functions that expect file name as input */
FILE*	nt_fopen(const char *, const char *);
int	nt_open(const char *, int, int);
int	nt_close(int);
int	nt_unlink(const char *);
int	nt_rename(const char *, const char *);
int	nt_access(const char *, int);
int	nt_chmod(const char *, int);
int	nt_stat(const char *, struct stat *);
int	nt_mkdir(char *dirname);
int	nt_chdir(char *dirname);
int	nt_utime(char *file, struct utimbuf *ut);
pid_t	nt_execvp (char *cmd, char **av);
int	nt_rmdir(char *dirname);

/* functions that return file name as output */
char*	nt_tmpnam(void);
char*	nt_getcwd(char *, int); 

/* function that does not need file name as input or output */
struct tm* nt_localtime(const time_t);
int	nt_dup(int);
int	nt_dup2(int, int);

/* socket functions */
int	nt_socket(int af, int type, int protocol);
int	nt_accept(int s, struct sockaddr *addr, int *addrlen);
int	nt_bind(int s, const struct sockaddr *addr, int addrlen);
int	nt_connect(int s, const struct sockaddr *addr, int addrlen);
int	nt_send(int s, const char *buf, int len, int flags);
int	nt_recv(int s, char *buf, int len, int flags);
int	nt_listen(int s, int backlog);
int	nt_getpeername(int s, struct sockaddr *addr, int *addlen);
int	nt_getsockname(int s, struct sockaddr *addr, int *addrlen);
int	nt_setsockopt(int s, int l, int oname, const char *oval, int olen);
int	nt_closesocket(int s);
int	nt_select(int n, fd_set *rfds, fd_set *wfds, fd_set *efds,
		struct timeval *t);
int	nt_shutdown(int s, int how);

#endif /* _RE_MAP_DECL_H_ */

