#include "../system.h"
#include "../sccs.h"

#ifndef WIN32
char *
mnext(register MMAP *m)
{
	register char	*s, *t;

	assert(m);
	if (m->where >= m->end) return (0);
	for (s = m->where; (s < m->end) && (*s++ != '\n'); );
	assert(s[-1] == '\n');		/* XXX - what if no newline? */
	t = m->where;
	m->where = s;
	return (t);
}
#else
/*
 * we need this to simulate text mode input
 */
char *
mnext(register MMAP *m)
{
	register char	*s, *p;
	register int	text_mode = 1;
	static char *buf = 0;
	static buf_size = 0;

	assert(m);
	if (m->where >= m->end) return (0);
	if (m->flags & MMAP_BIN_MODE) text_mode = 0;
	if (text_mode) {
		int needBuffer = 0;
		for (s = m->where; (s < m->end) && (*s++ != '\n'); ) {
			if ((s[0] == '\n') && (s[-1] == '\r')) needBuffer = 1;
		}
		unless (needBuffer) goto done;

		if ((s - m->where) > buf_size) {
			if (buf) free(buf);
			buf_size = s - m->where;
			buf = malloc(buf_size);
		}
		for (s = m->where, p = buf; s < m->end ;) {
			if (*s == '\n') {
				if (s[-1] == '\r') p--; /* remove DOS '\r' */
				*p = *s++;
				break;
			}
			*p++ = *s++;
		}
		assert(s[-1] == '\n');	/* XXX - what if no newline? */
		m->where = s;
		assert((p - buf) <= buf_size);
		return (buf);
	} else { /* binary mode */
		for (s = m->where; (s < m->end) && (*s++ != '\n'); );
done:	assert(s[-1] == '\n');	/* XXX - what if no newline? */
		p = m->where;
		m->where = s;
		return (p);
	}
}
#endif
