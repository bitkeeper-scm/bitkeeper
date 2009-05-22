/* %K%
 * Copyright (c) 1999 Andrew Chang 
 */
#ifndef	_RE_MAP_H_
#define	_RE_MAP_H_

/* Most of this moved to fslayer.c */
#define symlink(a, b)		(-1) /* always return fail */
#define	readlink(a, b, c)	(-1) /* always return fail */

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
