/*
 * Copyright 2003,2016 BitMover, Inc
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

/*
 * Note: This file is also used by src/win32/pub/diffutils
 *       Do not put BitKeeper specific code here
 */
#include "vss2bk.h"
#include "liblines.h"

#ifdef	TEST_LINES
#undef	malloc
#undef	calloc
#endif

#define	LINES_INVALID	((char **)-1)
static	char	**addLine_lastp = LINES_INVALID;
static	int	addLine_lasti;

/*
 * pre allocate line space.
 */
char	**
allocLines(int n)
{
	char	**space;

	assert(n > 1);
	space = calloc(n, sizeof(char *));
	assert(space);
	space[0] = int2p(n);
	return (space);
}

/*
 * Save a line in an array.  If the array is out of space, reallocate it.
 * The size of the array is in array[0].
 * This is OK on 64 bit platforms.
 */
char	**
addLine(char **space, void *line)
{
	int	i;

	unless (line) return (space);
	unless (space) {
		space = allocLines(32);
	} else if (space[LSIZ(space)-1]) {	/* full up, dude */
		int	size = LSIZ(space);
		char	**tmp = allocLines(size*4);

		assert(tmp);
		bcopy(space, tmp, size*sizeof(char*));
		tmp[0] = (char *)(long)(size * 4);
		free(space);
		space = tmp;
	}
	if (addLine_lastp == space) {
		i = ++addLine_lasti;	/* big perf win */
	} else {
		EACH(space); 		/* I want to get to the end */
		addLine_lastp = space;
		addLine_lasti = i;
	}
	assert(i < LSIZ(space));
	assert(space[i] == 0);
	space[i] = line;
	return (space);
}


void
freeLines(char **space, void(*freep)(void *ptr))
{
	int	i;

	if (!space) return;
	if (freep) {
		EACH(space) freep(space[i]);
	}
	space[0] = 0;
	free(space);
	addLine_lastp = LINES_INVALID;
}
