/*
 * Copyright 1999-2001,2005-2009,2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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
	unless ((fd = open(file, oflags, 0)) >= 0) return (0);

	unless (fstat(fd, &st) == 0) {
		perror(file);
		close(fd);
		return (0);
	}
	m = new(MMAP);
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
	MMAP	*m = new(MMAP);

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
