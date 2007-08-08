#include "system.h"


#undef	mopen
#undef	mclose

int
mpeekc(MMAP *m)
{
	assert(m);
	if (m->where >= m->end) return (EOF);
	return (*m->where);
}

size_t
msize(MMAP *m)
{
	return (m->size);
}

MMAP	*
mopen(char *file, char *mode)
{
	MMAP	*m;
	int	fd;
	struct	stat st;
	int	oflags = O_RDONLY;
	int	mprot = PROT_READ;

	if (*mode == 'w') {
		oflags = O_RDWR;
		mprot |= PROT_WRITE;
	}
	unless ((fd = open(file, oflags, 0)) >= 0) {
		perror(file);
		return (0);
	}

	unless (fstat(fd, &st) == 0) {
		perror(file);
		close(fd);
		return (0);
	}
	m = calloc(1, sizeof(*m));
	if (strchr(mode, 'b')) m->flags |= MMAP_BIN_MODE;
	/*
	 * Allow zero sized mappings,
	 * and force !regular files to zero sized.
	 */
	unless (S_ISREG(st.st_mode) && (m->size = st.st_size)) {
		m->fd = -1;
		close(fd);
		return (m);
	}
	m->mmap = mmap(0, m->size, mprot, MAP_SHARED, fd, 0);

#if     defined(hpux)
	if (m->mmap == (caddr_t)-1) {
                /*
                 * HP-UX won't let you have two shared mmap to the same file.
                 */
		m->mmap = mmap(0, m->size, mprot, MAP_PRIVATE, fd, 0);
        }
#endif

	if (m->mmap == (caddr_t)-1) {
		perror(file);
		free(m);
		close(fd);
		return (0);
	}
	m->flags |= MMAP_OURS;
	m->where = m->mmap;
	m->end = m->mmap + m->size;
	/* limitation in current win32 mmap implementation:  */
	/* cannot close fd when file is mmaped		     */	
	/* we close the fd when we mclose()		     */
	m->fd = fd;
	return (m);
}

/*
 * Map somebody else's data.
 */
MMAP	*
mrange(char *start, char *stop, char *mode)
{
	MMAP	*m = calloc(1, sizeof(*m));

	if (*mode == 'b') m->flags |= MMAP_BIN_MODE;
	if (start == stop) return (m);
	m->where = m->mmap = start;
	m->end = stop;
	m->size = stop - start;
	m->fd = -1;
	return (m);
}

void
mclose(MMAP *m)
{
	unless (m) return;
	if ((m->flags & MMAP_OURS) && m->mmap) munmap(m->mmap, m->size);
	if ((m->flags & MMAP_OURS) && (m->fd != -1)) close(m->fd);
	free(m);
}

void
mseekto(MMAP *m, off_t off)
{
	assert(m);
	m->where = m->mmap + off;
}

off_t
mtell(MMAP *m)
{
	assert(m);
	return (off_t)(m->where - m->mmap);
}

/* Compare lines from the gfile and the sfile.
 * The sfile_line points into an mmap as does the gf.
 * We allow either line to be \r+\n terminated even though only the
 * gfile is likely to be.  This is in case the sequence somehow got
 * into the file.
 */
int
mcmp(MMAP *gf, char *sf)
{
	register char	*g, *s, *t;

	assert(gf);

	if (gf->where >= gf->end) {
		unless (sf && *sf) return (MCMP_BOTH_EOF);
		return (MCMP_GFILE_EOF);
	}
	unless (sf && *sf) return (MCMP_SFILE_EOF);

	for (s = sf, g = gf->where; g < gf->end; g++, s++) {
		assert(*g && *s);
		if (*g != *s) break;
		if (*g == '\n') break;
	}
	for (t = g; (t < gf->end) && (*t == '\r'); t++);
	if ((t < gf->end) && (*t == '\n')) g = t;
	for (t = s; *t == '\r'; t++);
	if (*t == '\n') s = t;
	if (g >= gf->end) {
		if (*s != '\n') return (MCMP_DIFF);
		gf->where = g;
		return (MCMP_NOLF);
	}
	if ((*s != '\n') || (*g != '\n')) return (MCMP_DIFF);
	gf->where = g + 1;
	return (MCMP_MATCH);
}
