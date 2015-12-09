#ifndef	_MMAP_H
#define	_MMAP_H

/* %K% */

/*
 * Diffs are passed in mmap-ed chunks as described below.
 */
typedef	struct mmap {
	char	*mmap;			/* diffs start here */
	size_t	size;			/* go for this much */
	char	*where;			/* we are here in mapping */
	char	*end;			/* == map + len */
	int	flags;
	int	fd;			/* fd associated with mmap */
} MMAP;
#define	MMAP_OURS	0x01
#define MMAP_BIN_MODE	0x02		/* binary mode read, for win32 */

MMAP	*mopen(char *file, char *mode);
void	mclose(MMAP *);
char	*mnext(MMAP *);
int	mpeekc(MMAP *);
void	mseekto(MMAP *m, off_t off);
off_t	mtell(MMAP *m);
size_t	msize(MMAP *m);
MMAP	*mrange(char *start, char *stop, char *mode);

#endif
