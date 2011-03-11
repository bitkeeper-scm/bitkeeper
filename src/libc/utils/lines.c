/*
 * Note: This file is also used by src/win32/pub/diffutils
 *       Do not put BitKeeper specific code here
 */
#include "system.h"
#include "lines.h"

#ifdef	TEST_LINES
#undef	malloc
#undef	calloc
#endif

/* length of array (use nLines() in code) */
#define	LLEN(s)				(*(u32 *)(s) & LMASK)
#define	setLLEN(s, len)	(*(u32 *)(s) = (*(u32 *)(s) & ~LMASK) | len)

/* size of array (saves LSIZ-1 items) */
#define	LSIZ(s)				(1u << (*(u32 *)(s) >> LBITS))

/*
 * add 'add' more elements to an array, resizing as needed
 * internal version that doesn't clear memory
 */
private void	*
_growArray_int(void *space, int add, int elemsize)
{
	u32	size, len;	/* len and size of array */
	int	c;
	void	*tmp;

	if (space) {
		len = LLEN(space) + add;
		c = (*(u32 *)space >> LBITS);
		size = 1u << c;

		assert(size > 0);	 /* size==0 is read-only array */
	} else {
		assert(elemsize >= sizeof(u32));
		len = add;
		c = (elemsize > 128) ? 2 : 3; /* min alloc size */
		size = 1u << c;
		goto alloc;
	}
	if (len >= size) {	/* full up, dude */
alloc:		while (len >= size) {
			size *= 2;
			++c;
		}
		tmp = malloc(size * elemsize);
		assert(tmp);
		if (space) {
			memcpy((u8 *)tmp + elemsize,
			    (u8 *)space + elemsize,
			    (len - add) * elemsize);
			free(space);
		}
		space = tmp;
	}
	*(u32 *)space = (c << LBITS) | len;
	return (space);
}

/*
 * add 'add' more elements to an array, resizing as needed
 * new elements are set to zero
 * returns: a pointer to the new elements
 */
void	*
_growArray(void **space, int add, int size)
{
	void	*ret;

	*space = _growArray_int(*space, add, size);
	ret = *(u8 **)space + (LLEN(*space)-add+1)*size;
	memset(ret, 0, add * size);
	return (ret);
}

/*
 * Return a new array of length 0 with space for n lines.
 */
char	**
allocLines(int	n)
{
	char	**space = _growArray_int(0, n, sizeof(char *));

	setLLEN(space, 0);
	return (space);
}

/*
 * Add a char* to the end of an array, if line==0 then the array is
 * zero terminated, but the length doesn't actually change.
 */
char	**
addLine(char **space, void *line)
{
	int	len;

	space = _growArray_int(space, 1, sizeof(char *));
	len = nLines(space);
	space[len] = line;
	unless (line) setLLEN(space, len-1);
	return (space);
}

/*
 * Adds 1 new element to array and copies 'x' into, if
 * x==0 then the new item is cleared.
 * returns a pointer to the new item.
 */
void   *
_addArray(void **space, void *x, int size)
{
	void	*ret;
	int	len;

	*space = _growArray_int(*space, 1, size);
	len = nLines(*space);
	ret = (u8 *)(*space) + len * size;
	if (x) {
		memcpy(ret, x, size);
	} else {
		memset(ret, 0, size);
	}
	return (ret);
}

/*
 * set the length of a lines array to a new (smaller) value.
 */
void
truncArray(void *space, int len)
{
	if (space) {
		/* no growing the array at the moment */
		assert(len <= LLEN(space));
		setLLEN(space, len);
	} else {
		assert(len == 0);
	}
}

/*
 * copy array to the end of space and then zero array
 */
void	*
_catArray(void **space, void *array, int size)
{
	int	n1, n2;
	void	*ret = 0;

	if (n2 = nLines(array)) {
		n1 = nLines(*space);
		*space = _growArray_int(*space, n2, size);
		ret = (u8 *)*space+(n1+1)*size;
		memcpy(ret, (u8 *)array+size, n2*size);
		setLLEN(array, 0);
	}
	return (ret);
}

