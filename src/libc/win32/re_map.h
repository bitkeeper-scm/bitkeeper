/* %K%
 * Copyright (c) 1999 Andrew Chang 
 */
#ifndef	_RE_MAP_H_
#define	_RE_MAP_H_
#undef unlink
#undef rename

/* functions that expect file name as input */ 
#undef	fopen
#define fopen(n, f)		nt_fopen(n, f)
#define open(x,f,p)		nt_open(x, f, p)
#define unlink(f)		nt_unlink(f)
#define rename(oldf, newf)	nt_rename(oldf, newf)
#define access(f, m)		nt_access(f, m)
#define chmod(f, m)		nt_chmod(f, m)
#define stat(f, b)              nt_stat(f, b)
#define	link(f1, f2)		nt_link(f1, f2)
#define lstat(f, b)		nt_stat(f, b)
#define	utime(a, b)		nt_utime(a, b)
#define symlink(a, b)		(-1) /* always return fail */
#define	readlink(a, b, c)	(-1) /* always return fail */
#define	rmdir(d)		nt_rmdir(d)

/* functions that return file name as output */
#define tmpnam(x)		nt_tmpnam(x)
#define getcwd(b, l)		nt_getcwd(b, l)

/* functions that does not need file name translation */
#define	dup(fd)			nt_dup(fd)
#define	dup2(fd1, fd2)		nt_dup2(fd1, fd2)
#define sleep(x)		nt_sleep(x)
#define execvp(a, b)		nt_execvp(a, b)

#define sync()			nt_sync()

#endif /* _RE_MAP_H_ */


