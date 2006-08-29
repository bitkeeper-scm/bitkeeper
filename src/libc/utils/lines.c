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

	/*
	 * We might be reusing an old lines buffer, so we must stomp
	 * the cache.
	 */
	addLine_lastp = LINES_INVALID;
	return (space);
}

/*
 * Save a line in an array.  If the array is out of space, reallocate it.
 * The size of the array is in array[0].
 * This is OK on 64 bit platforms.
 *
 * addLine(space, 0) will make sure that the array has a null at the end.
 */
char	**
addLine(char **space, void *line)
{
	int	i;

	unless (space) {
		space = allocLines(16);
	} else if (space[LSIZ(space)-1]) {	/* full up, dude */
		int	size = LSIZ(space);
		char	**tmp = allocLines(size*2);

		assert(tmp);
		memcpy(tmp, space, size*sizeof(char*));
		tmp[0] = (char *)(long)(size * 2);
		free(space);
		space = tmp;
	}
	unless (line) return (space);
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

/* return number of lines in array */
int
nLines(char **space)
{
      int     i;

      if (addLine_lastp == space) {
              i = addLine_lasti + 1;
      } else {
              EACH(space);
      }
      return (i-1);
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

int
string_sort(const void *a, const void *b)
{
	char	*l, *r;

	l = *(char**)a;
	r = *(char**)b;
	return (strcmp(l, r));
}

void
sortLines(char **space, int (*compar)(const void *, const void *))
{
	if (!space) return;
	unless (compar) compar = string_sort;
	qsort((void*)&space[1], nLines(space), sizeof(char*), compar);
}

void
uniqLines(char **space, void(*freep)(void *ptr))
{
	int	i;

	unless (space) return;
	sortLines(space, 0);
	EACH(space) {
		if (i == 1) continue;
		while (space[i] && streq(space[i-1], space[i])) {
			removeLineN(space, i, freep);
		}
	}
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

	if (!space) return;
	if (freep) {
		EACH(space) freep(space[i]);
	}
	space[0] = 0;
	free(space);
	addLine_lastp = LINES_INVALID;
}

int
removeLine(char **space, char *s, void(*freep)(void *ptr))
{
	int	i, found, n = 0;

	do {
		found = 0;
		EACH(space) {
			if (streq(space[i], s)) {
				if (freep) freep(space[i]);
				space[i] = 0;
				while ((++i< (int)(long)space[0]) && space[i]) {
					space[i-1] = space[i];
				}
				space[i-1] = 0;
				n++;
				found = 1;
				addLine_lastp = LINES_INVALID;
				break;
			}
		}
	} while (found);
	return (n);
}

void
removeLineN(char **space, int rm, void(*freep)(void *ptr))
{
	int	i;

	assert(rm < (int)(long)space[0]);
	assert(rm > 0);
	if (freep) freep(space[rm]);
	for (i = rm; (++i < (int)(long)space[0]) && space[i]; ) {
		space[i-1] = space[i];
	}
	space[i-1] = 0;
	addLine_lastp = LINES_INVALID;
}

void	*
popLine(char **space)
{
	int	i;
	void	*data;

	unless (addLine_lastp == space) {
		EACH(space); 		/* I want to get to the end */
		addLine_lastp = space;
		addLine_lasti = i - 1;
	}
	unless (addLine_lasti > 0) return (0);
	i = addLine_lasti--;
	assert(i < LSIZ(space));
	assert(space[i]);
	data = space[i];
	space[i] = 0;
	return (data);
}

/*
 * Fill a lines array from a file.
 * Each line is chomp()ed.
 * XXX - does not handle long lines.
 */
char	**
file2Lines(char **space, char *file)
{
	FILE	*f;
	char	*p;
	char	buf[MAXLINE];

	unless (file && (f = fopen(file, "r"))) return (space);
	while (fnext(buf, f)) {
		p = strchr(buf, '\n');
		assert(p);
		while ((*p == '\n') || (*p == '\r')) *p-- = 0;
		space = addLine(space, strdup(buf));
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

	unless (file && (f = fopen(file, "w"))) return (-1);
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

	unless (space && space[1]) return (0);
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
 * Takes a string a parses it like /bin/sh and splits it into
 * tokens.  They are returned in a lines array.
 *
 * Rules:
 *
 *	The string is split on whitespace boundries unless the space
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
	u16	len[0];		/* array of data lengths */
				/* [0] = # of elements */
				/* [1] = lasti */
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
		s->len[0] = LSIZ(space);
		s->len[1] = 1;
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
		if (len < 1000) {
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
	if (s->len[0] != LSIZ(space)) {
		dinfo	*ns;
		int	i;
		
		ns = calloc(1, sizeof(*s) + (LSIZ(space) * sizeof(u16)));
		*ns = *s;
		for (i = 1; i < s->len[0]; ns->len[i] = s->len[i], i++);
		ns->len[0] = LSIZ(space);
		space[1] = (char*)ns;
		free(s);
		s = ns;
	}
	s->len[1] += inc;
	s->len[s->len[1]] += len;
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
	for (i = 2; i <= s->len[1]; i++) {
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
	srand(0x30962);
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