void
reverseLines(char **space)
{
	int	i, end;
	char	*tmp;

	end = nLines(space);
	i = 1;
	while (i < end) {
		tmp = space[i];
		space[i] = space[end];
		space[end] = tmp;
		++i;
		--end;
	}
}

void
_sortArray(void *space, int (*compar)(const void *, const void *), int size)
{
	if (!space) return;
	unless (compar) compar = string_sort;
	qsort((u8 *)space+size, nLines(space), size, compar);
}

/*
 * Looking for a key_sort?  It's in check.c and sorts on the key starting
 * at the pathname part.
 */

int
string_sort(const void *a, const void *b)
{
	char	*l, *r;

	l = *(char**)a;
	r = *(char**)b;
	return (strcmp(l, r));
}

int
string_sortrev(const void *a, const void *b)
{
	char	*l, *r;

	l = *(char**)a;
	r = *(char**)b;
	return (strcmp(r, l));	/* reverse sort */
}

int
number_sort(const void *a, const void *b)
{
	int	l, r;

	l = atoi(*(char**)a);
	r = atoi(*(char**)b);
	if (l - r) return (l - r);
	return (string_sort(a, b));
}

void
uniqLines(char **space, void(*freep)(void *ptr))
{
	int	src, dst;

	unless (space) return;
	sortLines(space, 0);

	/* skip up to the first dup */
	EACH_INDEX(space, src) {
		if ((src > 1) && streq(space[src-1], space[src])) goto dups;
	}
	return;			/* fast exit */

 dups:	/* now copy non-duped items */
	dst = src-1;		/* last valid item in output */
	for (; src <= LLEN(space); src++) { /* EACH() */
		if (streq(space[src], space[dst])) {
			if (freep) freep(space[src]);
		} else {
			space[++dst] = space[src];
		}
	}
	truncLines(space, dst);
}

/*
 * Return true if they are the same.
 * It's up to you to sort them first if you want them sorted.
 */
int
sameLines(char **p, char **p2)
{
	int	i;

	unless (p && p2) return (0);
	unless (nLines(p) == nLines(p2)) return (0);
	EACH(p) unless (streq(p[i], p2[i])) return (0);
	return (1);
}

void
freeLines(char **space, void(*freep)(void *ptr))
{
	int	i;

	if (!space || (space == INVALID)) return;
	if (freep) {
		EACH(space) freep(space[i]);
	}
	space[0] = 0;
	free(space);
}

/* same non O(n^2) idiom as uniqLines() */
int
removeLine(char **space, char *s, void(*freep)(void *ptr))
{
	int	src, dst, n = 0;

	/* skip up to the first match */
	EACH_INDEX(space, src) {
		if (streq(space[src], s)) goto match;
	}
	return (0);			/* fast exit */

 match:	/* now copy non-matched items */
	dst = src-1;		/* last non-matched item in output */
	for (; src <= LLEN(space); src++) { /* EACH() */
		if (streq(space[src], s)) {
			n++;
			if (freep) freep(space[src]);
		} else {
			space[++dst] = space[src];
		}
	}
	truncLines(space, dst);
	return (n);
}

/*
 * set space[j] = line
 * and shift everything down to make room
 * return ptr to new item
 */
void	*
_insertArrayN(void **space, int j, void *new, int size)
{
	int	len;
	void	*ret;

	len = nLines(*space) + 1;
	assert((j > 0) && (j <= len));
	/* alloc spot and inc line count */
	*space = _growArray_int(*space, 1, size);
	ret = (u8 *)*space+j*size;
	if (j < len) memmove((u8 *)ret+size, ret, (len-j)*size);
	if (new) {
		memcpy(ret, new, size);
	} else {
		memset(ret, 0, size);
	}
	return (ret);
}

void
_removeArrayN(void *space, int rm, int size)
{
	int	len = LLEN(space);

	assert(rm <= len);
	assert(rm > 0);
	if (rm < len) {
		memmove((u8 *)space+rm*size,
		    (u8 *)space+(rm+1)*size,
		    (len - rm)*size);
	}
	setLLEN(space, len-1);
}

/*
 * A simple wrapper for removeArray() to use for an array of pointers.
 * If freep is passed, then we free the pointer being removed
 * and return zero.  Otherwise we return the pointer removed
 * from the array.
 */
