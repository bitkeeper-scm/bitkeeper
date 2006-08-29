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

/* return values for mcmp() */

#define MCMP_ERROR	0
#define MCMP_MATCH	1
#define MCMP_NOLF	2
#define MCMP_DIFF	3
#define MCMP_BOTH_EOF	4
#define MCMP_SFILE_EOF	5
#define MCMP_GFILE_EOF	6

#ifdef	PURIFY_FILES
MMAP	*purify_mopen(char *file, char *mode, char *, int);
void	purify_mclose(MMAP *, char *, int);
#else
MMAP	*mopen(char *file, char *mode); void	mclose(MMAP *);
#endif
char	*mnext(MMAP *);
int	mcmp(MMAP *, char *);
int	mpeekc(MMAP *);
void	mseekto(MMAP *m, off_t off);
off_t	mtell(MMAP *m);
size_t	msize(MMAP *m);
MMAP	*mrange(char *start, char *stop, char *mode);

#endif
