/*
 * Copyright 2002-2007,2009-2016 BitMover, Inc
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
 * liblines - interfaces for autoexpanding data structures
 *
 * s= allocLines(n)
 *	pre allocate space for slightly less than N entries.
 * s = addLine(s, line)
 *	add line to s, allocating as needed.
 *	line must be a pointer to preallocated space.
 * freeLines(s, freep)
 *	free the lines array; if freep is set, call that on each entry.
 *	if freep is 0, do not free each entry.
 * buf = popLine(s)
 *	return the most recently added line (not an alloced copy of it)
 * reverseLines(s)
 *	reverse the order of the lines in the array
 * sortLines(space, compar)
 *	sort the lines using the compar function if set, else string_sort()
 * removeLine(s, which, freep)
 *	look for all lines which match "which" and remove them from the array
 *	returns number of matches found
 * removeLineN(s, i, freep)
 *	remove the 'i'th line.
 * lines = splitLine(buf, delim, lines)
 *	split buf on any/all chars in delim and put the tokens in lines.
 * buf = joinLines(":", s)
 *	return one string which is all the strings glued together with ":"
 *	does not free s, caller must free s.
 * buf = findLine(lines, needle);
 *	Return the index the line in lines that matches needle
 */
#ifndef	_LIB_LINES_H
#define	_LIB_LINES_H

/* lines are limited to 2^27 entries ~134 million */
#define	LBITS				(32 - 5)
#define	LMASK				0x07ffffff

/* sub-macro for EACH */
/* length of array (use nLines() in code) */
#define	_LLEN(s)			(*(u32 *)(s) & LMASK)

#define L(d) ({							\
	typeof(d) _d = d;						\
	(_d) ? (typeof(_d) []){(typeof(_d))1, (typeof(_d))_d} : 0;	\
})

#define	EACH_START(x, s, i)				\
	if ((i = (x)), (s)) for (; (i) <= _LLEN(s); i++)
#define	EACH_INDEX(s, i)		EACH_START(1, s, i)
#define	EACH(s)				EACH_START(1, s, i)
#define	EACH_REVERSE_INDEX(s, i)	for (i = nLines(s); i > 0; i--)
#define	EACH_REVERSE(s)			EACH_REVERSE_INDEX(s, i)

/* EACHP like EACH but walks a pointer to each array element instead of i */
#define	EACHP(s, ptr)							\
	if (s) for ((ptr) = (s)+1; (ptr) <= (s)+_LLEN(s); ++(ptr))
#define	EACHP_REVERSE(s, ptr)						\
	if (s) for ((ptr) = (s)+_LLEN(s); (ptr) > (s); --(ptr))

#define	EACH_STRUCT(s, c, i)				\
	if (((c) = 0), (i = 1), (s))			\
		for (((c) = (void *)(s)[i]);		\
		     ((i) <= _LLEN(s)) && ((c) = (void *)(s)[i]);	\
		     ++(i))

#define	LNEXT(s)					\
	((s) && ((i) <= _LLEN(s)) ? (s)[i++] : 0)


/* return number of lines in array */
#define	nLines(s)			((s) ? _LLEN(s) : 0)
#define	emptyLines(s)			(nLines(s) == 0)

char	**addLine(char **space, void *line);
char	**allocLines(int n);
#define	truncLines(s, len)	truncArray(s, len)
#define	insertLineN(space, n, val) \
	({ void	*str = (val);			\
	   char **_a = (space);			\
	   _insertArrayN((void **)&_a, n, &str, sizeof(char *));	\
	   _a; })
void	*removeLineN(char **space, int rm, void(*freep)(void *ptr));
#define	catLines(space, array)	\
	({ char **_a = (space); \
	   _catArray((void **)&_a, (array), sizeof(char *));	\
	   _a; })

#define	pushLine(s, l)		addLine(s, l)
#define	popLine(s)		removeLineN(s, nLines(s), 0)
#define	shiftLine(s)		removeLineN(s, 1, 0)
#define	unshiftLine(s, val)	insertLineN(s, 1, val)

char	**splitLine(char *line, char *delim, char **tokens);
char	*joinLines(char *sep, char **space);
void	freeLines(char **space, void(*freep)(void *ptr));
int	removeLine(char **space, char *s, void(*freep)(void *ptr));
void	reverseLines(char **space);
#define	sortLines(s, compar)	_sortArray(s, compar, sizeof(char *))
int	string_sort(const void *a, const void *b);
int	stringcase_sort(const void *a, const void *b);
int	string_sortrev(const void *a, const void *b);
int	number_sort(const void *a, const void *b);
char	**shellSplit(const char *line);
char	**prog2Lines(char **space, char *file);
char	**file2Lines(char **space, char *file);
int	lines2File(char **space, char *file);
void	uniqLines(char **space, void(*freep)(void *ptr));
int	sameLines(char **p, char **p2);
char	*shellquote(char *in);
int	findLine(char **haystack, char *needle);

int	parallelLines(char **a, char **b,
    int (*compar)(const void *, const void *),
    int (*walk)(void *token, char *a, char *b),
    void *token);

/* arrays of arbitrary sized data */

/* TYPE *growArray(TYPE **space, int n) */
#define	growArray(space, n)					\
	({ typeof(*space) _ret;					\
	   _ret = _growArray((void **)space, n, sizeof(*_ret));	\
	   _ret; })

/* TYPE *addArray(TYPE **space, TYPE *new) */
#define	addArray(space, x)				\
	({ typeof(*space) _arg = (x), _ret;			\
	   _ret = _addArray((void **)space, _arg, sizeof(*_ret));	\
	   _ret; })

/* TYPE *insertArrayN(TYPE **space, int n, TYPE *x) */
#define	insertArrayN(space, n, x)			\
	({ typeof(*space) _arg = (x), _ret;			\
	   _ret = _insertArrayN((void **)space, n, _arg,  sizeof(*_ret)); \
	   _ret; })

/* void removeArrayN(TYPE *space, int n); */
#define	removeArrayN(s, n)	_removeArrayN((s), (n), sizeof((s)[0]))

/* void catArray(TYPE **space, TYPE *array) */
#define	catArray(s, a)		_catArray((void **)(s), (a), sizeof((*(s))[0]))

/* void reverseArray(TYPE *space); */
#define	reverseArray(s)		_reverseArray((s), sizeof((s)[0]))

/* void sortArray(TYPE *space, cmpfn); */
#define	sortArray(s, compar)	_sortArray((s), (compar), sizeof((s)[0]))

/* TYPE popArray(TYPE *space); */
#define	popArray(space)				\
	({ int i = nLines(space);		\
	   typeof(*space) _ret;			\
	   if (i) {				\
		   _ret = space[i];		\
		   removeArrayN(space, i);	\
	   } else {				\
		   memset(&_ret, 0, sizeof(_ret)); \
	   }					\
	   _ret; })

void	*_growArray(void **space, int add, int size);
void	*_addArray(void **space, void *x, int size);
void	truncArray(void *space, int len);
void	*_insertArrayN(void **space, int j, void *line, int size);
void	_removeArrayN(void *space, int rm, int size);
void	*_catArray(void **space, void *array, int size);
void	_reverseArray(void *space, int size);
void	_sortArray(void *space,
    int (*compar)(const void *, const void *), int size);

void	*allocArray(int cnt, int size, void *(*allocate)(size_t len));

void	lines_tests(void);

#endif