void *
removeLineN(char **space, int rm, void(*freep)(void *ptr))
{
	char	*ret;

	if (freep) {
		freep(space[rm]);
		ret = 0;
	} else {
		if ((rm < 1) || (rm > nLines(space))) return (0);
		ret = space[rm];
	}
	_removeArrayN(space, rm, sizeof(void *));
	return (ret);
}

/*
 * Fill a lines array from output from a program.
 * Each line is chomp()ed.
 */
char	**
prog2Lines(char **space, char *cmdline)
{
	FILE	*f;
	char	*p;

	unless (cmdline && (f = popen(cmdline, "r"))) return (space);
	while (p = fgetline(f)) {
		space = addLine(space, strdup(p));
	}
	pclose(f);
	return (space);
}

/*
 * Fill a lines array from a file.
 * Each line is chomp()ed.
 */
char	**
file2Lines(char **space, char *file)
{
	FILE	*f;
	char	*p;

	unless (file && (f = fopen(file, "r"))) return (space);
	while (p = fgetline(f)) {
		space = addLine(space, strdup(p));
	}
	fclose(f);
	return (space);
}

/*
 * Fill a file from a lines array.
 */
int
lines2File(char **space, char *file)
{
	FILE	*f;
	int	i;
	struct	stat sbuf;

	unless (file) return (-1);
	unless (f = fopen(file, "w")) {
		if (stat(file, &sbuf)) return (-1);
		sbuf.st_mode |= 0664;
		chmod(file, sbuf.st_mode);
		f = fopen(file, "w");
	}
	unless (f) return (-1);
	EACH(space) {
		fprintf(f, "%s\n", space[i]);
	}
	if (fclose(f)) return (-1);
	return (0);
}

/*
 * Like perl's join(),
 * use it for making arbitrary length strings.
 */
char	*
joinLines(char *sep, char **space)
{
	int	i, slen, len = 0;
	char	*buf, *p;

	if (emptyLines(space)) return (0);
	slen = sep ? strlen(sep) : 0;
	EACH(space) {
		len += strlen(space[i]);
		len += slen;
	}
	len++;
	buf = malloc(len);
	p = buf;
	EACH(space) {
		strcpy(p, space[i]);
		p += strlen(space[i]);
		if (sep) {
			strcpy(p, sep);
			p += slen;
		}
	}
	/*
	 * No trailing sep.
	 */
	if (sep) {
		p -= slen;
		*p = 0;
	}
	return (buf);
}

/*
 * Split a C string into tokens like strtok()
 *
 * The string 'line' is seperated into tokens seperated
 * by one of more characters from 'delim' and each token will
 * be added to the 'tokens' line array.
 * The tokens will be null terminated and will not contain characters
 * from 'delim'
 */
char   **
splitLine(char *line, char *delim, char **tokens)
{
	int     len;

	while (1) {
		line += strspn(line, delim); /* skip delimiters */
		len = strcspn(line, delim);
		unless (len) break;
		tokens = addLine(tokens, strndup(line, len));
		line += len;
	}
	return (tokens);
}

/*
 * Split a C string for a block of text into a list of lines not containing
 * the trailing newlines.  Blank lines are not stripped.
 */
char **
splitLineToLines(char *line, char **tokens)
{
	int	len;
	char	*p;

	while (line) {
		if (p = strchr(line, '\n')) {
			len = p++ - line;
			unless (*p) p = 0;
			while (len > 0 && line[len-1] == '\r') --len;
		} else {
			len = strlen(line);
		}
		tokens = addLine(tokens, strndup(line, len));
		line = p;
	}
	return (tokens);
}

/*
 * Return a malloc'ed string with quotes such that it will be parsed
 * as one argument by the shell.  If a list of strings quoted by this
 * function are joined by spaces and passed to shellSplit() above, the
 * original strings will be returned.
 *
 * All characters in the input string are considered literals.
 */
