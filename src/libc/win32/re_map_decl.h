/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_RE_MAP_DECL_H_
#define	_RE_MAP_DECL_H_

/* file name translation functions */
#define nt2bmfname(ofn, nfn) _switch_char(ofn, nfn, '\\', '/')
#define bm2ntfname(ofn, nfn) _switch_char(ofn, nfn, '/', '\\')
char*	_switch_char(const char *, char [], char, char);

/* functions that expect file name as input */
int	nt_open(const char *, int, int);
int	nt_close(int);
int	nt_unlink(const char *);
int	nt_rename(const char *, const char *);
int	nt_access(const char *, int);
int	nt_chmod(const char *, int);
int	nt_stat(const char *, struct stat *);
int	nt_linkcount(const char *, struct stat *);
int	nt_mkdir(char *dirname);
int	nt_chdir(char *dirname);
int	nt_utime(const char *file, const struct utimbuf *ut);
pid_t	nt_execvp (char *cmd, char **av);
int	nt_rmdir(const char *dirname);
int	nt_link(const char *file1, const char *file2);
void	nt_sync(void);

/* functions that return file name as output */
char*	nt_tmpnam(void);
char*	nt_getcwd(char *, int); 

/* function that does not need file name as input or output */
struct tm* nt_localtime(const time_t);
int	nt_dup(int);
int	nt_dup2(int, int);

#endif /* _RE_MAP_DECL_H_ */

