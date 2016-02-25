/*
 * Copyright 1999-2000,2006-2007,2016 BitMover, Inc
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