char *
shellquote(char *in)
{
        int     len = strlen(in);
        int     nlen;
        char    *s, *t;
        char    *out;

        /* handle simple case */
        if (strcspn(in, " \t\n\r'\"<>|$&;[]*()\\")==len) return (strdup(in));

        nlen = len + 2 + 1;     /* quotes + null */
        for (s = in; *s; s++) {
                if ((*s == '"') || (*s == '\\')) ++nlen;
        }
        t = out = malloc(nlen);
        *t++ = '"';
        for (s = in; *s; ) {
                if ((*s == '"') || (*s == '\\')) *t++ = '\\';
                *t++ = *s++;
        }
        *t++ = '"';
        *t = 0;
        return (out);
}

/*
 * Takes a string, parses it like /bin/sh, and splits it into
 * tokens.  The result is is returned in a lines array.
 *
 * Rules:
 *
 *	The string is split on white space boundaries unless the white space
 *	is made literal by one of the following rules.
 *
 *	A backslash (\) is the escape character.  It preservers the
 *	literal value of the next character that follows.
 *
 *      Enclosing characters in single quotes preserves the literal
 *      value of each character within the quotes.  A single quote may
 *      not occur between single quotes, even when preceded by a
 *      backslash.
 *
 *      Enclosing characters in double quotes preserves the literal
 *      value of all characters within the quotes, with the exception
 *      of \.  The backslash retains its special meaning only when
 *      followed by the characters " or \. A double quote (") may be
 *      quoted within double quotes by preceding it with a backslash.
 *
 * As a special flag for spawn_pipeline() every string has a 1
 * character flag after the null at the end.  If it is 1 then that
 * item is a shell meta function (like > or |).  It is 0 otherwise.
 * This is so '>' and \> can be seperated from >.
 * (the marker is not visible unless you know to look for it.)
 */
char **
shellSplit(const char *line)
{
	const	char	*pin;	/* input pointer */
	const	char	*ein;	/* pointer to end of block to insert */
	char	**ret = 0;
	char	*e;
	int	item = 0;	/* buf contains data */
	int	c;
	int	bufsize = 128;
	char	*buf = malloc(bufsize);
	char	*pout;		/* end of 'buf' at insert point */

/* make room for 'extra' more characters in buffer */
#define BUF_RESIZE(extra) \
	while (pout - buf > bufsize - (extra)) { \
		char	*newbuf = malloc(2 * bufsize); \
		memcpy(newbuf, buf, pout - buf); \
		pout = newbuf + (pout - buf); \
		buf = newbuf; \
		bufsize *= 2; \
	}

/* If we have an item, save it one the list and reset the buffer */
#define SAVE_ITEM() \
	if (item) { \
		*pout++ = 0; /* 2 nulls */ \
		*pout++ = 0; \
		ret = addLine(ret, memcpy(malloc(pout-buf), buf, pout-buf)); \
		pout = buf; \
		item = 0; \
	}

	assert(line);
	pin = line;
	pout = buf;
	*pout = 0;
	while (*pin) {
		switch (*pin) {
		    /* whitespace */
		    case ' ': case '\t': case '\n': case '\r':
			++pin;
			SAVE_ITEM();
			break;
		    /* single quoted strings */
		    case '\'':
			++pin;
			ein = strchr(pin, '\'');
			unless (ein) {
				fprintf(stderr, "unmatched ' in (%s)\n", line);
				exit(1);
			}
			BUF_RESIZE(ein - pin);
			strncpy(pout, pin, ein - pin);
			pout += ein - pin;
			pin = ein+1;
			item = 1;
			break;
		    /* double quoted strings */
		    case '"':
			++pin;
			while (*pin && *pin != '"') {
				ein = pin + strcspn(pin, "\"\\");
				BUF_RESIZE(ein - pin + 1);
				strncpy(pout, pin, ein - pin);
				pout += ein - pin;
				pin = ein;
				if (*pin == '\\') {
					if (pin[1] == '\\' || pin[1] == '"') {
						++pin;
					}
					*pout++ = *pin++;
				}
			}
			if (*pin != '"') {
				fprintf(stderr, "unmatched \" in (%s)\n",
				    line);
				exit(1);
			}
			++pin;
			item = 1;
			break;
		    /* shell characters */
		    case '>': case '<': case '|':
			c = 1;
			if (pin[0] == '|' && pin[1] == '|') goto unhandled;
			if (pin[0] == '>' && pin[1] == '>') c = 2;
		sep:
			SAVE_ITEM();
			e = malloc(c+2);
			strncpy(e, pin, c);
			e[c] = 0;
			e[c+1] = 1;	/* 1 after null */
			ret = addLine(ret, e);
			pin += c;
			break;
		    /* unhandled functions */
		    case '`': case '$': case '&': case ';':
		    case '[': case ']': case '*': case '(': case ')':
		unhandled:
			fprintf(stderr,
			    "command line (%s) contains unhandled shell expressions.\n",
			    line);
			exit(1);
		    /* escape character */
		    case '\\':
			++pin;
			unless (*pin) break;
			/* falltrough */
		    default:
			if (isdigit(*pin) &&
			    (pin[1] == '<' || pin[1] == '>')) {
				if (pin[2] == '&' && isdigit(pin[3])) {
					c = 4;
				} else {
					c = 2;
				}
				goto sep;
			} else {
				*pout++ = *pin++;
				item = 1;
			}
			break;
		}
		BUF_RESIZE(2);
	}
	SAVE_ITEM();
	free(buf);
	return (ret);
}

