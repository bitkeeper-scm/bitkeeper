/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_BK_STAT_H_
#define	_BK_STAT_H_

#define	S_IFLNK 0120000  /* nt does not have sym link, but we need this for portability */
#define	S_ISLNK(st_mode) ((st_mode & S_IFMT) == S_IFLNK)

#define S_IRGRP 0040
#define S_IROTH 0004

#define S_IWGRP 0020
#define S_IWOTH 0002

#define	S_IXGRP 0010
#define	S_IXOTH 0001


#endif /* _BK_STAT_H_ */

