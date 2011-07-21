/*
 * SCCS library - all of SCCS is implemented here.  All the other source is
 * no more than front ends that call entry points into this library.
 * It's one big file so I can hide a bunch of stuff as statics (private).
 *
 * XXX - I don't handle memory allocation failures well.
 *
 * Copyright (c) 1997-2000 Larry McVoy and others.	 All rights reserved.
 */
#include "system.h"
#include "sccs.h"
#include "resolve.h"
#include "bkd.h"
#include "logging.h"
#include "tomcrypt.h"
#include "range.h"
#include "graph.h"
#include "bam.h"

typedef struct weave weave;
#define	WRITABLE_REG(s)	(WRITABLE(s) && isRegularFile((s)->mode))
private delta	*rfind(sccs *s, char *rev);
private delta	*dinsert(sccs *s, delta *d, int fixDate);
private int	samebranch(delta *a, delta *b);
private char	*sccsXfile(sccs *sccs, char type);
private int	badcksum(sccs *s, int flags);
private int	printstate(ser_t *state, u8 *slist);
private int	delstate(ser_t ser, ser_t *state, u8 *slist);
private int	whatstate(ser_t *state);
private int	visitedstate(ser_t *state, u8 *slist);
private ser_t	*changestate(ser_t *state, char type, ser_t serial);
private int	end(sccs *, delta *, FILE *, int, int, int, int);
private void	date(sccs *s, delta *d, time_t tt);
private int	getflags(sccs *s, char *buf);
private sum_t	fputmeta(sccs *s, u8 *buf, FILE *out);
private ser_t*	getserlist(sccs *sc, int isSer, char *s, int *ep);
private int	checkRev(sccs *s, char *file, delta *d, int flags);
private int	checkrevs(sccs *s, int flags);
private int	stripChecks(sccs *s, delta *d, char *who);
private delta*	hashArg(sccs *s, delta *d, char *name);
private delta*	csetFileArg(sccs *s, delta *d, char *name);
private delta*	dateArg(sccs *s, delta *d, char *arg, int defaults);
private delta*	userArg(sccs *s, delta *d, char *arg);
private delta*	hostArg(sccs *s, delta *d, char *arg);
private delta*	pathArg(sccs *s, delta *d, char *arg);
private delta*	randomArg(sccs *s, delta *d, char *arg);
private delta*	zoneArg(sccs *s, delta *d, char *arg);
private delta*	mergeArg(delta *d, char *arg);
private delta*	sumArg(delta *d, char *arg);
private	void	symArg(sccs *s, delta *d, char *name);
private	delta*	revArg(sccs *s, delta *d, char *arg);
private	int	unlinkGfile(sccs *s);
private int	write_pfile(sccs *s, int flags, delta *d,
		    char *rev, char *iLst, char *i2, char *xLst, char *mRev);
private time_t	date2time(char *asctime, char *z, int roundup);
private int	addSym(char *name, sccs *sc, int flags, admin *l, int *ep);
private int	sameFileType(sccs *s, delta *d);
private int	uuexpand_gfile(sccs *s, char *tmpfile);
private int	isRegularFile(mode_t m);
private void	sccs_freetable(sccs *s);
private	delta*	getCksumDelta(sccs *s, delta *d);
private delta	*gca(sccs *, delta *left, delta *right);
private delta	*gca2(sccs *s, delta *left, delta *right);
private delta	*gca3(sccs *s, delta *left, delta *right, char **i, char **e);
private int	compressmap(sccs *s, delta *d, u8 *set, char **i, char **e);
private	void	uniqDelta(sccs *s);
private	void	uniqRoot(sccs *s);
private int	weaveMove(weave *w, int line, int before, ser_t patchserial);
private int	doFast(weave *w, char **patchmap, MMAP *diffs);
private int	checkGone(sccs *s, int bit, char *who);
private	int	openOutput(sccs*s, int encode, char *file, FILE **op);
private	void	parseConfig(char *buf, MDBM *db);
private	void	taguncolor(sccs *s, delta *d);
private	void	prefix(sccs *s,
		    delta *d, u32 flags, int lines, char *name, FILE *out);
private	int	sccs_meta(char *m, sccs *s, delta *parent,
		    MMAP *initFile, int fixDates);
private	int	misc(sccs *s);
private	void	sccs_zputs_init(sccs *s, FILE *fout);
private	void	sccs_zputs_done(sccs *s);
private	int	bin_deltaTable(sccs *s, FILE *out);
private	off_t	bin_data(char *header);

/*
 * returns 1 if dir is a directory that is not empty
 * 0 for empty or non-directories
 */
int
emptyDir(char *dir)
{
	char	**d;
	int	i, n = 0;

	unless (d = getdir(dir)) return (0);
	EACH (d) n++;
	freeLines(d, free);
	return (n == 0);
}

/*
 * Convert lrwxrwxrwx -> 0120777, etc.
 */
private mode_t
a2mode(char *mode)
{
	mode_t	m;

	assert(mode && *mode);
	switch (*mode) {
	    case '-': m = S_IFREG; break;
	    case 'd': m = S_IFDIR; break;
	    case 'l': m = S_IFLNK; break;
	    default:
	    	fprintf(stderr, "Unsupported file type: '%c'\n", *mode);
		return (0);
	}
	mode++;
	if (*mode++ == 'r') m |= S_IRUSR;
	if (*mode++ == 'w') m |= S_IWUSR;
	switch (*mode++) {
#ifdef	S_ISUID
	    case 'S': m |= S_ISUID; break;
	    case 's': m |= S_ISUID; /* fall-through */
#endif
	    case 'x': m |= S_IXUSR; break;
	}

	/* group - XXX, inherite these on DOS? */
	if (*mode++ == 'r') m |= S_IRGRP;
	if (*mode++ == 'w') m |= S_IWGRP;
	switch (*mode++) {
#ifdef	S_ISGID
	    case 'S': m |= S_ISGID; break;
	    case 's': m |= S_ISGID; /* fall-through */
#endif
	    case 'x': m |= S_IXGRP; break;
	}

	/* other */
	if (*mode++ == 'r') m |= S_IROTH;
	if (*mode++ == 'w') m |= S_IWOTH;
	if (*mode++ == 'x') m |= S_IXOTH;
	return (m);
}

private mode_t
fixModes(mode_t m)
{
	unless (m & 0200) {
		fprintf(stderr, "Warning: adding owner write permission\n");
		m |= 0200;
	}
	unless (m & 0400) {
		fprintf(stderr, "Warning: adding owner read permission\n");
		m |= 0400;
	}
	return (m);
}

private mode_t
getMode(char *arg)
{
	mode_t	m;

	if (isdigit(*arg)) {
		char	*p = arg;
		for (m = 0; isdigit(*p); m <<= 3, m |= (*p - '0'), p++) {
err:			unless ((*p >= '0') && (*p <= '7')) {
				fprintf(stderr, "Illegal octal file mode: %s\n",
				    arg);
				return (0);
			}
		}
		unless (m & S_IFMT) m |= S_IFREG;
		unless (S_ISLNK(m) || S_ISDIR(m) || S_ISREG(m)) goto err;
	} else {
		m = a2mode(arg);
	}
	return (fixModes(m));
}

/*
 * chmod [ugoa]+rwxs
 * chmod [ugoa]-rwxs
 * chmod [ugoa]=rwxs
 *
 * Yes, this code knows the values of the bits.  Tough.
 */
private mode_t
newMode(delta *d, char *p)
{
	mode_t	mode, or = 0;
	int	op = 0, setid = 0, u = 0, g = 0, o = 0;

	assert(p && *p);
	if (isdigit(*p) || (strlen(p) == 10)) return (getMode(p));

	for ( ; *p; p++) {
		switch (*p) {
		    case 'u': u = 1; break;
		    case 'g': g = 1; break;
		    case 'o': o = 1; break;
		    case 'a': u = g = o = 1; break;
		    default: goto plusminus;
		}
	}
plusminus:
	unless (u || g || o) u = g = o = 1;
	switch (*p) {
	    case '+': case '-': case '=': op = *p++; break;
	    default: return (0);
	}

	setid = mode = 0;
	while (*p) {
		switch (*p++) {
		    case 'r': or |= 4; break;
		    case 'w': or |= 2; break;
		    case 'x': or |= 1; break;
		    case 's': setid = 1; break;
		    default: return (0);
		}
	}
	if (u) {
		mode |= (or << 6);
		if (setid) mode |= 04000;
	}
	if (g) {
		mode |= (or << 3);
		if (setid) mode |= 02000;
	}
	if (o) mode |= or;
#ifndef	S_ISUID
	mode &= 0777;	/* no setgid if no setuid */
#endif
	switch (op) {
	    case '-':	mode = (d->mode & ~mode); break;
	    case '+':	mode = (d->mode | mode); break;
	    case '=':
		unless (u) mode |= (d->mode & 0700);
		unless (g) mode |= (d->mode & 0070);
		unless (o) mode |= (d->mode & 0007);
		break;
	}
	mode |= (d->mode & S_IFMT);	/* don't lose file type */
	return (fixModes(mode));
}

/* value is overwritten on each call */
char	*
mode2a(mode_t m)
{
	static	char mode[12];
	char	*s = mode;

	if (S_ISLNK(m)) {
		*s++ = 'l';
	} else if (S_ISDIR(m)) {
		*s++ = 'd';
	} else if (S_ISREG(m)) {
		*s++ = '-';
	} else {
	    	fprintf(stderr, "Unsupported mode: '%o'\n", m);
		return ("<bad mode>");
	}
	*s++ = (m & S_IRUSR) ? 'r' : '-';
	*s++ = (m & S_IWUSR) ? 'w' : '-';
#ifndef	S_ISUID
	*s++ = (m & S_IXUSR) ? 'x' : '-';
#else
	*s++ = (m & S_IXUSR) ? ((m & S_ISUID) ? 's' : 'x')
			     : ((m & S_ISUID) ? 'S' : '-');
#endif
	*s++ = (m & S_IRGRP) ? 'r' : '-';
	*s++ = (m & S_IWGRP) ? 'w' : '-';
#ifndef	S_ISGID
	*s++ = (m & S_IXGRP) ? 'x' : '-';
#else
	*s++ = (m & S_IXGRP) ? ((m & S_ISGID) ? 's' : 'x')
			     : ((m & S_ISGID) ? 'S' : '-');
#endif
	*s++ = (m & S_IROTH) ? 'r' : '-';
	*s++ = (m & S_IWOTH) ? 'w' : '-';
	*s++ = (m & S_IXOTH) ? 'x' : '-';
	*s = 0;
	return (mode);
}

char	*
mode2FileType(mode_t m)
{
	if (S_ISREG(m)) {
		return ("FILE");
	} else if (S_ISLNK(m)) {
		return ("SYMLINK");
	} else {
		return ("unsupported file type");
	}
}

/*
 * extract the file type bits from the mode
 */
int
fileType(mode_t m)
{
	return (m & S_IFMT);
}

/*
 * These are the file types we currently suppprt
 * TODO: we may support empty directory & special file someday
 */
int
fileTypeOk(mode_t m)
{
	return ((S_ISREG(m)) || (S_ISLNK(m)));
}

/*
 * remove last character from string and return it
 * XXX this function is deprecated, pls use chomp() instead.
 */
char
chop(char *s)
{
	char	c;

	assert(s);
	unless (*s) return (0);
	s += strlen(s) - 1;
	c = *s;
	*s = 0;
	return (c);
}


/*
 * Keys are like u@h|path|date|.... whatever
 * We want to skip over any spaces in the path part.
 */
char	*
separator(char *s)
{
	while (s && (*s != '|') && *s) s++;
	unless (s && (*s == '|')) return (0);
	s++;
	while ((*s != '|') && *s) s++;
	unless (*s == '|') return (0);
	return (strchr(s, ' '));
}

/*
 * Convert the pointer into something we can write.
 * We trim the newline since most of the time that's what we want anyway.
 * This pointer points into a readonly mmapping.
 */
char	*
mkline(char *p)
{
	static	char buf[MAXLINE];
	char	*s;

	unless (p) return (0);
	for (s = buf; (*s++ = *p++) != '\n'; );
	s[-1] = 0;
	assert((s - buf) <= MAXLINE);
	return (buf);
}

/*
 * Return the length of the buffer until a newline.
 */
int
linelen(char *s)
{
	char	*t = s;

	while (*t && (*t++ != '\n'));
	return (t-s);
}

/*
 * Convert a serial to an ascii string.
 */
void
sertoa(register char *buf, ser_t val)
{
	char	reverse[6];
	int	i, j;

	for (i = 0; val; i++, val /= 10) {
		reverse[i] = '0' + val % 10;
	}
	for (j = 0; i--; ) buf[j++] = reverse[i];
	buf[j] = 0;
}

#define	atoi	myatoi
private int
atoi(register char *s)
{
	register int val = 0;

	if (!s) return (0);
	while (*s && isdigit(*s)) {
		val = val * 10 + *s++ - '0';
	}
	return (val);
}

int
atoi_p(char **sp)
{
	register int val = 0;
	register char *s = *sp;

	if (!s) return (0);
	while (*s && isdigit(*s)) {
		val = val * 10 + *s++ - '0';
	}
	*sp = s;
	return (val);
}

private int
atoiMult_p(char **p)
{
	register int val = 0;
	register char *s = *p;

	if (!s) return (0);
	while (*s && isdigit(*s)) {
		val = val * 10 + *s++ - '0';
	}
	switch (*s) {
	    case 'K': val *= 1000; s++; break;
	    case 'M': val *= 1000000; s++; break;
	    case 'G': val *= 1000000000; s++; break;
	}
	*p = s;
	return (val);
}

/*
 * Free a standalone delta (not in the array).
 */
void
sccs_freedelta(delta *d)
{
	if (!d) return;

	assert(!INARRAY(d));
	free(d);
}

private	void
freeExtra(sccs *s, delta *d)
{
	FREE(EXTRA(s, d)->rev);
}

/*
 * Free the entire delta table.
 * This follows the ->next pointer and is not recursive.
 */
private void
sccs_freetable(sccs *s)
{
	dextra	*dx;

	FREE(s->slist);
	EACHP(s->extra, dx) {
		free(dx->rev);	/* freeExtra(s, d) sped up */
	}
	FREE(s->extra);
	s->tree = s->table = 0;
	s->nextserial = 0;
	if (s->heap.buf) free(s->heap.buf);
	memset(&s->heap, 0, sizeof(DATA));
	if (s->uniqheap) {
		hash_free(s->uniqheap);
		s->uniqheap = 0;
	}
}

/*
 * Check a delta for duplicate fields which are normally inherited.
 * Also inherit any fields which are not set in the delta and are set in
 * the parent.
 * Also mark any merged deltas.
 * This routine must be called after the delta is added to a graph which
 * is already correct.	As in dinsert().
 * Make sure to keep this up for all inherited fields.
 */
void
sccs_inherit(sccs *s, delta *d)
{
	delta	*p;
	char	*phost;
	char	buf[MAXLINE];

	unless (d) return;
	unless (p = PARENT(s, d)) return;

#define	CHK_DUP(field) \
	if (!d->field && p->field) d->field = p->field;

	CHK_DUP(zone);
	CHK_DUP(csetFile);
	CHK_DUP(pathname);
	CHK_DUP(sortPath);

	if ((p->flags & D_MODE) && !(d->flags & D_MODE)) {
		d->flags |= D_MODE;
		d->mode = p->mode;
		CHK_DUP(symlink);
	}
#undef	CHK_DUP

	if (p->userhost) {
		assert(d->userhost);
		if (!strchr(USERHOST(s, d), '@') &&
		    (phost = HOSTNAME(s, p)) && *phost) {
			/* user but no @host, so get host from parent */
			sprintf(buf, "%s@%s", USERHOST(s, d), phost);
			d->userhost = sccs_addUniqStr(s, buf);
		}
	}
}

/*
 * Find first kid on same branch or oldest kid.
 * Use sparingly.  If need to use much, then use sccs_mkKidList()
 * and then use the KID() macro.
 * The first kid will be same branch in BK, not necessarily so in teamware
 */
delta *
sccs_kid(sccs *s, delta *d)
{
	delta	*e, *first = 0;

	for (e = d + 1; e <= s->table; e++) {
		if (e->flags && !TAG(e) && (e->pserial == SERIAL(s, d))) {
			if (samebranch(d, e)) return (e);
			unless (first) first = e;
		}
	}
	return (first);
}

void
sccs_mkKidList(sccs *s)
{
	delta	*d, *p;
	delta	*e;
	KIDS	*pk;		/* parent's kids */


	FREE(s->kidlist);
	growArray(&s->kidlist, s->nextserial);
	EACHP(s->slist, d) {
		unless (d->flags && !TAG(d)) continue;
		unless (p = PARENT(s, d)) continue;

		pk = s->kidlist + SERIAL(s, p);
		if (!pk->kid) {
			pk->kid = SERIAL(s, d);

		} else if (samebranch(p, KID(s, p))) { /* in right place */
			/*
			 * If there are siblings, add d at the end.
			 */
			for (e = KID(s, p); SIBLINGS(s, e); e = SIBLINGS(s, e));
			s->kidlist[SERIAL(s, e)].siblings = SERIAL(s, d);
		} else {
			/* else not in right place, put the new delta there. */
			s->kidlist[SERIAL(s, d)].siblings = pk->kid;
			pk->kid = SERIAL(s, d);
		}
	}
}

/*
 * Insert the delta in the (ordered) tree.
 * A little weirdness when it comes to removed deltas,
 * we want them off to the side if possible (it makes rfind work better).
 * New in Feb, '99: remove duplicate metadata fields here, maintaining the
 * invariant that a delta in the graph is always correct.
 */
private delta *
dinsert(sccs *s, delta *d, int fixDate)
{
	delta	*p;

	debug((stderr, "dinsert(%s)", d->rev));

	/* copy delta* into s->slist */
	unless (INARRAY(d)) {
		p = addArray(&s->slist, d);
		addArray(&s->extra, 0);
		free(d); /* not sccs_freedelta(); just free the mem */
		d = p;
		d->flags |= D_INARRAY;
		s->nextserial = SERIAL(s, d) + 1; /* until we get rid of it */
	}
	s->table = s->slist + nLines(s->slist);
	if (s->tree) {
		s->tree = SFIND(s, 1);
	} else {
		s->tree = SFIND(s, 1);
		debug((stderr, " -> ROOT\n"));
		if (fixDate) uniqRoot(s);
		return (d);
	}
	if (d->random) {
		debug((stderr, "GRAFT: %s@%s\n", s->gfile, d->rev));
		s->grafted = 1;
	}
	sccs_inherit(s, d);
	if (fixDate) {
		uniqDelta(s);
	}
	sccs_findKeyUpdate(s, d);
	return (d);
}


delta *
sfind(sccs *s, ser_t serial)
{
	delta	*d;

	assert(serial <= s->nextserial);
	if (s->slist && (d = SFIND(s, serial)) && d->flags) return (d);
	return (0);
}

/*
 * An array, indexed by years after 1971, which gives the seconds
 * at the beginning of that year.  Valid up to around 2038.
 * The first value is the time value for 1970-01-01-00:00:00 .
 *
 * Do NOT NOT NOT change this after shipping, even if it is wrong.
 *
 * Bummer.  I made the same mistake in generating this table (passing in
 * 70 instead of 1970 to the leap year calculation) so all entries
 * after 2000 were wrong.
 * What we'll do is rev the file format so that old binaries
 * won't be able to create deltas with bad time stamps.
 */
static const time_t  yearSecs[] = {
	     0,   31536000,   63072000,   94694400,  126230400,  157766400,
     189302400,  220924800,  252460800,  283996800,  315532800,  347155200,
     378691200,  410227200,  441763200,  473385600,  504921600,  536457600,
     567993600,  599616000,  631152000,  662688000,  694224000,  725846400,
     757382400,  788918400,  820454400,  852076800,  883612800,  915148800,
     946684800,  978307200, 1009843200, 1041379200, 1072915200, 1104537600,
    1136073600, 1167609600, 1199145600, 1230768000, 1262304000, 1293840000,
    1325376000, 1356998400, 1388534400, 1420070400, 1451606400, 1483228800,
    1514764800, 1546300800, 1577836800, 1609459200, 1640995200, 1672531200,
    1704067200, 1735689600, 1767225600, 1798761600, 1830297600, 1861920000,
    1893456000, 1924992000, 1956528000, 1988150400, 2019686400, 2051222400,
    2082758400, 2114380800, 0 };

/*
 * An array, indexed by the month which we are in, which gives the
 * number of seconds in all of the preceeding months of that year.
 * The index is 0 for Jan (which will return 0) and 12 for Dec
 * This is not adjusted for leap years.
 */
#define	DSECS	(24*60*60)
static const int monthSecs[13] = {
    0,		31*DSECS,  59*DSECS,  90*DSECS,
    120*DSECS,	151*DSECS, 181*DSECS, 212*DSECS,
    243*DSECS,	273*DSECS, 304*DSECS, 334*DSECS,
    365*DSECS };

static const char days[13] =
{ 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

private int
leapYear(int year)
{
	return ((!((year) % 4) && ((year) % 100)) || !((year) % 400));
}

/*
 * A version of mktime() which uses the fields
 *	year, mon, hour, min, sec
 * and assumes utc time.
 */
private time_t
tm2utc(struct tm *tp)
{
	time_t	t;

	if (tp->tm_year < 70) return (1);
	if (tp->tm_year >= 138) {
		fprintf(stderr,
		    "tm2utc: bad time structure: tm_year = %d\n", tp->tm_year);
		return  (0);
	}
	t = yearSecs[tp->tm_year - 70];
	t += monthSecs[tp->tm_mon];
	if ((tp->tm_mon > 1) && leapYear(1900 + tp->tm_year)) {
		t += DSECS;
	}
	t += ((tp->tm_mday - 1)* DSECS);
	t += (tp->tm_hour * 60 * 60);
	t += (tp->tm_min * 60);
	t += tp->tm_sec;
	return (t);
}

/*
 * given a UTC time_t return a tm struct that is also in UTC
 */
struct	tm *
utc2tm(time_t t)
{
	int	leap;
	int	tmp, i;
	static	struct tm tm;

	for (tmp = 70, i = 0; yearSecs[i+1]; i++, tmp++) {
		if (t < yearSecs[i+1]) break;
	}
	bzero(&tm, sizeof(tm));
	tm.tm_year = tmp;
	t -= yearSecs[i];
	leap = leapYear(1900 + tmp);
	/* Jan = 0, Feb = 1, ... */
	for (i = 0; i < 12; ++i) {
		/* [2] means tmp == end of january */
		tmp = monthSecs[i+1];
		if (leap && ((i+1) >= 2)) tmp += DSECS;

		/* if seconds < end of the month */
		if (t < tmp) {
			/* 0 = Jan, 1 = Feb, etc */
			tm.tm_mon = i;
			tmp = monthSecs[i];
			if (leap && (i >= 2)) tmp += DSECS;
			t -= tmp;
			break;
		}
	}
	tm.tm_mday = (t / DSECS) + 1; t -= DSECS * (tm.tm_mday - 1);
	assert(tm.tm_mday > 0);
	tm.tm_hour = t / 3600; t -= tm.tm_hour * 3600;
	tm.tm_min = t / 60; t -= tm.tm_min * 60;
	tm.tm_sec = t;
	return (&tm);
}

/*
 * Give an ascii date and a timezone in "[-]HH:MM" return a GMT time_t
 */
time_t
sccs_date2time(char *date, char *zone)
{
	return (date2time(date, zone, EXACT));
}

/*
 * Make sure that dates always increase in serial order.
 * XXX: note, this does not need to be true to have a valid bk graph
 * If the keys sort alphabetically with time the same, that is good enough.
 */
void
sccs_fixDates(sccs *s)
{
	int	i, f;
	delta	*d, *prev = 0;

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, i)) continue;
		if (prev && (prev->date <= d->date)) {
			f = (d->date - prev->date) + 1;
			prev->dateFudge += f;
			prev->date += f;
		}
		prev = d;
	}
}

char	*
age(time_t when, char *space)
{
	int	i;
	static	char buf[100];

#define	DOIT(SMALL, BIG, UNITS) \
	if (when <= 3*BIG) {		/* first 3 BIG as SMALL */	\
		for (i = 0; i <= 3*BIG; i += SMALL) {			\
			if (when <= i) {				\
				sprintf(buf, 				\
				    "%d%s%s", i/SMALL, space, UNITS);	\
				if (i/SMALL == 1) chop(buf);		\
				return (buf);				\
			}						\
		}							\
	}

	DOIT(1, MINUTE, "seconds");		/* first 3 minutes as seconds */
	DOIT(MINUTE, HOUR, "minutes");		/* first 3 hours as minutes */
	DOIT(HOUR, DAY, "hours");		/* first 3 days as hours */
	DOIT(DAY, WEEK, "days");		/* first 3 weeks as days */
	DOIT(WEEK, MONTH, "weeks");		/* first 3 months as days */
	DOIT(MONTH, YEAR, "months");		/* first 3 years as months */
	DOIT(YEAR, DECADE, "years");		/* first 3 decades as years */
	return ("more than 30 years");
}

private void
uniqRoot(sccs *s)
{
	delta	*d;

	assert(s->tree == s->table);
	d = s->tree;

	unless (uniq_open() == 0) return;	// XXX - no error?
	uniq_adjust(s, d);
	uniq_close();
}

/*
 * Fix the date in a new delta.
 * Make sure date is increasing
 */
private void
uniqDelta(sccs *s)
{
	delta	*next, *d;

	assert(s->tree != s->table);
	d = s->table;
	next = NEXT(d);
	assert(d != s->tree);

	/*
	 * This is kind of a hack.  We aren't in BK mode yet we are fudging.
	 * It keeps BK happy, I guess.
	 */
	unless (BITKEEPER(s)) {
		unless (next = NEXT(d)) return;
		if (next->date >= d->date) {
			time_t	tdiff;
			tdiff = next->date - d->date + 1;
			d->date += tdiff;
			d->dateFudge += tdiff;
		}
		return;
	}

	unless (uniq_open() == 0) return;
	if (d->date <= next->date) {
		time_t	tdiff;
		tdiff = next->date - d->date + 1;
		d->date += tdiff;
		d->dateFudge += tdiff;
	}

	/*
	 * We want the import convertor to produce the same tree
	 * when ran multiple times. Do not enforce unique key
	 * across different repository; 
	 */
	if (IMPORT(s)) {
		uniq_close();
		return;
	}

	uniq_adjust(s, d);
	uniq_close();
}

private int
monthDays(int year, int month)
{
	if (month != 2) return (days[month]);
	if (leapYear(year)) return (29);
	return (28);
}

/*
 * NOTE: because of the way this function handles timezone the tm struct
 * cannote be used directly and is really only acceptable to pass to tm2utc().
 */
private void
a2tm(struct tm *tp, char *asctime, char *z, int roundup)
{
	int	i, tmp;

#define	gettime(field) \
	if (isdigit(asctime[1])) { \
		tp->field = ((asctime[0] - '0') * 10 + (asctime[1] - '0')); \
		asctime += 2; \
	} else { \
		tp->field = *asctime++ - '0'; \
	} \
	for (; *asctime && !isdigit(*asctime); asctime++);

	/* this is weird, but all I care about here is up or !up */
	roundup = (roundup == ROUNDUP);
	bzero(tp, sizeof(*tp));
	tp->tm_mday = 1;
	if (roundup) {
		tp->tm_mday = 31;
		tp->tm_mon = 11;
		tp->tm_hour = 23;
		tp->tm_min = tp->tm_sec = 59;
	}

	/*
	 * We're moving towards always having 4 digit years but we still want
	 * to support, for now, 2 digit years and 4 digit years without 
	 * requiring a non-digit separator.  So here's how that works:
	 *
	 * If the first 2 digits are in the 69..18 range then we are going
	 * see that as 1969..2018.  And before 2019 we need to have dropped
	 * support for 2 digit years.  The reason for 1969 is imports which
	 * might have gone back far (and teamware grafted files w/ 1970 dates).
	 * Otherwise it's a 4 digit year.  
	 */
	for (i = tmp = 0; (i < 4) && isdigit(*asctime); i++) {
		/* I want the increment here because of the break below */
		tmp = tmp * 10 + (*asctime++ - '0');

		/* we want 69..99 or 0..18 and this does that */
		if ((i == 1) && ((tmp >= 69) || (tmp <= 18))) break;
    	}
	tp->tm_year = tmp;
	for (; *asctime && !isdigit(*asctime); asctime++);

	/*
	 * We want a value between 69 and 138 which covers the time between
	 * 1969 and 2038
	 */
	if (tp->tm_year < 69) tp->tm_year += 100;	/* we're 2000's */
	if (tp->tm_year >= 1969) tp->tm_year -= 1900;	/* 4 digit year */
	unless (*asctime) goto correct;

	/* tm_mon counts 0..11; ASCII is 1..12 */
	gettime(tm_mon); tp->tm_mon--; unless (*asctime) goto correct;
	gettime(tm_mday); unless (*asctime) goto correct;
	gettime(tm_hour); unless (*asctime) goto correct;
	gettime(tm_min); unless (*asctime) goto correct;
	gettime(tm_sec);
	if (((asctime[-1] == '-') || (asctime[-1] == '+')) && !z) z = --asctime;

correct:
	/* Correct for dates parsed as pre-epoch because of missing tz */
	if (tp->tm_year == 69) {
		bzero(tp, sizeof(*tp));
		z = 0;
	}
	
	/*
	 * Truncate down oversized fields.
	 */
	if (tp->tm_mon > 11) tp->tm_mon = 11;
	i = monthDays(1900 + tp->tm_year, tp->tm_mon + 1);
	if (i < tp->tm_mday) tp->tm_mday = i;
	if (tp->tm_hour > 23) tp->tm_hour = 23;
	if (tp->tm_min > 59) tp->tm_min = 59;
	if (tp->tm_sec > 59) tp->tm_sec = 59;

	if (z) {
		int	sign;

		if ((sign = (*z == '-'))) {
			z++;
			sign = 1;
		} else {
			sign = -1;	/* this is what I want */
		}
		if (*z == '+') z++;
		tp->tm_hour += atoi(z) * sign;
		while (*z++ != ':');
		tp->tm_min += atoi(z) * sign;
	}
}

/*
 * Take 93/07/25 21:14:11 and return a time_t that is useful.  Note that
 * "useful" means useful to BitKeeper, not anyone else.
 * For years < 69, assume 20xx which means add 100.
 *
 * roundup is EXACT or ROUNDUP.  Which do the implied
 * adjustments for the unspecified fields.  Fields which are incorrectly
 * specified, i.e., 31 for a month that has 30 days, are truncated back
 * down to legit values.
 *
 * This is a little weird because tm_ does not translate directly from
 * yy/mm/dd hh:mm:ss - see mktime(3) for details.
 */
private time_t
date2time(char *asctime, char *z, int roundup)
{
	struct	tm tm;

	/* when d->zone == 0 => (z = ZONE(s, d)) == ""; put z back to 0 */
	if (z && !z[0]) z = 0;
	a2tm(&tm, asctime, z, roundup);
#if	0
{	struct  tm tm2 = tm;
	struct  tm *tp;
	fprintf(stderr, "%s%s %04d/%02d/%02d %02d:%02d:%02d = %u = ",
	asctime,
	z ? z : "",
	tm.tm_year + 1900,
	tm.tm_mon + 1,
	tm.tm_mday,
	tm.tm_hour,
	tm.tm_min,
	tm.tm_sec,
	tm2utc(&tm2));
	tp = utc2tm(tm2utc(&tm2));
	fprintf(stderr, "%04d/%02d/%02d %02d:%02d:%02d\n",
	tp->tm_year + 1900,
	tp->tm_mon + 1,
	tp->tm_mday,
	tp->tm_hour,
	tp->tm_min,
	tp->tm_sec);
}
#endif
	return (tm2utc(&tm));
}

/*
 * Set the sfile so that it is older than any checked out associated gfile.
 * The -2 adjustments are because FAT file systems have a 2 second granularity.
 */
int
sccs_setStime(sccs *s, time_t newest)
{
	struct	utimbuf	ut;
	delta	*d;

	/* If we have no deltas we don't know what time it is */
	unless (s && s->table) return (0);

	/*
	 * To prevent the "make" command from doing a "get" due to 
	 * sfile's newer modification time, and then fail due to the
	 * editable gfile, adjust sfile's modification to be just
	 * before that of gfile's.
	 * Note: It is ok to do this, because we've already recorded
	 * the time of the delta in the delta table.
	 * A potential pitfall would be that it may confuse the backup
	 * program to skip the sfile when doing a incremental backup.
	 */
	ut.actime = time(0);

	/*
	 * Use the most recent "real" delta for the timestamp.
	 * We're skipping over TAGs or xflag changes because those don't
	 * modify the gfile.
	 */
	for (d = s->table; d; d = NEXT(d)) {
		if (d->merge) break;
		unless (TAG(d) || (d->flags & D_XFLAGS)) break;
	}
	unless (d) d = s->tree;		/* 1.0 has XFLAGS, so its skipped */
	ut.modtime = d->date - d->dateFudge - 2;

	/*
	 * In checkout:edit mode bk delta is like bk delta -l; so it will
	 * not touch the gfile.  So now s->table->date > gtime.
	 * We look for that and adjust: this will only roll back
	 */
	if (s->gtime && (s->gtime < (ut.modtime + 2))) {
		ut.modtime = s->gtime - 2;
	}
	/*
	 * if hard linked clone, and doing a get -T, don't pull the
	 * sfile modification time forward as that may mess things up
	 * in some other hardlinked clone which may have an older gfile.
	 */
	if (newest && (ut.modtime >= newest)) return (0);
	if (ut.modtime > (ut.actime - 2)) {
		ut.modtime = ut.actime - 2;	/* clamp to now */
	}
	return (utime(s->sfile, &ut));
}

/*
 * Diff can give me
 *	10a12, 14	-> 10 a
 *	10, 12d13	-> 10 d
 *	10, 12c12, 14	-> 10 c
 *
 * Diff -n can give me
 *	a10 2		-> 10 b 2
 *	d10 2		-> 10 e 2
 *
 * Mkpatch can give me
 *	I10 2		-> 10 I 2
 *	D10 2		-> 10 D 2
 */
private inline int
scandiff(char *s, int *where, char *what, int *howmany)
{
	if (!isdigit(*s)) { /* look for diff -n and mkpatch format */
		*what = *s;
		if (*s == 'a')
			*what = 'i';
		else if (*s == 'd')
			*what = 'x';
		else unless (*s == 'D' || *s == 'I' || *s == 'N')
			return (-1);
		s++;
		*where = atoi_p(&s);
		unless (*s == ' ')  return (-1);
		s++;
		*howmany = atoi_p(&s);
		return (0);
	}
	*howmany = 0;	/* not used by this part, but used by nonl fixer */
	*where = atoi_p(&s);
	if (*s == ',') {
		s++;
		(void)atoi_p(&s);
	}
	if (*s != 'a' && *s != 'c' && *s != 'd') {
		return (-1);
	}
	*what = *s;
	return (0);
}

/*
 * Convert ascii 1.2.3.4 -> 1, 2, 3, 4
 */
private int
scanrev(char *s, ser_t *a, ser_t *b, ser_t *c, ser_t *d)
{
	char	*p;

	/* make sure it's well formed or take nothing. */
	for (p = s; isdigit(*p) || (*p == '.'); p++);
	if (*p) return (0);

	*a = atoi_p(&s);
	if (b && *s == '.') {
		s++;
		*b = atoi_p(&s);
		if (c && *s == '.') {
			s++;
			*c = atoi_p(&s);
			if (d && *s == '.') {
				s++;
				*d = atoi(s);
				return (4);
			} else return (3);
		} else return (2);
	} else return (1);
}

private void
explode_rev(sccs *sc, delta *d, char *rev)
{
	if (strcnt(rev, '.') > 3) d->flags |= D_ERROR|D_BADFORM;
	switch (scanrev(rev, &d->r[0], &d->r[1], &d->r[2], &d->r[3])) {
	    case 1: d->r[1] = 0;	/* fall through */
	    case 2: d->r[2] = 0;	/* fall through */
	    case 3: d->r[3] = 0;
	}
}

char *
delta_rev(sccs *s, delta *d)
{
	dextra	*dx;
	char	buf[MAXREV];

	if (d->r[2]) {
		sprintf(buf, "%d.%d.%d.%d",
		    d->r[0], d->r[1], d->r[2], d->r[3]);
	} else {
		sprintf(buf, "%d.%d",
		    d->r[0], d->r[1]);
	}
	if (INARRAY(d)) {
		dx = EXTRA(s, d);
		unless (dx->rev && streq(buf, dx->rev)) {
			free(dx->rev);
			dx->rev = strdup(buf);
		}
		return (dx->rev);
	} else {
		/*
		 * Return non-strdup'd constant.  Any would do.
		 * It's from a patch and will get put into the array soon.
		 * The r[] is the definitive revision.
		 */
		return ("1.3.9.62"); 
	}
}

/*
 * Return the user for this delta.  Used by USER() macro
 */
char *
delta_user(sccs *s, delta *d)
{
	char	*u, *h;
	static	char	buf[MAXLINE];

	u = USERHOST(s, d);
	if (h = strchr(u, '@')) {
		strncpy(buf, u, h-u);
		buf[h-u] = 0;
		u = buf;
	}
	return (u);
}

/*
 * Return the hostname for this delta.  Used by HOSTNAME() macro
 */
char *
delta_host(sccs *s, delta *d)
{
	char	*h;

	if (d->userhost && (h = strchr(USERHOST(s, d), '@'))) {
		return (h+1);
	} else {
		return ("");
	}
}

/*
 * This one assumes SCCS style branch numbering, i.e., x.y.z.d
 */
private int
samebranch(delta *a, delta *b)
{
	if (!a->r[2] && !b->r[2]) return (1);
	return ((a->r[0] == b->r[0]) &&
		(a->r[1] == b->r[1]) &&
		(a->r[2] == b->r[2]));
}

/*
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * All of this pathname/changeset shit needs to be reworked.
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 */

void
sccs_mkroot(char *path)
{
	project	*proj;
	char	buf[MAXPATH];

	sprintf(buf, "%s/BitKeeper", path);
	if ((mkdir(buf, 0777) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/etc", path);
	if ((mkdir(buf, 0777) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	/*
	 * We have created a new repository, so clear the dir caches of
	 * the repository above this one in case any of these directories
	 * map to the wrong repository.
	 */
	concat_path(buf, path, "..");
	if (proj = proj_init(buf)) {
		proj_reset(proj);
		proj_free(proj);
	}

	sprintf(buf, "%s/SCCS", path);
	if ((mkdir(buf, 0777) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/etc/SCCS", path);
	if ((mkdir(buf, 0777) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/deleted", path);
	if ((mkdir(buf, 0777) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/deleted/SCCS", path);
	if ((mkdir(buf, 0777) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/tmp", path);
	if ((mkdir(buf, 0777) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/log", path);
	if ((mkdir(buf, 0777) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/.bk", path);
	if (isdir(buf)) {
		hide(buf, 1);
	}
}

/*
 * Reverse the effect of sccs_mkroot
 */
void
sccs_unmkroot(char *path)
{
	char	buf[MAXPATH];

	sprintf(buf, "%s/SCCS", path);
	if (rmdir(buf) == -1) {
		perror(buf);
	}

	sprintf(buf, "%s/BitKeeper/deleted/SCCS", path);
	if (rmdir(buf) == -1) {
		perror(buf);
	}
	sprintf(buf, "%s/BitKeeper/deleted", path);
	if (rmdir(buf) == -1) {
		perror(buf);
	}
	sprintf(buf, "%s/BitKeeper/tmp", path);
	if (rmdir(buf) == -1) {
		perror(buf);
	}
	sprintf(buf, "%s/BitKeeper/log", path);
	if (rmdir(buf) == -1) {
		perror(buf);
	}
	sprintf(buf, "%s/BitKeeper/etc/SCCS", path);
	if (rmdir(buf) == -1) {
		perror(buf);
	}
	sprintf(buf, "%s/BitKeeper/etc", path);
	if (rmdir(buf) == -1) {
		perror(buf);
	}
	sprintf(buf, "%s/BitKeeper", path);
	if (rmdir(buf) == -1) {
		perror(buf);
	}
}

/*
 * Return the pathname relative to the first ChangeSet file found.
 *
 * XXX - this causes a 5-10% slowdown, more if the tree is very deep.
 * I need to cache changeset lookups.
 */
char	*
_relativeName(char *gName, int isDir, int mustHaveRmarker, int wantRealName,
    project *proj)
{
	char	*t, *s;
	char	*root;
	int	len;
	project	*freeproj = 0;
	char	tmp[MAXPATH];
	static  char buf2[MAXPATH];

	fullname(gName, tmp);
	unless (IsFullPath(tmp)) return (0);
	tmp[strlen(tmp)+1] = 0;	/* for code below */
	t = tmp;

	unless (proj) proj = freeproj = proj_init(t);
	unless (proj) {
		if (mustHaveRmarker) return (0);
		strcpy(buf2, tmp);
		return (buf2);
	}

	root = proj_root(proj);
	len = strlen(root);
	assert(strneq(root, t, len));
	if (freeproj) proj_free(freeproj);
	s = &t[len];
	assert((*s == '\0') || (*s == '/'));

	/*
	 * Must cd to project root before we call getRealName()
	 */
	t[s-t] = 0; /* t now points to project root */
	if (wantRealName && proj_isCaseFoldingFS(proj)) {
		char	*here = strdup(proj_cwd());

		if (streq(here, t)) {
			getRealName(++s, NULL, buf2);
		} else if (chdir(t) == 0) {
			getRealName(++s, NULL, buf2);
			chdir(here);	/* restore pwd */
		} else {
			/*
			 * If chdir() filed
			 * skip getRealName()
			 */
			strcpy(buf2, ++s);
		}
		free(here);
	} else {
		strcpy(buf2, ++s);
	}

	if (isDir) {
		if (buf2[0] == 0) strcpy(buf2, ".");
		return (buf2);
	}
	return(buf2);
}


/*
 * Trim off the RESYNC/ part of the pathname, that's garbage.
 */
private char	*
relativeName(sccs *sc, int mustHaveRmarker, project *proj)
{
	char	*s, *g;

	g = sccs2name(sc->sfile);
	s = _relativeName(g, 0, mustHaveRmarker, 1, proj);
	free(g);
	unless (s) return (0);

	if (strncmp("RESYNC/", s, 7) == 0) s += 7;
	return (s);
}

private inline symbol *
findSym(sccs *s, char *name)
{
	symbol	*sym;

	unless (name) return (0);

	EACHP_REVERSE(s->symlist, sym) {
		if (streq(name, SYMNAME(s, sym))) return (sym);
	}
	return (0);
}

private inline int
samerev(ser_t a[4], ser_t b[4])
{
	return ((a[0] == b[0]) &&
		(a[1] == b[1]) &&
		(a[2] == b[2]) &&
		(a[3] == b[3]));
}

/*
 * Find the delta referenced by rev.  It must be an exact match.
 */
private delta *
rfind(sccs *s, char *rev)
{
	delta	*d;
	symbol	*sym;
	ser_t	R[4];

	debug((stderr, "rfind(%s) ", rev));
	unless (isdigit(rev[0])) {
		if (sym = findSym(s, rev)) return (SFIND(s, sym->ser));
		return (0);
	}
	R[0] = R[1] = R[2] = R[3] = 0;
	scanrev(rev, &R[0], &R[1], &R[2], &R[3]);
	debug((stderr, "aka %d.%d.%d.%d\n", R[0], R[1], R[2], R[3]));
	for (d = s->table; d; d = NEXT(d)) {
		unless (d->flags && !TAG(d)) continue;
		if (samerev(d->r, R)) return (d);
	}
	return (0);
}

private char *
defbranch(sccs *s)
{
	if (BITKEEPER(s)) return ("1");
	if (s->defbranch) return (s->defbranch);
	return ("65535");
}

/*
 * Find the specified delta.  The rev can be
 *	(null)	get top of the default branch
 *	d	get top of trunk for that release
 *	d.d	get that revision or error if not there
 *	d.d.d	get top of branch that matches those three digits
 *	d.d.d.d get that revision or error if not there.
 */
private delta *
findrev(sccs *s, char *rev)
{
	ser_t	a = 0, b = 0, c = 0, d = 0;
	ser_t	max = 0;
	delta	*e = 0, *f = 0;
	symbol	*sym;
	char	buf[20];

	debug((stderr,
	    "findrev(%s in %s def=%s)\n",
	    notnull(rev), s->sfile, defbranch(s)));
	unless (HASGRAPH(s)) return (0);
	if (!rev || !*rev || streq("+", rev)) {
		rev = defbranch(s);
	}

	/* 1.0 == s->tree even if s->tree is 1.1 */
	if (streq(rev, "1.0")) return (s->tree);

	if (*rev == '=') {
		e = sfind(s, atoi(++rev));
		unless (e) fprintf(stderr, "Serial %s not found\n", rev);
		return (e);
	}
	unless (isdigit(rev[0])) {
		if (sym = findSym(s, rev)) return (SFIND(s, sym->ser));
		return (0);
	}
	switch (scanrev(rev, &a, &b, &c, &d)) {
	    case 1:
		/* XXX: what does -r0 mean?? */
		unless (a) {
			fprintf(stderr, "Illegal revision 0\n");
			debug((stderr, " BAD rev %s\n", e->rev));
			return (0);
		}
		/* get max X.Y that is on same branch or tip of biggest */
		for (e = s->table; e; e = NEXT(e)) {
			if (e->flags & D_GONE) continue;
			if (TAG(e)
			    || (e->r[2] != 0)
			    || (e->r[0] > a))  continue;

			if (e->r[0] == a) break; /* first is it! */

			/* else save max of lesser releases */
			if (e->r[0] > max) {
				f = e;
				max = e->r[0];
			}
		}
		unless (e) e = f;	/* can't find, use max of lesser */
		debug((stderr, "findrev(%s) =  %s\n", rev, e ? e->rev: ""));
		return (e);
	    case 2:
	    case 4:
		e = rfind(s, rev);	/* well formed */
		debug((stderr,
		    "findrev(%s) =  %s\n", rev, e ? e->rev : "Not found"));
		return (e);
	    case 3:
		/* XXX - should this be a while loop, decrementing the
		 * third digit until it find it???  Nahh.
		 */
		sprintf(buf, "%s.1", rev);
		unless (e = rfind(s, buf)) { /* well formed */
			debug((stderr, "findrev(%s) =  Not found\n", rev));
			return (0);
		}
		/* find latest delta on same branch */
		for (f = s->table; f > e; f--) {
			if (samebranch(e, f)) {
				e = f;
				break;
			}
		}
		return (e);
	    default:
		fprintf(stderr, "Malformed revision: %s\n", rev);
		debug((stderr, " BAD %s\n", e->rev));
		return (0);
	}
}

/*
 * Return the top of the default branch.
 *
 * XXX - this could be much faster in big files by starting at the table
 * end and returning the first real delta which matches the default branch.
 */
delta	*
sccs_top(sccs *s)
{
	return (findrev(s,0));
}

/*
 * Compare two pathnames relative to the root of the repository and
 * return true if they are the same.  Either gfiles or sfiles can
 * be used.
 *
 * This is useful since some old repos have a sfile pathname stored
 * in d->pathname.
 */
int
sccs_patheq(char *file1, char *file2)
{
	int	s1, s2;
	int	ret;

	unless (ret = streq(file1, file2)) {
		/*
		 * if the files don't match check to see if one is a sfile
		 * and the other isn't
		 */
		s1 = (strstr(file1, "SCCS/s.") != 0);
		s2 = (strstr(file2, "SCCS/s.") != 0);
		if (s1 ^ s2) {
			/* normalize the mismatch case */
			file1 = name2sccs(file1);
			file2 = name2sccs(file2);
			ret = streq(file1, file2);
			free(file1);
			free(file2);
		}
	}
	return (ret);
}

delta	*
sccs_findrev(sccs *s, char *rev)
{
	delta	*d;
	project	*proj;
	char	*dk, *csetdk = 0;
	char	rk[MAXKEY];

	unless (rev && *rev) return (findrev(s, 0));
again:	if (rev[0] == '@') {
		if (rev[1] == '@') {
			if (rev[2] == '@') return (0);
			unless (proj_isComponent(s->proj)) {
				rev = rev+1;
				goto again;
			}
			proj = proj_product(s->proj);
			rev += 2;
			if (CSET(s)) goto atrev;
			rev = csetdk = proj_cset2key(proj,
			    rev, proj_rootkey(s->proj));
			if (rev) {
				proj = s->proj;
				goto atrev;
			} else {
				d = 0;
			}
		} else if (CSET(s) && !s->file) {
			++rev;
			goto again;
		} else {
			proj = s->proj;
			if (CSET(s) && s->file) proj = proj_product(proj);
			++rev;
atrev:			sccs_sdelta(s, sccs_ino(s), rk);
			if (dk = proj_cset2key(proj, rev, rk)) {
				d = sccs_findKey(s, dk);
				free(dk);
			} else {
				d = 0;
			}
			if (csetdk) free(csetdk);
		}
	} else if (isKey(rev)) {
		d = sccs_findKey(s, rev);
	} else if (!isalpha(rev[0])) {
		d = findrev(s, rev);
	} else if (!CSET(s) || proj_isComponent(s->proj)) {
		/* likely a tag, assume @@tag */
		sprintf(rk, "@@%s", rev);
		d = sccs_findrev(s, rk);
	} else {
		d = findrev(s, rev);
	}
	return (d);
}

/*
 * Take a date and return the delta closest to that date.
 *
 * The roundup parameter determines how to round inexact dates (ie 2001)
 * and which direction to look for deltas near that date.
 */
delta *
sccs_findDate(sccs *sc, char *s, int roundup)
{
	delta	*tmp, *d = 0;
	time_t	date;
	ser_t	x;

	unless (sc && sc->table) return (0);
	assert(s);
	assert((roundup == ROUNDUP) || (roundup == ROUNDDOWN));

	/*
	 * If it is a symbol, revision, or key,
	 * then just go get that delta and return it.
	 */
	if ((!isdigit(*s) && (*s != '-')) ||	/* not a date */
	    (scanrev(s, &x, &x, &x, &x) > 1) ||
	    isKey(s)) {
		return (sccs_findrev(sc, s));
		/* XXX what if findrev fails... */
	}

	/*
	 * It's a plain date.  Convert it to a number and then go
	 * find the closest delta.  If there is an exact match,
	 * return that.
	 * Depending on which call this, we think a little differently about
	 * endpoints.
	 * The order returned is rstart == 1.1 and rstop == 1.5, i.e.,
	 * oldest delta .. newest.
	 */
	if (*s == '-') {
		date = range_cutoff(s+1);
	} else {
		date = date2time(s, 0, roundup);
	}

	/* Walking the table newest .. oldest order */
	for (tmp = 0, d = sc->table; d; tmp = d, d = NEXT(d)) {
		if (TAG(d)) continue;
		if (d->date == date) return (d);
		/*
		 *                v date
		 * big date   1.4   1.3   1.2    little date
		 *             ^tmp  ^d
		 */
		if (d->date < date) break;
	}
	if (roundup == ROUNDDOWN) return (tmp);
	return (d);
}

private inline int
isbranch(delta *d)
{
	return (d->r[2]);
}

/*
 * Get the delta that is the basis for this edit.
 * Get the revision name of the new delta.
 * Sep 2000 - removed branch, we don't support it.
 */
delta *
sccs_getedit(sccs *s, char **revp)
{
	char	*rev = *revp;
	ser_t	a = 0, b = 0, c = 0, d = 0;
	delta	*e, *t;
	ser_t	R[4];
	static	char buf[MAXREV];

	debug((stderr, "sccs_getedit(%s, %s)\n", s->gfile, notnull(*revp)));
	/*
	 * use the findrev logic to get to the delta.
	 */
	e = findrev(s, rev);
	debug((stderr, "sccs_getedit(e=%s)\n", e?e->rev:""));

	unless (e) {
		/*
		 * Special case: we're at 1.x and they want 2.1 or 3.1.
		 * We allow that.
		 */
		scanrev(rev, &a, &b, &c, &d);
		if ((c == 0) && (b == 1)) {
			e = findrev(s, 0);
			assert(e);
			if (e->r[0] < a) goto ok;
		}
		return (0);
	}
ok:
	/*
	 * Just continue trunk/branch
	 * Because the kid may be a branch, we have to be extra careful here.
	 */
	for (t = e+1; t <= s->table; t++) {
		unless (t->flags && !TAG(t)) continue;
		if ((t->pserial == SERIAL(s, e)) && samebranch(t, e)) break;
	}
	if (t > s->table) {	/* no more kids of e */
		a = e->r[0];
		b = e->r[1];
		c = e->r[2];
		d = e->r[3];
		if (!c) {
			/* Seems weird but makes -e -r2 -> 2.1 when tot is 1.x
			 */
			int	release = rev ? atoi(rev) : 1;

			if (release > a) {
				a = release;
				b = 1;
			} else {
				b++;
			}
			sprintf(buf, "%d.%d", a, b);
		} else {
			sprintf(buf, "%d.%d.%d.%d", a, b, c, d+1);
		}
		debug((stderr, "sccs_getedit1(%s) -> %s\n", notnull(rev), buf));
		*revp = buf;
	} else {
		/*
		 * For whatever reason (they asked, or there is a kid in
		 * the way), we need a branch.  Branches are all based,
		 * in their /name/, off of the closest trunk node.  Go
		 * backwards up the tree until we hit the trunk and then
		 * use that rev as a basis.  Because all branches are
		 * below that trunk node, we don't have to search the
		 * whole tree.
		 */

		for (t = e; isbranch(t); t = PARENT(s, t));
		R[0] = t->r[0]; R[1] = t->r[1]; R[2] = 1; R[3] = 1;
again:		for (a = SERIAL(s, t)+1; a < s->nextserial; a++) {
			if (samerev(s->slist[a].r, R)) {
				/* found X.Y.Z.1 branch, try another */
				++R[2];
				goto again;
			}
		}
		sprintf(buf, "%d.%d.%d.%d", R[0], R[1], R[2], R[3]);
		*revp = buf;
	}
	return (e);
}

/* setup to call sccs_nextdata() */
void
sccs_rdweaveInit(sccs *s)
{
	zgetbuf	*zin;

	fseek(s->fh, s->data, SEEK_SET);
	if (GZIP(s)) {
		s->oldfh = s->fh;
		zin = zgets_initCustom(0, s->oldfh);
		s->fh = funopen(zin,
		    (int (*)(void*, char*, int))zread,
		    0, 0,
		    (int (*)(void*))zgets_done);
	}
}

/*
 * Returns a line from the sfile with the trailing NL stripped. It is
 * expected that all sfiles end in a trailing newline so this function
 * asserts if that is not true.  For any lines that contain a CRLF the
 * CR will not be stripped.  This is for compat with old broken
 * versions of bk.
 */
char *
sccs_nextdata(sccs *s)
{
	char	*buf;
	size_t	i;

	unless (buf = fgetln(s->fh, &i)) return (0);
	--i;
	unless (buf[i] == '\n') {
		fprintf(stderr, "error, truncated sccs file '%s', exiting.\n",
		    s->sfile);
		abort();
	}
	buf[i] = 0;
	return (buf);
}

/*
 * Cleans up gzip stuff.
 * fails if the whole weave was not read.
 */
int
sccs_rdweaveDone(sccs *s)
{
	int	c, ret = 0;

	unless ((c = fgetc(s->fh)) == EOF) {
		ungetc(c, s->fh);
		ret = 1;
	}
	if (GZIP(s)) {
		assert(s->oldfh);
		ret = fclose(s->fh);
		s->fh = s->oldfh;
		s->oldfh = 0;
	}
	return (ret);
}

/*
 * We are about to write a new sfile, open the x.file for writing and
 * return it.
 */
FILE *
sccs_startWrite(sccs *s)
{
	FILE	*sfile;
	char	*xfile;

	if (s->mem_out) {
		unless(s->outfh) s->outfh = fmem();
		assert(s->outfh);
		sfile = s->outfh;
	} else {
		xfile = sccsXfile(s, 'x');
		unless (sfile = fopen(xfile, "w")) perror(xfile);
	}
	unless (s->encoding_out) {
		s->encoding_out = sccs_encoding(s, 0, 0);
	}
	return (sfile);
}

/*
 * Bail, but clean up first.
 */
void
sccs_abortWrite(sccs *s, FILE **f)
{
	unless (s) return;
	if (*f) fclose(*f);	/* before the unlink */
	if (s->mem_out) {
		s->mem_in = 0;
		if (s->outfh) {
			if (*f != s->outfh) fclose(s->outfh);
			s->outfh = 0;
		}
		s->mem_out = 0;
	} else {
		unlink(sccsXfile(s, 'x'));
	}
	*f = 0;
	sccs_close(s);
}

/*
 * We have finished writing a new x.file and now it is time to update
 * the sfile.
 */
int
sccs_finishWrite(sccs *s, FILE **f)
{
	char	*xfile = sccsXfile(s, 'x');
	int	rc;
 	FILE	*tmp;

	unless (BFILE_OUT(s)) {
		fseek(*f, 0L, SEEK_SET);
		fprintf(*f, "\001%c%05u\n", BITKEEPER(s) ? 'H' : 'h', s->cksum);
	}
	rc = ferror(*f);
	if (s->mem_out) {
		tmp = s->fh;
		assert(s->state & S_SOPEN);
		s->fh = s->outfh;
		rewind(s->fh);
		if (s->mem_in) {
			s->outfh = tmp;
			ftrunc(s->outfh, 0);
		} else {
			fclose(tmp);
			s->outfh = fmem();
			s->mem_in = 1;
		}
		*f = 0;
	} else {
		if (fclose(*f)) rc = -1;
		*f = 0;
		if (rc) {
			perror(xfile);
			goto out;
		}
		sccs_close(s);
		if (rename(xfile, s->sfile)) {
			fprintf(stderr,
			    "can't rename(%s, %s) left in %s\n",
			    xfile, s->sfile, xfile);
			return (-1);
		}
		assert(size(s->sfile) > 0);
		/* Always set the time on the s.file behind the g.file or now */
		if (sccs_setStime(s, 0)) perror(s->sfile);
		if (chmod(s->sfile, 0444)) perror(s->sfile);
	}
	s->encoding_in = s->encoding_out;
out:	return (rc);
}

/*
 * Look for a bug in the weave that could be created by bitkeeper
 * versions 3.0.1 or older.  It happens when a file is delta'ed on
 * Windows when it ends in a bare \r without a trailing \n.
 *
 * This function gets run from check.c if checksums are being verified
 * right after we have walked the weave to check checksums.  If a
 * 'no-newline' marker is seen in the file then get_reg() sets
 * s->has_nonl.  And we skip this call if s->has_nonl is not set, that
 * way the weave is only extracted twice on a subset of the files.
 *
 * This code is a simple pattern match that looks for a blank line
 * followed by end marker with a no-newline flag.
 *
 * The function should be removed after all customers have upgraded
 * beyond 3.0.1.
 */
int
chk_nlbug(sccs *s)
{
	char	*buf, *p;
	int	sawblank = 0;
	int	ret = 0;

	sccs_rdweaveInit(s);
	while (buf = sccs_nextdata(s)) {
		if (buf[0] == '\001' && buf[1] == 'E') {
			p = buf + 3;
			while (isdigit(*p)) p++;
			if (*p == 'N' && sawblank) {
				getMsg("saw_blanknonl", s->sfile, 0, stderr);
				ret = 1;
			}
		}
		sawblank = (buf[0] == 0);
	}
	if (sccs_rdweaveDone(s)) ret = 1;
	return (ret);
}

/*
 * This does standard SCCS expansion, it's almost 100% here.
 * New stuff added:
 * %@%	user@host
 */
private char *
expand(sccs *s, delta *d, char *l, int *expanded)
{
	char 	*buf;
	char	*t;
	char	*tmp;
	time_t	now = 0;
	ser_t	a[4] = {0, 0, 0, 0};
	int hasKeyword = 0, buf_size;
#define MORE 1024

	a[0] = a[1] = a[2] = a[3] = 0;
	/* pre scan the line to determine if it needs keyword expansion */
	*expanded = 0;
	for (t = l; *t; t++) {
		if (hasKeyword) continue;
		unless ((t[0] == '%') && t[1] && (t[2] == '%')) continue;
		/* NOTE: this string *must* match the case label below */
		if (strchr("ABCDEFGHIKLMPQRSTUWYZ@", t[1])) hasKeyword = 1;
	}
	unless (hasKeyword) return (l);
	buf_size = t - l + MORE; /* get extra memory for keyword expansion */

	/* ok, we need to expand keyword, allocate a new buffer */
	t = buf = malloc(buf_size);
	a[0] = a[1] = a[2] = a[3] = 0;	/* bogus gcc 4 warning */
	while (*l) {
		unless ((l[0] == '%') && l[1] && (l[2] == '%')) {
			*t++ = *l++;
			continue;
		}
		switch (l[1]) {
		    case 'A':	/* %Z%%Y% %M% %I%%Z% */
			/* XXX - no tflag */
			strcpy(t, "@(#) "); t += 5;
			tmp = basenm(s->gfile);
			strcpy(t, tmp); t += strlen(tmp); *t++ = ' ';
			strcpy(t, REV(s, d)); t += strlen(REV(s, d));
			strcpy(t, "@(#)"); t += 4;
			break;

		    case 'B':	/* 1.2.3.4 -> 3 (branch name) */
			t += sprintf(t, "%d", d->r[2]);
			break;

		    case 'C':	/* line number - XXX */
			*t++ = '%'; *t++ = 'C'; *t++ = '%';
			break;

		    case 'D':	/* today: 97/06/22 */
			unless (now) now = time(0);
			t += strftime(t, 12,
			    YEAR4(s) ? "%Y/%m/%d" : "%y/%m/%d",
			    localtimez(&now, 0));
			break;

		    case 'E': 	/* most recent delta: 97/06/22 */
			t += delta_strftime(t, 12,
			    YEAR4(s) ? "%Y/%m/%d" : "%y/%m/%d",
			    s, d);
			break;

		    case 'F':	/* s.file name */
			strcpy(t, "SCCS/"); t += 5;
			tmp = basenm(s->sfile);
			strcpy(t, tmp); t += strlen(tmp);
			break;

		    case 'G':	/* most recent delta: 06/22/97 */
			t += delta_strftime(t, 12,
			    YEAR4(s) ? "%m/%d/%Y" : "%m/%d/%y",
			    s, d);
			break;

		    case 'H':	/* today: 06/22/97 */
			unless (now) now = time(0);
			t += strftime(t, 12,
			    YEAR4(s) ? "%m/%d/%Y" : "%m/%d/%y",
			    localtimez(&now, 0));
			break;

		    case 'I':	/* name of revision: 1.1 or 1.1.1.1 */
			strcpy(t, REV(s, d)); t += strlen(REV(s, d));
			break;

		    case 'K':	/* BitKeeper Key */
		    	t += sccs_sdelta(s, d, t);
			break;

		    case 'L':	/* 1.2.3.4 -> 2 */
			t += sprintf(t, "%d", d->r[1]);
			break;

		    case 'M':	/* mflag or filename: slib.c */
			tmp = basenm(s->gfile);
			strcpy(t, tmp); t += strlen(tmp);
			break;

		    case 'P':	/* full: /u/lm/smt/sccs/SCCS/s.slib.c */
			fullname(s->sfile, t);
			t += strlen(t);
			break;

		    case 'Q':	/* qflag */
			*t++ = '%'; *t++ = 'Q'; *t++ = '%';
			break;

		    case 'R':	/* release 1.2.3.4 -> 1 */
			t += sprintf(t, "%d", d->r[0]);
			break;

		    case 'S':	/* rev number: 1.2.3.4 -> 4 */
			t += sprintf(t, "%d", d->r[3]);
			break;

		    case 'T':	/* time: 23:04:04 */
			unless (now) now = time(0);
			t += strftime(t, 10, "%H:%M:%S", localtimez(&now, 0));
			break;

		    case 'U':	/* newest delta: 23:04:04 */
			t += delta_strftime(t, 10, "%H:%M:%S", s, d);
			break;

		    case 'W':	/* @(#)%M% %I%: @(#)slib.c 1.1 */
			strcpy(t, "@(#) "); t += 4;
			tmp = basenm(s->gfile);
			strcpy(t, tmp); t += strlen(tmp); *t++ = ' ';
			strcpy(t, REV(s, d)); t += strlen(REV(s, d));
			break;

		    case 'Y':	/* tflag */
			*t++ = '%'; *t++ = 'Y'; *t++ = '%';
			break;

		    case 'Z':	/* @(#) */
			strcpy(t, "@(#)"); t += 4;
			break;

		    case '@':	/* user@host */
			strcpy(t, USERHOST(s, d));
			t += strlen(USERHOST(s, d));
			break;

		    case '#':	/* user */
			strcpy(t, USER(s, d));
			t += strlen(USER(s, d));
			break;

		    default:
			*t++ = *l++;
			continue;
		}
		l += 3;
	}
	*t = 0;
	assert((t - buf) <= buf_size);
	*expanded = 1;  /* Note: if expanded flag is set	 */
					/* caller must free buffer when done */			
	return (buf);
}

/*
 * This does standard RCS expansion.
 *
 * This code recognizes RCS keywords and expands them.  This code can
 * handle finding a keyword that is already expanded and reexpanding
 * it to the correct value.  This is for when a RCS file gets added
 * with the keywords already expanded.
 *
 * XXX In BitKeeper the contents of the weave and edited files are is
 * normally not expanded. (ie: $Date$) However mistakes can happen.
 * We should unexpand RCS keywords on delta if they happen...
 */
private char *
rcsexpand(sccs *s, delta *d, char *line, int *expanded)
{
	static const struct keys {
		int	len;
		char	*keyword;
		char	*dspec;
	} keys[] = {
		/*   123456789 */
		{6, "Author", ": :USER:@:HOST: "},
		{4, "Date", ": :D: :T::TZ: "},
		{6, "Header", ": :GFILE: :I: :D: :T::TZ: :USER:@:HOST: "},
		{2, "Id", ": :G: :I: :D: :T::TZ: :USER:@:HOST: "},
		{6, "Locker", ": <Not implemented> "},
		{3, "Log", ": <Not implemented> "},
		{4, "Name", ": <Not implemented> "}, /* almost :TAG: */
		{8, "Revision", ": :I: "},
		{7, "RCSfile", ": s.:G: "},
		{6, "Source", ": :SFILE: "},
		{5, "State", ": <unknown> "}
	};
	const	int	keyslen = sizeof(keys)/sizeof(struct keys);
	char	*p, *prs;
	char	*out = 0;
	char	*outend = 0;
	char	*last = line;
	char	*ks;	/* start of keyword. */
	char	*ke;	/* byte after keyword name */
	int	i;

	*expanded = 0;
	p = line;
	while (*p) {
		/* Look for keyword */
		while (*p && (*p != '$')) p++;
		unless (*p) break;
		ks = ++p;  /* $ */
		while (isalpha(*p)) p++;
		if (*p == '$') {
			ke = p;
		} else if (*p == ':') {
			ke = p;
			while (*p && (*p != '$')) p++;
			unless (*p) break;
		} else {
		        continue;
		}
		/* found something that matches the pattern */
		for (i = 0; i < keyslen; i++) {
			if (keys[i].len == (ke-ks) &&
			    strneq(keys[i].keyword, ks, ke-ks)) {
				break;
			}
		}
		if (i == keyslen) continue; /* try again */

		unless (out) {
			char	*nl = p;
			while (*nl) nl++;
			outend = out = malloc(nl - line + MORE);
		}
		*expanded = 1;
		while (last < ke) *outend++ = *last++;
		prs = sccs_prsbuf(s, d, 0, keys[i].dspec);
		strcpy(outend, prs);
		free(prs);
		while (*outend) outend++;
		last = p;
		++p;
	}
	if (out) {
		while (last <= p) *outend++ = *last++;
		*outend = 0;
	} else {
		out = line;
	}
	return (out);
}

/*
 * We don't consider EPIPE an error since we hit it when we do stuff like
 * get -p | whatever
 * and whatever exits first.
 * XXX - on linux, at least, this doesn't work, so we catch the EPIPE case
 * where we call this function.  There should be only one place in the
 * get_reg() function.
 */
private int
flushFILE(FILE *out)
{
	if (fflush(out) && (errno != EPIPE)) {
		return (errno);
	}
	return (ferror(out));
}

/*
 * We couldn't initialize an SCCS file.	 Tell the user why not.
 * Possible reasons we handle:
 *	no s.file
 *	bad checksum
 *	couldn't build the graph
 *
 * XXX - use this for 100% of the error messages.
 */
void
sccs_whynot(char *who, sccs *s)
{
	if (s->io_error && s->io_warned) return;
	if (BEEN_WARNED(s)) return;
	unless (HAS_SFILE(s)) {
		fprintf(stderr, "%s: No such file: %s\n", who, s->sfile);
		return;
	}
	unless (s->cksumok) {
		fprintf(stderr, "%s: bad checksum in %s\n", who, s->sfile);
		return;
	}
	unless (HASGRAPH(s)) {
		fprintf(stderr, "%s: couldn't decipher delta table in %s\n",
		    who, s->sfile);
		return;
	}
	if (HAS_ZFILE(s)) {
		fprintf(stderr, "%s: %s is zlocked\n", who, s->gfile);
		return;
	}
	if (HAS_PFILE(s)) {
		fprintf(stderr, "%s: %s is edited\n", who, s->gfile);
		return;
	}
	if (errno == ENOSPC) {
		fprintf(stderr, "No disk space for %s\n", s->sfile);
		return;
	}
	fprintf(stderr, "%s: %s: unknown error.\n", who, s->sfile);
}

/*
 * Add this meta delta into the symbol graph.
 * Set this symbol's leaf flag and clear the parent's.
 */
private void
symGraph(sccs *s, delta *d)
{
	delta	*p;

	if (getenv("_BK_NO_TAG_GRAPH")) {
		assert(!SYMGRAPH(d));
		assert(!SYMLEAF(d));
		assert(!d->ptag);
		assert(!d->mtag);
		return;
	}
	if (SYMGRAPH(d)) return;
	for (p = s->table; p && !SYMLEAF(p); p = NEXT(p));
	if (p) {
		d->ptag = SERIAL(s, p);
		p->flags &= ~D_SYMLEAF;
	}
	d->flags |= D_SYMGRAPH | D_SYMLEAF;
}

/*
 * For each symbol saved by symArg() call addsym() to create the
 * entry.
 *
 * Called at the end of mkgraph(), not used with binary sfiles
 */
private void
metaSyms(sccs *sc)
{
	delta	*d;
	int	i;
	char	*symname;

	for (i = nLines(sc->mg_symname)-1; i > 0; i -= 2) {
		d = SFIND(sc, sc->mg_symname[i]);
		symname = sc->heap.buf + sc->mg_symname[i+1];
		addsym(sc, d, 0, symname);
	}
	FREE(sc->mg_symname);
}

int
sccs_tagleaves(sccs *s, delta **l1, delta **l2)
{
	delta	*d;
	symbol	*sym, *iter;
	int	first = 1;
	char	*arev = 0;
	char	*brev = 0;
	char	*aname, *bname;

	/*
	 * This code used to walk the symbol table list but that doesn't
	 * work because a tag graph node might not have a symbol; tag
	 * automerges do not have symbols.  So there is no entry in the
	 * symbol table.
	 */
	aname = bname = "?";
	for (d = s->table; d; d = NEXT(d)) {
		unless (SYMLEAF(d)) continue;
		sym = 0;
		EACHP_REVERSE(s->symlist, iter) {
			if (iter->meta_ser == SERIAL(s, d)) {
				sym = iter;
				break;
			}
		}
		unless (*l1) {
			arev = REV(s, d);
			if (sym) aname = SYMNAME(s, sym);
			*l1 = d;
			continue;
		}
		unless (*l2) {
			brev = REV(s, d);
			if (sym) bname = SYMNAME(s, sym);
			*l2 = d;
			continue;
		}
		if (first) {
			fprintf(stderr,
			    "Unmerged tag tips:\n"
			    "\t%-16s %s\n\t%-16s %s\n\t%-16s %s\n",
			    arev, aname, brev, bname,
			    REV(s, d), sym ? SYMNAME(s, sym) : "<tag merge>");
		    	first = 0;
		} else {
			fprintf(stderr,
			    "\t%-16s %s\n",
			    REV(s, d), sym ? SYMNAME(s, sym) : "<tag merge>");
		}
	}
	return (!first);	/* first == 1 means no errors */
}

/* make a meta delta in the RESYNC directory, storing in csets-in */
private	int
resyncMeta(sccs *s, delta *d, char *buf)
{
	sccs	*s2;
	MMAP	*m;
	FILE	*f;
	char	key[MAXKEY];

	m = mrange(buf, buf + strlen(buf), "");
	/* note: this rewrites the s.file, and does a sccs_close() */
	if (sccs_meta("resolve", s, d, m, 1)) {
		fprintf(stderr, "resolve: failed to make tag merge\n");
		return (1);
	}

	/* If called out of RESYNC, save in csets-in */
	/* XXX: could assert it is in RESYNC */
	unless (proj_isResync(s->proj)) return (0);
	unless (s2 = sccs_init(s->sfile, s->initFlags)) {
err:		if (s2) sccs_free(s2);
		return (1);
	}
	unless (SYMGRAPH(s2->table)) {
		fprintf(stderr, "resolve: new tip is not a tag merge\n");
		goto err;
	}
	sccs_sdelta(s, s->table, key);
	sccs_free(s2);
	unless (f = fopen(CSETS_IN, "a")) {
		fprintf(stderr,
		    "resolve: adding tag merge, csets-in did not open\n");
		perror("resolve");
		goto err;
	}
	fprintf(f, "%s\n", key);
	fclose(f);
	return (0);
}

/*
 * Add a merge delta which closes the tag graph.
 */
int
sccs_tagMerge(sccs *s, delta *d, char *tag)
{
	delta	*l1 = 0, *l2 = 0;
	char	*buf;
	char	k1[MAXKEY], k2[MAXKEY];
	time_t	tt = time(0);
	int	len, rc;

	if (sccs_tagleaves(s, &l1, &l2)) assert("too many tag leaves" == 0);
	assert(l1 && l2);
	/*
	 * If we are automerging, then use the later of the two tag tips.
	 */
	unless (d) {
		assert(tag == 0);
		d = (l1->date > l2->date) ? l1 : l2;
	}
	sccs_sdelta(s, l1, k1);
	sccs_sdelta(s, l2, k2);
	len = strlen(k1) + strlen(k1) + 2000;
	if (tag) len += strlen(tag);
	buf = malloc(len);
	/* XXX - if we ever remove |ChangeSet| from the keys fix P below */
	sprintf(buf,
	    "M 0.0 %s%s %s@%s 0 0 0/0/0\nP ChangeSet\n"
	    "%s%s%ss g\ns l\ns %s\ns %s\n%s\n",
	    time2date(tt), sccs_zone(tt), sccs_user(), sccs_host(),
	    tag ? "S " : "",
	    tag ? tag : "",
	    tag ? "\n" : "",
	    k1, k2,
	    "------------------------------------------------");
	assert(strlen(buf) < len);
	rc = resyncMeta(s, d, buf);
	free(buf);
	return (rc);
}

/*
 * Add another tag entry to the delta but do not close the graph, this
 * is what we call when we have multiple tags, the last tag calls the
 * tagMerge.
 */
int
sccs_tagLeaf(sccs *s, delta *d, delta *md, char *tag)
{
	char	*buf;
	char	k1[MAXKEY];
	time_t	tt = time(0);
	int	rc;
	delta	*l1 = 0, *l2 = 0;

	if (sccs_tagleaves(s, &l1, &l2)) assert("too many tag leaves" == 0);
	sccs_clearbits(s, D_RED);
	sccs_tagcolor(s, l1);
	if (md->flags & D_RED) {
		md = l1;
	} else {
		md = l2;
	}
	assert(md);
	sccs_sdelta(s, md, k1);
	assert(tag);
	buf = aprintf("M 0.0 %s%s %s@%s 0 0 0/0/0\nS %s\ns g\ns l\ns %s\n%s\n",
	    time2date(tt), sccs_zone(tt), sccs_user(), sccs_host(),
	    tag,
	    k1,
	    "------------------------------------------------");
	rc = resyncMeta(s, d, buf);
	free(buf);
	return (rc);
}

/*
 * This is called after sccs_tagcolor() which colors all nodes
 * related to a node.  It colors them RED and BLUE.
 * 
 * This code is used to uncolor the RED nodes that lie in the
 * intersection of histories of 2 different leaves.
 * If it finds a RED and BLUE, it uncolors RED.
 * If it find a BLUE but not RED, then it uncolors the BLUE.
 * For this trick to work, we need to go in table order rather
 * than recurse, because doing this, when we uncolor BLUE, we
 * know that we will not need to use it again.  The same is not
 * true if we did recurse.
 *
 * When calling sccs_tagcolor again, it will only color
 * RED and BLUE the nodes not in the intersection.
 */
private void
taguncolor(sccs *s, delta *d)
{
	assert(d);
	d->flags |= D_BLUE;
	for (; d; d = NEXT(d)) {
		unless (d->flags & D_BLUE) continue;
		if (d->flags & D_RED) {
			d->flags &= ~D_RED;
			continue;
		}
		if (d->ptag) sfind(s, d->ptag)->flags |= D_BLUE;
		if (d->mtag) sfind(s, d->mtag)->flags |= D_BLUE;
		d->flags &= ~D_BLUE;
	}
}

/*
 * Return an MDBM ${value} = rev,rev
 * for each value that was added by both the left and the right.
 */
MDBM	*
sccs_tagConflicts(sccs *s)
{
	MDBM	*db = 0;
	delta	*l1 = 0, *l2 = 0, *md;
	symbol	*sy1, *sy2;
	u8	*left, *right;
	int	i, j;
	char	buf[MAXREV*3];

	if (sccs_tagleaves(s, &l1, &l2)) assert("too many tag leaves" == 0);
	unless (l2) return (0);

	left = calloc(nLines(s->symlist)+1, sizeof(u8));
	right = calloc(nLines(s->symlist)+1, sizeof(u8));

	/* We always return an MDBM even if it is just an automerge case
	 * with nothing to merge.
	 */
	unless (db) db = mdbm_mem();
	sccs_tagcolor(s, l1);
	taguncolor(s, l2);	/* uncolor the intersection */
	EACH_REVERSE(s->symlist) {
		sy1 = &s->symlist[i];
		md = SFIND(s, sy1->meta_ser);
		unless (md->flags & D_RED) continue;
		md->flags &= ~D_RED;
		left[i] = 1;
	}
	sccs_tagcolor(s, l2);
	EACH_REVERSE(s->symlist) {
		sy1 = &s->symlist[i];
		md = SFIND(s, sy1->meta_ser);
		unless (md->flags & D_RED) continue;
		md->flags &= ~D_RED;
		right[i] = 1;
	}

	/*
	 * OK, for each symbol only in the left, see if there is one of the
	 * same name only in the right.
	 *
	 * Imagine a tree like so:
	 *	[ 1.5, "foo" ]
	 *	[ 1.6, "foo" ]	[ 1.5.1.1, "foo" ]
	 *	[ 1.7, "foo" ]
	 *
	 * We want to store 1.7,1.5.1.1 as the conflict.
	 */
	EACH_REVERSE(s->symlist) {
		unless (left[i] && !right[i]) continue;
		sy1 = &s->symlist[i];
		EACH_REVERSE_INDEX(s->symlist, j) {
			sy2 = &s->symlist[j];
			unless (right[j] && !left[j] &&
			    (streq(SYMNAME(s, sy1), SYMNAME(s, sy2)))) {
			    	continue;
			}
			/*
			 * Quick check to see if they added the same symbol
			 * twice to the same rev.
			 */
			if (sy1->ser == sy2->ser) continue;
			/*
			 * OK, we really have a conflict, save it.
			 * If it is already there, make sure that our version
			 * has later serials on both sides.
			 */
			sprintf(buf,
			    "%d %d", sy1->meta_ser, sy2->meta_ser);
			if (mdbm_store_str(db,
			    SYMNAME(s, sy1), buf, MDBM_INSERT)) {
				char	*old;
				int	a = 0, b = 0;

				old  = mdbm_fetch_str(db, SYMNAME(s, sy1));
				assert(old);
				sscanf(old, "%d %d", &a, &b);
				assert(a && b);
				if ((a > sy1->meta_ser) ||
				    (b > sy2->meta_ser)) {
				    	continue;
				}
				mdbm_store_str(db,
				    SYMNAME(s, sy1), buf, MDBM_REPLACE);
		    	}
		}
	}
	free(left);
	free(right);
	return (db);
}

private void
unvisit(sccs *s, delta *d)
{
	unless (d) return;
	unless (d->flags & D_BLUE) return;
	d->flags &= ~D_BLUE;

	if (d->ptag) unvisit(s, sfind(s, d->ptag));
	if (d->mtag) unvisit(s, sfind(s, d->mtag));
}

private delta *
tagwalk(sccs *s, delta *d)
{
	unless (d) return ((delta*)1);	/* this is an error case */

	/* note that the stripdel path has used D_RED */
	if (d->flags & D_BLUE) return(0);
	d->flags |= D_BLUE;

	if (d->ptag) if (tagwalk(s, sfind(s, d->ptag))) return (d);
	if (d->mtag) if (tagwalk(s, sfind(s, d->mtag))) return (d);
	return (0);
}

private int
checktags(sccs *s, delta *leaf, int flags)
{
	delta	*d, *e;

	unless (leaf) return(0);
	unless (d = tagwalk(s, leaf)) {
		unvisit(s, leaf);
		return (0);
	}
	if (d == (delta*)1) {
		verbose((stderr,
		    "Corrupted tag graph in %s\n", s->gfile));
		return (1);
	}
	unless (e = sfind(s, d->ptag)) {
		verbose((stderr,
		    "Cannot find serial %u, tag parent for %s:%u, in %s\n",
			d->ptag, REV(s, d), SERIAL(s, d), s->gfile));
	} else {
		assert(d->mtag);
		verbose((stderr,
		    "Cannot find serial %u, tag parent for %s:%u, in %s\n",
			d->mtag, REV(s, d), SERIAL(s, d), s->gfile));
	}
	return (1);
}

int
sccs_badTag(char *me, char *tag, int flags)
{
	char	*p;

	if (isdigit(*tag)) {
		verbose((stderr,
		    "%s: %s: tags can't start with a digit.\n", me, tag));
		return (1);
	}
	switch (*tag) {
	    case '@':
	    case '=':
	    case '-':
	    case '+':
	    case '.':
		verbose((stderr,
		    "%s: %s: tags can't start with a '%c'.\n", me, tag, *tag));
		return (1);
	}
	if (strstr(tag, "..")) {
		verbose((stderr,
		    "%s: tag %s cannot contain '..'\n", me, tag));
		return (1);
	}
	if (strstr(tag, ".,")) {
		verbose((stderr,
		    "%s: tag %s cannot contain '.,'\n", me, tag));
		return (1);
	}
	if (strstr(tag, ",.")) {
		verbose((stderr,
		    "%s: tag %s cannot contain ',.'\n", me, tag));
		return (1);
	}
	if (strstr(tag, ",,")) {
		verbose((stderr,
		    "%s: tag %s cannot contain ',,'\n", me, tag));
		return (1);
	}
	p = tag;
	while (*p) {
		switch (*p++) {
		    case '\001':
		    case '|':
		    case '\n':
		    case '\r':
			verbose((stderr,
			    "%s: tag %s cannot contain \"^A,|\\n\\r\"\n",
			    me, tag));
			return (1);
		}
	}
	return (0);
}

/*
 * Check tag graph integrity.
 */
private int
checkTags(sccs *s, int flags)
{
	delta	*l1 = 0, *l2 = 0;
	symbol	*sym;
	int	bad = 0;

	/* Nobody else has tags */
	unless (CSET(s)) return (0);

	/* Make sure that tags don't contain weird characters */
	EACHP_REVERSE(s->symlist, sym) {
		unless (sym->symname) continue;
		/* XXX - not really "check" all the time */
		if (sccs_badTag("check", SYMNAME(s, sym), flags)) bad = 1;
	}
	if (bad) return (128);

	if (sccs_tagleaves(s, &l1, &l2)) return (128);
	if (checktags(s, l1, flags) || checktags(s, l2, flags)) return (128);

	return (0);
}

/*
 * Dig meta data out of a delta.
 * The buffer looks like ^Ac<T>data where <T> is one character type.
 * A - hash for BAM
 * B - cset file root key
 * C - cset boundry
 * D - dangling delta
 * E - ??
 * F - date fudge
 * H - host name
 * K - delta checksum
 * M - merge parent
 * P - pathname
 * O - permissions.
 * R - random bits (1.0 only)
 * S - pre tag symbol
 * T - text description
 * V - file format version (1.0 delta only)
 * X - Xflags
 * Z - zone (really offset from GMT)
 *
 * This function is only used by mkgraph()
 */
private void
meta(sccs *s, delta *d, char *buf)
{
	if (TAG(d)) d->flags |= D_META;
	switch (buf[2]) {
	    case 'A':
		hashArg(s, d, &buf[3]);
		break;
	    case 'B':
		csetFileArg(s, d, &buf[3]);
		break;
	    case 'C':
		d->flags |= D_CSET;
		break;
	    case 'D':
		d->flags |= D_DANGLING;
		break;
	    case 'E':
		/* OLD, ignored */
		break;
	    case 'F':
		/* Do not add to date here, done in inherit */
		d->dateFudge = atoi(&buf[3]);
		break;
	    case 'H':
		hostArg(s, d, &buf[3]);
		break;
	    case 'K':
		sumArg(d, &buf[3]);
		break;
	    case 'M':
		mergeArg(d, &buf[3]);
		break;
	    case 'P':
		pathArg(s, d, &buf[3]);
		break;
	    case 'O':
		modeArg(s, d, &buf[3]);
		break;
	    case 'R':
		randomArg(s, d, &buf[3]);
		break;
	    case 'S':
		symArg(s, d, &buf[3]);
		break;
	    case 'T':
		assert(d);
		/* ignored, used to be d->text */
		break;
	    case 'V':
		s->version = atoi(&buf[3]);
		unless (s->version <= SCCS_VERSION) {
			fprintf(stderr,
			    "Later file format version %d, forcing read only\n",
			    s->version);
			s->state |= S_READ_ONLY;
		}
		break;
	    case 'X':
		assert(d);
		d->xflags = strtol(&buf[3], 0, 0); /* hex or dec */
		/* reenable longkeys for old KEY_FORMAT2 trees */
		if (CSET(s) && (s->xflags & X_LONGKEY)) d->xflags |= X_LONGKEY;
		d->xflags &= ~X_SINGLE; /* clear old single_user bit */
		d->flags |= D_XFLAGS;
		break;
	    case 'Z':
		zoneArg(s, d, &buf[3]);
		break;
	    default:
		fprintf(stderr, "%s: Ignoring %.5s\n", s->gfile, buf);
		/* got unknown field, force read only mode */
		s->state |= S_READ_ONLY;
		return;
	}
	s->bitkeeper = 1;
}

/*
 * Find the next delta in linear table order.
 * If you pass in 1.10, this should give you 1.11.
 */
delta	*
sccs_prev(sccs *s, delta *d)
{
	unless (s && d) return (0);
	for (d = d+1; d <= s->table; d++) {
		if (d->flags) return (d);
	}
	return (0);
}

private	void
bin_header(char *header, u32 *off_h, u32 *heapsz,
    u32 *off_d, u32 *deltasz, u32 *off_s, u32 *symsz)
{
	sscanf(header, "B %x/%x %x/%x %x/%x",
	    off_h, heapsz, off_d, deltasz, off_s, symsz);
}

private	off_t
bin_data(char *header)
{
	u32	ignore, off_s, symsz;

	bin_header(header,
	    &ignore, &ignore, &ignore, &ignore, &off_s, &symsz);
	
	return (off_s + symsz);	/* weave after sym table */
}

private void
bin_mkgraph(sccs *s, char *header)
{
	u32	heapsz, deltasz, symsz;
	u32	off_h, off_d, off_s;
	int	deltas;
	int	line = 1, len;
	char	*perfile;
	MMAP	*pf;

	bin_header(header, &off_h, &heapsz, &off_d, &deltasz, &off_s, &symsz);
	s->data = off_s + symsz;	/* weave after sym table */

	perfile = malloc(off_h);
	len = off_h - strlen(header);
	fread(perfile, 1, len, s->fh);
	pf = mrange(perfile, perfile+len, "b");
	unless (sccs_getperfile(s, pf, &line)) {
		fprintf(stderr, "%s: failed to load %s\n", prog, s->sfile);
		exit(1);
	}
	mclose(pf);
	free(perfile);
	s->bitkeeper = 1;

	data_resize(&s->heap, heapsz);
	s->heap.len = heapsz;
	s->mapping = addLine(s->mapping,
	    datamap(s->heap.buf, s->heap.len, s->fh, off_h));

	deltas = deltasz / sizeof(delta);
	s->nextserial = deltas + 1;
	s->tree = growArray(&s->slist, deltas);
	growArray(&s->extra, deltas);
	s->table = s->slist + nLines(s->slist);

	s->mapping = addLine(s->mapping,
	    datamap(SFIND(s, 1), deltasz, s->fh, off_d));

	growArray(&s->symlist, symsz/sizeof(symbol));
	s->mapping = addLine(s->mapping,
	    datamap(s->symlist+1, symsz, s->fh, off_s));

	/* XXX should move to common code. */
	unless (CSET(s)) s->file = 1;
	if (CSET(s) &&
	    proj_isComponent(s->proj) &&
	    proj_isProduct(0)) {
	    	s->file = 1;
    	}
}

/*
 * This is the NEXT(d) macro.
 * Move to the next older delta in table order.
 */
delta *
slist_next(delta *d)
{
	unless (d->pserial) return (0);
	while (1) {
		--d;
		if (d->flags) return (d);
	}
}

/*
 * Read in an sfile and build the delta table graph.
 * Scanf caused problems here when there are corrupted files.
 */
private void
mkgraph(sccs *s, int flags)
{
	delta	*d = 0, *t;
	char	rev[100], date[9], time[9], user[100];
	char	*p, *q;
	u32	added, deleted, same;
	int	i;
	int	line = 1;
	char	*expected = "?";
	char	*buf;
	ser_t	serial;
	char	(*dates)[20] = 0;
	char	type;
	FILE	*fcludes = fmem();

	rewind(s->fh);
	buf = sccs_nextdata(s);		/* checksum */
	if (buf[0] == 'B') {
		bin_mkgraph(s, buf);
		return;
	}
	line++;
	debug((stderr, "mkgraph(%s)\n", s->sfile));
	unless (buf = sccs_nextdata(s)) goto bad;
	/* skip version mark if it exists */
	line++;
	unless (streq(buf, BKID_STR)) goto first;
	for (;;) {
		unless (buf = sccs_nextdata(s)) {
bad:
			if (fcludes) fclose(fcludes);
			fprintf(stderr,
			    "%s: bad delta on line %d, expected `%s'",
			    s->sfile, line, expected);
			if (buf) {
				fprintf(stderr,
				    ", line follows:\n\t``%s''\n", buf);
			} else {
				fprintf(stderr, "\n");
			}
			sccs_freetable(s);
			return;
		}
		line++;
first:		if (streq(buf, "\001u")) break;

		/* ^As 00001/00000/00011 */
		if (buf[0] != '\001' || buf[1] != 's' || buf[2] != ' ') {
			expected = "^As ";
			goto bad;
		}
		p = &buf[3];
		added = atoiMult_p(&p);
		p++;
		deleted = atoiMult_p(&p);
		p++;
		same = atoiMult_p(&p);
		/* ^Ad D 1.2.1.1 97/05/15 23:11:46 lm 4 2 */
		/* ^Ad R 1.2.1.1 97/05/15 23:11:46 lm 4 2 */
		buf = sccs_nextdata(s);
		line++;
		if (buf[0] != '\001' || buf[1] != 'd' || buf[2] != ' ') {
			expected = "^Ad ";
			goto bad;
		}
	    /* D|R */
		type = buf[3];
	    /* 1.1 or 1.2.2.2, etc. */
		p = &buf[5];
		for (i = 0, p = &buf[5]; *p != ' '; rev[i++] = *p++);
		rev[i] = 0;
		if (*p != ' ') { expected = "^AD 1.1 "; goto bad; }
	    /* 98/03/17 */
		for (i = 0, p++; *p != ' '; date[i++] = *p++);
		date[i] = 0;
		if (*p != ' ') { expected = "^AD 1.1 98/03/17 "; goto bad; }
	    /* 18:32:39[.mmm] */
		for (i = 0, p++; *p != ' ' && *p != '.'; time[i++] = *p++);
		time[i] = 0;
		if (*p != ' ') {
			expected = "^AD 1.1 98/03/17 18:32:39";
			goto bad;
		}
	    /* user */
		for (i = 0, p++; *p != ' '; user[i++] = *p++);
		user[i] = 0;
		if (*p != ' ') {
			expected = "^AD 1.1 98/03/17 18:32:39 user ";
			goto bad;
		}
		p++;
	    /* 10 11 */
		serial = atoi_p(&p);
		if (serial >= s->nextserial) s->nextserial = serial + 1;
		unless (s->slist) {
			growArray(&s->slist, serial);
			growArray(&s->extra, serial);
			dates = calloc(s->nextserial, sizeof(*dates));
		}
		t = SFIND(s, serial);
		assert(t);
		s->numdeltas++;
		d = t;
		d->flags |= D_INARRAY;
		d->added = added;
		d->deleted = deleted;
		d->same = same;
		if (type == 'R') d->flags |= D_TAG;

		if (*p != ' ') {
			expected = "^AD 1.1 98/03/17 18:32:39 user 12 ";
			goto bad;
		}
		p++;
		d->pserial = atoi(p);
		debug((stderr, "mkgraph(%s)\n", rev));
		revArg(s, d, rev);
		if (d->flags & D_BADFORM) {
			expected = "1.2 or 1.2.3.4, too many dots";
			goto bad;
		}
		sprintf(dates[serial], "%s %s", date, time);

		/* this is just user, we add host later */
		d->userhost = sccs_addUniqStr(s, user);
		for (;;) {
			if (!(buf = sccs_nextdata(s)) || buf[0] != '\001') {
				expected = "^A";
				goto bad;
			}
			line++;
			/*
			 * Sun apparently puts empty lists in the file.
			 * Ignore those lines.
			 */
			switch (buf[1]) {
			    case 'i':
			    case 'x':
			    case 'g':
				unless (buf[2] && buf[3]) continue;
			}

			switch (buf[1]) {
			    case 'c':
				goto comment;
			    case 'i':
				p = &buf[3];
				while (q = eachstr(&p, &i)) {
					sccs_saveNum(
					    fcludes, atoi(q), 1);
				}
				break;
			    case 'x':
				p = &buf[3];
				while (q = eachstr(&p, &i)) {
					sccs_saveNum(
					    fcludes, atoi(q), -1);
				}
				break;
			    case 'g':
		    		s->state |= S_READ_ONLY;
				fprintf(stderr, "ignore serials unsupported\n");
				break;
			    case 'e':
				goto done;
			    case 'm':
		    		s->state |= S_READ_ONLY;
				fprintf(stderr, "mr records unsupported\n");
				break;
			    default:
				fprintf(stderr, "Bad file format: ");
				expected = "^A{c,i,x,g,e}";
				goto bad;
			}
		}
		/* ^Ac comments. */
		for (;;) {
			if (!(buf = sccs_nextdata(s)) || (buf[0] != '\001')) {
				expected = "^A";
				goto bad;
			}
			line++;
comment:		if ((buf[1] != 'c') || (buf[2] != ' ')) {
				goto meta;
			}
			unless (d->comments) d->comments = sccs_addStr(s, "");
			sccs_appendStr(s, buf+3);
			sccs_appendStr(s, "\n");
		}
		/* ^AcXmeta */
		for (;;) {
			if (!(buf = sccs_nextdata(s)) || (buf[0] != '\001')) {
				expected = "^A";
				goto bad;
			}
			line++;
meta:			switch (buf[1]) {
			    case 'e': goto done;
			    case 'c':
				if (buf[2] == '_') {	/* strip it */
					;
				} else {
					meta(s, d, buf);
				}
				break;
			    default:
				expected = "^A{e,c}";
				goto bad;
			}
		}
done:		if (CSET(s) && TAG(d) &&
		    !SYMGRAPH(d) && !(d->flags & D_SYMBOLS)) {
			MK_GONE(s, d);
		}
		if (ftell(fcludes) > 0) {
			d->cludes = sccs_addStr(s, fmem_peek(fcludes, 0));
			ftrunc(fcludes, 0);
		}
	}
	fclose(fcludes);
	/*
	 * Convert the linear delta table into a graph.
	 */
	s->tree = d;
	s->table = s->slist + nLines(s->slist);

	unless (CSET(s)) s->file = 1;
	if (CSET(s) &&
	    proj_isComponent(s->proj) &&
	    proj_isProduct(0)) {
	    	s->file = 1;
    	}
	EACHP(s->slist, d) {
		unless (d->flags) continue;

		sccs_inherit(s, d);
		d->date = date2time(dates[SERIAL(s, d)], ZONE(s, d), EXACT) +
		    d->dateFudge;
#define	PARANOID
#ifdef	PARANOID
		/*
		 * adds about 10% to the mkgraph() time, but helps
		 * sanity
		 * The funky number is because a very old version of rcs2bk
		 * would use a rootkey of 70/01/01 03:09:62
		 */
		if (d->date != 11399) {
			assert(streq(delta_sdate(s, d), dates[SERIAL(s, d)]));
		}
#endif
	}
	free(dates);
	unless (flags & INIT_WACKGRAPH) {
		if (checkrevs(s, 0)) s->state |= S_BADREVS|S_READ_ONLY;
	}

	/*
	 * For all the metadata nodes, go through and propogate the data up to
	 * the real node.
	 */
	if (CSET(s)) metaSyms(s);

	if (misc(s)) {
		sccs_freetable(s);
		return;
	}

	debug((stderr, "mkgraph() done\n"));
}

/*
 * Read the stuff after the ^au (which has already been read).
 * ^au
 * user
 * user
 * group name (ATT SCCS allows only numbers, maybe we should too)
 * ^aU
 * ^af whatever
 * ^at
 * Descriptive text (pass through).
 * ^aT
 * flags
 */
private int
misc(sccs *s)
{
	char	*buf;

	/* Save the users / groups list */
	for (; (buf = sccs_nextdata(s)) && !streq(buf, "\001U"); ) {
		if (buf[0] == '\001') {
			fprintf(stderr, "%s: corrupted user section.\n",
			    s->sfile);
			return (-1);
		}
		s->usersgroups = addLine(s->usersgroups, strdup(buf));
	}

	/* Save the flags.  Some are handled by the flags routine; those
	 * are not handled here.
	 */
	for (; (buf = sccs_nextdata(s)) && !streq(buf, "\001t"); ) {
		if (strneq(buf, "\001f &", 4) ||
		    strneq(buf, "\001f z _", 6)) {	/* XXX - obsolete */
			/* We strip these now */
			continue;
		} else if (strneq(buf, "\001f x", 4)) { /* strip it */
			unless (sccs_xflags(s, sccs_top(s))) {
				/* hex or dec */
				s->tree->xflags =
					strtol(&buf[5], 0, 0) & ~X_SINGLE;
				s->tree->flags |= D_XFLAGS;
			}
			continue;
		} else if (strneq(buf, "\001f e ", 5)) {
			switch (atoi(&buf[5])) {
			    case E_ASCII:
			    case E_ASCII|E_GZIP:
			    case E_UUENCODE:
			    case E_UUENCODE|E_GZIP:
			    case E_BAM:
				s->encoding_in |= atoi(&buf[5]);
				break;
			    default:
				fprintf(stderr,
				    "sccs: don't know encoding %d, "
				    "assuming ascii\n",
				    atoi(&buf[5]));
				s->encoding_in |= E_ASCII;
				return (-1);
			}
			continue;
		}
		if (!getflags(s, buf)) {
			s->flags = addLine(s->flags, strdup(&buf[3]));
		}
	}

	/* Save descriptive text. AT&T/teamware might have more after T */
	for (; (buf = sccs_nextdata(s)) && !strneq(buf, "\001T", 2); ) {
		s->text = addLine(s->text, strdup(buf));
	}
	s->data = ftell(s->fh);
	return (0);
}

/*
 * Reads until a space without a preceeding \.
 * XXX - counts on s[-1] being a valid address.
 */
private char *
gettok(char **sp)
{
	char	*t;
	char	*s = *sp;
	static char	copy[MAXLINE];

	t = copy;
	*t = 0;
	while ((*t = *s++)) {
		if (*t == '\n') {
			*t = 0;
			*sp = s;
			return (copy);
		}
		if (*t == ' ') {
			if (t[-1] == '\\') {
				t--;
				*t = ' ';
			} else {
				*t = 0;
				*sp = s;
				return (copy);
			}
		}
		t++;
	}
	*sp = s;
	return (copy);
}

/*
 * Look at the flag and if we handle in this routine, return true.
 * Otherwise, return false so it gets passed through.
 *
 * We handle here:
 *	d	<rev> default branch
 */
private int
getflags(sccs *s, char *buf)
{
	char	*p = &buf[5];
	char	f = buf[3];
	char	*t;
	delta	*d = 0;

	if (buf[0] != '\001' || buf[1] != 'f' || buf[2] != ' ') return (0);
	t = gettok(&p);
	assert(t);
	if ((f != 's') && (f != 'd') &&
	    (!HASGRAPH(s) || !(d = rfind(s, t)))) {
		/* ignore it. but keep it. */
		return (0);
	}
	if (f == 'd') {
		if (s->defbranch) free(s->defbranch);
		s->defbranch = strdup(t);
		return (1);
	}
	return (0);
}

/*
 * Add a symbol(val) to the symbol table.
 * Return 0 if we added it, 1 if it's a dup.
 *
 * if graph:
 *    this is a new symbol and create symgraph structures needed to
 *    put this node in place
 * else:
 *    this is part of a patch and the tag must be added, but other
 *    graph structures already exist
 */
int
addsym(sccs *s, delta *metad, int graph, char *val)
{
	symbol	*sym;
	int	i;
	delta	*d;

	/* find "real" delta being tagged */
	d = metad;
	while (d && TAG(d)) d = PARENT(s, d);
	assert(d);		/* should always find something */

	/*
	 * If graph, it means this is a new symgraph entry and duplicate
	 * tags should be rejected.
	 */
	if (graph && (sym = findSym(s, val)) && (SERIAL(s, d) == sym->ser)) {
		return (1);
	}

	EACH_REVERSE(s->symlist) {
		if (s->symlist[i].meta_ser <= SERIAL(s, metad)) break;
	}
	/* insert after the node found */
	sym = insertArrayN(&s->symlist, i+1, 0);
	assert(sym);
	sym->symname = sccs_addUniqStr(s, val);
	sym->ser = SERIAL(s, d);
	sym->meta_ser = SERIAL(s, metad);
	d->flags |= D_SYMBOLS;
	metad->flags |= D_SYMBOLS;

	if (graph) symGraph(s, metad);
	return (0);
}


/* [user][@host][:path] */
private	remote *
pref_parse(char *buf)
{
	remote	*r;
	char	*p;
	int 	want_path = 0, want_host = 0;

	r = new(remote);
	r->rfd = r->wfd = -1;
	unless (*buf) return (r);
	/* user */
	if (p = strchr(buf, '@')) {
		if (buf != p) {
			*p = 0; r->user = strdup(buf);
		}
		buf = p + 1;
		want_host = 1;
	}
	/* host */
	if (p = strchr(buf, ':')) {
		if (buf != p) {
			if (r->user || want_host) {
				*p = 0; r->host = strdup(buf);
				want_host = 0;
			} else {
				*p = 0; r->user = strdup(buf);
			}
		}
		buf = p + 1;
		want_path = 1;
	}
	if (*buf) {
		if (want_path) {
			r->path = strdup(buf);
		} else if (want_host) {
			r->host = strdup(buf);
		} else {
			assert(r->user == NULL);
			r->user = strdup(buf);
		}
	}
	return (r);
}

/*
 * filter: return 1 if match
 */
private int
filter(char *buf)
{
	remote *r;
	char *h;
	char	*root;

	r = pref_parse(buf);
	if ((r->user) && !match_one(sccs_getuser(), r->user, 0)) {
no_match:	remote_free(r);
		return (0);
	}

	if (r->host) {
		h = sccs_gethost();
		unless (h && match_one(h, r->host, 1)) goto no_match;
	}

	if (r->path && (root = proj_root(0))) {
		unless (match_one(root, r->path, !mixedCasePath())) {
			goto no_match;
		}
	}

	remote_free(r);
	return (1);
}

private char	*
filterMatch(char *buf)
{
	char	*end = strchr(buf, ']');

	unless (end) return (0);
	*end = 0;
	unless (filter(++buf)) return (0);
	return (end);
}

private int
parseConfigKV(char *buf, int nofilter, char **kp, char **vp)
{
	char	*p = 0;

	/* trim leading whitespace */
	while (*buf && isspace(*buf)) buf++;

	/* handle leading filters */
	if (*buf == '[') {
		if (nofilter) {
			/* include filter in key */
			unless (p = strchr(buf, ']')) return (0);
			for (p++; isspace(*p); p++);
		} else {
			/* match filter */
			unless (buf = filterMatch(buf)) return (0);
			for (buf++; isspace(*buf); buf++);
		}
	}
	unless (p) p = buf;
	p = strchr(p, ':');
	if ((*buf == '#') || !p) return (0);

	/*
	 * lose all white space on either side of ":"
	 */
	while ((p >= buf) && isspace(p[-1])) --p;
	if (*p != ':') {
		*p = 0;
		for (p++; *p != ':'; p++);
	}
	for (*p++ = 0; isspace(*p); p++);

	if (streq(buf, "logging_ok")) return (0);

	*kp = buf;
	*vp = p;

	/*
	 * Lose trailing whitespace including newline.
	 */
	while (p[0] && p[1]) p++;
	while (isspace(*p)) *p-- = 0;
//fprintf(stderr, "[%s] -> [%s]\n", *kp, *vp);
	return (1);
}

/*
 * parse a line of config file and insert that line into 'db'.
 */
private void
parseConfig(char *buf, MDBM *db)
{
	char	*k, *v, *p;
	int	flags = MDBM_INSERT;

	unless (parseConfigKV(buf, 0, &k, &v)) return;
	if (*v) {
		for (p = v; p[1]; p++);	/* find end of value */
		if (*p == '!') {
			*p = 0;
			flags = MDBM_REPLACE;
		}
	}
	mdbm_store_str(db, k, v, flags);
}

private void
config2mdbm(MDBM *db, char *config)
{
	FILE	*f;
	char 	buf[MAXLINE];

	if (f = fopen(config, "rt")) {
		while (fnext(buf, f)) parseConfig(buf, db);
		fclose(f);
	}
}

/*
 * Load config file into a MDBM DB
 */
private int
loadRepologConfig(MDBM *DB, char *root)
{
	char	config[MAXPATH];

	unless (root) return (-1);

 	/*
	 * Support for a non-versioned config file.
	 */
	concat_path(config, root, "/BitKeeper/log/config");
	if (exists(config)) config2mdbm(DB, config);
	return (0);
}


/*
 * Load config file into a MDBM DB
 */
private int
loadRepoConfig(MDBM *DB, char *root)
{
	sccs	*s;
	char	*tmpf;
	char	config[MAXPATH];

	unless (root) return (-1);

	/*
	 * If the config is already checked out, use that.
	 */
	concat_path(config, root, "/BitKeeper/etc/config");
	if (exists(config)) {
		config2mdbm(DB, config);
		return (0);
	}

	/*
	 * Normally we'll have a checked out file, check does that for us.
	 * This code is just for the rare case someone cleaned it.
	 *
	 * Note that we can't skip WACKGRAPH here because of a change
	 * where we call the notifier on deltas.  That forces a load
	 * of BitKeeper/etc/config while we're mucking with it.  So
	 * we just have to live without checking revs in loading the
	 * config file.  bk -r check -a will check them.
	 */
	concat_path(config, root, "BitKeeper/etc/SCCS/s.config");
	if (s = sccs_init(config, SILENT|INIT_MUSTEXIST|INIT_WACKGRAPH)) {
		int	ret = 0;

		tmpf = bktmp(0, 0);
		if (sccs_get(s, 0, 0, 0, 0, SILENT|PRINT|GET_EXPAND, tmpf)) {
			perror(tmpf);
			ret = 1;
		} else {
			config2mdbm(DB, tmpf);
		}
		unlink(tmpf);
		free(tmpf);
		sccs_free(s);
		return (ret);
	}
	return (-1);
}

/*
 * "Append" Global config to local config.
 * I.e local field have priority over global field.
 * If local field exists, it masks out the global counter part.
 */
private MDBM *
loadGlobalConfig(MDBM *db)
{
	char 	*config;

	assert(db);
	config = aprintf("%s/BitKeeper/etc/config", globalroot());
	config2mdbm(db, config);
	free(config);
	return(db);
}

/*
 * "Append" bin config to local config.
 * I.e local field have priority over global field.
 * If local field exists, it masks out the global counter part.
 */
MDBM *
loadBinConfig(MDBM *db)
{
	char 	*config;

	assert(db);
	config = aprintf("%s/config", bin);
	config2mdbm(db, config);
	free(config);
	return(db);
}


/*
 * "Append" .bk/config config to local config.
 * I.e local field have priority over global field.
 * If local field exists, it masks out the global counter part.
 */
MDBM *
loadDotBkConfig(MDBM *db)
{
	char 	*config;

	assert(db);
	config = aprintf("%s/config", getDotBk());
	config2mdbm(db, config);
	free(config);
	return(db);
}

/*
 * Override the config db with values from the BK_CONFIG enviromental
 * variable if it exists.
 *
 * BK_CONFIG='var1:value1;var2:values2'
 */
private MDBM *
loadEnvConfig(MDBM *db)
{
	char	*env = getenv("BK_CONFIG");
	char	**values;
	int	i;

	unless (env) return (db);
	assert(db);
	values = splitLine(env, ";", 0);
	EACH (values) parseConfig(values[i], db);
	freeLines(values, free);
	return (db);
}

/*
 * Load both local and global config
 *
 * if forcelocal is set, then we must find a repo config file.
 */
MDBM *
loadConfig(project *p, int forcelocal)
{
	MDBM	*db = mdbm_mem();
	char	*t;
	project	*prod = 0;
	char	**empty = 0;
	int	i;
	kvpair	kv;

	/*
	 * Support for a magic way to set clone default
	 */
	if (t = getenv("BKD_CLONE_DEFAULT")) {
		mdbm_store_str(db, "clone_default", t, MDBM_INSERT);
	}

	if (loadRepoConfig(db, proj_root(p))) {
		if (forcelocal) return (0);
	}
	if (proj_isComponent(p)) {
		unless (prod = proj_isResync(p)) prod = p;
		prod = proj_product(prod);

		/* fetch config from product */
		loadRepoConfig(db, proj_root(prod));
	}
	loadDotBkConfig(db);
	unless (getenv("BK_REGRESSION")) loadGlobalConfig(db);
	loadBinConfig(db);
	/* repolog is higher than all but env */
	loadRepologConfig(db, proj_root(p));
	if (proj_isComponent(p)) {
		/* prod still set from above */
		loadRepologConfig(db, proj_root(prod));
	}
	loadEnvConfig(db);

	/* now remove any empty keys */
	EACH_KV(db) if (kv.val.dsize == 1) empty = addLine(empty, kv.key.dptr);
	EACH(empty) mdbm_delete_str(db, empty[i]);
	freeLines(empty, 0);

	return (db);
}

private void
printconfig(char *file, MDBM *db, MDBM *cfg)
{
	kvpair	kv;
	int	i, j;
	char	*k, *v1, *v2, *t;
	char	**keys = 0;
	FILE	*f;
	MDBM	*freeme = 0;
	char 	buf[MAXLINE];

	unless (db) {
		db = freeme = mdbm_mem();

		if (f = fopen(file, "rt")) {
			while (fnext(buf, f)) {
				unless (parseConfigKV(buf, 0, &k, &v1)) {
					continue;
				}
				mdbm_store_str(db, k, v1, MDBM_INSERT);
			}
			fclose(f);
		}
	}
	EACH_KV(db) keys = addLine(keys, kv.key.dptr);
	unless (keys) goto out;
	sortLines(keys, 0);
	printf("%s:\n", file);
	EACH(keys) {
		k = keys[i];
		v1 = mdbm_fetch_str(db, k);
		assert(v1);
		unless (v2 = mdbm_fetch_str(cfg, k)) v2 = ""; /* deleted is empty for this */

		/* mark the config that doesn't get used */
		if (!*v1 || streq(v1, "!")) {
			if (*v2) putchar('#');
		} else {
			t = strdup(v1);
			j = strlen(t) - 1;
			if (t[j] == '!') t[j] = 0;
			unless (streq(t, v2)) putchar('#');
			free(t);
		}
		printf("\t%s:", k);
		if ((j = 15 - strlen(k)) > 0) {
			while (j--) putchar(' ');
		}
		puts(v1);
	}
	putchar('\n');
out:
	freeLines(keys, 0);
	if (freeme) mdbm_close(freeme);
}

/*
 * merge some new config keys into an existing config file.
 * This is used by the installer to add config items embedded in the
 * installer into the `bk bin` config file.
 */
private int
config_merge(char *file1, char *file2)
{
	hash	*config = hash_new(HASH_MEMHASH);
	FILE	*f;
	char	*k, *v;
	char	**keys = 0;
	int	i;
	char	buf[MAXLINE];

	/* load second file into hash */
	if (streq(file2, "-")) {
		f = stdin;
	} else {
		unless (f = fopen(file2, "r")) {
			perror(file2);
			return (1);
		}
	}
	while (fnext(buf, f)) {
		if (parseConfigKV(buf, 1, &k, &v)) {
			hash_storeStr(config, k, v);
		}
	}
	unless (f == stdin) fclose(f);

	/* copy first file and keep track of keys */
	unless (f = fopen(file1, "r")) {
		perror(file1);
		return (1);
	}
	while (fnext(buf, f)) {
		fputs(buf, stdout);
		if (parseConfigKV(buf, 1, &k, &v)) {
			hash_deleteStr(config, k);
		}
	}
	fclose(f);

	/* print any keys remaining from file2 */
	EACH_HASH(config) keys = addLine(keys, config->kptr);
	sortLines(keys, 0);
	EACH(keys) {
		printf("%s: %s\n", keys[i],
		    (char *)hash_fetchStr(config, keys[i]));
	}
	freeLines(keys, 0);
	hash_free(config);
	return (0);
}

int
config_main(int ac, char **av)
{
	char	*root, *file, *env;
	MDBM	*db;
	MDBM	*cfg = proj_config(0);
	char	**values = 0;
	char	*k, *v;
	int	i, c;
	int	verbose = 0;
	int	merge = 0;
	kvpair	kv;

	while ((c = getopt(ac, av, "mv", 0)) != -1) {
		switch (c) {
		    case 'm': merge = 1; break;
		    case 'v': verbose = 1; break;
		    default: bk_badArg(c, av);
		}
	}
	if (merge) {
		unless ((ac - optind) == 2) usage();
		if (verbose) usage();
		return (config_merge(av[optind], av[optind+1]));
	}
	unless (verbose) {
		if (av[optind]) {
			if (av[optind+1]) usage();
			if (v = mdbm_fetch_str(cfg, av[optind])) {
				puts(v);
				return (0);
			} else {
				return (1);
			}
		}
		EACH_KV(cfg) {
			values = addLine(values,
			    aprintf("%s: %s", kv.key.dptr, kv.val.dptr));
		}
		sortLines(values, 0);
		EACH(values) puts(values[i]);
		freeLines(values, free);
		return (0);
	}
	if (av[optind]) usage();

	/* repo config */
	if (root = proj_root(0)) {
		file = aprintf("%s/BitKeeper/etc/config", root);
		unless (exists(file)) get(file, SILENT|GET_EXPAND, "-");
		printconfig(file, 0, cfg);
		free(file);
	}

	/* product config */
	if (proj_isComponent(0) && (root = proj_root(proj_product(0)))) {
		file = aprintf("%s/BitKeeper/etc/config", root);
		unless (exists(file)) get(file, SILENT|GET_EXPAND, "-");
		printconfig(file, 0, cfg);
		free(file);
	}

	/* dotbk config */
	file = aprintf("%s/config", getDotBk());
	printconfig(file, 0, cfg);
	free(file);

	unless (getenv("BK_REGRESSION")) {
		/* global config */
		file = aprintf("%s/BitKeeper/etc/config", globalroot());
		printconfig(file, 0, cfg);
		free(file);
	}

	/* bin config */
	file = aprintf("%s/config", bin);
	printconfig(file, 0, cfg);
	free(file);

	/* local config used in bk clone --checkout=<mode> */
	if (root = proj_root(0)) {
		file = aprintf("%s/BitKeeper/log/config", root);
		if (exists(file)) printconfig(file, 0, cfg);
		free(file);
	}

	/* product config */
	if (proj_isComponent(0) && (root = proj_root(proj_product(0)))) {
		file = aprintf("%s/BitKeeper/log/config", root);
		if (exists(file)) printconfig(file, 0, cfg);
		free(file);
	}

	/* $BK_CONFIG */
	db = mdbm_mem();
	if (env = getenv("BK_CONFIG")) values = splitLine(env, ";", 0);
	EACH (values) {
		unless (parseConfigKV(values[i], 0, &k, &v)) continue;
		mdbm_store_str(db, k, v, MDBM_REPLACE);
	}
	freeLines(values, free);
	printconfig("$BK_CONFIG", db, cfg);
	mdbm_close(db);

	return (0);
}

/*
 * Return 0 for OK, -1 for error.
 */
int
check_gfile(sccs *s, int flags)
{
	struct	stat sbuf;

	if (lstat(s->gfile, &sbuf) == 0) {
		unless ((flags & INIT_NOGCHK) || fileTypeOk(sbuf.st_mode)) {
			verbose((stderr,
			    "unsupported file type: %s (%s) 0%06o\n",
			    s->sfile, s->gfile, sbuf.st_mode & 0177777));
err:			sccs_free(s);
			return (-1);
		}
		s->state |= S_GFILE;
		s->mode = sbuf.st_mode;
		s->gtime = sbuf.st_mtime;
		if (S_ISLNK(sbuf.st_mode)) {
			char link[MAXPATH];
			int len;

			s->mode |= 0777; /* hp11 is weird */
			len = readlink(s->gfile, link, sizeof(link));
			if ((len > 0 )  && (len < sizeof(link))){
				link[len] = 0;
				if (s->symlink) free(s->symlink);
				s->symlink = strdup(link);
			} else {
				verbose((stderr,
				    "cannot read sym link: %s\n", s->gfile));
				goto err;
			}
		}
	} else {
		s->state &= ~S_GFILE;
		s->mode = 0;
	}
	return (0);
}

void
gdb_backtrace(void)
{
	FILE	*f;
	char	*cmd;

	unless (getenv("_BK_BACKTRACE")) return;
	unless ((f = efopen("BK_TTYPRINTF")) ||
	    (f = fopen(DEV_TTY, "w"))) {
		f = stderr;
	}
	cmd = aprintf("gdb -batch -ex backtrace '%s/bk' %u 1>&%d 2>&%d",
	    bin, getpid(), fileno(f), fileno(f));

	system(cmd);
	free(cmd);
	if (f != stderr) fclose(f);
}

/*
 * Initialize an SCCS file.  Do this before anything else.
 * If the file doesn't exist, the graph isn't set up.
 * It should be OK to have multiple files open at once.
 * If the project is passed in, use it, else init one if we are in BK mode.
 */
sccs*
sccs_init(char *name, u32 flags)
{
	sccs	*s;
	struct	stat sbuf;
	char	*t;
	int	lstat_rc;
	delta	*d;
	int	fixstime = 0;
	static	int _YEAR4;
	static	int fixpath = -1;
	static	char *glob = 0;
	static	int show = -1;

	if (show == -1) {
		glob = getenv("BK_SHOWINIT");
		show = glob != 0;
	}
	if (show && match_one(name, glob, 0)) {
		ttyprintf("init(%s) [%s]\n", name, prog);
		gdb_backtrace();
	}

	if (strchr(name, '\n') || strchr(name, '\r')) {
		fprintf(stderr,
		   "bad file name, file name must not contain LF or CR "
		   "character\n");
		return (0);
	}
	localName2bkName(name, name);
	if (sccs_filetype(name) != 's') {
		fprintf(stderr, "Not an SCCS file: %s\n", name);
		return (0);
	}
	lstat_rc = lstat(name, &sbuf);
	if (lstat_rc && (flags & INIT_MUSTEXIST)) return (0);
	s = new(sccs);
	s->sfile = strdup(name);
	s->gfile = sccs2name(name);
	s->encoding_in |= E_ALWAYS;

	s->initFlags = flags;
	t = strrchr(s->sfile, '/');
	if (t) {
		*t = 0;
		s->proj = proj_init(s->sfile);
		*t = '/';
	} else {
		s->proj = proj_init(".");
	}

	/*
	 * This weirdness is for dspecs that need the component prefix.
	 * This is not historically correct but works for recent locations
	 * of the component.
	 */
	if (fixpath == -1) fixpath = (getenv("_BK_FIX_NESTED_PATH") != 0);
	if (fixpath && proj_isComponent(s->proj)) {
		s->comppath = proj_comppath(s->proj);
	}

	if (isCsetFile(s->sfile)) {
		s->xflags |= X_HASH;
		s->state |= S_CSET;
	}

	if (flags & INIT_NOSTAT) {
		if ((flags & INIT_HASgFILE) && check_gfile(s, flags)) return 0;
	} else {
		if (check_gfile(s, flags)) return (0);
	}
	if (lstat_rc == 0) {
		if (!S_ISREG(sbuf.st_mode)) {
			verbose((stderr, "Not a regular file: %s\n", s->sfile));
 err:			free(s->gfile);
			free(s->sfile);
			proj_free(s->proj);
			free(s);	/* We really mean free, not sccs_free */
			return (0);
		}
		if (sbuf.st_size == 0) {
			verbose((stderr, "Zero length file: %s\n", s->sfile));
			goto err;
		}
		s->state |= S_SFILE;
		s->size = sbuf.st_size;
		s->stime = sbuf.st_mtime;	/* for GET_DTIME */

		/*
		 * Catch any case where we would cause make to barf to
		 * fail if we are BK folks, otherwise fix it.
		 */
		if ((flags & INIT_CHK_STIME) &&
		    s->gtime && (sbuf.st_mtime > s->gtime)) {
			if ((t = getenv("_BK_DEVELOPER")) && *t) {
				fprintf(stderr,
				    "timestamp %s\n\ts\t%u\n\tgtime\t%u\n\t",
				    s->gfile,
				    (unsigned)sbuf.st_mtime,
				    (unsigned)s->gtime);
				if (lstat(s->gfile, &sbuf)) sbuf.st_mtime = 0;
				fprintf(stderr, "g\t%u\n",
				    (unsigned)sbuf.st_mtime);
				exit(1);
			}
			fixstime = 1;
		}
	} else if (CSET(s)) {
		int	bad;
		/* t still points at last slash in s->sfile */
		assert(*t == '/');

		t[1] = 'q';
		bad = exists(s->sfile);
		t[1] = 's';
		if (bad) {
			fprintf(stderr,
"Unable to proceed.  ChangeSet file corrupted.  error=57\n"
"Please run 'bk support' to request assistance.\n");
			goto err;
		}
	}
	s->pfile = strdup(sccsXfile(s, 'p'));
	s->zfile = strdup(sccsXfile(s, 'z'));
	if (flags & INIT_NOSTAT) {
		if (flags & INIT_HASpFILE) s->state |= S_PFILE;
		if (flags & INIT_HASzFILE) s->state |= S_ZFILE;
	} else {
		if (isreg(s->pfile)) s->state |= S_PFILE;
		if (isreg(s->zfile)) s->state |= S_ZFILE;
	}
	debug((stderr, "init(%s) -> %s, %s\n", s->gfile, s->sfile, s->gfile));
	s->nextserial = 1;
	sccs_open(s, &sbuf);

	unless (s->fh) {
		if ((errno == ENOENT) || (errno == ENOTDIR)) {
			/* Not an error if the file doesn't exist yet.  */
			debug((stderr, "%s doesn't exist\n", s->sfile));
			s->cksumok = 1;		/* but not done */
			goto out;
		} else {
			unless (flags & INIT_NOWARN) {
				fputs("sccs_init: ", stderr);
				perror(s->sfile);
			}
			free(s->sfile);
			free(s->gfile);
			free(s->pfile);
			proj_free(s->proj);
			free(s);	/* We really mean free, not sccs_free */
			return (0);
		}
	}
	if (((flags&INIT_NOCKSUM) == 0) && badcksum(s, flags)) {
		goto out;
	} else {
		s->cksumok = 1;
	}
	bk_featureRepoChk(s->proj); /* check before we parse sfile */
	mkgraph(s, flags);

	/* test lease after we have s->table->pathname */
	lease_check(s->proj, O_RDONLY, s);

	debug((stderr, "mkgraph found %d deltas\n", s->numdeltas));
	if (HASGRAPH(s)) {
		/*
		 * get the xflags from the delta graph
		 * instead of the sccs flag section
		 */
		s->xflags = sccs_xflags(s, sccs_top(s));
		unless (BITKEEPER(s)) s->xflags |= X_SCCS;

		/*
		 * Don't allow them to check in a gfile of a different type.
		 */
		if (HAS_GFILE(s) && (!(t=getenv("BK_NO_TYPECHECK")) || !*t)) {
			for (d = s->table; TAG(d); d = NEXT(d));
			assert(d);
			if ((d->flags & D_MODE) &&
			    (fileType(d->mode) != fileType(s->mode))) {
				verbose((stderr,
				    "%s has different file types, treating "
				    "this file as read only.\n", s->gfile));
		    		s->state |= S_READ_ONLY;
			}
		}
	}

	/*
	 * Let them force YEAR4
	 */
	unless (_YEAR4) _YEAR4 = getenv("BK_YEAR2") ? -1 : 1;
	if (_YEAR4 == 1) s->xflags |= X_YEAR4;

	if (sig_ignore() == 0) s->unblock = 1;

	if (CSET(s)) {
		int i, in_log = 0;

		EACH(s->text) {
			if (s->text[i][0] == '\001') continue;
			unless (in_log) {
				if (streq(s->text[i], "@ROOTLOG")) in_log = 1;
				continue;
			}
			if (streq(s->text[i], "detach") &&
			    bk_notLicensed(s->proj, LIC_PL, 0)) {
				exit(101);
			}
			if (s->text[i][0] == '@') break;
		}
	}
 out:
	if (fixstime) sccs_setStime(s, s->stime); /* only make older */
	return (s);
}

/*
 * Restart an sccs_init because we've changed the state of the file.
 *
 * This does not reread the delta table, if you want that, use sccs_reopen().
 */
sccs*
sccs_restart(sccs *s)
{
	struct	stat sbuf;
	char	*buf;
	char	*header;

	assert(s);
	if (check_gfile(s, 0)) return (0);
	bzero(&sbuf, sizeof(sbuf));	/* file may not be there */
	if (lstat(s->sfile, &sbuf) == 0) {
		if (!S_ISREG(sbuf.st_mode)) {
bad:			sccs_free(s);
			return (0);
		}
		if (sbuf.st_size == 0) goto bad;
		s->state |= S_SFILE;
	}
	if (s->mem_in) {
		assert(s->state & S_SOPEN);
		s->size = fseek(s->fh, 0, SEEK_END);
		goto skip;
	}
	unless ((s->state & S_SOPEN) && (s->size == sbuf.st_size)) {
		sccs_close(s);
		if (sccs_open(s, &sbuf)) return (s);
skip:
		rewind(s->fh);
		header = fgetline(s->fh);
		if (header[0] == 'B') {
			s->data = bin_data(header);
		} else {
			/* XXX need data-offset in header */
			while(buf = sccs_nextdata(s)) {
				if (streq(buf, "\001T")) break;
			}
			s->data = ftell(s->fh);
		}
	}
	if (isreg(s->pfile)) {
		s->state |= S_PFILE;
	} else {
		s->state &= ~S_PFILE;
	}
	if (isreg(s->zfile)) {
		s->state |= S_ZFILE;
	} else {
		s->state &= ~S_ZFILE;
	}
	if (s->mdbm) {
		mdbm_close(s->mdbm);
		s->mdbm = 0;
	}
	return (s);
}

sccs	*
sccs_reopen(sccs *s)
{
	sccs	*s2;

	assert(s);
	sccs_close(s);
	s2 = sccs_init(s->sfile, s->initFlags);
	assert(s2);
	sccs_free(s);
	return (s2);
}

/*
 * open & mmap the file.
 * Use this after an sccs_close() to reopen,
 * use sccs_reopen() if you need to reread the graph.
 * Note: this is used under windows via win32_open().
 */
int
sccs_open(sccs *s, struct stat *sp)
{
	struct	stat sbuf;

	assert(s);
	if (s->state & S_SOPEN) {
		assert(s->fh);
		return (0);
	} else {
		assert(s->fh == 0);
	}
	unless (s->fh = fopen(s->sfile, "rb")) return (-1);
	unless (sp) {
		sp = &sbuf;
		fstat(fileno(s->fh), sp);
	}
	s->size = sp->st_size;
	s->state |= S_SOPEN;
	return (0);
}

/*
 * close all open file stuff associated with an sccs structure.
 * Note: this is used under windows via win32_close().
 */
void
sccs_close(sccs *s)
{
	int	i;

	if (s->state & S_SOPEN) {
		assert(s->fh != 0);
		assert(!s->mem_in); /* don't want to lose data */
		fclose(s->fh);
		s->fh = 0;
		EACH(s->mapping) dataunmap((MAP*)s->mapping[i]);
		freeLines(s->mapping, 0);
		s->mapping = 0;
		s->state &= ~S_SOPEN;
	} else {
		assert(s->fh == 0);
	}
}

/*
 * a) If gfile exists and writable, insist p.file exist
 * b) If p.file exists, insist gfile writable or absent
 * Note 1: "delta -n" and "admin -i" violate  'a'
 * Note 2: "edit -g" may violate 'b'
 */
private void
chk_gmode(sccs *s)
{
	struct	stat sbuf;
	int	pfileExists, gfileExists, gfileWritable;
	char 	*gfile;

	unless (getenv("_BK_GMODE_DEBUG")) return;
	if (!s || !HASGRAPH(s)) return; /* skip new file */

	gfile = sccs2name(s->sfile); /* Don't trust s->gfile, see bk admin -i */
	gfileExists = !lstat(gfile, &sbuf);
	gfileWritable = (gfileExists && (sbuf.st_mode & 0200));
	if (S_ISLNK(sbuf.st_mode)) return; /* skip smylink */
	pfileExists = exists(sccs_Xfile(s, 'p'));

	if (gfileWritable) {
		unless (pfileExists) {
			fprintf(stderr,
			    "ERROR: %s: writable gfile with no p.file\n",
			    gfile);
			assert("writable gfile with no p.file" == 0);
		};
	}

	if (pfileExists) {
		if (gfileExists && !gfileWritable) {
			fprintf(stderr,
			    "ERROR: %s: p.file with read only gfile\n",
			    gfile);
			assert("p.file with read only gfile" == 0);
		}
	}
	free(gfile);
}

/*
 * Free up all resources associated with the file.
 * This is the last thing you do.
 */
void
sccs_free(sccs *s)
{
 	int	unblock;
	char	*relpath = 0, *fullpath;

	unless (s) return;
	if (s->io_error && !s->io_warned) {
		fprintf(stderr, "%s: unreported I/O error\n", s->sfile);
	}
	if (s->mem_out) {
		FILE	*f, *out;
		size_t	len;
		char	*buf;

		/* the sfile is still in memory.  Write it out. */
		assert(s->sfile);
		if (s->mem_in) {
			f = s->fh;
			assert(f != s->outfh);
			fclose(s->outfh);
			s->fh = 0;
			s->state &= ~S_SOPEN;
		} else {
			f = s->outfh;
		}
		s->outfh = 0;
		s->mem_in = s->mem_out = 0;

		buf = fmem_peek(f, &len);
		out = sccs_startWrite(s);
		assert(buf);
		fwrite(buf, 1, len, out);
		// XXX In this case sccs_free has transactional quality
		// and has no back channel to let caller know transaction
		// failed.
		if (sccs_finishWrite(s, &out)) sccs_abortWrite(s, &out);
		fclose(f);
	}
	chk_gmode(s);

	/*
	 * If we modified the s.file and we're in checkout:edit|last
	 * then kick explorer with the full pathname to this file.
	 * It's fine to do it as we are tearing down the sccs*, we've
	 * committed state to the file system already.
	 */
	if (s->modified &&
	    (CO(s) & (CO_EDIT|CO_LAST)) &&
	    (relpath = proj_relpath(s->proj, s->gfile)) &&
	    (fullpath = proj_fullpath(s->proj, relpath))) {
		notifier_changed(fullpath);
	}
	if (relpath) free(relpath);
	// No free on fullpath, proj.c maintains it.

	sccsXfile(s, 0);
	sccs_freetable(s);
	free(s->symlist);
	if (s->state & S_SOPEN) sccs_close(s); /* move this up for trace */
	if (s->sfile) free(s->sfile);
	if (s->gfile) free(s->gfile);
	if (s->zfile) free(s->zfile);
	if (s->pfile) free(s->pfile);
	if (s->state & S_CHMOD) {
		struct	stat sbuf;

		if (fstat(fileno(s->fh), &sbuf) == 0) {
			sbuf.st_mode &= ~0200;
			chmod(s->sfile, sbuf.st_mode & 0777);
		}
	}
	if (s->defbranch) free(s->defbranch);
	freeLines(s->usersgroups, free);
	freeLines(s->flags, free);
	freeLines(s->text, free);
	if (s->symlink) free(s->symlink);
	if (s->mdbm) mdbm_close(s->mdbm);
	if (s->goneDB) mdbm_close(s->goneDB);
	if (s->idDB) mdbm_close(s->idDB);
	if (s->findkeydb) hash_free(s->findkeydb);
	if (s->locs) free(s->locs);
	if (s->proj) proj_free(s->proj);
	if (s->kidlist) free(s->kidlist);
	if (s->rrevs) freenames(s->rrevs, 1);
	if (s->fastsum) free(s->fastsum);
	unblock = s->unblock;
	bzero(s, sizeof(*s));
	free(s);
	if (unblock) sig_default();
}

/*
 * open the ChangeSet file if can be found.  We are not necessarily
 * at the root, nor do we want to go to the root forcefully, as the
 * command line parameters may not be root relative
 */

sccs	*
sccs_csetInit(u32 flags)
{
	char	*rootpath;
	char	csetpath[MAXPATH];
	sccs	*cset = 0;

	if (exists(CHANGESET)) {
		strcpy(csetpath, CHANGESET);
	} else {
		unless (rootpath = proj_root(0)) return (0);
		concat_path(csetpath, rootpath, CHANGESET);
	}
	debug((stderr, "sccs_csetinit: opening changeset '%s'\n", csetpath));
	cset = sccs_init(csetpath, flags);
	return (cset);
}

/*
 * We want SCCS/s.foo or path/to/SCCS/s.foo
 * ATT allows s.foo or path/to/s.foo.
 * We insist on SCCS/s. unless in ATT compat mode.
 * XXX ATT compat mode sucks - it's really hard to operate on a
 * gfile named s.file .
 *
 * This returns the following:
 *	'c'	this is an SCCS pathname (whatever/SCCS/c.whatever)
 *	'd'	this is an SCCS pathname (whatever/SCCS/d.whatever)
 *	'm'	this is an SCCS pathname (whatever/SCCS/m.whatever)
 *	'p'	this is an SCCS pathname (whatever/SCCS/p.whatever)
 *	'r'	this is an SCCS pathname (whatever/SCCS/r.whatever)
 *	's'	this is an SCCS pathname (whatever/SCCS/s.whatever)
 *	'x'	this is an SCCS pathname (whatever/SCCS/x.whatever)
 *	'z'	this is an SCCS pathname (whatever/SCCS/z.whatever)
 *	0	none of the above
 */
int
sccs_filetype(char *name)
{
	char	*s = rindex(name, '/');

	if (!s) {
#ifdef	ATT_SCCS
		if (name[0] == 's' && name[1] == '.') return ((int)'s');
#endif
		return (0);
	}
	unless (s[1] && (s[2] == '.')) return (0);
	switch (s[1]) {
	    case 'c':	/* comments files */
	    case 'd':	/* delta pending */
	    case 'm':	/* name resolve files */
	    case 'p':	/* lock files */
	    case 'r':	/* content resolve files */
	    case 's':	/* sccs file */
	    case 'x':	/* temp file, about to be s.file */
	    case 'z':	/* lock file */
	    	break;
	    default:  return (0);
	}
#ifdef	ATT_SCCS
	return ((int)s[1]);
#endif
	if ((name <= &s[-4]) && pathneq("SCCS", &s[-4], 4)) {
		return ((int)s[1]);
	}
	return (0);
}

/*
 * remove SCCS/s. or just the s.
 * It's up to the caller to free() the resulting name.
 */
char	*
sccs2name(char *sfile)
{
	char	*s, *t;
	char	*new = strdup(sfile);

	s = rindex(new, '/');
	if (!s) {
		assert(new[0] == 's' && new[1] == '.');
		t = new;
		s = &new[2];
	} else {
		assert(s[1] == 's' && s[2] == '.');
		if ((s >= (new + 4)) &&
		    (s[-1] == 'S') && (s[-2] == 'C') &&
		    (s[-3] == 'C') && (s[-4] == 'S')) {
			t = &s[-4];
			s = &s[3];
		} else {
			t = &s[1];
			s = &s[3];
		}
	}
	while ((*t++ = *s++));
	return (new);
}

/*
 * Make the sccs dir if we need one.
 */
void
mksccsdir(char *sfile)
{
	char	*s = rindex(sfile, '/');

	if (!s) return;
	if ((s >= sfile + 4) &&
	    s[-1] == 'S' && s[-2] == 'C' && s[-3] == 'C' && s[-4] == 'S') {
		*s = 0;
		mkdir(sfile, 0777);
		*s = '/';
	}
}

/*
 * Take a file name such as foo.c and return SCCS/s.foo.c
 * Also works for /full/path/foo.c -> /fullpath/SCCS/s.foo.c.
 * It's up to the caller to free() the resulting name.
 */
char	*
name2sccs(char *name)
{
	int	len = strlen(name);
	char	*s, *newname;

	/* maybe it has the SCCS in it already */
	if (s = rindex(name, '/')) s -= 4;	/* point it at start of SCCS/ */
	unless (s >= name) s = 0;

	/* DIR_WITH_SCCS/GOTTEN screwed us up, this should fix it */
	if (s && strneq(s, "SCCS/", 5) && ((s == name) || (s[-1] == '/'))) {
		unless (sccs_filetype(name)) {
			fprintf(stderr,
			    "%s: invalid file name %s\n", prog, name);
			exit(1);
		}
		name = strdup(name);
		s = strrchr(name, '/');
		s[1] = 's';
		return (name);
	}
	newname = malloc(len + 8);
	assert(newname);
	strcpy(newname, name);
	if ((s = rindex(newname, '/'))) {
		s++;
		strcpy(s, "SCCS/s.");
		s += 7;
		strcpy(s, rindex(name, '/') + 1);
	} else {
		strcpy(s = newname, "SCCS/s.");
		s += 7;
		strcpy(s, name);
	}
	return (newname);
}

private int
stale(char *file)
{
	char	buf[300];
	char	*t, *h = sccs_gethost();
	int	n, fd = open(file, 0, 0);

	if (fd == -1) return (0);
	if ((n = read(fd, buf, sizeof(buf))) <= 0) {
		close(fd);
		return (0);
	}
	buf[n-1] = 0;				/* null the newline */
	close(fd);
	unless (t = strchr(buf, ' ')) return (0);
	*t++ = 0;
	unless (streq(t, h)) return (0);	/* different hosts */
	for (n = 0; n < 100; n++) {		/* about a second */
		if (kill(atoi(buf), 0) == -1) {
			unlink(file);
			return (1);
		}
		usleep(10000);
	}
	return (0);
}

/*
 * create SCCS/<type>.foo.c
 */
int
sccs_lock(sccs *s, char type)
{
	char	*t;
	int	lockfd, verbose;

	if (READ_ONLY(s)) return (0);

	verbose = (s->state & SILENT) ? 0 : 1;
	if ((type == 'z') && repository_locked(s->proj)) return (0);

	/* get -e does Z lock so we can skip past the repository locks */
	if (type == 'Z') type = 'z';
	t = sccsXfile(s, type);
again:	lockfd =
	    open(t, O_CREAT|O_WRONLY|O_EXCL, type == 'z' ? 0444 : GROUP_MODE);
	debug((stderr, "lock(%s) = %d\n", s->sfile, lockfd >= 0));
	if ((lockfd == -1) && stale(t)) goto again;
	if (lockfd == -1) {
		return (0);
	}
	if (type == 'z') {
		char	buf[20];
		char	*h = sccs_gethost();

		sprintf(buf, "%u ", getpid());
		write(lockfd, buf, strlen(buf));
		if (h) {
			write(lockfd, h, strlen(h));
		} else {
			write(lockfd, "?", 1);
		}
		write(lockfd, "\n", 1);
		s->state |= S_ZFILE;
	}
	close(lockfd);
	return (1);
}

/*
 * Take SCCS/s.foo.c and unlink SCCS/<type>.foo.c
 */
int
sccs_unlock(sccs *sccs, char type)
{
	char	*s;
	int	failed;

	debug((stderr, "unlock(%s, %c)\n", sccs->sfile, type));
	s = sccsXfile(sccs, type);
	failed  = unlink(s);
	// XXX This seems to a bug, we should only reset S_ZFILE if type == 'z'
	unless (failed) sccs->state &= ~S_ZFILE;
	return (failed);
}

/*
 * Take SCCS/s.foo.c, type and return a temp copy of SCCS/<type>.foo.c
 */
private char *
sccsXfile(sccs *sccs, char type)
{
	static	char	*s = 0;
	static	int	len = 0;
	char	*t;

	if (type == 0) {	/* clean up so purify doesn't barf */
		if (len) free(s);
		len = 0;
		s = 0;
		return (0);
	}
	if (!len) {
		len = strlen(sccs->sfile) + 50;
		s = malloc(len + 50);
		assert(s);
	} else if (len < (int) strlen(sccs->sfile) + 3) {
		free(s);
		len = strlen(sccs->sfile) + 50;
		s = malloc(len);
		assert(s);
	}
	if (!index(sccs->sfile, '/')) {
		strcpy(s, sccs->sfile);
		s[0] = type;
		return (s);
	}
	strcpy(s, sccs->sfile);
	t = rindex(s, '/') + 1;
	*t = type;
	return (s);
}

char	*
sccs_Xfile(sccs *s, char type)
{
	return (sccsXfile(s, type));
}

/*
 * Get the date as YY/MM/DD HH:MM:SS.mmm
 * and get timezone as minutes west of GMT
 */
private void
date(sccs *s, delta *d, time_t tt)
{
	d->date = tt + d->dateFudge;
	zoneArg(s, d, sccs_zone(tt));
}

char	*
testdate(time_t t)
{
	static char	date[50];
	char	zone[50];

	strcpy(date, time2date(t));
	strcpy(zone, sccs_zone(t));

	if (date2time(date, zone, EXACT) != t) {
		fprintf(stderr, "Internal error on dates, aborting.\n");
		fprintf(stderr, "time_t=%lu vs %lu date=%s zone=%s\n",
		    date2time(date, zone, EXACT), t, date, zone);
		exit(1);
	}
	return (date);
}

/*
 * Returns a date string like 01/08/16 13:54:42 in the current
 * timezone for a time_t.  Should be used with sccs_zone(tt) to
 * get the full time string.
 * Return value is in a staticly allocated buffer.
 */
char *
time2date(time_t tt)
{
	static	char	tmp[50];

	strftime(tmp, sizeof(tmp), "%y/%m/%d %H:%M:%S",
		 localtimez(&tt, 0));
	return (tmp);
}

/*
 * Call strftime() on the time of a delta.  The time is converted
 * back to the timezone of where the delta was created.
 */
int
delta_strftime(char *out, int sz, char *fmt, sccs *s, delta *d)
{
	int	zone = 0;
	int	neg = 1;
	char	*p;
	time_t	tt = d->date - d->dateFudge;

	p = ZONE(s, d);
	if (*p == '-') {
		neg = -1;
		++p;
	} else if (*p == '+') {
		++p;
	}
	zone = HOUR * atoi_p(&p);
	if (*p++ == ':') zone += MINUTE * atoi_p(&p);
	tt += neg * zone;
	return (strftime(out, sz, fmt, gmtime(&tt)));
}

 /*
  * Save a serial in an array.
  * The serial number is stored in ascending order.
  */
ser_t *
addSerial(ser_t *space, ser_t s)
{
	int	i;

	EACH(space) {
		/* addSerial allowed dups, so for now, let there be dups */
		// if (space[i] == s) return (space); /* no dups */
		if (space[i] > s) break;
	}
	insertArrayN(&space, i, &s);
	return (space);
}

/*
 * Parse the list of revisions passed, sort of like strtok().
 * The first call passes in the list, the next passes in 0.
 * Handle rev,rev,rev,rev-rev,rev...
 * "rev" can be a symbol.
 * We're careful to have the list unchanged if we return an error.
 */
private delta *
walkList(sccs *s, char *list, int *errp)
{
	static	char *next;	/* left pointing at next rev */
	static	delta *d;	/* set if we are working up the tree */
	static	delta *stop;	/* where to stop in the tree, inclusive */
	delta	*tmp;
	char	save, *t, *rev;

	if (list) {
		next = list;
		d = stop = 0;
	}
	if (d) {
		tmp = d;
		d = d == stop ? 0 : PARENT(s, d);
		return (tmp);
	}
	if (!next) return (0);
	/* XXX - this will screw up if they have symbols w/ - in the name */
	for (t = next; *t && *t != ',' && *t != '-'; t++);
	if (!*t || (*t == ',')) {
		save = *t;
		*t++ = 0;
		rev = next;
		next = save ? t : 0;
		if (streq(rev, "+")) {
			tmp = findrev(s, 0);
		} else {
			tmp = rfind(s, rev);
		}
		t[-1] = save;
		return (tmp);
	}
	if (*t != '-') {
		*errp = 1;
		return (0);
	}
	/*
	 * OK, it's a range.  Find the bottom/top and and start walking them.
	 * We insist that the ordering is 1.3-1.9, not the other way, and
	 * that there is a path from the second rev to the first through
	 * the parent pointers (i.e., it doesn't go around a corner down
	 * a branch).
	 */
	*t++ = 0;
	rev = next;
	next = t;
	stop = rfind(s, rev);
	t[-1] = '-';
	for (t = next; *t && *t != ',' && *t != '-'; t++);
	if (*t && (*t != ',')) {
		*errp = 1;
		return (0);
	}
	save = *t;
	*t++ = 0;
	rev = next;
	next = save ? t : 0;
	if (streq(rev, "+")) {
		d = findrev(s, 0);
	} else {
		d = rfind(s, rev);
	}
	t[-1] = save;

	if (!stop || !d) {
		*errp = 2;
		return (0);
	}
	for (tmp = d; tmp && tmp != stop; tmp = PARENT(s, tmp));
	if (tmp != stop) {
		*errp = 2;
		return (0);
	}
	tmp = d;
	d = (d == stop) ? 0 : PARENT(s, d);
	return (tmp);
}

/*
 * Read the list of serials in s and put them in list.
 * S can either be list of serials or a list of revisions.
 * In neither case does this recurse on the list, it just parses the list.
 * This is quite different than serialmap() which uses this routine but
 * stores the results in a different data structure.
 * XXX - this poorly named, it doesn't use "struct serlist".
 */
private ser_t *
getserlist(sccs *sc, int isSer, char *s, int *ep)
{
	delta	*t;
	ser_t	*l = 0;

	debug((stderr, "getserlist(%s)\n", s));
	if (isSer) {
		while (*s && *s != '\n') {
			l = addSerial(l, (ser_t) atoi(s));
			while (*s && *s != '\n' && isdigit(*s)) s++;
			while (*s && *s != '\n' && !isdigit(*s)) s++;
		}
		return (l);
	}
	for (t = walkList(sc, s, ep); !*ep && t; t = walkList(sc, 0, ep)) {
		l = addSerial(l, SERIAL(sc, t));
	}
	return (l);
}

/*
 * Generate a list of serials marked with D_SET tag
 */
private u8 *
setmap(sccs *s, int bit, int all)
{
	u8	*slist;
	delta	*t;

	slist = calloc(s->nextserial, sizeof(u8));
	assert(slist);

	for (t = s->table; t; t = NEXT(t)) {
		unless (all || !TAG(t)) continue;
		if (t->flags & bit) {
			slist[SERIAL(s, t)] = 1;
		}
	}
	return (slist);
}

/* compress a set of serials.  Assume 'd' is basis version and compute
 * include and exclude strings to go with it.  The strings are a
 * comma separated list of numbers
 */

private int
compressmap(sccs *s, delta *d, u8 *set, char **inc, char **exc)
{
	u8	*slist;
	delta	*t;
	char	*p;
	int	i, sign;
	int	active;
	ser_t	*incser = 0, *excser = 0;
	ser_t	tser, *tserp;
	FILE	*f;

	assert(d);
	assert(set);

	*exc = *inc = 0;

	slist = calloc(s->nextserial, sizeof(u8));
	assert(slist);

	slist[SERIAL(s, d)] = S_PAR;	/* seed the ancestor thread */

	for (t = s->table; t; t = NEXT(t)) {
		if (TAG(t)) continue;
		tser = SERIAL(s, t);

		/* Set up parent ancestory for this node */
		if ((slist[tser] & S_PAR) && t->pserial) {
			slist[t->pserial] |= S_PAR;
#ifdef MULTIPARENT
			if (t->merge) slist[t->merge] |= S_PAR;
#endif
		}

		/* if a parent and not excluded, or if included */
		active = (((slist[tser] & (S_PAR|S_EXCL)) == S_PAR)
		    || slist[tser] & S_INC);

		/* exclude if active in delta set and not in desired set */
		if (active && !set[tser]) addArray(&excser, &tser);
		unless (set[tser])  continue;

		/* include if not active in delta set and in desired set */
		if (!active) addArray(&incser, &tser);
		p = CLUDES(s, t);
		while (i = sccs_eachNum(&p, &sign)) {
			unless(slist[i] & (S_INC|S_EXCL)) {
				slist[i] |= (sign > 0) ? S_INC : S_EXCL;
			}
		}
	}
	if (slist)   free(slist);
	if (incser) {
		f = 0;
		EACHP_REVERSE(incser, tserp) {
			t = SFIND(s, *tserp);
			if (f) {
				fputs(",", f);
			} else {
				f = fmem();
			}
			fputs(REV(s, t), f);
		}
		*inc = fmem_close(f, 0);
		free(incser);
	}
	if (excser) {
		f = 0;
		EACHP_REVERSE(excser, tserp) {
			t = SFIND(s, *tserp);
			if (f) {
				fputs(",", f);
			} else {
				f = fmem();
			}
			fputs(REV(s, t), f);
		}
		*exc = fmem_close(f, 0);
		free(excser);
	}
	return (0);
}

/*
 * Generate a list of serials to use to get a particular delta and
 * allocate & return space with the list in the space.
 * The 0th entry is contains the maximum used entry.
 * Note that the error pointer is to be used only by walkList, it's null if
 * the lists are null.
 * Note we don't have to worry about growing tables here, the list isn't saved
 * across calls.
 */
private u8 *
serialmap(sccs *s, delta *d, char *iLst, char *xLst, int *errp)
{
	u8	*slist;
	delta	*t, *start = d;
	char	*p;
	int	i, sign;
	ser_t	tser;

	assert(d);

	slist = calloc(s->nextserial, sizeof(u8));
	assert(slist);

	/* initialize with iLst and xLst */
	if (iLst) {
		debug((stderr, "Included:"));
		for (t = walkList(s, iLst, errp);
		    !*errp && t; t = walkList(s, 0, errp)) {
			debug((stderr, " %s", t->rev));
			slist[SERIAL(s, t)] = S_INC;
			if (t > start) start = t;
 		}
		debug((stderr, "\n"));
		if (*errp) goto bad;
	}

	if (xLst) {
		debug((stderr, "Excluded:"));
		for (t = walkList(s, xLst, errp);
		    !*errp && t; t = walkList(s, 0, errp)) {
			debug((stderr, " %s", t->rev));
			if (slist[SERIAL(s, t)] == S_INC)
				*errp = 3;
			else {
				slist[SERIAL(s, t)] = S_EXCL;
			}
			if (t > start) start = t;
 		}
		debug((stderr, "\n"));
		if (*errp) goto bad;
 	}

	/* Use linear list, newest to oldest, looking only at 'D' */

	/* slist is used as temp storage for S_INC and S_EXCL then
	 * replaced with either a 0 or a 1 depending on if in view
	 * XXX clean up use of enum values mixed with 0 and 1
	 * XXX slist has only one of 5 values:
	 *     0, 1, S_INC, S_EXCL, S_PAR
	 */

	/* Seed the graph thread */
	slist[SERIAL(s, d)] |= S_PAR;

	for (t = start; t; t = NEXT(t)) {
		if (TAG(t)) continue;
		tser = SERIAL(s, t);

		/* Set up parent ancestory for this node */
		if ((slist[tser] & S_PAR) && t->pserial) {
			slist[t->pserial] |= S_PAR;
#ifdef MULTIPARENT
			if (t->merge) slist[t->merge] |= S_PAR;
#endif
		}

		/* if an ancestor and not excluded, or if included */
		if ( ((slist[tser] & (S_PAR|S_EXCL)) == S_PAR)
		     || slist[tser] & S_INC) {

			slist[tser] = 1;
			p = CLUDES(s, t);
			while (i = sccs_eachNum(&p, &sign)) {
				unless(slist[i] & (S_INC|S_EXCL)) {
					slist[i] |=
					    (sign > 0) ? S_INC : S_EXCL;
				}
			}
		} else {
			slist[tser] = 0;
		}
	}
	return (slist);
bad:	free(slist);
	return (0);
}

int
sccs_graph(sccs *s, delta *d, u8 *map, char **inc, char **exc)
{
	return (compressmap(s, d, map, inc, exc));
}

u8 *
sccs_set(sccs *s, delta *d, char *iLst, char *xLst)
{
	int	junk = 0;

	return (serialmap(s, d, iLst, xLst, &junk));
}


#define	SL_MASK		0x7fffffffUL
#define	SL_SER(x)	((x) & SL_MASK)
#define	SL_INS(x)	((x) & ~SL_MASK)
#define	SL_SET(ser, i)	((ser) | ((i) ? SL_MASK+1 : 0))

/*
 * The weave is a pretty restrictive data structure.
 * The I-E blocks nest, -- any new I will be the largest numbered I in the
 * list.  D-E blocks span across, but are rooted in I blocks of smaller
 * number.  You can see that in the assert in the first for loop: no I
 * will be found while looping looking for the item of interest.
 */
private	ser_t *
changestate(ser_t *state, char type, ser_t serial)
{
	int	i;

	debug2((stderr, "chg(%c, %d)\n", type, serial));
	assert(!(serial & ~SL_MASK)); /* serial doesn't overflow */

	/* find place in list */
	EACH_REVERSE(state) {
		if (SL_SER(state[i]) <= serial) break;
		assert (!SL_INS(state[i])); /* must be D */
	}

	/*
	 * Delete it if it is an 'E'; insert otherwise
	 */
	if (type == 'E') {
		assert(SL_SER(state[i]) == serial);
		removeArrayN(state, i);
	} else {
		serial = SL_SET(serial, (type == 'I'));
		insertArrayN(&state, i+1, &serial);
	}
	return (state);
}

private int
topstate(ser_t *state)
{
	int	i;

	i = nLines(state);
	return (i ? SL_SER(state[i]): 0);
}

private int
delstate(ser_t ser, ser_t *state, u8 *slist)
{
	int	ok = 0;
	int	i;

	/* To be yes, serial must delete and no others, and first I
	 * must be active.  If any other delete active, return false.
	 */
	assert(slist[ser]);
	EACH_REVERSE(state) {
		if (SL_INS(state[i])) break;
		if (SL_SER(state[i]) == ser) {
			ok = 1;
		} else if (slist[SL_SER(state[i])]) {
			return (0);
		}
	}
	if (ok && i) {
		ser = SL_SER(state[i]);
		if (slist[ser]) return (ser);
	}
	return (0);
}

private int
visitedstate(ser_t *state, u8 *slist)
{
	ser_t	ser;

	/* when ignoring D, is this block active? (for annotate) */
	return (((ser = whatstate(state)) && slist[ser]) ? ser : 0);
}

private int
whatstate(ser_t *state)
{
	int	i;

	/* Loop until an I */
	EACH_REVERSE(state) {
		if (SL_INS(state[i])) break;
	}
	return (i ? SL_SER(state[i]) : 0);
}

/* calculate printstate using where we are (state)
 * and list of active deltas (slist)
 * return either the serial if active, or 0 if not
 */

private int
printstate(ser_t *state, u8 *slist)
{
	int	i, ret = 0;
	ser_t	val;

	/* Loop until any I or active D */
	EACH_REVERSE(state) {
		val = SL_SER(state[i]);
		if (SL_INS(state[i])) {
			if (slist[val]) ret = val;
			break;
		} else if (slist[val]) {
			break;
		}
	}
	return (ret);
}

/*
 * This interface is used for writing the metadata (delta table, flags, etc).
 *
 * N.B. All checksum functions do the intermediate sum in an int variable
 * because 16-bit register arithmetic can be up to 2x slower depending on
 * the platform and compiler.
 */
private sum_t
fputmeta(sccs *s, u8 *buf, FILE *out)
{
	register u8	*t = buf;
	register unsigned int sum = 0;

	for (; *t; t++)	sum += *t;
	s->cksum += (sum_t)sum;
	fputs(buf, out);

	return (sum);
}

/*
 * Data for zputs() to writes it's data.  We currently can only write
 * one sfile at a time.
 */
private	zputbuf	*zput;

private void
gzip_sum(void *data, u8 *buf, int len)
{
	sccs	*s = ((void **)data)[0];
	FILE	*f = ((void **)data)[1];
	u32	sum = 0;
	int	i;

	if (len) {
		for (i = 0; i < len; sum += buf[i++]);
		s->cksum += (sum_t)sum;
		fwrite(buf, 1, len, f);
	} else {
		free(data);
	}
}

private void
sccs_zputs_init(sccs *s, FILE *fout)
{
	void	**data = malloc(2 * sizeof(void *));

	data[0] = s;
	data[1] = fout;
	zput = zputs_init(gzip_sum, data, -1);
	assert(zput);
}

private void
sccs_zputs_done(sccs *s)
{
	assert(zput);
	zputs_done(zput);
	zput = 0;
}

/*
 * If the data isn't a control line, just dump it.
 * Otherwise, put it out there after adjusting the serial.
 */
private sum_t
fputbumpserial(sccs *s, u8 *buf, int inc, FILE *out)
{
	u8	tmp[20];
	u8	*t;
	ser_t	ser, sum;

	unless (buf[0] == '\001') return (fputdata(s, buf, out));
	/* ^AI ddd\n
	 * ^AE ddd\n
	 * ^AE dddN\n
	 */
	tmp[1] = 0;
	tmp[0] = buf[1];
	sum = fputdata(s, "\001", out);
	sum += fputdata(s, tmp, out);
	sum += fputdata(s, " ", out);
	ser = atoi(&buf[3]);
	sprintf(tmp, "%u", ser + inc);
	sum += fputdata(s, tmp, out);
	for (t = &buf[3]; isdigit(*t); t++);
	sum += fputdata(s, t, out);
	return (sum);
}

/*
 * Like fputmeta, but optionally compresses the data stream.
 * This is used for the data section exclusively.
 */
sum_t
fputdata(sccs *s, u8 *buf, FILE *out)
{
	u32	sum = 0;
	u8	*p = buf;

	while (*p) {
		sum += *p;
		if (*p++ == '\n') break;
	}
	if (GZIP_OUT(s)) {
		zputs(zput, buf, p - buf);
	} else {
		fwrite(buf, 1, p - buf, out);
		s->cksum += sum;
	}
	return (sum);
}

#define	ENC(c)	((((uchar)c) & 0x3f) + ' ')
#define	DEC(c)	((((uchar)c) - ' ') & 0x3f)

private inline int
uuencode1(register uchar *from, register char *to, int n)
{
	int	space[4];
	register int *c = space;
	register int i;
	char	*save = to;

	*to++ = ENC(n);
	for (i = 0; i < n; i += 3) {
		c[0] = from[i] >> 2;
		c[1] = ((from[i]<<4)&0x30) | ((from[i+1]>>4)&0xf);
		c[2] = ((from[i+1]<<2)&0x3c) | ((from[i+2]>>6)&3);
		c[3] = from[i+2] & 0x3f;
		*to++ = ENC(c[0]);
		*to++ = ENC(c[1]);
		*to++ = ENC(c[2]);
		*to++ = ENC(c[3]);
	}
	*to++ = '\n';
	*to = 0;
	return (to - save);
}

private int
uuencode_sum(sccs *s, FILE *in, FILE *out)
{
	uchar	ibuf[450];
	char	obuf[80];
	register uchar *inp;
	register int n;
	register int length;
	int	added = 0;

	while ((length = fread(ibuf, 1, 450, in)) > 0) {
		inp = ibuf;
		while (length > 0) {
			n = (length > 45) ? 45 : length;
			if (n < 45) {
				uchar	*e = &inp[n];
				int	left = 45 - n;

				while (left--) *e++ = 0;
			}
			length -= n;
			uuencode1(inp, obuf, n);
			s->dsum += fputdata(s, obuf, out);
			inp += n;
			added++;
		}
	}
	s->dsum += fputdata(s, " \n", out);
	return (++added);
}

int
uuencode(FILE *in, FILE *out)
{
	uchar	ibuf[450];
	char	obuf[650];
	register uchar *inp;
	register char *outp;
	register int n;
	register int length;
	int	added = 0;

	while ((length = fread(ibuf, 1, 450, in)) > 0) {
		outp = obuf;
		inp = ibuf;
		while (length > 0) {
			n = (length > 45) ? 45 : length;
			if (n < 45) {
				uchar	*e = &inp[n];
				int	left = 45 - n;

				while (left--) *e++ = 0;
			}
			length -= n;
			outp += uuencode1(inp, outp, n);
			added++;
			inp += n;
		}
		*outp = 0;
		fputs((char *)obuf, out);
	}
	fputs(" \n", out);
	return (++added);
}

private inline int
uudecode1(register char *from, register uchar *to)
{
	int	length, save;

	unless (from[0] && from[1] && (length = DEC(*from++))) return (0);
	if (length > 50) {
		fprintf(stderr, "Corrupted data: %.25s\n", from);
		return (0);
	}
	save = length;
	while (length > 0) {
		if (length-- > 0)
			*to++ = (uchar)((DEC(from[0])<<2) | (DEC(from[1])>>4));
		if (length-- > 0)
			*to++ = (uchar)((DEC(from[1])<<4) | (DEC(from[2])>>2));
		if (length-- > 0)
			*to++ = (uchar)((DEC(from[2]) << 6) | DEC(from[3]));
		from += 4;
	}
	return (save);
}

int
uudecode(FILE *in, FILE *out)
{
	uchar	ibuf[650];
	char	obuf[450];
	int	n, moved = 0;

	while (fnext(ibuf, in)) {
		n = uudecode1(ibuf, obuf);
		moved += n;
		fwrite(obuf, n, 1, out);
	}
	return (moved);
}

private int
openOutput(sccs *s, int encode, char *file, FILE **op)
{
	char	*mode = "w";
	int	toStdout = streq(file, "-");

	assert(op);
	debug((stderr, "openOutput(%x, %s, %p)\n", encode, file, op));
	/*
	 * Note: This has no effect when we print to stdout We want
	 * this becuase we want diff_gfile() to diffs file with
	 * normlized to LF.
	 *
	 * Win32 note: t.bkd regression failed if ChangeSet have have
	 * CRLF termination.
	 */
	if (((encode & E_DATAENC) == E_ASCII) &&
	    !CSET(s) && (s->xflags & X_EOLN_NATIVE)) {
		mode = "wt";
	}
	if (toStdout) {
		*op = stdout;
	} else {
		unless (*op = fopen(file, mode)) {
			mkdirf(file);
			*op = fopen(file, mode);
		}
	}
	debug((stderr, "openOutput = %p\n", *op));
	return (0);
}

/*
 * Return a list of revisions from rev to the first gca of base.
 * Null return is an error.
 * An invariant is that no list is returned if the rev is already implied
 * by base..root.
 */
char *
sccs_impliedList(sccs *s, char *who, char *base, char *rev)
{
	delta	*baseRev, *t, *mRev;
	int	active;
	char	*inc = 0, *exc = 0;
	u8	*slist = 0;
	char	*p;
	int	i, sign;
	ser_t	tser;

	/* XXX: This can go away when serialmap does this directly
	 */

	unless (baseRev = findrev(s, base)) {
		fprintf(stderr,
		    "%s: cannot find base rev %s in %s\n",
		    who, base, s->sfile);
err:		s->state |= S_WARNED;
		if (inc) free(inc);
		if (exc) free(exc);
		if (slist) free(slist);
		return (0);
	}
	unless (mRev = findrev(s, rev)) {
		fprintf(stderr,
		    "%s: cannot find merge rev %s in %s\n",
		    who, rev, s->sfile);
		goto err;
	}

	slist = calloc(s->nextserial, sizeof(u8));

	slist[SERIAL(s, baseRev)] = S_PAR;
	slist[SERIAL(s, mRev)] = S_PAR;

	for (t = s->table; t; t = NEXT(t)) {
		if (TAG(t)) continue;
		tser = SERIAL(s, t);

		/* Set up parent ancestory for this node */
		if ((slist[tser] & S_PAR) && t->pserial) {
			slist[t->pserial] |= S_PAR;
#ifdef MULTIPARENT
			if (t->merge) slist[t->merge] |= S_PAR;
#endif
		}

		/* if a parent and not excluded, or if included */
		active = (((slist[tser] & (S_PAR|S_EXCL)) == S_PAR)
		     || slist[tser] & S_INC);

		unless (active) {
			slist[tser] = 0;
			continue;
		}
		slist[tser] = 1;
		p = CLUDES(s, t);
		while (i = sccs_eachNum(&p, &sign)) {
			unless(slist[i] & (S_INC|S_EXCL)) {
				slist[i] |= (sign > 0) ? S_INC : S_EXCL;
			}
		}
	}
	if (compressmap(s, baseRev, slist, &inc, &exc)) {
		fprintf(stderr, "%s: cannot compress merged set\n", who);
		goto err;
	}
	if (exc) {
		fprintf(stderr,
		    "%s: compressed map caused exclude list: %s\n",
		    who, (char *)exc);
		goto err;
	}
	if (slist) free(slist);
	return (inc);
}

/*
 * Take two strings and concat them into a new strings.
 * Caller frees.
 */
private char *
strconcat(char *a, char *b, char *sep)
{
	char	*tmp;

	if (!a) return (b);
	if (!b) return (a);
	tmp = malloc(strlen(a) + strlen(b) + 2);
	sprintf(tmp, "%s%s%s", a, sep, b);
	return (tmp);
}

private int
write_pfile(sccs *s, int flags, delta *d,
	char *rev, char *iLst, char *i2, char *xLst, char *mRev)
{
	int	fd, len;
	char	*tmp, *tmp2;

	if (WRITABLE_REG(s) && HAS_PFILE(s) && (iLst || i2 || xLst)) {
		/* going from plain edit to -i/-x -- need to clean first */
		if (sccs_clean(s, CLEAN_SHUTUP|SILENT)) {
			verbose((stderr,
			    "Writable %s exists which cannot be cleaned, "
			    "skipping it.\n", s->gfile));
			s->state |= S_WARNED;
			return (-1);
		}
	} else if ((WRITABLE_REG(s) ||
		S_ISLNK(s->mode) && HAS_GFILE(s) && HAS_PFILE(s)) && 
	    !(flags & GET_SKIPGET)) {
		verbose((stderr,
		    "Writable %s exists, skipping it.\n", s->gfile));
		s->state |= S_WARNED;
		return (-1);
	} else if (HAS_PFILE(s) && (!HAS_GFILE(s) || !WRITABLE_REG(s))) {
		/* bk edit foo; rm or chmod 444 foo => cleanup SCCS/p.foo */
		if (sccs_clean(s, CLEAN_SHUTUP|(flags & SILENT))) {
			s->state |= S_WARNED;
			return (-1);
		}
	}
	unless (sccs_lock(s, 'Z')) {
		fprintf(stderr, "get: can't zlock %s\n", s->gfile);
		repository_lockers(s->proj);
		return (-1);
	}
	fd = open(s->pfile, O_CREAT|O_WRONLY|O_EXCL, GROUP_MODE);
	if (fd == -1) {
		fprintf(stderr, "get: can't plock %s\n", s->gfile);
		sccs_unlock(s, 'z');
		return (-1);
	}
	tmp2 = time2date(time(0));
	assert(sccs_getuser() != 0);
	len = strlen(REV(s, d))
	    + MAXREV + 2
	    + strlen(rev)
	    + strlen(sccs_getuser())
	    + strlen(tmp2)
	    + (xLst ? strlen(xLst) + 3 : 0)
	    + (mRev ? strlen(mRev) + 3 : 0)
	    + 3 + 1 + 1; /* 3 spaces \n NULL */
	if (i2) {
		len += strlen(i2) + 3;
	} else {
#ifdef CRAZY_WOW
		// XXX: this used to be here, and was removed because
		// mRev can be set with no i2 list because of the
		// elements in the merge, while on another branch,
		// might have been previously included with -i.
		// see the t.merge file for an example of this.
		assert(!mRev);
#endif
		len += (iLst ? strlen(iLst) + 3 : 0);
	}
	tmp = malloc(len);
	sprintf(tmp, "%s %s %s %s", REV(s, d), rev, sccs_getuser(), tmp2);
	if (i2) {
		strcat(tmp, " -i");
		strcat(tmp, i2);
	} else if (iLst) {
		strcat(tmp, " -i");
		strcat(tmp, iLst);
	}
	if (xLst) {
		strcat(tmp, " -x");
		strcat(tmp, xLst);
	}
	if (mRev) {
		strcat(tmp, " -m");
		strcat(tmp, mRev);
	}
	strcat(tmp, "\n");
	write(fd, tmp, strlen(tmp));
	close(fd);
	free(tmp);
	s->state |= S_PFILE|S_ZFILE;
	return (0);
}

int
sccs_rewrite_pfile(sccs *s, pfile *pf)
{
	int	fd, len;
	char	*tmp;
	char	*user = sccs_getuser();
	char	*date = time2date(time(0));


	/* XXX: Do I need any special locking code? */
	if ((fd = open(s->pfile, O_WRONLY|O_TRUNC, 0666)) == -1) {
		perror("open pfile");
		return (-1);
	}
	len = strlen(pf->oldrev)
	    + MAXREV + 2
	    + strlen(pf->newrev)
	    + strlen(user)
	    + strlen(date)
	    + (pf->iLst ? strlen(pf->iLst) + 3 : 0)
	    + (pf->xLst ? strlen(pf->xLst) + 3 : 0)
	    + (pf->mRev ? strlen(pf->mRev) + 3 : 0)
	    + 3 + 1 + 1; /* 3 spaces \n NULL */
	tmp = malloc(len);
	sprintf(tmp, "%s %s %s %s",
	    pf->oldrev, pf->newrev, user, date);
	if (pf->iLst) {
		strcat(tmp, " -i");
		strcat(tmp, pf->iLst);
	}
	if (pf->xLst) {
		strcat(tmp, " -x");
		strcat(tmp, pf->xLst);
	}
	if (pf->mRev) {
		strcat(tmp, " -m");
		strcat(tmp, pf->mRev);
	}
	strcat(tmp, "\n");
	write(fd, tmp, strlen(tmp));
	close(fd);
	free(tmp);
	return (0);
}

/*
 * Returns: valid address if OK, (char*)-1 if error, 0 if not ok but not error.
 */
private char *
setupOutput(sccs *s, char *printOut, int flags, delta *d)
{
	char	*f, *full;
	char	*rel = 0;

	/*
	 * GET_SUM should always have PRINT as well because otherwise we
	 * may create a gfile when all we wanted was the SUM.  Yucky API.
	 */
	if (flags & (PRINT|GET_SUM)) {
		f = printOut;
	} else {
		if (flags & GET_NOREGET) flags |= SILENT;
		if (WRITABLE_REG(s) && writable(s->gfile)) {
			verbose((stderr, "Writable %s exists\n", s->gfile));
			s->state |= S_WARNED;
			return ((flags & GET_NOREGET) ? 0 : (char*)-1);
		} else if ((flags & GET_NOREGET) && exists(s->gfile) &&
		    (!(flags&GET_EDIT) || !(s->xflags & (X_RCS|X_SCCS)))) {
			if ((flags & GET_EDIT) && !WRITABLE(s)) {

				if (chmod(s->gfile, s->mode | 0200)) {
					/*
					 * If chmod fails then we just
					 * unlink the gfile and
					 * refetch it
					 */
					goto doreget;
				}
				s->mode |= 0200;

				/*
				 * We're changing the status of the file
				 * without touching the file, tell bkshellx
				 */
				if (BITKEEPER(s) &&
				    (rel = proj_relpath(s->proj, s->gfile)) &&
				    (full = proj_fullpath(s->proj, rel))) {
					notifier_changed(full);
				}
				if (rel) free(rel);
				// No free on full, proj.c maintains it.
			}
			return (0);
		}
doreget:	f = s->gfile;
		unlinkGfile(s);
	}
	return (f);
}

/*
 * Get the checksum of the first delta found with a real checksum, not
 * a made up one from almostUnique().
 * We depend on the fact that includes/excludes force a data checksum,
 * not an almostUnique() value.
 */
private	delta *
getCksumDelta(sccs *s, delta *d)
{
	delta	*t;

	for (t = d; t; t = PARENT(s, t)) {
		if (t->cludes || t->added || t->deleted) {
			return (t);
		}
	}
	return (0);
}

/*
 * Get the checksum of the first delta found with a real checksum, not
 * a made up one from almostUnique().
 * That would be the earliest delta that has the same symlink value as
 * d->symlink. if none exists, the checksum delta is d itself.
 * 
 * Note : A symlink merge node must have its own checksum,
 * because:
 * a) One of the parent delta has became the merge parent. This means we
 *    may not find the right checksum by following the parent pointer.
 *    Recomputing of checksum should be done in the resolver.
 *    The resolver does not have a symlink resolver yet, we
 *    need to re-test this after the symlink resolve is implemented.
 * b) It also possible that conflict is resolved by creating third symlink
 *    target.
 */
delta *
getSymlnkCksumDelta(sccs *s, delta *d)
{
	delta	*t, *p;

	assert(d->symlink);
	if (d->merge) return (d);
	for (t = d; t; t = PARENT(s, t)) {
		p = PARENT(s, t);
		unless (p->symlink) return (t);
		unless (p->symlink == d->symlink) return (t);
		if (p->merge && (p->symlink == d->symlink)) return (p);
	}
	return (d);
}

/*
 * fail if
 *   - the file for rk is there, and delta dk is missing and gone
 *   - the rk is gone and the file is missing
 * goneDB normally doesn't use the hash value.  Here it stores a "1"
 * in place of the default "" to mean really gone (not in file system).
 */
private	int
deltaChk(sccs *cset, char *rk, char *dk)
{
	sccs	*s;
	char	*rkgone;
	int	rc = 0;
	char	buf[MAXPATH];

	unless ((rkgone = mdbm_fetch_str(cset->goneDB, rk)) ||
	    mdbm_fetch_str(cset->goneDB, dk)) {
	    	return (0);
	}
	if (rkgone && (*rkgone == '1')) return (1);
	unless (cset->idDB) {
		concat_path(buf, proj_root(cset->proj), getIDCACHE(cset->proj));
		unless (cset->idDB = loadDB(buf, 0, DB_IDCACHE)) {
			perror(buf);
			exit(1);
		}
	}
	if (s = sccs_keyinit(cset->proj, rk, SILENT, cset->idDB)) {
		/* if file there, but key is missing, ignore line. */
		unless (sccs_findKey(s, dk)) rc = 1;
		sccs_free(s);
	} else if (rkgone) {
		/* if no file, mark goneDB that it was really gone */
		mdbm_store_str(cset->goneDB, rk, "1", MDBM_REPLACE);
		rc = 1;
	} else {
		// not rk gone but no keyinit -- let rset fail => rc = 0;
	}
	return (rc);
}

private int
getKey(sccs *s, MDBM *DB, char *data, int flags)
{
	char	*k, *v;
	int	rc;

	k = data;
	unless (v = separator(data)) {
		fprintf(stderr, "get hash: no separator in '%s'\n", data);
		return (-1);
	}
	*v++ = 0;
	if (flags & GET_SKIPGONE) {
		if (mdbm_fetch_str(DB, k) || deltaChk(s, k, v)) {
			goto skip;
		}
	}
	if (mdbm_store_str(DB, k, v, MDBM_INSERT) && (errno == EEXIST)) {
skip:		rc = 0;
	} else {
		rc = 1;
	}
	v[-1] = ' ';
	return (rc);
}

private char	*
get_lineName(sccs *s, ser_t ser, MDBM *db, u32 lnum, char *buf)
{
	datum	k, v;

	/* see if ser is in mdbm, if not, gen an md5 name and stick in buf */
	k.dptr = (void *)&ser;
	k.dsize = sizeof(ser_t);
	v = mdbm_fetch(db, k);
	if (v.dsize) {
		strcpy(buf, v.dptr);
	} else {
		sccs_md5delta(s, sfind(s, ser), buf);
		v.dptr = (void *)buf;
		v.dsize = strlen(buf) + 1;
		if (mdbm_store(db, k, v, MDBM_INSERT)) {
			fprintf(stderr, "lineName cache: insert error\n");
			return (0);
		}
	}
	sprintf(&buf[v.dsize - 1], ".%u", lnum);
	return (buf);
}

/*
 * Array contents:
 *   31 = set if command ; not if data
 * CMD
 *   30,29
 *    0  0 - D
 *    0  1 - E (only for D)
 *    1  0 - I (all I-E pairs translated to I<cur> - I<prev>)
 *    1  1 - N (E (now I) with nonewline tag)
 *   0-28 = serial
 * DATA
 *   30-16 = block line count
 *   0-15 = block checksum
 */

/* command defines */
#define	SUM_CMD		0x80000000
#define	SUM_I		0x40000000
#define	SUM_E		0x20000000
#define	SUM_N		0x20000000

#define	SUM_LAST	0x20000000
#define	SUM_MAXSER	(SUM_LAST - 1)

/* data defines - 1 bit CMD; 15 bits linecount; 16 bits sum */
#define	SUM_SIZE	(sizeof(sum_t) * 8)
#define	SUM_MAXCOUNT	((1 << (32 - SUM_SIZE - 1)) - 1)
#define	SUM_MASK	((1 << SUM_SIZE) - 1)

private	int
fastsum_load(sccs *s)
{
	sum_t	sum = 0;
	u32	linecount = 0;
	ser_t	*state = 0;
	u32	*fast = 0;
	u8	*buf, *p;	/* need u8 for summing so no sign extend */
	char	type, *n;	/* u8 but get api type matching */
	/* really serials */
	u32	ser, cur = 0, last = SUM_MAXSER + 1;
	/* u32 array entries */
	u32	x, top = 0;

	sccs_rdweaveInit(s);
	while (buf = (u8 *)sccs_nextdata(s)) {
		if (isData(buf)) {
			p = buf;
			if (*p == CNTLA_ESCAPE) p++;
			while (*p) sum += *p++;
			sum += '\n';
			linecount++;
			if (linecount < SUM_MAXCOUNT) continue;
		}
		if (linecount) {
			x = (linecount << SUM_SIZE) | sum;
			addArray(&fast, &x);
			top = x;
			sum = 0;
			linecount = 0;
			if (isData(buf)) continue;
			last = SUM_MAXSER + 1;	/* all "data ^AI" okay */
		}
		type = buf[1];
		n = &buf[3];
		ser = atoi_p(&n);
		assert(ser <= SUM_MAXSER);
		/* pack command into array */
		x = SUM_CMD;
		switch (type) {
		    case 'E':
			if (ser == cur) {
				last = cur;
				/* change I<cur>-E<cur> to I<cur>-I<prev> */
				state = changestate(state, 'E', cur);
				ser = whatstate(state);
				assert(ser < cur);
				cur = ser;
				x |= SUM_I;
				if (*n == 'N') {
					x |= SUM_N;
					/* for check.c to know to check */
					s->has_nonl = 1;
				}
			} else {
				x |= SUM_E;
			}
			break;
		    case 'I':
			assert(cur < ser);
			assert(ser < last); /* Ia..EaIb..Eb -> b < a */
			state = changestate(state, 'I', ser);
			cur = ser;
			x |= SUM_I;
			break;
		    case 'D':
			/* D == (!E && !I) so nothing set */
		    	break;
		}
		x |= ser;
		/* compress I - if top of cache is also I (and N matches) */
		if ((x & SUM_I) &&
		    ((x & (SUM_CMD|SUM_I|SUM_N)) ==
		    (top & (SUM_CMD|SUM_I|SUM_N)))) {
			fast[nLines(fast)] = x;
		} else {
			addArray(&fast, &x);
		}
		top = x;
	}
	assert(!linecount && !sum);
	free(state);
	if (sccs_rdweaveDone(s)) {
		free(fast);
		s->io_error = s->io_warned = 1;
		return (1);
	}
	s->fastsum = fast;
	return (0);
}

private int
fastsum(sccs *s, u8 *slist, int this)
{
	ser_t	*state = 0;
	sum_t	sum = 0;
	u32	linecount = 0, added = 0, deleted = 0, same = 0;
	u32	x;
	char	type;
	int	i;
	int	no_lf = 0;	/* boolean */
	/* serials */
	u32	dstate = 0, cur = 0, lf_pend = 0;
	u32	ser;

	if (!s->fastsum && fastsum_load(s)) return (1);

	EACH(s->fastsum) {
		x = s->fastsum[i];
		unless (x & SUM_CMD) {
			unless (slist[cur]) continue;

			linecount = (x >> SUM_SIZE);
			if (dstate < cur) {
				no_lf = 0;
				lf_pend = cur;
				sum += (x & SUM_MASK);
				if (cur == this) {
					added += linecount;
				} else {
					same += linecount;
				}
			} else {
				if (cur == lf_pend) lf_pend = 0;
				/* delstate() on steroids */
				if ((dstate == this) &&
				    (topstate(state) < cur)) {
					deleted += linecount;
				}
			}
			continue;
		}
		ser = x & SUM_MAXSER;
		if (x & SUM_I) {
			/* a step down means the block ended */
			if (ser < lf_pend) {
				lf_pend = 0;
				if (x & SUM_N) no_lf = 1;
			}
			cur = ser;
		} else if (slist[ser]) {
			/*
			 * Ignore inactive deletes.  Only process active.
			 * dstate has newest; state has rest.
			 * that adds complex logic below and pays off
			 * with fewer changestate calls and with the
			 * simple deleted linecount logic above.
			 */
			type = (x & SUM_E) ? 'E' : 'D';
			if (ser > dstate) {
				assert(type == 'D');	/* push */
				if (dstate) {
					state =
					    changestate(state, 'D', dstate);
				}
				dstate = ser;
			} else if (ser == dstate) { 
				assert(type == 'E');	/* pop */
				if (dstate = topstate(state)) {
					state =
					    changestate(state, 'E', dstate);
				}
			} else {
				/* non-top insert and rm */
				state = changestate(state, type, ser);
			}
		}
	}
	if (no_lf) sum -= '\n';
	s->dsum = sum;
	s->added = added;
	s->deleted = deleted;
	s->same = same;
	free(state);
	return (0);
}

private int
get_reg(sccs *s, char *printOut, int flags, delta *d,
		int *ln, char *iLst, char *xLst)
{
	u32	*state = 0;
	u8	*slist = 0;
	int	lines = 0, print = 0, error = 0;
	int	seq;
	int	encoding = (flags&GET_ASCII) ? E_ASCII : s->encoding_in;
	unsigned int sum;
	u32	same, added, deleted, other, *counter;
	FILE 	*out = 0;
	char	*buf, *name = 0, *gfile = 0;
	MDBM	*DB = 0;
	int	hash = 0;
	int	sccs_expanded, rcs_expanded;
	int	lf_pend = 0;
	char	*eol = "\n";
	ser_t	serial;
	char	align[16];
	char	lnamebuf[MD5LEN+32]; /* md5sum + '.' + linenumber */
	MDBM	*namedb = 0;
	u32	*lnum = 0;
	u32	fastflags = (NEWCKSUM|GET_SUM|GET_SHUTUP|SILENT|PRINT);

	assert(!BAM(s));
	if (EOLN_WINDOWS(s)) eol = "\r\n";
	slist = d ? serialmap(s, d, iLst, xLst, &error)
		  : setmap(s, D_SET, 0);
	if (error) {
		assert(!slist);
		switch (error) {
		    case 1:
			fprintf(stderr,
			    "Malformed include/exclude list for %s\n",
			    s->sfile);
			break;
		    case 2:
			fprintf(stderr,
			    "Can't find specified rev in include/exclude "
			    "list for %s\n", s->sfile);
			break;
		    case 3:
			fprintf(stderr,
			    "Error in include/exclude:\n"
			    "\tSame revision appears "
			    "in both lists for %s\n", s->sfile);
			break;
		    default:
			fprintf(stderr,
			    "Error in converting version plus include/exclude "
			    "to a set for %s\n", s->sfile);
			break;
		}
		s->state |= S_WARNED;
		return 1;
	}
	if (flags & GET_SUM) {
		flags |= NEWCKSUM;
	} else if (d && BITKEEPER(s) && !iLst && !xLst) {
		flags |= NEWCKSUM;
	}
	/* we're changing the meaning of the file, checksum would be invalid */
	if (HASH(s)) {
		if (flags & (GET_NOHASH|GET_SKIPGONE)) flags &= ~NEWCKSUM;
	}

	if (HASH(s) && !(flags & GET_NOHASH)) {
		hash = 1;
		unless ((encoding & E_DATAENC) == E_ASCII) {
			fprintf(stderr, "get: has files must be ascii.\n");
			s->state |= S_WARNED;
			goto out;
		}
		unless (DB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
			fprintf(stderr, "get: bad MDBM.\n");
			s->state |= S_WARNED;
			goto out;
		}
		assert(proj_root(s->proj));
	}

	/*
	 * Performance shortcut for sccs_renum(). See Notes/FASTSUM.
	 */
	if (!hash &&
	    ((flags & (fastflags|GET_HASHONLY)) == fastflags) &&
	    ((encoding & E_DATAENC) == E_ASCII)) {
		int	rc = fastsum(s, slist, SERIAL(s, d));

		free(slist);
		return (rc);
	}

	/* Think carefully before changing this */
	if (BINARY(s) || hash) {
		flags &= ~(GET_EXPAND|GET_PREFIX);
	}
	unless (SCCS(s) || RCS(s)) flags &= ~GET_EXPAND;

	if (flags & GET_LINENAME) {
		lnum = calloc(s->nextserial, sizeof(*lnum));
		namedb = mdbm_mem();
	}
	if (flags & GET_MODNAME) {
		name = basenm(d ? PATHNAME(s, d) : s->gfile);
	} else if (flags & GET_RELPATH) {
		name = d ? PATHNAME(s, d) : s->gfile;
	}

	/*
	 * We want the data to start on a tab aligned boundry
	 */
	if ((flags & GET_PREFIX) && (flags & GET_ALIGN)) {
		delta	*d2;
		int	len;

		s->revLen = s->userLen = 0;
		EACHP(s->slist, d2) {
			len = strlen(REV(s, d2));
			if (len > s->revLen) s->revLen = len;
			len = strlen(USER(s, d2));
			if (len > s->userLen) s->userLen = len;
		}

		len = 0;
		if (flags&(GET_MODNAME|GET_RELPATH)) len += strlen(name) + 1;
		if (flags&GET_PREFIXDATE) len += YEAR4(s) ? 11 : 9;
		if (flags&GET_USER) len += s->userLen + 1;
		if (flags&GET_REVNUMS) len += s->revLen + 1;
		if (flags&GET_LINENUM) len += 7;
		if (flags&GET_LINENAME) len += 37;
		if (flags&GET_SERIAL) len += 7;
		/* XXX GET_MD5KEY */
		len += 2;
		align[0] = 0;
		while (len++ % 8) strcat(align, " ");
		strcat(align, "| ");
	}

	unless (flags & (GET_HASHONLY|GET_SUM)) {
		gfile = d ? setupOutput(s, printOut, flags, d) : printOut;
		if ((gfile == (char *) 0) || (gfile == (char *)-1)) {
out:			if (slist) free(slist);
			if (state) free(state);
			if (DB) mdbm_close(DB);
			/*
			 * 0 == OK
			 * 1 == error
			 * 2 == No reget
			 */
			unless (gfile) return (2);
			return (1);
		}
		openOutput(s, encoding, gfile, &out);
		unless (out) {
			fprintf(stderr,
			    "get_reg: Can't open %s for writing\n", gfile);
			perror(gfile);
			fflush(stderr);
			goto out;
		}
	}
	seq = 0;
	sum = 0;
	added = 0;
	deleted = 0;
	same = 0;
	other = 0;
	counter = &other;
	sccs_rdweaveInit(s);
	while (buf = sccs_nextdata(s)) {
		register u8 *e, *e1, *e2;

		e1= e2 = 0;
		if (isData(buf)) {
			++seq;
			(*counter)++;
			if (lnum) { /* count named lines */
				lnum[print ? print : whatstate(state)]++;
			}
			if (buf[0] == CNTLA_ESCAPE) {
				assert((encoding & E_DATAENC) == E_ASCII);
				buf++; /* skip the escape character */
			}
			if (!print) {
				/* if we are skipping data from pending block */
				if (lf_pend && (lf_pend == whatstate(state))) {
					unless (flags & GET_SUM) {
						fputs(eol, out);
					}
					if (flags & NEWCKSUM) sum += '\n';
					lf_pend = 0;
				}
				continue;
			}
			if (hash) {
				if (getKey(s, DB, buf, flags) == 1) {
					unless (flags &
					    (GET_HASHONLY|GET_SUM)) {
						fputs(buf, out);
						fputc('\n', out);
					}
					if (flags & NEWCKSUM) {
						for (e = buf; *e; sum += *e++);
						sum += '\n';
					}
					lines++;
				}
				continue;
			}
			lines++;
			if (lf_pend) {
				unless (flags & GET_SUM) fputs(eol, out);
				if (flags & NEWCKSUM) sum += '\n';
				lf_pend = 0;
			}
			if (flags & NEWCKSUM) {
				for (e = buf; *e; sum += *e++);
				sum += '\n';
			}
			if (flags&GET_SEQ) smerge_saveseq(seq);
			if (flags & GET_PREFIX) {
				delta	*tmp = sfind(s, (ser_t) print);
				char	*p = 0;

				prefix(s, tmp, flags, lines, name, out);

				/* GET_LINENAME must be last for mdiff */
				if (flags & GET_LINENAME) {
					p = get_lineName(s, print,
					    namedb, lnum[print], lnamebuf);
					assert(p &&
					    strlen(p) < sizeof(lnamebuf));
				}
				if (flags & GET_ALIGN) {
					if (p) fprintf(out, "%-36s", p);
					fputs(align, out);
				} else {
					if (p) fprintf(out, "%s\t", p);
				}
			}

			e = buf;
			sccs_expanded = rcs_expanded = 0;
			unless (flags & GET_EXPAND) goto write;
			if (SCCS(s)) {
				for (e = buf; *e && (*e != '%'); e++);
				if (*e == '%') {
					e = e1 =
					    expand(s, d, buf, &sccs_expanded);
					if (sccs_expanded && EXPAND1(s)) {
						flags &= ~GET_EXPAND;
					}
				} else {
					e = buf;
				}
			}
			if (RCS(s)) {
				char	*t;

				for (t = e; *t && (*t != '$'); t++);
				if (*t == '$') {
					e = e2 =
					    rcsexpand(s, d, e, &rcs_expanded);
					if (rcs_expanded && EXPAND1(s)) {
						flags &= ~GET_EXPAND;
					}
				}
			} 

write:
			switch (encoding & (E_COMP|E_DATAENC)) {
			    case E_GZIP|E_UUENCODE:
			    case E_UUENCODE: {
				uchar	obuf[50];
				int	n;
				
				unless (flags & GET_SUM) {
					n = uudecode1(e, obuf);
					fwrite(obuf, n, 1, out);
				}
				break;
			    }
			    case E_ASCII:
			    case E_ASCII|E_GZIP:
				unless (flags & GET_SUM) fputs(e, out);
				if (flags & NEWCKSUM) sum -= '\n';
				lf_pend = print;
				if (sccs_expanded) free(e1);
				if (rcs_expanded) free(e2);
				break;
			    default:
				assert(0);
				break;
			}
			continue;
		}

		debug2((stderr, "%s", buf));
		serial = atoi(&buf[3]);
		/* seek out E which closes text block for last line
		 * printed.  serial for that block is in lf_pend.
		 * whatstate returns serial of current text block
		 * This is needed to make sure E isn't closing a D
		 * The whatstate test isn't needed assuming diff
		 * semantics of a replace is a delete followed
		 * by an insert.  We know in that case, the condition
		 * E and lf_pend == serial is enough because if E
		 * is tagged 'N', there can be no 'D' following it.
		 * If E isn't tagged, then lf_pend gets cleared.
		 * In the name of robustness, that someday this
		 * assumption might not be true, the whatstate check
		 * is in there.
		 */
		if (buf[1] == 'E' && lf_pend == serial &&
		    whatstate(state) == serial) {
			char	*n = &buf[3];
			while (isdigit(*n)) n++;
			if (*n != 'N') {
				unless (flags & GET_SUM) fputs(eol, out);
				lf_pend = 0;
				if (flags & NEWCKSUM) sum += '\n';
			} else {
				s->has_nonl = 1;
			}
		}
		state = changestate(state, buf[1], serial);
		if (d) {
			print = printstate(state, slist);
			unless (flags & NEWCKSUM) {
				/* don't recalc add/del/same unless CKSUM */
			}
			else if (print == SERIAL(s, d)) {
				counter = &added;
			}
			else if (print) {
				counter = &same;
			}
			else if (delstate(SERIAL(s, d), state, slist)) {
				counter = &deleted;
			}
			else {
				counter = &other;
			}
		}
		else {
			print = visitedstate(state, slist);
		}
	}
	if (BITKEEPER(s) &&
	    d && (flags & NEWCKSUM) && !(flags&GET_SHUTUP) && lines) {
		delta	*z = getCksumDelta(s, d);

		if (!z || ((sum_t)sum != z->sum)) {
		    fprintf(stderr,
			"get: bad delta cksum %u:%d for %s in %s, %s\n",
			(sum_t)sum, z ? z->sum : -1, REV(s, d), s->sfile,
			"gotten anyway.");
		}
	}
	/* Try passing back the sum in dsum in case someone wants it */
	s->dsum = sum;
	s->added = added;
	s->deleted = deleted;
	s->same = same;

	if (flags & (GET_HASHONLY|GET_SUM)) {
		error = 0;
	} else {
		if (error = flushFILE(out)) {
			/*
			 * In spite of flushFILE() looking like it catches
			 * EPIPE, it doesn't.  So we look for that case
			 * here.
			 */
			unless ((flags&PRINT) && streq("-", printOut)) {
				perror(s->gfile);
				s->io_error = s->io_warned = 1;
			} else {
				error = 0;
			}
		}
		if (flags & PRINT) {
			unless (streq("-", printOut)) fclose(out);
		} else {
			fclose(out);
		}
	}
	if (sccs_rdweaveDone(s)) {
		error = 1;
		s->io_error = s->io_warned = 1;
	}

	if (error) {
		unless (flags & PRINT) unlink(s->gfile);
		if (DB) mdbm_close(DB);
		return (1);
	}
#ifdef X_SHELL
	if (SHELL(s) && ((flags & PRINT) == 0)) {
		char	*path = strdup(getenv("PATH"));
		char	*t;
		char	cmd[MAXPATH];

		safe_putenv("PATH=%s", getenv("BK_OLDPATH"));
		t = strrchr(s->gfile, '/');
		if (t) {
			*t = 0;
			sprintf(cmd, "cd '%s'; sh '%s' -o", s->gfile, &t[1]);
			*t = '/';
		} else {
			sprintf(cmd, "sh '%s' -o", s->gfile);
		}
		system(cmd);
		safe_putenv("PATH=%s", path);
		free(path);
	}
#endif
	*ln = lines;
	if (slist) free(slist);
	if (state) free(state);
	if (namedb) mdbm_close(namedb);
	if (lnum) free(lnum);
	if (DB) {
		if (s->mdbm) mdbm_close(s->mdbm);
		s->mdbm = DB;
	}
	return 0;
}

private int
get_bp(sccs *s, char *printOut, int flags, delta *d,
		int *ln, char *iLst, char *xLst)
{
	char	*gfile = 0;
	int	error = 0;

	assert(BAM(s) && BITKEEPER(s));
	assert(d);

	/*
	 * Supported flags are: GET_EDIT (handled elsewhere),
	 * GET_SKIPGET (handled elsewhere)
	 * GET_SHUTUP (dunno)
	 * GET_FORCE (dunno)
	 * GET_DTIME
	 * GET_NOREGET (handled in setupOutput)
	 * GET_SUM
	 */
#define	BAD	(GET_PREFIX|GET_ASCII|GET_ALIGN|\
		GET_NOHASH|GET_HASHONLY|GET_DIFFS|GET_BKDIFFS|\
		GET_SKIPGONE|GET_SEQ|GET_COMMENTS)
	if (flags & BAD) {
		fprintf(stderr,
		    "get: bad flags on get for %s: %x\n", s->gfile, flags);
		return (1);
	}

	unless (flags & GET_SUM) {
		gfile = setupOutput(s, printOut, flags, d);
		if ((gfile == (char *) 0) || (gfile == (char *)-1)) {
			/*
			 * 1 == error
			 * 2 == No reget
			 */
			unless (gfile) return (2);
			return (1);
		}
	}
	if (error = bp_get(s, d, flags, gfile)) {
		unless (error == EAGAIN) return (1);
		if (flags & GET_NOREMOTE) {
			s->cachemiss = 1;
			return (1);
		} else if (bp_fetch(s, d)) {
			fprintf(stderr, "BAM: fetch failed for %s\n", s->gfile);
			return (1);
		} else if (error = bp_get(s, d, flags, gfile)) {
			fprintf(stderr,
			    "BAM: get after fetch failed for %s\n", s->gfile);
			return (1);
		}
	}
	/* Track get_reg from here on down (mostly) */
	if (BITKEEPER(s) &&
	    (flags & NEWCKSUM) && !(flags & GET_SHUTUP) && s->added) {
		delta	*z = getCksumDelta(s, d);

		if (!z || (s->dsum != z->sum)) {
		    fprintf(stderr,
			"get: bad delta cksum %u:%d for %s in %s, %s\n",
			s->dsum, z ? z->sum : -1, REV(s, d), s->sfile,
			"gotten anyway.");
		}
	}
	if (error) {
		fprintf(stderr, "get_bp: cannot chmod %s\n", s->gfile);
		perror(s->gfile);
	}
	if (ln) *ln = s->added;
	return (error);
}

private int
get_link(sccs *s, char *printOut, int flags, delta *d, int *ln)
{
	char *f = setupOutput(s, printOut, flags, d);
	u8 *t;
	u16 dsum = 0;
	delta	*e;

	unless (f && f != (char *)-1) return 2;

	/*
	 * What we want is to just checksum the symlink.
	 * However due two bugs in old binary, we do not have valid check if:
	 * a) It is a 1.1 delta
	 * b) It is 1.1.* delta (the 1.1 delta got moved after a merge)
	 * c) The recorded checsum is zero.
	 */
	e = getSymlnkCksumDelta(s, d);
	if ((e->flags & D_CKSUM) && (e->sum != 0) &&
	    !streq(REV(s, e), "1.1") && !strneq(REV(s, e), "1.1.", 4)) {
		for (t = SYMLINK(s, d); *t; t++) dsum += *t;
		if (e->sum != dsum) {
			fprintf(stderr,
				"get: bad delta cksum %u:%d for %s in %s, %s\n",
			        dsum, d->sum, REV(s, d), s->sfile,
				"gotten anyway.");
		}
	}
	if ((flags & PRINT) && !(flags & GET_PERMS)) {
		int	ret;
		FILE 	*out;

		ret = openOutput(s, E_ASCII, f, &out);
		assert(ret == 0);
		unless (out) {
			fprintf(stderr,
			    "get_link: Can't open %s for writing\n", f);
			fflush(stderr);
			return 1;
		}
		if (flags & GET_PREFIX) {
			char	*name = 0;

			assert(d->pathname);
			if (flags & GET_MODNAME) name = basenm(PATHNAME(s, d));
			if (flags & GET_RELPATH) name = PATHNAME(s, d);
			prefix(s, d, flags, 1, name, out);
			if (flags & GET_ALIGN) {
				int	len = 0;

				if (flags&(GET_MODNAME|GET_RELPATH)) {
					len += strlen(name) + 1;
				}
				if (flags&GET_PREFIXDATE) {
					len += YEAR4(s) ? 11 : 9;
				}
				if (flags&GET_USER) len += s->userLen + 1;
				if (flags&GET_REVNUMS) len += s->revLen + 1;
				if (flags&GET_LINENUM) len += 7;
				if (flags&GET_SERIAL) len += 7;
				/* XXX GET_MD5KEY */
				len += 2;
				while (len++ % 8) fputc(' ', out);
				fputs("| ", out);
			}
			// XXX - no GET_LINENAME (yet)
		}
		fprintf(out, "SYMLINK -> %s\n", SYMLINK(s, d));
		unless (streq("-", f)) fclose(out);
		*ln = 1;
	} else {
		mkdirf(f);
		unless (symlink(SYMLINK(s, d), f) == 0 ) {
#ifdef WIN32
			if (getenv("BK_WARN_SYMLINK")) {
				getMsg("symlink", s->gfile, '=', stderr);
			}
#else
			perror(f);
#endif
			return 1;
		}
		*ln = 0;
	}
	return 0;
}

/*
 * get the specified revision.
 * The output file is passed in so that callers can redirect it.
 * iLst and xLst are malloced and get() frees them.
 */
int
sccs_get(sccs *s, char *rev,
	char *mRev, char *iLst, char *xLst, u32 flags, char *printOut)
{
	delta	*d;
	int	lines = -1, locked = 0, error;
	char	*i2 = 0;

	debug((stderr, "get(%s, %s, %s, %s, %s, %x, %s)\n",
	    s->sfile, notnull(rev), notnull(mRev),
	    notnull(iLst), notnull(xLst), flags, printOut));
	if (BITKEEPER(s) && !s->tree->pathname) {
		fprintf(stderr, "get: no pathname for %s\n", s->sfile);
		return (-1);
	}
	unless (s->state & S_SOPEN) {
		fprintf(stderr, "get: couldn't open %s\n", s->sfile);
err:		if (i2) free(i2);
		if (locked) {
			sccs_unlock(s, 'p');
			sccs_unlock(s, 'z');
			s->state &= ~(S_PFILE|S_ZFILE);
		}
		return (-1);
	}
	unless (s->cksumok) {
		fprintf(stderr, "get: bad chksum on %s\n", s->sfile);
		goto err;
	}
	unless (HASGRAPH(s)) {
		fprintf(stderr, "get: no/bad delta tree in %s\n", s->sfile);
		goto err;
	}
	if ((s->state & S_BADREVS) && !(flags & GET_FORCE)) {
		fprintf(stderr,
		    "get: bad revisions, run renumber on %s\n", s->sfile);
		s->state |= S_WARNED;
		goto err;
	}

	if (BAM(s) && bk_notLicensed(s->proj, LIC_BAM, 0)) {
		s->state |= S_WARNED;
		goto err;
	}

	/* this has to be above the sccs_getedit() - that changes the rev */
	if (mRev) {
		char *tmp;

		tmp = sccs_impliedList(s, "get", rev, mRev);
#ifdef  CRAZY_WOW
		// XXX: why was this here?  Should revisions that are
		// inline (get -e -R1.7 -M1.5 foo) be an error?
		// Needed to take this out because valid merge set
		// could be empty (see t.merge for example)
		unless (tmp) goto err;
#endif
		/* XXX this is bogus if tmp==0 and iLst is set */
		i2 = strconcat(tmp, iLst, ",");
		if (tmp && i2 != tmp) free(tmp);
	}
	if (rev && streq(rev, "+")) rev = 0;
	if ((flags & (GET_EDIT|PRINT)) == GET_EDIT) {
		d = sccs_getedit(s, &rev);
		if (!d) {
			fprintf(stderr, "get: can't find revision %s in %s\n",
			    notnull(rev), s->sfile);
			s->state |= S_WARNED;
		}
	} else {
		d = sccs_findrev(s, rev ? rev : "+");
		unless (d) {
			verbose((stderr,
			    "get: can't find revision like %s in %s\n",
			rev, s->sfile));
			s->state |= S_WARNED;
		}
	}
	unless (d) goto err;

	if ((flags & (GET_EDIT|PRINT)) == GET_EDIT) {
		if (write_pfile(s, flags, d, rev, iLst, i2, xLst, mRev)) {
			goto err;
		}
		locked = 1;
	} 
#define	NOGFILE	(PRINT | GET_SKIPGET | \
		GET_HASHONLY | GET_DIFFS | GET_BKDIFFS)
	else if (HAS_PFILE(s) && !HAS_GFILE(s) && !(flags & NOGFILE)) {
		pfile	pf;
		int	rc;
		char	*gfile = sccs2name(s->sfile);

		/*
		 * If we have a pfile, no gfile, and we're getting the file,
		 * the the pfile is leftover crud and we should lose it.
		 * This happens when someone does "bk edit foo; rm foo".
		 * The streq below is to see if someone did a get -G,
		 * difftool used to do that.
		 *
		 * Eagle eye Rick points out that we probably don't want to
		 * lose merge pointers.
		 */
		if (streq(gfile, s->gfile) &&
		    (sccs_read_pfile("co", s, &pf) == 0)) {
			rc = 0;
			if (pf.mRev || pf.iLst || pf.xLst) {
			    rc = 1;
			    fprintf(stderr,
				"%s has merge|include|exclude "
				"but no gfile, co aborted.\n",
				s->gfile);
			}
			free_pfile(&pf);
			free(gfile);
			if (rc) goto err;
			unlink(s->pfile);
			s->state &= ~S_PFILE;
		} else {
			free(gfile);
		}
	}

	if (flags & GET_SKIPGET) {
		/*
		 * XXX - need to think about this for various file types.
		 * Remove read only files if we are editing.
		 * Do not error if there is a writable gfile, they may have
		 * wanted that.
		 */
		if ((flags & GET_EDIT) && HAS_GFILE(s) && !WRITABLE(s)) {
			unlink(s->gfile);
		}
		goto skip_get;
	}

	/*
	 * Based on the file type,
	 * we call the appropriate function to get the body
 	 */
	switch (fileType(d->mode)) {
	    case 0:		/* uninitialized mode, assume regular file */
	    case S_IFREG:	/* regular file */
		if (BAM(s)) {
			error = get_bp(s, printOut, flags, d, &lines, 0, 0);
			break;
		}
		error =
		    get_reg(s, printOut, flags, d, &lines, i2? i2 : iLst, xLst);
		break;
	    case S_IFLNK:	/* symlink */
		error = get_link(s, printOut, flags, d, &lines);
		break;
	    default:
		fprintf(stderr, "get unsupported file type %d\n",
			fileType(d->mode));
		error = 1;
	}
	switch (error) {
	    case 0: break;
	    case 1: goto err;
	    case 2: flags |= SILENT; error = 0; break;	/* reget; no get */
	    default:
		assert("bad error return in get" == 0);
	}
	/* Win32 restriction, must do this before we chmod to read only */
	if (!S_ISLNK(d->mode) && (flags & GET_DTIME)) {
		char	*fname = (flags&PRINT) ? printOut : s->gfile;
		int	doit = 1;
		time_t	now;
		struct	utimbuf ut;

		/*
		 * If we are doing a regular SCCS/s.foo -> foo get then
		 * we set the gfile time iff we can set the sfile time.
		 * This keeps make happy.
		 */
		ut.modtime = d->date - d->dateFudge;
		now = time(0);
		if (ut.modtime > now) ut.modtime = now;
		unless (flags & PRINT) {
			s->gtime = ut.modtime;
			if (sccs_setStime(s, s->stime)) doit = 0;
		}
		ut.actime = ut.modtime;
		if (doit && !streq(fname, "-") && (utime(fname, &ut) != 0)) {
			char	*msg;

			msg = aprintf("Cannot set mod time on %s:", fname);
			perror(msg);
			free(msg);
			s->state |= S_WARNED;
			goto err;
		}
	}
	if (!S_ISLNK(d->mode) &&
	    ((flags & GET_PERMS) || !(flags & NOGFILE))) {
		char	*fname = (flags&PRINT) ? printOut : s->gfile;
		mode_t	mode;

		mode = d->mode ? d->mode : 0666;
		unless (flags & GET_EDIT) mode &= ~0222;

		if (chmod(fname, mode)) {
			fprintf(stderr,
			    "get_reg: cannot chmod %s\n", fname);
			perror(fname);
		}
	}
	debug((stderr, "GET done\n"));

skip_get:
	if ((flags & (GET_EDIT|PRINT)) == GET_EDIT) {
		sccs_unlock(s, 'z');
		s->state &= ~S_ZFILE;
	}
	if (!(flags&SILENT)) {
		fprintf(stderr, "%s %s", s->gfile, REV(s, d));
		if (i2) {
			fprintf(stderr, " inc: %s", i2);
		} else if (iLst) {
			fprintf(stderr, " inc: %s", iLst);
		}
		if (xLst) {
			fprintf(stderr, " exc: %s", xLst);
		}
		if ((flags & (GET_EDIT|PRINT)) == GET_EDIT) {
			fprintf(stderr, " -> %s", rev);
		}
		unless (flags & GET_SKIPGET) {
			if (lines >= 0) {
				if (BINARY(s)) {
					fprintf(stderr, ": %s", psize(lines));
				} else {
					fprintf(stderr, ": %u lines", lines);
				}
			}
		}
		fprintf(stderr, "\n");
	}
	if (i2) free(i2);
	return (0);
}

/*
 * XXX - the userLen/revLen should be calculated for the set of serials that
 * we are displaying, not the full set.
 */
private void
prefix(sccs *s, delta *d, u32 flags, int lines, char *name, FILE *out)
{
	char	buf[32];

	if (flags & GET_ALIGN) {
		if (flags&(GET_MODNAME|GET_RELPATH)) fprintf(out, "%s ", name);
		if (flags&GET_PREFIXDATE) {
			delta_strftime(buf, sizeof(buf),
			    YEAR4(s) ? "%Y/%m/%d " : "%y/%m/%d ", s, d);
			fputs(buf, out);
		}
		if (flags&GET_USER) fprintf(out, "%-*s ", s->userLen, USER(s, d));
		if (flags&GET_REVNUMS) {
			fprintf(out, "%-*s ", s->revLen, REV(s, d));
		}
		if (flags&GET_LINENUM) fprintf(out, "%6d ", lines);
		if (flags&GET_SERIAL) fprintf(out, "%6d ", SERIAL(s, d));
		/* XXX GET_MD5KEY  */
	} else {
		/* tab style */
		if (flags&(GET_MODNAME|GET_RELPATH)) fprintf(out, "%s\t",name);
		if (flags&GET_PREFIXDATE) {
			delta_strftime(buf, sizeof(buf),
			    YEAR4(s) ? "%Y/%m/%d\t" : "%y/%m/%d\t", s, d);
			fputs(buf, out);
		}
		if (flags&GET_USER) fprintf(out, "%s\t", USER(s, d));
		if (flags&GET_REVNUMS) fprintf(out, "%s\t", REV(s, d));
		if (flags&GET_LINENUM) fprintf(out, "%d\t", lines);
#if 0
		if (flags&GET_MD5KEY) {
			char	key[MD5LEN];

			sccs_md5delta(s, d, key);
			fprintf(out, "%s\t", key);
		}
#endif
		if (flags&GET_SERIAL) fprintf(out, "%d\t", SERIAL(s, d));
	}
}

/*
 * cat the delta body formatted according to flags.
 */
int
sccs_cat(sccs *s, u32 flags, char *printOut)
{
	int	lines = 0, error;

	debug((stderr, "annotate(%s, %x, %s)\n",
	    s->sfile, flags, printOut));
	unless (s->state & S_SOPEN) {
		fprintf(stderr, "annotate: couldn't open %s\n", s->sfile);
err:		return (-1);
	}
	unless (s->cksumok) {
		fprintf(stderr, "annotate: bad chksum on %s\n", s->sfile);
		goto err;
	}
	unless (HASGRAPH(s)) {
		fprintf(stderr, "annotate: no delta tree in %s\n", s->sfile);
		goto err;
	}
	if ((s->state & S_BADREVS) && !(flags & GET_FORCE)) {
		fprintf(stderr,
		    "annotate: bad revisions, run renumber on %s\n", s->sfile);
		s->state |= S_WARNED;
		goto err;
	}
	if (BINARY(s)) {
		fprintf(stderr,
		    "annotate: can't annotate binary %s\n", s->gfile);
		s->state |= S_WARNED;
		goto err;
	}
	error = get_reg(s, printOut, flags, 0, &lines, 0, 0);
	if (error) return (-1);

	debug((stderr, "SCCSCAT done\n"));

	return (0);
}

/* These are used to describe 'side' of a diff */
#define	NEITHER 0
#define	LEFT	1
#define	RIGHT	2
#define	BOTH	3

private int
outdiffs(sccs *s, int type, int side, int *left, int *right, int count,
	int no_lf, FILE *in, FILE *out)
{
	char	*prefix;

	unless (count)  return (0);

	if (side == RIGHT) {
		if (type == GET_BKDIFFS) {
			fprintf(out, "%c%d %d\n", no_lf ? 'N' : 'I',
				*left, count);
			*right += count; /* not used, but easy to inc */
			prefix = "";
		} else {
			fprintf(out, "%da%d", *left, *right+1);
			*right += count;
			if (count != 1)
				fprintf(out, ",%d", *right);
			fputc('\n', out);
			prefix = "> ";
		}
	} else {
		assert(side == LEFT);
		if (type == GET_BKDIFFS) {
			fprintf(out, "D%d %d\n", *left+1, count);
			*left += count;
			prefix = "";
		} else {
			fprintf(out, "%d", *left+1);
			*left += count;
			if (count != 1)
				fprintf(out, ",%d", *left);
			fprintf(out, "d%d\n", *right);
			prefix = "< ";
		}
	}

	/* print out text block unless a diff -n type of delete */
	unless (type == GET_BKDIFFS && side == LEFT) {
		fseek(in, 0L, SEEK_SET);
		while (count--) {
			char	buf[MAXLINE];

			fputs(prefix, out);
			if (fnext(buf, in)) {
				/*
				 * We're escaping blank lines and the escape
				 * character itself.  Takepatch wants the
				 * diffs ended with ^\n so blank lines confuse
				 * it.
				 * Note that these are stripped in the delta
				 * routine.
				 */
				if ((buf[0] == '\\') || (buf[0] == '\n')) {
					fputs("\\", out);
				}
				fputs(buf, out);

				/*
				 * This loop is here to handle line
				 * that is longer than the MAXLINE buffer size
				 *
				 * XXX TODO: should mmap the tmp file
				 */
				while (buf[strlen(buf) - 1] != '\n') {
					unless (fnext(buf, in)) {
						fprintf(stderr,
		    "get: getdiffs temp file has no line-feed termination\n");
						fputs(buf, out);
						break;
					}
					fputs(buf, out);
				}
			} else {
				fprintf(stderr,
				    "get: getdiffs temp file early EOF\n");
				s->state |= S_WARNED;
				return (-1);
			}
		}
		fseek(in, 0L, SEEK_SET);
	}
	return (0);
}

/*
 * Get the diffs of the specified revision.
 * The diffs are only in terms of deletes and adds, no changes.
 * The output file is passed in so that callers can redirect it.
 */
int
sccs_getdiffs(sccs *s, char *rev, u32 flags, char *printOut)
{
	int	type = flags & (GET_DIFFS|GET_BKDIFFS);
	ser_t	*state = 0;
	u8	*slist = 0;
	ser_t	old = 0;
	delta	*d;
	int	with = 0, without = 0;
	int	count = 0, left = 0, right = 0;
	FILE	*out = 0;
	int	encoding = (flags&GET_ASCII) ? E_ASCII : s->encoding_in;
	int	error = 0;
	int	side, nextside;
	char	*buf;
	char	*tmpfile = 0, *tmppat = 0;
	FILE	*lbuf = 0;
	int	no_lf = 0;
	ser_t	serial;
	int	ret = -1;

	unless (s->state & S_SOPEN) {
		fprintf(stderr, "getdiffs: couldn't open %s\n", s->sfile);
		s->state |= S_WARNED;
		return (-1);
	}
	unless (s->cksumok) {
		fprintf(stderr, "getdiffs: bad chksum on %s\n", s->sfile);
		s->state |= S_WARNED;
		return (-1);
	}
	unless (HASGRAPH(s)) {
		fprintf(stderr,
		    "getdiffs: no/bad delta tree in %s\n", s->sfile);
		s->state |= S_WARNED;
		return (-1);
	}
	if (s->state & S_BADREVS) {
		fprintf(stderr,
		    "getdiffs: bad revisions, run renumber on %s\n", s->sfile);
		s->state |= S_WARNED;
		return (-1);
	}
	unless (d = findrev(s, rev)) {
		fprintf(stderr, "get: can't find revision like %s in %s\n",
		    rev, s->sfile);
		s->state |= S_WARNED;
		return (-1);
	}
	tmppat = aprintf("%s-%d", basenm(s->gfile), SERIAL(s, d));
	tmpfile = bktmp(0, tmppat);
	free(tmppat);
	openOutput(s, encoding, printOut, &out);
	setmode(fileno(out), O_BINARY); /* for win32 EOLN_NATIVE file */
	unless (lbuf = fopen(tmpfile, "w+")) {
		perror(tmpfile);
		fprintf(stderr, "getdiffs: couldn't open %s\n", tmpfile);
		s->state |= S_WARNED;
		goto done2;
	}
	slist = serialmap(s, d, 0, 0, &error);
	sccs_rdweaveInit(s);
	side = NEITHER;
	nextside = NEITHER;

	while (buf = sccs_nextdata(s)) {
		unless (isData(buf)) {
			debug2((stderr, "%s", buf));
			serial = atoi(&buf[3]);
			if (buf[1] == 'E' && serial == with &&
			    serial == SERIAL(s, d))
			{
				char	*n = &buf[3];
				while (isdigit(*n)) n++;
				if (*n == 'N') no_lf = 1;
			}
			state = changestate(state, buf[1], serial);
			with = printstate(state, slist);
			old = slist[SERIAL(s, d)];
			slist[SERIAL(s, d)] = 0;
			without = printstate(state, slist);
			slist[SERIAL(s, d)] = old;

			nextside = with ? (without ? BOTH : RIGHT)
					: (without ? LEFT : NEITHER);
			continue;
		}
		if (nextside == NEITHER) continue;
		if (count &&
		    nextside != side && (side == LEFT || side == RIGHT)) {
			if (outdiffs(s, type,
			    side, &left, &right, count, no_lf, lbuf, out)) {
				goto done;
			}
			count = 0;
			no_lf = 0;
		}
		side = nextside;
		switch (side) {
		    case LEFT:
		    case RIGHT:
			count++;
			if ((type == GET_DIFFS) || (side == RIGHT)) {
				if (buf[0] == CNTLA_ESCAPE) {
					assert((encoding & E_DATAENC)
					    == E_ASCII);
					buf++; /* skip the escape character */
				}
				fputs(buf, lbuf);
				fputc('\n', lbuf);
			}
			break;
		    case BOTH:
			left++, right++; break;
		}
	}
	if (count) { /* there is something left in the buffer */
		if (outdiffs(s, type, side, &left, &right, count, no_lf,
		    lbuf, out))
		{
			goto done;
		}
		count = 0;
		no_lf = 0;
	}
	ret = 0;
done:   ret = sccs_rdweaveDone(s);
done2:	/* for GET_HASHDIFFS, the encoding has been handled in get_reg() */
	if (lbuf) {
		if (flushFILE(lbuf)) {
			s->io_error = 1;
			ret = -1; /* i/o error: no disk space ? */
		}
		fclose(lbuf);
	}
	if (flushFILE(out)) {
		s->io_error = 1;
		ret = -1; /* i/o error: no disk space ? */
	}
	unless (streq("-", printOut)) fclose(out);
	if (slist) free(slist);
	if (state) free(state);
	if (tmpfile) {
		unlink(tmpfile);
		free(tmpfile);
	}
	return (ret);
}

/*
 * Output a diff for a block of deltas.
 * pmap - maps the serials in the table to serials in the patch
 * only output those with a nonzero pmap[serial].
 * When counting line numbers, include D_SET deltas.
 */
int
sccs_patchDiffs(sccs *s, ser_t *pmap, char *printOut)
{
	ser_t	*state = 0;
	delta	*d;
	char	*n, type, patchcmd;
	u8	*sump, *buf;
	int	track = 0, print = 0, lineno = 0;
	ser_t	serial;
	int	ret = -1;
	FILE	*out = 0;
	sum_t	sum = 0;

	unless (s->cksumok) {
		fprintf(stderr, "getdiffs: bad chksum on %s\n", s->sfile);
		s->state |= S_WARNED;
		return (-1);
	}
	if (s->state & S_BADREVS) {
		fprintf(stderr,
		    "getdiffs: bad revisions, run renumber on %s\n", s->sfile);
		s->state |= S_WARNED;
		return (-1);
	}
	openOutput(s, E_ASCII, printOut, &out);
	setmode(fileno(out), O_BINARY); /* for win32 EOLN_NATIVE file */

	unless (CSET(s)) {
		/*
		 * transitive close by marking all 1s in the
		 * history of non patch nodes
		 */
		for (d = s->table; d; d = NEXT(d)) {
			unless (pmap[SERIAL(s, d)]) continue;
			if (d->pserial && !pmap[d->pserial]) {
				pmap[d->pserial] = ~0;
			}
			if (d->merge && !pmap[d->merge]) {
				pmap[d->merge] = ~0;
			}
		}
	}
	sccs_rdweaveInit(s);

	fputs("F\n", out);
	while (buf = sccs_nextdata(s)) {
		unless (isData(buf)) {
			debug2((stderr, "%s", buf));
			type = buf[1];
			n = &buf[3];
			serial = atoi_p(&n);
			d = sfind(s, serial);
			assert(d);
			if (pmap[serial] && (pmap[serial] != ~0)) {
				patchcmd = type;
				if (*n == 'N') {
					assert(type == 'E');
					patchcmd = 'N';
				}
				/* yes, I know ?: isn't needed */
				fprintf(out, "%c%u %u\n",
				    patchcmd,
				    pmap[serial],
				    (lineno + ((type == 'D') ? 1 : 0)));
			}
			state = changestate(state, type, serial);
			if (track = whatstate(state)) {
				d = sfind(s, track);
				assert(d);
				if (track = pmap[track]) {
					print = (track != ~0);
				}
			}
			continue;
		}
		unless (track) continue;
		if (*buf == CNTLA_ESCAPE) buf++;
		sump = buf;
		while (*sump && (*sump != '\n')) sum += *sump++;
		sum += (u8) '\n';
		lineno++;
		if (print) fprintf(out, ">%.*s\n", (int)(sump-buf), buf);
	}
	fprintf(out, "K%u %u\n", sum, lineno);
	ret = sccs_rdweaveDone(s);
	if (flushFILE(out)) {
		s->io_error = 1;
		ret = -1; /* i/o error: no disk space ? */
	}
	unless (streq("-", printOut)) fclose(out);
	if (state) free(state);
	return (ret);
}

/*
 * Return true if bad cksum
 */
private int
badcksum(sccs *s, int flags)
{
	u8	*t;
	u32	sum = 0;
	int	i;
	int	filesum;
	u8	buf[4<<10];

	assert(s);
	rewind(s->fh);
	t = sccs_nextdata(s);
	if (t[0] == 'B') {
		/* ignore checksums for binary files for now */
		s->cksumok = 1;
		return (0);
	}
	s->cksum = filesum = atoi(&t[2]);
	s->cksumdone = 1;
	debug((stderr, "File says sum is %d\n", filesum));
	/* fread is not ideal here... */
	while ((i = fread(buf, 1, sizeof(buf), s->fh)) > 0) {
		t = buf;
		while (i >= 16) {
			sum +=
			    t[0] + t[1] + t[2] + t[3] +
			    t[4] + t[5] + t[6] + t[7] +
			    t[8] + t[9] + t[10] + t[11] +
			    t[12] + t[13] + t[14] + t[15];
			t += 16;
			i -= 16;
		}
		while (i--) sum += *t++;
	}
	debug((stderr, "Calculated sum is %d\n", (sum_t)sum));
	if ((sum_t)sum == filesum) {
		s->cksumok = 1;
	} else {
		fprintf(stderr,
		    "Bad checksum for %s, got %d, wanted %d\n",
		    s->sfile, (sum_t)sum, filesum);
	}
	debug((stderr,
	    "%s has %s cksum\n", s->sfile, s->cksumok ? "OK" : "BAD"));
	return ((sum_t)sum != filesum);
}

/*
 * If we are regular files, then return sameness in the modes.
 * Otherwise return sameness in the links.
 */
private int
sameMode(delta *a, delta *b)
{
	assert(a && b);
	if (S_ISREG(a->mode) && S_ISREG(b->mode)) return (a->mode == b->mode);
	unless (S_ISLNK(a->mode) && S_ISLNK(b->mode)) return (0);
	assert(a->symlink && b->symlink);
	return (a->symlink == b->symlink);
}

private int
sameFileType(sccs *s, delta *d)
{
	if (d->flags & D_MODE) {
		return (fileType(s->mode) == fileType(d->mode));
	} else {			/* assume no mode means regular file */
		return (S_ISREG(s->mode));
	}
}

/* Formatting routines used by delta_table.
 * sprintf() is so slow that it accounted for ~20% of execution time
 * of delta(1).
 * LMXXX - I've never believed this to be true, zack claimed it.
 * All of these write the requested data into the string provided
 * and return a pointer one past where they stopped writing, so you
 * can write code like
 * p = fmts(p, "blah");
 * p = fmtd(p, number);
 * ...
 * *p = '\0';
 * None of them null-terminate.
 * The names are all 'fmt' + what you'd put after the % in a sprintf format
 * spec (except "fmttt" which stands for "format time_t").
 */
private inline char *
fmts(register char *p, register char *s)
{
	while (*p++ = *s++);
	return (p - 1);
}

private char *
fmtu(char *p, unsigned int u)
{
	char tmp[10];  /* 4294967295 (2**32) has 10 digits.  */
	char *x = &tmp[10];

	if (u == 0) {
		*p++ = '0';
		return p;
	}

	do {
		*--x = u%10 + '0';
		u /= 10;
	} while (u);

	memcpy(p, x, 10 - (x - tmp));
	return p + 10 - (x - tmp);
}

private char *
fmtd(char *p, int d)
{
	if (d < 0) {
		*p++ = '-';
		d = -d;
	}

	return fmtu(p, d);
}

private char *
fmttt(char *p, time_t d)
{
	char tmp[21];  /* 18446744073709551616 (2**64) has 21 digits.
			* time_t may well be a 64 bit quantity.
			*/
	char *x = &tmp[21];

	/* time_t is signed! */
	if (d < 0) {
		*p++ = '-';
		d = -d;
	}

	if (d == 0) {
		*p++ = '0';
		return p;
	}

	do {
		*--x = d%10 + '0';
		d /= 10;
	} while (d);

	memcpy(p, x, 21 - (x - tmp));
	return p + 21 - (x - tmp);
}

private void
check_removed(sccs *s, delta *d, int strip_tags)
{
	assert(TAG(d));
	if (d->flags & D_GONE) return;
	if (strip_tags) {
		MK_GONE(s, d);
		return;
	}

	/*
	 * We don't need no skinkin' removed deltas.
	 */
	unless (SYMGRAPH(d) || (d->flags & D_SYMBOLS) || d->comments) {
		MK_GONE(s, d);
	}
}

/*
 * The table is all here in order, just print it.
 * New in Feb, '99: remove duplicates of metadata.
 */
int
delta_table(sccs *s, FILE *out, int willfix)
{
	delta	*d, *parent;
	int	i;	/* used by EACH */
	int	sign;
	int	first = willfix;
	int	firstadded = 1;
	char	buf[2*MAXPATH + 10];	/* ^AcP pathname|sortpath\n\0 */
	char	*p, *t, old;
	int	bits = 0;
	int	gonechkd = 0;
	int	strip_tags = CSET(s) && getenv("_BK_STRIPTAGS");
	int	version = SCCS_VERSION;
	symbol	*sym;

	if (getenv("_BK_SCCS_VERSION")) {
		version = atoi(getenv("_BK_SCCS_VERSION"));
		switch (version) {
		    case SCCS_VERSION:
			break;
		    default:
			fprintf(stderr,
			    "Bad version %d, defaulting to current\n", version);
		}
	}
	assert(!READ_ONLY(s));
	assert(s->state & S_ZFILE);

	/*
	 * Add in default xflags if the 1.0 delta doesn't have them.
	 */
	if (BITKEEPER(s)) {
		unless (s->tree->xflags) {
			s->tree->flags |= D_XFLAGS;
			s->tree->xflags = X_DEFAULT;
		}
		/* for old binaries (XXX: why bother setting above?) */
		s->tree->xflags |= X_BITKEEPER|X_CSETMARKED;
		if (CSET(s)) {
			s->tree->xflags &= ~(X_SCCS|X_RCS);
			s->tree->xflags |= X_HASH;
		}
	}

	if (BFILE_OUT(s)) return (bin_deltaTable(s, out));

	fprintf(out, "\001%cXXXXX\n", BITKEEPER(s) ? 'H' : 'h');
	s->cksum = 0;
	assert(sizeof(buf) >= 1024);	/* see comment code */

	if (BITKEEPER(s)) {
		/* add compat marker */
		unless (bk_featureCompat(s->proj)) {
			fputmeta(s, BKID_STR "\n", out);
		}
	}
	s->adddelOff = ftell(out);

	sym = s->symlist + nLines(s->symlist);
	for (d = s->table; d; d = NEXT(d)) {
		if (TAG(d)) check_removed(s, d, strip_tags);
		if (d->flags & D_GONE) {
			/* This delta has been deleted - it is not to be
			 * written out at all.
			 *
			 * All the children (d->kid) of this delta on
			 * the same branch must also have been deleted.
			 * If this branch was merged, the merge node and
			 * all its children must have been deleted too.
			 *
			 * This check is expensive, but D_GONE nodes should
			 * be extremely rare.
			 */
			unless (!gonechkd) {
				assert(!checkGone(s, D_GONE, "delta_table"));
				gonechkd = 1;
			}

			continue;
		}

		parent = d->pserial ? PARENT(s, d) : 0;

		/*
		 * XXX Whoa, nelly.  This is wrong, we must allow these if
		 * we are doing a takepatch.
		 */
		if (parent && BITKEEPER(s) &&
		    (d->date <= parent->date)) {
		    	s->state |= S_READ_ONLY;
			fprintf(stderr,
			    "%s@%s: dates do not increase\n",
			    s->sfile, REV(s, d));
			return (-1);
		}
		sprintf(buf, "\001s %d/%d/%d\n",
		    d->added, d->deleted, d->same);
		if (firstadded) {
			/*
			 * Space pad first occurance in case we need
			 * to fix it.  We do this always so sfiles are
			 * consistant.
			 */
			for(i = strlen(buf)-1; i < 43; i++) buf[i] = ' ';
			strcpy(buf+i, "\n");
			firstadded = 0;
		}
		if (first) {
			fputs(buf, out);
		} else {
			fputmeta(s, buf, out);
		}
		p = fmts(buf, "\001d ");
		*p++ = TAG(d) ? 'R' : 'D';
		*p++ = ' ';
		p = fmts(p, REV(s, d));
		*p++ = ' ';
		p += delta_strftime(p, 32, "%y/%m/%d %H:%M:%S", s, d);
		*p++ = ' ';
		p = fmts(p, USER(s, d));
		*p++ = ' ';
		p = fmtd(p, SERIAL(s, d));
		*p++ = ' ';
		p = fmtd(p, d->pserial);
		*p++ = '\n';
		*p = '\0';
		fputmeta(s, buf, out);
		if (d->cludes) {
			/* include */
			p = 0;
			t = CLUDES(s, d);
			while (i = sccs_eachNum(&t, &sign)) {
				unless (sign > 0) continue;
				unless (p) p = fmts(buf, "\001i");
				*p++ = ' ';
				p = fmtd(p, i);
				*p = 0;
				fputmeta(s, buf, out);
				p = buf;
			}
			if (p) fputmeta(s, "\n", out);
			/* exclude */
			p = 0;
			t = CLUDES(s, d);
			while (i = sccs_eachNum(&t, &sign)) {
				unless (sign < 0) continue;
				unless (p) p = fmts(buf, "\001x");
				*p++ = ' ';
				p = fmtd(p, i);
				*p = 0;
				fputmeta(s, buf, out);
				p = buf;
			}
			if (p) fputmeta(s, "\n", out);
		}
		t = COMMENTS(s, d);
		while (p = eachline(&t, &i)) {
			old = p[i];
			p[i] = 0;
			fputmeta(s, "\001c ", out);
			fputmeta(s, p, out);
			fputmeta(s, "\n", out);
			p[i] = old;
		}
		unless (BITKEEPER(s)) goto SCCS;

		if (d->bamhash) {
			p = fmts(buf, "\001cA");
			p = fmts(p, BAMHASH(s, d));
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->csetFile &&
		    !(parent && streq(CSETFILE(s, d), CSETFILE(s, parent)))) {
			p = fmts(buf, "\001cB");
			p = fmts(p, CSETFILE(s, d));
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->flags & D_CSET) {
			assert(!TAG(d));
			fputmeta(s, "\001cC\n", out);
		}
		if (DANGLING(d)) fputmeta(s, "\001cD\n", out);
		if (d->dateFudge) {
			p = fmts(buf, "\001cF");
			p = fmttt(p, d->dateFudge);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}

		if (((t = HOSTNAME(s, d)) && *t) &&
		    (!parent || !streq(t, HOSTNAME(s, parent)))) {
			sprintf(buf, "\001cH%s\n", t);
			fputmeta(s, buf, out);
		}

		if (first) {
			fputmeta(s, "\001cK", out);
			s->sumOff = ftell(out);
			fputs("XXXXX", out);
			fputmeta(s, "\n", out);
		} else if (d->flags & D_CKSUM) {
			/*
			 * It turns out not to be worth to save the
			 * few bytes you might save by not making this
			 * a fixed width field.  85% of the sums are
			 * 5 digits, 97% are 4 or 5.
			 * Leaving this fixed means we can diff the
			 * s.files easily.
			 */
			if ((d->flags & D_SORTSUM) && (d->sum != d->sortSum)) {
				sprintf(buf,
				    "\001cK%05u|%05u\n", d->sum, d->sortSum);
			} else {
				sprintf(buf, "\001cK%05u\n", d->sum);
			}
			fputmeta(s, buf, out);
		}
		if (d->merge) {
			p = fmts(buf, "\001cM");
			p = fmtd(p, d->merge);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		/* XXX: O follows P */
		if ((d->pathname &&
		    !(parent && streq(PATHNAME(s, parent), PATHNAME(s, d)))) ||
		    (d->sortPath &&
		    !(parent && streq(SORTPATH(s, parent), SORTPATH(s, d))))) {
			p = fmts(buf, "\001cP");
			p = fmts(p, PATHNAME(s, d));
			if (d->sortPath) {
				p = fmts(p, "|");
				p = fmts(p, SORTPATH(s, d));
			}
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->flags & D_MODE) {
		    	unless (parent && sameMode(parent, d)) {
				p = fmts(buf, "\001cO");
				p = fmts(p, mode2a(d->mode));
				if (d->symlink) {
					assert(S_ISLNK(d->mode));

					*p++ = ' ';
					p = fmts(p, SYMLINK(s, d));
				}
				*p++ = '\n';
				*p   = '\0';
				fputmeta(s, buf, out);
			}
		}
		if (d->random) {
			p = fmts(buf, "\001cR");
			p = fmts(p, RANDOM(s, d));
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->flags & D_SYMBOLS) {
			for (; sym != s->symlist; --sym) {
				if (sym->meta_ser < SERIAL(s, d)) break;
				unless (sym->meta_ser == SERIAL(s, d)) continue;
				if (!strip_tags || 
				    streq(KEY_FORMAT2, SYMNAME(s, sym))) {
					p = fmts(buf, "\001cS");
					p = fmts(p, SYMNAME(s, sym));
					*p++ = '\n';
					*p   = '\0';
					fputmeta(s, buf, out);
				}
			}
		}
		/* automagically strip tag serials from non-csetfiles */
		if (!strip_tags && SYMGRAPH(d) && CSET(s)) {
			p = fmts(buf, "\001cS");
			if (d->ptag) {
				p = fmtd(p, d->ptag);
				if (d->mtag) {
					*p++ = ' ';
					p = fmtd(p, d->mtag);
				}
			} else {
				p = fmtd(p, 0);
			}
			if (SYMLEAF(d)) {
				*p++ = ' ';
				*p++ = 'l';
			}
			*p++ = '\n';
			*p = 0;
			fputmeta(s, buf, out);
		}
		if (!NEXT(d)) {
			sprintf(buf, "\001cV%u\n", version);
			fputmeta(s, buf, out);
		}
		if (d->flags & D_XFLAGS) {
			sprintf(buf, "\001cX0x%x\n", d->xflags);
			fputmeta(s, buf, out);
		}
		if (d->zone &&
		    !(parent && streq(ZONE(s, parent), ZONE(s, d)))) {
			p = fmts(buf, "\001cZ");
			p = fmts(p, ZONE(s, d));
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
SCCS:
		first = 0;
		fputmeta(s, "\001e\n", out);
	}
	fputmeta(s, "\001u\n", out);
	EACH(s->usersgroups) {
		fputmeta(s, s->usersgroups[i], out);
		fputmeta(s, "\n", out);
	}
	fputmeta(s, "\001U\n", out);
	if (BITKEEPER(s) || (s->encoding_out != E_ALWAYS|E_ASCII)) {
		p = fmts(buf, "\001f e ");
		p = fmtd(p, (s->encoding_out & ~E_ALWAYS));
		*p++ = '\n';
		*p   = '\0';
		fputmeta(s, buf, out);
	}
	if (BITKEEPER(s)) {
		bits = sccs_xflags(s, sccs_top(s));
		if (bits) {
			sprintf(buf, "\001f x 0x%x\n", bits);
			fputmeta(s, buf, out);
		}
	}
	if (s->defbranch) {
		p = fmts(buf, "\001f d ");
		p = fmts(p, s->defbranch);
		*p++ = '\n';
		*p   = '\0';
		fputmeta(s, buf, out);
	}
	EACH(s->flags) {
		p = fmts(buf, "\001f ");
		p = fmts(p, s->flags[i]);
		*p++ = '\n';
		*p   = '\0';
		fputmeta(s, buf, out);
	}
	fputmeta(s, "\001t\n", out);
	EACH(s->text) {
		fputmeta(s, s->text[i], out);
		fputmeta(s, "\n", out);
	}
	fputmeta(s, "\001T\n", out);
	if (flushFILE(out)) {
		perror(s->sfile);
		s->io_warned = 1;
		return (-1);
	}
	if (BITKEEPER(s)) s->modified = 1;
	return (0);
}

/*
 * write out added/deleted/same line for new delta. Already did seek
 */
private	int
bin_deltaAdded(sccs *s, delta *d, FILE *out)
{
	return (fwrite(&d->added, sizeof(u32), 3, out) != 3);
}

/*
 * write out sum line for new delta. Already did seek
 */
private	int
bin_deltaSum(sccs *s, delta *d, FILE *out)
{
	return (fwrite(&d->sum, sizeof(u32), 1, out) != 1);
}

private	int
bin_deltaTable(sccs *s, FILE *out)
{
	u32	off_h, off_d, off_s;
	delta	*d;
	int	i;
	FILE	*perfile;

	perfile = fmem();
	sccs_perfile(s, perfile);

	off_h = 2 + 3 * 18 + ftell(perfile);
	fprintf(out, "B %08x/%08x",
	    off_h, s->heap.len);

	off_d = off_h + s->heap.len;
	/* next page */
	//off_d = ((off_d - 1) | 0xfff) + 1;
	fprintf(out, " %08x/%08x", off_d, nLines(s->slist)*(u32)sizeof(delta));

	off_s = off_d + nLines(s->slist)*sizeof(delta);
	/* next page */
	//off_s = ((off_s - 1) | 0xfff) + 1;
	fprintf(out, " %08x/%08x\n",
	    off_s, nLines(s->symlist)*(u32)sizeof(symbol));

	/* write perfile data */
	fputs(fmem_peek(perfile, 0), out);
	fclose(perfile);

	if (fseek(out, off_h, SEEK_SET)) perror("first");
	fwrite(s->heap.buf, 1, s->heap.len, out);

	if (fseek(out, off_d, SEEK_SET)) perror("second");
	EACHP(s->slist, d) {
		if (d->flags && !(d->flags & D_GONE)) {
			i = d->flags;
			d->flags &= 0x000FFFFF; /* only write some flags */
			fwrite(d, sizeof(delta), 1, out);
			d->flags = i;
		} else {
			fseek(out, sizeof(delta), SEEK_CUR);
		}
	}

	fseek(out, off_s, SEEK_SET);
	fwrite(s->symlist + 1, sizeof(symbol), nLines(s->symlist), out);

	s->adddelOff = off_d +
	    ((u8*)&s->table->added - (u8*)s->tree);
	s->sumOff = off_d +
	    ((u8*)&s->table->sum - (u8*)s->tree);
	if (BITKEEPER(s)) s->modified = 1;
	return (0);
}

void
cset_savetip(sccs *s)
{
	FILE	*f;
	delta	*d;
	char	*tmp, *tip;
	char	md5[MD5LEN];
	char	key[MAXKEY];

	assert(s->proj);
	tip = aprintf("%s/BitKeeper/log/TIP", proj_root(s->proj));
	tmp = aprintf("%s.new.%u", tip, getpid());
	f = fopen(tmp, "w");
	assert(f);
	sccs_md5delta(s, d = sccs_top(s), md5);
	sccs_sdelta(s, d, key);
	fprintf(f, "%s\n%s\n%s\n", md5, key, REV(s, d));
	fclose(f);
	if (rename(tmp, tip)) perror(tmp);
	free(tmp);
	free(tip);
}

/*
 * If we are trying to compare with expanded strings, do so.
 */
private int
expandeq(sccs *s, delta *d, char *gbuf, int glen, char *fbuf, int *flags)
{
	char	*e = fbuf, *e1 = 0, *e2 = 0;
	int sccs_expanded = 0 , rcs_expanded = 0, rc;

	if (BINARY(s)) return (0);
	unless (*flags & GET_EXPAND) return (0);
	if (SCCS(s)) {
		e = e1 = expand(s, d, e, &sccs_expanded);
		if (EXPAND1(s) && sccs_expanded) *flags &= ~GET_EXPAND;
	}
	if (RCS(s)) {
		e = e2 = rcsexpand(s, d, e, &rcs_expanded);
		if (EXPAND1(s) && rcs_expanded) *flags &= ~GET_EXPAND;
	}
	rc = (glen == strlen(e)) && strneq(gbuf, e, glen);
	if (sccs_expanded) free(e1);
	if (rcs_expanded) free(e2);
	return (rc);
}

/*
 * This is an expensive call but not as expensive as running diff.
 * flags is same as get flags.
 */
private int
_hasDiffs(sccs *s, delta *d, u32 flags, int inex, pfile *pf)
{
	FILE	*gfile = 0;
	char	*gline;
	size_t	flen, glen;
	MDBM	*ghash = 0;
	MDBM	*shash = 0;
	ser_t	*state = 0;
	u8	*slist = 0;
	int	print = 0, different;
	char	sbuf[MAXLINE];
	char	*name = 0, *mode = "rb";
	int	tmpfile = 0;
	char	*fbuf;
	int	no_lf = 0;
	int	in_weave = 0;
	int	lf_pend = 0;
	u32	eflags = flags; /* copy because expandeq destroys bits */
	int	error = 0, serial;

#define	RET(x)	{ different = x; goto out; }

	unless (HAS_GFILE(s)) RET(0);
	unless (s->mode & 0444) {
		errno = EACCES;
		perror(s->gfile);
		RET(-1);
	}

	if (inex && (pf->mRev || pf->iLst || pf->xLst)) RET(2);

	if (S_ISLNK(s->mode)) {
		unless (S_ISLNK(d->mode) && d->symlink) RET(1);
		RET(!streq(s->symlink, SYMLINK(s, d)));
	}
	/* If the path changed, it is a diff */
	if (d->pathname) {
		char	*r;
		project	*proj;

		if (CSET(s) && proj_isComponent(s->proj)) {
			proj = proj_product(s->proj);
		} else {
			proj = s->proj;
		}
		if ((r = _relativeName(s->gfile, 0, 1, 1, proj))
		    && !streq(PATHNAME(s, d), r)) {
		    	RET(1);
		}
	}

	/*
	 * Cannot enforce this assert here, gfile may be ready only
	 * due to  GET_SKIPGET
	 * assert(WRITABLE(s));
	 */
	if (UUENCODE(s)) {
		tmpfile = 1;
		unless (bktmp(sbuf, "getU")) RET(-1);
		name = strdup(sbuf);
		if (uuexpand_gfile(s, name)) {
			verbose((stderr, "can't open %s\n", s->gfile));
			RET(-1);
		}
	} else {
		mode = "rt";
		name = strdup(s->gfile);
	}
	if (BAM(s)) {
		different = bp_diff(s, d, name);
		if ((different == 2) && !bp_fetch(s, d)) {
			different = bp_diff(s, d, name);
		}
		goto out;
	} else if (HASH(s)) {
		int	flags = CSET(s) ? DB_KEYFORMAT : 0;

		ghash = loadDB(name, 0, flags);
		shash = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	}
	else unless (gfile = fopen(name, mode)) {
		verbose((stderr, "can't open %s\n", name));
		RET(-1);
	}
	assert(s->state & S_SOPEN);
	slist = serialmap(s, d, pf->iLst, pf->xLst, &error);
	assert(!error);
	sccs_rdweaveInit(s);
	in_weave = 1;
	while (fbuf = sccs_nextdata(s)) {
		if (isData(fbuf)) {
			if (fbuf[0] == CNTLA_ESCAPE) fbuf++;
			if (!print) {
				/* if we are skipping data from pending block */
				if (lf_pend &&
				    lf_pend == whatstate(state))
				{
					lf_pend = 0;
				}
				continue;
			}
			if (HASH(s)) {
				char	*from, *to, *val;
				/* XXX: hack to use sbuf, but it exists */
				val = CSET(s) ?
				    separator(fbuf) : strchr(fbuf, ' ');
				assert(val && (*val == ' '));
				for (from = fbuf, to = sbuf;
				    from != val;
				    *to++ = *from++) /* null body */;
				*to++ = 0;
				from++;
				val = to;
				for ( ; *from; *to++ = *from++) /* null body */;
				assert(*from == 0);
				*to = 0;
				/* XXX: could use the MDBM_INSERT fails */
				if (mdbm_fetch_str(shash, sbuf)) continue;
				if (mdbm_store_str(shash, sbuf, "1",
				    MDBM_INSERT))
				{
					fprintf(stderr, "sccs_hasDiffs: "
						"key insert caused conflict "
						"'%s'\n", sbuf);
					RET(-1);
				}
				unless (to = mdbm_fetch_str(ghash, sbuf)) {
#ifdef	WHEN_DELETE_KEY_SUPPORTED
					debug((stderr, "diff because sfile "
						"had key\n"));
					RET(1);
#else
					continue;
#endif
				}
				unless (streq(to, val)) {
					debug((stderr, "diff because same "
						"key diff value\n"));
					RET(1);
				}
				mdbm_delete_str(ghash, sbuf);
				continue;
			}
			no_lf = 0;
			lf_pend = print;
			unless (gline = fgetln(gfile, &glen)) {
				debug((stderr, "diff because EOF on gfile\n"));
				RET(1);
			}
			/* strip newline and remember if it was missing */
			if (gline[glen-1] == '\n') {
				glen--;
			} else {
				no_lf = 1;
			}
			/* now strip CR; if gline was "\n", glen now 0 */
			if (glen && (gline[glen-1] == '\r')) glen--;

			/* now strip CR in weave if old broken sfile */
			flen = strlen(fbuf);
			if (flen && (fbuf[flen-1] == '\r')) flen--;

			unless (((flen == glen) &&
				strneq(gline, fbuf, flen)) ||
			    expandeq(s, d, gline, glen, fbuf, &eflags)) {
				debug((stderr, "diff because diff data\n"));
				RET(1);
			}
			debug2((stderr, "SAME %s", fbuf));
			continue;
		}
		serial = atoi(&fbuf[3]);
		if (fbuf[1] == 'E' && lf_pend == serial &&
		    whatstate(state) == serial) {
			char	*n = &fbuf[3];
			while (isdigit(*n)) n++;
			if (*n == 'N') {
				no_lf = (no_lf) ? 0 : 1; /* toggle */
			}
			else if (no_lf) {
				/* sfile has newline when gfile had none */
				debug((stderr, "diff because EOF on gfile\n"));
				RET(1);
			}
			lf_pend = 0;
		}
		state = changestate(state, fbuf[1], serial);
		debug2((stderr, "%s\n", fbuf));
		print = printstate(state, slist);
	}
	if (HASH(s)) {
		kvpair	kv;
		kv = mdbm_first(ghash);
		if (kv.key.dsize) {
			debug((stderr, "diff because gfile had key\n"));
			RET(1);
		}
		debug((stderr, "hashes same\n"));
		RET(0);
	}
	if (no_lf) {
		/* gfile has newline when sfile had none */
		debug((stderr, "diff because EOF on sfile\n"));
		RET(1);
	}
	if (gline = fgetln(gfile, &glen)) {
		debug((stderr, "diff because EOF on sfile\n"));
		RET(1);
	} else {
		debug((stderr, "same\n"));
		RET(0);
	}
out:
	if (in_weave) sccs_rdweaveDone(s);
	if (gfile) fclose(gfile); /* must close before we unlink */
	if (ghash) mdbm_close(ghash);
	if (shash) mdbm_close(shash);
	if (name) {
		if (tmpfile) unlink(name);
		free(name);
	}
	if (slist) free(slist);
	if (state) free(state);
	return (different);
}

int
sccs_hasDiffs(sccs *s, u32 flags, int inex)
{
	pfile	pf;
	int	ret;
	delta	*d;

	unless (HAS_GFILE(s) && HAS_PFILE(s)) return (0);

	bzero(&pf, sizeof(pf));
	if (sccs_read_pfile("hasDiffs", s, &pf)) return (-1);
	unless (d = findrev(s, pf.oldrev)) {
		verbose((stderr,
		    "diffs: can't find %s in %s\n", pf.oldrev, s->gfile));
		free_pfile(&pf);
		return (-1);
	}
	ret = _hasDiffs(s, d, flags, inex, &pf);
	if ((ret == 1) && MONOTONIC(s) && DANGLING(d) && !DANGLING(s->tree)) {
		while (NEXT(d) && (DANGLING(d) || TAG(d))) d = NEXT(d);
		assert(NEXT(d));
		free(pf.oldrev);
		pf.oldrev = strdup(REV(s, d));
		ret = _hasDiffs(s, d, flags, inex, &pf);
	}
	free_pfile(&pf);
	return (ret);
}

/*
 * Apply the encoding to the gfile and leave it in tmpfile.
 */
private int
uuexpand_gfile(sccs *s, char *tmpfile)
{
	FILE	*in, *out;

	unless (out = fopen(tmpfile, "w")) return (-1);
	unless (in = fopen(s->gfile, "r")) {
		fclose(out);
		return (-1);
	}
	uuencode(in, out);
	fclose(in);
	fclose(out);
	return (0);
}


private int
isRegularFile(mode_t m)
{
	return ((m == 0) || S_ISREG(m));
}

/*
 * Check mode/symlink changes
 * Changes in permission are ignored
 * Returns:
 *	0 if no change
 *	1 if file type changed
 *	2 if meta mode field changed (e.g. symlink)
 * 	3 if path changed
 *
 */
private int
diff_gmode(sccs *s, pfile *pf)
{
	delta *d = findrev(s, pf->oldrev);

	/* If the path changed, it is a diff */
	if (d->pathname) {
		char *q, *r = _relativeName(s->sfile, 0, 1, 1, s->proj);

		if (r) {
			q = sccs2name(r);
			if (!streq(PATHNAME(s, d), q)) {
				free(q);
				return (3);
			}
			free(q);
		}
	}

	unless (sameFileType(s, d)) return (1);
	if (S_ISLNK(s->mode)) {
		unless (streq(s->symlink, SYMLINK(s, d))) {
			return (2);
		}
	}
	return (0); /* no  change */
}

/*
 * Load both files into MDBMs and then leave the difference in the tmpfile.
 * case 0:	no diffs
 * case 1:	diffs
 * case 2:	diff ran into problems
 */
int
diffMDBM(sccs *s, char *old, char *new, char *tmpfile)
{
	FILE	*f = fopen(tmpfile, "w");
	int	flags = CSET(s) ? DB_KEYFORMAT : 0;
	MDBM	*o = loadDB(old, 0, flags);
	MDBM	*n = loadDB(new, 0, flags);
	/* 'p' is not used as hash, but has simple storage */
	MDBM	*p = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	kvpair	kv;
	int	items = 0;
	int	ret;
	char	*val;

	unless (n && o && p && f) {
		ret = 2;
		unlink(tmpfile);
		goto out;
	}
	for (kv = mdbm_first(n); kv.key.dsize; kv = mdbm_next(n)) {
		if ((val = mdbm_fetch_str(o, kv.key.dptr)) &&
		    streq(val, kv.val.dptr)) {
		    	continue;
		}
		if (mdbm_store(p, kv.key, kv.val, MDBM_INSERT)) {
			ret = 2;
			fprintf(stderr, "diffMDBM: mdbm store failed\n");
			goto out;
		}
		items++;
	}
	if (items) {
		/* XXX NOT YET: fprintf(f, "I0 %u\n", items); */
		fprintf(f, "0a0\n");
		for (kv = mdbm_first(p); kv.key.dsize; kv = mdbm_next(p)) {
			fputs("> ", f);
			fputs(kv.key.dptr, f);
			fputc(' ', f);
			fputs(kv.val.dptr, f);
			fputc('\n', f);
		}
		ret = 1;
		if (s->mdbm) mdbm_close(s->mdbm);
		s->mdbm = o;
		o = 0;
	} else {
		ret = 0;
	}
out:	if (n) mdbm_close(n);
	if (o) mdbm_close(o);
	if (p) mdbm_close(p);
	if (f) fclose(f);
	return (ret);
}

/*
 * Returns:
 *	-1 for some already bitched about error
 *	0 if there were differences
 *	1 if no differences
 *
 * 	This function is called to determine the difference
 *	in the delta body. Note that, by definition, the delta body
 *	of non-regular file is empty.
 */
int
diff_gfile(sccs *s, pfile *pf, int expandKeyWord, char *tmpfile)
{
	char	old[MAXPATH];	/* the version from the s.file */
	char	new[MAXPATH];	/* the new file, usually s->gfile */
	int	ret, flags;
	delta *d;

	debug((stderr, "diff_gfile(%s, %s)\n", pf->oldrev, s->gfile));
	assert(s->state & S_GFILE);
	/*
	 * set up the "new" file
	 */
	if (isRegularFile(s->mode)) {
		if (UUENCODE(s)) {
			unless (bktmp(new, "getU")) return (-1);
			if (uuexpand_gfile(s, new)) {
				unlink(new);
				return (-1);
			}
		} else {
			strcpy(new, s->gfile);
		}
	} else { /* non regular file, e.g symlink */
		strcpy(new, DEVNULL_WR);
	}

	/*
	 * set up the "old" file
	 */
	d = findrev(s, pf->oldrev);
	unless (d) {
		fprintf(stderr,
		    "%s: Cannot find base revision \"%s\", bad p.file?\n",
		    s->sfile, pf->oldrev);
		return (-1);
	}
	flags =  GET_ASCII|SILENT|PRINT;
	if (expandKeyWord) flags |= GET_EXPAND;
	if (isRegularFile(d->mode) && !BAM(s)) {
		unless (bktmp(old, "get")) return (-1);
		if (sccs_get(s, pf->oldrev, pf->mRev, pf->iLst, pf->xLst,
		    flags, old)) {
			unlink(old);
			return (-1);
		}
	} else {
		strcpy(old, DEVNULL_WR);
	}

	/*
	 * now we do the diff
	 */
	if (HASH(s)) {
		ret = diffMDBM(s, old, new, tmpfile);
	} else if (BAM(s)) {
		ret = bp_diff(s, d, new);
		if ((ret == 2) && !bp_fetch(s, d)) {
			ret = bp_diff(s, d, new);
		}
	} else {
		ret = diff(old, new, DF_DIFF, tmpfile);
	}
	unless (streq(old, DEVNULL_WR)) unlink(old);
	if (!streq(new, s->gfile) && !streq(new, DEVNULL_WR)){
		unlink(new);		/* careful */
	}
	switch (ret) {
	    case 0:	/* no diffs */
		return (1);
	    case 1:	/* diffs */
		return (0);
	    case 2:	/* diff ran into problems */
		return (-1);
	    default:	/* unknown? */
		fprintf(stderr, "Unknown exit from diff.\n");
		return (-1);
	}
}

/*
 * Check mode changes & changes in delta body.
 * The real work is done in diff_gmode & diff_gfile.
 * Returns:
 *	-1 for some already bitched about error
 *	0 if there were differences
 *	1 if no differences
 *
 */
private int
diff_g(sccs *s, pfile *pf, char **tmpfile)
{
	static char	tmpname[MAXPATH];

	*tmpfile = DEVNULL_WR;
	switch (diff_gmode(s, pf)) {
	    case 0: 		/* no mode change */
		if (!isRegularFile(s->mode)) return 1;
		*tmpfile  = bktmp(tmpname, "diffg1");
		assert(*tmpfile);
		return (diff_gfile(s, pf, 0, *tmpfile));
	    case 2:		/* meta mode field changed */
		return 0;
	    case 3:		/* path changed */
	    case 1:		/* file type changed */
		*tmpfile  = bktmp(tmpname, "diffg2");
		assert(*tmpfile);
		if (diff_gfile(s, pf, 0, *tmpfile) == -1) return (-1);
		return 0;
	    default:
		return -1;
	}
}

private int
unlinkGfile(sccs *s)
{
	if (s->state & S_GFILE) {
		if (unlink(s->gfile)) return (-1);	/* Careful */
	}
	/*
	 * zero out all gfile related field
	 */
	if (s->symlink) free(s->symlink);
	s->symlink = 0;
	s->gtime = s->mode = 0;
	s->state &= ~S_GFILE;
	return (0);
}

private void
pdiffs(char *gfile, char *left, char *right, FILE *diffs)
{
	int	first = 1;
	char	buf[MAXLINE];

	while (fnext(buf, diffs)) {
		if (first) {
			printf("===== %s %s vs %s =====\n", gfile, left, right);
			first = 0;
		}
		fputs(buf, stdout);
	}
	fclose(diffs);
}

void
free_pfile(pfile *pf)
{
	if (pf->oldrev) free(pf->oldrev);
	if (pf->newrev) free(pf->newrev);
	if (pf->iLst) free(pf->iLst);
	if (pf->xLst) free(pf->xLst);
	if (pf->mRev) free(pf->mRev);
	bzero(pf, sizeof(*pf));
}

/*
 * clean / unedit the specified file.
 *
 * Return codes are passed out to exit() so don't error on warnings.
 * If listing, don't do any cleans.
 */
int
sccs_clean(sccs *s, u32 flags)
{
	pfile	pf;
	char	tmpfile[MAXPATH];
	delta	*d;
	int	ret;

	/* don't go removing gfiles without s.files */
	unless (HAS_SFILE(s) && HASGRAPH(s)) {
		verbose((stderr, "%s not under SCCS control\n", s->gfile));
		return (0);
	}

	unless (HAS_PFILE(s)) {
		pfile	dummy = { "+", "?", 0, 0, 0 };

		if (isRegularFile(s->mode) && !WRITABLE(s)) {
			verbose((stderr, "Clean %s\n", s->gfile));
			unless (flags & CLEAN_CHECKONLY) unlinkGfile(s);
			return (0);
		}

		/*
		 * It's likely that they did a chmod +w on the file.
		 * Go look and see if there are any diffs and if not,
		 * clean it. (The GET_EXPAND ignores keywords)
		 */
		ret = _hasDiffs(s, sccs_top(s), GET_EXPAND, 0, &dummy);
		unless (ret) {
			verbose((stderr, "Clean %s\n", s->gfile));
			unless (flags & CLEAN_CHECKONLY) unlinkGfile(s);
			return (0);
		}
		if (ret < 0) return (1);
		fprintf(stderr,
		    "%s writable, with changes, but not edited.\n", s->gfile);
		unless (flags & PRINT) return (1);
		sccs_diffs(s, 0, 0, DIFF_HEADER|SILENT, DF_DIFF, stdout);
		return (1);
	}

	if (sccs_read_pfile("clean", s, &pf)) return (1);
	if (pf.mRev || pf.iLst || pf.xLst) {
		unless (flags & CLEAN_SHUTUP) {
			fprintf(stderr,
			    "%s has merge|include|exclude, not cleaned.\n",
			    s->gfile);
		}
		free_pfile(&pf);
		return (1);
	}

	unless (HAS_GFILE(s)) {
		free_pfile(&pf);
		unlink(s->pfile);
		s->state &= ~S_PFILE;
		verbose((stderr, "cleaning plock for %s\n", s->gfile));
		return (0);
	}

	unless (d = findrev(s, pf.oldrev)) {
		free_pfile(&pf);
		return (1);
	}

	unless (sameFileType(s, d)) {
		unless (flags & PRINT) {
			fprintf(stderr,
			    "%s has different file types "
			    "which is unsupported.\n", s->gfile);
                } else {
			printf("===== %s (file type) %s vs edited =====\n",
			    s->gfile, pf.oldrev);
			printf("< %s\n-\n", mode2FileType(d->mode));
			printf("> %s\n", mode2FileType(s->mode));
 		}
		free_pfile(&pf);
		return (2);
	}

	if (BITKEEPER(s)) {
		char	*t;
		project	*proj;

		if (CSET(s) && proj_isComponent(s->proj)) {
			proj = proj_product(s->proj);
		} else {
			proj = s->proj;
		}
		unless (t = relativeName(s, 1, proj)) {
			fprintf(stderr,
			"%s: cannot compute relative path, no project root ?\n",
				s->gfile);
			free_pfile(&pf);
			return (1);
		}
		if (!(flags & CLEAN_SKIPPATH) && (!streq(t, PATHNAME(s, d)))) {
			unless (flags & PRINT) {
				verbose((stderr,
				   "%s has different pathnames: %s, needs delta.\n",
				    s->gfile, t));
			} else {
				printf(
				    "===== %s (pathnames) %s vs edited =====\n",
				    s->gfile, pf.oldrev);
				printf("< %s\n-\n", PATHNAME(s, d));
				printf("> %s\n", t);
			}
			free_pfile(&pf);
			return (2);
		}
	} 

	if (S_ISLNK(s->mode)) {
		if (streq(s->symlink, SYMLINK(s, d))) {
			verbose((stderr, "Clean %s\n", s->gfile));
			unless (flags & CLEAN_CHECKONLY) {
				unlink(s->pfile);
				s->state &= ~S_PFILE;
				unlinkGfile(s);
			}
			free_pfile(&pf);
			return (0);
		}
		unless (flags & CLEAN_SHUTUP) {
			unless (flags & PRINT) {
				fprintf(stderr,
			    "%s has been modified, needs delta.\n", s->gfile);
			} else {
				printf("===== %s %s vs %s =====\n",
				    s->gfile, pf.oldrev, "edited");
				printf("< SYMLINK -> %s\n-\n", SYMLINK(s, d));
				printf("> SYMLINK -> %s\n", s->symlink);
			}
		}
		free_pfile(&pf);
		return (2);
	}

	unless (bktmp(tmpfile, "diffg")) return (1);
	/*
	 * hasDiffs() is faster.
	 */
	unless (sccs_hasDiffs(s, 0, 1)) goto nodiffs;
	switch (diff_gfile(s, &pf, 0, tmpfile)) {
	    case 1:		/* no diffs */
nodiffs:	verbose((stderr, "Clean %s\n", s->gfile));
		unless (flags & CLEAN_CHECKONLY) {
			unlink(s->pfile);
			s->state &= ~S_PFILE;
			unlinkGfile(s);
		}
		free_pfile(&pf);
		unlink(tmpfile);
		return (0);
	    case 0:		/* diffs */
		unless (flags & CLEAN_SHUTUP) {
			unless (flags & PRINT) {
				fprintf(stderr,
			    "%s has been modified, needs delta.\n", s->gfile);
			} else {
				pdiffs(s->gfile,
				    pf.oldrev, "edited", fopen(tmpfile, "r"));
		}
		}
		free_pfile(&pf);
		unlink(tmpfile);
		return (2);
	    default:		/* error */
		fprintf(stderr,
		    "couldn't compute diffs on %s, skipping", s->gfile);
		free_pfile(&pf);
		unlink(tmpfile);
		return (1);
	}
}

/*
 * unedit the specified file.
 *
 * Return codes are passed out to exit() so don't error on warnings.
 */
int
sccs_unedit(sccs *s, u32 flags)
{
	int	modified = 0;
	int	getFlags = 0;
	int	currState = 0;

	/* don't go removing gfiles without s.files */
	unless (HAS_SFILE(s) && HASGRAPH(s)) {
		verbose((stderr, "%s not under SCCS control\n", s->gfile));
		return (0);
	}

	// XXX - pretty similar to do_checkout 
	switch (CO(s)) {
	    case CO_GET: getFlags = GET_EXPAND; break;
	    case CO_EDIT: getFlags = GET_EDIT; break;
	    case CO_LAST:
	    	if (HAS_PFILE(s)) {
			getFlags = GET_EDIT;
		} else if (HAS_GFILE(s)) {
			getFlags = GET_EXPAND;
		}
		break;
	}
	if (HAS_PFILE(s)) {
		if (HAS_GFILE(s)) {
			if (!getFlags || sccs_hasDiffs(s, flags, 1)) {
				modified = 1;
			}
			currState = GET_EDIT;
		} else {
			verbose((stderr, "Removing plock for %s\n", s->gfile));
		}
	} else {
		if (WRITABLE(s)) {
			fprintf(stderr, 
			    "%s writable but not edited?\n", s->gfile);
			return (1);
		} else {
			verbose((stderr, "Clean %s\n", s->gfile));
			if (HAS_GFILE(s)) currState = GET_EXPAND;
		}
	}
	if (!modified && getFlags &&
	    (getFlags == currState ||
		(currState != 0 && !(SCCS(s) || RCS(s))))) {
		if ((getFlags & GET_EDIT) && !WRITABLE(s)) {
			/*
			 * With GET_SKIPGET, sccs_get will unlink
			 * readonly files (look for unlink in sccs_get()),
			 * so fix perms here if EDIT and readonly
			 */
			if (fix_gmode(s, getFlags)) goto reget;
		}
		getFlags |= GET_SKIPGET;
	} else {
reget:		if (unlinkGfile(s)) return (1);
		/*
		 * For make, foo.o may be more recent than s.foo.c
		 *
		 * XXX causes problems when running BK_DEVELOPER and having
		 * hardlinks as it messes with other repos time stamps,
		 * possibly making the other repos sfile with a time
		 * newer than its gfile.  See use of turning off developer
		 * in t.bam-convert as a case when unedit in one repo
		 * causes push to fail in a different repo.
		 */
		utime(s->sfile, 0);
	}
	unlink(s->pfile);
	s->state &= ~S_PFILE;
	if (getFlags) {
		if (sccs_get(s, 0, 0, 0, 0, SILENT|getFlags, "-")) {
			return (1);
		}
		s = sccs_restart(s);
		if (fix_gmode(s, getFlags)) {
			if (getFlags & GET_SKIPGET) {
				getFlags &= ~GET_SKIPGET;
				goto reget;
			}
			perror(s->gfile);
		}
	}
	return (0);
}


private void
_print_pfile(sccs *s)
{
	FILE	*f;
	char	buf[MAXPATH];

	sprintf(buf, "%s:", s->gfile);
	printf("%-23s ", buf);
	f = fopen(s->pfile, "r");
	if (fgets(buf, sizeof(buf), f)) {
		char	*s;
		for (s = buf; *s && *s != '\n'; ++s);
		*s = 0;
		fputs(buf, stdout);
	} else {
		printf("(can't read pfile)\n");
	}
	fclose(f);
}

private void
sccs_infoMsg(sccs *s, char msgCode, u32 flags)
{
	char *gfile = s->gfile;

	if (flags & SINFO_TERSE) {
		printf("%s|%c\n", gfile, msgCode);
		return;
	}
	switch (msgCode) {
	    case 'x':	verbose((stderr, "%s not under SCCS control\n", gfile));
			break;
	    case 'u':	break; /* no long message */
	    case 'p':	/* locked, but has no g file */
			verbose((stderr, "%s not checked out\n", gfile));
			break;
	    case 'm':	_print_pfile(s);
			printf(" (has merge pointer, needs delta)\n");
			break;
	    case 'c':	_print_pfile(s);
			printf(" (modified, needs delta)\n");
			break;
	    case '?':	_print_pfile(s);
			fprintf(stderr,
		"couldn't compute diffs on %s, skipping\n", gfile);
			break;
	    case 'l':	_print_pfile(s);
			printf("\n");
			break;
	}
}

/*
 * provide information about the editing status of a file.
 * XXX - does way too much work for this, shouldn't sccs init.
 *
 * Return codes are passed out to exit() so don't error on warnings.
 */
int
sccs_info(sccs *s, u32 flags)
{
	unless (HAS_SFILE(s)) {
		sccs_infoMsg(s, 'x', flags);
		return (0);
	}
	GOODSCCS(s);
	if (!HAS_PFILE(s)) {
		sccs_infoMsg(s, 'u', flags);
		return (0);
	}
	unless (HAS_GFILE(s)) {
		sccs_infoMsg(s, 'p', flags);
		return (0);
	}

	switch (sccs_hasDiffs(s, flags, 1)) {
	    case 2:
		sccs_infoMsg(s, 'm', flags);
		return (1);
	    case 1:
		sccs_infoMsg(s, 'c', flags);
		return (1);
	    case -1:
		sccs_infoMsg(s, '?', flags);
		return (1);
	    default:
		sccs_infoMsg(s, 'l', flags);
	}
	return (0);
}

/*
 * Work backwards and count all the lines for this delta.
 *
 * XXX: This is wrong and if it comes up a number that is close, that
 *      is because of probability and luck.  This ignores lines added
 *      and deleted on any branches.  In other words, if I add 10 lines
 *      on the trunk and 10 lines somewhere else on the branch, then merge,
 *      the merge will have +0 and -0 and this alg only considers trunk,
 *      so it will show 10 fewer lines than real.  Now it is also wrong to
 *      then include the branch in the walk, because of deletes -- if there
 *      is a delete on a branch and the trunk, then no way to know if they
 *      were the same line or different lines.  The only way around it
 *      is to walk the weave and see.
 *      This is only used to output after a delta so that it is wrong
 *      doesn't really matter.
 */
private int
count_lines(sccs *s, delta *d)
{
	int	count = 0;

	while (d) {
		count += d->added - d->deleted;
		d = PARENT(s, d);
	}
	return (count);
}

/*
 * Look for files containing binary data that BitKeeper cannot handle,
 * when in text mode.
 * Right now only NUL is unsupported
 * "\n\001" used to cause problem, this has been fixed.
 *
 * XXX Performance warning: This is slow if we get a very large text file
 */
int
ascii(char *file)
{
	MMAP	*m = mopen(file, "b");
	u8	*p, *end;

	if (!m) return (2);
	for (p = (u8*)m->where, end = (u8*)m->end; p < end; ) {
		unless (*p++) { /* null is not allowed */
			mclose(m);
			return (0);
		}
	}
	mclose(m);
	return (1);
}

/*
 * Open the input for checkin.
 * The set of options we have are:
 *	{empty, stdin, file} | {cat, gzip|uuencode|bam}
 */
private int
openInput(sccs *s, int flags, FILE **inp)
{
	char	*file = (flags&DELTA_EMPTY) ? DEVNULL_WR : s->gfile;
	char	*mode = "rb";	/* default mode is binary mode */

	unless (flags & DELTA_EMPTY) {
		unless (HAS_GFILE(s)) {
			*inp = NULL;
			return (-1);
		}
	}
	/* handle auto promoting ascii to binary if needed */
	if (ASCII(s) && !streq("-", file) && !ascii(file)) {
		s->encoding_in = s->encoding_out =
		    sccs_encoding(s, size(file), "binary");
	}
	switch (s->encoding_in & E_DATAENC) {
	    case E_ASCII:
		mode = "rt";
		/* fall through, check if we are really ascii */
	    case E_UUENCODE:
		if (streq("-", file)) {
			*inp = stdin;
		} else {
			*inp = fopen(file, mode);
		}
		break;
	    case E_BAM:
		*inp = 0;
		break;
	    default:
		assert(0);
		break;
	}
	return (0);
}

/*
 * Do most of the initialization on a delta.
 */
delta *
sccs_dInit(delta *d, char type, sccs *s, int nodefault)
{
	char	*t;

	unless (d) d = new(delta);
	if (type == 'R') d->flags |= (D_META|D_TAG);
	assert(s);
	if (BITKEEPER(s) && !TAG(d)) d->flags |= D_CKSUM;
	unless (d->date || nodefault) {
		if (t = getenv("BK_DATE_TIME_ZONE")) {
			dateArg(s, d, t, 1);
			assert(!(d->flags & D_ERROR));
		} else if (s->gtime && (s->initFlags & INIT_FIXDTIME)) {
			time_t	now = time(0), use = s->gtime, i;

			if (use > now) {
				fprintf(stderr, "BK limiting the delta time "
				    "to the present time.\n"
				    "Timestamp on \"%s\" is in the future\n",
				    s->gfile);
				use = now;
			}
			date(s, d, use);

			/*
			 * If gtime is from the past, fudge the date
			 * to current, so the unique() code don't cut us off
			 * too early. This is important for getting unique
			 * root key.
			 */
			if ((i = (now - use)) > 0) {
				d->dateFudge = i;
				d->date += d->dateFudge;
			}
		} else {
			date(s, d, time(0));
		}
	}
	if (nodefault) {
		unless ((t = USER(s, d)) && *t) userArg(s, d, "Anon");
	} else {
		project	*proj = s->proj;
		char	*imp, *user, *host;
		int	changed = 0;

		if (CSET(s) && proj_isComponent(proj)) {
			proj = proj_product(proj);
		}
		unless ((user = USER(s, d)) && *user) {
			user = sccs_user();
			changed = 1;
		}
		unless ((host = HOSTNAME(s, d)) && *host) {
			if (imp = getenv("BK_IMPORTER")) {
				user = aprintf("%s@%s[%s]",
				    user, sccs_gethost(), imp);
			} else {
				user = aprintf("%s@%s",
				    user, sccs_host());
			}
			changed = 1;
		} else if (changed) {
			user = aprintf("%s@%s", user, host);
		}
		if (changed) {
			userArg(s, d, user);
			free(user);
		}
		unless (d->pathname) {
			char *p, *q;

			/*
			 * Get the relativename of the sfile,
			 * _not_ the gfile,
			 * because we cannot trust the gfile name on
			 * win32 case-folding file system.
			 */
			if (CSET(s)) {
				p = _relativeName(s->sfile, 1, 1, 1, proj);
				/* strip out RESYNC */
				str_subst(p, "/RESYNC/", "/", p);
			} else {
				p = _relativeName(s->sfile, 0, 0, 1, proj);
			}
			q = sccs2name(p);
			pathArg(s, d, q);
			free(q);
		}
		unless (d->csetFile) {
			csetFileArg(s, d, proj_rootkey(proj));
		}
#ifdef	AUTO_MODE
		assert("no" == 0);
		unless (d->flags & D_MODE) {
			if (s->state & GFILE) {
				d->mode = s->mode;
				d->symlink = s->symlink;
				s->symlink = 0;
				d->flags |= D_MODE;
				assert(!(d->flags & D_DUPLINK));
			} else {
				modeArg(s, d, "0664");
			}
		}
#endif
	}
	return (d);
}

/*
 * This poorly named function is trying to decide if the files are the
 * same type, and if they are symlinks, are they the same value.
 */
private int
needsMode(sccs *s, delta *p)
{
	unless (p) return (1);
	unless (sameFileType(s, p)) return (1);
	unless (s->symlink) return (0);
	return (!streq(s->symlink, SYMLINK(s, p)));
}

/*
 * Update the mode field and the symlink field.
 */
private void
updMode(sccs *s, delta *d, delta *dParent)
{
	if ((s->state & S_GFILE) &&
	    !(d->flags & D_MODE) && needsMode(s, dParent)) {
		assert(d->mode == 0);
		d->mode = s->mode;
		if (s->symlink) {
			d->symlink = sccs_addUniqStr(s, s->symlink);
		}
		d->flags |= D_MODE;
	}
}

void
updatePending(sccs *s)
{
	if (CSET(s) && !proj_isComponent(s->proj)) return;
	touch(sccsXfile(s, 'd'),  GROUP_MODE);
}

/*
 * Escape the control-A character in data buffer
 */
private void
fix_cntl_a(sccs *s, char *buf, FILE *out)
{
	char	cntlA_escape[2] = { CNTLA_ESCAPE, 0 };

	if (buf[0] == '\001') fputdata(s, cntlA_escape, out);
}

/*
 * Read the output of diff and determine if the files 
 * are binary.
 */
private int
binaryCheck(MMAP *m)
{
	u8	*p, *end;

	p = m->mmap;
	end = m->end;
	
	/* GNU diff reports binary files */
	if (p + 13 < end && strneq("Binary files ", p, 13)) return (1);
	
	/* no nulls */
	while (p < end) if (*p++ == 0) return (1);
	return (0);
}

private int
toobig(sccs *s)
{
	u64	sz;

	/*
	 * largest signed 32 bit value that is positive.
	 */
	sz = 0x7fffffff;
	if (BAM(s) && exists(s->gfile) && (size(s->gfile) > sz)) {
		fprintf(stderr,
		    "%s is too large for this version of BitKeeper\n",
		    s->gfile);
		return (1);
	}
	return (0);
}

/*
 * Given a relative gfile pathname from the repository root
 * determine if the filename should be legal.
 */
int
bk_badFilename(char *name)
{
	char	*base = basenm(name);

	if (getenv("_BK_BADNAME")) return (0);
	/*
	 * Disallow BK_FS character in file name.
	 * Some day we may allow caller to escape the BK_FS character
	 */
	if (strchr(name, BK_FS)) return (1);

	if (streq(base, BKSKIP)) return (1);
	if (streq(base, GCHANGESET) && !streq(name, GCHANGESET)) return (1);
	if (streq(base, ".bk")) return (1);

	return (0);
}

/*
 * Check in initial sfile.
 *
 * XXX - need to make sure that they do not check in binary files in
 * gzipped format - we can't handle that yet.
 */
/* ARGSUSED */
private int
checkin(sccs *s,
	int flags, delta *prefilled, int nodefault, MMAP *diffs, char **syms)
{
	FILE	*sfile = 0, *gfile = 0;
	delta	*n0 = 0, *n, *first;
	int	added = 0;
	size_t	len;
	int	i;
	char	*t;
	char	buf[MAXLINE];
	admin	l[2];
	int	no_lf = 0;
	int	error = 0;
	int	short_key = 0;
	MDBM	*db;
	static	int fixDate = -1;

	if (fixDate == -1) fixDate = (getenv("_BK_NO_UNIQUE") == 0);

	assert(s);
	debug((stderr, "checkin %s %x\n", s->gfile, flags));
	unless (flags & NEWFILE) {
		verbose((stderr,
		    "%s not checked in, use -i flag.\n", s->gfile));
out:		if (sfile) sccs_abortWrite(s, &sfile);
		sccs_unlock(s, 'z');
		if (prefilled) sccs_freedelta(prefilled);
		if (gfile && (gfile != stdin)) fclose(gfile);
		s->state |= S_WARNED;
		return (-1);
	}
	if (diffs) {
		if (binaryCheck(diffs)) {
			fprintf(stderr,
			    "%s has nulls (\\0) in diffs, checkin aborted.\n",
			    s->gfile);
			goto out;
		}
	} else if (isRegularFile(s->mode)) {
		openInput(s, flags, &gfile);
		if (BAM(s) && bk_notLicensed(s->proj, LIC_BAM, 1)) goto out;
		unless (gfile || BAM(s)) {
			perror(s->gfile);
			goto out;
		}
		if (toobig(s)) goto out;
	} else if (S_ISLNK(s->mode) && BINARY(s)) {
		fprintf(stderr, "%s: symlinks should not use BINARY mode!\n",
		    s->gfile);
		goto out;
	}

	if (HASGRAPH(s)) {
		fprintf(stderr, "delta: %s already exists\n", s->sfile);
		goto out;
	}
	/* This should never happen - the zlock should protect */
	if (exists(s->sfile)) {
		fprintf(stderr, "delta: lost checkin race on %s\n", s->sfile);
		goto out;
	}

	/* pathname, we need this below */
	buf[0] = 0;
	t = relativeName(s, 0, s->proj);
	assert(t);
	strcpy(buf, t);

	if (bk_badFilename(buf)) {
		fprintf(stderr, "%s: illegal filename: %s\n", prog, buf);
		goto out;
	}

	unless (sfile = sccs_startWrite(s)) goto out;
	if ((flags & DELTA_PATCH) || proj_root(s->proj)) {
		s->bitkeeper = 1;
		s->xflags |= X_BITKEEPER;
	}
	/*
	 * Do a 1.0 delta unless
	 * a) there is a init file (nodefault), or
	 * b) prefilled->rev is initialized, or
	 * c) the DELTA_EMPTY flag is set
	 */
	if (nodefault ||
	    (flags & DELTA_EMPTY) || (prefilled && prefilled->r[0])) {
		first = n = prefilled ? prefilled : new(delta);
	} else {
		first = n0 = new(delta);
		/*
		 * We don't do modes here.  The modes should be part of the
		 * per LOD state, so each new LOD starting from 1.0 should
		 * have new modes.
		 *
		 * We do do flags here, the initial flags are per file.
		 * XXX - is this the right answer?
		 */
		revArg(s, n0, "1.0");
		if (buf[0]) pathArg(s, n0, buf); /* pathname */

		n0 = sccs_dInit(n0, 'D', s, nodefault);
		n0->flags |= D_CKSUM;
		n0->sum = almostUnique();
		first = n0 = dinsert(s, n0, fixDate && !(flags & DELTA_PATCH));

		n = prefilled ? prefilled : new(delta);
		n->pserial = 1;
	}
	assert(n);
	if (!nodefault && buf[0]) pathArg(s, n, buf); /* pathname */
	n = sccs_dInit(n, 'D', s, nodefault);

	/*
	 * We want the 1.0 and 1.1 keys to naturally not collide so we
	 * we need different dates.
	 * It would probably be more correct to not fudge the 1.1 delta
	 * forward but instead fudge the 1.0 1 second backwards but that
	 * is a royal pain the butt to do in all cases.  So here we are.
	 */
	if (n0) {
		n->date++;
		n->dateFudge++;
	}

	if (s->mode & 0111) s->mode |= 0110;	/* force user/group execute */
	s->mode |= 0220;			/* force user/group write */
	s->mode |= 0440;			/* force user/group read */

	updMode(s, n, 0);
	if (!n->r[0]) revArg(s, n, n0 ? "1.1" : "1.0");
	if (nodefault) {
		if (prefilled) s->xflags |= prefilled->xflags;
	} else if (ASCII(s)) {
		unless (CSET(s)) {
			/* check eoln preference */
			s->xflags |= X_DEFAULT;
			if (s->proj) {
				db = proj_config(s->proj);
				if (db) {
					char *p = mdbm_fetch_str(db, "eoln");
					if (p && streq("unix", p)) {
						s->xflags &= ~X_EOLN_NATIVE;
						s->xflags &= ~X_EOLN_WINDOWS;
					}
					if (p && streq("windows", p)) {
						s->xflags &= ~X_EOLN_NATIVE;
						s->xflags |= X_EOLN_WINDOWS;
					}

					if (p = mdbm_fetch_str(db, "keyword")) {
						if (strstr(p, "sccs")) {
							s->xflags |= X_SCCS;
						}
						if (strstr(p, "rcs")) {
							s->xflags |= X_RCS;
						}
						if (strstr(p, "expand1")) {
							s->xflags |= X_EXPAND1;
						}
					}
				}
			}
		}
	}
	if (n->flags & D_BADFORM) {
		fprintf(stderr, "checkin: bad revision: %s for %s\n",
		    REV(s, n), s->sfile);
		goto out;
	} else {
		l[0].flags = 0;
	}
	/* need random set before the call to sccs_sdelta */
	/* XXX: changes n, so must be after syms stuff */
	unless (nodefault || (flags & DELTA_PATCH)) {
		delta	*d = n0 ? n0 : n;

		if (!d->random && !short_key) {
			if (t = getenv("BK_RANDOM")) {
				strcpy(buf+2, t);
			} else {
				randomBits(buf+2);
			}
			t = buf+2;
			assert(t[0]);
			if (BAM(s)) {
				t -= 2;
				t[0] = 'B';
				t[1] = ':';
			}
			d->random = sccs_addStr(s, t);
		}
	        unless (n->comments || (flags & DELTA_DONTASK)) {
			if (flags & DELTA_CFILE) {
				if (comments_readcfile(s, 0, n)) goto out;
			} else if (comments_readcfile(s, 1, n) == -2) {
				/* aborted prompt */
				goto out;
			}
		}
		unless (n->comments) {
			t = fullname(s->gfile, 0);
			sprintf(buf, "BitKeeper file %s\n", t);
			free(t);
			n->comments = sccs_addStr(s, buf);
		}
	}
	/* need to recover 'first' after a possible malloc */
	i = (first == n) ? 0 : SERIAL(s, first);
	n = dinsert(s, n, fixDate && !(flags & DELTA_PATCH));
	first = i ? SFIND(s, i) : n;
	s->numdeltas++;
	EACH(syms) addsym(s, n, !(flags & DELTA_PATCH), syms[i]);
	if (BITKEEPER(s)) {
		s->version = SCCS_VERSION;
		unless (flags & DELTA_PATCH) {
			first->flags |= D_XFLAGS;
			first->xflags = s->xflags;
		}
		if (CSET(s)) {
			unless (first->csetFile) {
				first->sum = almostUnique();
				first->flags |= D_ICKSUM;
				sccs_sdelta(s, first, buf);
				csetFileArg(s, first, buf);
			}
			first->flags |= D_CKSUM;
		} else {
			unless (first->csetFile) {
				csetFileArg(s, first, proj_rootkey(s->proj));
			}
		}
	}
	if (BAM(s)) {
		assert(n == s->table);
		if (!(flags & DELTA_PATCH) && bp_delta(s, n)) {
			fprintf(stderr, "BAM delta of %s failed\n", s->gfile);
			goto out;
		}
		added = s->added = n->added;
	}
	n->flags |= D_CKSUM;
	if (delta_table(s, sfile, 1)) {
		error++;
		goto out;
	}
	if (BAM(s)) goto skip_weave;
	buf[0] = 0;
	if (GZIP_OUT(s)) sccs_zputs_init(s, sfile);
	if (n0) {
		fputdata(s, "\001I 2\n", sfile);
	} else {
		fputdata(s, "\001I 1\n", sfile);
	}
	s->dsum = 0;
	if (!(flags & DELTA_PATCH) && BINARY(s)) {
		/* XXX - this is incorrect, it needs to do it depending on
		 * what the encoding is.
		 */
		added = uuencode_sum(s, gfile, sfile);
	} else {
		if (diffs) {
			int	off = 0;
			char	*t;

			if ((t = mnext(diffs)) && isdigit(t[0])) off = 2;
			while (t = mnext(diffs)) {
				if ((off == 0) && (t[0] == '\\')) {
					++t;
				} else {
					t = &t[off];
				}
				s->dsum += fputdata(s, t, sfile);
				added++;
			}
			mclose(diffs);
		} else if (gfile) {
			int	crnl_bug = (getenv("_BK_CRNL_BUG") != 0);

			strcpy(buf, "\n");
			while (t = fgetln(gfile, &len)) {
				assert(!no_lf);
				fix_cntl_a(s, t, sfile);
				--len;
				if (t[len] != '\n') {
					/* must be last line in file */
					no_lf = 1;
					buf[0] = t[len]; /* buf last char */
				} else unless (crnl_bug) {
					while ((len > 0) &&
					    (t[len - 1] == '\r')) {
						--len;
					}
				}
				t[len] = 0;
				if (len) s->dsum += fputdata(s, t, sfile);
				s->dsum += fputdata(s, buf, sfile);
				added++;
			}
			/*
			 * For ascii files, add missing \n automagically.
			 */
			if (no_lf) {
				/* put lf in sfile, but not in dsum */
				fputdata(s, "\n", sfile);
			}
		} else if (S_ISLNK(s->mode)) {
			u8	*t;

			for (t = s->symlink; t && *t; s->dsum += *t++);
		}
	}
	if (n0) {
		fputdata(s, "\001E 2", sfile);
		if (no_lf) fputdata(s, "N", sfile);
		fputdata(s, "\n", sfile);
		fputdata(s, "\001I 1\n", sfile);
		fputdata(s, "\001E 1\n", sfile);
	} else {
		fputdata(s, "\001E 1", sfile);
		if (no_lf) fputdata(s, "N", sfile);
		fputdata(s, "\n", sfile);
	}
skip_weave:
	if (GZIP_OUT(s)) sccs_zputs_done(s);
	error = end(s, n, sfile, flags, added, 0, 0);
	if (gfile && (gfile != stdin)) {
		fclose(gfile);
		gfile = 0;
	}
	if (error) {
		fprintf(stderr, "checkin: cannot construct sfile\n");
		goto out;
	}
	unless (flags & DELTA_SAVEGFILE) unlinkGfile(s);	/* Careful */
	if (sccs_finishWrite(s, &sfile)) goto out;
	if (BITKEEPER(s)) updatePending(s);
	comments_cleancfile(s);
	sccs_unlock(s, 'z');
	return (0);
}

/*
 * Figure out if anything has been used more than once.
 */
private int
checkdups(sccs *s)
{
	MDBM	*db;
	delta	*d;
	datum	k, v;
	char	c = 0;
	ser_t	ser;

	db = mdbm_open(NULL, 0, 0, 0);
	v.dsize = sizeof(ser_t);
	for (d = s->table; d; d = NEXT(d)) {
		if (TAG(d)) continue;
		ser = SERIAL(s, d);
		k.dptr = (void*)d->r;
		k.dsize = sizeof(d->r);
		v.dptr = (void*)&ser;
		if (mdbm_store(db, k, v, MDBM_INSERT)) {
			v = mdbm_fetch(db, k);
			fprintf(stderr, "%s: %s in use by serial %d and %d.\n",
			    s->sfile, REV(s, d), ser, *(ser_t*)v.dptr);
			c = 1;
		}
	}
	mdbm_close(db);
	return (c);
}

private inline int
isleaf(register sccs *s, register delta *d)
{
	delta	*t;

	if (TAG(d)) return (0);
	/*
	 * June 2002: ignore lod stuff, we're removing that feature.
	 * We'll later add back the support for 2.x, 3.x, style numbering
	 * and then we'll need to remove this.
	 */
	assert(d->r[0] == 1);
	for (t = d+1; t <= s->table; t++) {
		unless (t->flags && !(t->flags & D_GONE)) continue;
		if (TAG(t)) continue;

		if ((t->pserial == SERIAL(s, d)) ||
		    (t->merge == SERIAL(s, d))) {
			return (0);
		}
	}
	return (1);
}

int
sccs_isleaf(sccs *s, delta *d)
{
	return (isleaf(s, d));
}

/*
 * Check open branch
 * XXX: Assumes D_RED is clear ; exits with D_RED clear
 */
private int
checkOpenBranch(sccs *s, int flags)
{
	delta	*d, *tip = 0, *symtip = 0;
	int	ret = 0, tips = 0, symtips = 0;

	for (d = s->table; d; (d->flags &= ~D_RED), d = NEXT(d)) {
		/*
		 * This order is important:
		 * Skip 1.0,
		 * check for bad R delta even if it is marked gone so we warn,
		 * then skip the rest if they are GONE.
		 */
		if (streq(REV(s, d), "1.0")) continue;
		if (CSET(s)) {
			if (!d->added && !d->deleted && !d->same &&
			    !(d->flags & D_SYMBOLS) && !SYMGRAPH(d)) {
				verbose((stderr,
				    "%s: illegal removed delta %s\n",
					s->sfile, REV(s, d)));
				ret = 1;
			}
			if (SYMLEAF(d) && !(d->flags & D_GONE)) {
				if (symtips) {
					if (symtips == 1) {
					    verbose((stderr,
			    			"%s: unmerged symleaf %s\n",
						    s->sfile, REV(s, symtip)));
					}
					verbose((stderr,
			    		    "%s: unmerged symleaf %s\n",
					    s->sfile, REV(s, d)));
					ret = 1;
				}
				symtip = d;
				symtips++;
			}
		}
		if ((d->flags & D_GONE) || TAG(d)) continue;
		unless (d->flags & D_RED) {
			if (tips) {
				if (tips == 1) {
				    verbose((stderr,
		    			"%s: unmerged leaf %s\n",
					s->sfile, REV(s, tip)));
				}
				verbose((stderr,
		    		    "%s: unmerged leaf %s\n",
				    s->sfile, REV(s, d)));
				ret = 1;
			}
			tip = d;
			tips++;
		}
		if (d->pserial) PARENT(s, d)->flags |= D_RED;
		if (d->merge) MERGE(s, d)->flags |= D_RED;
	}
	return (ret);
}


/*
 * Check all the BitKeeper specific stuff such as
 *	. no open branches
 *	. checksums on all deltas
 *	. xflags history
 *	. tag structure
 */
private int
checkInvariants(sccs *s, int flags)
{
	int	error = 0;
	int	xf = (flags & SILENT) ? XF_STATUS : XF_DRYRUN;
	delta	*d;

	error |= checkOpenBranch(s, flags);
	error |= checkTags(s, flags);
	for (d = s->table; d; d = NEXT(d)) {
		if (!TAG(d) && !(d->flags & D_CKSUM)) {
			verbose((stderr,
			    "%s|%s: no checksum\n", s->gfile, REV(s, d)));
		}
		if (d->xflags && checkXflags(s, d, xf)) {
			extern	int xflags_failed;

			xflags_failed = 1;
			error |= 1;
		}
		if (d->mtag && !sfind(s, d->mtag)) {
			verbose((stderr,
			    "%s|%s: tag merge %u does not exist\n",
			    s->gfile, REV(s, d), d->mtag));
			error |= 1;
		}
		if (d->ptag && !sfind(s, d->ptag)) {
			verbose((stderr,
			    "%s|%s: tag parent %u does not exist\n",
			    s->gfile, REV(s, d), d->ptag));
			error |= 1;
		}
	}
	return (error);
}

/*
 * Given a graph with some deltas marked as gone (D_SET|D_GONE),
 * make sure that things will be OK with those deltas gone.
 * This means for each delta that is not gone:
 *	. make sure its parents (d->parent and d->merge) are not gone
 *	. make sure each delta it includes/excludes is not gone
 *
 * Since we are single rooted, then means that getting rid of all
 * the gone will leave us with a consistent tree.
 */
private int
checkGone(sccs *s, int bit, char *who)
{
	u8	*slist = setmap(s, bit, 1);
	delta	*d;
	char	*p;
	int	i, sign, error = 0;

	for (d = s->table; d; d = NEXT(d)) {
		if (d->flags & bit) continue;
		if (d->pserial && (PARENT(s, d)->flags & bit)) {
			error++;
			fprintf(stderr,
			"%s: revision %s not at tip of branch in %s.\n",
			    who, REV(s, PARENT(s, d)), s->sfile);
			s->state |= S_WARNED;
		}
		if (d->merge && slist[d->merge]) {
			error++;
			fprintf(stderr,
			"%s: revision %s not at tip of branch in %s.\n",
			    who, REV(s, MERGE(s, d)), s->sfile);
			s->state |= S_WARNED;
		}
		if (SYMGRAPH(d) && (d->ptag && slist[d->ptag])) {
			error++;
			fprintf(stderr,
			"%s: revision %s not at tip of tag graph in %s.\n",
			    who, REV(s, sfind(s, d->ptag)), s->sfile);
			s->state |= S_WARNED;
		}
		if (SYMGRAPH(d) && (d->mtag && slist[d->mtag])) {
			error++;
			fprintf(stderr,
			"%s: revision %s not at tip of tag graph in %s.\n",
			    who, REV(s, sfind(s, d->mtag)), s->sfile);
			s->state |= S_WARNED;
		}
		p = CLUDES(s, d);
		while (i = sccs_eachNum(&p, &sign)) {
			unless (slist[i]) continue;
			fprintf(stderr,
			    "%s: %s:%s %s %s\n", s->sfile,
			    who,
			    REV(s, d),
			    (sign > 0) ? "includes" : "excludes",
			    REV(s, sfind(s, i)));
			error++;
			s->state |= S_WARNED;
		}
	}
	free(slist);
	return (error);
}

private int
checkMisc(sccs *s, int flags)
{
	unless (s->version == SCCS_VERSION) {
	    verbose((stderr, "warning: %s version=%u, current version=%u\n",
		s->gfile, s->version, SCCS_VERSION));
	}
	return (0);
}

private int
checkrevs(sccs *s, int flags)
{
	delta	*prev;
	delta	*d;
	int	e;
	int	saw_sortkey = 0;

	prev = 0;
	for (e = 0, d = s->table; d; d = NEXT(d)) {
		e |= checkRev(s, s->sfile, d, flags);
		if ((flags & ADMIN_TIME) && prev && !earlier(s, d, prev)) {
			fprintf(stderr, "%s: %s is not earlier than %s\n",
			    s->sfile, REV(s, d), REV(s, prev));
			e |= 2;
		}
		prev = d;
		if (d->flags & D_SORTSUM) saw_sortkey = 1;
	}
	if (CSET(s) && saw_sortkey) {
		/*
		 * If the ChangeSet file has sortkey checksums then we
		 * enable the sortkey repo feature.  We don't bother
		 * looking for alternative pathnames for files because
		 * those will already be convered by the cset file.
		 */
		bk_featureSet(s->proj, FEAT_SORTKEY, 1);
	}
	return (e);
}

private int
checkRev(sccs *s, char *file, delta *d, int flags)
{
	char	*x;
	int	i, sign, error = 0;
	int	badparent;
	delta	*p;

	if (TAG(d) || (d->flags & D_GONE)) return (0);

	if (d->flags & D_BADFORM) {
		fprintf(stderr, "%s: bad rev '%s'\n", file, REV(s, d));
	}

	/*
	 * Make sure that the revision is well formed.
	 * The random part says that we allow x.y.z.0 if has random bits;
	 * that is for grafted trees.
	 */
	if (!d->r[0] || (BITKEEPER(s) && (d->r[0] != 1)) ||
	    (!d->r[2] && !d->r[1] && (d->r[0] != 1)) ||
	    (d->r[2] && (!d->r[3] && !d->random)))
	{
		unless (flags & ADMIN_SHUTUP) {
			fprintf(stderr, "%s: bad revision %s (parent = %s)\n",
			    file, REV(s, d),
			    d->pserial ? REV(s, PARENT(s, d)) : "Root");
		}
		error = 1;
	}
	/*
	 * XXX - this should check for BitKeeper files.  If it is not
	 * BitKeeper and the form is 1.0, that is an error.
	 */

	/*
	 * Make sure there is no garbage in the serial list[s].
	 */
	x = CLUDES(s, d);
	while (i = sccs_eachNum(&x, &sign)) {
		if (i < SERIAL(s, d)) continue;
		error = 1;
		if (flags & ADMIN_SHUTUP) continue;
		fprintf(stderr, "%s: %s has bad %s serial %d\n",
		    file, REV(s, d), (sign > 0) ? "include" : "exclude", i);
	}

	unless (p = PARENT(s, d)) {
		/* no parent */
		if (BITKEEPER(s) &&
		    !(streq(REV(s, d), "1.0") || streq(REV(s, d), "1.1"))) {
			unless (flags & ADMIN_SHUTUP) {
				fprintf(stderr,
				    "%s: rev %s not connected to trunk\n",
				    file, REV(s, d));
			}
			error = 1;
		}
		goto done;
	}

	badparent = 0;
	/*
	 * Two checks here.
	 * If they are on the same branch, is the sequence numbering
	 * correct?  Handle 1.9 -> 2.1 properly.
	 */
	if (d->r[2]) {
		/* If a x.y.z.q release, then it's trunk node should be x.y, */
		if ((p->r[0] != d->r[0]) || (p->r[1] != d->r[1])) {
			badparent = 1;
		}
		if (d->r[3] == 0) badparent = 1;
		if (p->r[2]) {
			if ((d->r[3] > 1) && (d->r[3] != p->r[3]+1)) {
				badparent = 1;
			}
#ifdef	CRAZY_WOW
			// XXX - this should be an option to admin.

			/* if there is a parent, and the parent is a
			 * x.y.z.q, and this is an only child, then
			 * insist that the revs are on the same
			 * branch.
			 */
			if (onlyChild(d) && !samebranch(d, d->parent)) {
				badparent = 1;
			}
#endif
		} else {
			if (d->r[3] != 1) badparent = 1;
		}
	} else {
		if (d->r[0] == p->r[0]) {
			if (d->r[1] != p->r[1]+1) badparent = 1;
		} else {
			if (d->r[1] != 1) badparent = 1;
		}
	}
	if (badparent) {
		unless (flags & ADMIN_SHUTUP) {
			fprintf(stderr,
			    "%s: rev %s has incorrect parent %s\n",
			    file, REV(s, d), REV(s, p));
		}
		error = 1;
	}

	/* If there is a parent, make sure the dates increase. */
	if (d->date < p->date) {
		if (flags & ADMIN_TIME) {
			fprintf(stderr,
			    "%s: time goes backwards between %s and %s\n",
			    file, REV(s, d), REV(s, PARENT(s, d)));
			fprintf(stderr, "\t%s: %s",
			    REV(s, d), delta_sdate(s, d));
			fprintf(stderr, "    %s: %s -> %d seconds\n",
			    REV(s, PARENT(s, d)),
			    delta_sdate(s, PARENT(s, d)),
			    (int)(d->date - PARENT(s, d)->date));
			error |= 2;
		}
	}

done:	if (error & 1) d->flags |= D_BADREV;
	return (error);
}

private int
getval(char *arg)
{
	if (!arg || !*arg || !isdigit(*arg)) return (-1);
	return (atoi(arg));
}

/*
 * Accept dates in any of the following forms and convert to the
 * SCCS form, filling out the delta struct.
 *	1998-01-11 20:00:00-08		(RCS -zLT)
 *	1990/01/12 04:00:00		(RCS UTC)
 *	98/01/11 20:00:00.000-8:00	(LMSCCS fully qualified)
 *	98/01/11 20:00:00.000		(LMSCCS localtime)
 *	98/01/11 20:00:00		(SCCS, local time)
 *
 * XXX this could be way simpler, but it isn't used much
 */
private delta *
dateArg(sccs *s, delta *d, char *arg, int defaults)
{
	char	*save = arg;
	char	tmp[50];
	int	year, month, day, hour, minute, second, msec, hwest, mwest;
	char	sign = ' ';
	int	rcs = 0;
	int	gotZone = 0;
	char	*zone;

	if (!d) d = new(delta);
	if (!arg || !*arg) { d->flags = D_ERROR; return (d); }
	year = getval(arg);
	if (year == -1) {
out:		fprintf(stderr, "sccs: can't parse date format %s at %s\n",
		    save, arg && *arg ? arg : "(unknown)");
		d->flags = D_ERROR;
		return (d);
	}
	if (year >= 100) {
		rcs++;
		if (year > 2068 || year < 1969) {
			goto out;
		}
		if (year < 2000) year -= 1900;
		if (year >= 2000) year -= 2000;
	}
/* CSTYLED */
#define	move(a) {while (*a && isdigit(*a)) a++; if (*a && (*a++ == '-')) rcs++;}
#define	getit(what) {move(arg); what = getval(arg); if (what == -1) goto out; }
	getit(month);
	getit(day);
	getit(hour);
	getit(minute);
	getit(second);
	hwest = mwest = 0;
	msec = 0;
	if (*arg && arg[2] == '.') {
		getit(msec);
		if (arg[-1] == '-') {
			gotZone++;
			sign = '-';
			getit(hwest);
			getit(mwest);
		}
	} else if (*arg && arg[2] == '-') {
		gotZone++;
		sign = '-';
		getit(hwest);
		/* I don't know if RCS ever puts in the minutes, but I'll
		 * take 'em if they give 'em.
		 */
		if (*arg && arg[2] == ':') getit(mwest);
	} else if (*arg && arg[2] == '+') {
		gotZone++;
		sign = '+';
		getit(hwest);
		/* I don't know if RCS ever puts in the minutes, but I'll
		 * take 'em if they give 'em.
		 */
		if (*arg && arg[2] == ':') getit(mwest);
	} else if (rcs || defaults) {
		/* This is a bummer because we can't figure out in which
		 * timezone the delta was performed.
		 * So we assume here.
		 * XXX - maybe not the right answer?
		 */
		long	seast;
		time_t	t = time(0);

		gotZone++;
		localtimez(&t, &seast);
		if (seast < 0) {
			seast = -seast;
			sign = '-';
		} else {
			sign = '+';
		}

		hwest = seast / 3600;
		mwest = (seast % 3600) / 60;
	}
	if (gotZone) {
		sprintf(tmp, "%c%02d:%02d", sign, hwest, mwest);
		zoneArg(s, d, tmp);
		zone = ZONE(s, d);
	} else if (s->table) {
		zone = ZONE(s, s->table);
	} else {
		zone = 0;
	}
	sprintf(tmp, "%02d/%02d/%02d %02d:%02d:%02d",
	    year, month, day, hour, minute, second);
	d->date = date2time(tmp, zone, EXACT) + d->dateFudge;
	return (d);
}
#undef	getit
#undef	move

private delta *
userArg(sccs *s, delta *d, char *arg)
{
	delta	*p = PARENT(s, d);
	char	*host;
	char	buf[MAXLINE];

	if (!strchr(arg, '@')) {
		/* if missing host then keep existing or use parent */
		if ((d->userhost && (host = HOSTNAME(s, d)) && *host) ||
		    (p && (host = HOSTNAME(s, p)) && *host)) {
			sprintf(buf, "%s@%s", arg, host);
			arg = buf;
		}
	}
	d->userhost = sccs_addUniqStr(s, arg);
	return (d);
}


/*
 * Process the various args which we might have to save.
 * Null args are accepted in those with a "0" as arg 2.
 */
private delta *
csetFileArg(sccs *s, delta *d, char *arg)
{
	unless (d) d = new(delta);
	unless (arg && *arg) return (d);
	d->csetFile = sccs_addUniqStr(s, arg);
	return (d);
}

private delta *
hashArg(sccs *s, delta *d, char *arg)
{
	unless (d) d = new(delta);
	d->bamhash = sccs_addStr(s, arg);
	return (d);
}

/*
 * we want to avoid calling this if possible, but
 * it changes the hostname in d->userhost
 */
private delta *
hostArg(sccs *s, delta *d, char *arg)
{
	char	buf[MAXLINE];

	unless (d) d = new(delta);
	assert(arg && *arg);
	sprintf(buf, "%s@%s", USER(s, d), arg);
	d->userhost = sccs_addUniqStr(s, buf);
	return (d);
}

private delta *
randomArg(sccs *s, delta *d, char *arg)
{
	if (!d) d = new(delta);
	d->random = sccs_addStr(s, arg);
	return (d);
}

/*
 * arg may have optional sortpath: "pathname[|sortpath]"
 * d->pathname stores 2 strings: the sortpath is in the hidden string
 */
private delta *
pathArg(sccs *s, delta *d, char *arg)
{
	char	*sp;
	char	buf[2 * MAXPATH];	// path|origpath\0

	if (!d) d = new(delta);
	if (!arg || !*arg) return (d);

	strcpy(buf, arg);
	if (sp = strchr(buf, '|')) {
		*sp++ = 0;

		d->sortPath = sccs_addUniqStr(s, sp);
	} else {
		d->sortPath = 0;
	}
	d->pathname = sccs_addUniqStr(s, buf);
	return (d);
}

/*
 * Handle either 0664 style or -rw-rw-r-- style.
 */
delta *
modeArg(sccs *s, delta *d, char *arg)
{
	char	*t;
	unsigned int m;

	if (!d) d = new(delta);
	unless (m = getMode(arg)) return (0);
	if (S_ISLNK(m)) {
		unless (t = strchr(arg, ' ')) return (0);
		++t;
		d->symlink = sccs_addUniqStr(s, t);
	}
	if (d->mode = m) d->flags |= D_MODE;
	return (d);
}

private delta *
sumArg(delta *d, char *arg)
{
	if (!d) d = new(delta);
	d->flags |= D_CKSUM;
	d->sum = atoi_p(&arg);
	if (*arg++ == '|') {
		d->sortSum = atoi_p(&arg);
		d->flags |= D_SORTSUM;
	}
	return (d);
}

private delta *
mergeArg(delta *d, char *arg)
{
	if (!d) d = new(delta);
	assert(d->merge == 0);
	assert(isdigit(arg[0]));
	d->merge = atoi(arg);
	return (d);
}


/* add a symbol in mkgraph(), not used with binary sfiles */
private void
symArg(sccs *s, delta *d, char *name)
{
	u32	tmp;

	assert(d);

	unless (CSET(s)) return;	/* no tags on regular files */

	/*
	 * Stash away the parent (and maybe merge) serial numbers
	 *
	 * Note that if this is a tag merge and there is no symbol on
	 * this node, then it never gets added to the symbol table.
	 * So you can't walk the symbol table looking for metad->symLeaf
	 * to find leaves.  See sccs_tagleaves().
	 */
	if (isdigit(*name)) {
		d->flags |= D_SYMGRAPH;
		d->ptag = atoi(name);
		while (isdigit(*name)) name++;
		unless (*name++ == ' ') {
			return;
		}
		if (isdigit(*name)) {
			d->mtag = atoi(name);
			while (isdigit(*name)) name++;
			unless (*name++ == ' ') {
				return;
			}
		}
		assert(*name == 'l');
		d->flags |= D_SYMLEAF;
		return;
	}
	tmp = SERIAL(s, d);
	addArray(&s->mg_symname, &tmp);
	tmp = sccs_addUniqStr(s, name);
	addArray(&s->mg_symname, &tmp);
	d->flags |= D_SYMBOLS;	/* so mkgraph won't MKGONE it */

	if ((SERIAL(s, d) == 1) && streq(name, KEY_FORMAT2)) {
		s->xflags |= X_LONGKEY;
		EACHP(s->slist, d) {
			if (d->flags & D_XFLAGS) d->xflags |= X_LONGKEY;
		}
	}
}

private delta *
zoneArg(sccs *s, delta *d, char *arg)
{
	char	buf[20];

	unless ((arg[0] == '+') || (arg[0] == '-')) {
		sprintf(buf, "-%s", arg);
		arg = buf;
	}
	unless (d) d = new(delta);
	d->zone = sccs_addUniqStr(s, arg);
	return (d);
}

/*
 * Take a string with newlines in it and split it into lines.
 * Note: null comments are accepted on purpose.
 */
private delta *
commentArg(sccs *s, delta *d, char *arg)
{
	if (!d) d = new(delta);
	unless (arg) arg = "\n";
	d->comments = sccs_addStr(s, arg);
	return (d);
}

/*
 * Explode the rev.
 */
private delta *
revArg(sccs *s, delta *d, char *arg)
{
	if (!d) d = new(delta);
	explode_rev(s, d, arg);
	return (d);
}

/*
 * Partially fill in a delta struct.  If the delta is null, allocate one.
 * Follow all the conventions used for delta creation such that this delta
 * can be added to the tree and freed later.
 */
delta *
sccs_parseArg(sccs *s, delta *d, char what, char *arg, int defaults)
{
	switch (what) {
	    case 'A':	/* hash */
		return (hashArg(s, d, arg));
	    case 'B':	/* csetFile */
		return (csetFileArg(s, d, arg));
	    case 'D':	/* any part of 1998/03/09 18:23:45.123-08:00 */
		return (dateArg(s, d, arg, defaults));
	    case 'U':	/* user or user@host */
		return (userArg(s, d, arg));
	    case 'H':	/* host */
		return (hostArg(s, d, arg));
	    case 'P':	/* pathname */
		return (pathArg(s, d, arg));
	    case 'O':	/* mode */
		return (modeArg(s, d, arg));
	    case 'C':	/* comments - one string, possibly multi line */
		return (commentArg(s, d, arg));
	    case 'R':	/* 1 or 1.2 or 1.2.3 or 1.2.3.4 */
		return (revArg(s, d, arg));
	    case 'Z':	/* zone */
		return (zoneArg(s, d, arg));
	    default:
		fprintf(stderr, "Unknown user arg %c ignored.\n", what);
		return (0);
	}
}

/*
 * Return true iff the most recent matching symbol is the same.
 */
private int
dupSym(sccs *sc, char *s, char *rev)
{
	symbol	*sym;

	sym = findSym(sc, s);
	/* If rev isn't set, then any name match is enough */
	if (sym && !rev) return (1);
	return (sym && streq(REV(sc, SFIND(sc, sym->ser)), rev));
}

/* 'bk tag' comes here */
private int
addSym(char *me, sccs *sc, int flags, admin *s, int *ep)
{
	int	added = 0, i, error = 0;
	char	*rev;
	char	*sym;
	delta	*d, *n = 0;

	unless (s && s[0].flags) return (0);

	unless (CSET(sc)) {
		fprintf(stderr,
		    "Tagging files is not supported, use bk tag instead\n");
		return (0);
	}
	if (proj_isComponent(sc->proj)) {
		fprintf(stderr,
		    "%s: component tags not yet supported.\n", prog);
		return (0);
	}

	/*
	 * "sym" means TOT
	 * "sym|" means TOT
	 * "sym|1.2" means that rev.
	 */
	for (i = 0; s && s[i].flags; ++i) {
		sym = strdup(s[i].thing);
		if (rev = strrchr(sym, '|')) *rev++ = 0;
		/* Note: rev is set or null from above test */
		unless (d = sccs_findrev(sc, rev)) {
			verbose((stderr,
			    "%s: can't find %s in %s\n",
			    me, rev, sc->sfile));
sym_err:		error = 1; sc->state |= S_WARNED;
			free(sym);
			continue;
		}
		if (!rev || !*rev) rev = REV(sc, d);
		if (sccs_badTag(me, sym, flags)) goto sym_err;
		if (dupSym(sc, sym, rev)) {
			verbose((stderr,
			    "%s: symbol %s exists on %s\n", me, sym, rev));
			goto sym_err;
		}

		// XXX - if anyone calls admin directly with two tags this can
		// be wrong.  bk tag doesn't.
		// We should just get rid of the multiple tag thing, it was
		// over engineered.
		unless (d == sccs_top(sc)) {
			safe_putenv("BK_TAG_REV=%s", REV(sc, d));
		}
		safe_putenv("BK_TAG=%s", sym);
		if (trigger("tag", "pre")) goto sym_err;

		n = new(delta);
		n = sccs_dInit(n, 'R', sc, 0);
		/*
		 * n->sum = (unsigned short) almostUnique(1);
		 *
		 * We don't write out the sum because we use chksum of zero to
		 * mean that this is a tag not a changeset.
		 * XXX - this is completely broken.  Changesets can have zero
		 * checksums.  The only place we use this is in the synckeys
		 * processing and we need to hand that info across some other
		 * way, like in the environment or an optional trailer block.
		 */
		memcpy(n->r, d->r, sizeof(d->r));
		n->pserial = SERIAL(sc, d);
		sc->numdeltas++;
		n = dinsert(sc, n, 1);
		if (addsym(sc, n, 1, sym)) {
			verbose((stderr,
			    "%s: won't add identical symbol %s to %s\n",
			    me, sym, sc->sfile));
			/* No error here, it's not necessary */
			free(sym);
			continue;
		}
		added++;
		verbose((stderr,
		    "%s: add symbol %s->%s in %s\n",
		    me, sym, rev, sc->sfile));
		free(sym);
	}
	if (ep) *ep = error;
	return (added);
}


private	delta *
newDelta(sccs *sc, delta *p, int isNullDelta)
{
	delta	*n;
	char	*rev;

	/*
 	 * Until we fix the ChangeSet processing code
	 * we cannot allow null delta in ChangeSet file
	 */
	if (CSET(sc) && isNullDelta) {
		fprintf(stderr,
			"Cannot create null delta in ChangeSet file\n");
		return (0);
	}

	n = new(delta);
	n = sccs_dInit(n, 'D', sc, 0);
	unless (p) p = findrev(sc, 0);
	rev = REV(sc, p);
	sccs_getedit(sc, &rev);
	revArg(sc, n, rev);
	n->pserial = SERIAL(sc, p);
	sc->numdeltas++;
	if (isNullDelta) {
		n->added = n->deleted = 0;
		n->same = p->same + p->added;
		n->sum = almostUnique();
		n->flags |= D_CKSUM;
	}
	n = dinsert(sc, n, 1);
	return (n);
}

private void
addMode(sccs *sc, delta *n, mode_t m, char ***comments)
{
	char	buf[50];
	char	*newmode;

	assert(n);
	newmode = mode2a(m);
	assert(!streq(newmode, "<bad mode>"));
	sprintf(buf, "Change mode to %s", newmode);
	*comments = addLine(*comments, strdup(buf));
	(void)modeArg(sc, n, newmode);
}

private int
changeXFlag(sccs *sc, delta *n, int flags, int add, char *flag,
    char ***comments)
{
	char	buf[50];
	u32	xflags, changing, eoln;

	assert(flag);

	changing = a2xflag(flag);
	unless (xflags = sccs_xflags(sc, n)) xflags = sc->xflags;

	if (add) {
		if (xflags & changing) {
			verbose((stderr,
			    "admin: warning: %s %s flag is already on\n",
			    sc->sfile, flag));
			return (0);
		} 
		xflags |= changing;
	} else {
		unless ((changing == X_EOLN_UNIX) || (xflags & changing)) {
			verbose((stderr,
			    "admin: warning: %s %s flag is already off\n",
			    sc->sfile, flag));
			return (0);
		}
		xflags &= ~changing;
	}
	sc->xflags = xflags;
	assert(n);
	n->flags |= D_XFLAGS;
	n->xflags = xflags;
	/* pseudo flag, we speak only native & windows */
	unless (changing == X_EOLN_UNIX) {
		sprintf(buf, "Turn %s %s flag", add ? "on": "off", flag);
		*comments = addLine(*comments, strdup(buf));
	}
	/*
	 * We have two real EOLN xflags: X_EOLN_NATIVE and X_WINDOWS
	 * and one fake one: X_EOLN_UNIX = !X_EOLN_NATIVE
	 * and they are all mutually exclusive.
	 * Setting any one clears the others.
	 * Setting X_EOLN_UNIX clears all of them.
	 */
	eoln = X_EOLN_NATIVE|X_EOLN_WINDOWS|X_EOLN_UNIX;
	if (add && (eoln & changing)) {
		if (changing == X_EOLN_NATIVE) {
			if (n->xflags & X_EOLN_WINDOWS) {
				*comments = addLine(*comments,
				    strdup("Turn off EOLN_WINDOWS flag"));
				n->xflags &= ~X_EOLN_WINDOWS;
			}
		}
		if (changing == X_EOLN_WINDOWS) {
			if (n->xflags & X_EOLN_NATIVE) {
				*comments = addLine(*comments,
				    strdup("Turn off EOLN_NATIVE flag"));
				n->xflags &= ~X_EOLN_NATIVE;
			}
		}
		if (changing == X_EOLN_UNIX) {
			if (n->xflags & X_EOLN_WINDOWS) {
				*comments = addLine(*comments,
				    strdup("Turn off EOLN_WINDOWS flag"));
				n->xflags &= ~X_EOLN_WINDOWS;
			}
			if (n->xflags & X_EOLN_NATIVE) {
				*comments = addLine(*comments,
				    strdup("Turn off EOLN_NATIVE flag"));
				n->xflags &= ~X_EOLN_NATIVE;
			}
		}
		n->xflags &= ~X_EOLN_UNIX;	/* fake, in mem only */
	} else if (!add) {
		if (changing & (X_EOLN_WINDOWS|X_EOLN_UNIX)) {
			*comments = addLine(*comments,
			    strdup("Turn on EOLN_NATIVE flag"));
			n->xflags |= X_EOLN_NATIVE;
		}
	}
	assert(!(n->xflags & X_EOLN_UNIX));
	return (1);
}

int
sccs_xflags(sccs *s, delta *d)
{
	while (d && !(d->flags & D_XFLAGS)) d = PARENT(s, d);
	if (d) return (d->xflags);
	return (0); /* old sfile, xflags values unknown */
}

/*
 * Translate an encoding string (e.g. "ascii") and a compression string
 * (e.g. "gzip") to a suitable value for sccs->encoding.
 */
int
sccs_encoding(sccs *sc, off_t size, char *encp)
{
	int	enc;
	int	bam;
	int	encoding = sc ? sc->encoding_in : E_ALWAYS;
	char	*compp;
	int	comp;

	if (encp) {
		if (streq(encp, "text")) enc = E_ASCII;
		else if (streq(encp, "ascii")) enc = E_ASCII;
		else if (streq(encp, "binary")) {
			bam = proj_configsize(sc ? sc->proj : 0, "BAM");
			if (bam && (bam <= size)) {
				enc = E_BAM;
			} else {
				enc = E_UUENCODE;
			}
		}
		else if (streq(encp, "uuencode")) enc = E_UUENCODE;
		else if (streq(encp, "BAM")) enc = E_BAM;
		else {
			fprintf(stderr,
			    "%s: unknown encoding format %s\n",
			    prog, encp);
			return (-1);
		}
		encoding &= ~E_DATAENC;
		encoding |= enc;
	}

	if (sc && sc->proj) {
		compp = proj_configval(sc->proj, "compression");

		if (!*compp || streq(compp, "gzip")) {
			comp = E_GZIP;
		} else if (streq(compp, "none")) {
			comp = 0;
		} else {
			fprintf(stderr, "%s: unknown compression format %s\n",
			    prog, compp);
			return (-1);
		}

		/* No gzip for BAM currently */
		if (encoding & E_BAM) comp = 0;
		encoding &= ~E_COMP;
		encoding |= comp;

		encoding &= ~E_BFILE;
		if (proj_configbool(sc->proj, "binfile")) encoding |= E_BFILE;
	}
	return (encoding);
}

/*
 * two uses:
 * if (sign == 0); print signed num
 * else printed sign + magnitude
 */
void
sccs_saveNum(FILE *f, int num, int sign)
{
	fprintf(f, " %s%d", (sign < 0) ? "-" : "", num);
}

/*
 * Give a string of serials stored in the heap however it is stored,
 * return the next token interpreted as a signed integer.
 * Note: if 0 is not a legitimate number, but if it needs to be,
 * then this can be modified.
 * EOS is either returning 0 (or too, *linep == 0).
 *
 * - return any junk after number
 *
 * ex:
 * line = CLUDES(s, d);
 * while (i = sccs_eachNum(&line, &sign)) {
 *	// do stuff to process integer
 * }
 *
 * Note: real atoi() does '-' sign handling; slib.c one doesn't
 */
int
sccs_eachNum(char **linep, int *signp)
{
	char	*p;
	int	neg;

	unless (p = eachstr(linep, 0)) return (0);
	if (signp) {
		neg = 1;
		if (*p == '-') {
			p++;
			*signp = -1;
		} else {
			*signp = 1;
		}
	} else {
		if (*p == '-') {
			p++;
			neg = -1;
		} else {
			neg = 1;
		}
	}
	return (neg * atoi(p));
}

/*
 * this is called before the insert or remove lines, so SERIAL(s,d)
 * is the old number.
 */
private void
adjust_serials(sccs *s, delta *d, int amount)
{
	int	ser, sign;
	char	*p;
	FILE	*f;

	unless (d->flags) return;
	/* Note: if HEAP_RELATIVE, then the cludes recalc does no change */
	if (d->cludes) {
		assert(INARRAY(d));
		p = CLUDES(s, d);
		f = fmem();
		while (ser = sccs_eachNum(&p, &sign)) {
			sccs_saveNum(f, ser + amount, sign);
		}
		d->cludes = sccs_addStr(s, fmem_peek(f, 0));
		fclose(f);
	}

	d->pserial += amount;
	if (d->ptag) d->ptag += amount;
	if (d->mtag) d->mtag += amount;
	if (d->merge) d->merge += amount;
}

/*
 * Cons up a 1.0 delta, initializing as much as possible from the 1.1 delta.
 * If this is a BitKeeper file with changeset marks, then we have to 
 * replicate the key on the 1.1 delta.
 *
 * RET: 0 - keeping going to add a 1.0 delta
 *      1 - no need to do any more work
 *      2 - 1.0 was there, but random added: only rechecksum
 */
private	int
insert_1_0(sccs *s, u32 flags)
{
	delta	*d;
	delta	*t;
	symbol	*sym;
	char	*p;
	int	csets = 0;
	int	len;
	char	key[MAXKEY];

	if (streq(REV(s, s->tree), "1.0")) {
		unless (s->tree->random) {
			len = sccs_sdelta(s, sccs_kid(s, s->tree), key);
			p = short_random(key, len);
			s->tree->random = sccs_addStr(s, p);
			free(p);
			return (2);
		}
		verbose((stderr, "admin: %s already has 1.0\n", s->gfile));
		return (1);
	}
	/*
	 * First bump all the serial numbers.
	 */
	EACHP(s->slist, d) {
		if (d->flags & D_CSET) csets++;
		adjust_serials(s, d, 1);
	}
	EACHP_REVERSE(s->symlist, sym) {
		if (sym->ser) sym->ser++;
		if (sym->meta_ser) sym->meta_ser++;
	}
	sccs_findKeyFlush(s);

	s->nextserial++;
	d = insertArrayN(&s->slist, 1, 0);
	insertArrayN(&s->extra, 1, 0);
	s->table = s->slist + nLines(s->slist);
	d->flags |= D_INARRAY;

	t = SFIND(s, 2);
	if (t->flags & D_XFLAGS) {
		/* move 1.1 xflags to new 1.0 delta */
		d->xflags = t->xflags;
		d->flags |= D_XFLAGS;
		t->flags &= ~D_XFLAGS;
	}
	s->tree = d;		/* tree is now linked */
	revArg(s, d, "1.0");
	d->userhost = t->userhost;
	d->pathname = t->pathname;
	if (t->zone) {
		d->zone = t->zone;
	} else {
		zoneArg(s, d, "-00:00");
	}
	t->pserial = 1;	/* nop as t->pserial was 0 and inc'd */

	/* date needs to be 1 second earler than 1.1 */
	d->date = t->date - 1;

	if (csets) {
		d->sum = t->sum;
		if (t->random) {
			d->random = t->random;
			t->random = 0;
		} else {
			len = sccs_sdelta(s, t, key);
			p = short_random(key, len);
			d->random = sccs_addStr(s, p);
			free(p);
		}
		/*
		 * rmshortkeys sets BK_HOST -- use it if needed
		 */
		unless (strchr(USERHOST(s, d), '@')) {
			hostArg(s, d, sccs_gethost());
		}
	} else {
		unless (d->random) {
			char	buf[20];

			buf[0] = 0;
			randomBits(buf);
			if (buf[0]) d->random = sccs_addStr(s, buf);
		}
		d->sum = almostUnique();
	}
	d = sccs_dInit(d, 'D', s, 0);
	return (0);
}

private int
remove_1_0(sccs *s)
{
	delta	*d;
	symbol	*sym;

	unless (streq(REV(s, s->tree), "1.0")) return (0);

	for (d = SFIND(s, 2); d <= s->table; d += 1) {
		adjust_serials(s, d, -1);
	}
	EACHP_REVERSE(s->symlist, sym) {
		if (sym->ser) --sym->ser;
		if (sym->meta_ser) --sym->meta_ser;
	}
	sccs_findKeyFlush(s);
	removeArrayN(s->slist, 1);
	freeExtra(s, SFIND(s, 1));
	removeArrayN(s->extra, 1);
	s->tree = SFIND(s, 1);
	memset(s->table, 0, sizeof(delta));
	s->table -= 1;
	s->nextserial--;
	return (1);
}

int
sccs_newchksum(sccs *s)
{
	return (sccs_adminFlag(s, NEWCKSUM));
}

/*
 * Reverse sort it, we want the ^A's, if any, at the end.
 */
private int
c_compar(const void *a, const void *b)
{
	return (*(char*)b - *(char*)a);
}

private	char *
obscure(int rmlicense, int uu, char *buf)
{
	int	len;
	char	*new;
	char	*p, *t;

	/*
	 * line either terminates with a '\n' (mmap of sfile)
	 * or '\0' if a d->comments[i] (which are chomped)
	 * Len could wind up 0.
	 */
	for (len = 0; buf[len] && (buf[len] != '\n'); len++);
	if (buf[len]) len++;	/* if newline, include it */
	new = strndup(buf, len);
	unless (len > 1) goto done; 	/* need to have something to obscure */

	if (*new == '\001') goto done;
	/*
	 * Try to match '.*\Wlicense\w*:.* or .*\Wlicsign\d\w*:
	 * and obsure them.
	 */
	if (rmlicense) {
		if (uu) goto done;
		/* only pick on active license and licsign lines */
		p = new;
		while (isspace(*p)) ++p;
		if (*p == '#') goto done;
		if ((*p == '[') && (p = strchr(p, ']'))) p++;
		unless (p && *p) goto done;
		while (isspace(*p)) ++p;
		unless ((strneq(p, "license", 7)) ||
		    (strneq(p, "licsign", 7))) {
			goto done;
		}
		t = p + 7;
		if ((t[-1] == 'n') && isdigit(*t)) ++t;	/* licsign\d */
		while (isspace(*t)) ++t;
		unless (*t == ':') goto done;
		*t = *new;		/* save first character in : */
		p[2] += ':' - '#';	/* turn a c into z */
		new[0] = '#';		/* replace : with # */
		uu = 1;			/* leave the first char in place */
	}
	if (uu) {
		qsort(new+1, len-2, 1, c_compar); /* leave first and last */
	} else {
		qsort(new, len-1, 1, c_compar);	/* leave last char */
	}
	assert(*new != '\001');
done:	return (new);
}

private	void
obscure_comments(sccs *s)
{
	delta	*d;
	char	*p, *buf;
	char	**comments = 0;

	for (d = s->table; d; d = NEXT(d)) {
		unless (buf = COMMENTS(s, d)) continue;
		while (p = eachline(&buf, 0)) {
			comments = addLine(comments, obscure(0, 0, p));
		}
		comments_set(s, d, comments);
		freeLines(comments, free);
		comments = 0;
	}
}

/* call sccs_admin() with only flags */
int
sccs_adminFlag(sccs *sc, u32 flags)
{
	return (sccs_admin(sc, 0, flags, 0, 0, 0, 0, 0, 0));
}

/*
 * admin the specified file.
 *
 * Note: flag values are optional.
 *
 * XXX - this could do the insert hack for paths/users/whatever.
 * For large files, this is a win.
 */
int
sccs_admin(sccs *sc, delta *p, u32 flags,
	admin *f, admin *z, admin *u, admin *s, char *mode, char *text)
{
	FILE	*sfile = 0;
	int	error = 0, locked = 0, i;
	int	rc;
	int	flagsChanged = 0;
	char	*buf;
	char	**comments = 0;
	delta	*d = 0;
	int	obscure_it, rmlicense;

	assert(!z); /* XXX used to be LOD item */

	GOODSCCS(sc);
	unless (flags & (ADMIN_BK|ADMIN_FORMAT|ADMIN_GONE)) {
		char	z = (flags & ADMIN_FORCE) ? 'Z' : 'z';

		unless (locked = sccs_lock(sc, z)) {
			verbose((stderr,
			    "admin: can't get lock on %s\n", sc->sfile));
			error = -1; sc->state |= S_WARNED;
out:
			if (sfile) sccs_abortWrite(sc, &sfile);
			if (locked) sccs_unlock(sc, 'z');
			debug((stderr, "admin returns %d\n", error));
			return (error);
		}
	}
#define	OUT	{ error = -1; sc->state |= S_WARNED; goto out; }
#define	ALLOC_D()	\
	unless (d) { \
		unless (d = newDelta(sc, p, 1)) OUT; \
		p = SFIND(sc, d->pserial); \
		if (BITKEEPER(sc)) updatePending(sc); \
	}

	unless (HAS_SFILE(sc)) {
		verbose((stderr, "admin: no SCCS file: %s\n", sc->sfile));
		OUT;
	}

	if ((flags & ADMIN_BK) && checkInvariants(sc, flags)) OUT;
	if ((flags & ADMIN_GONE) && checkGone(sc, D_GONE, "admin")) OUT;
	if (flags & ADMIN_FORMAT) {
		if (checkrevs(sc, flags) ||
		    checkdups(sc) || checkMisc(sc, flags)) {
			OUT;
		}
#if 0
		/*
		 * Until such time as we decide to rewrite all the serial
		 * numbers when running stripdel, we can't do this.
		 */
		if (sc->nextserial != (sc->numdeltas + 1)) {
			verbose((stderr,
			    "admin: gaps in serials in %s (somewhat unusual)\n",
			    sc->sfile));
		}
#endif
	}
	if (flags & (ADMIN_BK|ADMIN_FORMAT)) goto out;
	if ((flags & (ADMIN_OBSCURE|ADMIN_RMLICENSE))
	    && sccs_clean(sc, (flags & SILENT))) {
		goto out;
	}

	if (addSym("admin", sc, flags, s, &error)) flags |= NEWCKSUM;
	if (mode) {
		delta	*n = sccs_top(sc);
		mode_t	m;

		assert(n);
		if ((n->flags & D_MODE) && n->symlink) {
			fprintf(stderr,
				"admin: %s: chmod on symlink is illegal\n",
				sc->gfile);
			OUT;
		} 
		unless (m = newMode(n, mode)) {
			fprintf(stderr, "admin: %s: Illegal file mode: %s\n",
			    sc->gfile, mode);
			OUT;
		}
		/* No null deltas for nothing */
		if (m == n->mode) goto skipmode;
		if (S_ISLNK(m) || S_ISDIR(m)) {
			fprintf(stderr, "admin: %s: Cannot change mode to/of "
			    "%s\n", sc->gfile,
			    S_ISLNK(m) ? "symlink" : "directory");
			OUT;
		}
		ALLOC_D();
		addMode(sc, d, m, &comments);
		if (HAS_GFILE(sc) && HAS_PFILE(sc)) {
			if (chmod(sc->gfile, m)) perror(sc->gfile);
		} else if (HAS_GFILE(sc)) {
			if (chmod(sc->gfile, m & ~0222)) perror(sc->gfile);
		}
		flags |= NEWCKSUM;
	}
skipmode:

	if (text) {
		FILE	*desc;
		char	*dbuf;

		unless (text[0]) {
			if (sc->text) {
				freeLines(sc->text, free);
				sc->text = 0;
				flags |= NEWCKSUM;
			}
			goto user;
		}
		desc = fopen(text, "rt"); /* must be text mode */
		unless (desc) {
			fprintf(stderr, "admin: can't open %s\n", text);
			error = 1; sc->state |= S_WARNED;
			goto user;
		}
		if (sc->text) {
			freeLines(sc->text, free);
			sc->text = 0;
		}
		while (dbuf = fgetline(desc)) {
			sc->text = addLine(sc->text, strdup(dbuf));
		}
		fclose(desc);
		flags |= NEWCKSUM;
	}

user:	for (i = 0; u && u[i].flags; ++i) {
		if (BITKEEPER(sc)) {
			fprintf(stderr,
			    "admin: changing user/group is not supported\n");
			OUT;
		}
		flags |= NEWCKSUM;
		if (u[i].flags & A_ADD) {
			sc->usersgroups =
			    addLine(sc->usersgroups, strdup(u[i].thing));
		} else {
			unless (removeLine(sc->usersgroups, u[i].thing, free)) {
				verbose((stderr,
				    "admin: user/group %s not found in %s\n",
				    u[i].thing, sc->sfile));
				error = 1; sc->state |= S_WARNED;
			}
		}
	}

	/*
	 * flags, unknown single letter passed through.
	 */
	for (i = 0; f && f[i].flags; ++i) {
		int	add = f[i].flags & A_ADD;
		char	*v = &f[i].thing[1];

		if (isupper(f[i].thing[0])) {
			char *fl = f[i].thing;

			v = strchr(v, '=');
			if (v) *v++ = '\0';
			if (v && *v == '\0') v = 0;

			if ((a2xflag(fl) & X_MONOTONIC) &&
			    DANGLING(sccs_top(sc))) {
			    	fprintf(stderr, "admin: "
				    "must remove danglers first (monotonic)\n");
				error = 1;
				sc->state |= S_WARNED;
				continue;
			}
			if (a2xflag(fl) & X_MAYCHANGE) {
				if (v) goto noval;
				ALLOC_D();
				flagsChanged +=
				    changeXFlag(sc, d, flags, add, fl,
					&comments);
			} else if (streq(fl, "DEFAULT")) {
				if (BITKEEPER(sc) && add && v) {
					fprintf(stderr,
					   "Setting DEFAULT is unsupported.\n");
					error = 1;
					sc->state |= S_WARNED;
				} else {
					free(sc->defbranch);
					sc->defbranch =
					    (add && v) ? strdup(v) : 0;
					flagsChanged++;
				}
			} else {
				if (v) {
					fprintf(stderr,
					  "admin: unknown flag %s=%s\n", fl, v);
				} else {
					fprintf(stderr,
					     "admin: unknown flag %s\n", fl);
				}
				error = 1;
				sc->state |= S_WARNED;
			}
			continue;

		noval:	fprintf(stderr,
			    "admin: flag %s can't have a value\n", fl);
			error = 1;
			sc->state |= S_WARNED;
		} else {
			char	*buf;

			switch (f[i].thing[0]) {
			    case 'd':
				if (BITKEEPER(sc) && (add && *v)) {
					fprintf(stderr,
					   "Setting d flag is unsupported.\n");
					error = 1;
					sc->state |= S_WARNED;
				} else {
					if (sc->defbranch) free(sc->defbranch);
					sc->defbranch =
					    (add && *v) ? strdup(v) : 0;
					flagsChanged++;
				}
				break;
			    case 'e':
				if (BITKEEPER(sc)) {
					fprintf(stderr, "Unsupported.\n");
				}
		   		break;
			    case 'g':
				if (getenv("_BK_GTRANS_TEST")) {
					if (add) {
						unless (graph_v2(sc)) {
							flagsChanged++;
						}
					} else {
						unless (graph_v1(sc)) {
							flagsChanged++;
						}
					}
		   			break;
				}
				/* fall through if not testing */
			    default:
				/*
				 * Other flags are silently ignored in
				 * bk-mode. Note emacs vc-mode creates files
				 * with:
				 *    admin -fb -i<file>
				 */
				if (BITKEEPER(sc)) break;

				buf = aprintf("%c %s", v[-1], v);
				flagsChanged++;
				if (add) {
					sc->flags =
						addLine(sc->flags, strdup(buf));
				} else {
					unless (removeLine(sc->flags,
					    buf, free)) {
						verbose((stderr,
					"admin: flag %s not found in %s\n",
							 buf, sc->sfile));
						error = 1;
						sc->state |= S_WARNED;
					}
				}
				free(buf);
				break;
			}
		}
	}
	if (flagsChanged) flags |= NEWCKSUM;

	if (flags & ADMIN_ADD1_0) {
		if ((rc = insert_1_0(sc, flags)) == 1) goto out;
		if (rc == 2) flags &= ~ADMIN_ADD1_0;
		flags |= NEWCKSUM;
	} else if (flags & ADMIN_RM1_0) {
		if (remove_1_0(sc)) {
			flags |= NEWCKSUM;
		} else {
			flags &= ~ADMIN_RM1_0;
		}
	}

	if (flags & ADMIN_NEWPATH) {
		ALLOC_D(); /* We pick up the new path when we init the delta */
		assert(d->pathname);
		buf = aprintf("Rename: %s -> %s",
		    PATHNAME(sc, PARENT(sc, d)), PATHNAME(sc, d));
		comments = addLine(comments, buf);
		flags |= NEWCKSUM;
	}

	if (flags & ADMIN_DELETE) {
		ALLOC_D(); /* We pick up the new path when we init the delta */
		assert(d->pathname);
		buf = aprintf("Delete: %s", PATHNAME(sc, PARENT(sc, d)));
		comments = addLine(comments, buf);
		flags |= NEWCKSUM;
	}
	if (comments) {
		comments_set(sc, d, comments);
		freeLines(comments, free);
	}
	unless (flags & NEWCKSUM) goto out;

	if (flags & ADMIN_OBSCURE) obscure_comments(sc);

	/*
	 * Do the delta table & misc.
	 */
	unless (locked || (locked = sccs_lock(sc, 'z'))) {
		verbose((stderr, "admin: can't get lock on %s\n", sc->sfile));
		OUT;
	}
	unless (sfile = sccs_startWrite(sc)) {
		fprintf(stderr, "admin: can't create %s: ", sccsXfile(sc, 'x'));
		OUT;
	}
	if (delta_table(sc, sfile, 0)) {
		if (sc->io_warned) OUT;
		goto out;	/* we don't know why so let sccs_why do it */
	}
	assert(sc->state & S_SOPEN);
	debug((stderr, "seek to %d\n", (int)sc->data));
	obscure_it = (flags & ADMIN_OBSCURE);
	rmlicense = (flags & ADMIN_RMLICENSE);
	/* ChangeSet can't be obscured in any sense */
	if (CSET(sc)) {
	    	rmlicense = obscure_it = 0;
	}
	/* the BitKeeper/etc files can't be obscured in normal sense */
	if (sc->tree->pathname
	    && strneq(PATHNAME(sc, sc->tree), "BitKeeper/etc/",13)) {
	    	obscure_it = 0;
	}
	if (rmlicense) obscure_it = 1;
	if (sc->encoding_out & E_GZIP) sccs_zputs_init(sc, sfile);
	sccs_rdweaveInit(sc);
	while (buf = sccs_nextdata(sc)) {
		if (obscure_it) {
			buf = obscure(rmlicense, UUENCODE(sc), buf);
		}
		if (flags & ADMIN_ADD1_0) {
			fputbumpserial(sc, buf, 1, sfile);
		} else if (flags & ADMIN_RM1_0) {
			if (streq(buf, "\001I 1")) {
				buf = sccs_nextdata(sc);
				assert(streq(buf, "\001E 1"));
				assert(!sccs_nextdata(sc));
				break;
			}
			fputbumpserial(sc, buf, -1, sfile);
		} else {
			fputdata(sc, buf, sfile);
		}
		fputdata(sc, "\n", sfile);
		if (obscure_it) free(buf);
	}
	sccs_rdweaveDone(sc);
	if (flags & ADMIN_ADD1_0) {
		fputdata(sc, "\001I 1\n", sfile);
		fputdata(sc, "\001E 1\n", sfile);
	}

	/* not really needed, we already wrote it */
	if (GZIP_OUT(sc)) sccs_zputs_done(sc);
#ifdef	DEBUG
	badcksum(sc, flags);
#endif
	if (sccs_finishWrite(sc, &sfile)) OUT;
	goto out;
#undef	OUT
}

/*
 * Remve any gaps in the serial numbers.
 * Should be called after a stripdel.
 */
int
sccs_scompress(sccs *s, int flags)
{
	FILE	*sfile = 0, *f = 0;
	int	ser, sign, error = 0, locked = 0, i, j;
	char	*buf, *p;
	delta	*d, *e;
	ser_t	*remap;
	symbol	*sym;

	unless (locked = sccs_lock(s, 'z')) {
		fprintf(stderr, "scompress: can't get lock on %s\n", s->sfile);
		error = -1; s->state |= S_WARNED;
out:
		if (sfile) sccs_abortWrite(s, &sfile);
		if (locked) sccs_unlock(s, 'z');
		debug((stderr, "scompress returns %d\n", error));
		return (error);
	}
#define	OUT	{ error = -1; s->state |= S_WARNED; goto out; }

	remap = calloc(sizeof(ser_t), s->nextserial);

	f = fmem();
	ser = 0;
	for (j = 1; j < s->nextserial; j++) {
		unless (e = sfind(s, j)) continue;
		ser++;
		if (ser != j) {
			unless (flags & SILENT) {
				fprintf(stderr, "Remap %s:%d ->%d\n",
				    s->gfile, j, ser);
			}
			d = SFIND(s, ser);
			assert(d != e);
			memcpy(d, e, sizeof(delta));
			freeExtra(s, d);
			memcpy(EXTRA(s, d), EXTRA(s, e), sizeof(dextra));
			e->flags = 0;
		} else {
			d = e;
		}
		if (NEXT(d)) assert(d == (NEXT(d) + 1));
		remap[j] = ser;

		d->pserial = remap[d->pserial];
		if (d->ptag) d->ptag = remap[d->ptag];
		if (d->mtag) d->mtag = remap[d->mtag];
		if (d->merge) d->merge = remap[d->merge];

		if (d->cludes) {
			p = CLUDES(s, d);
			while (i = sccs_eachNum(&p, &sign)) {
				sccs_saveNum(f, remap[i], sign);
			}
			p = fmem_peek(f, 0);
			unless (streq(p, CLUDES(s, d))) {
				d->cludes = sccs_addStr(s, p);
			}
			ftrunc(f, 0);
		}
	}
	fclose(f);
	EACHP_REVERSE(s->symlist, sym) {
		if (sym->ser) sym->ser = remap[sym->ser];
		if (sym->meta_ser) sym->meta_ser = remap[sym->meta_ser];
	}
	/* clear old deltas */
	truncArray(s->slist, ser);
	EACH_REVERSE(s->extra) {
		if (i <= ser) break;
		freeExtra(s, SFIND(s, i));
	}
	truncArray(s->extra, ser);
	s->nextserial = nLines(s->slist)+1;
	s->table = s->slist + nLines(s->slist);
	sccs_findKeyFlush(s);

	unless (sfile = sccs_startWrite(s)) OUT;
	if (delta_table(s, sfile, 0)) {
		if (s->io_warned) OUT;
		goto out;	/* we don't know why so let sccs_why do it */
	}
	assert(s->state & S_SOPEN);
	debug((stderr, "seek to %d\n", (int)s->data));
	sccs_rdweaveInit(s);
	if (GZIP_OUT(s)) sccs_zputs_init(s, sfile);
	while (buf = sccs_nextdata(s)) {
		if (isData(buf)) {
			fputdata(s, buf, sfile);
		} else {
			ser = atoi(&buf[3]);
			fputbumpserial(s, buf, remap[ser] - ser, sfile);
		}
		fputdata(s, "\n", sfile);
	}
	if (GZIP_OUT(s)) sccs_zputs_done(s);
	sccs_rdweaveDone(s);
	if (sccs_finishWrite(s, &sfile)) OUT;
	goto out;
#undef	OUT
}

#define	MAXCMD	20
struct weave {
	sccs	*s;		// backpointer
	char	*buf;		// current data line
	ser_t	*wmap;		// weave map - renumber serials in weave
	ser_t	*state;		// weave state
	u8	*slist;		// weave set
	FILE	*out;		// print here
	sum_t	sum;		// checksum
	int	print;		// active serial in weave
	int	line;		// line number in weave
};

int
sccs_fastWeave(sccs *s, ser_t *weavemap, char **patchmap,
    MMAP *fastpatch, FILE *out)
{
	int	i;
	delta	*d;
	weave	*w = 0;
	int	rc = 0;

	assert(s);

	w = new(weave);
	w->s = s;
	w->out = out;
	w->wmap = weavemap;

	/* compute an serialmap view which matches sccs_patchDiffs() */
	w->slist = calloc(s->nextserial, sizeof(u8));
	EACH(patchmap) {
		if ((d = (delta *)patchmap[i]) == INVALID) continue;
		w->slist[SERIAL(s, d)] = 1;
	}
	unless (CSET(s)) {
		/* transitive close if not the cset file */
		for (d = s->table; d; d = NEXT(d)) {
			unless (w->slist[SERIAL(s, d)]) continue;
			if (d->pserial) {
				w->slist[d->pserial] = 1;
			}
			if (d->merge) {
				w->slist[d->merge] = 1;
			}
		}
	}
	if (HAS_SFILE(s)) {
		sccs_rdweaveInit(s);
		w->buf = sccs_nextdata(s);	/* prime the data flow */
	}
	if (GZIP_OUT(s)) sccs_zputs_init(s, out);

	rc = doFast(w, patchmap, fastpatch);

	if (HAS_SFILE(s)) {
		if (sccs_rdweaveDone(s)) rc = 1;	/* no EOF */
	}
	if (GZIP_OUT(s)) sccs_zputs_done(s);
	if (w->slist) free(w->slist);
	if (w->state) free(w->state);
	free(w);
	return (rc);
}

private int
doFast(weave *w, char **patchmap, MMAP *diffs)
{
	int	lineno, lcount = 0, serial, pmapsize;
	int	gotK = 0;
	int	inpatch = 0;
	int	ignore = 0;
	char	*p, *b;
	char	type;
	delta	*d;
	int	rc = 1;
	sum_t	sum = 0;
	ser_t	dser;
	char	cmdline[MAXCMD];

	unless (diffs) goto done;
	assert(patchmap);	/* if diffs, then there's a map */
	pmapsize = nLines(patchmap);

	while (b = mnext(diffs)) {
		p = &b[1];
		if (*b == 'F') continue;
		if (*b == 'K') {
			gotK = 1;
			sum = atoi_p(&p);
			p++;
			lcount = atoi_p(&p);
			break;
		}
		if (*b == '>') {
			if (ignore) {
				while (*p) {
					w->sum += *(u8 *)p;
					if (*p++ == '\n') break;
				}
				w->line++;
			} else if (inpatch) {
				fix_cntl_a(w->s, p, w->out);
				w->sum += fputdata(w->s, p, w->out);
				w->line++;
			}
			continue;
		}
		type = (*b == 'N') ? 'E' : *b;
		serial = atoi_p(&p);
		assert((serial > 0) && (serial <= pmapsize));
		d = (delta *)patchmap[serial];
		if (d == INVALID) {
			unless (ignore) {
				if (type == 'I') ignore = serial;
			} else if (ignore == serial) {
				assert(type == 'E');
				ignore = 0;
			}
			continue;
		}
		assert(!ignore);
		assert(d);
		unless (d->flags & D_REMOTE) continue;
		dser = SERIAL(w->s, d);
		unless (inpatch) {
			assert(*p == ' ');
			p++;
			lineno = atoi_p(&p);
			if (weaveMove(w, lineno, (type == 'D'), dser)) {
				goto err;
			}
			if (type == 'I') inpatch = dser;
		} else if (inpatch == dser) {
			assert(type == 'E');
			inpatch = 0;
		}
		if (*b == 'N') {
			sprintf(cmdline, "\001E %uN\n", dser);
		} else {
			sprintf(cmdline, "\001%c %u\n", type, dser);
		}
		fputdata(w->s, cmdline, w->out);
		w->state = changestate(w->state, type, dser);
		if (w->print = whatstate(w->state)) {
			unless (w->slist[w->print]) w->print = 0;
		}
	}
	assert(!inpatch && !ignore);
done:
	if (weaveMove(w, -1, 0, 0)) goto err;
	if (gotK && ((w->sum != sum) || (lcount != w->line))) {
		fprintf(stderr,
		    "computed sum %u and patch sum %u\n", w->sum, sum);
		fprintf(stderr,
		    "computed linecount %u and patch linecount %u\n",
		    w->line, lcount);
		goto err;
	}
	rc = 0;
err:
	return (rc);
}

/*
 * Move in the weave
 *
 * line		stop at which line in weave
 * before	stop before that line
 * patchserial	current serial being processed
 */
private int
weaveMove(weave *w, int line, int before, ser_t patchserial)
{
	sccs	*s = w->s;
	int	finish = (line < 0);
	int	skipblock;
	int	print;
	char	*n;
	char	type;
	ser_t	serial;
	char	*buf;
	char	cmdline[MAXCMD];

	if (before) {
		/* first move after the previous line, then upto this line */
		if (weaveMove(w, line - 1, 0, patchserial)) return (1);
	}

	unless (buf = w->buf) {	/* note: want assignment, not == */
		if (before ||
		    (!finish && (line != w->line)) || whatstate(w->state)) {
			goto eof;
		}
		return (0);
	}
	print = w->print;
	do {
		unless (finish || (w->line < line)) goto after;
		if (isData(buf)) {
			if (print) {
				if (before) goto end;
				w->line++;
				w->sum += fputdata(s, buf, w->out);
				w->sum += fputdata(s, "\n", w->out);
				if (buf[0] == CNTLA_ESCAPE) {
					w->sum -= CNTLA_ESCAPE;
				}
			} else {
				fputdata(s, buf, w->out);
				fputdata(s, "\n", w->out);
			}
			continue;
		}
		type = buf[1];
		n = &buf[3];
		serial = atoi_p(&n);
		assert(serial);
		if (w->wmap && (serial > w->wmap[0])) {
			serial = w->wmap[serial - w->wmap[0]];
		}
		assert(patchserial != serial);
		if (before && print && (type == 'D')) {
			if (patchserial < serial) goto end;
		}
		sprintf(cmdline, "\001%c %u%s\n", type, serial, n);
		fputdata(s, cmdline, w->out);
		w->state = changestate(w->state, type, serial);
		if (print = whatstate(w->state)) {
			unless (w->slist[print]) print = 0;
		}
	} while (buf = sccs_nextdata(s));
	assert(!buf);
	unless (finish && !whatstate(w->state)) {
eof:		fprintf(stderr, "Unexpected EOF in %s\n", s->sfile);
		return (1);
	}
	goto end;

	/*
	 * We are positioned in the weave after the desired data line.
	 * If we are the newest delta in the weave, we are done.  However,
	 * since we could be an older delta, we need to skip over I-E
	 * blocks from newer deltas.  Stopping conditions in order of
	 * appearance in code (first 3 with goto end, and 4th with loop exit):
	 * 1. Any data line not found inside a skipped I-E block.
	 * 2. Any D not in skipped IE block, as we are into the region
	 * of the next data line.
	 * 3. I or E with a smaller serial.
	 * 4. EOF - (but constrained -- we are weaving serial 1).
	 */
after:	skipblock = 0;
	assert(!before);
	do {
		if (isData(buf)) {
			unless (skipblock) goto end;
			assert(!print);
			fputdata(s, buf, w->out);
			fputdata(s, "\n", w->out);
			continue;
		}
		type = buf[1];
		if (!skipblock && (type == 'D')) goto end;
		n = &buf[3];
		serial = atoi_p(&n);
		assert(serial);
		if (w->wmap && (serial > w->wmap[0])) {
			serial = w->wmap[serial - w->wmap[0]];
		}
		assert(patchserial != serial);
		if (serial < patchserial) goto end;
		sprintf(cmdline, "\001%c %u%s\n", type, serial, n);
		fputdata(s, cmdline, w->out);
		unless (skipblock) {
			if (type == 'I') skipblock = serial;
		} else if (skipblock == serial) {
			assert(type == 'E');
			skipblock = 0;
		}
		w->state = changestate(w->state, type, serial);
		if (print = whatstate(w->state)) {
			unless (w->slist[print]) print = 0;
		}
	} while (buf = sccs_nextdata(s));
	assert(!buf);
	/* assert(patchserial == 1); kind of strong, but should be true */
	if (whatstate(w->state)) goto eof;

end:	w->buf = buf;
	w->print = print;
	return (0);
}

private void
doctrl(sccs *s, char *pre, int val, char *post, FILE *out)
{
	char	tmp[10];

	sertoa(tmp, (ser_t) val);
	fputdata(s, pre, out);
	fputdata(s, tmp, out);
	fputdata(s, post, out);
	fputdata(s, "\n", out);
}

private void
finish(sccs *s, int *ip, int *pp, int *last, FILE *out, ser_t **state,
    u8 *slist)
{
	int	print = *pp, incr = *ip;
	sum_t	sum;
	register char	*buf;
	ser_t	serial;
	int	lf_pend = *last;

	debug((stderr, "finish(incr=%d, sum=%d, print=%d) ",
		incr, s->dsum, print));
	if (lf_pend) s->dsum -= '\n';
	while (!feof(s->fh)) {
		unless (buf = sccs_nextdata(s)) break;
		debug2((stderr, "G> %s", buf));
		sum = fputdata(s, buf, out);
		sum += fputdata(s, "\n", out);
		if (isData(buf)) {
			/* CNTLA_ESCAPE is not part of the check sum */
			if (buf[0] == CNTLA_ESCAPE) sum -= CNTLA_ESCAPE;

			if (!print) {
				/* if we are skipping data from pending block */
				if (lf_pend &&
				    lf_pend == whatstate(*state)) {
					s->dsum += '\n';
					lf_pend = 0;
				}
				continue;
			}
			*last = 0;
			unless (lf_pend) sum -= '\n';
			lf_pend = print;
			s->dsum += sum;
			incr++;
			continue;
		}
		serial = atoi(&buf[3]);
		if (buf[1] == 'E' && lf_pend == serial &&
		    whatstate(*state) == serial)
		{
			char	*n = &buf[3];
			while (isdigit(*n)) n++;
			if (*n != 'N') {
				lf_pend = 0;
				s->dsum += '\n';
			}
		}
		*state = changestate(*state, buf[1], serial);
		print = printstate(*state, slist);
	}
	unless (lf_pend) *last = 0;
	*ip = incr;
	*pp = print;
	debug((stderr, "incr=%d, sum=%d\n", incr, s->dsum));
}

#define	nextline(inc)	\
    nxtline(s, &inc, 0, &lines, &print, &last, out, &state, slist, &savenext)
#define	beforeline(inc) \
    nxtline(s, &inc, 1, &lines, &print, &last, out, &state, slist, &savenext)

private void
nxtline(sccs *s, int *ip, int before, int *lp, int *pp, int *last, FILE *out,
    ser_t **state, u8 *slist, char ***savenext)
{
	int	print = *pp, incr = *ip, lines = *lp;
	int	serial;
	char	*n;
	int	len;
	sum_t	sum;
	register char	*buf;
	char	peek[3];	/* 2 chars plus \0 */

	debug((stderr, "nxtline(@%d, before=%d print=%d, sum=%d) ",
	    lines, before, print, s->dsum));
	while (!feof(s->fh)) {
		if (before && print) { /* if move upto next printable line */
			/* peek - read up to 2 and put them back */
			len = fread(peek, 1, 2, s->fh);
			peek[len] = 0;
			while (len--) ungetc(peek[len], s->fh);
			if (isData(peek)) break;
		}
		unless (buf = sccs_nextdata(s)) break;
		debug2((stderr, "[%d] ", lines));
		debug2((stderr, "G> %s", buf));
		sum = fputdata(s, buf, out);
		sum += fputdata(s, "\n", out);
		if (isData(buf)) {
			if (print) {
				if (*savenext) {
					**savenext = strdup(buf);
					assert(**savenext);
					*savenext = 0;
				}
				/* CNTLA_ESCAPE is not part of the check sum */
				if (buf[0] == CNTLA_ESCAPE) sum -= CNTLA_ESCAPE;
				s->dsum += sum;
				incr++; lines++;
				break;
			}
			continue;
		}
		n = &buf[3];
		serial = atoi_p(&n);
		if ((buf[1] == 'E') && (*last == serial) &&
		    (whatstate(*state) == serial) &&
		    (*n != 'N')) {
			*last = 0;
		}
		*state = changestate(*state, buf[1], serial);
		print = printstate(*state, slist);
	}
	*ip = incr;
	*lp = lines;
	*pp = print;
	debug((stderr, "sum=%d\n", s->dsum));
}

/*
 * Get the hash checksum.
 * A side effect of get_reg() is to set dsum.
 * We're getting what looks like the new delta but it is really
 * the basis for the new delta -
 * we are still operating on the old file, without the diffs applied.
 */
private int
getHashSum(sccs *sc, delta *n, MMAP *diffs)
{
	char	*buf;
	char	key[MAXPATH], val[MAXPATH];
	char	*v, *t;
	u8	*e;
	unsigned int sum = 0;
	int	lines = 0;
	int	flags = SILENT|GET_HASHONLY|GET_SUM|GET_SHUTUP;
	delta	*d;
	int	offset;

	assert(HASH(sc));
	assert(!BAM(sc));
	assert(diffs);
	/*
	 * If we have a hash already and it is a simple delta, then just
	 * use that.  Otherwise, regen from scratch.
	 */
	if (sc->mdbm && !n->cludes && (d = getCksumDelta(sc, n))) {
	    	sum = d->sum;
	} else {
		if (sc->mdbm) mdbm_close(sc->mdbm), sc->mdbm = 0;
		sccs_restart(sc);
		if (get_reg(sc, 0, flags, n, &lines, 0, 0)) {
			sccs_whynot("delta", sc);
			return (-1);
		}
		sum = sc->dsum;
	}
	mseekto(diffs, 0);
	unless (buf = mnext(diffs)) {
		/* there are no diffs, we have the checksum, it's OK */
		return (0);
	}
	offset = 0;
	if (strneq(buf, "0a0\n", 4)) {
		offset = 2;
	}
	else unless (strneq(buf, "I0 ", 3)) {
		fprintf(stderr, "Missing '0a0' or 'I0 #lines', ");
bad:		fprintf(stderr, "bad diffs: '%s'\n", buf);
		return (-1);
	}
	while (buf = mnext(diffs)) {
		unless (offset == 0 || buf[0] == '>') goto bad;
		t = key, v = &buf[offset];
		if (CSET(sc)) {
			int	pipes = 0;

			/*
			 * Keys are like u@h|path|date|.... whatever
			 * We want to skip over any spaces in the path part.
			 */
			while (v < diffs->end) {
				if (*v == '|') pipes++;
				if ((*v == ' ') && (pipes >= 2)) break;
				*t++ = *v++;
			}
		} else {
			while ((v < diffs->end) && (*v != ' ')) *t++ = *v++;
		}
		unless (*v == ' ') goto bad;
		*t = 0;
		for (t = val, v++; (v < diffs->end) && (*v != '\n'); ) {
			*t++ = *v++;
		}
		unless (*v == '\n') goto bad;
		*t = 0;
		if (v = mdbm_fetch_str(sc->mdbm, key)) {
			if (streq(v, val)) {
				fprintf(stderr,
				    "Redundant: %s %s\n", key, val);
				return (-1);
			} else {
				/*
				 * Subtract off the old value and add in new
				 */
				for (e = v; *e; sum -= *e++);
				for (e = val; *e; sum += *e++);
				mdbm_store_str(sc->mdbm, key, val,MDBM_REPLACE);
			}
		} else {
			/* completely new, add in the whole line */
			for (e = key; *e; sum += *e++); sum += ' ';
			for (e = val; *e; sum += *e++); sum += '\n';
			mdbm_store_str(sc->mdbm, key, val, MDBM_INSERT);
		}
	}
	sc->dsum = sum;
	return (0);
}

private int
delta_body(sccs *s, delta *n, MMAP *diffs, FILE *out, int *ap, int *dp, int *up)
{
	ser_t	*state;
	u8	*slist;
	int	print;
	int	lines;
	int	last;
	int	lastdel;
	int	fixdel = 0;
	char	*addthis;
	char	**savenext;
	long	offset;
	int	added, deleted, unchanged;
	sum_t	cksumsave;
	sum_t	sum;
	char	*b;
	int	no_lf;

	if (binaryCheck(diffs)) {
		assert(!BINARY(s));
		fprintf(stderr,
		    "%s: file format is ascii, delta is binary.", s->sfile);
		fprintf(stderr, "  Unsupported operation.\n");
		return (-1);
	}
	assert(!READ_ONLY(s));
	assert(s->state & S_ZFILE);
	offset = ftell(out);
	cksumsave = s->cksum;
again:
	*ap = *dp = *up = 0;
	state = 0;
	slist = 0;
	print = 0;
	lines = 0;
	last = 0;
	lastdel = 0;
	addthis = 0;
	savenext = 0;
	added = 0;
	deleted = 0;
	unchanged = 0;
	no_lf = 0;
	/*
	 * Do the actual delta.
	 */
	sccs_rdweaveInit(s);
	if (GZIP_OUT(s)) sccs_zputs_init(s, out);
	slist = serialmap(s, n, 0, 0, 0);	/* XXX - -gLIST */
	s->dsum = 0;
	assert(s->state & S_SOPEN);
	while (b = mnext(diffs)) {
		int	where;
		char	what;
		int	howmany;

newcmd:
		if (scandiff(b, &where, &what, &howmany) != 0) {
			int	len = linelen(b);

			fprintf(stderr,
			    "delta: can't figure out '%.*s'\n", len, b);
			if (state) free(state);
			if (slist) free(slist);
			return (-1);
		}
		debug2((stderr, "where=%d what=%c\n", where, what));

#define	ctrl(pre, val, post)	doctrl(s, pre, val, post, out)

		if (what == 'c' || what == 'd' || what == 'D' || what == 'x')
		{
			where--;
			lastdel = where;
			if (where && (fixdel == where)) {
				where--;
				howmany++;
				while (lines < where) {
					nextline(unchanged);
				}
				savenext = &addthis;
			}
		}
		while (lines < where) {
			/*
			 * XXX - this loops when I don't use the fudge as part
			 * of the ID in make/takepatch of SCCSFILE.
			 */
			nextline(unchanged);
		}
		last = print;
		switch (what) {
		    case 'c':
		    case 'd':
			beforeline(unchanged);
			ctrl("\001D ", SERIAL(s, n), "");
			sum = s->dsum;
			/* howmany != 0 only for nonewline corner fixer */
			while (howmany--) nextline(deleted);
			while (b = mnext(diffs)) {
				if (strneq(b, "---\n", 4)) break;
				if (strneq(b, "\\ No", 4)) continue;
				if (isdigit(b[0])) {
					ctrl("\001E ", SERIAL(s, n), "");
					s->dsum = sum;
					goto newcmd;
				}
				nextline(deleted);
				if (last == print) last = 0;
			}
			s->dsum = sum;
			if (what != 'c') break;
			ctrl("\001E ", SERIAL(s, n), "");
			/* fall through to */
		    case 'a':
			last = 0;
			ctrl("\001I ", SERIAL(s, n), "");
			while (b = mnext(diffs)) {
				if (strneq(b, "\\ No", 4)) {
					s->dsum -= '\n';
					no_lf = 1;
					break;
				}
				if (isdigit(b[0])) {
					ctrl("\001E ", SERIAL(s, n), "");
					goto newcmd;
				}
				fix_cntl_a(s, &b[2], out);
				s->dsum += fputdata(s, &b[2], out);
				debug2((stderr,
				    "INS %.*s", linelen(&b[2]), &b[2]));
				added++;
			}
			break;
		    case 'N':
		    case 'I':
		    case 'i':
			last = 0;
			ctrl("\001I ", SERIAL(s, n), "");
			while (howmany--) {
				/* XXX: not break but error */
				unless (b = mnext(diffs)) break;
				if (what != 'i' && b[0] == '\\') {
					fix_cntl_a(s, &b[1], out);
					s->dsum += fputdata(s, &b[1], out);
				} else {
					fix_cntl_a(s, b, out);
					s->dsum += fputdata(s, b, out);
				}
				debug2((stderr, "INS %.*s", linelen(b), b));
				added++;
			}
			if (what == 'N') {
				s->dsum -= '\n';
				no_lf = 1;
			}
			break;
		    case 'D':
		    case 'x':
			beforeline(unchanged);
			ctrl("\001D ", SERIAL(s, n), "");
			sum = s->dsum;
			while (howmany--) {
				nextline(deleted);
				if (last == print) last = 0;
			}
			s->dsum = sum;
			break;
		}
		ctrl("\001E ", SERIAL(s, n), no_lf ? "N" : "");
	}
	if (addthis) {
		last = 0;
		ctrl("\001I ", SERIAL(s, n), "");
		s->dsum += fputdata(s, addthis, out);
		s->dsum += fputdata(s, "\n", out);
		if (addthis[0] == CNTLA_ESCAPE) s->dsum -= CNTLA_ESCAPE;
		ctrl("\001E ", SERIAL(s, n), "");
		free(addthis);
	}
	finish(s, &unchanged, &print, &last, out, &state, slist);
	*ap = added;
	*dp = deleted;
	*up = unchanged;
	if (state) free(state);
	if (slist) free(slist);
	sccs_rdweaveDone(s);
	if (GZIP_OUT(s)) sccs_zputs_done(s);
	if (last) {
		off_t	end = offset;

		assert(!fixdel);	/* no infinite loops */
		fixdel = lastdel;
		if (fseek(out, offset, SEEK_SET)) {
			perror("fseek");
			return (-1);
		}
		/* In case gzip file gets smaller */
		ftrunc(out, end);
		mseekto(diffs, 0);
		s->cksum = cksumsave;
		goto again;
	}
	if (HASH(s) && (getHashSum(s, n, diffs) != 0)) {
		return (-1);
	}

	/*
	 * For ChangeSet file, force d->same to one
	 * because we do not maintain this field in the cweave code
	 */
	if (CSET(s)) *up = 1;

	return (0);
}

/*
 * Dump a cset weave file out: format is from cset_mkList() ...
 * <serial> tab <rootkey> space <deltakey>
 */

int
sccs_csetWrite(sccs *s, char **cweave)
{
	int	i, ret = -1;
	char	*keys;
	FILE	*out = 0;
	char	*ser, *oldser = 0;

	unless (sccs_lock(s, 'z')) {
		fprintf(stderr, "can't zlock %s\n", s->gfile);
		repository_lockers(s->proj);
		return (-1);
	}
	unless (out = sccs_startWrite(s)) goto err;
	if (delta_table(s, out, 0)) goto err;

	if (GZIP_OUT(s)) sccs_zputs_init(s, out);
	EACH(cweave) {
		unless (cweave[i][0]) continue;	/* skip deleted entries */
		ser = cweave[i];
		keys = strchr(ser, '\t');
		*keys++ = 0;
		unless (oldser && streq(ser, oldser)) {
			if (oldser) {
				fputdata(s, "\001E ", out);
				fputdata(s, oldser, out);
				fputdata(s, "\n", out);
			}
			oldser = ser;
			fputdata(s, "\001I ", out);
			fputdata(s, ser, out);
			fputdata(s, "\n", out);
		}
		fputdata(s, keys, out);
		fputdata(s, "\n", out);
	}
	if (oldser) {
		fputdata(s, "\001E ", out);
		fputdata(s, oldser, out);
		fputdata(s, "\n", out);
	}
	fputdata(s, "\001I 1\n", out);
	fputdata(s, "\001E 1\n", out);
	if (GZIP_OUT(s)) sccs_zputs_done(s);
	if (sccs_finishWrite(s, &out)) goto err;
	ret = 0;
err:
	sccs_abortWrite(s, &out);
	sccs_unlock(s, 'z');

	return (ret);
}

/*
 * Patch format is a little different, it looks like
 * 0a0
 * > key
 * > key
 * 
 * We need to strip out all the non-key stuff: 
 * a) "0a0"
 * b) "> "
 *
 * We want the output to look like:
 * ^AI serial
 * key
 * key
 * ^AE
 */
private void
patchweave(sccs *s, u8 *p, u32 len, u8 *buf, FILE *f)
{
	u8	*stop;

	fputdata(s, "\001I ", f);
	fputdata(s, buf, f);
	fputdata(s, "\n", f);
	if (p) {
		stop = p + len;
		assert(strneq(p, "0a0\n> ", 6));
		p += 6; /* skip "0a0\n" */
		while (1) {
			fputdata(s, p, f);
			while (*p++ != '\n') { /* null body */ }
			if (p == stop) break;
			assert(strneq("> ", p, 2));
			p += 2; /* skip "> " */
		}
	}
	fputdata(s, "\001E ", f);
	fputdata(s, buf, f);
	fputdata(s, "\n", f);
}

/*
 * sccs_csetPatchWeave()
 * This is similar to delta body, and makes use of many of the I/O
 * functions, which is why it is here.  The purpose is to weave in
 * patch items to the weave as it is copied to the output file.
 * This is useful for fast-takepatch operation for cset file.
 */

int
sccs_csetPatchWeave(sccs *s, FILE *f)
{
	u8	buf[20];
	u8	*line;
	u32	i;
	u32	ser;
	loc	*lp;

	assert(s);
	assert(s->state & S_CSET);
	assert(s->locs);
	lp = s->locs;
	i = s->iloc - 1; /* set index to final element in array */
	assert(i > 0); /* base 1 data structure */
	if (GZIP_OUT(s)) sccs_zputs_init(s, f);
	unless (HAS_SFILE(s)) goto skip;

	sccs_rdweaveInit(s);
	while (line = sccs_nextdata(s)) {
		assert(strneq(line, "\001I ", 3));
		ser = atoi(&line[3]);

		for ( ; i ; i--) {
			if (ser + i > lp[i].serial) break;
			unless (lp[i].p || lp[i].serial == 1) continue;
			sertoa(buf, lp[i].serial);
			patchweave(s, lp[i].p, lp[i].len, buf, f);
		}
		unless (i) break;

		/* bump the serial number up by # of items we have left */
		ser += i;
		sertoa(buf, ser);
		fputdata(s, "\001I ", f);
		fputdata(s, buf, f);
		fputdata(s, "\n", f);
		while (line = sccs_nextdata(s)) {
			if (*line == '\001') break;
			fputdata(s, line, f);
			fputdata(s, "\n", f);
		}
		assert(strneq(line, "\001E ", 3));
		fputdata(s, "\001E ", f);
		fputdata(s, buf, f);
		fputdata(s, "\n", f);
	}
	assert(!(i && line));
	/* No translation of serial numbers needed for remainder of file */
	for ( ; line; line = sccs_nextdata(s)) {
		fputdata(s, line, f);
		fputdata(s, "\n", f);
	}
	sccs_rdweaveDone(s);
	/* Print out remaining, forcing serial 1 block at the end */
skip:	for ( ; i ; i--) {
		unless (lp[i].p || lp[i].serial == 1) continue;
		sertoa(buf, lp[i].serial);
		patchweave(s, lp[i].p, lp[i].len, buf, f);
	}
	if (GZIP_OUT(s)) sccs_zputs_done(s);
	return (0);
}

int
sccs_hashcount(sccs *s)
{
	int	n;
	kvpair	kv;

	unless (HASH(s)) return (0);
	if (sccs_get(s, "+", 0, 0, 0, SILENT|GET_HASHONLY, 0)) {
		sccs_whynot("get", s);
		return (0);
	}
	/* count the number of long keys in the *values* since those
	 * are far more likely to be current.
	 */
	for (n = 0, kv = mdbm_first(s->mdbm);
	    kv.key.dsize; kv = mdbm_next(s->mdbm)) {
		unless (CSET(s)) {
			n++;
			continue;
		}
		if (sccs_iskeylong(kv.val.dptr)) n++;
	}
	return (n);
}

/*
 * Initialize as much as possible from the file.
 * Don't override any information which is already set.
 * XXX - this needs to track sccs_prsdelta/do_patch closely.
 *
 * R/D/M - delta type
 * B - cset file key
 * C - cset boundry marker
 * D - dangle marker
 * c - comments
 * E - ignored for now
 * F - date fudge
 * i - include keys
 * K - delta checksum
 * M - merge delta key
 * O - modes (file permissions)
 * P - pathname
 * R - random bits
 * S - symbols
 * s - symbol graph/leaf
 * T - text
 * V - file format version
 * x - exclude keys
 * X - X_flags
 *
 * args:
 *   f	       the INIT file to be read
 *   takepatch
 *   flags     options to function:
 *      DELTA_PATCH      INIT is from a patch ('D' has different format)
 *      DELTA_TAKEPATCH  called from takepatch
 *   errorp    ptr to return error output (required if errors can occur)
 *   linesp    ptr to return number of lines read
 *   symsp     ptr to return list of new tags
 */
delta *
sccs_getInit(sccs *sc, delta *d, MMAP *f, u32 flags, int *errorp, int *linesp,
	     char ***symsp)
{
	char	*s, *t;
	char	*buf;
	int	nocomments = d && d->comments;
	char	**comments = 0;
	int	error = 0;
	int	lines = 0;
	char	type = '?';
	char	**syms = 0;
	ser_t	serial = 0;
	FILE	*cludes = 0;

	/* these are the only possible flags */
	assert((flags & ~(DELTA_TAKEPATCH|DELTA_PATCH)) == 0);

	unless (f) {
		if (errorp) *errorp = 0;
		return (d);
	}

#define	WANT(c) ((buf[0] == c) && (buf[1] == ' '))
	unless (buf = mkline(mnext(f))) {
		fprintf(stderr, "Warning: no delta line in init file.\n");
		error++;
		goto out;
	}
	lines++;
	unless (WANT('R') || WANT('D') || WANT('M')) {
		fprintf(stderr, "Warning: no D/R/M line in init file.\n");
		error++;
		goto out;
	}
	type = buf[0];

	/* D 1.2 93/03/11 00:50:40[-8:00] butthead 2 1	9/2/44 */
	assert((buf[1] == ' ') && isdigit(buf[2]));
	for (s = &buf[2]; *s++ != ' '; );
	if (!d || !d->r[0]) {
		s[-1] = 0;
		d = sccs_parseArg(sc, d, 'R', &buf[2], 0);
	}
	assert(d);
	t = s;
	while (*s++ != ' ');	/* eat date */
	while (*s++ != ' ');	/* eat time */
	unless (d->date) {
		s[-1] = 0;
		d = sccs_parseArg(sc, d, 'D', t, 0);
	}
	t = s;
	while (*s && (*s++ != ' '));	/* eat user@host */
	unless (d->userhost) {
		if (s[-1] == ' ') s[-1] = 0;
		d = sccs_parseArg(sc, d, 'U', t, 0);
	}
	if (flags & DELTA_PATCH) {
		if ((*s == '+') && !d->added) {
			d->added = atoi(s+1);
			while (*s && (*s++ != ' '));
			if (*s == '-') d->deleted = atoi(s+1);
			while (*s && (*s++ != ' '));
			if (*s == '=') d->same = atoi(s+1);
		}
		goto skip;	/* skip the rest of this line */
	}
	t = s;
	while (*s && (*s++ != ' '));	/* serial */
	serial = atoi(t);
	t = s;
	while (*s && (*s++ != ' '));	/* pserial */
	unless (d->pserial) {
		if (s[-1] == ' ') s[-1] = 0;
		d->pserial = atoi(t);
	}
	while (*s == ' ') s++;
	t = s;
	while (*s && (*s++ != '/'));	/* added */
	unless (d->added) {		// XXX - test a patch with > 99999 lines in a delta
		if (s[-1] == '/') s[-1] = 0;
		d->added = atoi(t);
	}
	t = s;
	while (*s && (*s++ != '/'));	/* deleted */
	unless (d->deleted) {
		if (s[-1] == '/') s[-1] = 0;
		d->deleted = atoi(t);
	}
	unless (d->same) {
		d->same = atoi(s);
	}

skip:
	buf = mkline(mnext(f)); lines++;

	/* hash hash */
	if (WANT('A')) {
		d = sccs_parseArg(sc, d, 'A', &buf[2], 0);
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* Cset file ID */
	if (WANT('B')) {
		unless (d->csetFile) d = sccs_parseArg(sc, d, 'B', &buf[2], 0);
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* Cset marker */
	if ((buf[0] == 'C') && !buf[1]) {
		d->flags |= D_CSET;
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* Dangle marker */
	if ((buf[0] == 'D') && !buf[1]) {
		d->flags |= D_DANGLING;
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/*
	 * Comments are optional and look like:
	 * c added 4.x etc targets
	 */
	while (buf[0] == 'c') {
		char *p;
		if (buf[1] == ' ') p = &buf[2];
		else if (buf[1] == '\0') p = &buf[1];
		else break;
		unless (nocomments) comments = addLine(comments, strdup(p));
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	if (WANT('E')) {
		/* ignored */
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* date fudges are optional */
	if (WANT('F')) {
		d->dateFudge = atoi(&buf[2]);
		d->date += d->dateFudge;
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* Includes are optional and are specified as keys.
	 * If there is no sccs* ignore them.
	 */
	while (WANT('i')) {
		unless (flags & DELTA_TAKEPATCH) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			unless (e) {
				fprintf(stderr, "Can't find inc %s in %s\n",
				    &buf[2], sc->sfile);
				sc->state |= S_WARNED;
				error++;
				goto out;
			} else {
				unless (cludes) cludes = fmem();
				sccs_saveNum(cludes, SERIAL(sc, e), 1);
			}
		}
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* cksums are optional but shouldn't be */
	if (WANT('K')) {
		d = sumArg(d, &buf[2]);
		d->flags |= D_ICKSUM;
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* merge deltas are optional */
	if (WANT('M')) {
		unless (flags & DELTA_TAKEPATCH) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			unless (e) {
				fprintf(stderr, "Can't find merge %s in %s\n",
				    &buf[2], sc->sfile);
				sc->state |= S_WARNED;
				error++;
				goto out;
			} else {
				d->merge = SERIAL(sc, e);
			}
		}
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* modes are optional */
	if (WANT('O')) {
		unless (d->mode) d = modeArg(sc, d, &buf[2]);
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* pathnames are optional */
	if (WANT('P')) {
		unless (d->pathname) d = pathArg(sc, d, &buf[2]);
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* Random bits are used only for 1.0 deltas in conversion scripts */
	if (WANT('R')) {
		d->random = sccs_addStr(sc, &buf[2]);
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* symbols are optional */
	while (WANT('S')) {
		syms = addLine(syms, strdup(&buf[2]));
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/*
	 * symbols as keys are optional.  Order of keys is ptag, mtag.
	 * s g - set d->symGraph
	 * s l - set d->symLeaf
	 * s <key> - set d->ptag
	 * s <key> - set d->mtag
	 */
	while (WANT('s')) {
		TRACE("buf = %s", buf);
		if (streq(&buf[2], "g")) {
			if (d) d->flags |= D_SYMGRAPH;
		} else if (streq(&buf[2], "l")) {
			if (d) d->flags |= D_SYMLEAF;
		} else if (!(flags & DELTA_TAKEPATCH)) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			assert(e);
			TRACE("e->serial = %d", SERIAL(sc, e));
			assert(SYMGRAPH(e));
			if (d->ptag) {
				d->mtag = SERIAL(sc, e);
			} else {
				d->ptag = SERIAL(sc, e);
			}
			e->flags &= ~D_SYMLEAF;
		}
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* text are optional */
	/* Cannot be WANT('T'), buf[1] could be null */
	while (buf[0] == 'T') {
		/* ignored, was d->text */
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	if (WANT('V')) {
		unless (streq("1.0", REV(sc, d))) {
			fprintf(stderr, "sccs_getInit: version only on 1.0\n");
		} else {
			int	vers = atoi(&buf[3]);

			if (sc->version < vers) sc->version = vers;
		}
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* Excludes are optional and are specified as keys.
	 * If there is no sccs* ignore them.
	 */
	while (WANT('x')) {
		unless (flags & DELTA_TAKEPATCH) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			unless (e) {
				fprintf(stderr, "Can't find ex %s in %s\n",
				    &buf[2], sc->sfile);
				sc->state |= S_WARNED;
				error++;
			} else {
				unless (cludes) cludes = fmem();
				sccs_saveNum(cludes, SERIAL(sc, e), -1);
			}
		}
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	if (WANT('X')) {
		d->xflags = strtol(&buf[2], 0, 0); /* hex or dec */
		d->xflags &= ~X_SINGLE;
		d->flags |= D_XFLAGS;
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* closing line is not optional. */
	if (strcmp("------------------------------------------------", buf)) {
		fprintf(stderr,
		    "Warning: Bad line in init file follows.\n'%s'\n", buf);
		error++;
	}

out:
	if (d) {
		if (comments) {
			comments_set(sc, d, comments);
			freeLines(comments, free);
		}
		if (cludes) {
			d->cludes = sccs_addStr(sc, fmem_peek(cludes, 0));
			fclose(cludes);
		}
		if (type == 'M') {
			d->flags |= D_META|D_TAG;
		} else if (type == 'R') {
			d->flags |= D_TAG;
		}
#ifdef	GRAFT_BREAKS_LOD
		if (flags & DELTA_PATCH) {
			free(d->rev);
			d->rev = 0;
		}
#endif
	}
	assert(errorp || !error);
	if (errorp) *errorp = error;
	if (linesp) *linesp += lines;
	if (symsp) *symsp = syms;
	return (d);
}

/*
 * Read the p.file and extract and return the old/new/user/{inc, excl} lists.
 *
 * Returns 0 if OK, -1 on error.  Warns on all errors.
 */
int
sccs_read_pfile(char *who, sccs *s, pfile *pf)
{
	int	fsize = size(s->pfile);
	char	*iLst, *xLst;
	char	*mRev = malloc(MAXREV+1);
	char	c1 = 0, c2 = 0, c3 = 0;
	int	e;
	FILE	*tmp;
	char	oldrev[MAXREV], newrev[MAXREV];
	char	date[10], time[10], user[40];

	bzero(pf, sizeof(*pf));
	if (!fsize) {
		fprintf(stderr, "Empty p.file %s - aborted.\n", s->pfile);
		return (-1);
	}
	unless (tmp = fopen(s->pfile, "r")) {
		fprintf(stderr, "pfile: can't open %s\n", s->pfile);
		free(mRev);
		return (-1);
	}
	iLst = malloc(fsize);
	xLst = malloc(fsize);
	iLst[0] = xLst[0] = 0;
	e = fscanf(tmp, "%s %s %s %s %s -%c%s -%c%s -%c%s",
	    oldrev, newrev, user, date, time, &c1, iLst, &c2, xLst,
	    &c3, mRev);
	pf->oldrev = strdup(oldrev);
	pf->newrev = strdup(newrev);
	fclose(tmp);

	/*
	 * mRev always means there is at least an include -
	 * we already expolded it in get and wrote it out.
	 * There may or may not be an xLst w/ mRev.
	 */
	switch (e) {
	    case 5:		/* No extras, cool. */
		free(iLst); iLst = 0;
		free(xLst); xLst = 0;
		free(mRev); mRev = 0;
		break;
	    case 7:		/* figure out if it was -i or -x or -m */
		free(xLst); xLst = 0;
		free(mRev); mRev = 0;
		if (c1 == 'i') {
			/* do nothing */
		} else if (c1 == 'x') {
			xLst = iLst;
			iLst = 0;
		} else {
			assert(c1 == 'm');
			mRev = iLst;
			iLst = 0;
		}
		break;
	    case 9:		/* Could be -i -x or -i -m  or -x -m */
		assert(c1 != 'm');
		assert(c1 == 'i' || c1 == 'x');
		free(mRev);
		mRev = 0;
		if (c1 == 'x') {
			assert(c2 == 'm');
			mRev = xLst;
			xLst = iLst;
			iLst = 0;
		}
		else {
			assert(c1 == 'i');
			if (c2 == 'm') {
				mRev = xLst;
				xLst = 0;
			} else {
				assert(c2 == 'x');
			}
		}
		break;
	    case 11:		/* has to be -i -x -m */
		assert(c1 == 'i');
		assert(c2 == 'x');
		assert(c3 == 'm');
		break;
	    default:
		free(iLst);
		free(xLst);
		fprintf(stderr,
		    "%s: can't get revision info from %s\n", who, s->pfile);
		return (-1);
	}
	pf->iLst = iLst;
	pf->xLst = xLst;
	pf->mRev = mRev;
	debug((stderr, "pfile(%s, %s, %s, %s, %s, %s, %s)\n",
    	    pf->oldrev, pf->newrev, user, date,
	    notnull(pf->iLst), notnull(pf->xLst), notnull(pf->mRev)));
	return (0);
}

int
isValidHost(char *h)
{
	if (!h || !(*h)) return 0;
	if (streq(h, "localhost") || streq(h, "localhost.localdomain")) {
		return 0;
	}
	/*
	 * XXX TODO: should we do a gethostbyname to verify host name ??
	 * 	     disallow non-alpha numberic character ?
	 */
	return 1;
}

int
isValidUser(char *u)
{
	if (!u || !(*u)) return 0;
	if (sameuser(u, ROOT_USER) || sameuser(u, UNKNOWN_USER)) return 0;
	/*
	 * XXX TODO:
	 * 	a) should we disallow "Guest/guest" as user name ??
	 * 	b) should we check /etc/passwd to verify user name ??
	 * 	c) disallow non-allha numberic character ?
	 */
	return 1;
}

/*
 * Add a metadata delta to the tree.
 * There are no contents, just various fields which are defined in the
 * init file passed in.
 *
 * XXX - there is a lot of duplicated code here, in checkin, in sccs_delta,
 * and probably other places.  I need to make a generic sccs_addDelta()
 * which can handle all of those cases.
 */
private int
sccs_meta(char *me, sccs *s, delta *parent, MMAP *iF, int fixDate)
{
	delta	*m;
	int	i, e = 0;
	FILE	*sfile = 0;
	char	*buf;
	char	**syms;

	unless (sccs_lock(s, 'z')) {
		fprintf(stderr,
		    "meta: can't get lock on %s\n", s->sfile);
		s->state |= S_WARNED;
		debug((stderr, "meta returns -1\n"));
		return (-1);
	}
	unless (iF) {
		sccs_unlock(s, 'z');
		return (-1);
	}
	m = sccs_getInit(s, 0, iF, DELTA_PATCH, &e, 0, &syms);
	mclose(iF);
	memcpy(m->r, parent->r, sizeof(m->r));
	m->pserial = SERIAL(s, parent);
	s->numdeltas++;
	m = dinsert(s, m, fixDate);
	EACH(syms) addsym(s, m, 0, syms[i]);
	freeLines(syms, free);
	/*
	 * Do the delta table & misc.
	 */
	unless (sfile = sccs_startWrite(s)) {
		sccs_unlock(s, 'z');
		exit(1);
	}
	if (delta_table(s, sfile, 0)) {
abort:		sccs_abortWrite(s, &sfile);
		sccs_unlock(s, 'z');
		return (-1);
	}
	sccs_rdweaveInit(s);
	if (GZIP_OUT(s)) sccs_zputs_init(s, sfile);
	assert(s->state & S_SOPEN);
	while (buf = sccs_nextdata(s)) {
		fputdata(s, buf, sfile);
		fputdata(s, "\n", sfile);
	}
	if (GZIP_OUT(s)) sccs_zputs_done(s);
	sccs_rdweaveDone(s);
	if (sccs_finishWrite(s, &sfile)) goto abort;
	sccs_unlock(s, 'z');
	return (0);
}

/*
 * Make this delta start off as a .0.
 */
private void
dot0(sccs *s, delta *d)
{
	int	i = 1;
	char	buf[MAXREV];

	do {
		sprintf(buf, "1.0.%d.0", i++);
	} while (rfind(s, buf));
	revArg(s, d, buf);
}

/*
 * delta the specified file.
 *
 * Init file implies the old NODEFAULT flag, i.e., if there was an init
 * file, that is the truth.  The truth can be over ridden with a prefilled
 * delta but even that is questionable.
 * Return codes:
 *	-1 = error
 *	-2 = DELTA_AUTO (delta -a) and no diff in gfile
 *	-3 = not DELTA_AUTO or DELTA_FORCE and no diff in gfile (gfile unlinked)
 */
int
sccs_delta(sccs *s,
    	u32 flags, delta *prefilled, MMAP *init, MMAP *diffs, char **syms)
{
	FILE	*sfile = 0;	/* the new s.file */
	int	i, free_syms = 0, error = 0;
	delta	*d = 0, *p, *n = 0;
	char	*rev, *tmpfile = 0;
	int	added = 0, deleted = 0, unchanged = 0;
	int	locked;
	pfile	pf;
	ser_t	*include = 0;
	ser_t	*exclude = 0;

	assert(s);
	debug((stderr, "delta %s %x\n", s->gfile, flags));
	if (flags & NEWFILE) mksccsdir(s->sfile);
	bzero(&pf, sizeof(pf));
	unless (locked = sccs_lock(s, 'z')) {
		fprintf(stderr,
		    "delta: can't get write lock on %s\n", s->sfile);
		repository_lockers(s->proj);
		error = -1; s->state |= S_WARNED;
out:
		if (prefilled) sccs_freedelta(prefilled);
		if (sfile) sccs_abortWrite(s, &sfile);
		if (diffs) mclose(diffs);
		free_pfile(&pf);
		if (free_syms) freeLines(syms, free); 
		if (tmpfile  && !streq(tmpfile, DEVNULL_WR)) unlink(tmpfile);
		if (locked) sccs_unlock(s, 'z');
		debug((stderr, "delta returns %d\n", error));
		return (error);
	}
#define	OUT	{ error = -1; s->state |= S_WARNED; goto out; }
#define	WARN	{ error = -1; goto out; }

	if (init) {
		int	e;

		if (syms) {
			fprintf(stderr, "delta: init or symbols, not both\n");
			init = 0;  /* prevent double free */
			OUT;
		}
		prefilled = sccs_getInit(s,
		    prefilled, init, flags&DELTA_PATCH, &e, 0, &syms);
		/*
		 * Normally, the syms list is passed in by the caller
		 * and we let the caller free it.
		 * Here we are getting the syms list from the init file,
		 * i.e this syms list is unknown to the caller.
		 * thus we must free the syms list before we return.
		 */
		if (syms) free_syms = 1;
		unless (prefilled && !e) {
			fprintf(stderr, "delta: bad init file\n");
			OUT;
		}
		debug((stderr, "delta got prefilled %s\n", prefilled->rev));
		if (flags & DELTA_PATCH) {
			if (prefilled->pathname &&
			    streq(PATHNAME(s, prefilled), "ChangeSet")) {
				s->state |= S_CSET;
		    	}
			unless (flags & NEWFILE) {
				/* except the very first delta   */
				/* all rev are subject to rename */
				memset(prefilled->r, 0, sizeof(prefilled->r));

				/*
				 * If we have random bits, we are the root of
				 * some other file, so make our rev start at
				 * .0
				 */
				if (prefilled->random) dot0(s, prefilled);
			}
		}
	}

	if ((flags & NEWFILE) || (!HAS_SFILE(s) && HAS_GFILE(s))) {
		int rc;

		rc = checkin(s, flags, prefilled, init != 0, diffs, syms);
		if (free_syms) freeLines(syms, free); 
		return rc;
	}

	if (BAM(s) && bk_notLicensed(s->proj, LIC_BAM, 1)) OUT;
	if (toobig(s)) OUT;

	unless (HAS_SFILE(s) && HASGRAPH(s)) {
		fprintf(stderr, "delta: %s is not an SCCS file\n", s->sfile);
		OUT;
	}

	unless (HAS_PFILE(s)) {
		if (WRITABLE(s)) {
			fprintf(stderr,
			    "delta: %s writable but not checked out?\n",
			    s->gfile);
			OUT;
		} else {
			verbose((stderr,
			    "delta: %s is not locked.\n", s->sfile));
			goto out;
		}
	}

	unless (WRITABLE(s) || diffs) {
		fprintf(stderr,
		    "delta: %s is locked but not writable.\n", s->gfile);
		OUT;
	}

	if (HAS_GFILE(s) && diffs) {
		fprintf(stderr,
		    "delta: diffs or gfile for %s, but not both.\n",
		    s->gfile);
		OUT;
	}

	/* Refuse to make deltas to 100% dangling files */
	if (DANGLING(s->tree) && !(flags & DELTA_PATCH)) {
		fprintf(stderr,
		    "delta: entire file %s is dangling, abort.\n", s->gfile);
		OUT;
	}

	/*
	 * OK, checking done, start the delta.
	 */
	if (sccs_read_pfile("delta", s, &pf)) OUT;
	unless (d = findrev(s, pf.oldrev)) {
		fprintf(stderr,
		    "delta: can't find %s in %s\n", pf.oldrev, s->gfile);
		OUT;
	}

	/*
	 * Catch p.files with bogus revs.
	 */
	rev = REV(s, d);
	p = sccs_getedit(s, &rev);
	assert(p);	/* we just found it above */
	unless (streq(rev, pf.newrev)) {
		fprintf(stderr,
		    "delta: invalid nextrev %s in p.file, using %s instead.\n",
		    pf.newrev, rev);
		free(pf.newrev);
		pf.newrev = strdup(rev);
	}

	if (DANGLING(d)) {
		if (diffs && !(flags & DELTA_PATCH)) {
			fprintf(stderr,
			    "delta: dangling deltas may not be "
			    "combined with diffs\n");
			OUT;
		}
		while (NEXT(d) && (DANGLING(d) || TAG(d))) d = NEXT(d);
		assert(NEXT(d));
		strcpy(pf.oldrev, REV(s, d));
	}

	if (pf.mRev || pf.xLst || pf.iLst) flags |= DELTA_FORCE;
	debug((stderr, "delta found rev\n"));
	if (diffs) {
		debug((stderr, "delta using diffs passed in\n"));
	} else {
		switch (diff_g(s, &pf, &tmpfile)) {
		    case 1:		/* no diffs */
			if (flags & DELTA_FORCE) {
				break;     /* forced 0 sized delta */
			}
			unless (flags & SILENT) {
				fprintf(stderr,
				    "Clean %s (no diffs)\n", s->gfile);
			}
			if (flags & DELTA_AUTO) {
				error = -2;
				goto out;
			}
			unlink(s->pfile);
			unless (flags & DELTA_SAVEGFILE) unlinkGfile(s);
			error = -3;
			goto out;
		    case 0:		/* diffs */
			break;
		    default: OUT;
		}
		/* We prefer binary mode, but win32 GNU diff used text mode */ 
		unless (tmpfile && (diffs = mopen(tmpfile, "t"))) {
			fprintf(stderr,
			    "delta: can't open diff file %s\n", tmpfile);
			OUT;
		}
	}
	if (flags & PRINT) {
		fprintf(stdout, "==== Changes to %s ====\n", s->gfile);
		if (BAM(s)) {
			fprintf(stdout, "Binary files differ.\n");
		} else {
			fwrite(diffs->mmap, diffs->size, 1, stdout);
		}
		fputs("====\n\n", stdout);
	}
	/*
	 * Add a new delta table entry.	 We'll come back and fix up
	 * the add/del/unch lines later.
	 * In conflicts between prefilled and the pfile, prefilled wins.
	 * prefilled can have date, user, host, path, comments, and/or rev.
	 */
	if (prefilled) {
		n = prefilled;
		prefilled = 0;
	} else {
		n = new(delta);
		assert(n);
	}
	if (pf.iLst) {
		include = getserlist(s, 0, pf.iLst, &error);
	}
	if (pf.xLst) {
		exclude = getserlist(s, 0, pf.xLst, &error);
	}
	if (pf.mRev) {
		delta	*e = findrev(s, pf.mRev);

		unless (e) {
			fprintf(stderr,
			    "delta: no such rev %s in %s\n", pf.mRev, s->sfile);
		    	OUT;
		}
		if (n->merge && (SERIAL(s, e) != n->merge)) {
			fprintf(stderr,
			    "delta: conflicting merge revs: %s %s\n",
			    REV(s, n), REV(s, e));
			OUT;
		}
		n->merge = SERIAL(s, e);
	}
	if (error) OUT;
	n = sccs_dInit(n, 'D', s, init != 0);
	updMode(s, n, d);

	unless (n->r[0]) revArg(s, n, pf.newrev);
	assert(d);
	n->pserial = SERIAL(s, d);
	if (!n->comments && !init &&
	    !(flags & DELTA_DONTASK)) {
		/*
		 * If they told us there should be a c.file, use it.
		 * Else look for one and if found, prompt and use it.
		 * Else ask for comments.
		 */
		if (flags & DELTA_CFILE) {
			if (comments_readcfile(s, 0, n)) OUT;
		} else switch (comments_readcfile(s, 1, n)) {
		    case -1: /* no c.file found */
			unless (comments_get(s->gfile, pf.newrev, s, n)) {
				error = -4;
				goto out;
			}
			break;
		    case -2: /* aborted in prompt */
			error = -4;
			goto out;
		}
	}
	n = dinsert(s, n, !(flags & DELTA_PATCH));
	d = SFIND(s, n->pserial);
	assert(s->table == n);

	if (include || exclude) {
		FILE	*f = fmem();

		EACH(include) sccs_saveNum(f, include[i], 1);
		EACH(exclude) sccs_saveNum(f, exclude[i], -1);
		n->cludes = sccs_addStr(s, fmem_peek(f, 0));
		fclose(f);
	}

	unless (init || !n->pathname || !d->pathname ||
	    streq(PATHNAME(s, d), PATHNAME(s, n)) || getenv("_BK_MV_OK")) {
	    	fprintf(stderr,
		    "delta: must use bk mv to rename %s to %s\n",
		    PATHNAME(s, d), PATHNAME(s, n));
		OUT;
	}
	s->numdeltas++;

	/* Uses n->parent, has to be after dinsert() */
	if ((flags & DELTA_MONOTONIC) && !(sccs_xflags(s, n) & X_MONOTONIC)) {
		n->xflags |= sccs_xflags(s, PARENT(s, n));
		n->xflags |= X_MONOTONIC;
		n->flags |= D_XFLAGS;
	}

	EACH(syms) addsym(s, n, !(flags&DELTA_PATCH), syms[i]);

	if (BAM(s)) {
		if (!(flags & DELTA_PATCH) && bp_delta(s, n)) {
			fprintf(stderr, "BAM: delta of %s failed\n",
			    s->gfile);
			return (-1);
		}
		added = s->added = n->added;
	}

	/*
	 * Do the delta table & misc.
	 */
	unless (sfile = sccs_startWrite(s)) OUT;

	/*
	 * If the new delta is a top-of-trunk, update the xflags
	 * This is needed to maintain the xflags invariant:
	 * s->state should always match sccs_xflags(tot);
	 * where "tot" is the top-of-trunk delta.
 	 */
	if (init && (flags&DELTA_PATCH) && (n->flags & D_XFLAGS)) {
		if (n == sccs_top(s)) s->xflags = n->xflags;
	}

	n->flags |= D_CKSUM;
	if (delta_table(s, sfile, 1)) {
		goto out;	/* not OUT - we want the warning */
	}

	assert(d);
	unless (BAM(s)) {
		if (delta_body(s, n, diffs,
			sfile, &added, &deleted, &unchanged)) {
			OUT;
		}
	}
	if (S_ISLNK(n->mode)) {
		u8 *t;
		/*
		 * if symlink, check sum the symlink path
		 */
		for (t = SYMLINK(s, n); *t; t++) s->dsum += *t;
	}
	if (end(s, n, sfile, flags, added, deleted, unchanged)) {
		sccs_abortWrite(s, &sfile);
		WARN;
	}
	unless (flags & DELTA_SAVEGFILE)  {
		if (unlinkGfile(s)) {				/* Careful. */
			fprintf(stderr, "delta: cannot unlink %s\n", s->gfile);
			OUT;
		}
	}
	if (sccs_finishWrite(s, &sfile)) OUT;
	unlink(s->pfile);
	comments_cleancfile(s);
	if (BITKEEPER(s) && !(flags & DELTA_NOPENDING)) {
		 updatePending(s);
	}
	goto out;
#undef	OUT
}

int	do_fsync = -1;

/*
 * Print the summary and go and fix up the top.
 */
private int
end(sccs *s, delta *n, FILE *out, int flags, int add, int del, int same)
{
	int	i;
	char	buf[100];

	unless (flags & SILENT) {
		int	lines = count_lines(s, PARENT(s, n)) - del + add;

		fprintf(stderr, "%s revision %s: ", s->gfile, REV(s, n));
		fprintf(stderr, "+%d -%d = %d\n", add, del, lines);
	}
	n->added = add;
	n->deleted = del;
	n->same = same;

	/*
	 * Now fix up the checksum and summary.
	 */
	fseek(out, s->adddelOff, SEEK_SET);
	if (BFILE_OUT(s)) {
		bin_deltaAdded(s, n, out);
	} else {
		sprintf(buf, "\001s %d/%d/%d", add, del, same);
		for (i = strlen(buf); i < 43; i++) buf[i] = ' ';
		strcpy(buf+i, "\n");
		fputmeta(s, buf, out);
	}
	if (BITKEEPER(s)) {
		if (!BAM(s) && (add || del || same) && (n->flags & D_ICKSUM)) {
			delta	*z = getCksumDelta(s, n);

			assert(z);
			/* they should match */
			if (s->dsum != z->sum) {
				/*
				 * we allow bad symlink chksums if they are
				 * zero; it's a bug in old binaries.
				 */
				unless (S_ISLNK(n->mode) && !n->sum) {
					fprintf(stderr,
					    "%s: bad delta checksum: "
					    "%u:%d for %s\n",
					    s->sfile, s->dsum,
					    z ? z->sum : -1, REV(s, n));
					s->bad_dsum = 1;
				}
			}
		}
		unless (n->flags & D_ICKSUM) {
			/*
			 * XXX: would like "if cksum is same as parent"
			 * but we can't do that because we use the inc/ex
			 * in getCksumDelta().
			 */
			if (add || del || n->cludes || S_ISLNK(n->mode)) {
				n->sum = s->dsum;
			} else {
				n->sum = almostUnique();
			}
#if 0
Breaks up citool

			/*
			 * XXX - should this be fatal for cset?
			 * It probably should but only if the
			 * full keys are the same.
			 */
			if (n->parent && (n->parent->sum == n->sum)) {
				fprintf(stderr,
				    "%s: warning %s & %s have same sum\n",
				    s->sfile, n->parent->rev, n->rev);
			}
#endif
		}
		if (fseek(out, s->sumOff, SEEK_SET)) perror("fseek");
		if (BFILE_OUT(s)) {
			if (bin_deltaSum(s, n, out)) {
				perror("fwrite");
			}
		} else {
			sprintf(buf, "%05u", n->sum);
			fputmeta(s, buf, out);
		}
	}
	if (proj_sync(s->proj)) fsync(fileno(out));
	return (0);
}

private void
mkTag(char *rev, char *revM, pfile *pf, char *path, char tag[])
{
	/*
	 * 1.0 => create (or reverse create in a reverse pacth )
	 * /dev/null => delete (i.e. sccsrm)
	 */
	if (streq(rev, "1.0") || streq(path, DEVNULL_RD)) {
		sprintf(tag, "%s", "/dev/null");
	} else {
		strcpy(tag, rev);
		if (revM) { 
			strcat(tag, "+");
			strcat(tag, revM);
		}
		if (pf) {
			if (pf->iLst) {
				strcat(tag, "+");
				strcat(tag, pf->iLst);
			}
			if (pf->xLst) {
				strcat(tag, "-");
				strcat(tag, pf->xLst);
			}
		}
		strcat(tag, "/");
		strcat(tag, path);
	}
}


/*
 * helper function for sccs_diffs
 */
private void
mkDiffHdr(u32 kind, char tag[], char *buf, FILE *out)
{
	char	*marker, *date;

	unless (kind & (DF_UNIFIED|DF_CONTEXT|DF_GNUp)) {
		fputs(buf, out);
		return; 
	}

	/*
	 * Fix the file names.
	 *
	 * For bk diff, the file name should be 
	 * +++ bk.sh 1.34  Thu Jun 10 21:22:08 1999
	 *
	 */
	if (strneq(buf, "+++ ", 4) || strneq(buf, "--- ", 4)) {
		date = strchr(buf, '\t'); assert(date);
		buf[3] = 0; marker = buf;
		fprintf(out, "%s %s%s", marker, tag, date);
	} else	fputs(buf, out);
}

/*
 * set_comments(), diffComments():
 * Print out the comments for the diff range passed in.
 */
private	char	**h;

private void
set_comments(sccs *s, delta *d)
{
	char	*buf;
	char	*p;

	buf = sccs_prsbuf(s, d, 0, 
	    ":D: :T: :P:$if(:HT:){@:HT:} :I: +:LI: -:LD:\n"
	    "$each(:C:){   (:C:)\n}");
	if (buf &&
	    (p = strchr(buf, '\n')) && !streq(p, "\n   Auto merged\n")) {
		h = addLine(h, strdup(buf));
	}
}

private void
diffComments(u32 kind, FILE *out, sccs *s, char *lrev, char *rrev)
{
	int	i;

	unless (rrev) rrev = "+";
	if (streq(rrev, "edited")) rrev = "+";	/* XXX - what about edit??? */
	set_diff(s, set_get(s, rrev), set_get(s, lrev), set_comments);
	if ((kind & DF_IFDEF) && h) fputs("#ifdef !!COMMENTS!!\n", out);
	EACH(h) {
		fputs(h[i], out);
	}
	if (h) {
		if (kind & DF_IFDEF) {
			fputs("#endif !!COMMENTS!!\n", out);
		} else {
			fputs("\n", out);
		}
		freeLines(h, free);
		h = 0;
	}
}

private int
doDiff(sccs *s, u32 flags, u32 kind, char *leftf, char *rightf,
	FILE *out, char *lrev, char *rrev, char *ltag, char *rtag)
{
	FILE	*diffs = 0;
	char	diffFile[MAXPATH];
	char	buf[MAXLINE];
	char	spaces[80];
	int	first = 1;
	char	*error = "";

	if (kind & DF_SDIFF) {
		int	i, c;
		char	*columns = 0;

		unless (columns = getenv("COLUMNS")) columns = "80";
		c = atoi(columns);
		for (i = 0; i < c/2 - 18; ) spaces[i++] = '=';
		spaces[i] = 0;
		sprintf(buf, "bk sdiff -w%s '%s' '%s'", columns, leftf, rightf);
		diffs = popen(buf, "r");
		if (!diffs) return (-1);
		diffFile[0] = 0;
	} else {
		strcpy(spaces, "=====");
		unless (bktmp(diffFile, "diffs")) return (-1);
		diff(leftf, rightf, kind, diffFile);
		diffs = fopen(diffFile, "rt");
	}
	if (WRITABLE(s) && !EDITED(s)) {
		error = " (writable without lock!) ";
	}
	while (fnext(buf, diffs)) {
		if (first) {
			if ((flags & DIFF_HEADER) && (kind & DF_IFDEF)) {
				fprintf(out, "#ifdef !!HEADER!!\n");
				fprintf(out, "< %s %s\n", s->gfile, lrev);
				fprintf(out, "> %s %s\n", s->gfile, rrev);
				fprintf(out, "#endif !!HEADER!!\n");
			} else if (flags & DIFF_HEADER) {
				fprintf(out, "%s %s %s vs %s%s %s\n",
				   spaces, s->gfile, lrev, rrev, error, spaces);
			}
			if (flags & DIFF_COMMENTS) {
				diffComments(kind, out, s, lrev, rrev);
			}
			unless (flags & DIFF_HEADER) fprintf(out, "\n");
			first = 0;
			mkDiffHdr(kind, ltag, buf, out);
			unless (fnext(buf, diffs)) break;
			mkDiffHdr(kind, rtag, buf, out);
		} else {
			fputs(buf, out);
		}
	}
	/* XXX - gross but useful hack to get spacers */
	if ((flags & DIFF_COMMENTS) && !(kind & DF_IFDEF) && !first) {
		fprintf(out, "\n");
	}
	if (kind & DF_SDIFF) {
		pclose(diffs);
	} else {
		fclose(diffs);
		unlink(diffFile);
	}
	return (0);
}


/*
 * Given r1, r2, compute rev1, rev2
 * r1 & r2 are user specified rev; can be incomplete 
 */
private int
mapRev(sccs *s, u32 flags, char *r1, char *r2,
			char **rev1, char **rev1M, char **rev2, pfile *pf)
{
	char *lrev, *lrevM = 0, *rrev;

	if (r1 && r2) {
		if (streq(r1, r2)) { /* r1 == r2 means diffs against parent(s) */
			if (sccs_parent_revs(s, r2, &lrev, &lrevM)) {
				return (-1);
			}
		} else {
			lrev = r1;
		}
		rrev = r2;
	} else if (HAS_PFILE(s)) {
		if (r1) {
			lrev = r1;
		} else {
			if (sccs_read_pfile("diffs", s, pf)) return (-1);
			lrev = pf->oldrev;
			lrevM = pf->mRev;
		}
		rrev = "edited";
	} else if (r1) {
		lrev = r1;
		rrev = 0;
	} else {
		unless (HAS_GFILE(s)) {
			verbose((stderr,
			    "diffs: %s not checked out.\n", s->gfile));
			s->state |= S_WARNED;
			return (-1);
		}
		lrev = 0;
		rrev = "?";
	}
	unless (sccs_findrev(s, lrev)) {
		return (-2);
	}
	if (r2 && !sccs_findrev(s, r2)) {
		return (-3);
	}
	if (!rrev) rrev = REV(s, findrev(s, 0));
	if (!lrev) lrev = REV(s, findrev(s, 0));
	if (streq(lrev, rrev)) return (-3);
	*rev1 = lrev; *rev1M = lrevM, *rev2 = rrev; 
	return 0;
}

private char *
getHistoricPath(sccs *s, char *rev)
{
	delta	*d;
	char	*p, *ret;

	d = sccs_findrev(s, rev);
	if (d && d->pathname) {
		return (sccs_prsbuf(s, d, PRS_FORCE, ":DPN:"));
	} else {
		ret = p = proj_relpath(s->proj, s->gfile);
		if (s->comppath) {
			ret = aprintf("%s/%s", s->comppath, p);
			free(p);
		}
		return (ret);
	}
}

private int
mkDiffTarget(sccs *s,
	char *rev, char *revM, u32 flags, char *target, pfile *pf)
{
	char	*pat;

	if (streq(rev, "1.0")) {
		strcpy(target, DEVNULL_RD);
		return (0);
	}
	pat = aprintf("%s-%s", basenm(s->gfile), rev);
	bktmp(target, pat);
	free(pat);

	if ((streq(rev, "edited") || streq(rev, "?")) && !findrev(s, rev)) {
		assert(HAS_GFILE(s));
		if (S_ISLNK(s->mode)) {
			char	buf[MAXPATH];
			FILE	*f;
			int	len;
			
			len = readlink(s->gfile, buf, sizeof(buf));
			if (len <= 0) return (-1);
			buf[len] = 0; /* stupid readlink... */
			f = fopen(target, "w");
			unless (f) return (-1);
			fprintf(f, "SYMLINK -> %s\n", buf);
			fclose(f);
		} else {
			unlink(target);
			strcpy(target, s->gfile);
		}
	} else if (sccs_get(s, rev, revM, pf ? pf->iLst : 0,
		    pf ? pf->xLst : 0, flags|SILENT|PRINT|GET_DTIME, target)) {
		return (-1);
	}
	/* Assumes that we only delta the ChangeSet file from the root */
	if (CSET(s)) cset_savetip(s);
	return (0);
}

private int
normal_diff(sccs *s, char *lrev, char *lrevM,
	char *rrev, u32 flags, u32 kind, FILE *out, pfile *pf)
{
	char	*lpath = 0, *rpath = 0;
	int	rc = -1;
	char	lfile[MAXPATH], rfile[MAXPATH];
	char	ltag[MAXPATH],	rtag[MAXPATH];

	/*
	 * Create the lfile & rfile for diff
	 */
	if (mkDiffTarget(s, lrev, lrevM, flags, lfile, pf)) {
		goto done;
	}
	if (mkDiffTarget(s, rrev, NULL,  flags, rfile, 0 )) {
		goto done;
	}

	lpath = getHistoricPath(s, lrev); assert(lpath);
	rpath = getHistoricPath(s, rrev); assert(rpath);

	/*
	 * make the tag string to label the diff output, e.g.
	 *
	 * +++ bk.sh 1.34  Thu Jun 10 21:22:08 1999
	 */
	mkTag(lrev, lrevM, pf, lpath, ltag);
	mkTag(rrev, NULL, NULL, rpath, rtag);

	/*
	 * Now diff the lfile & rfile
	 */
	rc = doDiff(s, flags, kind, lfile, rfile, out, lrev, rrev, ltag, rtag);
done:	unless (streq(lfile, DEVNULL_RD)) unlink(lfile);
	unless (streq(rfile, s->gfile) || streq(rfile, DEVNULL_RD)) unlink(rfile);
	if (lpath) free(lpath);
	if (rpath) free(rpath);
	return (rc);
}

/*
 * diffs - diff the gfile or the specified (or implied) rev
 */
int
sccs_diffs(sccs *s, char *r1, char *r2, u32 flags, u32 kind, FILE *out)
{
	char	*lrev, *lrevM, *rrev;
	pfile	pf;
	int	rc = 0;
	
	bzero(&pf, sizeof(pf));
	GOODSCCS(s);

	/*
	 * Figure out which revision the user want.
	 * Translate r1 => lrev, r2 => rrev.
	 */
	if (rc = mapRev(s, flags, r1, r2, &lrev, &lrevM, &rrev, &pf)) {
		goto done;
	}

	rc = normal_diff(s, lrev, lrevM, rrev, flags, kind, out, &pf);

done:	free_pfile(&pf);
	return (rc);
}

/*
 * Include the kw2val() hash lookup function.  This is dynamically
 * generated during the build by kwextract.pl and gperf (see the
 * Makefile).
 */
#include "kw2val_lookup.c"

#define	notKeyword -1
#define	nullVal    0
#define	strVal	   1

/*
 * Given a PRS DSPEC keyword, get the associated string value
 * If out is non-null print to out
 * If kw is not a keyword, return notKeyword
 * If kw has null value, return nullVal
 * Otherwise return strVal
 * Keyword definition is compatible with open group SCCS
 * This function may call itself recursively
 * kw2val() and fprintDelta() are mutually recursive
 *
 * XXX WARNING: If you add new keyord to this function, do _not_ print
 * to out directly, you _must_ use the fc()/fd()/fx()/f5d()/fs()
 * macros.
 *
 * ALSO: you must follow the format of the main switch statement below.
 * Each leg MUST look like:
 *     <tab>case KW_enumname:
 * followed by a comment containing the keyword name.  If a block follows,
 * the comment MUST appear before the {.
 */
int
kw2val(FILE *out, char *kw, int len, sccs *s, delta *d)
{
	struct kwval *kwval;
	char	*p, *q, *t;
	delta	*e = 0;
	char	*rev;

#define	KW(x)	kw2val(out, x, strlen(x), s, d)
#define	fc(c)	show_d(s, out, "%c", c)
#define	fd(n)	show_d(s, out, "%d", n)
#define	fu(n)	show_d(s, out, "%u", n)
#define	fx(n)	show_d(s, out, "0x%x", n)
#define	f5d(n)	show_d(s, out, "%05d", n)
#define	fs(str)	show_s(s, out, str, -1)
#define	fsd(d)	show_s(s, out, d.dptr, d.dsize)
#define	fm(ptr, len) show_s(s, out, ptr, len)

	unless (s && d) return (nullVal);

	/*
	 * Allow keywords of the form "word|rev"
	 * to mean "word" for revision "rev".
	 */
	for (p = kw, q = p+len; (*p != '|') && (p < q); ++p) ;
	if (p < q) {
		p++;
		rev = strndup(p, len - (p-kw));
		if ((rev[0] == '$') && isdigit(rev[1]) && !rev[2]) {
			/* substitute for a $\d variable */
			FILE	*f = fmem();

			dspec_eval(f, s, d, rev);
			if (t = fmem_close(f, 0)) {
				/* FYI: returns "" if empty */
				free(rev);
				rev = t;
			}
		}
		if (streq(rev, "PARENT")) {
			if (d) e = PARENT(s, d);
		} else if (streq(rev, "MPARENT")) {
			if (d) e = MERGE(s, d);
		} else {
			e = sccs_findrev(s, rev);
		}
		free(rev);
		unless (e) return (nullVal);
		len = p - 1 - kw;
		d = e;
	}
	kwval = kw2val_lookup(kw, len);
	unless (kwval) return notKeyword;

	unless (out) return (nullVal);
	switch (kwval->kwnum) {
	case KW_each: /* each */ {
		dspec_printeach(s, out);
		return (strVal);
	}
	case KW_eachline: /* line */ {
		dspec_printline(s, out);
		return (strVal);
	}
	case KW_Dt: /* Dt */ {
		/* :Dt: = :DT::I::D::T::P::DS::DP: */
		KW("DT"); fc(' '); KW("I"); fc(' ');
		KW("D"); fc(' '); KW("T"); fc(' ');
		KW("P"); fc(' '); KW("DS"); fc(' ');
		KW("DP");
		return (strVal);
	}
	case KW_DL: /* DL */ {
		/* :DL: = :LI:/:LD:/:LU: */
		KW("LI"); fc('/'); KW("LD"); fc('/'); KW("LU");
		return (strVal);
	}

	case KW_I: /* I */ {
		fs(REV(s, d));
		return (strVal);
	}

	case KW_D: /* D */ {
		/* date */
		KW("Dy"); fc('/'); KW("Dm"); fc('/'); KW("Dd");
		return (strVal);
	}

	case KW_D_: /* D_ */ {
		/* date */
		KW("Dy"); fc('-'); KW("Dm"); fc('-'); KW("Dd");
		return (strVal);
	}

	case KW_T: /* T */ {
		/* Time */
		/* XXX TODO: need to figure out when to print time zone info */
		KW("Th"); fc(':'); KW("Tm"); fc(':'); KW("Ts");
		return (strVal);
	}

	case KW_DI: /* DI */ {
		/* serial number of included and excluded deltas.
		 * :DI: = :Dn:/:Dx:/:Dg: in ATT, we do :Dn:/:Dx:
		 */
		unless (d->cludes) return (nullVal);
		p = strchr(CLUDES(s, d), '-');
		if (KW("Dn") && p) fc('/');
		if (p) KW("Dx");
		return (strVal);
	}

	case KW_RI: /* RI */ {
		/* rev number of included and excluded deltas.
		 * :DR: = :Rn:/:Rx:
		 */
		unless (d->cludes) return (nullVal);
		p = strchr(CLUDES(s, d), '-');
		if (KW("Rn") && p) fc('/');
		if (p) KW("Rx");
		return (strVal);
	}

	case KW_Dn: /* Dn */ {
		/* serial number of included deltas */
		int	i = 0, num, sign;

		unless (d->cludes) return (nullVal);
		t = CLUDES(s, d);
		while (num = sccs_eachNum(&t, &sign)) {
			unless (sign > 0) continue;
			unless (i) {
				i = 1;
				fc('+');
			} else {
				fc(',');
			}
			fd(num);
		}
		return (i ? strVal : nullVal);
	}

	case KW_Dx: /* Dx */ {
		/* serial number of excluded deltas */
		int	i = 0, num, sign;

		unless (d->cludes) return (nullVal);
		t = CLUDES(s, d);
		while (num = sccs_eachNum(&t, &sign)) {
			unless (sign < 0) continue;
			unless (i) {
				i = 1;
				fc('-');
			} else {
				fc(',');
			}
			fd(num);
		}
		return (i ? strVal : nullVal);
	}

	case KW_Dg: /* Dg */ {
		/* ignored delta - definition unknow, not implemented	*/
		/* always return null					*/
		return (nullVal);
	}

	/* rev number of included deltas */
	case KW_Rn: /* Rn */ {
		delta	*r;
		int	ser, sign, i = 0;

		unless (d->cludes) return (nullVal);
		t = CLUDES(s, d);
		while (ser = sccs_eachNum(&t, &sign)) {
			unless (sign > 0) continue;
			r = sfind(s, ser);
			unless (i) {
				i = 1;
				fc('+');
			} else {
				fc(',');
			}
			fs(REV(s, r));
		}
		return (i ? strVal : nullVal);
	}

	/* rev number of excluded deltas */
	case KW_Rx: /* Rx */ {
		delta	*r;
		int	ser, sign, i = 0;

		unless (d->cludes) return (nullVal);
		t = CLUDES(s, d);
		while (ser = sccs_eachNum(&t, &sign)) {
			unless (sign < 0) continue;
			r = sfind(s, ser);
			unless (i) {
				i = 1;
				fc('-');
			} else {
				fc(',');
			}
			fs(REV(s, r));
		}
		return (i ? strVal : nullVal);
	}

	/* Lose this in 2008 */
	case KW_W: /* W */ {
		/* a form of "what" string */
		/* :W: = :Z::M:\t:I: */
		KW("Z"); KW("M"); fc('\t'); KW("I");
		return (strVal);
	}

	case KW_A: /* A */ {
		/* a form of "what" string */
		/* :A: = :Z::Y: :M:I:Z: */
		KW("Z"); KW("Y"); fc(' ');
		KW("M"); KW("I"); KW("Z");
		return (strVal);
	}

	case KW_LI: /* LI */ {
		/* lines inserted */
		fd(d->added);
		return (strVal);
	}

	case KW_LD: /* LD */ {
		/* lines deleted */
		fd(d->deleted);
		return (strVal);
	}

	case KW_LU: /* LU */ {
		/* lines unchanged */
		fd(d->same);
		return (strVal);
	}

	case KW_Li: /* Li */ {
		/* lines inserted */
		f5d(d->added);
		return (strVal);
	}

	case KW_Ld: /* Ld */ {
		/* lines deleted */
		f5d(d->deleted);
		return (strVal);
	}

	case KW_Lu: /* Lu */ {
		/* lines unchanged */
		f5d(d->same);
		return (strVal);
	}

	case KW_DT: /* DT */ {
		/* delta type */
		if (TAG(d)) {
			if (SYMGRAPH(d)) {
				fc('T');
			} else {
				fc('R');
			}
		} else {
			fc('D');
		}
		return (strVal);
	}

	case KW_R: /* R */ {
		/* release */
		for (p = REV(s, d); *p && *p != '.'; )
			fc(*p++);
		return (strVal);
	}

	case KW_L: /* L */ {
		/* level */
		/* skip release field */
		for (p = REV(s, d); *p && *p != '.'; p++);
		for (p++; *p && *p != '.'; )
			fc(*p++);
		return (strVal);
	}

	case KW_B: /* B */ {
		/* branch */
		for (p = REV(s, d); *p && *p != '.'; p++); /* skip release field */
		for (p++; *p && *p != '.'; p++);	/* skip branch field */
		unless (*p) return (nullVal);
		for (p++; *p && *p != '.'; )
			fc(*p++);
		return (strVal);
	}

	case KW_S: /* S */ {
		/* sequence */
		for (p = REV(s, d); *p && *p != '.'; p++); /* skip release field */
		for (p++; *p && *p != '.'; p++);	/* skip branch field */
		unless (*p) return (nullVal);
		for (p++; *p && *p != '.'; p++);	/* skip level field */
		for (p++; *p; )
			fc(*p++);
		return (strVal);
	}

	case KW_Dy: /* Dy */ {
		/* year */
		char	val[16];

		delta_strftime(val, sizeof(val), YEAR4(s) ? "%Y" : "%y", s, d);
		fs(val);
		return (strVal);
	}

	case KW_Dm: /* Dm */ {
		/* month */
		char	val[16];

		delta_strftime(val, sizeof(val), "%m", s, d);
		fs(val);
		return (strVal);
	}

	case KW_DM: /* DM */ {
		/* month in Jan, Feb format */
		char	val[16];

		delta_strftime(val, sizeof(val), "%b", s, d);
		fs(val);
		return (strVal);
	}

	case KW_Dd: /* Dd */ {
		/* day */
		char	val[16];

		delta_strftime(val, sizeof(val), "%d", s, d);
		fs(val);
		return (strVal);
	}

	case KW_Th: /* Th */ {
		/* hour */
		char	val[16];

		delta_strftime(val, sizeof(val), "%H", s, d);
		fs(val);
		return (strVal);
	}

	case KW_Tm: /* Tm */ {
		/* minute */
		char	val[16];

		delta_strftime(val, sizeof(val), "%M", s, d);
		fs(val);
		return (strVal);
	}

	case KW_Ts: /* Ts */ {
		/* second */
		char	val[16];

		delta_strftime(val, sizeof(val), "%S", s, d);
		fs(val);
		return (strVal);
	}


	case KW_P: /* P */
	case KW_USER: /* USER */ {
		/* programmer */
		if ((p = USER(s, d)) && *p) {
			if (q = strchr(p, '/')) *q = 0;
			fs(p);
			if (q) *q = '/';
			return (strVal);
		}
		return (nullVal);
	}
	case KW_USERHOST: /* USERHOST */
		KW("USER");
		if (strchr(USERHOST(s, d), '@')) {
			fc('@');
			KW("HOST");
		}
		return (strVal);

	case KW_REALUSERHOST: /* REALUSERHOST */
		KW("REALUSER");
		if (strchr(USERHOST(s, d), '@')) {
			fc('@');
			KW("REALHOST");
		}
		return (strVal);

	case KW_FULLUSERHOST: /* FULLUSERHOST */
		fs(USERHOST(s, d));
		return (strVal);

	case KW_REALUSER: /* REALUSER */ {
		if ((p = USER(s, d)) && *p) {
			if (q = strchr(p, '/')) p = q + 1;
			fs(p);
			return (strVal);
		}
		return (nullVal);
	}
	case KW_FULLUSER: /* FULLUSER */ {
		if ((p = USER(s, d)) && *p) {
			fs(p);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_DS: /* DS */ {
		/* serial number */
		fd(SERIAL(s, d));
		return (strVal);
	}

	case KW_DP: /* DP */ {
		/* parent serial number */
		fd(d->pserial);
		return (strVal);
	}


	case KW_MR: /* MR */ {
		/* MR numbers for delta				*/
		/* not implemeted yet, return NULL for now	*/
		return (strVal);
	}

	case KW_C: /* C */ {
		/* comments */
		int	len;

		unless (t = COMMENTS(s, d)) return (nullVal);
		while (p = eachline(&t, &len)) fm(p, len);
		return (strVal);
	}

	case KW_HTML_C: /* HTML_C */ {
		char	html_ch[20];
		unsigned char *p;

		unless (p = COMMENTS(s, d)) return (nullVal);
		for (; *p; p++) {
			switch (*p) {
			    case '\n': fs("<br>"); break;
			    case '\t':
				fs("&nbsp;&nbsp;&nbsp;&nbsp;");
				fs("&nbsp;&nbsp;&nbsp;&nbsp;");
				break;
			    case ' ': fs("&nbsp;"); break;
			    default:
				if (isalnum(*p)) {
					fc(*p);
				} else {
					sprintf(html_ch, "&#%d;", *p);
					fs(html_ch);
				}
				break;
			}
		}
		return (strVal);
	}

	case KW_UN: /* UN */ {
		/* users name(s) */
		/* XXX this is a multi-line text field, definition unknown */
		fs("??");
		return (strVal);
	}

	case KW_FL: /* FL */ {
		/* flag list */
		/* XX TODO: ouput flags in symbolic names ? */
		fx(d->flags);
		return (strVal);
	}

	case KW_Y: /* Y */ {
		/* moudle type, not implemented */
		fs("");
		return (strVal);
	}

	case KW_MF: /* MF */ {
		/* MR validation flag, not implemented	*/
		fs("");
		return (strVal);
	}

	case KW_MP: /* MP */ {
		/* MR validation pgm name, not implemented */
		fs("");
		return (strVal);
	}

	case KW_KF: /* KF */ {
		/* keyword error warining flag	*/
		/* not implemented		*/
		fs("no");
		return (strVal);
	}

	case KW_KV: /* KV */ {
		/* keyword validation string	*/
		/* not impleemnted		*/
		return (nullVal);
	}

	case KW_BF: /* BF */ {
		/* branch flag */
		/* BitKeeper does not have a branch flag */
		/* but we can derive the value		 */
		if (d->r[0]) {
			int i;
			/* count the number of dot */
			for (i = 0, p = REV(s, d); *p && i <= 2; p++) {
				if (*p == '.') i++;
			}
			if (i == 2)  { /* if we have 2 dot, it is a branch */
				fs("yes");
			} else	{
				fs("no");
			}
		}
		return (strVal);
	}

	case KW_J: /* J */ {
		/* Join edit flag  */
		/* not implemented */
		fs("no");
		return (strVal);
	}

	case KW_LK: /* LK */ {
		/* locked releases */
		/* not implemented */
		return (nullVal);
	}

	case KW_Q: /* Q */ {
		/* User defined keyword */
		/* not implemented	*/
		return (nullVal);
	}

	case KW_M: /* M */ {
		/* XXX TODO: get the value from the	*/
		/* 'm' flag if/when implemented		*/
		fs(basenm(s->gfile));
		return (strVal);
	}

	case KW_FB: /* FB */ {
		/* floor boundary */
		/* not implemented */
		return (nullVal);
	}

	case KW_CB: /* CB */ {
		/* ceiling boundary */
		return (nullVal);
	}

	case KW_Ds: /* Ds */ {
		/* default branch or "none", see also DFB */
		if (s->defbranch) {
			fs(s->defbranch);
		} else {
			fs("none");
		}
		return (strVal);
	}

	case KW_ND: /* ND */ {
		/* Null delta flag */
		/* not implemented */
		fs("no");
		return (strVal);
	}

	case KW_FD: /* FD */ {
		/* file description text */
		int i = 0, j = 0;
		EACH(s->text) {
			if (s->text[i][0] == '\001') continue;
			j++;
			fs(s->text[i]);
			fc('\n');
		}
		if (j) return (strVal);
		return (nullVal);
	}

	case KW_ROOTLOG: /* ROOTLOG */ {
		int i, in_log = 0;

		/* note: ROOTLOG does not support $each(:ROOTLOG:){} */
		EACH(s->text) {
			if (s->text[i][0] == '\001') continue;
			unless (in_log) {
				if (streq(s->text[i], "@ROOTLOG")) in_log = 1;
				continue;
			}
			fs(s->text[i]);
			fc('\n');
			if (s->text[i][0] == '@') break;
		}
		if (in_log) return (strVal);
		return (nullVal);
	}

	case KW_BD: /* BD */ {
		/* Body text */
		/* XX TODO: figure out where to extract this info */
		fs("??");
		return (strVal);
	}

	case KW_GB: /* GB */ {
		/* Gotten body */
		sccs_restart(s);
		sccs_get(s, REV(s, d), 0, 0, 0, GET_EXPAND|SILENT|PRINT, "-");
		return (strVal);
	}

	/* Lose this in 2008 */
	case KW_Z: /* Z */ {
		fs("@(#)");
		return (strVal);
	}

	case KW_F: /* F */ {
		/* s file basename */
		if (s->sfile) {
			/* scan backward for '/' */
			for (p = s->sfile, q = &p[strlen(p) -1];
				(q > p) && (*q != '/'); q--);
			if (*q == '/') q++;
			fs(q);
		}
		return (strVal);
	}

	case KW_PN: /* PN */
	case KW_SFILE: /* SFILE */ {
		/* s file path */
		if (s->sfile) {
			fs(s->sfile);
			return (strVal);
		}
		return nullVal;
	}

	case KW_N: /* N */ {
		fd(s->numdeltas);
		return (strVal);
	}

	case KW_ODD: /* ODD */ {
		if (s->prs_odd) {
			fs("1");
			return (strVal);
		} else {
			return (nullVal);
		}
	}

	case KW_EVEN: /* EVEN */ {
		unless (s->prs_odd) {
			fs("1");
			return (strVal);
		} else {
			return (nullVal);
		}
	}

	case KW_JOIN: /* JOIN */ {
		unless (s->prs_join) {
			s->prs_join = 1;
			return (nullVal);
		}
		fs(",");
		return (strVal);
	}

	case KW_G: /* G */ {
		/* g file basename */
		if (s->gfile) {
			/* scan backward for '/' */
			for (p = s->gfile, q = &p[strlen(p) -1];
				(q > p) && (*q != '/'); q--);
			if (*q == '/') q++;
			fs(q);
		}
		return (strVal);
	}

	case KW_DSUMMARY: /* DSUMMARY */ {
	/* :DT: :I: :D: :T::TZ: :USERHOST: :DS: :DP: :Li:/:Ld:/:Lu: */
	 	KW("DT"); fc(' '); KW("I"); fc(' '); KW("D"); fc(' ');
		KW("T"); KW("TZ"); fc(' '); KW("USERHOST");
		fc(' ');
	 	KW("DS"); fc(' '); KW("DP"); fc(' '); KW("DL");
		return (strVal);
	}

	case KW_PATH: /* PATH */ {	/* $if(:DPN:){P :DPN:\n} */
		if (d->pathname) {
			fs("P ");
			KW("DPN");
			fc('\n');
			return (strVal);
		}
		return (nullVal);
	}

	/* $each(:TAG:){S (:TAG:)\n} */
	case KW_SYMBOLS: /* SYMBOLS */
	case KW_TAGS: /* TAGS */ {
		symbol	*sym;
		int	j = 0;

		unless (d && (d->flags & D_SYMBOLS)) return (nullVal);
		EACHP_REVERSE(s->symlist, sym) {
			unless (SERIAL(s, d) ==
			    (s->prs_all ? sym->meta_ser : sym->ser)) {
				continue;
			}
			j++;
			fs("S ");
			fs(SYMNAME(s, sym));
			fc('\n');
		}
		if (j) return (strVal);
		return (nullVal);
	}

	case KW_COMMENTS: /* COMMENTS */ {	/* $if(:C:){$each(:C:){C (:C:)}\n} */
		int	len;

		/* comments */
		/* XXX TODO: we may need to the walk the comment graph	*/
		/* to get the latest comment				*/
		unless (d->comments) return (nullVal);
		t = COMMENTS(s, d);
		while (p = eachline(&t, &len)) {
			fs("C ");
			fm(p, len);
			fc('\n');
		}
		return (strVal);
	}

	case KW_DEFAULT: /* DEFAULT */
	case KW_PRS: /* PRS */ {
		KW("DSUMMARY");
		fc('\n');
		KW("PATH");
		KW("SYMBOLS");
		KW("COMMENTS");
		fs("------------------------------------------------\n");
		return (strVal);
	}

	case KW_LOG: /* LOG */ {
		symbol	*sym;
		int	len;

		if (d->pathname) {
			KW("DPN");
		} else {
			fs(s->gfile);
		}
		fc(' ');
		KW("REV");
		fs("\n  ");
		KW("D_"); fc(' '); KW("T"); KW("TZ");
		fc(' '); KW("USERHOST");
		fc(' ');
		fc('+'); KW("LI"); fs(" -"); KW("LD"); fc('\n');
		t = COMMENTS(s, d);
		while (p = eachline(&t, &len)) {
			fs("  ");
			fm(p, len);
			fc('\n');
		}
		fc('\n');
		unless (d && (d->flags & D_SYMBOLS)) return (strVal);
		EACHP_REVERSE(s->symlist, sym) {
			unless (sym->ser == SERIAL(s, d)) continue;
			fs("  TAG: ");
			fs(SYMNAME(s, sym));
			fc('\n');
		}
		fc('\n');
		return (strVal);
	}

	case KW_CHANGESET: /* CHANGESET */ {
		if (CSET(s)) {
			fs(REV(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_RANDOM: /* RANDOM */ {
		if (s->tree->random) {
			fs(RANDOM(s, s->tree));
			return (strVal);
		}
		return nullVal;
	}

	case KW_ENC: /* ENC */ {
		switch (s->encoding_in & E_DATAENC) {
		    case E_ASCII:
			fs("ascii"); return (strVal);
		    case E_UUENCODE:
			fs("uuencode"); return (strVal);
		    case E_BAM:
			fs("BAM"); return (strVal);
		}
		return nullVal;
	}

	case KW_COMPRESSION: /* COMPRESSION */ {
		switch (s->encoding_in & E_COMP) {
		    case 0: 
			fs("none"); return (strVal);
		    case E_GZIP:
			fs("gzip"); return (strVal);
		}
		return nullVal;
	}

	case KW_VERSION: /* VERSION */ {
		fd(s->version);
		return (strVal);
	}

	case KW_X_FLAGS: /* X_FLAGS */ {
		char	buf[20];

		sprintf(buf, "0x%x", sccs_xflags(s, d));
		fs(buf);
		return (strVal);
	}

	case KW_X_XFLAGS: /* X_XFLAGS */ {
		char	buf[20];

		sprintf(buf, "0x%x", s->xflags);
		fs(buf);
		return (strVal);
	}

	case KW_FLAGS: /* FLAGS */
	case KW_XFLAGS: /* XFLAGS */ {
		int	flags =
		    (kwval->kwnum == KW_FLAGS) ? sccs_xflags(s, d) : s->xflags;

		fs(xflags2a(flags));
		unless (flags & (X_EOLN_NATIVE|X_EOLN_WINDOWS)) {
			fs(",EOLN_UNIX");
		}
		return (strVal);
	}

	case KW_REV: /* REV */ {
		fs(REV(s, d));
		return (strVal);
	}

	/* print the first rev at/below this which is in a cset */
	case KW_CSETREV: /* CSETREV */ {
		unless (d = sccs_csetBoundary(s, d)) return (nullVal);
		fs(REV(s, d));
		return (strVal);
	}

	case KW_CSETKEY: /* CSETKEY */ {
		char key[MAXKEY];
		unless (d->flags & D_CSET) return (nullVal);
		sccs_sdelta(s, d, key);
		fs(key);
		return (strVal);
	}

	case KW_HASHCOUNT: /* HASHCOUNT */ {
		int	n = sccs_hashcount(s);

		unless (HASH(s)) return (nullVal);
		fd(n);
		return (strVal);
	}

	case KW_MD5KEY: /* MD5KEY */ {
		char	b64[MD5LEN];

		sccs_md5delta(s, d, b64);
		fs(b64);
		return (strVal);
	}

	case KW_KEY: /* KEY */ {
		char	key[MAXKEY];

		sccs_sdelta(s, d, key);
		fs(key);
		return (strVal);
	}

	case KW_SORTKEY: /* SORTKEY */ {
		char	key[MAXKEY];

		sccs_sortkey(s, d, key);
		fs(key);
		return (strVal);
	}

	case KW_ROOTKEY: /* ROOTKEY */ {
		char key[MAXKEY];

		sccs_sdelta(s, sccs_ino(s), key);
		fs(key);
		return (strVal);
	}

	case KW_SYNCROOT: /* SYNCROOT */ {
		char key[MAXKEY];

		sccs_syncRoot(s, key);
		fs(key);
		return (strVal);
	}

	case KW_SHORTKEY: /* SHORTKEY */ {
		char	buf[MAXPATH+200];
		char	*t;

		sccs_sdelta(s, d, buf);
		if (t = sccs_iskeylong(buf)) *t = 0;
		fs(buf);
		return (strVal);
	}

	case KW_SYMBOL: /* SYMBOL */
	case KW_TAG: /* TAG */ {
		symbol	*sym;
		int	j = 0;

		unless (d && (d->flags & D_SYMBOLS)) return (nullVal);
		EACHP_REVERSE(s->symlist, sym) {
			unless (SERIAL(s, d) ==
			    (s->prs_all ? sym->meta_ser : sym->ser)) {
				continue;
			}
			j++;
			fs(SYMNAME(s, sym));
		}
		if (j) return (strVal);
		return (nullVal);
	}

	case KW_TAG_PSERIAL: /* TAG_PSERIAL */ {
		unless (d->ptag) return (nullVal);
		fd(d->ptag);
		return (strVal);
	}

	case KW_TAG_MSERIAL: /* TAG_MSERIAL */ {
		unless (d->mtag) return (nullVal);
		fd(d->mtag);
		return (strVal);
	}

	case KW_TAG_PREV: /* TAG_PREV */ {
		delta	*p;

		unless (d->ptag) return (nullVal);
		p = sfind(s, d->ptag);
		assert(p);
		fs(REV(s, p));
		return (strVal);
	}

	case KW_TAG_MREV: /* TAG_MREV */ {
		delta	*p;

		unless (d->mtag) return (nullVal);
		p = sfind(s, d->mtag);
		assert(p);
		fs(REV(s, p));
		return (strVal);
	}

	case KW_GFILE: /* GFILE */ {
		if (s->gfile) {
			fs(s->gfile);
		}
		return (strVal);
	}

	case KW_FILE: /* FILE */ {
		if (s->file) {
			fs(s->gfile);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_HT: /* HT */
	case KW_HOST: /* HOST */ {
		/* host without any importer name */
		if ((q = HOSTNAME(s, d)) && *q) {
			if (p = strchr(q, '/')) {
				*p = 0;
				fs(q);
				*p = '/';
			} else if (p = strchr(q, '[')) {
				*p = 0;
				fs(q);
				*p = '[';
			} else {
				fs(q);
			}
			return (strVal);
		}
		return (nullVal);
	}
	case KW_REALHOST: /* REALHOST */ {
		if ((q = HOSTNAME(s, d)) && *q) {
			if (p = strchr(q, '/')) {
				fs(p+1);
			} else if (p = strchr(q, '[')) {
				*p = 0;
				fs(q);
				*p = '[';
			} else {
				fs(q);
			}
			return (strVal);
		}
		return (nullVal);
	}
	case KW_FULLHOST: /* FULLHOST */ {
		if ((q = HOSTNAME(s, d)) && *q) {
			if (p = strchr(q, '[')) {
				*p = 0;
				fs(q);
				*p = '[';
			} else {
				fs(q);
			}
			return (strVal);
		}
		return (nullVal);
	}

	case KW_IMPORTER: /* IMPORTER */ {
		/* importer name */
		if ((p = HOSTNAME(s, d)) && (p = strchr(p, '['))) {
			while (*(++p) != ']') fc(*p);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_DOMAIN: /* DOMAIN */ {
		/* domain: the truncated domain name.
		 * Counting from the right, we keep zero or more
		 * two-letter components, then zero or one three
		 * letter component, then one more component
		 * irrespective of length; anything after that is
		 * nonsignificant.  If we run out of hostname before
		 * we run out of short components, the entire name is
		 * significant.  This heuristic is not always right
		 * (consider .to) but works most of the time.
		 */
		int i;

		unless ((q = HOSTNAME(s, d)) && *q) return (nullVal);
		p = &q[strlen(q)];
		do {
			i = 0;
			while (*--p != '.' && p != q) i++;
		} while (i == 2);
		if (i == 3) while (*--p != '.' && p != q);
		if (*p == '.') p++;
		fs(p);
		return (strVal);
	}

	case KW_TZ: /* TZ */ {
		/* time zone */
		if (d->zone) {
			fs(ZONE(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_TIME_T: /* TIME_T */ {
		char	buf[20];

		sprintf(buf, "%d", (int)d->date);
		fs(buf);
		return (strVal);
	}

	case KW_UTC: /* UTC */ {
		char	*utcTime;
		if (utcTime = sccs_utctime(s, d)) {
			fs(utcTime);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_UTC_FUDGE: /* UTC-FUDGE */ {
		char	*utcTime;

		d->date -= d->dateFudge;
		if (utcTime = sccs_utctime(s, d)) {
			fs(utcTime);
			d->date += d->dateFudge;
			return (strVal);
		}
		d->date += d->dateFudge;
		return (nullVal);
	}

	case KW_FUDGE: /* FUDGE */ {
		char	buf[20];

		sprintf(buf, "%d", (int)d->dateFudge);
		fs(buf);
		return (strVal);
	}

	case KW_AGE: /* AGE */ {	/* how recently modified */
		time_t	when = time(0) - (d->date - d->dateFudge);

		fs(age(when, " "));
		return (strVal);
	}

	case KW_HTML_AGE: /* HTML_AGE */ {	/* how recently modified */
		time_t	when = time(0) - (d->date - d->dateFudge);

		fs(age(when, "&nbsp;"));
		return (strVal);
	}

	case KW_DSUM: /* DSUM */ {
		if (d->flags & D_CKSUM) {
			fd((int)d->sum);
			return (strVal);
		}
		if (TAG(d)) {
			assert(d->sum == 0);
			fs("0");
			return (strVal);
		}
		return (nullVal);
	}

	case KW_FSUM: /* FSUM */ {
		unless (s->cksumdone) badcksum(s, SILENT);
		if (s->cksumok) {
			char	buf[20];

			sprintf(buf, "%d", (int)s->cksum);
			fs(buf);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_SYMLINK: /* SYMLINK */ {
		if (d->symlink) {
			fs(SYMLINK(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_MODE: /* MODE */ {
		char	buf[20];

		sprintf(buf, "%o", (int)d->mode);
		fs(buf);
		return (strVal);
	}

	case KW_RWXMODE: /* RWXMODE */ {
		char	buf[20];

		sprintf(buf, "%s", mode2a(d->mode));
		fs(buf);
		return (strVal);
	}

	case KW_TYPE: /* TYPE */ {
		if (BITKEEPER(s)) {
			fs("BitKeeper");
			if (CSET(s)) fs("|ChangeSet");
		} else {
			fs("SCCS");
		}
		return (strVal);
	}

	case KW_RENAME: /* RENAME */ {
		/* per delta path name if the pathname is a rename */
		unless (d->pathname &&
		    (!d->pserial ||
		    !streq(PATHNAME(s, d), PATHNAME(s, PARENT(s, d))))) {
			/* same, empty */
			return (nullVal);
		}
		/* different, fall into :DPN: */
	}

	case KW_DPN: /* DPN */ {
		/* per delta path name */
		if (CSET(s)) {
			if (s->prs_indentC) {
				fs(PATHNAME(s, d));
			} else {
				if (s->comppath) {
					fs(s->comppath);
					fc('/');
				}
				fs(basenm(PATHNAME(s, d)));
			}
			return (strVal);
		} else if (d->pathname) {
			if (s->comppath) {
				fs(s->comppath);
				fc('/');
			}
			fs(PATHNAME(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_SPN: /* SPN */ {
		/* per delta SCCS path name */
		if (d->pathname) {
			char	*p = name2sccs(PATHNAME(s, d));

			fs(p);
			free(p);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_MGP: /* MGP */ {
		/* merge parent's serial number */
		fd(d->merge);
		return (strVal);
	}

	case KW_PARENT: /* PARENT */ {
		if (d->pserial) {
			fs(REV(s, PARENT(s, d)));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_MPARENT: /* MPARENT */ {	/* print the merge parent if present */
		if (d->merge) {
			fs(REV(s, MERGE(s, d)));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_MERGE: /* MERGE */ {	/* print this rev if a merge node */
		if (d->merge) {
			fs(REV(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_GCA: /* GCA */ {		/* print gca rev if a merge node */
		if (d->merge && (d = gca(s, MERGE(s, d), PARENT(s, d)))) {
			fs(REV(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_GCA2: /* GCA2 */ {	/* print gca rev if a merge node */
		if (d->merge && (d = gca2(s, MERGE(s, d), PARENT(s, d)))) {
			fs(REV(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_SETGCA: /* SETGCA */ {	/* print gca rev if a merge node */
		char	*inc, *exc;

		if (d->merge &&
		    (d = gca3(s, MERGE(s, d), PARENT(s, d), &inc, &exc))) {
			fs(REV(s, d));
			if (inc) {
				fc('+');
				fs(inc);
				free(inc);
			}
			if (exc) {
				fc('-');
				fs(exc);
				free(exc);
			}
			return (strVal);
		}
		return (nullVal);
	}

	case KW_GET_SETGCA: /* GET_SETGCA */ {	/* print gca args for get*/
		char	*inc, *exc;

		if (d->merge &&
		    (d = gca3(s, MERGE(s, d), PARENT(s, d), &inc, &exc))) {
			fs("-r");
			fs(REV(s, d));
			if (inc) {
				fs(" -i");
				fs(inc);
				free(inc);
			}
			if (exc) {
				fs(" -x");
				fs(exc);
				free(exc);
			}
			return (strVal);
		}
		return (nullVal);
	}

	case KW_GET_SETGCA301: /* GET_SETGCA301 */ {	/* print gca args for get*/
		char	*inc, *exc;

		if (d->merge &&
		    (d = gca3(s, MERGE(s, d), PARENT(s, d), &inc, &exc))) {
			fs("-r");
			fs(REV(s, d));
			if (inc) free(inc);
			if (exc) free(exc);
			return (strVal);
		}
		return (nullVal);
	}
	case KW_GET_SETGCA302: /* GET_SETGCA302 */ {	/* print gca args for get*/
		char	*inc, *exc;

		if (d->merge &&
		    (d = gca3(s, MERGE(s, d), PARENT(s, d), &inc, &exc))) {
			fs("-r");
			fs(REV(s, d));
			if (inc) {
				fs(" -M");
				fs(inc);
				free(inc);
			}
			if (exc) {
				fs(" -i");
				fs(exc);
				free(exc);
			}
			return (strVal);
		}
		return (nullVal);
	}

	case KW_PREV: /* PREV */ {
		if (NEXT(d)) {
			fs(REV(s, NEXT(d)));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_NEXT: /* NEXT */ {
		if (d = sccs_prev(s, d)) {
			fs(REV(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_KID: /* KID */ {
		if (d = sccs_kid(s, d)) {
			fs(REV(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_KIDS: /* KIDS */ {
		int	space = 0;
		delta	*m;

		for (m = s->table; m; m = NEXT(m)) {
			if ((m->merge == SERIAL(s, d)) ||
			    (m->pserial == SERIAL(s, d))) {
				if (space) fs(" ");
				fs(REV(s, m));
				space = 1;
			}
		}
		return (space ? strVal : nullVal);
	}

	case KW_TIP: /* TIP */ {
		if (sccs_isleaf(s, d)) {
			fs(REV(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_SIBLINGS: /* SIBLINGS */ {
		/*
		 * SIBLINGS is convoluted in non-bk case
		 * as kid in teamware is not necessarily oldest
		 */
		unless (s->kidlist) sccs_mkKidList(s);
		if (d = SIBLINGS(s, d)) {
			fs(REV(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_DFB: /* DFB */ {
		/* default branch */
		if (s->defbranch) {
			fs(s->defbranch);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_DANGLING: /* DANGLING */ {
		/* don't clause on MONOTONIC, we had a bug there, see chgset */
		if (DANGLING(d)) {
			fs(REV(s, d));
			return (strVal);
		}
		return (nullVal);
	}

	case KW_RREV: /* RREV */ {
		names	*n;

		unless (s->rrevs) {
			s->rrevs = res_getnames(sccsXfile(s, 'r'), 'r');
		}
		n = (names *)s->rrevs;
		if (n && n->remote) {
			fs(n->remote);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_LREV: /* LREV */ {
		names	*n;

		unless (s->rrevs) {
			s->rrevs = res_getnames(sccsXfile(s, 'r'), 'r');
		}
		n = (names *)s->rrevs;
		if (n && n->local) {
			fs(n->local);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_GREV: /* GREV */ {
		names	*n;

		unless (s->rrevs) {
			s->rrevs = res_getnames(sccsXfile(s, 'r'), 'r');
		}
		n = (names *)s->rrevs;
		if (n && n->gca) {
			fs(n->gca);
			return (strVal);
		}
		return (nullVal);
	}

	case KW_RPN: /* RPN */ {
		names	*n;

		unless (s->rrevs) {
			s->rrevs = res_getnames(sccsXfile(s, 'r'), 'r');
		}
		n = (names *)s->rrevs;
		if (n && n->remote &&
		    (d = findrev(s, n->remote)) && d->pathname) {
			KW("DPN");
			return (strVal);
		}
		return (nullVal);
	}

	case KW_LPN: /* LPN */ {
		names	*n;

		unless (s->rrevs) {
			s->rrevs = res_getnames(sccsXfile(s, 'r'), 'r');
		}
		n = (names *)s->rrevs;
		if (n && n->local &&
		    (d = findrev(s, n->local)) && d->pathname) {
			KW("DPN");
			return (strVal);
		}
		return (nullVal);
	}

	case KW_GPN: /* GPN */ {
		names	*n;

		unless (s->rrevs) {
			s->rrevs = res_getnames(sccsXfile(s, 'r'), 'r');
		}
		n = (names *)s->rrevs;
		if (n && n->gca && (d = findrev(s, n->gca)) && d->pathname) {
			KW("DPN");
			return (strVal);
		}
		return (nullVal);
	}

	case KW_DIFFS: /* DIFFS */
	case KW_DIFFS_U: /* DIFFS_U */
	case KW_DIFFS_UP: /* DIFFS_UP */ {
		int	kind;
		int	open = (s->state & S_SOPEN);

		switch (kwval->kwnum) {
		    default:
		    case KW_DIFFS:	kind = DF_DIFF; break;
		    case KW_DIFFS_U:	kind = DF_UNIFIED; break;
		    case KW_DIFFS_UP:	kind = DF_UNIFIED|DF_GNUp; break;
		}
		if (d == s->tree) return (nullVal);
		unless (open) sccs_open(s, 0);
		sccs_diffs(s, REV(s, d), REV(s, d), SILENT, kind, out);
		unless (open) sccs_close(s);
		return (strVal);
	}

	/* don't document, TRUE/FALSE are for t.prs */
	case KW_FALSE: /* FALSE */
		return (nullVal);

	case KW_TRUE: /* TRUE */
		fs("1");
		return (strVal);

	case KW_ID: /* ID */
		if (d->csetFile) {
			fs(CSETFILE(s, d));
			return (strVal);
		}
		return (nullVal);

	case KW_PRODUCT_ID: /* PRODUCT_ID */ {
		project *proj;

		/*
		 * The reason this is not just
		 * if (proj = proj_product(s->proj))
		 * is that if there is a standalone copied into a product
		 * it will find the product that way.  We want this to
		 * return the value if and only if it is our product.
		 */
		if (proj_isProduct(s->proj)) {
			proj = s->proj;
		} else if (proj_isComponent(s->proj)) {
			proj = proj_product(s->proj);
		} else {
			return (nullVal);
		}
		fs(proj_rootkey(proj));
		return (strVal);
	}

	case KW_CSETFILE: /* CSETFILE */  /* compat */
	case KW_PACKAGE_ID: /* PACKAGE_ID */ {
		/*
		 * Rootkey of repository containing file
		 * Note that component->proj used to vary based on how it
		 * was inited, from product or from component; that is
		 * no longer the case so this code is simple.
		 */
		if (s->proj) {
			fs(proj_rootkey(s->proj));
			return (strVal);
		} else {
			return (nullVal);
		}
	}
	case KW_ATTACHED_ID: /* ATTACHED_ID */ {
		project *proj = s->proj;

		if (proj) {
			if (CSET(s) && proj_isComponent(proj)) {
				proj = proj_product(proj);
			}
			fs(proj_rootkey(proj));
			return (strVal);
		} else {
			return (nullVal);
		}
	}

	case KW_REPO_ID: /* REPO_ID */
		/* repo_id of repository containing file */
		if  (s->proj) {
			fs(proj_repoID(s->proj));
			return (strVal);
		} else {
			return (nullVal);
		}

	case KW_REPOTYPE: /* REPOTYPE */
		if (proj_isComponent(s->proj)) {
			fs("component");
		} else if (proj_isProduct(s->proj)) {
			fs("product");
		} else if (s->proj) {
			fs("standalone");
		} else {
			fs("SCCS");
		}
		return (strVal);

	case KW_BAMHASH: /* BAMHASH */
		if (d->bamhash) {
			fs(BAMHASH(s, d));
			return (strVal);
		}
		return (nullVal);

	case KW_BAMSIZE: /* BAMSIZE */
		if (d->bamhash) {
			// XXX - BAMSIZE could be much bigger than 4G
			fu(d->added);
			return (strVal);
		}
		return (nullVal);

	case KW_CSET_MD5ROOTKEY: /* CSET_MD5ROOTKEY */
		/* md5rootkey of repository containing file */
		if (s->proj) {
			fs(proj_md5rootkey(s->proj));
			return (strVal);
		} else {
			return (nullVal);
		}

	case KW_BAMENTRY: /* BAMENTRY */
		if (BAM(s) && (p = bp_lookup(s, d))) {
			q = bp_dataroot(s->proj, 0);
			assert(q);
			t = p + strlen(q) + 1;
			free(q);
			fs(t);
			free(p);
			return (strVal);
		}
		return (nullVal);
	case KW_BAMFILE: /* BAMFILE */
		if (BAM(s) && (p = bp_lookup(s, d))) {
			project	*prod = proj_product(s->proj);

			/* XXX: what should do if non-prod RESYNC? */
			unless (prod) prod = s->proj;
			q = proj_relpath(prod, p);
			free(p);
			fs(q);
			free(q);
			return (strVal);
		}
		return (nullVal);
	case KW_BAM: /* BAM */
		if (BAM(s)) {
			fs("1");
			return (strVal);
		} else {
			return (nullVal);
		}
	case KW_BAMLOG: /* BAMLOG */
		if (BAM(s) && (p = bp_lookup(s, d))) {
			char	key[MAXKEY];
			char	b64[MD5LEN];

			sccs_sdelta(s, d, key);
			sccs_md5delta(s, s->tree, b64);
			q = bp_dataroot(s->proj, 0);
			assert(q);
			t = p + strlen(q) + 1;
			free(q);
			q = aprintf("%s %s %s %s", BAMHASH(s, d), key, b64, t);
			fs(q);
			fc(' ');
			sprintf(key, "%08x", (u32)adler32(0, q, strlen(q)));
			fs(key);
			free(p);
			free(q);
			return (strVal);
		}
		return (nullVal);

	case KW_SPACE: /* SPACE */
		fc(' ');
		return (strVal);
	case KW_COMPONENT: /* COMPONENT */
		if (q = proj_comppath(s->proj)) {
			fs(q);
			fc('/');
			return (strVal);
		} else {
			return (nullVal);
		}
	case KW_COMPONENT_V: /* COMPONENT_V */
	        if (s->prs_indentC && (q = proj_comppath(s->proj))) {
			fs(q);
			fc('/');
			return (strVal);
		} else {
			return (nullVal);
		}
	case KW_INDENT: /* INDENT */
		if (s->prs_indentC && proj_isComponent(s->proj)) fs("  ");
		unless (CSET(s)) fs("  ");
		return (strVal);
	case KW_RM_NAME: /* RM_NAME */ {
		char	key[MAXKEY];

		sccs_sdelta(s, sccs_ino(s), key);
		p = key2rmName(key);
		fs(p);
		free(p);
		return (strVal);
	}
	case KW_UNRM_NAME: /* UNRM_NAME */
		/*
		 * XXX: loose interpretation of history: while older, not
		 * necessarily in the same ancestory.  Good enough for tip?!
		 */
		for (; d; d = NEXT(d)) {
			unless (strneq(PATHNAME(s, d),
				"BitKeeper/deleted/", 18)) {
				fs(PATHNAME(s, d));
				return (strVal);
			}
		}
		return (nullVal);

	case KW_ATTR_LICENSE: /* ATTR_LICENSE */
	case KW_ATTR_VERSION: /* ATTR_VERSION */
	case KW_ATTR_ID: /* ATTR_ID */
	case KW_ATTR_HERE: /* ATTR_HERE */
	case KW_ATTR_TEST: /* ATTR_TEST */
	{
		int	cnt, i;
		FILE	*f;
		char	buf[MAXLINE];
		char	cmd[MAXLINE];

		unless (CSET(s)) return (nullVal);

		/*
		 * Return attribute for a given cset
		 * XXX: Like :ROOTLOG:, doesn't do $each()
		 *
		 * This version is not efficient. Each fetch must walk
		 * the ChangeSet file weave and the weave of the attr
		 * file.  This can be cached and made fast in the future.
		 * Do an all-rev cset walk like checksum.c and then cache
		 * the mapping from csetkey->attribkey.  Then the same walk on
		 * on attr file can save the values.
		 */
		sccs_sdelta(s, d, buf);
		sprintf(cmd,
		    "bk -R get -qp -r@'%s' " ATTR "| bk _getkv - %.*s",
		    buf, len - 5, kw + 5);	// yuck - strlen(ATTR)
		cnt = 0;
		if (f = popen(cmd, "r")) {
			while ((i = fread(buf, 1, sizeof(buf), f)) > 0) {
				fm(buf, i);
				cnt += i;
			}
			pclose(f);
		}
		return (cnt ? strVal : nullVal);
	}
	default:
		return (notKeyword);
	}
}

int
sccs_prsdelta(sccs *s, delta *d, int flags, char *dspec, FILE *out)
{
	if (TAG(d) && !(flags & (PRS_ALL|PRS_FORCE))) return (0);
	if (SET(s) && !(d->flags & D_SET) && !(flags & PRS_FORCE)) return (0);
	s->prs_all = ((flags & PRS_ALL) != 0);
	s->prs_output = 0;
	dspec_eval(out, s, d, dspec);
	if (s->prs_output) {
		s->prs_odd = !s->prs_odd;
		if (flags & PRS_LF) fputc('\n', out);
	}
	return (s->prs_output);
}

char *
sccs_prsbuf(sccs *s, delta *d, int flags, char *dspec)
{
	FILE	*f;

	f = fmem();
	sccs_prsdelta(s, d, flags, dspec, f);
	return (fmem_close(f, 0)); /* FYI: returns "" if empty */
}


/*
 * Write per-file data to bk patch
 *
 * Order is
 *	f d default
 *	f e encoding
 *	f x bitkeeper bits
 *	R random
 *	T descriptive text
 *	T descriptive text
 *	...
 */
void
sccs_perfile(sccs *s, FILE *out)
{
	int	i;
	int	enc;

	if (s->defbranch) fprintf(out, "f d %s\n", s->defbranch);
	/* Make a patch according to env; not how this file is stored */
	unless (enc = s->encoding_out) {
		enc = sccs_encoding(s, 0, 0);
	}
	enc &= ~E_ALWAYS;
	if (enc) fprintf(out, "f e %d\n", enc);
	EACH(s->text) fprintf(out, "T %s\n", s->text[i]);
	if (s->version) fprintf(out, "V %u\n", s->version);
	fprintf(out, "\n");
}

#define	FLAG(c)	((buf[0] == 'f') && (buf[1] == ' ') &&\
		(buf[2] == c) && (buf[3] == ' '))

sccs	*
sccs_getperfile(sccs *s_in, MMAP *in, int *lp)
{
	sccs	*s;
	int	unused = 1;
	char	*buf;

	s = s_in ? s_in : new(sccs);
	unless (buf = mkline(mnext(in))) goto err;
	unless (buf[0]) {
		unless (s_in) free(s);
		return (0);
	}
	(*lp)++;
	if (FLAG('d')) {
		unused = 0;
		s->defbranch = strdup(&buf[4]);
		unless (buf = mkline(mnext(in))) {
err:			fprintf(stderr,
			    "takepatch: file format error near line %d\n", *lp);
			unless (s_in) free(s);
			return (0);
		}
		(*lp)++;
	}
	if (FLAG('e')) {
		unused = 0;
		s->encoding_in |= atoi(&buf[4]) | E_ALWAYS;
		unless (buf = mkline(mnext(in))) goto err; (*lp)++;
	}
	if (FLAG('x')) {
		/* Ignored */
		unless (buf = mkline(mnext(in))) goto err; (*lp)++;
	}
	while (strneq(buf, "T ", 2)) {
		unused = 0;
		s->text = addLine(s->text, strdup(&buf[2]));
		unless (buf = mkline(mnext(in))) goto err; (*lp)++;
	}
	if (strneq(buf, "V ", 2)) {
		unused = 0;
		s->version = atoi(&buf[2]);
		unless (buf = mkline(mnext(in))) goto err; (*lp)++;
	}
	unless (s->version) s->version = SCCS_VERSION;
	if (buf[0]) goto err;		/* should be empty */

	if (unused) {
		unless (s_in) free(s);
		return (0);
	}
	return (s);
}

private int
do_patch(sccs *s, delta *d, int flags, FILE *out)
{
	int	i;	/* used by EACH */
	symbol	*sym;
	char	type;
	char	*p, *t;
	delta	*e;
	int	len, sign;

	if (!d) return (0);
	type = TAG(d) ? 'R' : 'D';
	if ((type == 'R') &&
	    PARENT(s, d) && streq(REV(s, d), REV(s, PARENT(s, d)))) {
	    	type = 'M';
	}

	fprintf(out, "%c %s %s%s %s +%u -%u",
	    type, REV(s, d), delta_sdate(s, d),
	    ZONE(s, d),
	    USERHOST(s, d),
	    d->added, d->deleted);
	if (flags & PRS_FASTPATCH) fprintf(out, " =%u", d->same);
	fputs("\n", out);

	/*
	 * Order from here down is alphabetical.
	 */
	if (d->bamhash) fprintf(out, "A %s\n", BAMHASH(s, d));
	if (d->csetFile) fprintf(out, "B %s\n", CSETFILE(s, d));
	if (d->flags & D_CSET) fprintf(out, "C\n");
	if (DANGLING(d)) fprintf(out, "D\n");

	t = COMMENTS(s, d);
	while (p = eachline(&t, &len)) fprintf(out, "c %.*s\n", len, p);
	if (d->dateFudge) fprintf(out, "F %d\n", (int)d->dateFudge);
	p = CLUDES(s, d);
	while (i = sccs_eachNum(&p, &sign)) {
		unless (sign > 0) continue;
		e = sfind(s, i);
		assert(e);
		fprintf(out, "i ");
		sccs_pdelta(s, e, out);
		fprintf(out, "\n");
	}
	if (d->flags & D_CKSUM) {
		fprintf(out, "K %u", d->sum);
		if ((d->flags & D_SORTSUM) && (d->sum != d->sortSum)) {
			fprintf(out, "|%u", d->sortSum);
		}
		fputc('\n', out);
	}
	if (d->merge) {
		e = MERGE(s, d);
		assert(e);
		fprintf(out, "M ");
		sccs_pdelta(s, e, out);
		fprintf(out, "\n");
	}
	if (d->flags & D_MODE) {
		fprintf(out, "O %s", mode2a(d->mode));
		if (S_ISLNK(d->mode)) {
			assert(d->symlink);
			fprintf(out, " %s\n", SYMLINK(s, d));
		} else {
			fprintf(out, "\n");
		}
	}
	if (s->tree->pathname) assert(d->pathname);
	if (d->pathname) {
		if (d->sortPath) {
			fprintf(out, "P %s|%s\n",
			    PATHNAME(s, d), SORTPATH(s, d));
		} else {
			fprintf(out, "P %s\n", PATHNAME(s, d));
		}
	}
	if (d->random) fprintf(out, "R %s\n", RANDOM(s, d));
	if ((d->flags & D_SYMBOLS) || SYMGRAPH(d)) {
		EACHP_REVERSE(s->symlist, sym) {
			unless (sym->meta_ser == SERIAL(s, d)) continue;
			fprintf(out, "S %s\n", SYMNAME(s, sym));
		}
		if (SYMGRAPH(d)) fprintf(out, "s g\n");
		if (SYMLEAF(d)) fprintf(out, "s l\n");
		if (d->ptag) {
			delta	*e = sfind(s, d->ptag);
			char	buf[MAXKEY];

			assert(e);
			sccs_sdelta(s, e, buf);
			fprintf(out, "s %s\n", buf);
		}
		if (d->mtag) {
			delta	*e = sfind(s, d->mtag);
			char	buf[MAXKEY];

			assert(e);
			sccs_sdelta(s, e, buf);
			fprintf(out, "s %s\n", buf);
		}
	}
	if ((flags & PRS_GRAFT) && s->version) {
		fprintf(out, "V %u\n", s->version);
	}
	p = CLUDES(s, d);
	while (i = sccs_eachNum(&p, &sign)) {
		unless (sign < 0) continue;
		e = sfind(s, i);
		assert(e);
		fprintf(out, "x ");
		sccs_pdelta(s, e, out);
		fprintf(out, "\n");
	}
	if (d->flags & D_XFLAGS) {
		assert((d->xflags & X_EOLN_UNIX) == 0);
		fprintf(out, "X 0x%x\n", d->xflags);
	}
	if (s->tree->zone) assert(d->zone);
	fprintf(out, "------------------------------------------------\n");
	return (0);
}

private void
prs_reverse(sccs *s, int flags, char *dspec, FILE *out)
{
	delta	*d;
	int	ser;
	
	for (ser = 1; ser < s->nextserial; ser++) {
		unless ((d = sfind(s, ser)) && (d->flags & D_SET)) continue;
		if (sccs_prsdelta(s, d, flags, dspec, out) && s->prs_one) {
			return;
		}
	}
}

private void
prs_forward(sccs *s, int flags, char *dspec, FILE *out)
{
	delta	*d;

	for (d = s->table; d; d = NEXT(d)) {
		unless (d->flags & D_SET) continue;
		if (sccs_prsdelta(s, d, flags, dspec, out) && s->prs_one) {
			return;
		}
	}
}

int
sccs_prs(sccs *s, u32 flags, int reverse, char *dspec, FILE *out)
{
	delta	*d;

	if (!dspec) dspec = ":DEFAULT:";
	s->prs_odd = 0;
	s->prs_join = 0;
	GOODSCCS(s);
	if (flags & PRS_PATCH) {
		assert(s->rstart == s->rstop);
		return (do_patch(s, s->rstart, flags, out));
	}
	unless (SET(s)) {
		for (d = s->rstop; d; d = NEXT(d)) {
			d->flags |= D_SET;
			if (d == s->rstart) break;
		}
	}
	if (reverse) {
		 prs_reverse(s, flags, dspec, out);
	} else {
		 prs_forward(s, flags, dspec, out);
	}

	if (KV(s) && s->mdbm) {
		mdbm_close(s->mdbm);
		s->mdbm = 0;
	}
	return (0);
}

private inline int
samekeystr(char *a, char *b)
{
	char	*parts_a[6], *parts_b[6];

	debug((stderr, "samekeystr(%s, %s)\n", a, b));
	/* tight compare */
	if (streq(a, b)) return (1);

	/* loose compare */
	explodeKey(a, parts_a);
	explodeKey(b, parts_b);
	debug((stderr, "samekeystr: parts_a:%s, %s, %s, %s, %s, %s\n",
		parts_a[0],
		parts_a[1] ? parts_a[1] : "NULL",
		parts_a[2],
		parts_a[3],
		parts_a[4] ? parts_a[4] : "NULL",
		parts_a[5] ? parts_a[5] : "NULL"));
	debug((stderr, "samekeystr: parts_b:%s, %s, %s, %s, %s, %s\n",
		parts_b[0],
		parts_b[1] ? parts_b[1] : "NULL",
		parts_b[2],
		parts_b[3],
		parts_b[4] ? parts_b[4] : "NULL",
		parts_b[5] ? parts_b[5] : "NULL"));

	/* user */
	assert(parts_a[0] && parts_b[0]);
	unless (streq(parts_a[0], parts_b[0])) return(0);
	/* host (may not be there; if not, then not in both) */
	unless ((!parts_a[1] && !parts_b[1]) ||
		(parts_a[1] && parts_b[1]))    return(0);
	unless (!parts_a[1] || streq(parts_a[1], parts_b[1]))
		return (0);
	/* pathname (might be zero length, but will be there */
	assert(parts_a[2] && parts_b[2]);
	unless (streq(parts_a[2], parts_b[2])) return(0);
	/* utc + fudge timestamp */
	assert(parts_a[3] && parts_b[3]);
	unless (streq(parts_a[3], parts_b[3])) return(0);
	/* chksum (or almostUnique if null delta) */
	if (parts_a[4] && parts_b[4])
		unless (streq(parts_a[4], parts_b[4])) return (0);
	/* if 1.0 node, then random number */
	if (parts_a[5] && parts_b[5])
		unless (streq(parts_a[5], parts_b[5])) return (0);
	return (1);
}

/*
 * This gets a GCA which tends to be on the trunk.
 * Because it doesn't look up the mparent path, it tends not to get
 * the closest gca.
 */
private delta *
gca(sccs *s, delta *left, delta *right)
{
	delta	*d;

	unless (left && right) return (0);
	/*
	 * Clear the visited flag up to the root via one path,
	 * set it via the other path, then go look for it.
	 */
	for (d = left; d; d = PARENT(s, d)) d->flags &= ~D_RED;
	for (d = right; d; d = PARENT(s, d)) d->flags |= D_RED;
	for (d = left; d; d = PARENT(s, d)) {
		if (d->flags & D_RED) return (d);
	}
	return (0);
}

private delta *
gca2(sccs *s, delta *left, delta *right)
{
	delta	*d;
	u8	*slist;
	int	value;

	unless (s && s->nextserial && left && right) return (0);

	slist = calloc(s->nextserial, sizeof(u8));
	slist[SERIAL(s, left)] |= 1;
	slist[SERIAL(s, right)] |= 2;
	d = (left > right) ? left : right;
	for ( ; d ; d = NEXT(d)) {
		if (TAG(d)) continue;
		unless (value = slist[SERIAL(s, d)]) continue;
		if (value == 3) break;
		if (d->pserial) slist[d->pserial] |= value;
		if (d->merge)   slist[d->merge] |= value;
	}
	free(slist);
	return (d);
}

/*
 * compute a delta +i -x represtation of a list of the gca deltas
 *
 * A better interface would be to return the gca list and
 * have 'get' work with a list:
 * sccs_get(sccs *s, char **delta, ..)
 * bk get -r<rev1>,<rev2>,<rev3> foo.c
 */
private delta *
gca3(sccs *s, delta *left, delta *right, char **inc, char **exc)
{
	delta	*ret = 0;
	delta	*gca;
	u8	*gmap = 0;
	char	**glist, **list;
	int	count;

	*inc = *exc = 0;
	unless (s && s->nextserial && left && right) return (0);

	list = addLine(0, left);
	list = addLine(list, right);
	glist = range_gcalist(s, list);
	freeLines(list, 0);
	count = nLines(glist);
	assert(count);
	gca = (delta *)glist[1];
	if (count > 1) {
		gmap = (u8 *)calloc(s->nextserial, sizeof(u8));
		graph_symdiff(s, (delta *)glist, 0, gmap, 0, -1, SD_MERGE);
		if (compressmap(s, gca, gmap, inc, exc)) {
			goto bad;
		}
	}
	ret = gca;

bad:	if (gmap) free (gmap);
	freeLines(glist, 0);
	return (ret);
}

delta	*
sccs_gca(sccs *s, delta *left, delta *right, char **inc, char **exc)
{
	return (gca3(s, left, right, inc, exc));
}

/*
 * Find the two tips of the graph so they can be closed by a merge.
 */
int
sccs_findtips(sccs *s, delta **a, delta **b)
{
	delta	*d;

	*a = *b = 0;

	/*
	 * b is that branch which needs to be merged.
	 * At any given point there should be exactly one of these.
	 */
	for (d = s->table; d; d = NEXT(d)) {
		if (TAG(d)) continue;
		unless (isleaf(s, d)) continue;
		if (!*a) {
			*a = d;
		} else {
			assert(!*b);
			*b = d;
			/* Could break but I like the error checking */
		}
	}
	return (*b != 0);
}

/*
 * Create resolve file.
 * The order of the deltas in the file is important - the "branch"
 * should be last.
 * This currently only works for the trunk (i.e., there is one LOD).
 * XXX - this is also where we would handle pathnames, symbols, etc.
 */
int
sccs_resolveFiles(sccs *s)
{
	FILE	*f = 0;
	delta	*p, *g = 0, *a = 0, *b = 0;
	char	*n[3];
	int	retcode = -1;

	if (s->defbranch) {
		fprintf(stderr, "resolveFiles: defbranch set.  "
			"LODs are no longer supported.\n"
			"Please run 'bk support' to request "
			"assistance.\n");
err:
		return (retcode);
	}

	/*
	 * If we have no conflicts, then make sure the paths are the same.
	 * What we want to compare is whatever the tip path is with the
	 * whatever the path is in the most recent delta.
	 */
	unless (sccs_findtips(s, &a, &b)) {
		for (p = s->table; p; p = NEXT(p)) {
			if (!TAG(p) && !(p->flags & D_REMOTE)) {
				break;
			}
		}
		if (!p || streq(PATHNAME(s, p), PATHNAME(s, a))) {
			return (0);
		}
		b = a;
		a = g = p;
		retcode = 0;
		goto rename;
	} else {
		if (a->r[0] != b->r[0]) {
			fprintf(stderr, "resolveFiles: Found tips on "
			    "different LODs.\n"
			    "LODs are no longer supported.\n"
			    "Please run 'bk support' to "
			    "request assistance.\n");
			goto err;
		}
		retcode = 1;
	}

	g = gca2(s, a, b);
	assert(g);

	unless (f = fopen(sccsXfile(s, 'r'), "w")) {
		perror("r.file");
		goto err;
	}
	/*
	 * Always put the local stuff on the left, if there
	 * is any.
	 */
	unless (a->flags & D_REMOTE) {
		s->local = a;
		s->remote = b;
		fprintf(f, "merge deltas %s %s %s %s %s\n",
		    REV(s, a), REV(s, g), REV(s, b),
		    sccs_getuser(), time2date(time(0)));
	} else {
		s->local = b;
		s->remote = a;
		fprintf(f, "merge deltas %s %s %s %s %s\n",
		    REV(s, b), REV(s, g), REV(s, a),
		    sccs_getuser(), time2date(time(0)));
	}
	fclose(f);
	unless (streq(PATHNAME(s, g), PATHNAME(s, a)) &&
	    streq(PATHNAME(s, g), PATHNAME(s, b))) {
 rename:	n[1] = name2sccs(PATHNAME(s, g));
		unless (b->flags & D_REMOTE) {
			n[2] = name2sccs(PATHNAME(s, a));
			n[0] = name2sccs(PATHNAME(s, b));
		} else {
			n[0] = name2sccs(PATHNAME(s, a));
			n[2] = name2sccs(PATHNAME(s, b));
		}
		unless (f = fopen(sccsXfile(s, 'm'), "w")) {
			perror("m.file");
			goto err;
		}
		fprintf(f, "rename %s|%s|%s\n", n[0], n[1], n[2]);
		fclose(f);
		free(n[0]);
		free(n[1]);
		free(n[2]);
	}
	/* retcode set above */
	return (retcode);
}

/*
 * Take a key like sccs_sdelta makes and find it in the tree.
 */
int
sccs_istagkey(char *key)
{
	char	*parts[6];	/* user, host, path, date as integer */
	char	buf[MAXKEY];

	strcpy(buf, key);
	explodeKey(buf, parts);
	unless (parts[4] && atoi(parts[4])) return (1);
	return (0);
}

/*
 * Find an MD5 based key. Uses the findkeydb to be fast.
 */
delta	*
sccs_findMD5(sccs *s, char *md5)
{
	delta	*d;
	u32	dd;
	char	dkey[MAXKEY];

	unless (s && HASGRAPH(s)) return (0);

	unless (s->findkeydb) {
		s->findkeydb = hash_new(HASH_MEMHASH);
		for (d = s->table; d; d = NEXT(d)) {
			sccs_findKeyUpdate(s, d);
		}
	}

	strncpy(dkey, md5, 8);
	dkey[8] = 0;
	dd = strtoul(dkey, 0, 16);

	unless (hash_fetch(s->findkeydb, &dd, sizeof(dd))) return (0);
	d = SFIND(s, *(ser_t *)s->findkeydb->vptr);
	for (; d && (dd == d->date); d = NEXT(d)) {
		sccs_md5delta(s, d, dkey);
		if (streq(md5, dkey)) return (d);
	}
	return (0);
}

int
isKey(char *key)
{
	int	i;

	if (strchr(key, '|')) return (1);
	if (isxdigit(key[0]) && (strlen(key) == 30)) {
		for (i = 1; i < 8; i++) unless (isxdigit(key[i])) return (0);
		for (; i < 30; i++) {
			unless (isalnum(key[i]) || (key[i] == '-') || (key[i] == '_')) {
				return (0);
			}
		}
		return (1);
	}
	return (0);
}

/*
 * Take a key like sccs_sdelta makes and find it in the tree.
 */
delta *
sccs_findKey(sccs *s, char *key)
{
	delta	*d;
	char	*t;
	u32	dd;
	char	dkey[MAXKEY];

	unless (s && HASGRAPH(s)) return (0);
	debug((stderr, "findkey(%s)\n", key));
	unless (strchr(key, '|')) return (sccs_findMD5(s, key));

	unless (s->findkeydb) {
		s->findkeydb = hash_new(HASH_MEMHASH);
		for (d = s->table; d; d = NEXT(d)) {
			sccs_findKeyUpdate(s, d);
		}
	}

	unless (t = strchr(key, '|')) return (0);	/* path */
	unless (t = strchr(t+1, '|')) return (0);	/* date */

	dd = sccs_date2time(t+1, 0);
	unless (hash_fetch(s->findkeydb, &dd, sizeof(dd))) return (0);
	d = SFIND(s, *(ser_t *)s->findkeydb->vptr);
	for (; d && (dd == d->date); d = NEXT(d)) {
		sccs_sdelta(s, d, dkey);
		if (s->keydb_nopath) {
			if (!keycmp_nopath(key, dkey)) return (d);
		} else {
			if (streq(key, dkey)) return (d);
		}
		if (!LONGKEY(s) && (t = sccs_iskeylong(dkey))) {
			*t = 0;
			if (streq(key, dkey)) return (d);
		}
	}
	return (0);
}

void
sccs_findKeyUpdate(sccs *s, delta *d)
{
	u32	dd;
	ser_t	ser;

	unless (s->findkeydb) return;

	dd = d->date;
	ser = SERIAL(s, d);
	unless (hash_insert(s->findkeydb,
	    &dd, sizeof(dd), &ser, sizeof(ser))) {
		/* date conflict */
		if (ser > *(ser_t *)(s->findkeydb->vptr)) {
			hash_store(s->findkeydb,
			    &dd, sizeof(dd), &ser, sizeof(ser));
		}
	}
}

void
sccs_findKeyFlush(sccs *s)
{
	if (s->findkeydb) {
		hash_free(s->findkeydb);
		s->findkeydb = 0;
	}
}

/* return the time of the delta in UTC.
 * Do not change times without time zones to localtime.
 */
char *
sccs_utctime(sccs *s, delta *d)
{
	struct	tm *tp;
	static	char sdate[30];

	tp = utc2tm(d->date);
	sprintf(sdate, "%d%02d%02d%02d%02d%02d",
	    tp->tm_year + 1900,
	    tp->tm_mon + 1,
	    tp->tm_mday,
	    tp->tm_hour,
	    tp->tm_min,
	    tp->tm_sec);
	return (sdate);
}

/*
 * Return the sccs date format for the this delta.
 *
 * This is in the localtime of that delta.
 */
char *
delta_sdate(sccs *s, delta *d)
{
	static	char sdate[30];

	delta_strftime(sdate, sizeof(sdate),
	    "%y/%m/%d %H:%M:%S", s, d);
	return (sdate);
}

void
sccs_pdelta(sccs *s, delta *d, FILE *out)
{
	assert(d);
	fprintf(out, "%s|%s|%s|%05u",
	    USERHOST(s, d),
	    PATHNAME(s, d),
	    sccs_utctime(s, d),
	    d->sum);
	if (d->random) fprintf(out, "|%s", RANDOM(s, d));
}

/* Get the checksum of the 5 digit checksum */
int
sccs_sdelta(sccs *s, delta *d, char *buf)
{
	char	*tail;
	int	len;

	assert(d);
	len = sprintf(buf, "%s|%s|%s|%05u",
	    USERHOST(s, d),
	    PATHNAME(s, d),
	    sccs_utctime(s, d),
	    d->sum);
	assert(len);
	unless (d->random) return (len);
	for (tail = buf; *tail; tail++);
	len += sprintf(tail, "|%s", RANDOM(s, d));
	return (len);
}

void
sccs_sortkey(sccs *s, delta *d, char *buf)
{
	sum_t	origsum;
	u32	origpath;

	origpath = d->pathname;
	origsum = d->sum;

	unless (getenv("_BK_NO_SORTKEY")) {
		if (origpath && d->sortPath) d->pathname = d->sortPath;
		if (d->flags & D_SORTSUM) d->sum = d->sortSum;
	}
	sccs_sdelta(s, d, buf);

	d->pathname = origpath;
	d->sum = origsum;
}

/*
 * This is really not an md5, it is <date><md5> so we can find the key fast.
 */
void
sccs_md5delta(sccs *s, delta *d, char *b64)
{
	char	*hash;
	char	key[MAXKEY+16];

	sccs_sdelta(s, d, key);
	if (s->tree->random) strcat(key, RANDOM(s, s->tree));
	hash = hashstr(key, strlen(key));
	sprintf(b64, "%08x%s", (u32)d->date, hash);
	free(hash);
}

/*
 * Given a long rootkey/deltakey pair return the md5key for that delta.
 */
void
sccs_key2md5(char *rootkey, char *deltakey, char *b64)
{
	char	*hash, *p, *random;
	int	i;
	char	key[MAXKEY+64];

	/* like this to work with shortkeys */
	random = rootkey;
	for (i = 0; i < 4; i++) {
		unless (random = strchr(random, '|')) break;
		random++;
	}

	strcpy(key, deltakey);
	if (random) strcat(key, random);
	hash = hashstr(key, strlen(key));

	p = strchr(deltakey, '|');
	p = strchr(p+1, '|');
	sprintf(b64, "%08x%s", (u32)sccs_date2time(p+1, 0), hash);
	free(hash);
}

void
sccs_setPath(sccs *s, delta *d, char *new)
{
	if (streq(new, PATHNAME(s, d))) return;		// NOP: rename (A, A)

	unless (d->sortPath) {
		d->sortPath = d->pathname;
		bk_featureSet(s->proj, FEAT_SORTKEY, 1);
	} else if (streq(new, SORTPATH(s, d))) {
		d->sortPath = 0;
	}
	d->pathname = sccs_addUniqStr(s, new);
}

/*
 * Given a delta, return the delta which is the cset marked one for the cset
 * which contains this delta.  Note Rick's cool code that handles going through
 * merges.
 * If the delta is not in a cset (i.e., it's pending) then return null.
 */
delta	*
sccs_csetBoundary(sccs *s, delta *d)
{
	delta	*e, *start, *end;

	start = d;
	d->flags |= D_RED;
	for (; d <= s->table; ++d) {
		unless (d->flags && !TAG(d)) continue;

		if ((e = PARENT(s, d)) && (e->flags & D_RED)) {
			d->flags |= D_RED;
		}
		if ((e = MERGE(s, d)) && (e->flags & D_RED)) {
			d->flags |= D_RED;
		}
		if ((d->flags & (D_CSET|D_RED)) == (D_CSET|D_RED)) break;
	}
	end = d;
	if (d > s->table) {
		end = s->table;
		d = 0;
	}
	for (e = start; e <= end; ++e) e->flags &= ~D_RED;
	return (d);
}

/*
 * Identify SCCS files that the user creates.  Seperate from
 * BK files.  Basicly the ChangeSet file and anything created
 * in BitKeeper/ is a system file.  The "created" is because of
 * BitKeeper/deleted...
 */
int
sccs_userfile(sccs *s)
{
	char	*pathname;

	if (CSET(s) && !proj_isComponent(s->proj))		/* ChangeSet */
		return 0;

	pathname = PATHNAME(s, sccs_ino(s));
	if (strneq(pathname, "BitKeeper/", 10))
		return 0;
	return 1;
}

/*
 * Return true for any file we want to hide from the user,
 * eg. any file that is BK maintained metadata.
 * The ChangeSet weave matches but the comments are user
 * generated so it doesn't match.
 * Right now, just the BitKeeper/etc/attr file.
 */
int
sccs_metafile(char *file)
{
	return (streq(file, ATTR));
}

void
sccs_shortKey(sccs *s, delta *d, char *buf)
{
	assert(d);
	sprintf(buf, "%s|%s|%s",
	    USERHOST(s, d),
	    PATHNAME(s, d),
	    sccs_utctime(s, d));
}

/*
 * Take in a string like what pdelta spits out and break it into the
 * parts.
 */
void
explodeKey(char *key, char *parts[6])
{
	char	*s;

	/* user[@host]|sccs/slib.c|19970518232929[|23330|[233de234]] */
	/* user[@host]|path|date[|cksum|[random]] */

	/* parts[0] = user[@host] */
	for (s = key; *key && (*key != '|'); key++);
	parts[0] = s;
	assert(key);
	*key++ = 0;

	/* parts[2] = path or NULL if no path listed, but || */
	for (s = key; *key && (*key != '|'); key++);
	parts[2] = s == key ? 0 : s;
	assert(key);
	*key++ = 0;

	/* parts[3] = utc fudged time */
	for (s = key; *key && (*key != '|'); key++);
	parts[3] = s;

	/* if more data .... it's a cksum or maybe null field? */
	if (*key) {
		*key++ = 0;
		for (s = key; *key && (*key != '|'); key++);
		parts[4] = s == key ? 0 : s;
	}
	else {
		parts[4] = 0;
	}
	/* if more data .... random string */
	if (*key) {
		*key++ = 0;
		for (s = key; *key && (*key != '|'); key++);
		parts[5] = s == key ? 0 : s;
	}
	else {
		parts[5] = 0;
	}

	assert(!*key);

	/* go back and split user@host to user and host */
	for (key = parts[0]; *key && (*key != '@'); key++);
	if (key = strchr(parts[0], '@')) {
		*key++ = 0;
		parts[1] = key;
	} else {
		parts[1] = 0;
	}
}

/*
 * Sun has this sort of entry on files which have no common heritage.
 * Seems reasonable to use the first delta which is real to get something
 * unique.
 * XXX - this is a good reason to listen to Rick about putting the id elsewhere.
 */
delta *
sccs_ino(sccs *s)
{
	delta	*d = s->tree;

	if ((d->date == 0) && streq(USER(s, d), "Fake")) {
		d = sccs_kid(s, d);
	}
	return (d);
}

int
sccs_reCache(int quiet)
{
	char	*av[4];
	int	i;

	av[0] = "bk";  av[1] = "idcache"; 
	if (quiet) {
		av[2] = "-q";
	} else {
		av[2] = 0;
	}
	av[3] = 0;
	i = spawnvp(_P_WAIT, av[0], av);
	unless (WIFEXITED(i)) return (1);
	return (WEXITSTATUS(i));
}

/*
 * Figure out if the file is gone from the DB.
 * XXX - currently only works with keys, not filenames.
 */
int
gone(char *key, MDBM *db)
{
	if (db) {
		unless (strchr(key, '|')) return (0);
		if (mdbm_fetch_str(db, key) != 0) return (1);
	}

	/*
	 * OK, so it's not marked as gone.  It might be a changeset rootkey
	 * for a component and we're going to let those be considered 
	 * auto-goned for now.
	 * XXX - needs an env var so we can turn this off.
	 */
	return (changesetKey(key) &&
	    proj_isProduct(0) && !streq(key, proj_rootkey(0)));
}

MDBM	*
loadDB(char *file, int (*want)(char *), int style)
{
	MDBM	*DB = 0;
	FILE	*f = 0;
	char	*v;
	int	idcache = 0, first = 1, quiet = 1;
	char	*cwd = 0, *t;
	char	buf[MAXLINE];
	u32	sum = 0;


	// XXX awc->lm: we should check the z lock here
	// someone could be updating the file...
	idcache = (style == DB_IDCACHE) ? 1 : 0;
again:	unless (f = fopen(file, "rt")) {
		if (first && idcache) {
recache:		first = 0;
			sum = 0;
			if (f) fclose(f);
			if (DB) mdbm_close(DB), DB = 0;
			unless (strneq(file, "BitKeeper/", 10)) {
				cwd = strdup(proj_cwd());
				t = strrchr(file, '/');
				*t = 0;
				chdir(file);
				*t = '/';
			}
			if (sccs_reCache(quiet)) {
				fprintf(stderr, "Failed to rebuild idcache\n");
				goto out;
			}
			if (cwd) {
				chdir(cwd);
				free(cwd);
			}
			goto again;
		}
		t = name2sccs(file);
		if (first && exists(t)) {
			get(t, SILENT, "-");
			free(t);
			first = 0;
			goto again;
		}
		free(t);
		if (first && streq(file, GONE) && proj_isResync(0)) {
			sprintf(buf, "%s/%s", RESYNC2ROOT, GONE);
			file = buf;
			/* don't clear first */
			goto again;
		}
out:		if (f) fclose(f);
		if (DB) mdbm_close(DB), DB = 0;
		return (0);
	}
	DB = mdbm_mem();
	assert(DB);
	while (fnext(buf, f)) {
		if (buf[0] == '#') {
			if (strneq(buf, "#$sum$ ", 7)) {
				if (atoi(&buf[7]) == sum) {
					idcache = 2;	/* OK */
					break;		/* done */
				}
				if (first) {
					fprintf(stderr,
					    "Bad idcache chksum %u:%u, ",
					    atoi(&buf[7]), sum);
					quiet = 0;
					goto recache;
				}
			}
			continue;
		}
		if (want && !want(buf)) continue;
		if (idcache) {
			u8	*u;

			for (u = buf; *u; sum += *u++);
		}
		if (chop(buf) != '\n') {
			if (first && idcache) {
				fprintf(stderr, "Detected bad idcache, ");
				quiet = 0;
				goto recache;
			}
			fprintf(stderr, "bad path: <%s> in %s\n", buf, file);
			mdbm_close(DB), DB = 0;
			return (0);
		}
		if (style & DB_KEYSONLY) {
			v = "";
		} else {
			if (style & DB_KEYFORMAT) {
				v = separator(buf);
			} else {
				v = strchr(buf, ' ');
			}
			unless (v) {
				if (first && idcache) {
					fprintf(stderr,
					    "Detected bad idcache, ");
					quiet = 0;
					goto recache;
				}
				fprintf(stderr, "Corrupted DB %s\n", file);
				mdbm_close(DB), DB = 0;
				return (0);
			}
			*v++ = 0;
		}
		switch (mdbm_store_str(DB, buf, v, MDBM_INSERT)) {
		    case 0: break;
		    case 1:
		    	if ((style & DB_NODUPS)) {
				fprintf(stderr,
				    "Duplicate key '%s' in %s.\n", buf, file);
				fprintf(stderr,
				    "\tvalue: %s\n\tvalue: %s\n",
				    mdbm_fetch_str(DB, buf), v);
				goto out;
			}
			break;
		    default:
			fprintf(stderr, "loadDB(%s) failed\n", file);
			goto out;
		}
	}
	if (idcache && (idcache != 2)) {
		fprintf(stderr, "No checksum trailer in idcache, ");
		quiet = 0;
		goto recache;
	}
	fclose(f);
	return (DB);
}

/*
 * Scan key and return:
 * Location of '|' which starts long key part
 * or the NULL pointer (0) if it was already short.
 * Does not modify the string (though the caller can do a *t=0 upeon return
 * to make a short key).
 */
char *
sccs_iskeylong(char *t)
{
	assert(t);
	for ( ; *t && *t != '|'; t++);
	assert(t);
	for (t++; *t && *t != '|'; t++);
	assert(t);
	for (t++; *t && *t != '|'; t++);
	unless (*t) return (0);
	assert(*t == '|');
	return (t);
}

/*
 * Translate a key into an sccs struct.
 * If it is in the idDB, use that, otherwise use the name in the key.
 * Return NULL if we can't find (i.e., if there is no s.file).
 * Return NULL if the file is there but does not have the same root inode.
 */
sccs	*
sccs_keyinit(project *proj, char *key, u32 flags, MDBM *idDB)
{
	datum	k, v;
	char	*p, *t, *r;
	sccs	*s;
	char	*localkey = 0;
	delta	*d;
	project	*localp;
	char	buf[MAXKEY];

	/*
	 * We call this with the crap people put in the gone file,
	 * do a little sanity check.
	 * x@y|K|19990319224848|02682|x
	 * 1234567890123456789012345678
	 * so 28 bytes, and MD5KEYS are longer, so we're good @ 28.
	 */
	unless (key && *key && (strlen(key) >= 28)) return (0);

	/*
	 * Id cache contains both long and short keys
	 * so we don't need to look things up as long then short.
	 */
	k.dptr = key;
	k.dsize = strlen(key) + 1;
	v  = mdbm_fetch(idDB, k);
	if (v.dsize) {
		p = name2sccs(v.dptr);
	} else {
		/*
		 * For sparse trees we need to short circuit the inits
		 * ChangeSet files that are not there or we will init
		 * our changeset file over and over again only to find
		 * that the key doesn't match.
		 */
		if (changesetKey(key) &&
	            proj_isProduct(proj) && !streq(key, proj_rootkey(proj))) {
		    	return (0);
		}

		unless (t = strchr(k.dptr, '|')) return (0);
		t++;
		unless (r = strchr(t, '|')) return (0);
		*r = 0;
		p = name2sccs(t);
		*r = '|';
	}
	if (proj) {
		t = aprintf("%s/%s", proj_root(proj), p);
		free(p);
		r = proj_cwd();
		if (strneq(t, r, strlen(r))) {
			/* proj is below cwd, use relative path */
			p = strdup(t + strlen(r) + 1);
			free(t);
		} else {
			/* full pathname */
			p = t;
		}
	}
	s = sccs_init(p, flags|INIT_MUSTEXIST);
	free(p);
	unless (s && HAS_SFILE(s))  goto out;
	unless (s->cksumok) return (s);
	if (proj) {
		localp = proj;
	} else {
		localp = proj_init(".");
		proj_free(localp);
	}
	if (!proj_isComponent(s->proj) &&
	    (s->proj != localp)) { /* use after free OK */
		/* We're trying to commit an sfile from a nested project
		 * in the enclosing project. Bail.*/
		goto out;
	}

	/*
	 * Go look for this key in the file.
	 */
	d = sccs_ino(s);
	sccs_sdelta(s, d, buf);

	/* modifies buf and key, so copy key to local key */
	localkey = strdup(key);
	assert(localkey);
	if (samekeystr(buf, localkey)) {
		free(localkey);
		return (s);
	}
	free(localkey);
	localkey = 0;
out:	if (s) sccs_free(s);
	return (0);
}

void
sccs_color(sccs *s, delta *d)
{
        while (d && !(d->flags & D_RED)) {
        	assert(!TAG(d));
        	d->flags |= D_RED;
        	if (d->merge) sccs_color(s, MERGE(s, d));
        	d = PARENT(s, d);
	}
}

/*
 * Given an SCCS structure with a list of marked deltas, strip them from
 * the delta table and place the striped body in out
 */
private int
stripDeltas(sccs *s, FILE *out)
{
	ser_t	*state = 0;
	int	prune = 0;
	char	*buf;
	int	ser;

	sccs_rdweaveInit(s);
	if (GZIP_OUT(s)) sccs_zputs_init(s, out);
	while (buf = sccs_nextdata(s)) {
		if (isData(buf)) {
			unless (prune) {
				fputdata(s, buf, out);
				fputdata(s, "\n", out);
			}
			continue;
		}
		debug2((stderr, "%s", buf));
		ser = atoi(&buf[3]);
		unless (SFIND(s, ser)->flags & D_SET) {
			fputdata(s, buf, out);
			fputdata(s, "\n", out);
		}
		state = changestate(state, buf[1], ser);
		ser = whatstate(state);
		prune = (ser && (SFIND(s, ser)->flags & D_SET));
	}
	free(state);
	if (GZIP_OUT(s)) sccs_zputs_done(s);
	if (sccs_rdweaveDone(s)) return (1);
	if (sccs_finishWrite(s, &out)) return (1);
	return (0);
}

/*
 * Strip out the deltas marked with D_SET and D_GONE.
 * Note: rmdel doesn't strip, so only sets D_SET
 * E.g. to remove top rev: 
 * delta *d = sccs_top(s); MK_GONE(d); d->flags |= D_SET;
 * sccs_stripdel(s, "die");
 */
int
sccs_stripdel(sccs *s, char *who)
{
	FILE	*sfile = 0;
	int	error = 0;
	int	locked;
	delta	*e;
	symbol	*sym;

#define	OUT	\
	do { error = -1; s->state |= S_WARNED; goto out; } while (0)

	assert(s && HASGRAPH(s));
	if (HAS_PFILE(s) && sccs_clean(s, SILENT)) return (-1);
	debug((stderr, "stripdel %s %s\n", s->gfile, who));
	unless (locked = sccs_lock(s, 'z')) {
		fprintf(stderr, "%s: can't get lock on %s\n", who, s->sfile);
		OUT;
	}
	if (stripChecks(s, 0, who)) OUT;
	unless (sfile = sccs_startWrite(s)) OUT;

	/*
	 * Find the new top-of-trunk.
	 * We're assuming that we are going to strip stufff such that we
	 * leave a lattice so we can find the first non-gone regular delta
	 * and that's our top.
	 */
	for (e = s->table; e; e = NEXT(e)) {
		if (!TAG(e) && !(e->flags & D_SET)) break;
	}
	assert(e);
	if (BITKEEPER(s) && e->pathname &&
	    strneq(PATHNAME(s, e), "BitKeeper/moved/", 16)) {
		/* csetprune creates this path */
		fprintf(stderr,
		    "%s: illegal to leave file in the BitKeeper/moved "
		    "directory: %s\n", who, s->gfile);
		unless (getenv("_BK_UNDO_OK")) OUT;
	}
	s->xflags = sccs_xflags(s, e);

	/* remove deleted symbols */
	EACHP(s->symlist, sym) {
		unless (SFIND(s, sym->meta_ser)->flags & D_GONE) continue;
		removeArrayN(s->symlist, (sym - s->symlist));
		--sym;
	}

 	/* fix things up so that nothing after the tip is written */
	for (e = s->table; e; e = NEXT(e)) {
		/* include tags in the search; not D_SET because keep rmdel */
		unless (e->flags & D_GONE) break;
	}
	truncArray(s->slist, SERIAL(s, e));
	s->tree = SFIND(s, 1);
	s->table = s->slist + nLines(s->slist);
	s->nextserial = nLines(s->slist)+1;

	/* write out upper half */
	if (delta_table(s, sfile, 0)) {  /* 0 means as-is, so chksum works */
		fprintf(stderr,
		    "%s: can't write delta table for %s\n", who, s->sfile);
		OUT;
	}

	/* write out the lower half */
	if (stripDeltas(s, sfile)) {
		fprintf(stderr,
		    "%s: can't write delta body for %s\n", who, s->sfile);
		OUT;
	}
	/* sfile closed by stripDeltas() */
	sfile = 0;
#undef	OUT

out:
	if (sfile) sccs_abortWrite(s, &sfile);
	if (locked) sccs_unlock(s, 'z');
	debug((stderr, "stripdel returns %d\n", error));
	return (error);
}

/*
 * Remove the delta, leaving a delta of type 'R' behind.
 */
int
sccs_rmdel(sccs *s, delta *d, u32 flags)
{
	d->flags |= D_SET;
	if (stripChecks(s, d, "rmdel")) return (1);

	d->flags |= D_TAG;	/* mark delta as Removed */
	d->flags &= ~D_CKSUM;

	return (sccs_stripdel(s, "rmdel"));
}

/*
 * Make sure it is OK to remove a delta.
 */
private int
stripChecks(sccs *s, delta *d, char *who)
{
	if (checkGone(s, D_SET, who)) return (1);

	unless (d) return (0);
	/*
	 * Do not let them remove the root.
	 */
	unless (d->pserial) {	/* don't remove if this is 1.1 (no parent) */
		fprintf(stderr,
			"%s: can't remove root change %s in %s.\n",
			who, REV(s, d), s->sfile);
		s->state |= S_WARNED;
		return (1);
	}
	return (0);
}
/* TIMESTAMP HANDLING */
/*
 * timestampDBChanged is set when entries in the database have changed but
 * not deleted. This means the disk file may contain stale data if no
 * existing entries are modified or new ones added. This does not matter
 * as the checks that caused the entry to be deleted will fail next time
 */
#define	TIMESTAMPS	"BitKeeper/log/timestamps"

private int timestampDBChanged = 0;

/*
 * we need to parse the timestamp file. Its format is:
 * relative/file/path|gfile_mtime|gfile_size|permissions|sfile_mtime|sfile_size
 */
private int
parseTimestamps(char *buf, tsrec *ts)
{
	char	*p;

	/* parse gfile_mtime */
	ts->gfile_mtime = strtoul(buf, &p, 16);
	unless (p && (*p == BK_FS)) return (0);		/* bogus entry */

	/* parse gfile size */
	ts->gfile_size = strtoul(p + 1, &p, 10);
	unless (p && (*p == BK_FS)) return (0);		/* bogus entry */

	/* parse permissions */
	ts->permissions = strtoul(p + 1, &p, 8);
	unless (p && (*p == BK_FS)) return (0);		/* bogus entry */

	/* parse sfile_mtime */
	ts->sfile_mtime = strtoul(p + 1, &p, 16);
	unless (p && (*p == BK_FS)) return (0);		/* bogus entry */

	/* parse sfile_size */
	ts->sfile_size = strtoul(p + 1, &p, 10);
	unless (p && (*p == 0)) return (0);		/* bogus entry */

	return (1);
}

/*
 * generate a timestamp database - cannot use loadDB as it can't cope with
 * this format - or I couldn't figure out how! (andyc).
 */
hash *
generateTimestampDB(project *p)
{
	/* want to use the timestamp database */
	FILE	*f = 0;
	hash	*db;
	char	*tsname;
	char	buf[MAXLINE];

	assert(p);
	if (streq(proj_configval(p, "clock_skew"), "off")) return (0);

	tsname = aprintf("%s/%s", proj_root(p), TIMESTAMPS);
	db = hash_new(HASH_MEMHASH);
	assert(db);
	if (f = fopen(tsname, "r")) {
		while (fnext(buf, f)) {
			tsrec	*ts;
			char	*p;

			chomp(buf);
			unless (p = strchr(buf, BK_FS)) {
bad:				/* the database has invalid information in it
				 * so get it regenerated
				 */
				timestampDBChanged = 1;
				continue;
			}
			*p++ = 0;
			unless (ts = hash_fetchStrMem(db, buf)) {
				ts = hash_storeStrMem(db,
				    buf, 0, sizeof(tsrec));
			}
			unless (parseTimestamps(p, ts)) goto bad;
		}
		fclose(f);
	}
	free(tsname);
	return (db);
}

void
dumpTimestampDB(project *p, hash *db)
{
	FILE	*f = 0;
	char	*tsname;

	assert(p);
	unless (timestampDBChanged) return;

	tsname = aprintf("%s/%s", proj_root(p), TIMESTAMPS);
	unless (f = fopen(tsname, "w")) {
		free(tsname);
		return;			/* leave stale timestamp file there */
	}
	EACH_HASH(db) {
		tsrec   *ts = (tsrec *)db->vptr;

		assert(db->vlen == sizeof(*ts));
		fprintf(f, "%s|%x|%u|0%o|%x|%u\n",
		    (char *)db->kptr,
		    ts->gfile_mtime, ts->gfile_size,
		    ts->permissions,
		    ts->sfile_mtime, ts->sfile_size);
		if (ferror(f)) {
			/* some error writing the timestamp db so delete it */
			unlink(tsname);
			break;
		}
	}
	free(tsname);
	fclose(f);
}

void
sccs_clearbits(sccs *s, u32 flags)
{
	delta	*d;

	unless (s) return;
	for (d = s->table; d; d = NEXT(d)) {
		d->flags &= ~flags;
	}
	if (flags & D_GONE) s->hasgone = 0;
	if (flags & D_SET) s->state &= ~S_SET;
}

/*
 * timeMatch checks our file of timestamps against the current timestamps
 * of the given file
 */
int
timeMatch(project *proj, char *gfile, char *sfile, hash *timestamps)
{
	tsrec	*ts;
	char	*relpath;
	int	ret = 0;
	struct	stat	sb;

	assert(proj);
	relpath = proj_relpath(proj, gfile);
	ts = (tsrec *)hash_fetchStr(timestamps, relpath);
	free(relpath);
	unless (ts) goto out;			/* no entry for file */

	if (lstat(gfile, &sb) != 0) goto out;	/* might not exist */

	if ((sb.st_mtime != ts->gfile_mtime) ||
	    (sb.st_size != ts->gfile_size) ||
	    (sb.st_mode != ts->permissions)) {
		goto out;			/* gfile doesn't match */
	}
	if (lstat(sfile, &sb) != 0) {
		/* We should never get here */
		perror(sfile);
		goto out;
	}
	if ((sb.st_mtime != ts->sfile_mtime) ||
	    (sb.st_size != ts->sfile_size)) {
		goto out;			/* sfile doesn't match */
	}

	/* as far as we're concerned, the file hasn't changed */
	ret = 1;
 out:	return (ret);
}

void
updateTimestampDB(sccs *s, hash *timestamps, int different)
{
	tsrec	ts;
	struct	stat	sb;
	char	*relpath;
	static time_t clock_skew = 0;
	time_t now;

	assert(s->proj);
	unless (clock_skew) {
		char	*p = proj_configval(s->proj, "clock_skew");

		if (streq(p, "off")) {
			clock_skew = 2147483647;  /* 2^31 */
		} else {
			unless (clock_skew = strtoul(p, 0,0)) clock_skew = WEEK;
		}
	}

	relpath = proj_relpath(s->proj, s->gfile);
	if (different) {
		hash_deleteStr(timestamps, relpath);
		goto out;
	}

	if (lstat(s->gfile, &sb) != 0) goto out;	/* might not exist */
	ts.gfile_mtime = sb.st_mtime;
	ts.gfile_size = sb.st_size;
	ts.permissions = sb.st_mode;

	if (lstat(s->sfile, &sb) != 0) {	/* We should never get here */
		perror(s->sfile);
		goto out;
	}
	ts.sfile_mtime = sb.st_mtime;
	ts.sfile_size = sb.st_size;

	now = time(0);
	if ((now - ts.gfile_mtime) < clock_skew) {
		hash_deleteStr(timestamps, relpath);
	} else {
		hash_store(timestamps,
		    relpath, strlen(relpath) + 1, &ts, sizeof(ts));
		timestampDBChanged = 1;
	}
out:	free(relpath);
}