/*
 * Assuming sorted arrays representing a set, subtract from 'space'
 * items that exist in 'remove'.  'space' is modified, and 'remove'
 * isn't.
 *
 * returns number of items removed from space
 */
int
pruneLines(char **space, char **remove,
    int (*compar)(const void *, const void *), void(*freep)(void *ptr))
{
	int	i, j;
	int	newj;
	int	cmp;
	int	removed = 0;

	if (emptyLines(space)) goto out;
	unless (compar) compar = string_sort;
	newj = j = 1;
	EACH(remove) {
		while (1) {
			while ((cmp = compar(&space[j], &remove[i])) < 0) {
				/* skip item from space */
				if (j > newj) space[newj] = space[j];
				newj++;
				j++;
				unless (j <= LLEN(space)) {
					/* done with space */
					goto out;
				}
			}
			if (cmp > 0) break; /* need next remove item */

			/* matched, remove item */
			if (freep) freep(space[j]);
			j++;
			removed++;
			unless (j <= LLEN(space)) {
				/* done with space */
				goto out;
			}
		}
	}
out:	if (removed) {
		while (j <= LLEN(space)) {
			space[newj++] = space[j++];
		}
		truncLines(space, newj-1);
	}
	return (removed);
}

/*
 * Appends a C null terminated string to a data buffer allocated in a
 * lines array.  This is just a simple alias for data_append() and any
 * decent compiler will just add a jump into data_append().
 */
char	**
str_append(char **space, void *str, int gift)
{
	return (data_append(space, str, strlen(str), gift));
}

/*
 * For an N entry lines struct the bytes used are
 *	sizeof(ptr) + 12 + N*2 + N*sizeof(ptr)
 * 4 entry array (2 data items, 32 bit arch) = 40 bytes or 10 bytes/entry.
 * 100 entry array (98 data items, 32 bit arch) = 616 bytes or 6.1 bytes/entry.
 *
 * You definitely don't want to shove 1 byte entries in here.
 */
typedef struct {
	char	*buf;		/* partially filled buffer */
	u32	buflen;		/* sizeof(buf) */
	u32	left;		/* space left */
	u32	bytes;		/* bytes saved so far */
	u32	nlen;		/* size of len array */
	u32	lasti;		/* len of len array */
	u16	len[0];		/* array of data lengths */
} dinfo;

