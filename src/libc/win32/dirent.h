/* %K% Copyright (c) 1999 Andrew Chang */
#ifndef	_DIRENT_H_
#define	_DIRENT_H_
/* unix dirent.h simulation, Andrew Chang 1998 */
#include "misc.h"
struct dirent
{
	char d_name[NBUF_SIZE];
};

struct dir_info {
	long	dh;
	char	first[1024];
};

typedef struct dir_info DIR;

extern DIR *opendir(const char *);
extern struct dirent *readdir(DIR *);
extern void closedir(DIR *);
#endif /*_DIRENT_H_ */

