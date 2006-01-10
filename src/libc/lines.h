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
 *
 * ================ Arbitrarily long buffer interfaces =================
 *
 * s = str_append(s, str, gift)
 *	append str in the s lines array
 *	if gift is not set, then autoallocate a copy.
 *	if gift is set, the use of str after this call is prohibited
 *
 * s = str_nappend(s, str, len, gift)
 *	as above, length is passed in.  Use this if possible, it's faster.
 *
 * buf = str_pullup(&len, s)
 *	return a normal C string which contains all the strings
 *	strlen(buf) is in len.
 *	frees the s array as a side effect.
 *
 * s = data_append(s, data, len, gift)
 *	append len bytes of data to the array (not null terminated)
 *
 * data = data_pullup(&len, s)
 *	return the data as one large block, length is in len.
 *	frees the s array as a side effect.
 */
#ifndef	_LIB_LINES_H
#define	_LIB_LINES_H

#define	str_nappend(s, str, len, gift)	data_append(s, str, len, gift)
#define	data_pullup(p, s)		_pullup(p, s, 0)
#define	str_pullup(p, s)		_pullup(p, s, 1)
#define	LSIZ(s)				((int)(long)s[0])
#define	EACH_INDEX(s, i)	for (i=1; (s) && (i < LSIZ(s)) && (s)[i]; i++)
#define	EACH(s)				EACH_INDEX(s, i)
#define	emptyLines(s)			(!s || !s[1])
#define	str_empty(s)			(!s || !s[2])
#define	data_empty(s)			(!s || !s[2])

char	**addLine(char **space, void *line);
char	**allocLines(int n);
int	nLines(char **space);
char	**splitLine(char *line, char *delim, char **tokens);
char	**splitLineToLines(char *line, char **tokens);
char	*joinLines(char *sep, char **space);
void	*popLine(char **space);
void	freeLines(char **space, void(*freep)(void *ptr));
int	removeLine(char **space, char *s, void(*freep)(void *ptr));
void	removeLineN(char **space, int rm, void(*freep)(void *ptr));
void	reverseLines(char **space);
void	sortLines(char **space, int (*compar)(const void *, const void *));
int	string_sort(const void *a, const void *b);
char	**shellSplit(const char *line);

char	**str_append(char **space, void *str, int gift);
char	**data_append(char **space, void *str, int len, int gift);
char	*_pullup(u32 *bytep, char **space, int null);
#endif