char	**
data_append(char **space, void *str, int len, int gift)
{
	dinfo	*s;
	int	inc = 1;

	unless (len) {
		/* str_append(buf, strnonldup("\n"), 1); */
		if (str && gift) free(str);
		return (space);
	}
	assert(len < 64<<10);
	unless (space) {
		space = allocLines(4);
		s = calloc(1, sizeof(*s) + (LSIZ(space) * sizeof(u16)));
		s->nlen = LSIZ(space);
		s->lasti = 1;
		space = addLine(space, s);
	}
	s = (dinfo*)space[1];
	assert(s);
	s->bytes += len;
	if (len < s->left) {
		memcpy(&s->buf[s->buflen - s->left], str, len);
		s->left -= len;
		inc = 0;
		if (gift) free(str);
	} else if (gift && (len > 32)) {
		s->buf = 0;
		s->left = 0;
		space = addLine(space, str);
	} else {
		if (len < 64) {
			s->buflen = 128;
		} else if (len < 1024) {
			s->buflen = len * 4;
		} else {
			s->buflen = len;
		}
		s->buf = malloc(s->buflen);
		s->left = s->buflen - len;
		memcpy(s->buf, str, len);
		space = addLine(space, s->buf);
		if (gift) free(str);
	}
	if (s->nlen != LSIZ(space)) {
		dinfo	*ns;
		int	i;

		ns = calloc(1, sizeof(*s) + (LSIZ(space) * sizeof(u16)));
		*ns = *s;
		for (i = 2; i <= s->lasti; ns->len[i] = s->len[i], i++);
		ns->nlen = LSIZ(space);
		space[1] = (char*)ns;
		free(s);
		s = ns;
	}
	s->lasti += inc;
	s->len[s->lasti] += len;
	assert(s);
	return (space);
}

int
data_length(char **space)
{
	dinfo	*s;

	unless (space) return (0);
	s = (dinfo*)space[1];
	return (s->bytes);
}

char	*
_pullup(u32 *bytep, char **space, int null)
{
	int	i, len = 0;
	char	*data;
	dinfo	*s;

	unless (space) {
		if (bytep) *bytep = 0;
		return (null ? strdup("") : 0);
	}
	s = (dinfo*)space[1];
	data = malloc(s->bytes + (null ? 1 : 0));
	for (i = 2; i <= s->lasti; i++) {
		memcpy(&data[len], space[i], s->len[i]);
		len += s->len[i];
	}
	assert(len == s->bytes);
	if (bytep) *bytep = len;
	if (null) data[s->bytes] = 0;
	freeLines(space, free);
	return (data);
}

#ifdef	TEST_LINES
#define	RAND(max)	(1 + (int)((float)max*rand()/(RAND_MAX+1.0)))

int
main()
{
	char	*want, *data, *buf;
	int	biggest, inserts, iter, len, off, i;
	int	ins = 0, bytes = 0;
	char	**s = 0;
	char	c;

	/*
	 * Test by generating random numbers of inserts of varying sizes.
	 */
	srand(0x30962 + time(0) + getpid());
	for (iter = 1; iter <= 30000; ++iter) {
		if (iter < 500) {
			biggest = RAND(10000);
			unless (iter % 10) fprintf(stderr, "%u\r", iter);
		} else if (iter < 1000) {
			biggest = RAND(1000);
			unless (iter % 100) fprintf(stderr, "%u\r", iter);
		} else if (iter < 10000) {
			biggest = RAND(333);
			unless (iter % 500) fprintf(stderr, "%u\r", iter);
		} else {
			biggest = RAND(80);
			unless (iter % 1000) fprintf(stderr, "%u\r", iter);
		}
		inserts = RAND(250);
		want = malloc(biggest * inserts + 1);
		buf = malloc(biggest + 1);
		c = 'a';
		off = 0;
		s = 0;
		while (inserts--) {
			len = RAND(biggest);
			for (i = 0; i < len; buf[i++] = c, want[off++] = c);
			buf[i] = 0;
			c++;
			if (c > 'z') c = 'a';
			if (RAND(100) > 25) {
				char	*dup = malloc(len+1);

				memcpy(dup, buf, len+1);
#ifdef	T_DATA
				s = data_append(s, dup, len, 1);
#else
				s = str_nappend(s, dup, len, 1);
#endif
			} else {
#ifdef	T_DATA
				s = data_append(s, buf, len, 0);
#else
				s = str_nappend(s, buf, len, 0);
#endif
			}
			ins++;
			bytes += len;
		}
#ifdef	T_DATA
		data = data_pullup(&len, s);
#else
		data = str_pullup(&len, s);
#endif
		assert(len == off);
		assert(memcmp(data, want, len) == 0);
		free(data);
		free(want);
		free(buf);
	}
	fprintf(stderr, "INS=%u bytes=%u avg=%u\n", ins, bytes, bytes/ins);
	return (0);
}
#endif
