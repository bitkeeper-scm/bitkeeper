/*
 * SCCS library - all of SCCS is implemented here.  All the other source is
 * no more than front ends that call entry points into this library.
 * It's one big file so I can hide a bunch of stuff as statics (private).
 *
 * XXX - I don't handle memory allocation failures well.
 *
 * Copyright (c) 1997-1998 Larry McVoy.	 All rights reserved.
 */
#include "system.h"
#include "sccs.h"
#include "zgets.h"
WHATSTR("@(#)%K%");

private delta	*rfind(sccs *s, char *rev);
private void	dinsert(sccs *s, int flags, delta *d);
private int	samebranch(delta *a, delta *b);
private int	samebranch_bk(delta *a, delta *b, int bk_mode);
private char	*sccsXfile(sccs *sccs, char type);
private int	badcksum(sccs *s);
private int	printstate(const serlist *state, const ser_t *slist);
private int	visitedstate(const serlist *state, const ser_t *slist);
private void	changestate(register serlist *state, char type, int serial);
private serlist *allocstate(serlist *old, int oldsize, int n);
private int	end(sccs *, delta *, FILE *, int, int, int, int);
private void	date(delta *d, time_t tt);
private int	getflags(sccs *s, char *buf);
private int	addsym(sccs *s, delta *d, delta *metad, char *a, char *b);
private void	inherit(sccs *s, int flags, delta *d);
private void	linktree(sccs *s, delta *l, delta *r);
private sum_t	fputmeta(sccs *s, u8 *buf, FILE *out);
private sum_t	fputdata(sccs *s, u8 *buf, FILE *out);
private int	fflushdata(sccs *s, FILE *out);
private void	putserlist(sccs *sc, ser_t *s, FILE *out);
private ser_t*	getserlist(sccs *sc, int isSer, char *s, int *ep);
private int	read_pfile(char *who, sccs *s, pfile *pf);
private int	hasComments(delta *d);
private int	checkRev(sccs *s, char *file, delta *d, int flags);
private int	checkrevs(sccs *s, int flags);
private int	stripChecks(sccs *s, delta *d, char *who);
private delta*	csetFileArg(delta *d, char *name);
private delta*	hostArg(delta *d, char *arg);
private delta*	pathArg(delta *d, char *arg);
private delta*	zoneArg(delta *d, char *arg);
delta*	modeArg(delta *d, char *arg);
private delta*	mergeArg(delta *d, char *arg);
private delta*	sumArg(delta *d, char *arg);
private	void	symArg(sccs *s, delta *d, char *name);
private int	delta_table(sccs *s, FILE *out, int willfix, int fixDate);
private time_t	getDate(delta *d);
private	void	unlinkGfile(sccs *s);
private time_t	date2time(char *asctime, char *z, int roundup);
private	char	*sccsrev(delta *d);
private int	addSym(char *name, sccs *sc, int flags, admin *l, int *ep);
private void	updatePending(sccs *s, delta *d);
private int	fix_lf(char *gfile);
private int	sameFileType(sccs *s, delta *d);
private int	deflate_gfile(sccs *s, char *tmpfile);
private int	isRegularFile(mode_t m);
private void	sccs_freetable(delta *d);
private	delta*	getCksumDelta(sccs *s, delta *d);
private int	fprintDelta(FILE *,
			char *, const char *, const char *, sccs *, delta *);
private	void	fitCounters(char *buf, int a, int d, int s);
private delta	*gca(delta *left, delta *right);
private delta	*gca2(sccs *s, delta *left, delta *right);

private unsigned int u_mask = 0x5eadbeef;

int
executable(char *f)
{
	return (access(f, X_OK) == 0);
}

int
exists(char *s)
{
	struct	stat sbuf;

	return (lstat(s, &sbuf) == 0);
}

int
isdir(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf) == -1) return 0;
	return (S_ISDIR(sbuf.st_mode));
}

int
isreg(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf) == -1) return 0;
	return (S_ISREG(sbuf.st_mode));
}

inline int
writable(char *s)
{
	return (access(s, W_OK) == 0);
}

off_t
size(char *s)
{
	struct	stat sbuf;

	if (lstat(s, &sbuf) == -1) return 0;
	unless (S_ISREG(sbuf.st_mode)) return (0);
	return (sbuf.st_size);
}

int
emptyDir(char *dir)
{
	DIR *d;
	struct dirent *e;

	d = opendir(dir);
	unless (d) {
		perror(dir);
		return (0);
	}

	while (e = readdir(d)) {
		if (streq(e->d_name, ".") || streq(e->d_name, "..")) continue;
		closedir(d);
		return (0);
	}
	closedir(d);
	return (1);
}

/*
 * Convert lrwxrwxrwx -> 0120777, etc.
 */
mode_t
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
	/* owner bits - does not handle setuid/setgid */
	if (*mode++ == 'r') m |= S_IRUSR;
	if (*mode++ == 'w') m |= S_IWUSR;
	if (*mode++ == 'x') m |= S_IXUSR;

	/* group - XXX, inherite these on DOS? */
	if (*mode++ == 'r') m |= S_IRGRP;
	if (*mode++ == 'w') m |= S_IWGRP;
	if (*mode++ == 'x') m |= S_IXGRP;

	/* other */
	if (*mode++ == 'r') m |= S_IROTH;
	if (*mode++ == 'w') m |= S_IWOTH;
	if (*mode++ == 'x') m |= S_IXOTH;

	return (m);
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
	*s++ = (m & S_IXUSR) ? 'x' : '-';
	*s++ = (m & S_IRGRP) ? 'r' : '-';
	*s++ = (m & S_IWGRP) ? 'w' : '-';
	*s++ = (m & S_IXGRP) ? 'x' : '-';
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
	// XXX this code may not be portable
	return (m & S_IFMT);
}

/*
 * These are the file types we currently suppprt
 * TODO: we may support empty directory & special file someday
 */
inline int
fileTypeOk(mode_t m)
{
	return ((S_ISREG(m)) || (S_ISLNK(m)));
}


/*
 * Not to be used before first sccs_init() call.
 */
private int
Chmod(char *fname, mode_t mode)
{
	return (chmod(fname, UMASK(mode)));
}

/*
 * Compare up to but not including the newline.
 * They should be newlines or nulls.
 */
#if 0
private int
strnonleq(register char *s, register char *t)
{
	while (*s && *t && (*s == *t) && (*s || (*s != '\n'))) s++, t++;
	return ((!*s || (*s == '\n')) && (!*t || (*t == '\n')));
}
#endif

char
chop(register char *s)
{
	char	c;

	while (*s++);
	c = s[-2];
	s[-2] = 0;
	return (c);
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
 * Save a line in an array.  If the array is out of space, reallocate it.
 * The size of the array is in array[0].
 * This is OK on 64 bit platforms.
 */
char	**
addLine(char **space, char *line)
{
	int	i;

	if (!space) {
		space = calloc(32, sizeof(char *));
		assert(space);
		space[0] = (char *)32;
	} else if (space[(int)(long)space[0]-1]) {	/* full up, dude */
		int	size = (int)(long)space[0];
		char	**tmp = calloc(size*2, sizeof(char*));

		assert(tmp);
		bcopy(space, tmp, size*sizeof(char*));
		tmp[0] = (char *)(long)(size * 2);
		free(space);
		space = tmp;
	}
	EACH(space);	/* I want to get to the end */
	assert(i < (int)(long)space[0]);
	assert(space[i] == 0);
	space[i] = line;
	return (space);
}

void
freeLines(char **space)
{
	int	i;

	if (!space) return;
	EACH(space) {
		free(space[i]);
	}
	space[0] = 0;
	free(space);
}

private int
removeLine(char **space, char *s)
{
	int	i, found, n = 0;

	do {
		found = 0;
		EACH(space) {
			if (streq(space[i], s)) {
				free(space[i]);
				while ((++i< (int)(long)space[0]) && space[i]) {
					space[i-1] = space[i];
					space[i] = 0;
				}
				n++;
				found = 1;
				break;
			}
		}
	} while (found);
	return (n > 0);
}

/*
 * Compare up to and including the newline.  Both have to be on \n to match.
 */
private int
strnleq(register char *s, register char *t)
{
	while (*t == *s) {
		if (!*t || (*t == '\n')) return (1);
		t++, s++;
	}
	return (0);
}

char	*
sccs_getuser(void)
{
	static	char	*s;

	if (s) return (s);
	s = getenv("USER");
	if (!s || !s[0] ) {
		s = getlogin();
	}
#ifndef WIN32
	if (!s || !s[0] ) {
		struct	passwd	*p = getpwuid(getuid());

		s = p->pw_name;
	}
#endif
	if (!s || !s[0] ) {
		s = UNKNOWN_USER;
	}
	return (s);
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
private inline int
atoi(register char *s)
{
	register int val = 0;

	if (!s) return (0);
	while (*s && isdigit(*s)) {
		val = val * 10 + *s++ - '0';
	}
	return (val);
}

private inline int
atoi2(char **sp)
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

private inline int
atoiMult(char *s)
{
	register int val = 0;

	if (!s) return (0);
	while (*s && isdigit(*s)) {
		val = val * 10 + *s++ - '0';
	}
	switch (*s) {
	    case 'K': val *= 1000; break;
	    case 'M': val *= 1000000; break;
	    case 'G': val *= 1000000000; break;
	}
	return (val);
}

/* Free one delta.  */
private inline void
freedelta(delta *d)
{
	freeLines(d->comments);
	freeLines(d->mr);
	freeLines(d->text);

	if (d->rev) free(d->rev);
	if (d->user) free(d->user);
	if (d->sdate) free(d->sdate);
	if (d->include) free(d->include);
	if (d->exclude) free(d->exclude);
	if (d->ignore) free(d->ignore);
	if (d->sym) free(d->sym);
	if (d->symlink && !(d->flags & D_DUPLINK)) free(d->symlink);
	if (d->hostname && !(d->flags & D_DUPHOST)) free(d->hostname);
	if (d->pathname && !(d->flags & D_DUPPATH)) free(d->pathname);
	if (d->zone && !(d->flags & D_DUPZONE)) free(d->zone);
	if (d->csetFile && !(d->flags & D_DUPCSETFILE)) {
		free(d->csetFile);
	}
	free(d);
}


/*
 * Free the delta tree.
 */
void
sccs_freetree(delta *tree)
{
	if (!tree) return;

	debug((stderr, "freetree(%s %s %d)\n",
	       notnull(tree->rev), notnull(tree->sdate), tree->serial));
	sccs_freetree(tree->siblings);
	sccs_freetree(tree->kid);
	freedelta(tree);
}

/*
 * Free the entire delta table.
 * This follows the ->next pointer and is not recursive.
 */
private void
sccs_freetable(delta *t)
{
	delta *u;

	if (!t) return;

	debug((stderr, "freetable():\n"));
	for(; t; t = u) {
		debug((stderr, "\t%s %s %d\n",
		       notnull(t->rev), notnull(t->sdate), t->serial));
		u = t->next;
		freedelta(t);
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
private void
inherit(sccs *s, int flags, delta *d)
{
	delta	*p;

	unless (d) return;
	unless (p = d->parent) {
		getDate(d);
		return;
	}

	/*
	 * For each metadata field, check if not null, dup if null.
	 * If it is already dupped, leave it alone.
	 */
#define	CHK_DUP(field, flag, str) \
	if (d->field) { \
		unless (d->flags & flag) { \
			if (p->field && streq(d->field, p->field)) { \
				free(d->field); \
				d->field = p->field; \
				d->flags |= flag; \
			} \
		} \
	} else if (p->field) { \
		d->field = p->field; \
		d->flags |= flag; \
	}
	CHK_DUP(pathname, D_DUPPATH, "path");
	CHK_DUP(hostname, D_DUPHOST, "host");
	CHK_DUP(zone, D_DUPZONE, "zone");
	CHK_DUP(csetFile, D_DUPCSETFILE, "csetFile");
	if ((p->flags & D_MODE) && !(d->flags & D_MODE)) {
		d->flags |= D_MODE;
		d->mode = p->mode;
		CHK_DUP(symlink, D_DUPLINK, "symlink");
	}
	getDate(d);
	if (d->merge) {
		d = sfind(s, d->merge);
		assert(d);
		d->flags |= D_MERGED;
	}
}

/*
 * Reinherit if we found stuff in the flags section.
 */
void
reinherit(sccs *s, delta *d)
{
	if (!d) return;
	/* XXX - do I really need this? */
	d->date = 0;
	inherit(s, 0, d);
	reinherit(s, d->kid);
	reinherit(s, d->siblings);
}

/*
 * Insert the delta in the (ordered) tree.
 * A little weirdness when it comes to removed deltas,
 * we want them off to the side if possible (it makes rfind work better).
 * New in Feb, '99: remove duplicate metadata fields here, maintaining the
 * invariant that a delta in the graph is always correct.
 */
private void
dinsert(sccs *s, int flags, delta *d)
{
	delta	*p;

	debug((stderr, "dinsert(%s)", d->rev));
	if (!s->tree) {
		s->tree = d;
		s->lastinsert = d;
		debug((stderr, " -> ROOT\n"));
		return;
	}
	if (s->lastinsert && (s->lastinsert->serial == d->pserial)) {
		p = s->lastinsert;
		debug((stderr, " (fast)"));
	} else {
		p = sfind(s, d->pserial);
	}
	assert(p);
	s->lastinsert = d;
	d->parent = p;
	assert(!d->kid && !d->siblings);
	if (!p->kid) {
		p->kid = d;
		debug((stderr, " -> %s (kid)\n", p->rev));

	} else if ((p->kid->type == 'D') &&
	    samebranch_bk(p, p->kid, 1)) { /* in right place */
		/*
		 * If there are siblings, add d at the end.
		 */
		if (p->kid->siblings) {
			delta	*l = p->kid->siblings;

			while (l->siblings) l = l->siblings;
			l->siblings = d;
		} else {
			p->kid->siblings = d;
		}
		debug((stderr, " -> %s (sib)\n", p->rev));
	} else {  /* else not in right place, put the new delta there. */
		debug((stderr, "kid type %c, %d.%d.%d.%d vs %d.%d.%d.%d\n",
		    p->kid->type, p->r[0], p->r[1], p->r[2], p->r[3],
		    p->kid->r[0], p->kid->r[1], p->kid->r[2], p->kid->r[3]));
		d->siblings = p->kid;
		p->kid = d;
		debug((stderr, " -> %s (kid, moved sib %s)\n",
		    p->rev, d->siblings->rev));
	}
	inherit(s, flags, d);
}

/*
 * Find the delta referenced by the serial number.
 */
delta *
sfind(sccs *s, ser_t serial)
{
	delta	*t;

	assert(serial <= s->nextserial);
	if (serial >= s->ser2dsize) goto realloc;
	if (s->ser2delta && s->ser2delta[serial]) return (s->ser2delta[serial]);
	if (s->ser2delta) {
		for (t = s->table; t; t = t->next) {
			if (t->serial == serial) {
				assert(serial < s->ser2dsize);
				s->ser2delta[serial] = t;
				return (t);
			}
		}
		return (0);
	}

realloc:
	if (s->ser2delta) free(s->ser2delta);
	/* We leave a little extra room for sccs_delta. */
	s->ser2dsize = s->nextserial+10;
	s->ser2delta = calloc(s->ser2dsize, sizeof(delta*));
	for (t = s->table; t; t = t->next) {
		if (t->serial >= s->ser2dsize) {
			fprintf(stderr, "%s %d %d\n",
			    s->sfile, t->serial, s->ser2dsize);
		}
		assert(t->serial < s->ser2dsize);
		s->ser2delta[t->serial] = t;
	}
	return (s->ser2delta[serial]);
}

/*
 * An array, indexed by years after 1971, which gives the seconds
 * at the beginning of that year.  Valid up to around 2038.
 * The first value is the time value for 1970-01-01-00:00:00 .
 *
 * Do NOT NOT NOT change this after shipping, even if it is wrong.
 */
static const time_t  yearSecs[] = {
             0,   31536000,   63072000,   94694400,  126230400,  157766400,
     189302400,  220924800,  252460800,  283996800,  315532800,  347155200,
     378691200,  410227200,  441763200,  473385600,  504921600,  536457600,
     567993600,  599616000,  631152000,  662688000,  694224000,  725846400,
     757382400,  788918400,  820454400,  852076800,  883612800,  915148800,
     946684800,  978220800, 1009756800, 1041292800, 1072828800, 1104451200,
    1135987200, 1167523200, 1199059200, 1230681600, 1262217600, 1293753600,
    1325289600, 1356912000, 1388448000, 1419984000, 1451520000, 1483142400,
    1514678400, 1546214400, 1577750400, 1609372800, 1640908800, 1672444800,
    1703980800, 1735603200, 1767139200, 1798675200, 1830211200, 1861833600,
    1893369600, 1924905600, 1956441600, 1988064000, 2019600000, 2051136000,
    2082672000, 2114294400, 2145830400, 0 };

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

int
leapYear(int year)
{
	return ((!((year) % 4) && ((year) % 100)) || !((year) % 400));
}

/*
 * A version of mktime() which uses the fields
 *	year, mon, hour, min, sec
 * and assumes utc time.
 */
time_t
tm2utc(struct tm *tp)
{
	time_t	t;

	if (tp->tm_year < 70) return (1);
	assert(tp->tm_year < 138);
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
	for (i = 0; i < 12; ++i) {
		tmp = monthSecs[i+1];
		if (leap && ((i+1) >= 2)) tmp += DSECS;
		if (t < tmp) {
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

private time_t
getDate(delta *d)
{
	if (!d->date) {
		d->date = date2time(d->sdate, d->zone, EXACT);
		if (d->dateFudge) {
			d->date += d->dateFudge;
		}
	}
	unless (d->date || streq("70/01/01 00:00:00", d->sdate)) {
		assert(d->date);
	}
	return (d->date);
}

/*
 * The prev pointer is a more recent delta than this one,
 * so make sure that the prev date is > that this one.
 */
private void
fixDates(delta *prev, delta *d)
{
	unless (d->date) (void)getDate(d);

	/* recurse forwards first */
	if (d->next) fixDates(d, d->next);

	/* When we get here, we're done. */
	unless (prev) return;

	if (prev->date <= d->date) {
		prev->dateFudge = (d->date - prev->date) + 1;
		prev->date += prev->dateFudge;
	}
}

void
sccs_fixDates(sccs *s)
{
	fixDates(0, s->table);
}

/*
 * Fix the date in a new delta.
 * Make sure date is increasing
 */
void
fixNewDate(sccs *s)
{
	delta	*next, *d;
	time_t	last;
	char	buf[MAXPATH+100];

	d = s->table;
	assert(!d->dateFudge);
	unless (d->date) (void)getDate(d);

	/*
	 * This is kind of a hack.  We aren't in BK mode yet we are fudging.
	 * It keeps BK happy, I guess.
	 */
	unless (s->state & S_BITKEEPER) {
		unless (next = d->next) return;
		if (next->date >= d->date) {
			time_t	tdiff;
			tdiff = next->date - d->date + 1;
			d->date += tdiff;
			d->dateFudge += tdiff;
		}
		return;
	}

	uniq_open();
	sccs_sdelta(s, sccs_ino(s), buf);
	/*
	 * If we are the first delta, make sure our key doesn't exist.
	 */
	unless (next = d->next) {
		while (uniq_root(buf)) {
//fprintf(stderr, "COOL: caught a duplicate root: %s\n", buf);
			d->dateFudge++;
			d->date++;
			sccs_sdelta(s, d, buf);
		}
		uniq_update(buf, d->date);
		uniq_close();
		return;
	}
	assert(next->date);
	if (d->date <= next->date) {
		d->dateFudge = (next->date - d->date) + 1;
		d->date += d->dateFudge;
	}
	if ((last = uniq_time(buf)) && (last >= d->date)) {
//fprintf(stderr, "COOL: caught a duplicate key: %s\n", buf);
		while (d->date <= last) {
			d->date++;
			d->dateFudge++;
		}
	}
	uniq_update(buf, d->date);
	uniq_close();
}



private int
monthDays(int year, int month)
{
	if (month != 2) return (days[month]);
	if (leapYear(year)) return (29);
	return (28);
}

private void
a2tm(struct tm *tp, char *asctime, char *z, int roundup)
{
	int	mday;

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
	/* Adjust for year 2000 problems */
	gettime(tm_year); if (tp->tm_year < 69) tp->tm_year += 100;
	unless (*asctime) goto correct;

	/* tm_mon counts 0..11; ASCII is 1..12 */
	gettime(tm_mon); tp->tm_mon--; unless (*asctime) goto correct;
	gettime(tm_mday); unless (*asctime) goto correct;
	gettime(tm_hour); unless (*asctime) goto correct;
	gettime(tm_min); unless (*asctime) goto correct;
	gettime(tm_sec);
	if (((asctime[-1] == '-') || (asctime[-1] == '+')) && !z) z = --asctime;

correct:
	/*
	 * Truncate down oversized fields.
	 */
	if (tp->tm_mon > 11) tp->tm_mon = 11;
	mday = monthDays(tp->tm_year, tp->tm_mon + 1);
	if (mday < tp->tm_mday) tp->tm_mday = mday;
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
 * roundup is ROUNDDOWN, EXACT, or ROUNDUP.  Which do the implied
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

	a2tm(&tm, asctime, z, roundup);
#if	0
{	struct  tm tm2 = tm;
	fprintf(stderr, "%s%s %02d/%02d/%02d %02d:%02d:%02d = %u\n",
	asctime,
	z ? z : "",
	tm.tm_year,
	tm.tm_mon + 1,
	tm.tm_mday,
	tm.tm_hour,
	tm.tm_min,
	tm.tm_sec,
	tm2utc(&tm2));
}
#endif
	return (tm2utc(&tm));
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
		else unless (*s == 'D' || *s == 'I')
			return (-1);
		s++;
		*where = atoi2(&s);
		unless (*s == ' ')  return (-1);
		s++;
		*howmany = atoi2(&s);
		return (0);
	}
	*howmany = 0;	/* not used by this part */
	*where = atoi2(&s);
	if (*s == ',') {
		s++;
		(void)atoi2(&s);
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
private inline int
scanrev(char *s, u16 *a, u16 *b, u16 *c, u16 *d)
{
	if (!isdigit(*s)) return (0);
	*a = atoi2(&s);
	if (b && *s == '.') {
		s++;
		*b = atoi2(&s);
		if (c && *s == '.') {
			s++;
			*c = atoi2(&s);
			if (d && *s == '.') {
				s++;
				*d = atoi(s);
				return (4);
			} else return (3);
		} else return (2);
	} else return (1);
}

private inline void
explode_rev(delta *d)
{
	register char *s = d->rev;
	int	dots = 0;

	while (*s) { if (*s++ == '.') dots++; }
	if (dots > 3) d->flags |= D_ERROR|D_BADFORM;
	switch (scanrev(d->rev, &d->r[0], &d->r[1], &d->r[2], &d->r[3])) {
	    case 1: d->r[1] = 0;	/* fall through */
	    case 2: d->r[2] = 0;	/* fall through */
	    case 3: d->r[3] = 0;
	}
}

private char *
sccsrev(delta *d)
{
	return (d->rev);
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
 * This one assumes SCCS style branch numbering, i.e., x.y.z.d
 */
private int
samebranch_bk(delta *a, delta *b, int bk_mode)
{
	if (!a->r[2] && !b->r[2])
		return (bk_mode? (a->r[0] == b->r[0]) : 1);
	return ((a->r[0] == b->r[0]) &&
		(a->r[1] == b->r[1]) &&
		(a->r[2] == b->r[2]));
}

private char *
branchname(delta *d)
{
	u16	a1 = 0, a2 = 0, a3 = 0, a4 = 0;
	static	char buf[6];

	scanrev(d->rev, &a1, &a2, &a3, &a4);
	if (a3) {
		sprintf(buf, "%d", a3);
	} else {
		sprintf(buf, "0");
	}
	return (buf);
}

char	*
basenm(char *s)
{
	char	*t;

	for (t = s; *t; t++);
	do {
		t--;
	} while (*t != '/' && t > s);
	if (*t == '/') t++;
	return (t);
}

/*
 * clean up ".." and "." in a path name
 */
void
cleanPath(char *path, char cleanPath[])
{
	char	buf[MAXPATH], *p, *r, *top;
	int	dotCnt = 0;	/* number of "/.." */
#define isEmpty(buf, r) 	(r ==  &buf[sizeof (buf) - 2])

	r = &buf[sizeof (buf) - 1]; *r-- = 0;
	p = &path[strlen(path) - 1];

	/* for win32 path */
	top = (path[1] == ':')  ? top = &path[2] : path;
	/* trim trailing '/' */
	if ((*p == '/') && (p != top))  p--;
	while (p >= top) { 	/* scan backward */
		if ((p >= &top[2]) && (p[-2] == '/') &&
		    (p[-1] == '.') && (p[0] == '.')) {
			dotCnt++; p = &p[-3];	/* process "/.." */
		} else if ((p >= &top[1]) && (p[-1] == '/') &&
		    	 (p[0] == '.')) {
			p = &p[-2];		/* process "/." */
		} else if ((p == top) && (p[0] == '.')) {
			p = &p[-1];		/* process "." */
		} else {
			if (dotCnt) {
				/* skip dir impacted by ".." */
				while ((p >= top) && (*p != '/')) p--;
				p--; dotCnt--;
			} else {
				/* copy regular directory */
				unless (isEmpty(buf, r)) *r-- = '/';
				while ((p >= top) && (*p != '/')) *r-- = *p--;
				p--;
			}
		}
	}

	if (!isEmpty(buf, r) && (top[0] != '/')) {
		/* put back any ".." with no known parent directory  */
		while (dotCnt--) {
			unless (isEmpty(buf, r)) *r-- = '/';
			*r-- = '.'; *r-- = '.';
		}
	}

	if (top[0] == '/') *r-- = '/';
	if (top != path) { *r-- = path[1]; *r-- = path[0]; }
	if (*++r) {
		strcpy(cleanPath, r);
	} else {
		strcpy(cleanPath, ".");
	}
	/* for win32 path */
	if ((r[1] == ':') && (r[2] == '\0')) strcat(cleanPath, "/");
#undef	isEmpty
}

/*
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * All of this pathname/changeset shit needs to be reworked.
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 */

/*
 * Change directories to the project root or return -1.  If the second
 * arg is non-null, then that's the root, and we aren't to call
 * sccs_root to find it.  The only place that does that is
 * takepatch.c, and it probably shouldn't.
 */
int
sccs_cd2root(sccs *s, char *root)
{
	char	*r;

	if (root) r = root;
	else r = sccs_root(s);

	if (r && (chdir(r) == 0)) {
		unless (exists(BKROOT)) {
			perror(BKROOT);
			return (-1);
		}
		return (0);
	}
	return (-1);
}

void
sccs_mkroot(char *path)
{
	char	buf[MAXPATH];

	sprintf(buf, "%s/SCCS", path);
	if ((mkdir(buf, 0775) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper", path);
	if ((mkdir(buf, 0775) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/etc", path);
	if ((mkdir(buf, 0775) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/etc/SCCS", path);
	if ((mkdir(buf, 0775) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/deleted", path);
	if ((mkdir(buf, 0775) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/deleted/SCCS", path);
	if ((mkdir(buf, 0775) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/tmp", path);
	if ((mkdir(buf, 0775) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
	sprintf(buf, "%s/BitKeeper/log", path);
	if ((mkdir(buf, 0775) == -1) && (errno != EEXIST)) {
		perror(buf);
		exit(1);
	}
}

/*
 * Return the id file as a FILE *
 */
FILE	*
idFile(sccs *s)
{
	char	file[MAXPATH];
	char	*root;

	unless (root = sccs_root(s)) return (0);
	sprintf(file, "%s/%s", root, IDCACHE);
	// XXX - locking of this file.
	return (fopen(file, "a"));
	return (0);
}

/*
 * Return the ChangeSet file id.
 */
char	*
getCSetFile(sccs *s)
{
	char	file[MAXPATH];
	char	*root;
	sccs	*sc;

	unless (root = sccs_root(s)) return (0);
	sprintf(file, "%s/%s", root, CHANGESET);
	if (exists(file)) {
		sc = sccs_init(file, INIT_NOCKSUM, 0);
		assert(sc->tree);
		sccs_sdelta(sc, sc->tree, file);
		sccs_free(sc);
		return (strdup(file));
	}
	return (0);
}

/*
 * Return the pathname relative to the first ChangeSet file found.
 *
 * XXX - this causes a 5-10% slowdown, more if the tree is very deep.
 * I need to cache changeset lookups.
 */
char	*
_relativeName(char *gName,
	    int isDir, int withsccs, int mustHaveRmarker, char *root)
{
	char	*t, *s, *top;
	int	i, j;
	static	char buf[MAXPATH];

	t = fullname(gName, 0);
	strcpy(buf, t); top = buf;
	if (buf[0] && buf[1] == ':') top = &buf[2]; /* for WIN32 path */
	assert(top[0] == '/');
	if (isDir) {
		s = &buf[strlen(buf)];
		s[0] = '/';
	} else {
		/* trim off the file part */
		s = strrchr(buf, '/');
	}

	/*
	 * Now work backwards up the tree until we find a root marker
	 */
	for (i = 0; s >= top; i++) {
		strcpy(++s, BKROOT);
		if (exists(buf))  break;
		if (--s <= top) {
			/*
			 * if we get here, we hit the top
			 * and did not find the root marker
			 */
			if (root) root[0] = 0;
			if (mustHaveRmarker) return (0);
			return (t); /* return full path name */
		}
		/* s -> / in .../foo/SCCS/s.foo.c */
		for (--s; (*s != '/') && (s > top); s--);
	}
	assert(s >= top);

	/*
	 * go back in other buffer to this point and copy forwards,
	 */
	if (isDir) {
		s = &t[strlen(t)];
		s[0] = '/'; s[1] = 0;
	} else {
		/* trim off the file part */
		s = strrchr(t, '/');
	}
	for (j = 1; j <= i; ++j) {
		for (--s; (*s != '/') && (s > t); s--);
	}
	if (root) {
		int len = s - t;
		strncpy(root, t, len); root[len] = 0;
	}
	strcpy(buf, ++s);
	if (isDir) {
		if (buf[0] == 0) strcpy(buf, ".");
		return buf;
	}
	if (withsccs) {
		char *sName;

		sName = name2sccs(buf);
		strcpy(buf, sName);
		free(sName);
	}
	return (buf);
}

/*
 * Trim off the RESYNC/ part of the pathname, that's garbage.
 */
char	*
relativeName(sccs *sc, int withsccs, int mustHaveRmarker)
{
	char	*s, *g;

	g = sccs2name(sc->sfile);
	s = _relativeName(g, 0, withsccs, mustHaveRmarker, NULL);
	free(g);

	unless (s) return (0);

	if (strncmp("RESYNC/", s, 7) == 0) s += 7;
	return (s);
}

#ifdef SPLIT_ROOT

/*
 * If we get a rootfile, (i.e split root),
 * copy the the new sroot from the root file, return 1;
 * otherwise return 0
 */
int
hasRootFile(char *gRoot, char *sRoot)
{
	FILE	*f;
	char 	*p, rootfile[1024], buf[1024];
#define ROOTFILE  "BitKeeper/etc/RootFile"

	/*
	 * TODO:
	 * we need to cache the sRoot value
	 */
	concat_path(rootfile, gRoot, ROOTFILE);
	unless (exists(rootfile)) return 0;
	f = fopen(rootfile, "rt");
	unless (f) {
		perror(rootfile);
		return 0;
	}

	while(fnext(buf, f)) {
		if (buf[0] == '#') continue; /* skip comment */
		if (chop(buf) != '\n') {
			assert("line too long in rootfile" == 0);
		}
		p = strchr(buf, ' '); assert(p);
		*p++ = 0;
		if (streq(buf, "SROOT")) {
			strcpy(sRoot, p);
			localName2bkName(sRoot, sRoot);
			fclose(f);
			return 1;
		}
	}
	fclose(f);
	fprintf(stderr, "Warning: %s has no SROOT entry, ignored\n", rootfile);
	return 0;
}

/*
 * Given a file/dir name , return its path in the S root tree
 * return value is in local static buffer
 * user must copy it before calling other function
 */
char *
sPath(char *name, int isDir)
{
	static	char buf[1024];
	char	*path, gRoot[1024], sRoot[1024];

	/*
	 *  If there is a local SCCS directory, use it
	 */
	cleanPath(name, buf);
	path = name2sccs(buf);
	strrchr(path, '/')[0] = 0;
	assert(streq("SCCS", basenm(path)));
	if (isdir(path)) {
		free(path);
		//return (fullname(buf, 0));
		debug((stderr, "sPath(%s) -> %s\n", name, name));
		return (name);
	}
	free(path);

	path = _relativeName(name, isDir, 0, 0, gRoot);
	if (IsFullPath(path)) return path; /* no root marker */
	if (hasRootFile(gRoot, sRoot)) {
		concat_path(buf, sRoot, path);
	} else {
		return (name);
	}
	cleanPath(buf, buf);
	debug((stderr, "sPath(%s) -> %s\n", name, buf));
	return buf;
}
#else
char *
sPath(char *name, int isDir) { return name; }
#endif /* SPLIT_ROOT */

/*
 * Revision number/branching theory of operation:
 *	rfind() does an exact match - use this if you want a specific delta.
 *	findrev() does an inexact match - it will find top of trunk for the
 *		trunk or a branch if it is specified as -r1 or -r1.2.1.
 *		Note that -r1 will not find tot if tot is 2.1; use a null
 *		rev to get that.
 *	getedit() finds the revision and gets the new revision number for a
 *		new delta.  The find logic is findrev().  The new revision
 *		will be the next available on that line, unless there is
 *		a conflict or the forcebranch flag is set.  In either of
 *		those cases, the new revision will be a branch.
 *		If the revision is on the trunk and they wanted to bump
 *		the release, do so.
 */
private inline int
name2rev(sccs *s, char **rp)
{
	char	*rev = *rp;
	symbol	*sym;

	if (!rev) return (0);
	if (isdigit(rev[0])) return (0);

again:
	for (sym = s->symbols; sym; sym = sym->next) {
		if (streq(rev, sym->name)) {
			rev = sym->rev;
			if (isdigit(rev[0])) {
				*rp = rev;
				return (0);
			}
			goto again;
		}
	}
	return (-1);
}

private delta *
sym2delta(sccs *s, char *sym)
{
	if (name2rev(s, &sym) == -1) return (0);
	return (rfind(s, sym));
}

private inline int
samerev(u16 a[4], u16 b[4])
{
	return ((a[0] == b[0]) &&
		(a[1] == b[1]) &&
		(a[2] == b[2]) &&
		(a[3] == b[3]));
}

private u16	R[4];
/*
 * This one uses the globals set up below.  This hack makes this library
 * !MT safe.  Which it wasn't anyway.
 */
private delta *
_rfind(delta *d)
{
	delta	*t;

	debug((stderr,
	    "\t_rfind(%d.%d.%d.%d)\n", d->r[0], d->r[1], d->r[2], d->r[3]));
	if (samerev(d->r, R)) {
		return (d);
	}
	if (d->kid && (t = _rfind(d->kid))) {
		return (t);
	}
	if (d->siblings && (t = _rfind(d->siblings))) {
		return (t);
	}
	return (0);
}

/*
 * Find the delta referenced by rev.  It must be an exact match.
 */
private delta *
rfind(sccs *s, char *rev)
{
	delta	*d;

	debug((stderr, "rfind(%s) ", rev));
	name2rev(s, &rev);
	R[0] = R[1] = R[2] = R[3] = 0;
	scanrev(rev, &R[0], &R[1], &R[2], &R[3]);
	debug((stderr, "aka %d.%d.%d.%d\n", R[0], R[1], R[2], R[3]));
	d = _rfind(s->tree);
	debug((stderr, "rfind(%s) got %s\n", rev, d ? d->rev : "Not found"));
	return (d);
}

private char *
defbranch(sccs *s)
{
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
delta *
findrev(sccs *s, char *rev)
{
	u16	a = 0, b = 0, c = 0, d = 0;
	u16	max = 0;
	delta	*e = 0, *f = 0;
	char	buf[20];

	debug((stderr,
	    "findrev(%s in %s def=%s)\n",
	    notnull(rev), s->sfile, defbranch(s)));
	if (!s->tree) return (0);
	if (!rev || !*rev) rev = defbranch(s);

	if (name2rev(s, &rev)) return (0);
	switch (scanrev(rev, &a, &b, &c, &d)) {
	    case 1:
		/* XXX: what does -r0 mean?? */
		unless (a) {
			fprintf(stderr, "Illegal revision 0\n");
			debug((stderr, " BAD rev %s\n", e->rev));
			return (0);
		}
		/* get max X.Y that is on same branch or tip of biggest */
		for (e = s->table; e; e = e->next) {
			if (e->type != 'D'
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
		assert(e);
		debug((stderr, "findrev(%s) =  %s\n", rev, e->rev));
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
		for (; e->kid && e->kid->type == 'D' && samebranch(e, e->kid);
		    e = e->kid)
			;
		debug((stderr, "findrev(%s) =  %s\n", rev, e->rev));
		return (e);
	    default:
		fprintf(stderr, "Malformed revision: %s\n", rev);
		debug((stderr, " BAD\n", e->rev));
		return (0);
	}
}

/*
 * Find the first delta (in this LOD) that is > date.
 *
 * XXX - if there are two children in the same LOD, this finds only one of them.
 */
private delta *
findDate(delta *d, time_t date)
{
	time_t	d2;
	delta	*tmp, *tmp2;

	if (!d) return (0);
	d2 = DATE(d);
	if (d2 >= date) return (d);
	for (tmp = d->kid; tmp; tmp = tmp->siblings) {
		if (samebranch(tmp, d) && (tmp2 = findDate(tmp, date))) {
			return (tmp2);
		}
	}
	return (0);
}

/*
 * Take either a revision or date/symbol and return the delta.
 *
 * Date tokens may have a prefix of "+" or "-" to imply rounding direction.
 * If there is no prefix, then the roundup variable is used.
 */
delta *
sccs_getrev(sccs *sc, char *rev, char *dateSym, int roundup)
{
	delta	*tmp, *d = 0;
	time_t	date;
	char	*s = rev ? rev : dateSym;
	char	ru = 0;

	unless (sc && sc->table) return (0);

	/*
	 * Strip off prefix.
	 */
	if (s && *s) {
		switch (*s) {
		    case '+':
			ru = *s;
			roundup = ROUNDUP;
			s++;
			break;
		    case '-':
			ru = *s;
			roundup = ROUNDDOWN;
			s++;
			break;
		}
	}

	/* Allow null revisions to mean TOT or first delta */
	if (!s || !*s) {
		unless (sc->state & S_RANGE2) {	/* this is first call */
			unless (ru == '+') return (sc->tree);
		}
		return (findrev(sc, 0));
	}

	/*
	 * If it's a revision, go find it and use it.
	 */
	if (rev) {
		unless (d = findrev(sc, s)) return (0);
		return (d);
	}

	/*
	 * If it is a symbol, then just go get that delta and return it.
	 */
	unless (isdigit(*s)) return (sym2delta(sc, s));

	/*
	 * It's a plain date.  Convert it to a number and then go
	 * find the closest delta.  If there is an exact match,
	 * return that.
	 * Depending on which call this, we think a little differently about
	 * endpoints.
	 * The order returned is rstart == 1.1 and rstop == 1.5, i.e.,
	 * oldest delta .. newest.
	 */
	date = date2time(s, 0, roundup);

	unless (sc->state & S_RANGE2) {	/* first call */
		if (date < sc->tree->date) return (sc->tree);
		if (date > sc->table->date) return (0);
	} else {			/* second call */
		if (date > sc->table->date) return (sc->table);
		if (date < sc->tree->date) {
			sc->rstart = 0;
			return (0);
		}
		if (sc->rstart && (date < sc->rstart->date)) {
			sc->rstart = 0;
			return (0);
		}
	}
	/* Walking the table newest .. oldest order */
	for (tmp = 0, d = sc->table; d; tmp = d, d = d->next) {
		unless (d->type == 'D') continue;
		if (d->date == date) return (d);
		/*
		 *                v date
		 * big date   1.4   1.3   1.2    little date
		 *             ^tmp  ^d
		 */
		if (d->date < date) {
			unless (sc->state & S_RANGE2) {	/* first call */
				return (tmp);
			} else {
				return (d);
			}
		}
	}
	return (tmp);
}

private inline int
isbranch(delta *d)
{
	return (d->r[2]);
}

private inline int
morekids(delta *d, int bk_mode)
{
	return (d->kid && (d->kid->type != 'R')
		&& samebranch_bk(d, d->kid, bk_mode));
}

/*
 * See if default is a version X.X or X.X.X.X
 */

private int
defIsVer(sccs *s)
{
	u16	a = 0, b = 0, c = 0, d = 0;
	int	howmany;

	unless (s->defbranch) return (0);
	howmany = scanrev(s->defbranch, &a, &b, &c, &d);
	return ((howmany == 2 || howmany == 4) ? 1 : 0);
}

/*
 * Get the delta that is the basis for this edit.
 * Get the revision name of the new delta.
 */
private delta *
getedit(sccs *s, char **revp, int branch)
{
	char	*rev = *revp;
	u16	a = 0, b = 0, c = 0, d = 0;
	delta	*e, *t;
	static	char buf[MAXREV];

	debug((stderr,
	    "getedit(%s, %s, b=%d)\n", s->gfile, notnull(*revp), branch));
	/*
	 * use the findrev logic to get to the delta.
	 */
	e = findrev(s, rev);
	debug((stderr, "getedit(e=%s)\n", e?e->rev:""));

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
	/* if BK, and no rev and default is x.x or x.x.x.x, then newlod */
	if ((s->state & S_BITKEEPER) && !rev && defIsVer(s)) {
		unless (a = sccs_nextlod(s)) {
			fprintf(stderr, "getedit: no next lod\n");
			exit(1);
		}
		sprintf(buf, "%d.1", a);
		*revp = buf;
	}
	/*
	 * Just continue trunk/branch
	 * Because the kid may be a branch, we have to be extra careful here.
	 */
	else if (!branch && !morekids(e, (s->state & S_BITKEEPER))) {
		a = e->r[0];
		b = e->r[1];
		c = e->r[2];
		d = e->r[3];
		if (!c) {
			/* Seems weird but makes -e -r2 -> 2.1 when tot is 1.x
			 */
			int	release = rev ? atoi(rev) : 1;

			if (release > a) {
				a = (s->state & S_BITKEEPER) 
				    ? sccs_nextlod(s)
				    : release;
				b = 1;
			} else {
				b++;
			}
			sprintf(buf, "%d.%d", a, b);
		} else {
			sprintf(buf, "%d.%d.%d.%d", a, b, c, d+1);
		}
		debug((stderr, "getedit1(%s) -> %s\n", notnull(rev), buf));
		*revp = buf;
	}
	else {
		/*
		 * For whatever reason (they asked, or there is a kid in
		 * the way), we need a branch.  Branches are all based,
		 * in their /name/, off of the closest trunk node.  Go
		 * backwards up the tree until we hit the trunk and then
		 * use that rev as a basis.  Because all branches are
		 * below that trunk node, we don't have to search the
		 * whole tree.
		 */

		for (t = e; isbranch(t); t = t->parent);
		R[0] = t->r[0]; R[1] = t->r[1]; R[2] = 1; R[3] = 1;
		while (_rfind(t)) R[2]++;
		sprintf(buf, "%d.%d.%d.%d", R[0], R[1], R[2], R[3]);
		debug((stderr, "getedit2(%s) -> %s\n", notnull(rev), buf));
		*revp = buf;
	}

	return (e);
}

inline int
peekc(sccs *s)
{
	if (s->encoding & E_GZIP) return (zpeekc());
	return (*s->where);
}

off_t
sccstell(sccs *s)
{
	return (s->where - s->mmap);
}

private inline char	*
fastnext(sccs *s)
{
	register char *t = s->where;
	register char *tmp = s->mmap + s->size;
	register size_t n = tmp - t;

	if (n <= 0) return (0);
	/*
	 * I tried unrolling this a couple of ways and it got worse
	 *
	 * An idea for improvement: read ahead and cache the pointers.
	 * It can be done here but be careful not to screw up s->where, that
	 * needs to stay valid, peekc and others use it.
	 */
	while (n-- && *t++ != '\n');

	tmp = s->where;
	s->where = t;
	return (tmp);
}

/*
 * uncompress data into a local buffer and return a pointer to it.
 * Callers of this interface want a \n terminated buffer, so if we get
 * to the end of the buffer and there isn't enough space, we have to
 * move and decompress some more.
 */
private inline char *
nextdata(sccs *s)
{
	unless (s->encoding & E_GZIP) return (fastnext(s));
	return (zgets());
}

/*
 * This does standard SCCS expansion, it's almost 100% here.
 * New stuff added:
 * %@%	user@host
 */
private char *
expand(sccs *s, delta *d, char *l, int *expanded)
{
	static	char buf[MAXLINE];
	char	*t = buf;
	char	*tmp, *g;
	time_t	now = 0;
	struct	tm *tm = 0;
	u16	a[4];
	int	expn = 0;

	while (*l != '\n') {
		if (l[0] != '%' || l[1] == '\n' || l[2] != '%') {
			*t++ = *l++;
			continue;
		}
		switch (l[1]) {
		    case 'A':	/* %Z%%Y% %M% %I%%Z% */
			/* XXX - no tflag */
			strcpy(t, "@(#) "); t += 5;
			tmp = basenm(s->gfile);
			strcpy(t, tmp); t += strlen(tmp); *t++ = ' ';
			strcpy(t, d->rev); t += strlen(d->rev);
			strcpy(t, "@(#)"); t += 4;
			expn = 1;
			break;

		    case 'B':	/* branch name: XXX */
			tmp = branchname(d); strcpy(t, tmp); t += strlen(tmp);
			expn = 1;
			break;

		    case 'C':	/* line number - XXX */
			*t++ = '%'; *t++ = 'C'; *t++ = '%';
			expn = 1;
			break;

		    case 'D':	/* today: 97/06/22 */
			if (!now) { time(&now); tm = localtime(&now); }
			assert(tm);
			if (s->state & S_YEAR4) {
				int	y = tm->tm_year;

				if (y < 69) y += 2000; else y += 1900;
				sprintf(t, "%4d/%02d/%02d",
				    y, tm->tm_mon+1, tm->tm_mday);
				t += 10;
			} else {
				sprintf(t, "%02d/%02d/%02d",
				    tm->tm_year, tm->tm_mon+1, tm->tm_mday);
				t += 8;
			}
			expn = 1;
			break;

		    case 'E':	/* most recent delta: 97/06/22 */
			if (s->state & S_YEAR4) {
				if (atoi(d->sdate) > 69) {
					*t++ = '1'; *t++ = '9';
				} else {
					*t++ = '2'; *t++ = '0';
				}
			}
			strncpy(t, d->sdate, 8); t += 8;
			expn = 1;
			break;

		    case 'F':	/* s.file name */
			strcpy(t, "SCCS/"); t += 5;
			tmp = basenm(s->sfile);
			strcpy(t, tmp); t += strlen(tmp);
			expn = 1;
			break;

		    case 'G':	/* most recent delta: 06/22/97 */
			*t++ = d->sdate[3]; *t++ = d->sdate[4]; *t++ = '/';
			*t++ = d->sdate[6]; *t++ = d->sdate[7]; *t++ = '/';
			if (s->state & S_YEAR4) {
				if (atoi(d->sdate) > 69) {
					*t++ = '1'; *t++ = '9';
				} else {
					*t++ = '2'; *t++ = '0';
				}
			}
			*t++ = d->sdate[0]; *t++ = d->sdate[1];
			expn = 1;
			break;

		    case 'H':	/* today: 06/22/97 */
			if (!now) { time(&now); tm = localtime(&now); }
			assert(tm);
			if (s->state & S_YEAR4) {
				int	y = tm->tm_year;

				if (y < 69) y += 2000; else y += 1900;
				sprintf(t, "%4d/%02d/%02d",
				    y, tm->tm_mon+1, tm->tm_mday);
				sprintf(t, "%02d/%02d/%04d",
				    tm->tm_mon+1, tm->tm_mday, y);
				t += 10;
			} else {
				sprintf(t, "%02d/%02d/%02d",
				    tm->tm_mon+1, tm->tm_mday, tm->tm_year);
				t += 8;
			}
			expn = 1;
			break;

		    case 'I':	/* name of revision: 1.1 or 1.1.1.1 */
			strcpy(t, d->rev); t += strlen(d->rev);
			expn = 1;
			break;

		    case 'K':	/* BitKeeper Key */
		    	t += sccs_sdelta(s, d, t);
			expn = 1;
			break;

		    case 'L':	/* 1.2.3.4 -> 2 */
			scanrev(d->rev, &a[0], &a[1], 0, 0);
			sprintf(t, "%d", a[1]); t += strlen(t);
			expn = 1;
			break;

		    case 'M':	/* mflag or filename: slib.c */
			tmp = basenm(s->gfile);
			strcpy(t, tmp); t += strlen(tmp);
			expn = 1;
			break;

		    case 'P':	/* full: /u/lm/smt/sccs/SCCS/s.slib.c */
			g = sccs2name(s->sfile);
			tmp = fullname(g, 1);
			free(g);
			strcpy(t, tmp); t += strlen(tmp);
			expn = 1;
			break;

		    case 'Q':	/* qflag */
			*t++ = '%'; *t++ = 'Q'; *t++ = '%';
			expn = 1;
			break;

		    case 'R':	/* release 1.2.3.4 -> 1 */
			scanrev(d->rev, &a[0], 0, 0, 0);
			sprintf(t, "%d", a[0]); t += strlen(t);
			expn = 1;
			break;

		    case 'S':	/* rev number: 1.2.3.4 -> 4 */
			a[3] = 0;
			scanrev(d->rev, &a[0], &a[1], &a[2], &a[3]);
			sprintf(t, "%d", a[3]); t += strlen(t);
			expn = 1;
			break;

		    case 'T':	/* time: 23:04:04 */
			if (!now) { time(&now); tm = localtime(&now); }
			assert(tm);
			sprintf(t, "%02d:%02d:%02d",
			    tm->tm_hour, tm->tm_min, tm->tm_sec);
			t += 8;
			expn = 1;
			break;

		    case 'U':	/* newest delta: 23:04:04 */
			strcpy(t, &d->sdate[9]); t += 8;
			expn = 1;
			break;

		    case 'W':	/* @(#)%M% %I%: @(#)slib.c 1.1 */
			strcpy(t, "@(#) "); t += 4;
			tmp = basenm(s->gfile);
			strcpy(t, tmp); t += strlen(tmp); *t++ = ' ';
			strcpy(t, d->rev); t += strlen(d->rev);
			expn = 1;
			break;

		    case 'Y':	/* tflag */
			*t++ = '%'; *t++ = 'Y'; *t++ = '%';
			expn = 1;
			break;

		    case 'Z':	/* @(#) */
			strcpy(t, "@(#)"); t += 4;
			expn = 1;
			break;

		    case '@':	/* user@host */
			strcpy(t, d->user);
			t += strlen(d->user);
			if (d->hostname) {
				*t++ = '@';
				strcpy(t, d->hostname);
				t += strlen(d->hostname);
			}
			expn = 1;
			break;

		    default:	t[0] = l[0];
				t[1] = l[1];
				t[2] = l[2];
				t += 3;
				break;
		}
		l += 3;
	}
	*t++ = '\n'; *t = 0;
	if (expanded) *expanded = expn;
	return (buf);
}

/*
 * This does standard RCS expansion.
 * Keywords to expand:
 *	$Revision$
 *	$Id$
 *	$Author$
 *	$Date$
 *	$Header$
 *	$Locker$
 *	$Log$		Not done
 *	$Name$
 *	$RCSfile$
 *	$Source$
 *	$State$
 */
private char *
rcsexpand(sccs *s, delta *d, char *l, int *expanded)
{
	static	char buf[MAXLINE];
	char	*t = buf;
	char	*tmp, *g;
	delta	*h;
	int	expn = 0;

	while (*l != '\n') {
		if (l[0] != '$') {
			*t++ = *l++;
			continue;
		}
		if (strneq("$Author$", l, 8)) {
			strcpy(t, "$Author: "); t += 9;
			strcpy(t, d->user);
			t += strlen(d->user);
			for (h = d; h && !h->hostname; h = h->parent);
			if (h && h->hostname) {
				*t++ = '@';
				strcpy(t, h->hostname);
				t += strlen(h->hostname);
			}
			*t++ = ' ';
			*t++ = '$';
			l += 8;
			expn = 1;
		} else if (strneq("$Date$", l, 6)) {
			strcpy(t, "$Date: "); t += 7;
			strncpy(t, d->sdate, 17); t += 17;
			for (h = d; h && !h->zone; h = h->parent);
			if (h && h->zone) {
				strcpy(t, h->zone);
				t += strlen(h->zone);
			}
			*t++ = ' ';
			*t++ = '$';
			l += 6;
			expn = 1;
		} else if (strneq("$Header$", l, 8)) {
			strcpy(t, "$Header: "); t += 9;
			tmp = s->sfile;
			strcpy(t, tmp); t += strlen(tmp); *t++ = ' ';
			strcpy(t, d->rev); t += strlen(d->rev);
			*t++ = ' ';
			strncpy(t, d->sdate, 17); t += 17;
			for (h = d; h && !h->zone; h = h->parent);
			if (h && h->zone) {
				strcpy(t, h->zone);
				t += strlen(h->zone);
			}
			*t++ = ' ';
			strcpy(t, d->user);
			t += strlen(d->user);
			for (h = d; h && !h->hostname; h = h->parent);
			if (h && h->hostname) {
				*t++ = '@';
				strcpy(t, h->hostname);
				t += strlen(h->hostname);
			}
			*t++ = ' ';
			*t++ = '$';
			l += 8;
			expn = 1;
		} else if (strneq("$Id$", l, 4)) {
			strcpy(t, "$Id: "); t += 5;
			tmp = basenm(s->sfile);
			strcpy(t, tmp); t += strlen(tmp); *t++ = ' ';
			strcpy(t, d->rev); t += strlen(d->rev);
			*t++ = ' ';
			strncpy(t, d->sdate, 17); t += 17;
			for (h = d; h && !h->zone; h = h->parent);
			if (h && h->zone) {
				strcpy(t, h->zone);
				t += strlen(h->zone);
			}
			*t++ = ' ';
			strcpy(t, d->user);
			t += strlen(d->user);
			for (h = d; h && !h->hostname; h = h->parent);
			if (h && h->hostname) {
				*t++ = '@';
				strcpy(t, h->hostname);
				t += strlen(h->hostname);
			}
			*t++ = ' ';
			*t++ = '$';
			l += 4;
			expn = 1;
		} else if (strneq("$Locker$", l, 8)) {
			strcpy(t, "$Locker: <Not implemented> $"); t += 28;
			l += 8;
			expn = 1;
		} else if (strneq("$Log$", l, 5)) {
			strcpy(t, "$Log: <Not implemented> $"); t += 25;
			l += 5;
			expn = 1;
		} else if (strneq("$Name$", l, 6)) {
			strcpy(t, "$Name: <Not implemented> $"); t += 26;
			l += 6;
			expn = 1;
		} else if (strneq("$RCSfile$", l, 9)) {
			strcpy(t, "$RCSfile: "); t += 10;
			tmp = basenm(s->sfile);
			strcpy(t, tmp); t += strlen(tmp);
			*t++ = ' '; *t++ = '$';
			l += 9;
			expn = 1;
		} else if (strneq("$Revision$", l, 10)) {
			strcpy(t, "$Revision: "); t += 11;
			strcpy(t, d->rev); t += strlen(d->rev);
			*t++ = ' ';
			*t++ = '$';
			l += 10;
			expn = 1;
		} else if (strneq("$Source$", l, 8)) {
			strcpy(t, "$Source: "); t += 9;
			g = sccs2name(s->sfile);
			tmp = fullname(g, 1);
			free(g);
			strcpy(t, tmp); t += strlen(tmp);
			*t++ = ' '; *t++ = '$';
			l += 8;
			expn = 1;
		} else if (strneq("$State$", l, 7)) {
			strcpy(t, "$State: "); t += 8;
			*t++ = ' ';
			strcpy(t, "<unknown>");
			t += 9;
			*t++ = ' '; *t++ = '$';
			l += 7;
			expn = 1;
		} else {
			*t++ = *l++;
		}
	}
	*t++ = '\n'; *t = 0;
	if (expanded) *expanded = expn;
	return (buf);
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
	if (BEEN_WARNED(s)) return;
	if (diskfull(s->sfile)) {
		fprintf(stderr, "No disk space for %s\n", s->sfile);
		return;
	}
	unless (HAS_SFILE(s)) {
		fprintf(stderr, "%s: No such file: %s\n", who, s->sfile);
		return;
	}
	unless (s->cksumok) {
		fprintf(stderr, "%s: bad checksum in %s\n", who, s->sfile);
		return;
	}
	unless (s->tree) {
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
	fprintf(stderr, "%s: %s: unknown error.\n", who, s->sfile);
}

/*
 * For each symbol, go find the real delta and point to it instead of the
 * meta delta.
 */
private void
metaSyms(sccs *sc)
{
	delta	*d;
	symbol	*sym;

	for (sym = sc->symbols; sym; sym = sym->next) {
		assert(sym->d);
		if (sym->d->type == 'D') continue;
		assert(sym->d->parent);
		for (d = sym->d->parent; d->type != 'D'; d = d->parent) {
			assert(d);
		}
		sym->d = d;
		d->flags |= D_SYMBOLS;
	}
}

/*
 * Dig meta data out of a delta.
 * The buffer looks like ^Ac<T>data where <T> is one character type.
 */
private void
meta(sccs *s, delta *d, char *buf)
{
	if (d->type != 'D') d->flags |= D_META;
	switch (buf[2]) {
	    case 'B':
		csetFileArg(d, &buf[3]);
		break;
	    case 'C':
		d->flags |= D_CSET;
		break;
	    case 'K':
		sumArg(d, &buf[3]);
		break;
	    case 'F':
		/* Do not add to date here, done in inherit */
		d->dateFudge = atoi(&buf[3]);
		break;
	    case 'H':
		hostArg(d, &buf[3]);
		break;
	    case 'M':
		mergeArg(d, &buf[3]);
		break;
	    case 'P':
		pathArg(d, &buf[3]);
		break;
	    case 'O':
		modeArg(d, &buf[3]);
		break;
	    case 'R':
		assert(s->random == 0);
	    	s->random = strnonldup(&buf[3]);
		break;
	    case 'S':
		symArg(s, d, &buf[3]);
		break;
	    case 'T':
		assert(d);
		d->flags |= D_TEXT;
		if (buf[3] == ' ') {
			d->text = addLine(d->text, strnonldup(&buf[4]));
		}
		break;
	    case 'X':
		assert(d);
		d->flags |= D_XFLAGS;
		d->xflags = atoi(&buf[3]);
		break;
	    case 'Z':
		zoneArg(d, &buf[3]);
		break;
	    default:
		fprintf(stderr, "Ignoring %.5s...\n", buf);
		/* got unknown field, force read only mode */
		s->state |= S_READ_ONLY;
	}
}

/*
 * Find the next delta in linear table order.
 * If you pass in 1.10, this should give you 1.11.
 */
delta	*
sccs_next(sccs *s, delta *d)
{
	delta	*e;

	if (!s || !d) return (0);
	if (d == s->table) return (0);
	for (e = d->kid ? d->kid : s->table; e->next != d; e = e->next);
	assert(e && (e->next == d));
	return (e);
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
	char	*p;
	char	tmp[100];
	int	i;
	int	line = 1;
	char	*expected = "?";
	char	*buf;

	seekto(s, 0);
	fastnext(s);			/* checksum */
	line++;
	debug((stderr, "mkgraph(%s)\n", s->sfile));
	for (;;) {
nextdelta:	unless (buf = fastnext(s)) {
bad:
			fprintf(stderr,
			    "%s: bad delta on line %d, expected `%s', "
			    "line follows:\n\t",
			    s->sfile, line, expected);
			fprintf(stderr, "``%.*s''\n", linelen(buf)-1, buf);
			sccs_freetable(s->table);
			s->table = 0;
			return;
		}
		line++;
		if (strneq(buf, "\001u\n", 3)) break;
		t = calloc(sizeof(*t), 1);
		assert(t);
		s->numdeltas++;
		t->kid = d;
		if (d)
			d->next = t;
		else
			s->table = t;
		d = t;
		/* ^As 00001/00000/00011 */
		if (buf[0] != '\001' || buf[1] != 's' || buf[2] != ' ') {
			expected = "^As ";
			goto bad;
		}
		d->added = atoiMult(&buf[3]);
		d->deleted = atoiMult(&buf[9]);
		d->same = atoiMult(&buf[15]);
		/* ^Ad D 1.2.1.1 97/05/15 23:11:46 lm 4 2 */
		/* ^Ad R 1.2.1.1 97/05/15 23:11:46 lm 4 2 */
		buf = fastnext(s);
		line++;
		if (buf[0] != '\001' || buf[1] != 'd' || buf[2] != ' ') {
			expected = "^Ad ";
			goto bad;
		}
	    /* D|R */
		d->type = buf[3];
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
		d->serial = atoi2(&p);
		if (*p != ' ') {
			expected = "^AD 1.1 98/03/17 18:32:39 user 12 ";
			goto bad;
		}
		p++;
		d->pserial = atoi(p);
		if (d->serial >= s->nextserial) s->nextserial = d->serial + 1;
		debug((stderr, "mkgraph(%s)\n", rev));
		d->rev = strdup(rev);
		explode_rev(d);
		if (d->flags & D_BADFORM) {
			expected = "1.2 or 1.2.3.4, too many dots";
			goto bad;
		}
		sprintf(tmp, "%s %s", date, time);
		d->sdate = strdup(tmp);
		d->user = strdup(user);
		for (;;) {
			if (!(buf = fastnext(s)) || buf[0] != '\001') {
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
				if ((buf[2] == '\n') || (buf[3] == '\n')) {
					continue;
				}
			}

			switch (buf[1]) {
			    case 'c':
				i = 0; goto comment;
			    case 'i':
				d->include = getserlist(s, 1, &buf[3], 0);
				break;
			    case 'x':
				d->exclude = getserlist(s, 1, &buf[3], 0);
				break;
			    case 'g':
				d->ignore = getserlist(s, 1, &buf[3], 0);
				break;
			    case 'e':
				goto nextdelta;
			    case 'm':	/* save MR's and pass them through */
				d->mr = addLine(d->mr, strnonldup(buf));
				break;
			    default:
				fprintf(stderr, "Bad file format: ");
				expected = "^A{c,i,x,g,e}";
				goto bad;
			}
		}
		/* ^Ac branch. */
		for (;;) {
			if (!(buf = fastnext(s)) || buf[0] != '\001') {
				expected = "^A";
				freeLines(d->comments);
				goto bad;
			}
			line++;
comment:		switch (buf[1]) {
			    case 'e': goto done;
			    case 'c':
				/* XXX - stdio support */
				if (buf[2] == '_') {
					s->landingpad = &buf[2];
				} else if (strneq(&buf[3], "&__", 3)) {
					s->landingpad = &buf[3];
				} else if (buf[2] != ' ') {
					meta(s, d, buf);
				} else {
					d->comments =
					    addLine(d->comments,
					    strnonldup(&buf[3]));
				}
				break;
			    default:
				expected = "^A{e,c}";
				freeLines(d->comments);
				goto bad;
			}
		}
done:		;	/* CSTYLED */
	}

	/*
	 * Convert the linear delta table into a graph.
	 * You would think that this is the place to adjust the times,
	 * but it isn't because we used to store the timezones in the flags.
	 */
	s->tree = d;
	inherit(s, flags, d);
	d = d->kid;
	s->tree->kid = 0;
	while (d) {
		delta	*therest = d->kid;

		d->kid = 0;
		dinsert(s, flags, d);
		d = therest;
	}
	if (checkrevs(s, flags) & 1) s->state |= S_BADREVS;

	/*
	 * For all the metadata nodes, go through and propogate the data up to
	 * the real node.
	 */
	metaSyms(s);

	/*
	 * The very first (1.1) delta has a landing pad in it for fast file
	 * rewrites.  We strip that out if it is here.
	 */
	d = s->tree;
	EACH(d->comments) {
		if ((d->comments[i][0] == '&') && (d->comments[i][1] == '_')) {
			free(d->comments[i]);
			d->comments[i] = 0;
			assert(d->comments[i+1] == 0);
		}
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
	for (; (buf = fastnext(s)) && !strneq(buf, "\001U\n", 3); ) {
		if (buf[0] == '\001') {
			fprintf(stderr, "%s: corrupted user section.\n",
			    s->sfile);
			return (-1);
		}
		s->usersgroups = addLine(s->usersgroups, strnonldup(buf));
	}

	/* Save the flags.  Some are handled by the flags routine; those
	 * are not handled here.
	 */
	for (; (buf = fastnext(s)) && !strneq(buf, "\001t\n", 3); ) {
		if (strneq(buf, "\001f R\n", 5)) {	/* XXX - obsolete */
			s->state |= S_RCS;
			continue;
		} else if (strneq(buf, "\001f b\n", 5)) {
			s->state |= S_BRANCHOK;
			continue;
		} else if (strneq(buf, "\001f Y\n", 5)) { /* XXX - obsolete */
			s->state |= S_YEAR4;
			continue;
		} else if (strneq(buf, "\001f x ", 5)) {
			int	bits = atoi(&buf[5]);

			if (bits & X_BITKEEPER) s->state |= S_BITKEEPER;
			if (bits & X_YEAR4) s->state |= S_YEAR4;
			if (bits & X_RCSEXPAND) s->state |= S_RCS;
			if (bits & X_EXPAND1) s->state |= S_EXPAND1;
			if (bits & X_CSETMARKED) s->state |= S_CSETMARKED;
#ifdef S_ISSHELL
			if (bits & X_ISSHELL) s->state |= S_ISSHELL;
#endif
			if (bits & X_HASH) s->state |= S_HASH;
			continue;
		} else if (strneq(buf, "\001f &", 4) ||
		    strneq(buf, "\001f z _", 6)) {	/* XXX - obsolete */
			/* We strip these now */
			continue;
		} else if (strneq(buf, "\001f e ", 5)) {
			switch (atoi(&buf[5])) {
			    case E_ASCII:
			    case E_UUENCODE:
			    case E_UUGZIP:
			    case E_GZIP:
			    case E_GZIP|E_UUENCODE:
				s->encoding = atoi(&buf[5]);
				break;
			    default:
				fprintf(stderr,
				    "sccs: don't know encoding %d, "
				    "assuming ascii\n",
				    atoi(&buf[5]));
				s->encoding = E_ASCII;
				return (-1);
			}
			continue;
		}
		if (!getflags(s, buf)) {
			s->flags = addLine(s->flags, strnonldup(&buf[3]));
		}
	}

	if (s->state & S_REINHERIT) reinherit(s, s->tree);

	/* Save descriptive text. */
	for (; (buf = fastnext(s)) && !strneq(buf, "\001T\n", 3); ) {
		s->text = addLine(s->text, strnonldup(buf));
	}
	s->data = sccstell(s);
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
 *	h	<rev> <host> hostname where checkin happened.
 *	p	<rev> <path> pathname of the revision.
 *	s	<rev> <symbol> symbolic name for the rev.
 *	w	<rev> <hh:mm> minutes west of GMT where checkin happened.
 * If we get any of the old format fields, set it up to reinherit the world.
 */
private int
getflags(sccs *s, char *buf)
{
	char	*p = &buf[5];
	char	f = buf[3];
	char	*t;
	delta	*d = 0;
	char	rev[MAXREV];

	if (buf[0] != '\001' || buf[1] != 'f' || buf[2] != ' ') return (0);
	t = gettok(&p);
	assert(t);
	if ((f != 's') && (f != 'd') &&
	    (!s->tree || !(d = rfind(s, t)))) {
		/* ignore it. but keep it. */
		return (0);
	}
	if (f == 'd') {
		if (s->defbranch) free(s->defbranch);
		s->defbranch = strdup(t);
		return (1);
	}
	strcpy(rev, t);
	t = gettok(&p);
	if (!t) return (0);
	switch (f) {
	    case 'h':	/* hostname */
		s->state |= S_REINHERIT;
		hostArg(d, t);
		return (1);
	    case 'p':	/* pathname */
		s->state |= S_REINHERIT;
		pathArg(d, t);
		return (1);
	    case 's':	/* symbol */
		/*
		 * OK, this is the new way, to be consistent with the others.
		 * But I accept the old way.
		 */
		if (isdigit(rev[0])) {
	    		unless (d = rfind(s, rev)) return (0);
			return (addsym(s, d, d, rev, t));
		} else {
	    		unless (d = rfind(s, t)) return (0);
			return (addsym(s, d, d, t, rev));
		}
		return (1);
	    case 'w':	/* minutes west of GMT */
		s->state |= S_REINHERIT;
		zoneArg(d, t);
		return (1);
	}
	return (0);
}

/*
 * Read a symbol out of the old format flags section and add it to the
 * symbol table.
 * The cool thing to note is that this is the only item of this sort
 * associated with the delta and that the date is the same as this delta
 * so we can just add it to the delta's comments.
 */
private int
addsym(sccs *s, delta *d, delta *metad, char *rev, char *val)
{
	symbol	*sym, *s2, *s3;

	/* If we can't find it, just pass it through */
	if (!d && !(d = rfind(s, rev))) return (0);

	for (sym = s->symbols; sym; sym = sym->next) {
		if (streq(sym->name, val)) break;
	}
	if (sym && streq(sym->rev, rev)) {
		return (0);
	} else {
		sym = calloc(1, sizeof(*sym));
		assert(sym);
	}
	sym->rev = strdup(rev);
	sym->name = strdup(val);
	sym->d = d;
	sym->metad = metad;
	d->flags |= D_SYMBOLS;
	metad->flags |= D_SYMBOLS;
	if (!d->date) getDate(d);
	assert(d->date);

	/*
	 * Insert in sorted order, most recent first.
	 */
	if (!s->symbols || !s->symbols->d) {
		sym->next = s->symbols;
		s->symbols = sym;
	} else if (metad->date >= s->symbols->d->date) {
		sym->next = s->symbols;
		s->symbols = sym;
	} else {
		for (s3 = 0, s2 = s->symbols; s2; s3 = s2, s2 = s2->next) {
			if (!s2->d || (metad->date >= s2->d->date)) {
				sym->next = s2;
				s3->next = sym;
				break;
			}
		}
		/* insert at end */
		if (!sym->next) {
			if (s3) {
				s3->next = sym;
			} else {
				s->symbols = sym;
			}
		}
	}
	debug((stderr, "Added symbol %s->%s (%d,%d) in %s\n",
	    val, rev, d->serial, metad->serial, s->sfile));
	return (1);
}

sccs *
check_gfile(sccs *s, int flags)
{
	struct	stat sbuf;

	if (lstat(s->gfile, &sbuf) == 0) {
		unless (fileTypeOk(sbuf.st_mode))
		{
			verbose((stderr,
				"unsupported file type: %s\n", s->gfile));
err:			free(s->gfile);
			free(s->sfile);
			free(s);
			return (0);
		}
		s->state |= S_GFILE;
		s->mode = sbuf.st_mode;
		s->gtime = (flags & INIT_GTIME) ? sbuf.st_mtime : 0;
		if (S_ISLNK(sbuf.st_mode)) {
			char link[MAXPATH];
			int len;

			len = readlink(s->gfile, link, sizeof(link));
			if ((len > 0 )  && (len < sizeof(link))){
				link[len] = 0;
				if (s->symlink) free(s->symlink);
				s->symlink = strdup(link);
			} else {
				verbose((stderr,
				    "can not read sym link: %s\n", s->gfile));
				goto err;
			}
		}
	} else {
		s->state &= ~S_GFILE;
		s->mode = 0;
	}
	return (s);
}

/*
 * Initialize an SCCS file.  Do this before anything else.
 * If the file doesn't exist, the graph isn't set up.
 * It should be OK to have multiple files open at once.
 */
sccs*
sccs_init(char *name, u32 flags, char *root)
{
	sccs	*s;
	struct	stat sbuf;
	char	*t;

	platformSpecificInit(name);
	if (u_mask == 0x5eadbeef) {
		u_mask = ~umask(0);
		umask(~u_mask);
	}
	if (sccs_filetype(name) == 's') {
		s = calloc(1, sizeof(*s));
		s->sfile = strdup(sPath(name, 0));
		s->gfile = sccs2name(name);
	} else {
		fprintf(stderr, "Not an SCCS file: %s\n", name);
		return (0);
	}
	t = strrchr(s->sfile, '/');
	if (t) {
		if (streq(t, "/s.ChangeSet")) s->state |= S_HASH|S_CSET;
	} else {
		// awc -> lm : this seem to be a impossbile code path
		// if we get here, there should be no '/' in s->sfile
		// do you mean streq(s->sfile, "s.ChangeSet") ?
		if (streq(s->sfile, "SCCS/s.ChangeSet")) s->state |= S_CSET;
	}
	unless (t && (t >= s->sfile + 4) && strneq(t - 4, "SCCS/s.", 7)) {
		s->state |= S_NOSCCSDIR;
	}
	unless (check_gfile(s, flags)) return (0);
	if (lstat(s->sfile, &sbuf) == 0) {
		if (!S_ISREG(sbuf.st_mode)) {
			verbose((stderr, "Not a regular file: %s\n", s->sfile));
			free(s->gfile);
			free(s->sfile);
			free(s);
			return (0);
		}
		if (sbuf.st_size == 0) {
			verbose((stderr, "Zero length file: %s\n", s->sfile));
			free(s->gfile);
			free(s->sfile);
			free(s);
			return (0);
		}
		s->state |= S_SFILE;
		s->size = sbuf.st_size;
	}
	s->pfile = strdup(sccsXfile(s, 'p'));
	s->zfile = strdup(sccsXfile(s, 'z'));
	if (isreg(s->pfile)) s->state |= S_PFILE;
	if (isreg(s->zfile)) s->state |= S_ZFILE;
	if (root) {
		s->root = root;
		s->state |= S_CACHEROOT;
	}
	debug((stderr, "init(%s) -> %s, %s\n", name, s->sfile, s->gfile));
	s->nextserial = 1;
	s->fd = -1;
	s->mmap = (caddr_t)-1;
	if (flags & INIT_MAPWRITE) {
		sbuf.st_mode |= 0200;
		if (chmod(s->sfile, UMASK(sbuf.st_mode & 0777)) == 0) {
			s->state |= S_CHMOD;
			s->fd = open(s->sfile, O_RDWR, 0);
		} else {
			/* We might not be allowed to chmod this file, or
			 * it might not exist yet.  Turn off MAPWRITE.
			 */
			sbuf.st_mode &= ~0200;
			flags &= ~INIT_MAPWRITE;
			s->fd = open(s->sfile, O_RDONLY, 0);
		}
	} else {
		s->fd = open(s->sfile, O_RDONLY, 0);
	}
	if (s->fd >= 0) {
		int mapmode = (flags & INIT_MAPWRITE)
			? PROT_READ|PROT_WRITE
			: PROT_READ;

		debug((stderr, "Attempting map: %d for %u, %s\n",
		       s->fd, s->size, (mapmode & PROT_WRITE) ? "rw" : "ro"));

		s->mmap = mmap(0, s->size, mapmode, MAP_SHARED, s->fd, 0);
		if (s->mmap == (caddr_t)-1) {
			/* Some file systems don't support shared writable
			 * maps (smbfs).
			 * HP-UX won't let you have two to the same file.
			 */
			debug((stderr,
			       "MAP_SHARED failed, trying MAP_PRIVATE\n"));
			s->mmap =
			    mmap(0, s->size, mapmode, MAP_PRIVATE, s->fd, 0);
			s->state |= S_MAPPRIVATE;
		}
	}

	if (s->mmap == (caddr_t)-1) {
		if (errno == ENOENT) {
			/* Not an error if the file doesn't exist yet.  */
			debug((stderr, "%s doesn't exist\n", s->sfile));
			s->cksumok = -1;
			return (s);
		} else {
			fputs("sccs_init: ", stderr);
			perror(s->sfile);
			free(s->sfile);
			free(s->gfile);
			free(s->pfile);
			free(s);
			return (0);
		}
	}
	debug((stderr, "mapped %s for %d at 0x%x\n",
	    s->sfile, s->size, s->mmap));
	s->state |= S_SOPEN;
	if (((flags&INIT_NOCKSUM) == 0) && badcksum(s)) {
		return (s);
	} else {
		s->cksumok = 1;
	}
	mkgraph(s, flags);
	debug((stderr, "mkgraph found %d deltas\n", s->numdeltas));
	if (s->tree) {
		if (misc(s)) {
			sccs_free(s);
			return (0);
		}
	}
#ifndef WIN32
	signal(SIGPIPE, SIG_IGN); /* win32 platform does not have sigpipe */
#endif
#ifdef	ANSIC
	signal(SIGINT, SIG_IGN);
#else
	sig(CATCH, SIGINT);
	sig(BLOCK, SIGINT);
#endif
	return (s);
}

/*
 * Restart an sccs_init because we've changed the state of the file.
 *
 * This does not reread the delta table, if you want that, open and close the
 * file.
 */
sccs*
sccs_restart(sccs *s)
{
	struct	stat sbuf;

	assert(s);
	unless (check_gfile(s, 0)) {
bad:		sccs_free(s);
		return (0);
	}
	if (lstat(s->sfile, &sbuf) == 0) {
		if (!S_ISREG(sbuf.st_mode)) goto bad;
		if (sbuf.st_size == 0) goto bad;
		s->state |= S_SFILE;
	}
	if ((s->size != sbuf.st_size) || (s->fd == -1)) {
		char	*buf;

		if (s->fd == -1) {
			s->fd = open(s->sfile, 0, 0);
		}
		if (s->mmap != (caddr_t)-1L) munmap(s->mmap, s->size);
		s->size = sbuf.st_size;
		s->mmap = mmap(0, s->size, PROT_READ, MAP_SHARED, s->fd, 0);
		if (s->mmap != (caddr_t)-1L) s->state |= S_SOPEN;
		seekto(s, 0);
		for (; (buf = fastnext(s)) && !strneq(buf, "\001T\n", 3); );
		s->data = sccstell(s);
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

/*
 * close all open file stuff associated with an sccs structure.
 */
void
sccs_close(sccs *s)
{
	unless (s->state & S_SOPEN) return;
	munmap(s->mmap, s->size);
	close(s->fd);
	s->mmap = (caddr_t) -1;
	s->fd = -1;
	s->state &= ~S_SOPEN;
}

/*
 * Free up all resources associated with the file.
 * This is the last thing you do.
 */
void
sccs_free(sccs *s)
{
	symbol	*sym, *t;

	unless (s) return;
	sccsXfile(s, 0);
#if 0
	{ struct stat sb;
	sb.st_size = 0;
	stat(s->sfile, &sb);
	fprintf(stderr, "Closing size = %d\n", sb.st_size);
	}
#endif
	if (s->table) sccs_freetable(s->table);
	for (sym = s->symbols; sym; sym = t) {
		t = sym->next;
		if (sym->name) free(sym->name);
		if (sym->rev) free(sym->rev);
		free(sym);
	}
	if (s->sfile) free(s->sfile);
	if (s->gfile) free(s->gfile);
	if (s->zfile) free(s->zfile);
	if (s->pfile) free(s->pfile);
	if (s->state & S_CHMOD) {
		struct	stat sbuf;

		if (fstat(s->fd, &sbuf) == 0) {
			sbuf.st_mode &= ~0200;
#ifdef	ANSIC
			chmod(s->sfile, UMASK(sbuf.st_mode & 0777));
#else
			fchmod(s->fd, UMASK(sbuf.st_mode & 0777));
#endif
		}
	}
	if (s->state & S_SOPEN) sccs_close(s);
	if (s->defbranch) free(s->defbranch);
	if (s->ser2delta) free(s->ser2delta);
	freeLines(s->usersgroups);
	freeLines(s->flags);
	freeLines(s->text);
	if ((s->root) && ((s->state & S_CACHEROOT) == 0)) free(s->root);
	if (s->random) free(s->random);
	if (s->symlink) free(s->symlink);
	if (s->mdbm) mdbm_close(s->mdbm);
	free(s);
#ifdef	ANSIC
	signal(SIGINT, SIG_DFL);
#else
	sig(UNCATCH, SIGINT);
	sig(UNBLOCK, SIGINT);
#endif
}

/*
 * We want SCCS/s.foo or path/to/SCCS/s.foo
 * ATT allows s.foo or path/to/s.foo.
 * We insist on SCCS/s. unless in ATT compat mode.
 * XXX ATT compat mode sucks - it's really hard to operate on a
 * gfile named s.file .
 *
 * This returns the following:
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
	    case 'm':	/* merge files */
	    case 'p':	/* lock files */
	    case 'r':	/* resolve files */
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
#if defined(SPLIT_ROOT)
		unless (exists(sfile)) mkdirp(sfile);
#else
		mkdir(sfile, 0775);
#endif
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
	s = rindex(name, '/');
	if ((s >= name + 4) && strneq(s - 4, "SCCS/", 5)) {
		unless (sccs_filetype(name)) return (0);
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

/*
 * create SCCS/<type>.foo.c
 */
int
sccs_lock(sccs *sccs, char type)
{
	char	*s;
	int	lockfd;

	if ((type == 'z') && (sccs->state & S_READ_ONLY)) return (0);
	s = sccsXfile(sccs, type);
	lockfd =
	    open(s, O_CREAT|O_WRONLY|O_EXCL, type == 'z' ? 0444 : GROUP_MODE);

	if (lockfd >= 0) close(lockfd);
	if ((lockfd >= 0) && (type == 'z')) sccs->state |= S_ZFILE;
	debug((stderr, "lock(%s) = %d\n", sccs->sfile, lockfd >= 0));
	return (lockfd >= 0);
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
	unless (failed) sccs->state &= ~S_ZFILE;
	return (failed);
}

/*
 * Take SCCS/s.foo.c, type and return a temp copy of SCCS/<type>.foo.c
 */
private char *
sccsXfile(sccs *sccs, char type)
{
	static	char	*s;
	static	int	len;
	char	*t;

	if (type == 0) {	/* clean up so purify doesn't barf */
		if (len) free(s);
		len = 0;
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

/*
 * Get the date as YY/MM/DD HH:MM:SS.mmm
 * and get timezone as minutes west of GMT
 */
private void
date(delta *d, time_t tt)
{
	struct	tm tm;
	char	tmp[50];
	long   	seast;
	int	mwest, hwest;
	char	sign = '+';

	// XXX - fix this before release 1.0 - make it be 4 digits
	seast = localtimez(tt, &tm);
	strftime(tmp, sizeof(tmp), "%y/%m/%d %H:%M:%S", &tm);
	d->sdate = strdup(tmp);

	if (seast < 0) {
		sign = '-';
		seast = -seast;  /* now swest */
	}
	hwest = seast / 3600;
	mwest = (seast % 3600) / 60;
	sprintf(tmp, "%c%02d:%02d", sign, hwest, mwest);

	zoneArg(d, tmp);
	getDate(d);
}

/*
 * Return an at most 5 digit !0 integer.
 */
long
almostUnique(int harder)
{
	struct	timeval tv;
	int	max = 100;
	int	val;

	if (harder) max = 1000000;
	do {
		gettimeofday(&tv, 0);
		val = tv.tv_usec / 10;
	} while (max-- && !val);
	while (!val) val = time(0) / 10;
	return (val);
}

/* XXX - make this private once tkpatch is part of slib.c */
char *
now()
{
	struct	tm *tm;
	time_t	tt = time(0);
	static	char	tmp[50];

	tm = localtime(&tt);
	bzero(tmp, sizeof(tmp));
	/* XXX - timezone correction? */
	strftime(tmp, sizeof(tmp), "%y/%m/%d %H:%M:%S", tm);
	return (tmp);
}

/*
 * Expand the set of deltas already tagged with D_SET to include all
 * metadata children of those deltas.
 * This is subtle.  You might think that the inner loop was unnecessary,
 * but it isn't: we want to tag only meta deltas whose _data_ parent is
 * already tagged.  If more than one meta delta is hanging off a data delta,
 * just checking d->parent won't work.
 *
 * Returns the total number of deltas with D_SET on.
 */
int
sccs_markMeta(sccs *s)
{
	int	n;
	delta	*d, *e;
	
	for (n = 0, e = s->table; e; e = e->next) {
		if (e->flags & D_SET) n++;
		unless (e->flags & D_META) continue;
		for (d = e->parent; d && (d->type != 'D'); d = d->parent);
		if (d && (d->flags & D_SET)) {
			unless (e->flags & D_SET) {
				e->flags |= D_SET;
				n++;
			}
		}
	}
	return (n);
}


/*
 * Save a serial in an array.  If the array is out of space, reallocate it.
 * The size of the array is in array[0].
 * The serial number is stored in ascending order.
 */
ser_t *
addSerial(ser_t *space, ser_t s)
{
	int	i, j, size;
	ser_t	*tmp;

	if (!space) {
		space = calloc(16, sizeof(ser_t));
		assert(space);
		space[0] = (ser_t)16;
		space[1] = s;
		return (space);
	}

	size = (int) space[0];
	if (space[size -1]) {	/* full up, dude */
		tmp = calloc(size*2, sizeof(ser_t));
		assert(tmp);
		if (space[size - 1] < s)  {
			/* s is the largest, stick it at the end */
			memcpy(tmp, space, size * sizeof(ser_t));
			tmp[size] = s;
		} else {
			/* s is not the largest, insert it while we copy */
			for (i = j = 1; i < size;) {
				if (space[i] > s)  { tmp[j++] = s; break; }
				tmp[j++] = space[i++];
			}
			memcpy(&tmp[j], &space[i], (size - i) * sizeof(ser_t));
		}
		tmp[0] = (ser_t)(size * 2);
		free(space);
		return (tmp);
	}

	EACH(space) if (space[i] > s) break;
	if (space[i] > s) {
		/* we have a "insert", move stuff up one slot */
		for (j = i; space[j]; j++);
		assert(j <= (size - 1));
		tmp = &space[i + 1];
		memmove(tmp, &tmp[-1], (j - i) * sizeof(ser_t));
	}
	space[i] = s;
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
		d = d == stop ? 0 : d->parent;
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
		name2rev(s, &rev);
		tmp = rfind(s, rev);
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
	name2rev(s, &rev);
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
	name2rev(s, &rev);
	d = rfind(s, rev);
	t[-1] = save;

	if (!stop || !d) {
		*errp = 2;
		return (0);
	}
	for (tmp = d; tmp && tmp != stop; tmp = tmp->parent);
	if (tmp != stop) {
		*errp = 2;
		return (0);
	}
	tmp = d;
	d = d == stop ? 0 : d->parent;
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

	debug((stderr, "getserlist(%.*s)\n", linelen(s), s));
	if (isSer) {
		while (*s && *s != '\n') {
			l = addSerial(l, (ser_t) atoi(s));
			while (*s && *s != '\n' && isdigit(*s)) s++;
			while (*s && *s != '\n' && !isdigit(*s)) s++;
		}
		return (l);
	}
	for (t = walkList(sc, s, ep); !*ep && t; t = walkList(sc, 0, ep)) {
		l = addSerial(l, t->serial);
	}
	return (l);
}

/*
 * XXX - this poorly named, it doesn't use "struct serlist".
 */
private void
putserlist(sccs *sc, ser_t *s, FILE *out)
{
	int	first = 1, i;
	char	buf[20];

	if (!s) return;
	EACH(s) {
		sertoa(buf, s[i]);
		if (!first) fputmeta(sc, " ", out);
		fputmeta(sc, buf, out);
		first = 0;
	}
}

/*
 * Generate a list of serials marked with D_SET tag
 */
private ser_t *
setmap(sccs *s, int bit, int all)
{
	ser_t	*slist;
	delta	*t;

	slist = calloc(s->nextserial, sizeof(ser_t));
	assert(slist);

	for (t = s->table; t; t = t->next) {
		unless (all || (t->type == 'D')) continue;
 		assert(t->serial <= s->nextserial);
		if (t->flags & bit) {
			slist[t->serial] = 1;
		}
	}
	return (slist);
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
private ser_t *
serialmap(sccs *s, delta *d, int flags, char *iLst, char *xLst, int *errp)
{
	ser_t	*slist;
	delta	*t, *n = d;
	int	i;

	assert(d);

	slist = calloc(s->nextserial, sizeof(ser_t));
	assert(slist);

	/* initialize with iLst and xLst */
	if (iLst) {
		verbose((stderr, "Included:"));
		for (t = walkList(s, iLst, errp);
		    !*errp && t; t = walkList(s, 0, errp)) {
			verbose((stderr, " %s", t->rev));
			assert(t->serial <= s->nextserial);
			slist[t->serial] = S_INC;
 		}
		verbose((stderr, "\n"));
		if (*errp) goto bad;
	}

	if (xLst) {
		verbose((stderr, "Excluded:"));
		for (t = walkList(s, xLst, errp);
		    !*errp && t; t = walkList(s, 0, errp)) {
			assert(t->serial <= s->nextserial);
			verbose((stderr, " %s", t->rev));
			if (slist[t->serial] == S_INC)
				*errp = 3;
			else {
				slist[t->serial] = S_EXCL;
			}
 		}
		verbose((stderr, "\n"));
		if (*errp) goto bad;
 	}

	/* Use linear list, newest to oldest, looking only at 'D' */

	/* slist is used as temp storage for S_INC and S_EXC then
	 * replaced with either a 0 or a 1 depending on if in view
	 * XXX clean up use of enum values mixed with 0 and 1
	 * XXX The slist[0] has a ser_t entry ... is it needed?
	 * XXX slist has (besides slist[0]) only one of 3 to 4 values:
	 *     0, 1, S_INC, S_EXCL so it doesn't need to be ser_t?
	 */

	for (t = s->table; t; t = t->next) {
		if (t->type != 'D') continue;

 		assert(t->serial <= s->nextserial);

		/* if an ancestor and not excluded, or if included */
		if ( (t == n && slist[t->serial] != S_EXCL)
		     || slist[t->serial] == S_INC) {

			/* slist [0] = Max serial that is in slist */
			unless (slist[0])  slist[0] = t->serial;

			slist[t->serial] = 1;
			/* alter only if item hasn't been set yet */
			EACH(t->include) {
				unless(slist[t->include[i]])
					slist[t->include[i]] = S_INC;
			}
			EACH(t->exclude) {
				unless(slist[t->exclude[i]])
					slist[t->exclude[i]] = S_EXCL;
			}
		}
		else
			slist[t->serial] = 0;

		if (t == n)  n = t->parent;
	}
	return (slist);
bad:	free(slist);
	return (0);
}

private void
changestate(register serlist *state, char type, int serial)
{
	register serlist *p, **pp;
	register serlist *n;

	debug2((stderr, "chg(%c, %d)\n", type, serial));

	/* find place in linked list */
	for (pp = &(state[SLIST].next), p = *pp; p; pp = &(p->next), p = *pp) {
		if (p->serial <= serial) break;
	}

	/*
	 * Delete it if it is an 'E'.
	 */
	if (type == 'E') {	/* free this item */
		assert(p && (p->serial == serial));
		*pp = p->next;
		p->next = state[SFREE].next;
		state[SFREE].next = p;
		return;
	}

	/*
	 * Else a 'D' or an 'I', so insert it in list
	 */

	assert(!p || (p->serial < serial));
	assert(state[SFREE].next || ("Ran out of serial numbers" == 0));

	n = state[SFREE].next;
	state[SFREE].next = n->next;

	*pp = n;
	n->next = p;
	n->serial = serial;
	n->type = type;
	verify(state);
}

/*
 * Allocate (or realocate) a serial list array.	 The way this works is like
 * so:
 * [SLIST] is the allocated list, linked through .next and .prev.
 * [SLIST].serial is the number of allocated nodes.
 * [SFREE] is the free list, linked only through .next.
 * [SFREE].serial is the number of free nodes.
 */
private serlist *
allocstate(serlist *old, int oldsize, int n)
{
	serlist *s;
	int	i;

	n += 2;		/* to account for the accounting overhead */
	n += 10;	/* XXX - I'm not sure I need this but haven't walked
			 * all the paths to be sure I don't.  If I do need it,
			 * it is perhaps due to metadata being added.
			 */
	assert(!old);	/* XXX - realloc not done */
	s = malloc(n * sizeof(serlist));
	assert(s);
	for (i = 2; i < n-1; ++i) {
		s[i].next = &s[i+1];
	}
	s[i].next = 0;
	s[SFREE].next = &s[2];
	s[SFREE].serial = 0;
	s[SLIST].next = 0;
	s[SLIST].serial = 0;
	return (s);
}

private int
visitedstate(const serlist *state, const ser_t *slist)
{
	register serlist *s;

	/* Find the first not D and return serial if it is active */
	for (s = state[SLIST].next; s; s = s->next) {
		if (s->type != 'D') break;
	}

	if (s && slist[s->serial])
		return (s->serial);

	return (0);
}

/* calculate printstate using where we are (state)
 * and list of active deltas (slist)
 * return either the serial if active, or 0 if not
 */

private int
printstate(const serlist *state, const ser_t *slist)
{
	register serlist *s;

	/* Loop until any I or active D */
	for (s = state[SLIST].next; s; s = s->next) {
		unless (s->type == 'D' && !slist[s->serial])
			break;
	}

	if (s) {
		int ret = (s->type == 'I')
			? (slist[s->serial] ? s->serial : 0)
 			: 0;
		debug2((stderr, "printstate {t %c s %d p %d} = %d\n", \
			s->type, s->serial, slist[s->serial], ret));
		return (ret);
	}

	debug2((stderr, "printstate {} = 0\n"));

	return (0);
}

private void inline
fnlputs(char *buf, FILE *out)
{
	register char	*t = buf;
	char		fbuf[MAXLINE];
	register char	*p = fbuf;

	do {
		*p++ = *t;
		if (p == &fbuf[MAXLINE-1]) {
			*p = 0;
			p = fbuf;
			fputs(fbuf, out);
		}
	} while (*t && (*t++ != '\n'));
	if (p != fbuf) {
		*p = 0;
		fputs(fbuf, out);
	}
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

void
gzip_sum(void *p, u8 *buf, int len, FILE *out)
{
	register unsigned int sum = 0;
	register int i;

	for (i = 0; i < len; sum += buf[i++]);
	((sccs *)p)->cksum += (sum_t)sum;
	fwrite(buf, 1, len, out);
}

/*
 * Like fputmeta, but optionally compresses the data stream.
 * This is used for the data section exclusively.
 * Note that only the first line of the buffer is considered valid.
 */

/* These will be hung off the sccs structure in the near future.  */
static u8 data_block[8192];
static u8 *data_next = data_block;

private sum_t
fputdata(sccs *s, u8 *buf, FILE *out)
{
	unsigned int sum = 0, c;
	u8	*p, *q;

	/* Checksum up to and including the first newline
	 * or the end of the string.
	 */

	p = buf;
	q = data_next;
	for (;;) {
		c = *p++;
		if (c == '\0') break;
		sum += c;
		*q++ = c;
		if (q == &data_block[sizeof(data_block)]) {
			if (s->encoding & E_GZIP) {
				zputs((void *)s, out, data_block,
				      sizeof(data_block), gzip_sum);
			} else {
				fwrite(data_block, sizeof(data_block), 1, out);
			}
			q = data_block;
		}
		if (c == '\n') break;
	}
	data_next = q;
	unless (s->encoding & E_GZIP) s->cksum += sum;
	return (sum);
}

/* Flush out data buffered by fputdata.  */
private int
fflushdata(sccs *s, FILE *out)
{
	if (s->encoding & E_GZIP) {
		if (data_next != data_block) {
			zputs((void *)s, out, data_block,
			      data_next - data_block,
			      gzip_sum);
		}
		zputs_done((void *)s, out, gzip_sum);
	} else {
		if (data_next != data_block) {
			fwrite(data_block,
			       data_next - data_block, 1, out);
		}
	}
	data_next = data_block;
	debug((stderr, "SUM2 %u\n", s->cksum));
	if (ferror(out) || fflush(out)) return (-1);
	return (0);
}

#define	ENC(c)	((((uchar)c) & 0x3f) + ' ')
#define	DEC(c)	((((uchar)c) - ' ') & 0x3f)

inline int
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

int
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

inline int
uudecode1(register char *from, register uchar *to)
{
	register int	length = DEC(*from++);
	int	save = length;

	assert(length <= 50);
	if (!length) return (0);
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

private int
openOutput(int encode, char *file, FILE **op)
{
	char	buf[100];
	int	toStdout = streq(file, "-");

	assert(op);
	debug((stderr, "openOutput(%x, %s, %x)\n", encode, file, op));
	switch (encode) {
	    case E_ASCII:
	    case E_UUENCODE:
	    case E_GZIP:
	    case (E_GZIP|E_UUENCODE):
#ifdef SPLIT_ROOT
		unless (toStdout) {
			char *s = rindex(file, '/');
			if (s) {
				*s = 0; /* split off the file part */
				unless (exists(file)) mkdirp(file);
				*s = '/';
			}
		}
#endif
		*op = toStdout ? stdout : fopen(file, "w");
		break;
	    case E_UUGZIP:
		if (toStdout) {
			*op = popen("gzip -d", M_WRITE_B);
		} else {
			sprintf(buf, "gzip -d > %s", file);
			*op = popen(buf, M_WRITE_B);
		}
		break;
	    default:
		*op = NULL;
		debug((stderr, "openOutput = %x\n", *op));
		return (-1);
	}
	debug((stderr, "openOutput = %x\n", *op));
	return (encode == E_UUGZIP);
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
	delta	*baseRev, *d, *mRev;
	int	len  = 0;
	char	*tmp;

	unless (baseRev = findrev(s, base)) {
		fprintf(stderr,
		    "%s: can not find base rev %s in %s\n",
		    who, base, s->sfile);
err:		s->state |= S_WARNED;
		return (0);
	}
	for (d = baseRev; d; d = d->parent) {
		/* we should be the only user or other users should also tidy */
		assert(!(d->flags & D_VISITED));
		d->flags |= D_VISITED;
	}
	unless (mRev = findrev(s, rev)) {
		fprintf(stderr,
		    "%s: can not find merge rev %s in %s\n",
		    who, rev, s->sfile);
		goto err;
	}
	if (mRev->flags & D_VISITED) {
		fprintf(stderr, "%s: %s already part of %s in %s\n",
		    who, mRev->rev, baseRev->rev, s->sfile);
		goto err;
	}
	for (d = mRev; d && !(d->flags & D_VISITED); d = d->parent) {
		len += strlen(d->rev) + 1;
	}
	assert(d);
	tmp = malloc(len + 1);
	tmp[0] = 0;
	for (d = mRev; d && !(d->flags & D_VISITED); d = d->parent) {
		if (tmp[0]) strcat(tmp, ",");
		strcat(tmp, d->rev);
	}
	for (d = baseRev; d; d = d->parent) d->flags &= ~D_VISITED;
	return (tmp);
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

int
write_pfile(sccs *s, int flags, delta *d,
	char *rev, char *iLst, char *i2, char *xLst, char *mRev)
{
	int	fd, len;
	char	*tmp, *tmp2;

	if (WRITABLE(s) && !(flags & GET_SKIPGET)) {
		fprintf(stderr,
		    "Writeable %s exists, skipping it.\n", s->gfile);
		s->state |= S_WARNED;
		return (-1);
	}
	if (!sccs_lock(s, 'z')) {
		fprintf(stderr, "get: can't zlock %s\n", s->gfile);
		return (-1);
	}
	if (!sccs_lock(s, 'p')) {
		fprintf(stderr, "get: can't plock %s\n", s->gfile);
		sccs_unlock(s, 'z');
		return (-1);
	}
	fd = open(s->pfile, 2, 0);
	tmp2 = now();
	assert(sccs_getuser() != 0);
	len = strlen(d->rev)
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
		assert(!mRev);
		len += (iLst ? strlen(iLst) + 3 : 0);
	}
	tmp = malloc(len);
	sprintf(tmp, "%s %s %s %s", d->rev, rev, sccs_getuser(), tmp2);
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

char *
setupOutput(sccs *s, char *printOut, int flags, delta *d)
{
	char *f;
	static char path[1024];

	if (flags & PRINT) {
		f = printOut;
	} else if (flags & GET_PATH) {
		/* put the file in its historic location */
		assert(d->pathname);
		_relativeName(".", 1 , 0, 0, path); /* get groot */
		concat_path(path, path, d->pathname);
		f = path;
		unlink(f);
	} else {
		/* With -G/somewhere/foo.c we need to check the gfile again */
		if (WRITABLE(s) && writable(s->gfile)) {
			fprintf(stderr, "Writeable %s exists\n", s->gfile);
			s->state |= S_WARNED;
			return 0;
		}
		f = s->gfile;
		unlinkGfile(s);
	}
#ifdef SPLIT_ROOT
	unless (flags & PRINT) {
		char *p = rindex(f, '/');
		//unlinkGfile(s);
		if (p) { /* if parent dir does not exist, creat it */
			*p = 0; /* split off the file part */
			unless (exists(f)) mkdirp(f);
			*p = '/';
		}
	}
#endif
	return f;
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

	for (t = d; t; t = t->parent) {
		if (t->include || t->exclude || t->added || t->deleted) {
			return (t);
		}
	}
	return (0);
}

private sum_t
getKey(MDBM *DB, char *buf, int flags)
{
	char	*e;
	int	len;
	char	data[MAXLINE];

	for (e = buf; *e != '\n'; ++e);
	len = (char *)e - buf;
	assert(len < MAXLINE);
	if (len) strncpy(data, buf, len);
	data[len] = 0;
	unless (e = strchr(data, ' ')) {
		fprintf(stderr, "get hash: no space char in line\n");
		return (-1);
	}
	*e++ = 0;
	switch (mdbm_store_str(DB, data, e, MDBM_INSERT)) {
	    case 1:	/* key already in DB */
		return (0);
	    case -1:
		return (-1);
	    default:
		return (1);
	}
}

private int
getRegBody(sccs *s, char *printOut, int flags, delta *d,
		int *ln, char *iLst, char *xLst)
{
	serlist *state = 0;
	ser_t	*slist = 0;
	int	lines = 0, print = 0, popened = 0, error = 0;
	int	encoding = (flags&GET_ASCII) ? E_ASCII : s->encoding;
	unsigned int sum;
	FILE 	*out;
	char	*buf, *base = 0, *f;
	MDBM	*DB = 0;
	int	hash = 0;

	slist = d ? serialmap(s, d, flags, iLst, xLst, &error)
		  : setmap(s, D_SET, 0);
	if (error == 1) {
		assert(!slist);
		fprintf(stderr,
		    "Malformed include/exclude list for %s\n",
		    s->sfile);
		s->state |= S_WARNED;
		return 1;
	}
	if (error == 2) {
		assert(!slist);
		fprintf(stderr,
		    "Can't find specified rev in include/exclude list for %s\n",
		    s->sfile);
		s->state |= S_WARNED;
		return 1;
	}
	if (flags & GET_SUM) {
		flags |= NEWCKSUM;
	} else if (d && (s->state & S_BITKEEPER) && !iLst && !xLst) {
		flags |= NEWCKSUM;
	}
	/* we're changing the meaning of the file, checksum would be invalid */
	if ((s->state & S_HASH) && (flags & GET_NOHASH)) {
		flags &= ~NEWCKSUM;
	}
	if ((s->state & S_HASH) && !(flags & GET_NOHASH)) {
		hash = 1;
		unless ((encoding == E_ASCII) || (encoding == E_GZIP)) {
			fprintf(stderr, "get: has files must be ascii.\n");
			s->state |= S_WARNED;
			goto out;
		}
		unless (DB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
			fprintf(stderr, "get: bad MDBM.\n");
			s->state |= S_WARNED;
			goto out;
		}
	}

	if ((s->state & S_RCS) && (flags & GET_EXPAND)) flags |= GET_RCSEXPAND;
	/* Think carefully before changing this */
	if ((s->encoding != E_ASCII) || hash) {
		flags &= ~(GET_EXPAND|GET_RCSEXPAND|GET_PREFIX);
	}
	state = allocstate(0, 0, s->nextserial);
	if (flags & GET_MODNAME) base = basenm(s->gfile);

	unless (flags & GET_HASHONLY) {
		f = d ? setupOutput(s, printOut, flags, d) : printOut;
		unless (f) {
out:			if (slist) free(slist);
			if (state) free(state);
			if (DB) mdbm_close(DB);
			return (1);
		}
		popened = openOutput(encoding, f, &out);
		unless (out) {
			fprintf(stderr, "Can't open %s for writing\n", f);
			goto out;
		}
	}
	seekto(s, s->data);
	if (s->encoding & E_GZIP) zgets_init(s->where, s->size - s->data);
	sum = 0;
	while (buf = nextdata(s)) {
		register u8 *e;

		if (isData(buf)) {
			if (!print) continue;
			if (hash) {
				if (getKey(DB, buf, flags) == 1) {
					unless (flags & GET_HASHONLY) {
						fnlputs(buf, out);
					}
					if (flags & NEWCKSUM) {
						for (e = buf;
						    *e != '\n'; sum += *e++);
						sum += '\n';
					}
					lines++;
				}
				continue;
			}
			lines++;
			if (flags & NEWCKSUM) {
				for (e = buf; *e != '\n'; sum += *e++);
				sum += '\n';
			}
			if (flags & GET_PREFIX) {
				delta *tmp = sfind(s, (ser_t) print);

				if (flags&GET_MODNAME)
					fprintf(out, "%s\t", base);
				if (flags&GET_PREFIXDATE)
					fprintf(out, "%.8s\t", tmp->sdate);
				if (flags&GET_USER)
					fprintf(out, "%s\t", tmp->user);
				if (flags&GET_REVNUMS)
					fprintf(out, "%s\t", tmp->rev);
				if (flags&GET_LINENUM)
					fprintf(out, "%6d\t", lines);
			}
			e = buf;
			if (flags & GET_EXPAND) {
				for (e = buf; *e != '%' && *e != '\n'; e++);
				if (*e == '%') {
					int didit;
					e = expand(s, d, buf, &didit);
					if (didit && (s->state & S_EXPAND1)) {
						flags &= ~GET_EXPAND;
					}
				} else {
					e = buf;
				}
			}
			if (flags & GET_RCSEXPAND) {
				char	*t;

				for (t = buf; *t != '$' && *t != '\n'; t++);
				if (*t == '$') {
					int didit;
					e = rcsexpand(s, d, e, &didit);
					if (didit && (s->state & S_EXPAND1)) {
						flags &= ~GET_RCSEXPAND;
					}
				}
			}
			switch (encoding) {
			    case E_GZIP|E_UUENCODE:
			    case E_UUENCODE:
			    case E_UUGZIP: {
				uchar	obuf[50];
				int	n = uudecode1(e, obuf);

				fwrite(obuf, n, 1, out);
				break;
			    }
			    case E_ASCII:
			    case E_GZIP:
				fnlputs(e, out);
				break;
			}
			continue;
		}

		debug2((stderr, "%.*s", linelen(buf), buf));
		changestate(state, buf[1], atoi(&buf[3]));
		print = (d)
		    ? printstate((const serlist*)state, (const ser_t*)slist)
		    : visitedstate((const serlist*)state, (const ser_t*)slist);
	}
	if (d && (flags & NEWCKSUM) && !(flags&GET_SHUTUP) && lines) {
		delta	*z = getCksumDelta(s, d);

		if (!z || ((sum_t)sum != z->sum)) {
		    fprintf(stderr,
			"get: bad delta cksum %u:%d for %s in %s, %s\n",
			(sum_t)sum, z ? z->sum : -1, d->rev, s->sfile,
			"gotten anyway.");
		}
	}
	/* Try passing back the sum in dsum in case someone wants it */
	s->dsum = sum;

	if (s->encoding & E_GZIP) zgets_done();
	if (flags & GET_HASHONLY) {
		error = 0;
	} else {
		error = ferror(out) || fflush(out);
		if (popened) {
			pclose(out);
		} else if (flags & PRINT) {
			unless (streq("-", printOut)) fclose(out);
		} else {
			fclose(out);
		}
	}

	if (error) {
		if (diskfull(s->gfile)) {
			fprintf(stderr, "No disk space for %s\n", s->gfile);
			s->state |= S_WARNED;
		}
		unless (flags & PRINT) unlink(s->gfile);
		if (DB) mdbm_close(DB);
		return (1);
	}

	/* Win32 restriction, must do this before we chmod to read only */
	if (d && (flags&GET_DTIME) && !(flags&PRINT)){
		struct utimbuf ut;

		assert(d->sdate);
		ut.actime = ut.modtime = date2time(d->sdate, d->zone, EXACT);
		if (utime(s->gfile, &ut) != 0) {
			char msg[1024];

			sprintf(msg, "%s: Can not set modificatime; ", s->gfile);
			perror(msg);
			s->state |= S_WARNED;
			goto out;
		}
	}
	if (flags&GET_EDIT) {
		if (d->mode) {
			chmod(s->gfile, UMASK(d->mode));
		} else {
			chmod(s->gfile, UMASK(0666));
		}
	} else if (!(flags&PRINT)) {
		if (d->mode) {
			chmod(s->gfile, UMASK(d->mode & ~0222));
		} else {
			chmod(s->gfile, UMASK(0444));
		}
	}


#ifdef S_ISSHELL
	if ((s->state & S_ISSHELL) && ((flags & PRINT) == 0)) {
		char cmd[MAXPATH], *t;

		t = strrchr(s->gfile, '/');
		if (t) {
			*t = 0;
			sprintf(cmd, "cd %s; sh %s -o", s->gfile, &t[1]);
			*t = '/';
		} else  sprintf(cmd, "sh %s -o", s->gfile);
		system(cmd);
	}
#endif
	*ln = lines;
	if (slist) free(slist);
	if (state) free(state);
	if (DB) {
		if (s->mdbm) mdbm_close(s->mdbm);
		s->mdbm = DB;
	}
	return 0;
}

private int
getLinkBody(sccs *s,
	char *printOut, int flags, delta *d, int *ln)
{
	char *f = setupOutput(s, printOut, flags, d);

	unless (f) return 1;
	if (flags & PRINT) {
		int	popened;
		FILE 	*out;

		popened = openOutput(E_ASCII, f, &out);
		assert(popened == 0);
		unless (out) {
			fprintf(stderr,
				"Can't open %s for writing\n", f);
			return 1;
		}
		fprintf(out, "SYMLINK -> %s\n", d->symlink);
		unless (streq("-", f)) fclose(out);
		*ln = 1;
	} else {
		unless (symlink(d->symlink, f) == 0 ) {
			perror(f);
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
	int	lines = 0, locked = 0, error;
	char	*i2 = 0;

	debug((stderr, "get(%s, %s, %s, %s, %s, %x, %s)\n",
	    s->sfile, notnull(rev), notnull(mRev),
	    notnull(iLst), notnull(xLst), flags, printOut));
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
	unless (s->tree) {
		fprintf(stderr, "get: no/bad delta tree in %s\n", s->sfile);
		goto err;
	}
	if ((s->state & S_BADREVS) && !(flags & GET_FORCE)) {
		fprintf(stderr,
		    "get: bad revisions, run renumber on %s\n", s->sfile);
		s->state |= S_WARNED;
		goto err;
	}
	/* this has to be above the getedit() - that changes the rev */
	if (mRev) {
		char *tmp;

		tmp = sccs_impliedList(s, "get", rev, mRev);
		unless (tmp) goto err;
		i2 = strconcat(tmp, iLst, ",");
		if (i2 != tmp) free(tmp);
	}
	if (rev && streq(rev, "+")) rev = 0;
	if (flags & GET_EDIT) {
		int	f = (s->state & S_BRANCHOK) ? flags&GET_BRANCH : 0;

		d = getedit(s, &rev, f);
		if (!d) {
			fprintf(stderr, "get: can't find revision %s in %s\n",
			    notnull(rev), s->sfile);
			s->state |= S_WARNED;
		}
		if (flags & PRINT) {
			fprintf(stderr, "get: can't combine edit and print\n");
			s->state |= S_WARNED;
			d = 0;
		}
	} else {
		d = findrev(s, rev);
		if (!d) {
			fprintf(stderr,
			    "get: can't find revision like %s in %s\n",
			rev, s->sfile);
			s->state |= S_WARNED;
		}
	}
	unless (d) goto err;

	if (flags & GET_EDIT) {
		if (write_pfile(s, flags, d, rev, iLst, i2, xLst, mRev)) {
			goto err;
		}
		locked = 1;
	}
	if (flags & GET_SKIPGET) {
		/*
		 * XXX - need to think about this for various file types.
		 * Remove read only files if we are editing.
		 * Do not error if there is a writable gfile, they may have
		 * wanted that.
		 */
		if ((flags & GET_EDIT) && HAS_GFILE(s) && !IS_WRITABLE(s)) {
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
		error = getRegBody(s,
			    printOut, flags, d, &lines, i2? i2: iLst, xLst);
		break;
	    case S_IFLNK:	/* symlink */
		error = getLinkBody(s, printOut, flags, d, &lines);
		break;
	    default:
		fprintf(stderr, "get unsupported file type %d\n",
			fileType(d->mode));
		error = 1;
	}
	if (error) goto err;
	debug((stderr, "GET done\n"));

skip_get:
	if (flags & GET_EDIT) {
		sccs_unlock(s, 'z');
		s->state &= ~S_ZFILE;
	}
	if (!(flags&SILENT)) {
		fprintf(stderr, "%s %s", s->gfile, d->rev);
		if (flags & GET_EDIT) {
			fprintf(stderr, " -> %s", rev);
		}
		unless (flags & GET_SKIPGET) {
			fprintf(stderr, ": %d lines", lines);
		}
		fprintf(stderr, "\n");
	}
	if (i2) free(i2);
	return (0);
}

/*
 * cat the delta body formatted according to flags.
 */
int
sccs_cat(sccs *s, u32 flags, char *printOut)
{
	int	lines = 0, error;

	debug((stderr, "sccscat(%s, %x, %s)\n",
	    s->sfile, flags, printOut));
	unless (s->state & S_SOPEN) {
		fprintf(stderr, "sccscat: couldn't open %s\n", s->sfile);
err:		return (-1);
	}
	unless (s->cksumok) {
		fprintf(stderr, "sccscat: bad chksum on %s\n", s->sfile);
		goto err;
	}
	unless (s->tree) {
		fprintf(stderr, "sccscat: no/bad delta tree in %s\n", s->sfile);
		goto err;
	}
	if ((s->state & S_BADREVS) && !(flags & GET_FORCE)) {
		fprintf(stderr,
		    "sccscat: bad revisions, run renumber on %s\n", s->sfile);
		s->state |= S_WARNED;
		goto err;
	}

	error = getRegBody(s, printOut, flags, 0, &lines, 0, 0);
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
	FILE *in, FILE *out)
{
	char	*prefix;

	unless (count)  return (0);

	if (side == RIGHT) {
		if (type == GET_BKDIFFS) {
			fprintf(out, "I%d %d\n", *left, count);
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
	int	type = flags & (GET_DIFFS|GET_BKDIFFS|GET_HASHDIFFS);
	serlist *state = 0;
	ser_t	*slist = 0;
	ser_t	old = 0;
	delta	*d;
	int	with = 0, without = 0;
	int	count = 0, left = 0, right = 0;
	FILE	*out = 0;
	int	popened = 0;
	int	encoding = (flags&GET_ASCII) ? E_ASCII : s->encoding;
	int	error = 0;
	int	side, nextside;
	char	*buf;
	char	tmpfile[100];
	FILE	*lbuf = 0;
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
	unless (s->tree) {
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
	sprintf(tmpfile, "%s/%s-%s-%d", TMP_PATH, basenm(s->gfile), d->rev, getpid());
	popened = openOutput(encoding, printOut, &out);
	if (type == GET_HASHDIFFS) {
		int	lines = 0;
		int	f = PRINT;
		int	hash = s->state & S_HASH;
		int	set = d->flags & D_SET;
		char	b[MAXLINE];

		s->state |= S_HASH;
		d->flags |= D_SET;
		ret =
		    getRegBody(s, tmpfile, f|flags, 0, &lines, 0, 0);
		unless (hash) s->state &= ~S_HASH;
		unless (set) d->flags &= ~D_SET;
		unless ((ret == 0) && (lines != 0)) {
		    	goto done3;
		}
		unless (lbuf = fopen(tmpfile, "r")) {
			perror(tmpfile);
			ret = -1;
			goto done2;
		}
		fputs("0a0\n", out);
		while (fnext(b, lbuf)) {
			fputs("> ", out);
			fputs(b, out);
		}
		goto done2;
	}
	unless (lbuf = fopen(tmpfile, "w+")) {
		perror(tmpfile);
		fprintf(stderr, "getdiffs: couldn't open %s\n", tmpfile);
		s->state |= S_WARNED;
		return (-1);
	}
	slist = serialmap(s, d, flags, 0, 0, &error);
	state = allocstate(0, 0, s->nextserial);
	seekto(s, s->data);
	if (s->encoding & E_GZIP) zgets_init(s->where, s->size - s->data);
	side = NEITHER;
	nextside = NEITHER;

	while (buf = nextdata(s)) {
		unless (isData(buf)) {
			debug2((stderr, "%.*s", linelen(buf), buf));
			changestate(state, buf[1], atoi(&buf[3]));
			with = printstate((const serlist*)state,
					(const ser_t*)slist);
			old = slist[d->serial];
			slist[d->serial] = 0;
			without = printstate((const serlist*)state,
				    	(const ser_t*)slist);
			slist[d->serial] = old;

			nextside = with ? (without ? BOTH : RIGHT)
					: (without ? LEFT : NEITHER);
			continue;
		}
		if (nextside == NEITHER) continue;
		if (count &&
		    nextside != side && (side == LEFT || side == RIGHT)) {
			if (outdiffs(s, type,
			    side, &left, &right, count, lbuf, out)) {
				goto done;
			}
			count = 0;
		}
		side = nextside;
		switch (side) {
		    case LEFT:
		    case RIGHT:
			count++;
			if ((type == GET_DIFFS) || (side == RIGHT)) {
				fnlputs(buf, lbuf);
			}
			break;
		    case BOTH:
			left++, right++; break;
		}
	}
	if (count) { /* there is something left in the buffer */
		if (outdiffs(s, type, side, &left, &right, count, lbuf, out))
			goto done;
		count = 0;
	}
	ret = 0;
done:	if (s->encoding & E_GZIP) zgets_done();
done2:	/* for GET_HASHDIFFS, the encoding has been handled in getRegBody() */
	if (lbuf) {
		fclose(lbuf);
done3:		unlink(tmpfile);
	}
	if (popened) {
		pclose(out);
	} else {
		unless (streq("-", printOut)) fclose(out);
	}
	if (slist) free(slist);
	if (state) free(state);
	return (ret);
}

/*
 * Return true if bad cksum
 */
private int
signed_badcksum(sccs *s)
{
	register char *t;
	register char *end = s->mmap + s->size;
	register unsigned int sum = 0;
	int	filesum;

	debug((stderr, "Checking sum from %x to %x (%d)\n",
	    s->mmap + 8, end, (char*)end - s->mmap - 8));
	assert(s);
	seekto(s, 0);
	filesum = atoi(&s->mmap[2]);
	debug((stderr, "File says sum is %d\n", filesum));
	t = s->mmap + 8;
	end -= 16;
	while (t < end) {
		sum += t[0] + t[1] + t[2] + t[3] + t[4] + t[5] + t[6] + t[7] +
		    t[8] + t[9] + t[10] + t[11] + t[12] + t[13] + t[14] + t[15];
		t += 16;
	}
	end += 16;
	while (t < end) sum += *t++;
	if ((sum_t)sum == filesum) {
		s->cksumok = 1;
	} else {
		fprintf(stderr,
		    "Bad old style checksum for %s, got %d, wanted %d\n",
		    s->sfile, (sum_t)sum, filesum);
	}
	debug((stderr,
	    "%s has %s cksum\n", s->sfile, s->cksumok ? "OK" : "BAD"));
	return ((sum_t)sum != filesum);
}

/*
 * Return true if bad cksum
 */
private int
badcksum(sccs *s)
{
	register u8 *t;
	register u8 *end = s->mmap + s->size;
	register unsigned int sum = 0;
	int	filesum;

	debug((stderr, "Checking sum from %x to %x (%d)\n",
	    s->mmap + 8, end, (char*)end - s->mmap - 8));
	assert(s);
	seekto(s, 0);
	filesum = atoi(&s->mmap[2]);
	debug((stderr, "File says sum is %d\n", filesum));
	t = s->mmap + 8;
	end -= 16;
	while (t < end) {
		sum += t[0] + t[1] + t[2] + t[3] + t[4] + t[5] + t[6] + t[7] +
		    t[8] + t[9] + t[10] + t[11] + t[12] + t[13] + t[14] + t[15];
		t += 16;
	}
	end += 16;
	while (t < end) sum += *t++;
	debug((stderr, "Calculated sum is %d\n", (sum_t)sum));
	if ((sum_t)sum == filesum) {
		s->cksumok = 1;
	} else {
		if (signed_badcksum(s)) {
			fprintf(stderr,
			    "Bad checksum for %s, got %d, wanted %d\n",
			    s->sfile, (sum_t)sum, filesum);
		} else {
			fprintf(stderr,
			    "Accepting old 7 bit checksum for %s\n", s->sfile);
			return (0);
		}
	}
	debug((stderr,
	    "%s has %s cksum\n", s->sfile, s->cksumok ? "OK" : "BAD"));
	return ((sum_t)sum != filesum);
}


inline int
isAscii(c)
{
	if (c & 0x60) return (1);
	return (c == '\f') ||
	    (c == '\n') || (c == '\b') || (c == '\r') || (c == '\t');
}

/*
 * Check for bad characters in the file.
 */
private int
badchars(sccs *s)
{
	register char *t = s->mmap;
	register char *end = s->mmap + s->size;

	assert(s);
	assert(s->mmap);
	if ((t[0] != '\001') || (t[1] != 'h') || (t[7] != '\n')) {
		fprintf(stderr, "admin: bad checksum format in %s\n",
		    s->sfile);
		return (1);
	}
	for (t = s->mmap + 8; t < end; t++) {
		unless (isAscii(*t) || ((*t == '\001') && (t[-1] == '\n'))) {
			char	*r = t;

			while ((r > s->mmap) && (r[-1] != '\n')) r--;
			fprintf(stderr,
			    "admin: bad line in %s follows:\n%.*s",
			    s->sfile, linelen(r), r);
			return (1);
		}
	}
	return (0);
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
	return (streq(a->symlink, b->symlink));
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

/*
 * The table is all here in order, just print it.
 * New in Feb, '99: remove duplicates of metadata.
 */
private int
delta_table(sccs *s, FILE *out, int willfix, int fixDate)
{
	delta	*d;
	int	i;	/* used by EACH */
	int	first = willfix;
	char	buf[MAXLINE];
	char	*p;
	int	bits = 0;

	assert((s->state & S_READ_ONLY) == 0);
	assert(s->state & S_ZFILE);
	fputs("\001hXXXXX\n", out);
	s->cksum = 0;

	if (fixDate) fixNewDate(s);
	for (d = s->table; d; d = d->next) {
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
			delta *k = d;
			while(k = sccs_kid(s, k)) {
				assert(k->flags & D_GONE);
			}

			continue;
		}

		assert(d->date);
		/*
		 * XXX Whoa, nelly.  This is wrong, we must allow these if
		 * we are doing a takepatch.
		 */
		if (d->parent && (s->state & S_BITKEEPER) &&
		    (d->date <= d->parent->date)) {
		    	s->state |= S_READ_ONLY;
			fprintf(stderr,
			    "%s@%s: dates do not increase\n", s->sfile, d->rev);
			return (-1);
		}

		/* Do not change this */
		sprintf(buf, "\001s %05d/%05d/%05d\n",
		    d->added, d->deleted, d->same);
		if (strlen(buf) > 21) {
			unless (s->state & S_BITKEEPER) {
				fprintf(stderr,
				    "%s: file too large\n", s->gfile);
				return (-1);
			}
			fitCounters(buf, d->added, d->deleted, d->same);
		}
		if (first)
			fputs(buf, out);
		else
			fputmeta(s, buf, out);

		p = fmts(buf, "\001d ");
		*p++ = d->type;
		*p++ = ' ';
		p = fmts(p, sccsrev(d));
		*p++ = ' ';
		p = fmts(p, d->sdate);
		*p++ = ' ';
		p = fmts(p, d->user);
		*p++ = ' ';
		p = fmtd(p, d->serial);
		*p++ = ' ';
		p = fmtd(p, d->pserial);
		*p++ = '\n';
		*p = '\0';
		fputmeta(s, buf, out);
		if (d->include) {
			fputmeta(s, "\001i ", out);
			putserlist(s, d->include, out);
			fputmeta(s, "\n", out);
		}
		if (d->exclude) {
			fputmeta(s, "\001x ", out);
			putserlist(s, d->exclude, out);
			fputmeta(s, "\n", out);
		}
		if (d->ignore) {
			fputmeta(s, "\001g ", out);
			putserlist(s, d->ignore, out);
			fputmeta(s, "\n", out);
		}
		EACH(d->mr) {
			fputmeta(s, d->mr[i], out);
			fputmeta(s, "\n", out);
		}
		EACH(d->comments) {
			/* metadata */
			p = buf;
			if (d->comments[i][0] != '\001') {
				p = fmts(p, "\001c ");
			}
			p = fmts(p, d->comments[i]);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->csetFile && !(d->flags & D_DUPCSETFILE)) {
			p = fmts(buf, "\001cB");
			p = fmts(p, d->csetFile);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->flags & D_CSET) {
			fputmeta(s, "\001cC\n", out);
		}
		if (d->dateFudge) {
			p = fmts(buf, "\001cF");
			p = fmttt(p, d->dateFudge);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->hostname && !(d->flags & D_DUPHOST)) {
			p = fmts(buf, "\001cH");
			p = fmts(p, d->hostname);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (s->state & S_BITKEEPER) {
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
				sprintf(buf, "\001cK%05u\n", d->sum);
				fputmeta(s, buf, out);
			}
		}
		first = 0;
		if (d->merge) {
			p = fmts(buf, "\001cM");
			p = fmtd(p, d->merge);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->pathname && !(d->flags & D_DUPPATH)) {
			p = fmts(buf, "\001cP");
			p = fmts(p, d->pathname);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->flags & D_MODE) {
		    	unless (d->parent && sameMode(d->parent, d)) {
				p = fmts(buf, "\001cO");
				p = fmts(p, mode2a(d->mode));
				if (d->symlink) {
					assert(S_ISLNK(d->mode));

					*p++ = ' ';
					p = fmts(p, d->symlink);
				}
				*p++ = '\n';
				*p   = '\0';
				fputmeta(s, buf, out);
			}
		}
		if (!d->next && s->random) {
			p = fmts(buf, "\001cR");
			p = fmts(p, s->random);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->flags & D_SYMBOLS) {
			symbol	*sym;

			for (sym = s->symbols; sym; sym = sym->next) {
				unless (sym->metad == d) continue;
				p = fmts(buf, "\001cS");
				p = fmts(p, sym->name);
				*p++ = '\n';
				*p   = '\0';
				fputmeta(s, buf, out);
			}
		}
		if (d->flags & D_TEXT) {
			unless (d->text) {
				fputmeta(s, "\001cT\n", out);
			} else {
				EACH(d->text) {
					p = buf;
					p = fmts(p, "\001cT ");
					p = fmts(p, d->text[i]);
					*p++ = '\n';
					*p   = '\0';
					fputmeta(s, buf, out);
				}
			}
		}
		if (d->flags & D_XFLAGS) {
			p = fmts(buf, "\001cX");
			p = fmtu(p, d->xflags);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (d->zone && !(d->flags & D_DUPZONE)) {
			p = fmts(buf, "\001cZ");
			p = fmts(p, d->zone);
			*p++ = '\n';
			*p   = '\0';
			fputmeta(s, buf, out);
		}
		if (!d->next) {
#if LPAD_SIZE > 0
			/* Landing pad for fast rewrites */
			fputmeta(s, "\001c", out);
			for (i = 0; i < LPAD_SIZE; i += 10) {
				fputmeta(s, "__________", out);
			}
			if (s->state & S_BIGPAD) {
				for (i = 0; i < 10*LPAD_SIZE; i += 10) {
					fputmeta(s, "__________", out);
				}
			}
			fputmeta(s, "\n", out);
#endif
		}
		fputmeta(s, "\001e\n", out);
	}
	fputmeta(s, "\001u\n", out);
	EACH(s->usersgroups) {
		fputmeta(s, s->usersgroups[i], out);
		fputmeta(s, "\n", out);
	}
	fputmeta(s, "\001U\n", out);
	p = fmts(buf, "\001f e ");
	p = fmtd(p, s->encoding);
	*p++ = '\n';
	*p   = '\0';
	fputmeta(s, buf, out);
	if (s->state & S_BRANCHOK) {
		fputmeta(s, "\001f b\n", out);
	}
	if (s->state & S_BITKEEPER) bits |= X_BITKEEPER;
	if (s->state & S_YEAR4) bits |= X_YEAR4;
	if (s->state & S_RCS) bits |= X_RCSEXPAND;
	if (s->state & S_EXPAND1) bits |= X_EXPAND1;
	if (s->state & S_CSETMARKED) bits |= X_CSETMARKED;
#ifdef S_ISSHELL
	if (s->state & S_ISSHELL) bits |= X_ISSHELL;
#endif
	if (s->state & S_HASH) bits |= X_HASH;
	if (bits) {
		char	buf[40];

		p = fmts(buf, "\001f x ");
		p = fmtu(p, bits);
		*p++ = '\n';
		*p   = '\0';
		fputmeta(s, buf, out);
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
	if (fflush(out) || ferror(out)) return (-1);
	return (0);
}

/*
 * If we are trying to compare with expanded strings, do so.
 */
private inline int
expandnleq(sccs *s, delta *d, char *fbuf, char *sbuf, int flags)
{
	char	*e = fbuf;
	int expanded;

	if (s->encoding != E_ASCII) return (0);
	if (!(flags & (GET_EXPAND|GET_RCSEXPAND))) return 0;
	if (flags & GET_EXPAND) {
		e = expand(s, d, e, &expanded);
		if (s->state & S_EXPAND1) {
			if (expanded) flags &= ~GET_EXPAND;
		}
	}
	if (flags & GET_RCSEXPAND) {
		e = rcsexpand(s, d, e, &expanded);
		if (s->state & S_EXPAND1) {
			if (expanded) flags &= ~GET_RCSEXPAND;
		}
	}
	return strnleq(e, sbuf);
}

/*
 * This is an expensive call but not as expensive as running diff.
 * flags is same as get flags.
 */
int
sccs_hasDiffs(sccs *s, u32 flags)
{
	FILE	*tmp = 0;
	pfile	pf;
	serlist *state = 0;
	ser_t	*slist = 0;
	int	print = 0, different;
	delta	*d;
	char	sbuf[MAXLINE];
	char	*name = 0, *mode = "rb";
	int	tmpfile = 0;
	char	*fbuf;

#define	RET(x)	{ different = x; goto out; }

	unless (HAS_GFILE(s) && HAS_PFILE(s)) return (0);

	bzero(&pf, sizeof(pf));
	if (read_pfile("hasDiffs", s, &pf)) return (-1);
	if (pf.mRev) RET(2);
	unless (d = findrev(s, pf.oldrev)) {
		verbose((stderr, "can't find %s in %s\n", pf.oldrev, s->gfile));
		RET(-1);
	}

	/* If the file type changed, it is a diff */
	if (d->flags & D_MODE) {
		if (fileType(s->mode) != fileType(d->mode)) RET(1);
		if (S_ISLNK(s->mode)) RET(!streq(s->symlink, d->symlink));
	}

	/* If the path changed, it is a diff */
	if (d->pathname) {
		char *r = _relativeName(s->gfile, 0, 0, 1, 0);
		if (r && !patheq(d->pathname, r)) RET(1);
	}

	/*
	 * Can not enforce this assert here, gfile may be ready only
	 * due to  GET_SKIPGET
	 * assert(IS_WRITABLE(s));
	 */
	if ((s->encoding != E_ASCII) && (s->encoding != E_GZIP)) {
		tmpfile = 1;
		if (gettemp(sbuf, "getU")) RET(-1);
		name = strdup(sbuf);
		if (deflate_gfile(s, name)) {
			unlink(name);
			free(name);
			RET(-1);
		}
	} else {
		if (fix_lf(s->gfile) == -1) return (-1);
		mode = "rt";
		name = strdup(s->gfile);
	}
	unless (tmp = fopen(name, mode)) {
		verbose((stderr, "can't open %s\n", name));
		RET(-1);
	}
	assert(s->state & S_SOPEN);
	slist = serialmap(s, d, 0, 0, 0, 0);
	state = allocstate(0, 0, s->nextserial);
	seekto(s, s->data);
	if (s->encoding & E_GZIP) zgets_init(s->where, s->size - s->data);
	while (fbuf = nextdata(s)) {
		if (fbuf[0] != '\001') {
			if (!print) continue;
			unless (fnext(sbuf, tmp)) {
				debug((stderr, "diff because EOF on gfile\n"));
				RET(1);
			}
			unless (strnleq(fbuf, sbuf) ||
			    expandnleq(s, d, fbuf, sbuf, flags)) {
				debug((stderr, "diff because diff data\n"));
				RET(1);
			}
			debug2((stderr, "SAME %.*s", linelen(fbuf), fbuf));
			continue;
		}
		changestate(state, fbuf[1], atoi(&fbuf[3]));
		debug2((stderr, "%.*s\n", linelen(fbuf), fbuf));
		print = printstate((const serlist*)state, (const ser_t*)slist);
	}
	unless (fnext(sbuf, tmp)) {
		debug((stderr, "same\n"));
		RET(0);
	}
	debug((stderr, "diff because not EOF on both %s %s\n",
	    eof(s) ? "sfile done" : "sfile not done",
	    feof(tmp) ? "gfile done" : "gfile not done"));
	RET(1);
out:
	if (s->encoding & E_GZIP) zgets_done();
	if (tmp) fclose(tmp); /* must close before we unlink */
	if (name) {
		if (tmpfile) unlink(name);
		free(name);
	}
	free_pfile(&pf);
	if (slist) free(slist);
	if (state) free(state);
	return (different);
}

private inline int
hasComments(delta *d)
{
	int	i;

	EACH(d->comments) {
		if (d->comments[i][0] != '\001') return (1);
	}
	return (0);
}

/*
 * Apply the encoding to the gfile and leave it in tmpfile.
 */
private int
deflate_gfile(sccs *s, char *tmpfile)
{
	FILE	*in, *out;
	char	cmd[MAXPATH];
	int	n;

	unless (out = fopen(tmpfile, "w")) return (-1);
	switch (s->encoding & E_DATAENC) {
	    case E_UUENCODE:
		in = fopen(s->gfile, "r");
		n = uuencode(in, out);
		fclose(in);
		fclose(out);
		break;
	    case E_UUGZIP:
		sprintf(cmd, "gzip -nq4 < %s", s->gfile);
		in = popen(cmd, M_READ_B);
		uuencode(in, out);
		pclose(in);
		fclose(out);
		break;
	    default:
		assert("Bad encoding" == 0);
	}
	return (0);
}


/*
 * if the file is non-empty & not LF terminated
 * force a LF
 */
private int
fix_lf(char *gfile)
{
	int	fd;
	struct	stat sb;
	char	c;

	if (lstat(gfile, &sb)) {
		fprintf(stderr, "lstat: ");
		perror(gfile);
		return (-1);
	}
	unless (sb.st_mode & 0200) return (0);
	if (sb.st_size > 0) {
		if ((fd = open(gfile, 2, GROUP_MODE)) == -1) {
			return (0);
		}
		if (lseek(fd, sb.st_size - 1, 0) != sb.st_size - 1) {
			perror(gfile);
			close(fd); return (-1);
		} else {
			if (read(fd, &c, 1) != 1) {
				perror(gfile);
				close(fd); return (-1);
			} else if (c != '\n') {
				if (write(fd, "\n", 1) != 1) {
					perror(gfile);
					close(fd); return (-1);
				}
			}
		}
		close(fd);
	}
	return 0;
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
int
diff_gmode(sccs *s, pfile *pf)
{
	delta *d = findrev(s, pf->oldrev);

	/* If the path changed, it is a diff */
	if (d->pathname) {
		char *r = _relativeName(s->gfile, 0, 0, 1, 0);
		if (r && !patheq(d->pathname, r)) return (3);
	}

	unless (sameFileType(s, d)) return (1);
	if (S_ISLNK(s->mode)) {
		unless (streq(s->symlink, d->symlink)) {
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
	MDBM	*o = loadDB(old, 0, DB_USEFIRST);
	MDBM	*n = loadDB(new, 0, DB_USEFIRST);
	kvpair	kv;
	int	items = 0;
	int	ret;
	char	*val;

	unless (n && o && f) {
		ret = 2;
		unlink(tmpfile);
		goto out;
	}
	fputs("0a0\n", f);
	for (kv = mdbm_first(n); kv.key.dsize; kv = mdbm_next(n)) {
		if ((val = mdbm_fetch_str(o, kv.key.dptr)) &&
		    streq(val, kv.val.dptr)) {
		    	continue;
		}
		fputs("> ", f);
		fputs(kv.key.dptr, f);
		fputc(' ', f);
		fputs(kv.val.dptr, f);
		fputc('\n', f);
		items++;
	}
	if (items) {
		ret = 1;
		if (s->mdbm) mdbm_close(s->mdbm);
		s->mdbm = o;
		o = 0;
	} else {
		ret = 0;
	}
out:	if (n) mdbm_close(n);
	if (o) mdbm_close(o);
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
private int
diff_gfile(sccs *s, pfile *pf, char *tmpfile)
{
	char	old[100];	/* the version from the s.file */
	char	new[100];	/* the new file, usually s->gfile */
	int	ret;
	delta *d;

	debug((stderr, "diff_gfile(%s, %s)\n", pf->oldrev, s->gfile));
	assert(s->state & S_GFILE);
	/*
	 * set up the "new" file
	 */
	if (isRegularFile(s->mode)) {
		if ((s->encoding != E_ASCII) && (s->encoding != E_GZIP)) {
			if (gettemp(new, "getU")) return (-1);
			if (IS_WRITABLE(s)) {
				if (deflate_gfile(s, new)) {
					unlink(new);
					return (-1);
				}
			} else {
			/* XXX - I'm not sure when this would ever be used. */
				if (sccs_get(s,
				    0, 0, 0, 0, GET_ASCII|SILENT|PRINT, new)) {
					unlink(new);
					return (-1);
				}
			}
		} else {
			if (fix_lf(s->gfile) == -1) return (-1);
			strcpy(new, s->gfile);
		}
	} else { /* non regular file, e.g symlink */
		strcpy(new, DEV_NULL);
	}

	/*
	 * set up the "old" file
	 */
	d = findrev(s, pf->oldrev);
	assert(d);
	if (isRegularFile(d->mode)) {
		if (gettemp(old, "get")) return (-1);
		if (sccs_get(s, pf->oldrev, pf->mRev, pf->iLst, pf->xLst,
		    GET_ASCII|SILENT|PRINT, old)) {
			unlink(old);
			return (-1);
		}
	} else {
		strcpy(old, DEV_NULL);
	}

	/*
	 * now we do the diff
	 */
	if (s->state & S_HASH) {
		ret = diffMDBM(s, old, new, tmpfile);
	} else {
		ret = diff(old, new, DF_DIFF, tmpfile);
	}
	unless (streq(old, DEV_NULL)) unlink(old);
	if (!streq(new, s->gfile) && !streq(new, DEV_NULL)){
		unlink(new);		/* careful */
	}

	switch (ret) {
	    case 0:	/* no diffs */
		return (1);
	    case 1:	/* diffs */
		return (0);
	    case 2:	/* diff ran into problems */
		fprintf(stderr, "Arrrrg.  Diff errored\n");
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
int
diff_g(sccs *s, pfile *pf, char **tmpfile)
{
	*tmpfile = DEV_NULL;
	switch (diff_gmode(s, pf)) {
	    case 0: 		/* no mode change */
		if (!isRegularFile(s->mode)) return 1;
		*tmpfile  = tmpnam(0);
		return (diff_gfile(s, pf, *tmpfile));
	    case 2:		/* meta mode field changed */
		return 0;
	    case 3:		/* path changed */
	    case 1:		/* file type changed */
		*tmpfile  = tmpnam(0);
		if (diff_gfile(s, pf, *tmpfile) == -1) return (-1);
		return 0;
	    default:
		return -1;
	}
}

private void
unlinkGfile(sccs *s)
{
	unlink(s->gfile);	/* Careful */
	/*
	 * zero out all gfile related field
	 */
	if (s->symlink) free(s->symlink);
	s->symlink = 0;
	s->gtime = s->mode = 0;
	s->state &= ~S_GFILE;
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
	if (pf->iLst) free(pf->iLst);
	if (pf->xLst) free(pf->xLst);
	if (pf->mRev) free(pf->mRev);
	if (pf->user) free(pf->user);
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
	char	tmpfile[50];
	delta	*d;

	unless (HAS_SFILE(s)) {
		verbose((stderr, "%s not under SCCS control\n", s->gfile));
		return (0);
	}
	unless (s->tree) return (-1);

	/* clean up lock files but not gfile */
	if (flags & CLEAN_UNLOCK) {
		unlink(s->pfile);
		sccs_unlock(s, 'z');
		sccs_unlock(s, 'x');
		return (0);
	}

	unless (HAS_PFILE(s)) {
		unless (WRITABLE(s)) {
			verbose((stderr, "Clean %s\n", s->gfile));
			unless (flags & CLEAN_UNLOCK) unlinkGfile(s);
			return (0);
		}
		fprintf(stderr, "%s writable but not edited?\n", s->gfile);
		return (1);
	}
	if (flags & CLEAN_UNEDIT) {
		unlink(s->pfile);
		unless (flags & CLEAN_UNLOCK) unlinkGfile(s);
		return (0);
	}

	unless (HAS_GFILE(s)) {
		verbose((stderr, "%s not checked out\n", s->gfile));
		return (0);
	}

	if (read_pfile("clean", s, &pf)) return (1);
	unless (d = findrev(s, pf.oldrev)) {
		free_pfile(&pf);
		return (1);
	}

	unless (sameFileType(s, d)) {
		unless (flags & PRINT) {
			fprintf(stderr,
			    "%s has different file types, needs delta.\n",
			    s->gfile);
                } else {
			printf("===== %s (file type) %s vs edited =====\n",
			    s->gfile, pf.oldrev);
			printf("< %s\n-\n", mode2FileType(d->mode));
			printf("> %s\n", mode2FileType(s->mode));
 		}
		free_pfile(&pf);
		return (2);
	}

	if ((s->state & S_BITKEEPER)  &&
	    !streq(relativeName(s, 0, 1), d->pathname)) {
		unless (flags & PRINT) {
			fprintf(stderr,
			    "%s has different pathnames, needs delta.\n",
			    s->gfile);
                } else {
			printf("===== %s (pathnames) %s vs edited =====\n",
			    s->gfile, pf.oldrev);
			printf("< %s\n-\n", d->pathname);
			printf("> %s\n", relativeName(s, 0, 1));
 		}
		free_pfile(&pf);
		return (2);
	}

	if (S_ISLNK(s->mode)) {
		if (streq(s->symlink, d->symlink)) {
			verbose((stderr, "Clean %s\n", s->gfile));
			unlink(s->pfile);
			unlinkGfile(s);
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
				printf("< SYMLINK -> %s\n-\n", d->symlink);
				printf("> SYMLINK -> %s\n", s->symlink);
			}
		}
		free_pfile(&pf);
		return (2);
	}

	/*
	 * XXX - there is a bug somewhere that leaves stuff edited when it
	 * isn't.  I suspect some interactions with make, but I'm not
	 * sure.  The difference ends up being on a line with the keywords.
	 */
	if (access(s->gfile, W_OK)) {
		if (s->encoding == E_ASCII) {
			flags |= GET_EXPAND;
			if (s->state & S_RCS) flags |= GET_RCSEXPAND;
		}
	}
	if (gettemp(tmpfile, "diffg")) return (1);
	/*
	 * hasDiffs() ignores keyword expansion differences.
	 * And it's faster.
	 */
	unless (sccs_hasDiffs(s, flags)) goto nodiffs;
	switch (diff_gfile(s, &pf, tmpfile)) {
	    case 1:		/* no diffs */
nodiffs:	verbose((stderr, "Clean %s\n", s->gfile));
		unlink(s->pfile);
		unlinkGfile(s);
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
		printf(buf);
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

	switch (sccs_hasDiffs(s, flags)) {
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
 */
private int
count_lines(delta *d)
{
	if (!d) return (0);
	return (count_lines(d->parent) + d->added - d->deleted);
}

/*
 * Check the file for binary data.
 */
int
ascii(FILE *f)
{
	char	buf[4096];
	int	n, i;

	if (!f) return (1);
	n = fread(buf, 1, sizeof(buf), f);
	rewind(f);
	for (i = 0; i < n; ++i) {
		unless (isAscii(buf[i])) return (0);
	}
	return (1);
}

/*
 * Open the input for a checkin/delta.
 * The set of options we have are:
 *	{empty, stdin, file} | {cat, gzip|uuencode}
 */
private int
openInput(sccs *s, int flags, FILE **inp)
{
	char	*file = (flags&DELTA_EMPTY) ? DEV_NULL : s->gfile;
	char	buf[MAXPATH];
	char	*mode = "rb";	/* default mode is binary mode */
	int 	compress;

	unless (flags & DELTA_EMPTY) {
		unless (HAS_GFILE(s)) {
			*inp = NULL;
			return (-1);
		}
	}
	compress = s->encoding & E_GZIP;
	switch (s->encoding & E_DATAENC) {
	    default:
	    case E_ASCII:
		mode = "rt"; /* read in text mode */
		/* fall through, check if we are really ascii */
	    case E_UUENCODE:
		if (streq("-", file)) {
			*inp = stdin;
			return (0);
		}
		*inp = fopen(file, mode);
		if (((s->encoding & E_DATAENC)== E_ASCII) && ascii(*inp))
			return (0);
		s->encoding = compress | E_UUENCODE;
		return (0);
	    case E_UUGZIP:
		/* we do'nt support compressed E_UUGZIP yet */
		assert((compress & E_GZIP) == 0);
		/*
		 * Some very seat of the pants testing showed that -4 was
		 * the best time/space tradeoff.
		 */
		if (streq("-", file)) {
			*inp = popen("gzip -nq4", M_READ_B);
		} else {
			sprintf(buf, "gzip -nq4 < %s", file);
			*inp = popen(buf, M_READ_B);
		}
		return (1);
	}
}

/*
 * Do most of the initialization on a delta.
 */
delta *
sccs_dInit(delta *d, char type, sccs *s, int nodefault)
{
	if (!d) d = calloc(1, sizeof(*d));
	d->type = type;
	assert(s);
	if ((s->state & S_BITKEEPER) && (type == 'D')) d->flags |= D_CKSUM;
	unless (d->sdate) {
		if (s->gtime) {
			date(d, s->gtime);
		} else {
			date(d, time(0));
		}
	}
	if (nodefault) {
		unless (d->user) d->user = strdup("Anon");
	} else {
		unless (d->user) d->user = strdup(sccs_getuser());
		unless (d->hostname && sccs_gethost()) {
			hostArg(d, sccs_gethost());
		}
		unless (d->pathname && s) pathArg(d, relativeName(s, 0, 0));
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
				modeArg(d, "0664");
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
int
needsMode(sccs *s, delta *p)
{
	unless (p) return (1);
	unless (sameFileType(s, p)) return (1);
	unless (s->symlink) return (0);
	return (!streq(s->symlink, p->symlink));
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
			d->symlink = strdup(s->symlink);
			assert(!(d->flags & D_DUPLINK));
		}
		d->flags |= D_MODE;
	}
}

void
get_sroot(char *sfile, char *sroot)
{
	char *g;
	g = sccs2name(sfile); /* strip SCCS */
	sroot[0] = 0;
	_relativeName(g, 0, 0, 1, sroot);
	free(g);
}

/*
 * TODO: split the x.pending file by LOD
 */
private void
updatePending(sccs *s, delta *d)
{
#ifdef LATER
	int fd;
	char sRoot[1024], buf[2048];

	// XXX
	// Do not enable this until
	// we fix "sfiles -C" or cset to consume the entries in x.pending
	// This feature is need for performance only
	assert(s); assert(d);
	if (s->state & S_CSET) return;
	get_sroot(s->sfile, sRoot);
	unless (sRoot[0]) return;
	concat_path(buf, sRoot, "SCCS");

	/* should never happen, "bk setup" should have created it */
	assert(exists(buf));

	concat_path(buf, buf, "x.pending");
	fd = open(buf, O_CREAT|O_APPEND|O_WRONLY, GROUP_MODE);
	unless (fd > 0) return;
	sccs_sdelta(s, sccs_ino(s), buf);
	strcat(buf, " ");
	sccs_sdelta(s, d, &buf[strlen(buf)]);
	strcat(buf, "\n");
	if (write(fd, buf, strlen(buf)) == -1) {
		perror("Can't write to pending file");
	}
	close(fd);
#endif
}

/*
 * Check in initial sfile.
 *
 * XXX - need to make sure that they do not check in binary files in
 * gzipped format - we can't handle that yet.
 */
/* ARGSUSED */
private int
checkin(sccs *s, int flags, delta *prefilled, int nodefault, MMAP *diffs)
{
	FILE	*sfile, *gfile = 0;
	delta	*n0 = 0, *n, *first;
	int	added = 0;
	int	popened = 0, len;
	char	*t;
	char	buf[MAXLINE];
	admin	l[2];
	int	error = 0;

	assert(s);
	debug((stderr, "checkin %s %x\n", s->gfile, flags));
	unless (flags & NEWFILE) {
		verbose((stderr,
		    "%s not checked in, use -i flag.\n", s->gfile));
		sccs_unlock(s, 'z');
		if (prefilled) sccs_freetree(prefilled);
		s->state |= S_WARNED;
		return (-1);
	}
	if (!diffs && isRegularFile(s->mode)) {
		popened = openInput(s, flags, &gfile);
		unless (gfile) {
			perror(s->gfile);
			sccs_unlock(s, 'z');
			if (prefilled) sccs_freetree(prefilled);
			return (-1);
		}
	}
	/* This should never happen - the zlock should protect */
	if (exists(s->sfile)) {
		fprintf(stderr, "delta: lost checkin race on %s\n", s->sfile);
		if (prefilled) sccs_freetree(prefilled);
		if (gfile && (gfile != stdin)) {
			if (popened) pclose(gfile); else fclose(gfile);
		}
		sccs_unlock(s, 'z');
		return (-1);
	}
	/*
	 * Disallow BK_FS characters in file name
	 * ':' is used on Unix, '@' is used on Win32
	 */
	t = basenm(s->sfile);
	if (strchr(t, ':') || strchr(t, '@')) {
		fprintf(stderr,
			"delta: %s: filename must not contain \":/@\"\n" , t);
		sccs_unlock(s, 'z');
		if (prefilled) sccs_freetree(prefilled);
		s->state |= S_WARNED;
		return (-1);
	}
	/*
	 * XXX - this is bad we should use the x.file
	 */
	sfile = fopen(s->sfile, "wb"); /* open in binary mode */
	/*
	 * Do a 1.0 delta unless
	 * a) there is a init file (nodefault), or
	 * b) prefilled->rev is initialized, or
	 * c) the DELTA_EMPTY flag is set
	 */
	if (nodefault ||
	    (flags & DELTA_EMPTY) || (prefilled && prefilled->rev)) {
		first = n = prefilled ? prefilled : calloc(1, sizeof(*n));
	} else {
		first = n0 = calloc(1, sizeof(*n0));
		n0 = sccs_dInit(n0, 'D', s, nodefault);
		/*
		 * We don't do modes here.  The modes should be part of the
		 * per LOD state, so each new LOD starting from 1.0 should
		 * have new modes.
		 */
		n0->rev = strdup("1.0");
		explode_rev(n0);
		n0->serial = s->nextserial++;
		n0->next = 0;
		s->table = n0;
		n0->flags |= D_CKSUM;
		n0->sum = (unsigned short) almostUnique(1);
		dinsert(s, flags, n0);
		n = prefilled ? prefilled : calloc(1, sizeof(*n));
		n->pserial = n0->serial;
		n->next = n0;
	}
	n = sccs_dInit(n, 'D', s, nodefault);
	unless (s->mode & S_IWUSR) s->mode |= S_IWUSR;
	updMode(s, n, 0);
	if (!n->rev) n->rev = n0 ? strdup("1.1") : strdup("1.0");
	explode_rev(n);
	/*
	 * determine state to set BITKEEPER flag before dinsert
	 * XXX: don't understand why, cloned old logic because
	 * I needed to move part of the logic after dinsert
	 */
	unless (s->state & S_NOSCCSDIR) {
		t = 0;
		unless (s->state & S_CSET) {
			t = relativeName(s, 0, 0);
			assert(t);
		}
		if (s->state & S_CSET || (t && !IsFullPath(t))) {
			s->state |= S_BITKEEPER|S_CSETMARKED;
			first->flags |= D_CKSUM;
		} else {
			if (sccs_root(s)) {
				unless (first->csetFile) {
					first->csetFile = getCSetFile(s);
				}
				s->state |= S_BITKEEPER|S_CSETMARKED;
			}
		}
	}
	n->serial = s->nextserial++;
	s->table = n;
	if (n->flags & D_BADFORM) {
		sccs_unlock(s, 'z');
		if (prefilled) sccs_freetree(prefilled);
		fprintf(stderr, "checkin: bad revision: %s for %s\n",
		    n->rev, s->sfile);
		return (-1);
	} else {
		l[0].flags = 0;
	}
	dinsert(s, flags, n);
	s->numdeltas++;
	if (n->sym) {
		addsym(s, n, n, n->rev, n->sym);
		free(n->sym);
		n->sym = 0;
	}
	/* need random set before the call to sccs_sdelta */
	/* XXX: changes n, so must be after n->sym stuff */
	unless (nodefault || (flags & DELTA_PATCH)) {
		randomBits(buf);
		if (buf[0]) s->random = strdup(buf);
		if (n0) n = n0;
		unless (hasComments(n)) {
			sprintf(buf, "BitKeeper file %s",
			    fullname(s->gfile, 0));
			n->comments = addLine(n->comments, strdup(buf));
		}
	}
	unless (s->state & S_NOSCCSDIR) {
		if (s->state & S_CSET) {
			unless (first->csetFile) {
				first->sum = (unsigned short) almostUnique(1);
				first->flags |= D_ICKSUM;
				sccs_sdelta(s, first, buf);
				first->csetFile = strdup(buf);
			}
			first->flags |= D_CKSUM;
		} else {
			t = relativeName(s, 0, 0);
			assert(t);
			if (t[0] != '/') {
				unless (first->csetFile) {
					first->csetFile = getCSetFile(s);
				}
			}
		}
	}
	if (flags & DELTA_HASH) s->state |= S_HASH;
	if (delta_table(s, sfile, 1, (flags & DELTA_PATCH) == 0)) {
		error++;
		goto abort;
	}
	buf[0] = 0;
	if (s->encoding & E_GZIP) zputs_init();
	if (n0) {
		fputdata(s, "\001I 2\n", sfile);
	} else {
		fputdata(s, "\001I 1\n", sfile);
	}
	s->dsum = 0;
	if (!(flags & DELTA_PATCH) &&
	    ((s->encoding != E_ASCII) && (s->encoding != E_GZIP))) {
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
			while (fnext(buf, gfile)) {
				s->dsum += fputdata(s, buf, sfile);
				added++;
			}
			/*
			 * For ascii files, add missing \n automagically.
			 */
			len = strlen(buf);
			if (len && (buf[len - 1] != '\n')) {
				s->dsum += fputdata(s, "\n", sfile);
			}
		}
	}
	if (n0) {
		fputdata(s, "\001E 2\n", sfile);
		fputdata(s, "\001I 1\n", sfile);
		fputdata(s, "\001E 1\n", sfile);
	} else {
		fputdata(s, "\001E 1\n", sfile);
	}
	error = end(s, n, sfile, flags, added, 0, 0);
	if (gfile && (gfile != stdin)) {
		if (popened) pclose(gfile); else fclose(gfile);
	}
	if (error) {
abort:		fclose(sfile);
		unlink(s->sfile);
		sccs_unlock(s, 'z');
		return (-1);
	}
	unless (flags & DELTA_SAVEGFILE) unlinkGfile(s);	/* Careful */
	Chmod(s->sfile, 0444);
	fclose(sfile);
	if (s->state & S_BITKEEPER) updatePending(s, n);
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

	db = mdbm_open(NULL, 0, 0, 0);
	v.dsize = sizeof(ser_t);
	for (d = s->table; d; d = d->next) {
		if (d->type == 'R') continue;
		k.dptr = (void*)d->r;
		k.dsize = sizeof(d->r);
		v.dptr = (void*)&d->serial;
		if (mdbm_store(db, k, v, MDBM_INSERT)) {
			v = mdbm_fetch(db, k);
			fprintf(stderr, "%s: %s in use by serial %d and %d.\n",
			    s->sfile, d->rev, d->serial, *(ser_t*)v.dptr);
			c = 1;
		}
	}
	mdbm_close(db);
	return (c);
}

private inline int
isleaf(register delta *d)
{
	if (d->type != 'D') return (0);
	for (d = d->kid; d; d = d->siblings) {
		if (d->flags & D_GONE) continue;
		if (d->type != 'D') continue;
		if (d->r[0] == 1 || d->r[1] != 1) return (0);
	}
	return (1);
}

/*
 * Check all the BitKeeper specific stuff such as
 *	. no open branches
 */
private int
checkInvariants(sccs *s)
{
	delta	*d;
	int	tips = 0;
	u8	*lodmap = 0;
	u16	next;

	next = sccs_nextlod(s);
	/* next now equals one more than greatest that exists
	 * so last entry in array is lodmap[next-1] which is 
	 * is current max.
	 */
	unless (lodmap = calloc(next, sizeof(lodmap))) {
		perror("calloc lodmap");
		return (1);
	}

	for (d = s->table; d; d = d->next) {
		if (d->flags & D_GONE) continue;
		unless (!(d->flags & D_MERGED) && isleaf(d)) continue;
		unless (lodmap[d->r[0]]++) continue; /* first leaf OK */
		tips++;
	}

	unless (tips) {
		if (lodmap) free(lodmap);
		return (0);
	}

	for (d = s->table; d; d = d->next) {
		if (d->flags & D_GONE) continue;
		unless (!(d->flags & D_MERGED) && isleaf(d)) continue;
		unless (lodmap[d->r[0]] > 1) continue;
		fprintf(stderr, "%s: unmerged leaf %s\n", s->sfile, d->rev);
	}
	if (lodmap) free(lodmap);
	return (1);
}

/*
 * Given a graph with some deltas marked as gone (D_SET|D_GONE),
 * make sure that things will be OK with those deltas gone.
 * Checks are:
 *	. make sure each delta has no kids
 *	. make sure each delta is not included/excluded anywhere else
 */
private int
checkGone(sccs *s, int bit, char *who)
{
	ser_t	*slist = setmap(s, bit, 0);
	delta	*d;
	int	i, error = 0;

	for (d = s->table; d; d = d->next) {
		if (d->flags & bit) {
			if (d->kid && !(d->kid->flags & bit)) {
				error++;
				fprintf(stderr,
				"%s: revision %s not at tip of branch in %s.\n",
				    who, d->rev, s->sfile);
				s->state |= S_WARNED;
			}
			continue;
		}
		EACH(d->include) {
			if (slist[d->include[i]]) {
				fprintf(stderr,
				    "%s: %s:%s includes %s\n", s->sfile,
				    who, d->rev, sfind(s, d->include[i])->rev);
				error++;
				s->state |= S_WARNED;
			}
		}
		EACH (d->exclude) {
			if (slist[d->exclude[i]]) {
				fprintf(stderr,
				    "%s: %s:%s excludes %s\n", s->sfile,
				    who, d->rev, sfind(s, d->exclude[i])->rev);
				error++;
				s->state |= S_WARNED;
			}
		}
	}
	free(slist);
	return (error);
}

private int
checkrevs(sccs *s, int flags)
{
	delta	*d;
	int	e;

	for (e = 0, d = s->table; d; d = d->next) {
		e |= checkRev(s, s->sfile, d, flags);
	}
	return (e);
}

private int
checkRev(sccs *s, char *file, delta *d, int flags)
{
	int	error = 0;
	delta	*e;

	if ((d->type == 'R') || (d->flags & D_GONE)) return (0);

	if (d->flags & D_BADFORM) {
		fprintf(stderr, "%s: bad rev '%s'\n", file, d->rev);
	}

	/*
	 * Make sure that the revision is well formed.
	 */
	if (!d->r[0] || (!d->r[1] && (d->r[0] != 1)) || (d->r[2] && !d->r[3])) {
		unless (flags & ADMIN_SHUTUP) {
			fprintf(stderr, "%s: bad revision %s (parent = %s)\n",
			    file, d->rev, d->parent?d->parent->rev:"Root");
		}
		error = 1;
	}
	/*
	 * XXX - this should check for BitKeeper files.  If it is not
	 * BitKeeper and the form is 1.0, that is an error.
	 */

	/*
	 * make sure that the parent points at us.
	 */
	if (d->parent) {
		for (e = d->parent->kid; e; e = e->siblings) {
			if (e == d) break;
		}
		if (!e) {
			unless (flags & ADMIN_SHUTUP) {
				fprintf(stderr,
				    "%s: parent %s does not point to %s?!?!\n",
				    file, d->parent->rev, d->rev);
			}
			error = 1;
		}
	}

	/*
	 * Two checks here.
	 * If they are on the same branch, is the sequence numbering
	 * correct?  Handle 1.9 -> 2.1 properly.
	 */
	if (!d->parent) goto done;
	/* If a x.y.z.q release, then it's trunk node should be x.y */
	if (d->r[2]) {
		delta	*p;

		for (p = d->parent; p && p->r[3]; p = p->parent);
		if (!p) {
			unless (flags & ADMIN_SHUTUP) {
				fprintf(stderr,
				    "%s: rev %s not connected to trunk\n",
				    file, d->rev);
			}
			error = 1;
		}
		if ((p->r[0] != d->r[0]) || (p->r[1] != d->r[1])) {
			unless (flags & ADMIN_SHUTUP) {
				fprintf(stderr,
				    "%s: rev %s has incorrect parent %s\n",
				    file, d->rev, p->rev);
			}
			error = 1;
		}
		/* if it's a x.y.z.q and not a .1, then check parent */
		if ((d->r[3] > 1) && (d->parent->r[3] != d->r[3]-1)) {
			unless (flags & ADMIN_SHUTUP) {
				fprintf(stderr,
				    "%s: rev %s has incorrect parent %s\n",
				    file, d->rev, p->rev);
			}
			error = 1;
		}
#ifdef	CRAZY_WOW
		XXX - this should be an option to admin.

		/* if there is a parent, and the parent is a x.y.z.q, and
		 * this is an only child,
		 * then insist that the revs are on the same branch.
		 */
		if (d->parent && d->parent->r[2] &&
		    onlyChild(d) && !samebranch(d, d->parent)) {
			fprintf(stderr, "%s: rev %s has incorrect parent %s\n",
			    file, d->rev, d->parent->rev);
			error = 1;
		}
#endif
		/* OK */
		goto time;
	}
	/* If on the trunk and release numbers are the same,
	 * then the revisions should be in sequence.
	 */
	if (d->r[0] == d->parent->r[0]) {
		if (d->r[1] != d->parent->r[1]+1) {
			unless (flags & ADMIN_SHUTUP) {
				fprintf(stderr,
				    "%s: rev %s has incorrect parent %s\n",
				    file, d->rev, d->parent->rev);
			}
			error = 1;
		}
	} else {
		/* Otherwise, this should be a .1 node */
		if (d->r[1] != 1) {
			unless (flags & ADMIN_SHUTUP) {
				fprintf(stderr, "%s: rev %s should be a .1 rev"
				    " since parent %s is a different release\n",
				    file, d->rev, d->parent->rev);
			}
			error = 1;
		}
	}
	/* If there is a parent, make sure the dates increase. */
time:	if (d->parent && (d->date < d->parent->date)) {
		if (flags & ADMIN_TIME) {
			fprintf(stderr,
			    "%s: time goes backwards between %s and %s\n",
			    file, d->rev, d->parent->rev);
			fprintf(stderr, "\t%s: %s    %s: %s -> %d seconds\n",
			    d->rev, d->sdate, d->parent->rev, d->parent->sdate,
			    (int)(d->date - d->parent->date));
			error |= 2;
		}
	}
	/* If the dates are identical, check that the keys are sorted */
	if (d->parent && (d->date == d->parent->date)) {
		char	me[MAXPATH], parent[MAXPATH];

		sccs_sdelta(s, d, me);
		sccs_sdelta(s, d->parent, parent);
		unless (strcmp(parent, me) < 0) {
			fprintf(stderr,
			    "\t%s: %s,%s have same date and bad key order\n",
			    s->sfile, d->rev, d->parent->rev);
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
 */
private delta *
dateArg(delta *d, char *arg, int defaults)
{
	char	*save = arg;
	char	tmp[50];
	int	year, month, day, hour, minute, second, msec, hwest, mwest;
	char	sign = ' ';
	int	rcs = 0;
	int	gotZone = 0;

	if (!d) {
		d = (delta *)calloc(1, sizeof(*d));
		assert(d);
	}
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
		struct tm dummy;
		long seast;

		gotZone++;
		seast = localtimez(time(0), &dummy);
		if (seast < 0) {
			seast = -seast;
			sign = '-';
		} else {
			sign = '+';
		}

		hwest = seast / 3600;
		mwest = (seast % 3600) / 60;
	}
	sprintf(tmp, "%02d/%02d/%02d %02d:%02d:%02d",
	    year, month, day, hour, minute, second);
	d->sdate = strdup(tmp);
	if (gotZone) {
		sprintf(tmp, "%c%02d:%02d", sign, hwest, mwest);
		d->zone = strdup(tmp);
	}
	getDate(d);
	return (d);
}
#undef	getit
#undef	move

private delta *
userArg(delta *d, char *arg)
{
	char	*save = arg;

	if (!d) {
		d = (delta *)calloc(1, sizeof(*d));
		assert(d);
	}
	if (!arg || !*arg) { d->flags = D_ERROR; return (d); }
	while (*arg && (*arg++ != '@'));
	if (arg[-1] == '@') {
		arg[-1] = 0;
		if (d->hostname && !(d->flags & D_DUPHOST)) free(d->hostname);
		d->hostname = strdup(arg);
	}
	d->user = strdup(save);		/* has to be after we null the @ */
	return (d);
}

#define	ARG(field, flag, dup) \
	if (!d) { \
		d = (delta *)calloc(1, sizeof(*d)); \
		assert(d); \
	} \
	if (!arg || !*arg) { \
		d->flags |= flag; \
	} else { \
		if (d->field && !(d->flags & dup)) free(d->field); \
		d->field = strnonldup(arg); \
	} \
	return (d)

/*
 * Process the various args which we might have to save.
 * Null args are accepted in those with a "0" as arg 2.
 */
private delta *
csetFileArg(delta *d, char *arg) { ARG(csetFile, 0, D_DUPCSETFILE); }

private delta *
hostArg(delta *d, char *arg) { ARG(hostname, D_NOHOST, D_DUPHOST); }

private delta *
pathArg(delta *d, char *arg) { ARG(pathname, D_NOPATH, D_DUPPATH); }

/*
 * Handle either 0664 style or -rw-rw-r-- style.
 */
delta *
modeArg(delta *d, char *arg)
{
	unsigned int m;

	assert(d);
	if (isdigit(*arg)) {
		for (m = 0; isdigit(*arg); m <<= 3, m |= (*arg - '0'), arg++);
		m |= S_IFREG;
	} else {
		m = a2mode(arg);
		if (S_ISLNK(m))	 {
			char *p = strchr(arg , ' ');
			d->symlink = strnonldup(++p);
			assert(!(d->flags & D_DUPLINK));
		}
	}
	if (d->mode = m) d->flags |= D_MODE;
	return (d);
}

private delta *
sumArg(delta *d, char *arg)
{
	assert(d);
	d->flags |= D_CKSUM;
	d->sum = atoi(arg);
	return (d);
}

private delta *
mergeArg(delta *d, char *arg)
{
	if (!d) {
		d = (delta *)calloc(1, sizeof(*d));
		assert(d);
	}
	assert(d->merge == 0);
	assert(isdigit(arg[0]));
	d->merge = atoi(arg);
	return (d);
}


private void
symArg(sccs *s, delta *d, char *name)
{
	symbol	*sym = calloc(1, sizeof(*sym));

	assert(d);
	sym->rev = strdup(d->rev);
	sym->name = strnonldup(name);
	if (!s->symbols) {
		s->symbols = s->symTail = sym;
	} else {
		s->symTail->next = sym;
		s->symTail = sym;
	}
	/*
	 * This is temporary.  "d" gets reset to real delta once we
	 * build the graph.
	 */
	sym->d = sym->metad = d;
	d->flags |= D_SYMBOLS;

	/*
	 * If this succeeds, then ALL keys must be in long key format
	 */
	if ((s->state & S_CSET) &&
	    streq(d->rev, "1.0") && streq(name, KEY_FORMAT2)) {
	    	s->state |= S_KEY2;
	}
	return;
}

private delta *
zoneArg(delta *d, char *arg)
{
	char	buf[20];

	unless ((arg[0] == '+') || (arg[0] == '-')) {
		sprintf(buf, "-%s", arg);
		arg = buf;
	}
	ARG(zone, D_NOZONE, D_DUPZONE);
}

/*
 * Take a string with newlines in it and split it into lines.
 * Note: null comments are accepted on purpose.
 */
private delta *
commentArg(delta *d, char *arg)
{
	char	*tmp;

	if (!d) {
		d = (delta *)calloc(1, sizeof(*d));
		assert(d);
	}
	if (!arg) {
		/* don't call me unless you want one. */
		d->comments = addLine(d->comments, strdup(""));
		return (d);
	}
	while (arg && *arg) {
		tmp = arg;
		while (*arg && *arg++ != '\n');
		if (arg[-1] == '\n') arg[-1] = 0;
		d->comments = addLine(d->comments, strdup(tmp));
	}
	return (d);
}

/*
 * Explode the rev.
 */
delta *
revArg(delta *d, char *arg)
{
	if (!d) {
		d = (delta *)calloc(1, sizeof(*d));
		assert(d);
	}
	d->rev = strdup(arg);
	explode_rev(d);
	return (d);
}

/*
 * Partially fill in a delta struct.  If the delta is null, allocate one.
 * Follow all the conventions used for delta creation such that this delta
 * can be added to the tree and freed later.
 */
delta *
sccs_parseArg(delta *d, char what, char *arg, int defaults)
{
	switch (what) {
	    case 'D':	/* any part of 1998/03/09 18:23:45.123-08:00 */
		return (dateArg(d, arg, defaults));
	    case 'U':	/* user or user@host */
		return (userArg(d, arg));
	    case 'H':	/* host */
		return (hostArg(d, arg));
	    case 'P':	/* pathname */
		return (pathArg(d, arg));
	    case 'O':	/* mode */
		return (modeArg(d, arg));
	    case 'C':	/* comments - one string, possibly multi line */
		return (commentArg(d, arg));
	    case 'R':	/* 1 or 1.2 or 1.2.3 or 1.2.3.4 */
		return (revArg(d, arg));
	    case 'Z':	/* zone */
		return (zoneArg(d, arg));
	    default:
		fprintf(stderr, "Unknown user arg %c ignored.\n", what);
		return (0);
	}
}

/*
 * Return true iff the most recent matching symbol is the same.
 */
private int
dupSym(symbol *symbols, char *s, char *rev)
{
	symbol	*sym;

	for (sym = symbols; sym; sym = sym->next) {
		if (streq(sym->name, s)) break;
	}
	/* If rev isn't set, then any name match is enough */
	if (sym && !rev) return (1);
	return (sym && streq(sym->rev, rev));
}

/*
 * Try and stuff the symbol into the landing pad,
 * return 1 if it fails.
 * XXX - needs to insist on a revision.
 */
int
sccs_addSym(sccs *sc, u32 flags, char *s)
{
	char	*rev;
	delta	*n = 0, *d = 0;
	char	*t, *r;
	sum_t	sum;
	int	len;
	char	buf[1024];
	char	fudge[30];

	if (!sccs_lock(sc, 'z')) {
		fprintf(stderr, "sccs_addSym: can't zlock %s\n", sc->gfile);
		return -1;
	}
	assert((sc->state & S_READ_ONLY) == 0);
	assert(sc->state & S_ZFILE);
	assert(sc->landingpad > sc->mmap);
	assert(sc->landingpad < sc->mmap + sc->data);
	s = strdup(s);
	if ((rev = strrchr(s, ':'))) {
		*rev++ = 0;
	} else if ((rev = strrchr(s, '='))) {
		verbose((stderr,
		    "admin: SYM=REV form obsolete, using SYM:REV\n"));
		*rev++ = 0;
	} else {
		unless (d = findrev(sc, rev)) {
norev:			verbose((stderr, "admin: can't find rev %s in %s\n",
			    rev, sc->sfile));
			free(s);
			sccs_unlock(sc, 'z');
			return (-1);
		}
		rev = d->rev;
	}
	if (dupSym(sc->symbols, s, rev)) {
		verbose((stderr,
		    "admin (fast add): symbol %s exists on %s\n", s, rev));
		free(s);
		sccs_unlock(sc, 'z');
		return (-1);
	}
	unless (d || (d = findrev(sc, rev))) goto norev;
	if (!rev || !*rev) rev = d->rev;
	/*
	 * Make a new delta table entry and figure out it's length.
	 * ^As 00000/00000/00000
	 * ^Ad R 1.22 98/07/28 18:31:10 lm 22 21
	 * ^Ac S symbol
	 * ^Ae
	 */
	n = sccs_dInit(0, 'R', sc, 0);
	n->sum = almostUnique(1);
	if (n->date <= sc->table->date) {
		time_t	tdiff;
		tdiff = sc->table->date - n->date + 1;
		n->date += tdiff;
		n->dateFudge += tdiff;
	}
	if (n->dateFudge) {
		sprintf(fudge, "\001cF%ld\n", (long) n->dateFudge);
	} else {
		fudge[0] = 0;
	}
	sprintf(buf,
"\001s 00000/00000/00000\n\
\001d R %s %s %s %d %d\n\
%s\
\001cK%05lu\n\
\001cS%s\n\
\001e\n",
	    rev, n->sdate, n->user, sc->nextserial++, d->serial, 
	    fudge, almostUnique(1), s);
	sc->numdeltas++;
	/* XXX - timezone */
	len = strlen(buf);

	/*
	 * figure out how much space we have and see if we fit.
	 */
	for (t = sc->landingpad; *t != '\n'; t++);
	if ((t - sc->landingpad) <= len) {
		sc->numdeltas--;
		sc->nextserial--;
		if (n) freedelta(n);
		free(s);
		sccs_unlock(sc, 'z');
		return (EAGAIN);
	}

	/*
	 * Shift it down.  We shift everything, knowing we'll rewrite to
	 * top.
	 */
	sum = atoi(&sc->mmap[2]);
	r = sc->mmap + 8;
	t = r + len;
	memmove(t, r, sc->landingpad - r);
#define	ADD(c)	{ sum -= '_'; *t++ = c; sum += c; }
	for (r = buf, t = &sc->mmap[8]; *r; r++) ADD(*r);
	sprintf(sc->mmap, "\001h%05u", sum);
	sc->mmap[7] = '\n';	/* overwrite null */

	/*
	 * SAMBA files only support MAP_PRIVATE
	 * so we have to write it out here.
	 */
	if (sc->state & S_MAPPRIVATE) {
		lseek(sc->fd, 0, SEEK_SET);
		write(sc->fd, sc->mmap, sc->landingpad + len + 8 - sc->mmap);
	}
	verbose((stderr, "admin: fast add symbol %s->%s in %s\n",
	    s, rev, sc->sfile));
	free(s);
	sccs_unlock(sc, 'z');
	return (0);
}

private int
addSym(char *me, sccs *sc, int flags, admin *s, int *ep)
{
	int	added = 0, i, error = 0;

	/*
	 * "sym" means TOT of current LOD.
	 * "sym:" means TOT of current LOD.
	 * "sym:1.2" means that rev.
	 * "sym:1" or "sym:1.2.1" means TOT of that branch.
	 * "sym;" and the other forms mean do it only if symbol not present.
	 */
	for (i = 0; s && s[i].flags; ++i) {
		char	*rev;
		char	*sym = strdup(s[i].thing);
		delta	*d, *n;

		if ((rev = strrchr(sym, ':'))) {
			*rev++ = 0;
		}

		unless (d = findrev(sc, rev)) {
			verbose((stderr,
			    "%s: can't find %s in %s\n",
			    me, rev, sc->sfile));
sym_err:		error = 1; sc->state |= S_WARNED;
			free(sym);
			continue;
		}
		if (!rev || !*rev) rev = d->rev;
		if (isdigit(s[i].thing[0])) {
			fprintf(stderr,
			    "%s: %s: can't start with a digit.\n",
			    me, sym);
			goto sym_err;
		}
		if (dupSym(sc->symbols, sym, rev)) {
			verbose((stderr,
			    "%s: symbol %s exists on %s\n", me, sym, rev));
			goto sym_err;
		}
		n = calloc(1, sizeof(delta));
		n->next = sc->table;
		sc->table = n;
		n = sccs_dInit(n, 'R', sc, 0);
		n->sum = almostUnique(1);
		n->rev = strdup(d->rev);
		explode_rev(n);
		n->pserial = d->serial;
		n->serial = sc->nextserial++;
		sc->numdeltas++;
		dinsert(sc, 0, n);
		if (addsym(sc, d, n, rev, sym) == 0) {
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


delta *
sccs_newDelta(sccs *sc, int isNullDelta)
{
	delta *p, *n;
	char	*rev;
	int 	f = 0;  /* XXX this may be affected by LOD */

	/*
 	 * Until we fix the ChangeSet processing code
	 * we can not allow null delta in ChangeSet file
	 */
	if ((sc->state & S_CSET) && isNullDelta) {
		fprintf(stderr,
			"Can not create null delta in ChangeSet file\n");
		return 0;
	}

	p = findrev(sc, 0);
	n = calloc(1, sizeof(delta));
	n->next = sc->table;
	sc->table = n;
	n = sccs_dInit(n, 'D', sc, 0);
	rev = p->rev;
	getedit(sc, &rev, f);
	n->rev = strdup(rev);
	n->pserial = p->serial;
	n->serial = sc->nextserial++;
	sc->numdeltas++;
	if (isNullDelta) {
		n->added = n->deleted = 0;
		n->same = p->same + p->added - p->deleted;
		n->sum = (unsigned short) almostUnique(0);
		n->flags |= D_CKSUM;
	}
	dinsert(sc, 0, n);
	return n;
}

private int 
name2xflg(char *fl)
{
	if (streq(fl, "EXPAND1")) {
		return X_EXPAND1;
	} else if (streq(fl, "RCS")) {
		return X_RCSEXPAND;
	} else if (streq(fl, "YEAR4")) {
		return X_YEAR4;
	}
	assert("bad flag" == 0);
	return (0);			/* lint */
}

private delta *
addMode(char *me, sccs *sc, delta *n, char *mode, int *fixDate)
{
	char	buf[50];

	assert(mode);
	unless (n) {
		n = sccs_newDelta(sc, 1);
		*fixDate = 1;
	}
	sprintf(buf, "Change mode to %s", mode);
	n->comments = addLine(n->comments, strdup(buf));
	n = modeArg(n, mode);
	return n;
}

private delta *
changeXFlag(char *me, sccs *sc, delta *n, int add, char *flag, int *fixDate)
{
	char	buf[50];
	u32	xflags, mask;

	assert(flag);

	/*
	 * If this is the first time we touch n->xflags,
	 * initialize it from sc->state.
	 */
	unless (n && (n->flags & D_XFLAGS)) {
		xflags = 0;
		if (sc->state & S_BITKEEPER) xflags |= X_BITKEEPER;
		if (sc->state & S_RCS) xflags |= X_RCSEXPAND;
		if (sc->state & S_YEAR4) xflags |= X_YEAR4;
		if (sc->state & S_ISSHELL) xflags |= X_ISSHELL;
		if (sc->state & S_EXPAND1) xflags |= X_EXPAND1;
		if (sc->state & S_CSETMARKED) xflags |= X_CSETMARKED;
		if (sc->state & S_HASH) xflags |= X_HASH;
	} else {
		xflags = n->xflags;
	}

	mask = name2xflg(flag);
	if (add) {
		if (xflags & mask) {
			fprintf(stderr,
				"%s: %s flag is already on, ignored\n",
				me, flag);
			return n;
		} 
		xflags |= mask;
	} else {
		unless (xflags & mask) {
			fprintf(stderr,
				"%s: %s flag is already off, ignored \n",
				me, flag);
			return n;
		}
		xflags &= ~mask;
	}
	unless (n) {
		n = sccs_newDelta(sc, 1);
		*fixDate = 1; 
	}
	n->flags |= D_XFLAGS;
	n->xflags = xflags;
	sprintf(buf, "Turn %s %s flag", add ? "on": "off", flag);
	n->comments = addLine(n->comments, strdup(buf));
	return n;
}

/*
 * Translate an encoding string (e.g. "ascii") and a compression string
 * (e.g. "gzip") to a suitable value for sccs->encoding.
 */
int
sccs_encoding(sccs *sc, char *encp, char *compp)
{
	int enc, comp;

	if (encp) {
		if (streq(encp, "text")) enc = E_ASCII;
		else if (streq(encp, "ascii")) enc = E_ASCII;
		else if (streq(encp, "binary")) enc = E_UUENCODE;
		else if (streq(encp, "uugzip")) enc = E_UUGZIP;
		else {
			fprintf(stderr,	"admin: unknown encoding format %s\n",
				encp);
			return (-1);
		}
	} else if (sc) {
		enc = (sc->encoding & E_DATAENC);
	} else {
		enc = 0;
	}

	if (compp) {
		if (streq(compp, "gzip")) comp = E_GZIP;
		else if (streq(compp, "none")) comp = 0;
		else {
			fprintf(stderr, "admin: unknown compression format %s\n",
				compp);
			return (-1);
		}
	} else if (sc) {
		comp = (sc->encoding & ~E_DATAENC);
	} else {
	        comp = 0;
	}

	return (enc | comp);
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
sccs_admin(sccs *sc, delta *d, u32 flags, char *new_encp, char *new_compp,
	admin *f, admin *z, admin *u, admin *s, char *mode, char *text)
{
	FILE	*sfile = 0;
	int	new_enc, error = 0, locked = 0, i, old_enc = 0;
	char	*t;
	char	*buf;
	int	fixDate = 0;

	assert(!z); /* XXX used to be LOD item */

	new_enc = sccs_encoding(sc, new_encp, new_compp);
	if (new_enc == -1) return -1;

	debug((stderr, "new_enc is %d\n", new_enc));
	if (new_enc == (E_GZIP|E_UUGZIP)) {
		fprintf(stderr,
			"can't compress a file with E_UUGZIP encoding\n");
		error = -1; sc->state |= S_WARNED;
		return (error);
	}
	GOODSCCS(sc);
	unless (flags & (ADMIN_BK|ADMIN_FORMAT|ADMIN_GONE)) {
		unless (locked = sccs_lock(sc, 'z')) {
			verbose((stderr,
			    "admin: can't get lock on %s\n", sc->sfile));
			error = -1; sc->state |= S_WARNED;
out:
			if (sfile) fclose(sfile);
			if (locked) sccs_unlock(sc, 'z');
			debug((stderr, "admin returns %d\n", error));
			return (error);
		}
	}
#define	OUT	{ error = -1; sc->state |= S_WARNED; goto out; }

	unless (HAS_SFILE(sc)) {
		verbose((stderr, "admin: no SCCS file: %s\n", sc->sfile));
		OUT;
	}

	if ((flags & ADMIN_BK) && checkInvariants(sc)) OUT;
	if ((flags & ADMIN_GONE) && checkGone(sc, D_GONE, "admin")) OUT;
	if (flags & ADMIN_FORMAT) {
		if (checkrevs(sc, flags) || checkdups(sc) ||
		    ((flags & ADMIN_ASCII) && badchars(sc))) {
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
		verbose((stderr, "admin: %s checks out OK\n", sc->sfile));
	}
	if (flags & (ADMIN_BK|ADMIN_FORMAT)) goto out;

	if (addSym("admin", sc, flags, s, &error)) {
		flags |= NEWCKSUM;
		fixDate = 1;
	}
	if (mode) {
		d = addMode("admin", sc, d, mode, &fixDate);
		unless(d) OUT;
		flags |= NEWCKSUM;
	}

	if (text) {
		FILE	*desc;
		char	dbuf[200];

		if (!text[0]) {
			if (sc->text) {
				freeLines(sc->text);
				sc->text = 0;
				flags |= NEWCKSUM;
			}
			unless (d) {
				d = sccs_newDelta(sc, 1);
				unless(d) OUT;
				fixDate = 1;
			}
			sprintf(dbuf, "Remove Descriptive Text");
			d->comments = addLine(d->comments, strdup(dbuf));
			assert(d->text == 0);
			d->flags |= D_TEXT;
			goto user;
		}
		desc = fopen(text, "rt"); /* must be text mode */
		if (!desc) {
			fprintf(stderr, "admin: can't open %s\n", text);
			error = 1; sc->state |= S_WARNED;
			goto user;
		}
		if (sc->text) {
			freeLines(sc->text);
			sc->text = 0;
		}
		unless (d) {
			d = sccs_newDelta(sc, 1);
			unless(d) OUT;
			fixDate = 1;
		}
		sprintf(dbuf, "Change Descriptive Text");
		d->comments = addLine(d->comments, strdup(dbuf));
		d->flags |= D_TEXT;
		while (fgets(dbuf, sizeof(dbuf), desc)) {
			sc->text = addLine(sc->text, strnonldup(dbuf));
			d->text = addLine(d->text, strnonldup(dbuf));
		}
		fclose(desc);
		flags |= NEWCKSUM;
	}

user:	for (i = 0; u && u[i].flags; ++i) {
		if (sc->state & S_BITKEEPER) {
			fprintf(stderr,
			    "admin: changing user/group is not supported\n");
			OUT;
		}
		flags |= NEWCKSUM;
		if (u[i].flags & A_ADD) {
			sc->usersgroups =
			    addLine(sc->usersgroups, strdup(u[i].thing));
		} else {
			unless (removeLine(sc->usersgroups, u[i].thing)) {
				verbose((stderr,
				    "admin: user/group %s not found in %s\n",
				    u[i].thing, sc->sfile));
				error = 1; sc->state |= S_WARNED;
			}
		}
	}

	/*
	 * b	turn on branching support (S_BRANCHOK)
	 * e	encoding
	 * d	default branch (sc->defbranch)
	 * R	turn on rcs keyword expansion (S_RCS)
	 * Y	turn on 4 digit year printouts
	 *
	 * Anything else, just eat it.
	 */
	for (i = 0; f && f[i].flags; ++i) {
		int	add = f[i].flags & A_ADD;
		char	*v = &f[i].thing[1];

		flags |= NEWCKSUM;
		if (isupper(f[i].thing[0])) {
			char *fl = f[i].thing;

			v = strchr(v, '=');
			if (v) *v++ = '\0';
			if (v && *v == '\0') v = 0;

			if (streq(fl, "EXPAND1")) {
				if (v) goto noval;
				d = changeXFlag(
					"admin", sc, d, add, fl, &fixDate);
				unless(d) OUT;
				if (add)
					sc->state |= S_EXPAND1;
				else
					sc->state &= ~S_EXPAND1;
			} else if (streq(fl, "RCS")) {
				if (v) goto noval;
				d = changeXFlag(
					"admin", sc, d, add, fl, &fixDate);
				unless(d) OUT;
				if (add)
					sc->state |= S_RCS;
				else
					sc->state &= ~S_RCS;
			} else if (streq(fl, "YEAR4")) {
				if (v) goto noval;
				d = changeXFlag(
					"admin", sc, d, add, fl, &fixDate);
				unless(d) OUT;
				if (add)
					sc->state |= S_YEAR4;
				else
					sc->state &= ~S_YEAR4;
			/* Flags below are non propagated */
			} else if (streq(fl, "BK")) {
				if (v) goto noval;
				if (add)
					sc->state |= S_BITKEEPER;
				else
					sc->state &= ~S_BITKEEPER;
#ifdef S_ISSHELL
			} else if (streq(fl, "SHELL")) {
				if (v) goto noval;
				if (add)
					sc->state |= S_ISSHELL;
				else
					sc->state &= ~S_ISSHELL;
#endif
			} else if (streq(fl, "BRANCHOK")) {
				if (v) goto noval;
				if (add)
					sc->state |= S_BRANCHOK;
				else
					sc->state &= ~S_BRANCHOK;
			} else if (streq(fl, "DEFAULT")) {
				if (sc->defbranch) free(sc->defbranch);
				sc->defbranch = v ? strdup(v) : 0;
			} else if (streq(fl, "ENCODING")) {
				/* XXX Need symbolic values */
				if (v) {
					new_enc = atoi(v);
					verbose((stderr, "New encoding %d\n", new_enc));
				} else {
					fprintf(stderr,
						"admin: -fENCODING requires a value\n");
					error = 1;
					sc->state |= S_WARNED;
				}
			}
#if 0 /* Not in this tree yet... */
			else if (streq(fl, "HASH")) {
				if (v) goto noval;
				if (add)
					sc->state |= S_HASH;
				else
					sc->state &= ~S_HASH;
			}
#endif
			else {
				if (v) fprintf(stderr,
					       "admin: unknown flag %s=%s\n",
					       fl, v);
				else fprintf(stderr,
					     "admin: unknown flag %s\n", fl);

				error = 1;
				sc->state |= S_WARNED;
			}
			continue;

		noval:	fprintf(stderr,
				"admin: flag %s can't have a value\n", fl);
			error = 1;
			sc->state = S_WARNED;
		} else {
			switch (f[i].thing[0]) {
				char	buf[500];

			case 'b':
				if (add)
					sc->state |= S_BRANCHOK;
				else
					sc->state &= ~S_BRANCHOK;
				break;
			case 'd':
				if (sc->defbranch) free(sc->defbranch);
				sc->defbranch = *v ? strdup(v) : 0;
				break;
			case 'e':
				if (*v) new_enc = atoi(v);
				verbose((stderr, "New encoding %d\n", new_enc));
		   		break;
			default:
				sprintf(buf, "%c %s", v[-1], v);
				if (add) {
					sc->flags =
						addLine(sc->flags, strdup(buf));
				} else {
					unless (removeLine(sc->flags, buf)) {
						verbose((stderr,
					"admin: flag %s not found in %s\n",
							 buf, sc->sfile));
						error = 1;
						sc->state |= S_WARNED;
					}
				}
				break;
			}
		}
	}

	if ((flags & NEWCKSUM) == 0) {
		goto out;
	}

	/*
	 * Do the delta table & misc.
	 */
	unless (sfile = fopen(sccsXfile(sc, 'x'), "w")) {
		fprintf(stderr, "admin: can't create %s: ", sccsXfile(sc, 'x'));
		perror("");
		OUT;
	}
	old_enc = sc->encoding;
	sc->encoding = new_enc;
	if (delta_table(sc, sfile, 0, fixDate)) {
		sccs_unlock(sc, 'x');
		goto out;	/* we don't know why so let sccs_why do it */
	}
	assert(sc->state & S_SOPEN);
	seekto(sc, sc->data);
	debug((stderr, "seek to %d\n", sc->data));
	if (old_enc & E_GZIP) zgets_init(sc->where, sc->size - sc->data);
	if (new_enc & E_GZIP) zputs_init();
	if (new_enc != old_enc) {
		sc->encoding = old_enc;
		while (buf = nextdata(sc)) {
			sc->encoding = new_enc;
			fputdata(sc, buf, sfile);
			sc->encoding = old_enc;
		}
	} else {
		while (buf = nextdata(sc)) {
			fputdata(sc, buf, sfile);
		}
	}
	/* not really needed, we already wrote it */
	sc->encoding = new_enc;
	if (fflushdata(sc, sfile)) {
		sccs_close(sc), fclose(sfile), sfile = NULL;
		goto out;
	}
	fseek(sfile, 0L, SEEK_SET);
	fprintf(sfile, "\001h%05u\n", sc->cksum);
#ifdef	DEBUG
	badcksum(sc);
#endif
	sccs_close(sc), fclose(sfile), sfile = NULL;
	if (old_enc & E_GZIP) zgets_done();
	unlink(sc->sfile);		/* Careful. */
	t = sccsXfile(sc, 'x');
	if (rename(t, sc->sfile)) {
		fprintf(stderr,
		    "admin: can't rename(%s, %s) left in %s\n",
		    t, sc->sfile, t);
		OUT;
	}
	Chmod(sc->sfile, 0444);
	goto out;
#undef	OUT
}


private void
doctrl(sccs *s, char *pre, int val, FILE *out)
{
	char	tmp[10];

	sertoa(tmp, (unsigned short) val);
	fputdata(s, pre, out);
	fputdata(s, tmp, out);
	fputdata(s, "\n", out);
}

#define	nextline(inc)	nxtline(s, &inc, 0, &lines, &print, out, state, slist)
#define	beforeline(inc) nxtline(s, &inc, 1, &lines, &print, out, state, slist)

void
nxtline(sccs *s, int *ip, int before, int *lp, int *pp, FILE *out,
	register serlist *state, ser_t *slist)
{
	int	print = *pp, incr = *ip, lines = *lp;
	sum_t	sum;
	register char	*buf;

	debug((stderr, "nxtline(@%d, before=%d print=%d, sum=%d) ",
	    lines, before, print, s->dsum));
	while (!eof(s)) {
		if (before && print) { /* if move upto next printable line */
			unless (peekc(s) == '\001') break;
		}
		unless (buf = nextdata(s)) break;
		debug2((stderr, "[%d] ", lines));
		debug2((stderr, "G> %.*s", linelen(buf), buf));
		sum = fputdata(s, buf, out);
		if (isData(buf)) {
			if (print) {
				s->dsum += sum;
				incr++; lines++;
				break;
			}
			continue;
		}
		changestate(state, buf[1], atoi(&buf[3]));
		print = printstate((const serlist*)state, (const ser_t*)slist);
	}
	*ip = incr;
	*lp = lines;
	*pp = print;
	debug((stderr, "sum=%d\n", s->dsum));
}

/*
 * Get the hash checksum.
 * A side effect of getRegBody() is to set dsum.
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

	assert(sc->state & S_HASH);
	assert(diffs);
	/*
	 * If we have a hash already and it is a simple delta, then just
	 * use that.  Otherwise, regen from scratch.
	 */
	if (sc->mdbm
	    && !n->include && !n->exclude && (d = getCksumDelta(sc, n))) {
	    	sum = d->sum;
	} else {
		if (sc->mdbm) mdbm_close(sc->mdbm), sc->mdbm = 0;
		sccs_restart(sc);
		if (getRegBody(sc, 0, flags, n, &lines, 0, 0)) {
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
	unless (strneq(buf, "0a0\n", 4)) {
		fprintf(stderr, "Missing 0a0, ");
bad:		fprintf(stderr, "bad diffs: '%.*s'\n", linelen(buf), buf);
		return (-1);
	}
	while (buf = mnext(diffs)) {
		unless (buf[0] == '>') goto bad;
		for (t = key, v = &buf[2]; (v < diffs->end) && (*v != ' '); ) {
			*t++ = *v++;
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

int
delta_body(sccs *s, delta *n, MMAP *diffs, FILE *out, int *ap, int *dp, int *up)
{
	serlist *state = 0;
	ser_t	*slist = 0;
	int	print = 0;
	int	lines = 0;
	int	added = 0, deleted = 0, unchanged = 0;
	sum_t	sum;
	char	*b;

	assert((s->state & S_READ_ONLY) == 0);
	assert(s->state & S_ZFILE);
	*ap = *dp = *up = 0;
	/*
	 * Do the actual delta.
	 */
	seekto(s, s->data);
	if (s->encoding & E_GZIP) {
		zgets_init(s->where, s->size - s->data);
		zputs_init();
	}
	slist = serialmap(s, n, 0, 0, 0, 0);	/* XXX - -gLIST */
	s->dsum = 0;
	assert(s->state & S_SOPEN);
	state = allocstate(0, 0, s->nextserial);
	while (b = mnext(diffs)) {
		int	where;
		char	what;
		int	howmany;

newcmd:
		if (scandiff(b, &where, &what, &howmany) != 0) {
			fprintf(stderr,
			    "delta: can't figure out '%.*s'\n",
			    linelen(b), b);
			if (state) free(state);
			if (slist) free(slist);
			return (-1);
		}
		debug2((stderr, "where=%d what=%c\n", where, what));

#define	ctrl(pre, val)	doctrl(s, pre, val, out)

		if (what != 'a' && what != 'b' && what != 'I') where--;
		while (lines < where) {
			/*
			 * XXX - this loops when I don't use the fudge as part
			 * of the ID in make/takepatch of SCCSFILE.
			 */
			nextline(unchanged);
		}
		switch (what) {
		    case 'a':
			ctrl("\001I ", n->serial);
			while (b = mnext(diffs)) {
				if (isdigit(b[0])) {
					ctrl("\001E ", n->serial);
					goto newcmd;
				}
				s->dsum += fputdata(s, &b[2], out);
				debug2((stderr,
				    "INS %.*s", linelen(&b[2]), &b[2]));
				added++;
			}
			break;
		    case 'd':
			beforeline(unchanged);
			ctrl("\001D ", n->serial);
			sum = s->dsum;
			while (b = mnext(diffs)) {
				if (isdigit(b[0])) {
					ctrl("\001E ", n->serial);
					s->dsum = sum;
					goto newcmd;
				}
				nextline(deleted);
			}
			s->dsum = sum;
			break;
		    case 'c':
			beforeline(unchanged);
			ctrl("\001D ", n->serial);
			sum = s->dsum;
			/* Toss the old stuff */
			while (b = mnext(diffs)) {
				if (strneq(b, "---\n", 4)) break;
				nextline(deleted);
			}
			s->dsum = sum;
			ctrl("\001E ", n->serial);
			/* add the new stuff */
			ctrl("\001I ", n->serial);
			while (b = mnext(diffs)) {
				if (isdigit(b[0])) {
					ctrl("\001E ", n->serial);
					goto newcmd;
				}
				s->dsum += fputdata(s, &b[2], out);
				debug2((stderr,
				    "INS %.*s", linelen(&b[2]), &b[2]));
				added++;
			}
			break;
		    case 'I':
		    case 'i':
			ctrl("\001I ", n->serial);
			while (howmany--) {
				/* XXX: not break but error */
				unless (b = mnext(diffs)) break;
				if (what == 'I' && b[0] == '\\') {
					s->dsum += fputdata(s, &b[1], out);
				} else {
					s->dsum += fputdata(s, b, out);
				}
				debug2((stderr, "INS %.*s", linelen(b), b));
				added++;
			}
			break;
		    case 'D':
		    case 'x':
			beforeline(unchanged);
			ctrl("\001D ", n->serial);
			sum = s->dsum;
			while (howmany--) {
				if (isdigit(b[0])) {
					ctrl("\001E ", n->serial);
					goto newcmd;
				}
				nextline(deleted);
			}
			s->dsum = sum;
			break;
		}
		ctrl("\001E ", n->serial);
	}
	while (!eof(s)) {
		nextline(unchanged);
	}
	*ap = added;
	*dp = deleted;
	*up = unchanged;
	if (state) free(state);
	if (slist) free(slist);
	if (s->encoding & E_GZIP) zgets_done();
	if ((s->state & S_HASH) && (getHashSum(s, n, diffs) != 0)) {
		return (-1);
	}
	return (0);
}

/*
 * Initialize as much as possible from the file.
 * Don't override any information which is already set.
 * XXX - this needs to track sccs_prsdelta/do_patch closely.
 */
delta *
sccs_getInit(sccs *sc, delta *d, MMAP *f, int patch, int *errorp, int *linesp)
{
	char	*s, *t;
	char	*buf;
	int	nocomments = d && d->comments;
	int	error = 0;
	int	lines = 0;
	char	type = '?';

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
	if (!d || !d->rev) {
		s[-1] = 0;
		d = sccs_parseArg(d, 'R', &buf[2], 0);
	}
	assert(d);
	t = s;
	while (*s++ != ' ');	/* eat date */
	while (*s++ != ' ');	/* eat time */
	unless (d->sdate) {
		s[-1] = 0;
		d = sccs_parseArg(d, 'D', t, 0);
	}
	t = s;
	while (*s && (*s++ != ' '));	/* eat user */
	unless (d->user) {
		if (s[-1] == ' ') s[-1] = 0;
		d = sccs_parseArg(d, 'U', t, 0);
		if (!d->hostname) d->flags |= D_NOHOST;
	}
	if (patch) goto skip;	/* skip the rest of this line */
	t = s;
	while (*s && (*s++ != ' '));	/* serial */
	unless (d->serial) {
		if (s[-1] == ' ') s[-1] = 0;
		d->serial = atoi(t);
	}
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

	/* Cset file ID */
	if (WANT('B')) {
		unless (d->csetFile) d = csetFileArg(d, &buf[2]);
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* Cset marker */
	if ((buf[0] == 'C') && !buf[1]) {
		d->flags |= D_CSET;
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
		unless (nocomments) {
			d->comments = addLine(d->comments, strdup(p));
		}
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
		if (sc) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			unless (e) {
				fprintf(stderr, "Can't find inc %s in %s\n",
				    &buf[2], sc->sfile);
				sc->state |= S_WARNED;
				error++;
			} else {
				d->include = addSerial(d->include, e->serial);
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
		if (sc) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			unless (e) {
				fprintf(stderr, "Can't find merge %s in %s\n",
				    &buf[2], sc->sfile);
				sc->state |= S_WARNED;
				error++;
			} else {
				d->merge = e->serial;
			}
		}
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* modes are optional */
	if (WANT('O')) {
		unless (d->mode) d = modeArg(d, &buf[2]);
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* pathnames are optional */
	if (WANT('P')) {
		unless (d->pathname) d = pathArg(d, &buf[2]);
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* Random bits are used only for 1.0 deltas in conversion scripts */
	if (WANT('R')) {
		unless (streq("1.0", d->rev)) {
			fprintf(stderr, "sccs_getInit: ramdom only on 1.0\n");
		} else if (sc) {
			sc->random = strnonldup(&buf[2]);
		} else {
			fprintf(stderr, "sccs_getInit: ramdom needs sccs\n");
		}
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* symbols are optional */
	if (WANT('S')) {
		d->sym = strdup(&buf[2]);
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* text are optional */
	/* Can not be WANT('T'), buf[1] could be null */
	while (buf[0] == 'T') {
		if (buf[1] == ' ') {
			d->text = addLine(d->text, strdup(&buf[2]));
		}
		d->flags |= D_TEXT;
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* Excludes are optional and are specified as keys.
	 * If there is no sccs* ignore them.
	 */
	while (WANT('x')) {
		if (sc) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			unless (e) {
				fprintf(stderr, "Can't find ex %s in %s\n",
				    &buf[2], sc->sfile);
				sc->state |= S_WARNED;
				error++;
			} else {
				d->exclude = addSerial(d->exclude, e->serial);
			}
		}
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	if (WANT('X')) {
		d->xflags = atoi(&buf[2]);	
		d->flags |= D_XFLAGS;
		unless (buf = mkline(mnext(f))) goto out; lines++;
	}

	/* closing line is not optional. */
	if (strcmp("------------------------------------------------", buf)) {
		fprintf(stderr,
		    "Warning: Bad line in init file follows.\n'%s'\n", buf);
		error++;
	}

out:	if (d) {
		unless (hasComments(d)) d->flags |= D_NOCOMMENTS;
		unless (d->pathname) d->flags |= D_NOPATH;
		if (type == 'M') {
			d->flags |= D_META;
			type = 'R';
		}
		d->type = type;
	}
	*errorp = error;
	if (linesp) *linesp += lines;
	return (d);
}

/*
 * Read the p.file and extract and return the old/new/user/{inc, excl} lists.
 *
 * Returns 0 if OK, -1 on error.  Warns on all errors.
 */
private int
read_pfile(char *who, sccs *s, pfile *pf)
{
	int	fsize = size(s->pfile);
	char	*iLst, *xLst;
	char	*mRev = malloc(MAXREV+1);
	char	c1 = 0, c2 = 0, c3 = 0;
	int	e;
	FILE	*tmp;
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
	    pf->oldrev, pf->newrev, user, date, time, &c1, iLst, &c2, xLst,
	    &c3, mRev);
	pf->user = strdup(user);
	strcpy(pf->date, date);
	strcat(pf->date, " ");
	strcat(pf->date, time);
	fclose(tmp);
	pf->sccsrev[0] = 0;

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
	    case 7:		/* figure out if it was -i or -x */
		if (c1 == 'x') {
			free(xLst);
			xLst = iLst;
			iLst = 0;
		} else {
			assert(c1 == 'i');
			free(xLst);
			xLst = 0;
		}
		free(mRev);
		mRev = 0;
		break;
	    case 9:		/* Could be -i -x or -i -m but not -x -m */
		assert(c1 != 'x');
		assert(c1 == 'i');
		free(mRev);
		mRev = 0;
		if (c2 == 'm') {
			mRev = xLst;
			xLst = 0;
		} else {
			assert(c2 == 'x');
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
    	    pf->oldrev, pf->newrev, user, pf->date,
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
	if (streq(u, ROOT_USER) || streq(u, UNKNOWN_USER)) return 0;
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
int
sccs_meta(sccs *s, delta *parent, MMAP *iF)
{
	delta	*m;
	int	e = 0;
	FILE	*sfile = 0;
	char	*sccsXfile();
	char	*t;
	char	*buf;

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
	m = sccs_getInit(s, 0, iF, 1, &e, 0);
	mclose(iF);
	if (m->rev) free(m->rev);
	m->rev = strdup(parent->rev);
	bcopy(parent->r, m->r, sizeof(m->r));
	m->serial = s->nextserial++;
	m->pserial = parent->serial;
	m->next = s->table;
	s->table = m;
	s->numdeltas++;
	dinsert(s, 0, m);
	if (m->sym) {
		addsym(s, m, m, m->rev, m->sym);
		free(m->sym);
		m->sym = 0;
	}

	/*
	 * Do the delta table & misc.
	 */
	unless (sfile = fopen(sccsXfile(s, 'x'), "w")) {
		fprintf(stderr, "admin: can't create %s: ", sccsXfile(s, 'x'));
		perror("");
		sccs_unlock(s, 'z');
		exit(1);
	}
	if (delta_table(s, sfile, 0, 0)) {
abort:		fclose(sfile);
		sccs_unlock(s, 'x');
		return (-1);
	}
	seekto(s, s->data);
	if (s->encoding & E_GZIP) {
		zgets_init(s->where, s->size - s->data);
		zputs_init();
	}
	assert(s->state & S_SOPEN);
	while (buf = nextdata(s)) {
		fputdata(s, buf, sfile);
	}
	if (fflushdata(s, sfile)) goto abort;
	if (s->encoding & E_GZIP) zgets_done();
	fseek(sfile, 0L, SEEK_SET);
	fprintf(sfile, "\001h%05u\n", s->cksum);
	sccs_close(s); fclose(sfile); sfile = NULL;
	unlink(s->sfile);		/* Careful. */
	t = sccsXfile(s, 'x');
	if (rename(t, s->sfile)) {
		fprintf(stderr,
		    "takepatch: can't rename(%s, %s) left in %s\n",
		    t, s->sfile, t);
		sccs_unlock(s, 'z');
		exit(1);
	}
	Chmod(s->sfile, 0444);
	sccs_unlock(s, 'z');
	return (0);
}

/* return the next available release */

u16
sccs_nextlod(sccs *s)
{
	u16	lod = 0;
	delta	*d;

	for (d = s->table; d; d = d->next) {
		if (d->type == 'D' && d->r[0] > lod) lod = d->r[0];
	}
	if (lod) lod++; /* leave lod 0 == 0 meaning error */
	return (lod);
}

/*
 * see if the delta->rev is x.1 node.  If yes, then figure out number
 * to check in as new LOD
 */
private int
chknewlod(sccs *s, delta *d)
{
	u16	lod;
	char	buf[MAXPATH];

	unless (d->rev) return (0);

	unless (d->r[0] != 1 && d->r[1] == 1 && d->r[2] == 0) {
		free(d->rev);
		d->rev = 0;
		return (0);
	}
	lod = sccs_nextlod(s);
	sprintf(buf, "%d.1", lod);
	free(d->rev);
	d->rev = strdup(buf);
	explode_rev(d);
	return (0);
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
sccs_delta(sccs *s, u32 flags, delta *prefilled, MMAP *init, MMAP *diffs)
{
	FILE	*sfile = 0;	/* the new s.file */
	int	i, error = 0, fixDate = 1;
	char	*t;
	delta	*d = 0, *n = 0;
	char	*tmpfile = 0;
	int	added, deleted, unchanged;
	int	locked;
	pfile	pf;

	assert(s);
	debug((stderr, "delta %s %x\n", s->gfile, flags));
	if (flags & NEWFILE) mksccsdir(s->sfile);
	bzero(&pf, sizeof(pf));
	unless(locked = sccs_lock(s, 'z')) {
		fprintf(stderr, "delta: can't get lock on %s\n", s->sfile);
		error = -1; s->state |= S_WARNED;
out:
		if (prefilled) sccs_freetree(prefilled);
		if (sfile) fclose(sfile);
		if (diffs) mclose(diffs);
		free_pfile(&pf);
		if (tmpfile  && !streq(tmpfile, DEV_NULL)) unlink(tmpfile);
		if (locked) sccs_unlock(s, 'z');
		debug((stderr, "delta returns %d\n", error));
		return (error);
	}
#define	OUT	{ error = -1; s->state |= S_WARNED; goto out; }

	if (init) {
		int	e;

		prefilled =
		    sccs_getInit(s, prefilled, init, flags&DELTA_PATCH, &e, 0);
		unless (prefilled && !e) {
			fprintf(stderr, "delta: bad init file\n");
			goto out;
		}
		debug((stderr, "delta got prefilled %s\n", prefilled->rev));
		if (flags & DELTA_PATCH) {
			fixDate = 0;
			if (prefilled->pathname &&
			    streq(prefilled->pathname, "ChangeSet")) {
				s->state |= S_CSET;
		    	}
			if (prefilled->flags & D_XFLAGS) {
				/*  XXX this code will be affected by LOD */
				u32 bits = prefilled->xflags;
				if (bits & X_BITKEEPER) {
					s->state |= S_BITKEEPER;
				} else {
					s->state &= ~S_BITKEEPER;
				}
				if (bits & X_YEAR4) {
					s->state |= S_YEAR4;
				} else {
					s->state &= ~S_YEAR4;
				}
				if (bits & X_RCSEXPAND) {
					s->state |= S_RCS;
				} else {
					s->state &= ~S_RCS;
				}
				if (bits & X_EXPAND1) {
					s->state |= S_EXPAND1;
				} else {
					s->state &= ~S_EXPAND1;
				}
			}
			if (prefilled->flags & D_TEXT) {
				if (s->text) {
					freeLines(s->text);
					s->text = 0;
				}
				EACH(prefilled->text) {
					s->text = addLine(s->text,
						strnonldup(prefilled->text[i]));
				}
			}
			unless (flags & NEWFILE) {
				/* except the very first delta   */
				/* all rev are subject to rename */
				/* if x.1 then ensure new LOD */
				chknewlod(s, prefilled);
				/* free(prefilled->rev);
				 * prefilled->rev = 0;
				 */
			}
		}
	}

	if ((flags & NEWFILE) || (!HAS_SFILE(s) && HAS_GFILE(s))) {
		return (checkin(s, flags, prefilled, init != 0, diffs));
	}

	unless (HAS_SFILE(s) && s->tree) {
		fprintf(stderr, "delta: %s is not an SCCS file\n", s->sfile);
		s->state |= S_WARNED;
		OUT;
	}

	unless (HAS_PFILE(s)) {
		if (IS_WRITABLE(s)) {
			fprintf(stderr,
			    "delta: %s writable but not checked out?\n",
			    s->gfile);
			s->state |= S_WARNED;
			OUT;
		} else {
			verbose((stderr,
			    "delta: %s is not locked.\n", s->sfile);
			goto out;
		}
	}

	unless (IS_WRITABLE(s) || diffs) {
		fprintf(stderr,
		    "delta: %s is locked but not writable.\n", s->gfile));
		s->state |= S_WARNED;
		OUT;
	}

	if (HAS_GFILE(s) && diffs) {
		fprintf(stderr,
		    "delta: diffs or gfile for %s, but not both.\n",
		    s->gfile);
		s->state |= S_WARNED;
		OUT;
	}

#ifdef WIN32
	/*
	 * Win32 note: If gfile is in use, we can not delete
	 * it when we are done.It is better to bail now
	 */
	if (HAS_GFILE(s) &&
	    !(flags & DELTA_SAVEGFILE) && fileBusy(s->gfile)) {
		verbose((stderr, "delta: %s is busy\n", s->gfile));
		OUT;
	}
#endif

	/*
	 * OK, checking done, start the delta.
	 */
	if (read_pfile("delta", s, &pf)) OUT;
	unless (d = findrev(s, pf.oldrev)) {
		fprintf(stderr,
		    "delta: can't find %s in %s\n", pf.oldrev, s->gfile);
		OUT;
	}
	if (pf.mRev) flags |= DELTA_FORCE;
	debug((stderr, "delta found rev\n"));
	if (diffs) {
		debug((stderr, "delta using diffs passed in\n"));
	} else {
		switch (diff_g(s, &pf, &tmpfile)) {
		    case 1:		/* no diffs */
			if (flags & DELTA_FORCE) {
				break;     /* forced 0 sized delta */
			}
			if (!(flags & SILENT))
				fprintf(stderr,
				    "Clean %s (no diffs)\n", s->gfile);
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
		unless (diffs = mopen(tmpfile, "t")) {
			fprintf(stderr,
			    "delta: can't open diff file %s\n", tmpfile);
			OUT;
		}
	}
	if (flags & PRINT) {
		fprintf(stdout, "==== Changes to %s ====\n", s->gfile);
		fwrite(diffs->mmap, diffs->size, 1, stdout);
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
		n = calloc(sizeof(*n), 1);
		assert(n);
	}
	if (pf.iLst) {
		assert(!n->include);
		n->include = getserlist(s, 0, pf.iLst, &error);
	}
	if (pf.xLst) {
		assert(!n->exclude);
		n->exclude = getserlist(s, 0, pf.xLst, &error);
	}
	if (pf.mRev) {
		delta	*e = findrev(s, pf.mRev);

		unless (e) {
			fprintf(stderr,
			    "delta: no such rev %s in %s\n", pf.mRev, s->sfile);
		    	OUT;
		}
		if (n->merge && (e->serial != n->merge)) {
			fprintf(stderr,
			    "delta: conflicting merge revs: %s %s\n",
			    n->rev, e->rev);
			OUT;
		}
		n->merge = e->serial;
	}
	if (error) OUT;
	n = sccs_dInit(n, 'D', s, init != 0);
	updMode(s, n, d);

	if (!n->rev) {
		if (pf.sccsrev[0]) {
			n->rev = pf.sccsrev;
			explode_rev(n);
			n->rev = strdup(pf.newrev);
		} else {
			n->rev = strdup(pf.newrev);
			explode_rev(n);
		}
	} else {
		explode_rev(n);
	}
	n->serial = s->nextserial++;
	n->next = s->table;
	s->table = n;
	assert(d);
	n->pserial = d->serial;
	if (!hasComments(n) && !init &&
	    !(flags & DELTA_DONTASK) && !(n->flags & D_NOCOMMENTS)) {
		/*
		 * XXX - andrew make sure host/user is correct right here.
		 */
		if (sccs_getComments(s->gfile, pf.newrev, n)) OUT;
	}
	dinsert(s, flags, n);
	s->numdeltas++;

	if (n->sym) {
		if (dupSym(s->symbols, n->sym, 0)) {
			fprintf(stderr,
			    "delta: symbol %s exists in %s\n",
			    n->sym, s->sfile);
			fprintf(stderr, "use admin to override old value\n");
		} else {
			addsym(s, n, n, n->rev, n->sym);
			free(n->sym);
			n->sym = 0;
		}
	}

	/*
	 * Start new lod if X.1
	 * XXX: Could require the old defbranch to match exactly the
	 * initial version in the pfile to make conditions tighter.
	 */
	if ((s->state & S_BITKEEPER) && n->r[1] == 1 && n->r[2] == 0) {
		if (s->defbranch) {
			free(s->defbranch);
			s->defbranch = 0;
		}
	}

	/*
	 * Do the delta table & misc.
	 */
	unless (sfile = fopen(sccsXfile(s, 'x'), "w")) {
		fprintf(stderr, "delta: can't create %s: ", sccsXfile(s, 'x'));
		perror("");
		OUT;
	}

	if (delta_table(s, sfile, 1, fixDate)) {
		fclose(sfile); sfile = NULL;
		sccs_unlock(s, 'x');
		goto out;	/* not OUT - we want the warning */
	}

	assert(d);
	if (delta_body(s, n, diffs, sfile, &added, &deleted, &unchanged)) {
		OUT;
	}
	if (end(s, n, sfile, flags, added, deleted, unchanged)) {
		fclose(sfile); sfile = NULL;
		sccs_unlock(s, 'x');
		goto out;	/* not OUT - we want the warning */
	}

	sccs_close(s), fclose(sfile), sfile = NULL;
	unlink(s->sfile);					/* Careful. */
	unless (flags & DELTA_SAVEGFILE)  unlinkGfile(s);	/* Careful */
	t = sccsXfile(s, 'x');
	if (rename(t, s->sfile)) {
		fprintf(stderr,
		    "delta: can't rename(%s, %s) left in %s\n",
		    t, s->sfile, t);
		OUT;
	}
	Chmod(s->sfile, 0444);
	unlink(s->pfile);
	if (s->state & S_BITKEEPER) updatePending(s, n);
	goto out;
#undef	OUT
}

/*
 * works for 32 bit unsigned only
 */
void
fit(char *buf, unsigned int i)
{
	int	j;
	float	f;
	static	char *s[] = { "K", "M", "G", 0 };
	if (i < 100000) {
		sprintf(buf, "%05d\n", i);
		return;
	}
	for (j = 0, f = 1000.; s[j]; j++, f *= 1000.) {
		sprintf(buf, "%04.3g%s", i/f, s[j]);
		if (strlen(buf) == 5) return;
	}
	sprintf(buf, "E2BIG");
	return;
}

private void
fitCounters(char *buf, int a, int d, int s)
{
	/* ^As 12345/12345/12345\n
	 *  012345678901234567890
	 */
	strcpy(buf, "\001s ");
	fit(&buf[3], a);
	buf[8] = '/';
	fit(&buf[9], d);
	buf[14] = '/';
	fit(&buf[15], s);
	buf[20] = '\n';
	buf[21] = 0;
}

/*
 * Print the summary and go and fix up the top.
 */
private int
end(sccs *s, delta *n, FILE *out, int flags, int add, int del, int same)
{
	char	buf[100];

	/*
	 * Flush and make sure we have disk space.
	 */
	if (fflushdata(s, out)) return (-1);
	unless (flags & SILENT) {
		int	lines = count_lines(n->parent) - del + add;

		fprintf(stderr, "%s revision %s: ", s->gfile, n->rev);
		fprintf(stderr, "+%d -%d = %d\n", add, del, lines);
	}
	n->added = add;
	n->deleted = del;
	n->same = same;

	/*
	 * Now fix up the checksum and summary.
	 */
	fseek(out, 8L, SEEK_SET);
	sprintf(buf, "\001s %05d/%05d/%05d\n", add, del, same);
	if (strlen(buf) > 21) {
		unless (s->state & S_BITKEEPER) {
			fprintf(stderr, "%s: file too large\n", s->gfile);
			exit(1);
		}
		fitCounters(buf, add, del, same);
	}
	fputmeta(s, buf, out);
	if (s->state & S_BITKEEPER) {
		if ((add || del || same) && (n->flags & D_ICKSUM)) {
			delta	*z = getCksumDelta(s, n);

			if (!z || (s->dsum != z->sum)) {
				fprintf(stderr,
				    "%s: bad delta checksum: %u:%d for %s\n",
				    s->sfile, s->dsum,
				    z ? z->sum : -1, n->rev);
				s->state |= S_BAD_DSUM;
			}
		}
		unless (n->flags & D_ICKSUM) {
			/*
			 * XXX: would like "if cksum is same as parent"
			 * but we can't do that because we use the inc/ex
			 * in getCksumDelta().
			 */
			if (add || del || n->include || n->exclude) {
				n->sum = s->dsum;
			} else {
				n->sum = (unsigned short) almostUnique(0);
			}
#if 0

Fucks up citool

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
		fseek(out, s->sumOff, SEEK_SET);
		sprintf(buf, "%05u", n->sum);
		fputmeta(s, buf, out);
	}
	fseek(out, 0L, SEEK_SET);
	fprintf(out, "\001h%05u\n", s->cksum);
	if (fflush(out) || ferror(out)) return (-1);
	return (0);
}

/*
 * diffs - diff the gfile or the specified (or implied) rev
 */
int
sccs_diffs(sccs *s, char *r1, char *r2, u32 flags, char kind, FILE *out)
{
	FILE	*diffs = 0;
	char	*left, *right;
	char	*leftf, *rightf;
	char	tmpfile[MAXPATH];
	char	diffFile[MAXPATH];
	char	*columns = 0;
	char	tmp2[MAXPATH];
	char	buf[MAXLINE];
	pfile	pf;
	int	first = 1;
	int	here;
	char	spaces[80];

	bzero(&pf, sizeof(pf));
	GOODSCCS(s);
	if (kind == DF_SDIFF) {
		unless (columns = getenv("COLUMNS")) columns = "80";
	}
	if (r1 && r2) {
		left = r1;
		right = r2;
	} else if (r1) {
		left = r1;
		right = HAS_PFILE(s) ? "edited" : 0;
	} else if (HAS_PFILE(s)) {
		if (read_pfile("diffs", s, &pf)) return (-1);
		left = pf.oldrev;
		right = "edited";
	} else {
		unless (HAS_GFILE(s)) {
			verbose((stderr,
			    "diffs: %s not checked out.\n", s->gfile));
			s->state |= S_WARNED;
			return (-1);
		}
		left = 0;
		right = "?";
	}
	unless (findrev(s, left)) {
		free_pfile(&pf);
		return (-2);
	}
	if (r2 && !findrev(s, r2)) {
		free_pfile(&pf);
		return (-3);
	}
	if (!right) right = findrev(s, 0)->rev;
	if (!left) left = findrev(s, 0)->rev;
	strcpy(tmp2, s->gfile);		/* because dirname stomps */
	sprintf(tmpfile, "%s", dirname(tmp2));
	here = writable(tmpfile);
	if (here) {
		sprintf(tmpfile, "%s-%s", s->gfile, left);
	} else {
		sprintf(tmpfile,
		    "%s/%s-%s-%d", TMP_PATH, basenm(s->gfile), left, getpid());
	}
	if (exists(tmpfile) || sccs_get(s, left,
	    pf.mRev, pf.iLst, pf.xLst, flags|SILENT|PRINT, tmpfile)) {
			unlink(tmpfile);
			free_pfile(&pf);
			return (-1);
	}
	if (here) {
		sprintf(tmp2, "%s-%s", s->gfile, r2 ? r2 : "-2");
	} else {
		sprintf(tmp2, "%s/%s-%s-%d", TMP_PATH, basenm(s->gfile),
		    r2 ? r2 : "-2", getpid());
	}
	if (r2 || !HAS_GFILE(s)) {
		if (sccs_get(s, right, 0, 0, 0, flags|SILENT|PRINT, tmp2)) {
			unlink(tmpfile);
			unlink(tmp2);
			free_pfile(&pf);
			return (-1);
		}
		leftf = tmpfile;
		rightf = tmp2;
	} else if (s->symlink) {
		if (diffs = fopen(tmp2, "w")) {
			fprintf(diffs, "SYMLINK -> %s\n", s->symlink);
			fclose(diffs);
			leftf = tmpfile;
			rightf = tmp2;
		} else {
			perror(tmp2);
			unlink(tmpfile);
			unlink(tmp2);
			free_pfile(&pf);
			return (-1);
		}
	} else {
		tmp2[0] = 0;
		leftf = tmpfile;
		rightf = s->gfile;
	}
	if (kind == DF_SDIFF) {
		int	i, c = atoi(columns);

		for (i = 0; i < c/2 - 18; ) spaces[i++] = '=';
		spaces[i] = 0;
		sprintf(buf, "sdiff -w%s %s %s", columns, leftf, rightf);
		diffs = popen(buf, "rt");
		if (!diffs) {
			unlink(tmpfile);
			if (tmp2[0]) unlink(tmp2);
			free_pfile(&pf);
			return (-1);
		}
		diffFile[0] = 0;
	} else {
		strcpy(spaces, "=====");
		if (gettemp(diffFile, "diffs")) return (-1);
		diff(leftf, rightf, kind, diffFile);
		diffs = fopen(diffFile, "rt");
	}
	while (fnext(buf, diffs)) {
		if (first) {
			if (flags & DIFF_HEADER) {
				fprintf(out, "%s %s %s vs %s %s\n",
				    spaces, s->gfile, left, right, spaces);
			} else {
				fprintf(out, "\n");
			}
			first = 0;
			/*
			 * Make the file names be the same, so change
			 * +++ bk.sh-1.34  Thu Jun 10 21:22:08 1999
			 * to
			 * +++ bk.sh 1.34  Thu Jun 10 21:22:08 1999
			 * XXX /tmp case
			 */
			if ((kind == DF_UNIFIED) || (kind == DF_CONTEXT)) {
				int	len = strlen(s->gfile);

				if (strneq(s->gfile, &buf[4], len)) {
					buf[4+len] = ' ';
				}
				fputs(buf, out);
				unless (fnext(buf, diffs)) break;
				if (strneq(s->gfile, &buf[4], len)) {
					buf[4+len] = ' ';
				}
			}
		}
		fputs(buf, out);
	}
	if (kind == DF_SDIFF) {
		pclose(diffs);
	} else {
		fclose(diffs);
	}
	unlink(tmpfile);
	if (tmp2[0]) unlink(tmp2);
	if (diffFile[0]) unlink(diffFile);
	free_pfile(&pf);
	return (0);
}

private void
show_d(FILE *out, char *vbuf, char *format, int d)
{
	if (out) fprintf(out, format, d);
	if (vbuf) {
		char	dbuf[512];

		sprintf(dbuf, format, d);
		assert(strlen(dbuf) < 512);
		strcat(vbuf, dbuf);
		assert(strlen(vbuf) < 1024);
	}
}

private void
show_s(FILE *out, char *vbuf, char *s) {

	if (out) fputs(s, out);
	if (vbuf) {
		strcat(vbuf, s);
		assert(strlen(vbuf) < 1024);
	}
}

#define	notKeyword -1
#define	nullVal    0
#define	strVal	   1
/*
 * Given a PRS DSPEC keyword, get the associated string value
 * If out is non-null print to out
 * If vbuf is non-null, append the value to vbuf (vbuf size must be >= 1K)
 * If kw is not a keyword, return notKeyword
 * If kw has null value, return nullVal
 * Otherwise return strVal
 * Keyword definition is compatible with open group SCCS
 * This function may call itself recursively
 * kw2val() and fprintDelta() are mutually recursive
 */
private int
kw2val(FILE *out, char *vbuf, const char *prefix, int plen, const char *kw,
	const char *suffix, int slen, sccs *s, delta *d)
{
	char	*p, *q;
#define	KW(x)	kw2val(out, vbuf, "", 0, x, "", 0, s, d)
#define	fc(c)	show_d(out, vbuf, "%c", c)
#define	fd(d)	show_d(out, vbuf, "%d", d)
#define	f5d(d)	show_d(out, vbuf, "%05d", d)
#define	fs(s)	show_s(out, vbuf, s)

	if (streq(kw, "Dt")) {
		/* :Dt: = :DT::I::D::T::P::DS::DP: */
		KW("DT"); fc(' '); KW("I"); fc(' ');
		KW("D"); fc(' '); KW("T"); fc(' ');
		KW("P"); fc(' '); KW("DS"); fc(' ');
		KW("DP");
		return (strVal);
	}
	if (streq(kw, "DL")) {
		/* :DL: = :Li:/:Ld:/:Lu: */
		KW("Li"); fc('/'); KW("Ld"); fc('/'); KW("Lu");
		return (strVal);
	}

	if (streq(kw, "I")) {
		fs(d->rev);
		return (strVal);
	}

	if (streq(kw, "D")) {
		/* date */
		KW("Dy"); fc('/'); KW("Dm"); fc('/'); KW("Dd");
		return (strVal);
	}


	if (streq(kw, "T")) {
		/* Time */
		/* XXX TODO: need to figure out when to print time zone info */
		KW("Th"); fc(':'); KW("Tm"); fc(':'); KW("Ts");
		return (strVal);
	}

	if (streq(kw, "DI")) {
		/* serial number of included and excluded deltas.
		 * :DI: = :Dn:/:Dx:/:Dg: in ATT, we do :Dn:/:Dx:
		 */
		unless (d->include || d->exclude) return (nullVal);
		KW("Dn");
		if (d->exclude) {
			if (d->include) fc('/');
			KW("Dx");
		}
		return (strVal);
	}

	if (streq(kw, "RI")) {
		/* rev number of included and excluded deltas.
		 * :DR: = :Rn:/:Rx:
		 */
		unless (d->include || d->exclude) return (nullVal);
		KW("Rn");
		if (d->exclude) {
			if (d->include) fc('/');
			KW("Rx");
		}
		return (strVal);
	}


	if (streq(kw, "Dn")) {
		/* serial number of included deltas */
		int i;

		unless (d->include) return (nullVal);
		fc('+');
		EACH(d->include) {
			if (i > 1) fc(',');
			fd(d->include[i]);
		}
		return (strVal);
	}

	if (streq(kw, "Dx")) {
		/* serial number of excluded deltas */
		int i;

		unless (d->exclude) return (nullVal);
		fc('-');
		EACH(d->exclude) {
			if (i > 1) fc(',');
			fd(d->exclude[i]);
		}
		return (strVal);
	}

	if (streq(kw, "Dg")) {
		/* ignored delta - definition unknow, not implemented	*/
		/* always return null					*/
		return (nullVal);
	}

	/* rev number of included deltas */
	if (streq(kw, "Rn")) {
		int	i;
		delta	*r;

		unless (d->include) return (nullVal);
		fc('+');
		EACH(d->include) {
			if (i > 1) fc(',');
			r = sfind(s, d->include[i]);
			fs(r->rev);
		}
		return (strVal);
	}

	/* rev number of excluded deltas */
	if (streq(kw, "Rx")) {
		int	i;
		delta	*r;

		unless (d->exclude) return (nullVal);
		fc('-');
		EACH(d->exclude) {
			if (i > 1) fc(',');
			r = sfind(s, d->exclude[i]);
			fs(r->rev);
		}
		return (strVal);
	}

	if (streq(kw, "W")) {
		/* a form of "what" string */
		/* :W: = :Z::M:\t:I: */
		KW("Z"); KW("M"); fc('\t'); KW("I");
		return (strVal);
	}

	if (streq(kw, "A")) {
		/* a form of "what" string */
		/* :A: = :Z::Y: :M:I:Z: */
		KW("Z"); KW("Y"); fc(' ');
		KW("M"); KW("I"); KW("Z");
		return (strVal);
	}

	if (streq(kw, "Li")) {
		/* lines inserted */
		f5d(d->added);
		return (strVal);
	}

	if (streq(kw, "Ld")) {
		/* lines deleted */
		f5d(d->deleted);
		return (strVal);
	}

	if (streq(kw, "Lu")) {
		/* lines unchanged */
		f5d(d->same);
		return (strVal);
	}

	if (streq(kw, "DT")) {
		/* delta type */
		fc(d->type);
		return (strVal);
	}

	if (streq(kw, "R")) {
		/* release */
		for (p = d->rev; *p && *p != '.'; )
			fc(*p++);
		return (strVal);
	}

	if (streq(kw, "L")) {
		/* level */
		for (p = d->rev; *p && *p != '.'; p++); /* skip release field */
		for (p++; *p && *p != '.'; )
			fc(*p++);
		return (strVal);
	}

	if (streq(kw, "B")) {
		/* branch */
		for (p = d->rev; *p && *p != '.'; p++); /* skip release field */
		for (p++; *p && *p != '.'; p++);	/* skip branch field */
		for (p++; *p && *p != '.'; )
			fc(*p++);
		return (strVal);
	}

	if (streq(kw, "S")) {
		/* sequence */
		for (p = d->rev; *p && *p != '.'; p++); /* skip release field */
		for (p++; *p && *p != '.'; p++);	/* skip branch field */
		for (p++; *p && *p != '.'; p++);	/* skip level field */
		for (p++; *p; )
			fc(*p++);
		return (strVal);
	}

	if (streq(kw, "Dy")) {
		/* year */
		if (d->sdate) {
			char	val[512];

			if (s->state & S_YEAR4) {
				q = &val[2];
			} else {
				q = val;
			}
			for (p = d->sdate; *p && *p != '/'; )
				*q++ = *p++;
			*q = '\0';
			if (s->state & S_YEAR4) {
				if (atoi(&val[2]) <= 68) {
					val[0] = '2';
					val[1] = '0';
				} else {
					val[0] = '1';
					val[1] = '9';
				}
			}
			fs(val);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "Dm")) {
		/* month */
		if (d->sdate) {
			for (p = d->sdate; *p && *p != '/'; p++);
			for (p++; *p && *p != '/'; )
				fc(*p++);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "Dd")) {
		/* day */
		if (d->sdate) {
			for (p = d->sdate; *p && *p != '/'; p++);
			for (p++; *p && *p != '/'; p++);
			for (p++; *p && *p != ' '; )
				fc(*p++);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "Th")) {
		/* hour */
		if (d->sdate)
		{
			for (p = d->sdate; *p && *p != ' '; p++);
			for (p++; *p && *p != ':'; )
				fc(*p++);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "Tm")) {
		/* minute */
		if (d->sdate) {
			for (p = d->sdate; *p && *p != ' '; p++);
			for (p++; *p && *p != ':'; p++);
			for (p++; *p && *p != ':'; )
				fc(*p++);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "Ts")) {
		/* second */
		if (d->sdate) {
			for (p = d->sdate; *p && *p != ' '; p++);
			for (p++; *p && *p != ':'; p++);
			for (p++; *p && *p != ':'; p++);
			for (p++; *p; )
				fc(*p++);
			return (strVal);
		}
		return (nullVal);
	}


	if (streq(kw, "P")) {
		/* programmer */
		if (d->user) {
			fs(d->user);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "DS")) {
		/* serial number */
		fd(d->serial);
		return (strVal);
	}

	if (streq(kw, "DP")) {
		/* parent serial number */
		fd(d->pserial);
		return (strVal);
	}


	if (streq(kw, "MR")) {
		/* MR numbers for delta				*/
		/* not implemeted yet, return NULL for now	*/
		return (strVal);
	}

	if (streq(kw, "C")) {
		int i, j = 0;
		/* comments */
		/* XXX TODO: we may need to the walk the comment graph	*/
		/* to get the latest comment				*/
		EACH(d->comments) {
			if (d->comments[i][0] == '\001') continue;
			if (j++ > 0) fc('\n');
			fprintDelta(out, vbuf, prefix, &prefix[plen -1], s, d);
			fs(d->comments[i]);
			fprintDelta(out, vbuf, suffix, &suffix[slen -1], s, d);
		}
		if (j) return (strVal);
		return (nullVal);
	}

	if (streq(kw, "UN")) {
		/* users name(s) */
		/* XXX this is a multi-line text field, definition unknown */
		fs("??");
		return (strVal);
	}

	if (streq(kw, "FL")) {
		/* flag list */
		/* XX TODO: ouput flags in symbolic names ? */
		if (out) fprintf(out, "0x%x", d->flags);
		return (strVal);
	}

	if (streq(kw, "Y")) {
		/* moudle type, not implemented */
		fs("??");
		return (strVal);
	}

	if (streq(kw, "MF")) {
		/* MR validation flag, not implemented	*/
		fs("??");
		return (strVal);
	}

	if (streq(kw, "MP")) {
		/* MR validation pgm name, not implemented */
		fs("??");
		return (strVal);
	}

	if (streq(kw, "KF")) {
		/* keyword error warining flag	*/
		/* not implemented		*/
		fs("no");
		return (strVal);
	}

	if (streq(kw, "KV")) {
		/* keyword validation string	*/
		/* not impleemnted		*/
		return (nullVal);
	}

	if (streq(kw, "BF")) {
		/* branch flag */
		/* Bitkeeper does not have a branch flag */
		/* but we can derive the value		 */
		// re-check this when LOD is done
		if (d->rev) {
			int i;
			/* count the number of dot */
			for (i = 0, p = d->rev; *p && i <= 2; p++) {
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

	if (streq(kw, "J")) {
		/* Join edit flag  */
		/* not implemented */
		fs("no");
		return (strVal);
	}

	if (streq(kw, "LK")) {
		/* locked releases */
		/* not implemented */
		return (nullVal);
	}

	if (streq(kw, "Q")) {
		/* User defined keyword */
		/* not implemented	*/
		return (nullVal);
	}

	if (streq(kw, "M")) {
		/* XXX TODO: get the value from the	*/
		/* 'm' flag if/when implemented		*/
		fs(basenm(s->gfile));
		return (strVal);
	}

	if (streq(kw, "FB")) {
		/* floor boundary */
		/* not implemented */
		return (nullVal);
	}

	if (streq(kw, "CB")) {
		/* ceiling boundary */
		return (nullVal);
	}

	if (streq(kw, "Ds")) {
		return (KW("I"));
	}

	if (streq(kw, "ND")) {
		/* Null delta flag */
		/* not implemented */
		fs("no");
		return (strVal);
	}

	if (streq(kw, "FD")) {
		/* file description text */
		int i = 0, j = 0;
		EACH(s->text) {
			if (s->text[i][0] == '\001') continue;
			if (j++ > 0) fc('\n');
			fprintDelta(out, vbuf, prefix, &prefix[plen -1], s, d);
			fs(s->text[i]);
			fprintDelta(out, vbuf, suffix, &suffix[slen -1], s, d);
		}
		if (j) return (strVal);
		return (nullVal);
	}

	if (streq(kw, "BD")) {
		/* Body text */
		/* XX TODO: figure out where to extract this info */
		fs("??");
		return (strVal);
	}

	if (streq(kw, "GB")) {
		/* Gotten body */
		sccs_restart(s);
		sccs_get(s, d->rev, 0, 0, 0, GET_EXPAND|SILENT|PRINT, "-");
		return (strVal);
	}

	if (streq(kw, "Z")) {
		fs("@(#)");
		return (strVal);
	}

	if (streq(kw, "F")) {
		/* s file name */
		if (s->sfile) {
			/* scan backward for '/' */
			for (p = s->sfile, q = &p[strlen(p) -1];
				(q > p) && (*q != '/'); q--);
			if (*q == '/') q++;
			fs(q);
		}
		return (strVal);
	}

	if (streq(kw, "PN")) {
		/* s file path */
		if (s->sfile) {
			fs(s->sfile);
			return (strVal);
		}
		return nullVal;
	}

	/* ======== BITKEEPER SPECIFIC KEYWORDS ========== */
	if (streq(kw, "REV")) {
		fs(d->rev);
		return (strVal);
	}

	/* print the first rev at/below this which is in a cset */
	if (streq(kw, "CSETREV")) {
		while (d && !(d->flags & D_CSET)) d = d->kid;
		unless (d) return (nullVal);
		fs(d->rev);
		return (strVal);
	}

	if (streq(kw, "CSETKEY")) {
		unless (d->flags & D_CSET) return (nullVal);
		sccs_pdelta(s, d, out);
		return (strVal);
	}

	if (streq(kw, "KEY")) {
		if (out) sccs_pdelta(s, d, out);
		return (strVal);
	}

	if (streq(kw, "ROOTKEY")) {
		if (out) sccs_pdelta(s, sccs_ino(s), out);
		return (strVal);
	}

	if (streq(kw, "SHORTKEY")) {
		char	buf[MAXPATH+200];
		char	*t;

		sccs_sdelta(s, d, buf);
		if (t = sccs_iskeylong(buf)) *t = 0;
		fs(buf);
		return (strVal);
	}

	/*
	 * Print out more information than normal, including the
	 * checksum and the path to the root of the project ChangeSet
	 * if this is the ChangeSet file.
	 */
	if (streq(kw, "LONGKEY")) {
		if (out && d) {

			sccs_pdelta(s, d, out);
			if (d->flags & D_CKSUM) {
				fprintf(out, "-%05d", (int)d->sum);
			}
			if ((s->state & S_CSET) && (d == sccs_ino(s)) &&
			    d->comments && d->comments[1] &&
			    (d->comments[1][0] == '/')) {
				fprintf(out, "-%s", d->comments[1]);
			}
		}
		return (strVal);
	}

	if (streq(kw, "SYMBOL")) {
		symbol	*sym;
		int	j = 0;

		unless (d && (d->flags & D_SYMBOLS)) return (nullVal);
		for (sym = s->symbols; sym; sym = sym->next) {
			unless (sym->d == d) continue;
			j++;
			fprintDelta(out, vbuf, prefix, &prefix[plen -1], s, d);
			fs(sym->name);
			fprintDelta(out, vbuf, suffix, &suffix[slen -1], s, d);
		}
		if (j) return (strVal);
		return (nullVal);
	}

	if (streq(kw, "GFILE")) {
		if (s->gfile) {
			fs(s->gfile);
		}
		return (strVal);
	}

	if (streq(kw, "HT") || streq(kw, "HOST")) {
		/* host */
		if (d->hostname) {
			fs(d->hostname);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "DOMAIN")) {
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
		char *p, *q;
		int i;
		
		if (!d->hostname) return (nullVal);
		q = d->hostname;
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

	if (streq(kw, "TZ")) {
		/* time zone */
		if (d->zone) {
			fs(d->zone);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "TIME_T")) {
		char	buf[20];

		sprintf(buf, "%d", (int)d->date);
		fs(buf);
		return (strVal);
	}

	if (streq(kw, "UTC")) {
		char	*utcTime;
		char	*sccs_utctime();
		if (utcTime = sccs_utctime(d)) {
			fs(utcTime);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "UTC-FUDGE")) {
		char	*utcTime;
		char	*sccs_utctime();

		getDate(d);
		d->date -= d->dateFudge;
		if (utcTime = sccs_utctime(d)) {
			fs(utcTime);
			d->date += d->dateFudge;
			return (strVal);
		}
		d->date += d->dateFudge;
		return (nullVal);
	}

	if (streq(kw, "FUDGE")) {
		char	buf[20];

		sprintf(buf, "%d", (int)d->dateFudge);
		fs(buf);
		return (strVal);
	}

	if (streq(kw, "CHKSUM")) {
		if (d->flags & D_CKSUM) {
			char	buf[20];

			sprintf(buf, "%d", (int)d->sum);
			fs(buf);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "MODE")) {
		char	buf[20];

		sprintf(buf, "%o", (int)d->mode);
		fs(buf);
		return (strVal);
	}

	if (streq(kw, "TYPE")) {
		if (s->state & S_BITKEEPER) {
			fs("BitKeeper");
		} else {
			fs("SCCS");
		}
		return (strVal);
	}

	if (streq(kw, "DPN")) {
		/* per delta path name */
		if (d->pathname) {
			fs(d->pathname);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "SPN")) {
		/* per delta SCCS path name */
		if (d->pathname) {
			char	*p = name2sccs(d->pathname);

			fs(p);
			free(p);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "MGP")) {
		/* merge parent's serial number */
		fd(d->merge);
		return (strVal);
	}

	if (streq(kw, "PARENT")) {
		if (d->parent) {
			fs(d->parent->rev);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "MPARENT")) {	/* print the merge parent if present */
		if (d->merge && (d = sfind(s, d->merge))) {
			fs(d->rev);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "MERGE")) {	/* print this rev if a merge node */
		if (d->merge) {
			fs(d->rev);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "GCA")) {		/* print gca rev if a merge node */
		if (d->merge && (d = gca(sfind(s, d->merge), d->parent))) {
			fs(d->rev);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "GCA2")) {	/* print gca rev if a merge node */
		if (d->merge && (d = gca2(s, sfind(s, d->merge), d->parent))) {
			fs(d->rev);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "PREV")) {
		if (d->next) {
			fs(d->next->rev);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "NEXT")) {
		if (d = sccs_next(s, d)) {
			fs(d->rev);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "KID")) {
		if (d = d->kid) {
			fs(d->rev);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "SIBLINGS")) {
		if (d = d->siblings) {
			fs(d->rev);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "DFB")) {
		/* default branch */
		if (s->defbranch) {
			fs(s->defbranch);
			return (strVal);
		}
		return (nullVal);
	}

	return notKeyword;
}

/*
 * given a string "<token><endMarker>..."
 * extrtact token and put it in a buffer
 * return the length of the token
 */
private int
extractToken(const char *q, const char *end, char *endMarker, char *buf,
	    int maxLen)
{
	const char *t;
	int	len;

	if (buf) buf[0] = '\0';
	/* look for endMarker */
	for (t = q; (t <= end) && !strchr(endMarker, *t); t++);
	unless (strchr(endMarker, *t)) return (-1);
	len = t - q;
	if ((buf) && (len < maxLen)) {
		strncpy(buf, q, len);
		buf[len] = '\0';
	}
	return (len);
}

/*
 * ectract the prefix inside a $each{...} statement
 */
private int
extractPrefix(const char *b, const char *end, char *kwbuf)
{
	const char *t;
	int	len, klen;

	/*
	 * XXX TODO, this does not support
	 * compound statement inside $each{...} yet
	 * We may need to support this later
	 */
	for (t = b; t <= end; t++) {
		while ((t <= end) && (*t != '(')) t++;
		klen = strlen(kwbuf);
		if ((t <= end) && (t[1] == ':') &&
		     !strncmp(&t[2], kwbuf, klen) &&
		     !strncmp(&t[klen + 2], ":)", 2)) {
			len = t - b;
			return (len);
		}
	}
	return (-1);
}

/*
 * ectract the statement portion of a $if(<kw>){....} statement
 * support nested statement
 */
private int
extractStatement(const char *b, const char *end)
{
	const char *t = b;
	int bCnt = 0, len;

	while (t <= end) {
		if (*t == '{') {
			bCnt++;
		} else if (*t == '}') {
			if (bCnt) {
				bCnt--;
			} else {
				len = t -b;
				return (len);
			}
		}
		t++;
	}
	return (-1);
}

/*
 * Evaluate expression, return boolean
 * currently only support one form of expression:
 * 	leftVal = rightVal
 */
private int
eval(char *leftVal, char *op, char *rightVal)
{
	if (streq(op , "=")) {
		return streq(leftVal, rightVal);
	} else {
		assert(streq(op, "!="));
		return (!streq(leftVal, rightVal));
	}
}

/*
 * Expand the dspec string and print result to "out"
 * This function may call itself recursively
 * kw2val() and fprintDelta() are mutually recursive
 */
private int
fprintDelta(FILE *out, char *vbuf,
	    const char *dspec, const char *end, sccs *s, delta *d)
{
#define	KWSIZE 64
#define	VSIZE 1024
#define	extractSuffix(a, b) extractToken(a, b, "}", NULL, 0)
#define	extractKeyword(a, b, c, d) extractToken(a, b, c, d, KWSIZE)
	const char *b, *t, *q = dspec;
	char	kwbuf[KWSIZE], rightVal[VSIZE], leftVal[VSIZE];
	char	*v, op[3];
	int	len, printLF = 1;

	while (q <= end) {
		if (*q == '\\') {
			switch (q[1]) {
			    case 'n': fc('\n'); q += 2; break;
			    case 't': fc('\t'); q += 2; break;
			    case '$': fc('$'); q += 2; break;
			    case 'c': if (q == &end[-1]) {
					printLF = 0;
					q += 2;
					break;
				      } /* else fall thru */
			    default:  fc('\\'); q++; break;
			}
		} else if (*q == ':') {		/* keyword expansion */
			b = &q[1];
			len = extractKeyword(b, end, ":", kwbuf);
			if ((len > 0) && (len < KWSIZE) &&
			    (kw2val(out, NULL, "", 0, kwbuf,
				    "", 0, s, d) != notKeyword)) {
				/* got a keyword */
				q = &b[len + 1];
			} else {
				/* not a keyword */
				fc(*q++);
			}
		} else if ((*q == '$') &&	/* conditional expansion */
		    (q[1] == 'i') && (q[2] == 'f') &&
		    (q[3] == '(') && (q[4] == ':')) {
			v = 0;
			b = &q[5];
			len = extractKeyword(b, end, ":", kwbuf);
			if (len < 0) { return (printLF); } /* error */
			if (b[len + 1] == '=') {
				int vlen;

				op[0] = '='; op[1] = '\0';
				t = &b[len] + 2;
				v = rightVal; *v = '\0';
				leftVal[0] = '\0';
				vlen = extractToken(t, end, ")", v, VSIZE);
				if (vlen < 0) { return (printLF); }  /* error */
				len += vlen + 1;
			} else if (b[len + 1] == '!') {
				int vlen;
				if (b[len + 2] != '=') {
					fprintf(stderr,
			    "Unknown operator '=%c'\n", b[len + 2]);
					return (printLF);
				}
				strcpy(op, "!=");
				t = &b[len] + 3;
				v = rightVal; *v = '\0';
				leftVal[0] = '\0';
				vlen = extractToken(t, end, ")", v, VSIZE);
				if (vlen < 0) { return (printLF); }  /* error */
				len += vlen + 2;
			}
			if (b[len + 2] != '{') {
				/* syntax error */
				fprintf(stderr,
				    "must have '{' in conditional string\n");
				return (printLF);
			}
			if (len && (len < KWSIZE) &&
			    (kw2val(NULL, v ? leftVal: NULL, "", 0, kwbuf,
				    "", 0,  s, d) == strVal) &&
			    (!v || eval(leftVal, op, v))) {
				const char *cb;	/* conditional spec */
				int clen;

				cb = b = &b[len + 3];
				clen = extractStatement(b, end);
				if (clen < 0) { return (printLF); } /* error */
				fprintDelta(out, vbuf, cb, &cb[clen -1], s, d);
				q = &b[clen + 1];
			} else {
				int bcount  = 1; /* brace count */
				for (t = &b[len + 3]; bcount > 0 ; t++) {
					if (*t == '{') {
						bcount++;
					} else if (*t == '}') {
						if (--bcount == 0) break;
					} else if (*t == '\0') {
						break;
					}
				}
				if (t[0] != '}') {
					/* syntax error */
					fprintf(stderr,
					    "unbalance '{' in dspec string\n");
					return (printLF);
				}
				q = &t[1];
			}
		} else if ((*q == '$') &&	/* conditional prefix/suffix */
		    (q[1] == 'e') && (q[2] == 'a') && (q[3] == 'c') &&
		    (q[4] == 'h') && (q[5] == '(') && (q[6] == ':')) {
			const char *prefix, *suffix;
			int	plen, klen, slen;
			b = &q[7];
			klen = extractKeyword(b, end, ":", kwbuf);
			if (klen < 0) { return (printLF); } /* error */
			if ((b[klen + 1] != ')') && (b[klen + 2] != '{')) {
				/* syntax error */
				fprintf(stderr,
	    "must have '((:keywod:){..}{' in conditional prefix/suffix\n");
				return (printLF);
			}
			prefix = &b[klen + 3];
			plen = extractPrefix(prefix, end, kwbuf);
			suffix = &prefix[plen + klen + 4];
			slen = extractSuffix(suffix, end);
			kw2val(
			    out, NULL, prefix, plen, kwbuf, suffix, slen, s, d);
			q = &suffix[slen + 1];
		} else {
			fc(*q++);
		}
	}
	return (printLF);
}

void
sccs_prsdelta(sccs *s, delta *d, int flags, const char *dspec, FILE *out)
{
	const char *end;

	if (d->type != 'D' && !(flags & PRS_ALL)) return;
	if ((s->state & S_SET) && !(d->flags & D_SET)) return;
	if (fprintDelta(out, NULL,
	    dspec, end = &dspec[strlen(dspec) - 1], s, d)) {
	    	fputc('\n', out);
	}
}

/*
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
	int	i = 0;

	if (s->defbranch) fprintf(out, "f d %s\n", s->defbranch);
	if (s->encoding) fprintf(out, "f e %d\n", s->encoding);
	if (s->state & S_BITKEEPER) i |= X_BITKEEPER;
	if (s->state & S_YEAR4) i |= X_YEAR4;
	if (s->state & S_RCS) i |= X_RCSEXPAND;
	if (s->state & S_EXPAND1) i |= X_EXPAND1;
	if (s->state & S_CSETMARKED) i |= X_CSETMARKED;
#ifdef S_ISSHELL
	if (s->state & S_ISSHELL) i |= X_ISSHELL;
#endif
	if (s->state & S_HASH) i |= X_HASH;

	if (i) fprintf(out, "f x %u\n", i);
	if (s->random) fprintf(out, "R %s\n", s->random);
	EACH(s->text) fprintf(out, "T %s\n", s->text[i]);
	fprintf(out, "\n");
}

#define	FLAG(c)	((buf[0] == 'f') && (buf[1] == ' ') &&\
		(buf[2] == c) && (buf[3] == ' '))

sccs	*
sccs_getperfile(MMAP *in, int *lp)
{
	sccs	*s = calloc(1, sizeof(sccs));
	int	unused = 1;
	char	*buf;

	s->state |= S_BITKEEPER;		/* duh */
	unless (buf = mkline(mnext(in))) goto err;
	unless (buf[0]) {
		free(s);
		return (0);
	}
	(*lp)++;
	if (FLAG('d')) {
		unused = 0;
		s->defbranch = strdup(&buf[4]);
		unless (buf = mkline(mnext(in))) {
err:			fprintf(stderr,
			    "takepatch: file format error near line %d\n", *lp);
			free(s);
			return (0);
		}
		(*lp)++;
	}
	if (FLAG('e')) {
		unused = 0;
		s->encoding = atoi(&buf[4]);
		unless (buf = mkline(mnext(in))) goto err; (*lp)++;
	}
	if (FLAG('x')) {
		int	bits = atoi(&buf[4]);

		unused = 0;
		if (bits & X_BITKEEPER) s->state |= S_BITKEEPER;
		if (bits & X_YEAR4) s->state |= S_YEAR4;
		if (bits & X_RCSEXPAND) s->state |= S_RCS;
		if (bits & X_EXPAND1) s->state |= S_EXPAND1;
		if (bits & X_CSETMARKED) s->state |= S_CSETMARKED;
#ifdef S_ISSHELL
		if (bits & X_ISSHELL) s->state |= S_ISSHELL;
#endif
		if (bits & X_HASH) s->state |= S_HASH;
		unless (buf = mkline(mnext(in))) goto err; (*lp)++;
	}
	if (strneq(buf, "R ", 2)) {
		unused = 0;
		s->random = strnonldup(&buf[2]);
		unless (buf = mkline(mnext(in))) goto err; (*lp)++;
	}
	while (strneq(buf, "T ", 2)) {
		unused = 0;
		s->text = addLine(s->text, strnonldup(&buf[2]));
		unless (buf = mkline(mnext(in))) goto err; (*lp)++;
	}
	if (buf[0]) goto err;		/* should be empty */

	if (unused) {
		free(s);
		return (0);
	}
	return (s);
}

private void
do_patch(sccs *s, delta *start, delta *stop, int flags, FILE *out)
{
	int	i;	/* used by EACH */
	symbol	*sym;
	char	type;

	/*
	 * XXX - does it ever make sense to call this for more than one
	 * delta?
	 */
	if (!start) return;
	if (!stop || (stop != start)) {
		do_patch(s, start->kid, stop, flags, out);
		do_patch(s, start->siblings, stop, flags, out);
	}
	type = start->type;
	if ((start->type == 'R') &&
	    start->parent && streq(start->rev, start->parent->rev)) {
	    	type = 'M';
	}
	fprintf(out, "%c %s %s%s %s%s%s +%u -%u\n",
	    type, start->rev, start->sdate,
	    start->zone ? start->zone : "",
	    start->user,
	    start->hostname ? "@" : "",
	    start->hostname ? start->hostname : "",
	    start->added, start->deleted);

	/*
	 * Order from here down is alphabetical.
	 */
	if (start->csetFile) fprintf(out, "B %s\n", start->csetFile);
	if (start->flags & D_CSET) fprintf(out, "C\n");
	EACH(start->comments) {
		assert(start->comments[i][0] != '\001');
		fprintf(out, "c %s\n", start->comments[i]);
	}
	if (start->dateFudge) fprintf(out, "F %d\n", (int)start->dateFudge);
	EACH(start->include) {
		delta	*d = sfind(s, start->include[i]);
		assert(d);
		fprintf(out, "i ");
		sccs_pdelta(s, d, out);
		fprintf(out, "\n");
	}
	if (start->flags & D_CKSUM) fprintf(out, "K %u\n", start->sum);
	if (start->merge) {
		delta	*d = sfind(s, start->merge);
		assert(d);
		fprintf(out, "M ");
		sccs_pdelta(s, d, out);
		fprintf(out, "\n");
	}
	if (start->flags & D_MODE) {
		fprintf(out, "O %s", mode2a(start->mode));
		if (S_ISLNK(start->mode)) {
			assert(start->symlink);
			fprintf(out, " %s\n", start->symlink);
		} else {
			fprintf(out, "\n");
		}
	}
	if (s->tree->pathname) assert(start->pathname);
	if (start->pathname) fprintf(out, "P %s\n", start->pathname);
	if (start->flags & D_SYMBOLS) {
		for (sym = s->symbols; sym; sym = sym->next) {
			unless (sym->metad == start) continue;
			fprintf(out, "S %s\n", sym->name);
		}
	}
	if (start->flags & D_TEXT) {
		if (start->text) {
			EACH(start->text) {
				fprintf(out, "T %s\n", start->text[i]);
			}
		} else {
			fprintf(out, "T\n");
		}
	}
	EACH(start->exclude) {
		delta	*d = sfind(s, start->exclude[i]);
		assert(d);
		fprintf(out, "x ");
		sccs_pdelta(s, d, out);
		fprintf(out, "\n");
	}
	if (start->flags & D_XFLAGS) {
		fprintf(out, "X %u\n", start->xflags);
	}
	if (s->tree->zone) assert(start->zone);
	fprintf(out, "------------------------------------------------\n");
}

private void
prs_reverse(sccs *s, delta *d, int flags, char *dspec, FILE *out)
{
	if (d->next) prs_reverse(s, d->next, flags, dspec, out);
	if (d->flags & D_SET) sccs_prsdelta(s, d, flags, dspec, out);
}

private void
prs_forward(sccs *s, delta *d, int flags, char *dspec, FILE *out)
{
	for (; d; d = d->next) {
		if (d->flags & D_SET) sccs_prsdelta(s, d, flags, dspec, out);
	}
}

int
sccs_prs(sccs *s, u32 flags, int reverse, char *dspec, FILE *out)
{
	delta	*d;
#define	DEFAULT_DSPEC \
":DT: :I: :D: :T::TZ: :P:$if(:HT:){@:HT:} :DS: :DP: :Li:/:Ld:/:Lu:\n\
$if(:DPN:){P :DPN:\n}$each(:SYMBOL:){S (:SYMBOL:)\n}\
$if(:C:){$each(:C:){C (:C:)}\n}\
------------------------------------------------"

	if (!dspec) dspec = DEFAULT_DSPEC;
	GOODSCCS(s);
	if (flags & PRS_PATCH) {
		do_patch(s,
		    s->rstart ? s->rstart : s->tree,
		    s->rstop, flags, out);
		return (0);
	}
	/* print metadata if they asked */
	if (flags & PRS_META) {
		symbol	*sym;
		for (sym = s->symbols; sym; sym = sym->next) {
			fprintf(out, "S %s %s\n", sym->name, sym->rev);
		}
	}
	unless (s->state & S_SET) {
		for (d = s->rstop; d; d = d->next) {
			d->flags |= D_SET;
			if (d == s->rstart) break;
		}
	}
	if (flags & PRS_ALL) sccs_markMeta(s);

	if (reverse) prs_reverse(s, s->table, flags, dspec, out);
	else prs_forward(s, s->table, flags, dspec, out);
	return (0);
}

#ifndef	ANSIC

#ifndef	_NSIG
#define	_NSIG	32	/* XXX - might be wrong, probably OK */
#endif

private int	caught[_NSIG];
private struct	sigaction savesig[_NSIG];
private void	catch(int sig) { caught[sig]++; }

/*
 * Signal theory of operation:
 *	block signals when initializing,
 *	unblock when we are going to do anything that takes a long time.
 *	XXX - doesn't stack more than one deep.	 Bad for a library.
 */
int
sig(int what, int sig)
{
	struct	sigaction sa;
	sigset_t sigs;

	assert(sig > 0);
	assert(sig < _NSIG);
	switch (what) {
	    case CATCH:
		bzero(&sa, sizeof(sa));
		sa.sa_handler = catch;
		sigaction(sig, &sa, &savesig[sig]);
		break;
	    case UNCATCH:
		sigaction(sig, &savesig[sig], 0);
		break;
	    case BLOCK:
		sigemptyset(&sigs);
		sigaddset(&sigs, sig);
		sigprocmask(SIG_BLOCK, &sigs, 0);
		break;
	    case UNBLOCK:
		sigemptyset(&sigs);
		sigaddset(&sigs, sig);
		sigprocmask(SIG_UNBLOCK, &sigs, 0);
		break;
	    case CHKPENDING:
		sigemptyset(&sigs);
		sigpending(&sigs);
		return (sigismember(&sigs, sig));
	    case CAUGHT:
		return (caught[sig]);
	    case CLEAR:
		caught[sig] = 0;
		break;
	}
	return (0);
}
#endif	/* !ANSIC */

/* --------------------- module smoosh ---------------------------- */

private inline int
strmatch(char *s, char *t)
{
	if (!s && !t) return (1);
	if ((s && !t) || (!s && t) || strcmp(s, t)) return (0);
	return (1);
}

/*
 * Check user/host/pathname/date/lines/comments
 */
private inline int
samedelta(delta *l, delta *r)
{
	return ((l->added == r->added) &&
	    (l->deleted == r->deleted) &&
	    (l->same == r->same) &&
	    strmatch(l->user, r->user) &&
	    strmatch(l->hostname, r->hostname) &&
	    strmatch(l->pathname, r->pathname) &&
	    strmatch(l->sdate, r->sdate));
}

void
because(delta *a, delta *b)
{
	unless (a && b) {
		printf("Because one is missing\n");
		return;
	}
	sccs_print(a);
	sccs_print(b);
	printf("Because ");
	if (a->added != b->added) printf("added ");
	if (a->deleted != b->deleted) printf("deleted ");
	if (a->same != b->same) printf("same ");
	unless (strmatch(a->user, b->user))
		printf("user '%s' '%s' ", a->user, b->user);
	unless (strmatch(a->hostname, b->hostname))
		printf("hostname '%s' '%s' ", a->hostname, b->hostname);
	unless (strmatch(a->pathname, b->pathname))
		printf("pathname '%s' '%s' ", a->pathname, b->pathname);
	unless (strmatch(a->sdate, b->sdate))
		printf("sdate '%s' '%s'", a->sdate, b->sdate);
	printf("\n");
}

private sccs	*left, *right;	/* globals for graft. */

/*
 * return the number of nodes, including this one, in this subgraph.
 */
private int
numNodes(delta *d)
{
	if (!d || (d->type != 'D')) return (0);

	return (1 + numNodes(d->kid) + numNodes(d->siblings));
}

int
dcmp(const void *a, const void *b)
{
	return ((*(delta **)a)->date - (*(delta **)b)->date);
}

void
addNodes(sccs *s, delta **list, int j, delta *d)
{
	if (!d || (d->type != 'D')) return;
	list[j++] = d;
	addNodes(s, list, j, d->kid);
	while (list[j]) j++;
	addNodes(s, list, j, d->siblings);
}

/*
 * Spit out the command line for a mkpatch which will generate the new work
 * in the left file for incorporation into the right file.
 */
private void
mkpatch(sccs *s, delta *a)
{
	delta	**alist;
	int	i;

	alist = calloc((i = numNodes(a)) + 1, sizeof(delta *));
	addNodes(s, alist, 0, a);
	qsort((void*)alist, i, sizeof(delta *), dcmp);
	i = 0;
	while (alist[i]) {
		if ((alist[i+1] != 0) && (alist[i+1] == alist[i])) {
			i++;
			continue;
		}
		printf("%s:%s\n", left->gfile, alist[i++]->rev);
	}
	free(alist);
}

/*
 * The two deltas passed in are the same.   Link them.
 * For each kid/sibling, try linking that one as well.
 * If they don't link up, spit out the rmdel and mkpatch lines which
 * will regenerate the union from the left and right into the right.
 */
private void
linktree(sccs *s, delta *l, delta *r)
{
	delta	*a, *b;

	if (l->link || r->link) return;		/* insurance */
	l->link = r;
	r->link = l;
	//printf("L(%s%c, %s%c)\n", l->rev, l->type, r->rev, r->type);

	/*
	 * What I want is a loop like this:
	 *
	 *	foreach lk (l->kid, @l->siblings) {
	 *		foreach rk (r->kid, @r->siblings) {
	 *			if samedelta(lk, rk) ....
	 *		}
	 *	}
	 */
	for (a = l->kid; a; a = a->siblings) {
		if (a->type != 'D') continue;
		assert(!a->link);
		for (b = r->kid; b; b = b->siblings) {
			if ((b->type != 'D') || b->link) continue;
			if (samedelta(a, b)) {
				linktree(s, a, b);
			}
		}
	}
	for (a = l->kid; a; a = a->siblings) {
		unless (a->link || (a->type != 'D')) mkpatch(s, a);
	}
}

int
sccs_smoosh(char *lfile, char *rfile)
{
	int	error = 0;

	left = sccs_init(lfile, 0, 0);
	right = sccs_init(rfile, 0, 0);
	if (!left || !HAS_SFILE(left) || !right || !HAS_SFILE(right)) {
		error = 100;
		goto out;
	}
	if (!samedelta(left->tree, right->tree)) {
		because(left->tree, right->tree);
		error = 101;
		goto out;
	}
	linktree(left, left->tree, right->tree);
out:	sccs_free(left);
	sccs_free(right);
	left = right = 0;
	return (error);
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

private inline int
samekey(delta *d, char *user, char *host, char *path, time_t date,
	sum_t *cksump)
{
	getDate(d);
	if (d->date != date) {
//printf("%s: %d (%s%s) vs %d\n", d->rev, d->date, d->sdate, d->zone ? d->zone : "", date);
		return (0);
	}
//printf("USER %s %s\n", d->user, user);
	unless (streq(d->user, user)) return (0);
//printf("HOST %s %s\n", d->hostname, host);
	if (d->hostname) {
		unless (host && streq(d->hostname, host)) return (0);
	} else if (host) {
		return (0);
	}
//printf("PATH %s %s\n", d->pathname, path);
	if (d->pathname) {
		unless (path && streq(d->pathname, path)) return (0);
	} else if (path) {
		return (0);
	}
	/* XXX: all d->cksum are valid: we'll assume always there */
	if (cksump) {
		unless (d->sum == *cksump) return (0);
	}
//printf("MATCH\n");
	return (1);
}

/*
 * This gets a GCA which tends to be on the trunk.
 * Because it doesn't look up the mparent path, it tends not to get
 * the closest gca.
 */
private delta *
gca(delta *left, delta *right)
{
	delta	*d;

	unless (left && right) return (0);
	/*
	 * Clear the visited flag up to the root via one path,
	 * set it via the other path, then go look for it.
	 */
	for (d = left; d; d = d->parent) d->flags &= ~D_VISITED;
	for (d = right; d; d = d->parent) d->flags |= D_VISITED;
	for (d = left; d; d = d->parent) {
		if (d->flags & D_VISITED) return (d);
	}
	return (0);
}

private delta *
gca2(sccs *s, delta *left, delta *right)
{
	delta	*d;
	char	*slist;
	int	value;

	unless (s && s->nextserial && left && right) return (0);

	slist = calloc(s->nextserial, sizeof(char));
	slist[left->serial] |= 1;
	slist[right->serial] |= 2;
	d = (left->serial > right->serial) ? left : right;
	for ( ; d ; d = d->next) {
		unless (d->type == 'D') continue;
		unless (value = slist[d->serial]) continue;
		if (value == 3) break;
		if (d->parent)  slist[d->parent->serial] |= value;
		if (d->merge)   slist[d->merge] |= value;
	}
	free(slist);
	return (d);
}

delta	*
sccs_gca(sccs *s, delta *left, delta *right, int best)
{
	return (best ? gca2(s, left, right) : gca(left, right));
}

private int
samelod(sccs *s, delta *a, delta *b)
{
	assert(s && a && b);
	return (a->r[0] == b->r[0]);
}

private delta **
sccs_lodmap(sccs *s)
{
	delta	**lodmap;
	delta	*d;
	u16	next;

	next = sccs_nextlod(s);
	/* next now equals one more than greatest that exists
	 * make room for having a lodmap[next] slot by extending it by one
	 */
	next++;
	unless (lodmap = calloc(next, sizeof(lodmap))) {
		perror("calloc lodmap");
		return(0);
	}

	for (d = s->table; d; d = d->next) {
		if ((d->type == 'D') && (d->r[1] == 1) && (d->r[2] == 0)) {
			lodmap[d->r[0]] = d;
		}
	}

	return (lodmap);
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
	delta	*p, *d, *g = 0, *a = 0, *b = 0;
	char	*n[3];
	u8	*lodmap;
	u16	next;
	u16	defbranch;
	int	retcode = -1;

	next = sccs_nextlod(s);
	/* next now equals one more than greatest that exists
	 * so last entry in array is lodmap[next-1] which is 
	 * is current max.
	 */
	unless (lodmap = calloc(next, sizeof(lodmap))) {
		perror("calloc lodmap");
		goto err;
	}

	defbranch = (s->defbranch) ? atoi(s->defbranch) : (next - 1);

	/*
	 * b is that branch which needs to be merged.
	 * At any given point there should be exactly one of these.
	 * LODXXX - can be two if there are LODs.
	 */
	for (d = s->table; d; d = d->next) {
		if (d->type != 'D') continue;
		if ((d->flags & D_MERGED) || !isleaf(d)) continue;
		if (d->r[0] == defbranch) {
			if (!a) {
				a = d;
			} else {
				assert(!b);
				b = d;
				/* Could break but I like the error checking */
			}
			continue;
		}
		unless (lodmap[d->r[0]]++) continue; /* if first leaf */

		fprintf(stderr, "\ntakepatch: ERROR: conflict on lod %d\n",
			d->r[0]);
		goto err;
	}

	/*
	 * If we have no conflicts, then make sure the paths are the same.
	 * What we want to compare is whatever the tip path is with the
	 * whatever the path is in the most recent delta in this LOD.
	 * XXX - Rick, I don't do the lod stuff yet.
	 */
	unless (b) {
		for (p = s->table;
		    p && ((p->type == 'R') || (p->flags & D_REMOTE));
		    p = p->next);
		if (!p || streq(p->pathname, a->pathname)) {
			free(lodmap);
			return (0);
		}
		b = a;
		a = g = p;
		retcode = 0;
		goto rename;
	} else {
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
	if (a->flags & D_LOCAL) {
		fprintf(f, "merge deltas %s %s %s %s %s\n",
			a->rev, g->rev, b->rev, sccs_getuser(), now());
	} else {
		fprintf(f, "merge deltas %s %s %s %s %s\n",
			b->rev, g->rev, a->rev, sccs_getuser(), now());
	}
	fclose(f);
	unless (streq(g->pathname, a->pathname) &&
	    streq(g->pathname, b->pathname)) {
rename:		n[1] = name2sccs(g->pathname);
		if (b->flags & D_LOCAL) {
			n[2] = name2sccs(a->pathname);
			n[0] = name2sccs(b->pathname);
		} else {
			n[0] = name2sccs(a->pathname);
			n[2] = name2sccs(b->pathname);
		}
		unless (f = fopen(sccsXfile(s, 'm'), "w")) {
			perror("m.file");
			goto err;
		}
		fprintf(f, "rename %s %s %s\n", n[0], n[1], n[2]);
		fclose(f);
		free(n[0]);
		free(n[1]);
		free(n[2]);
	}
	/* retcode set above */
err:
	if (lodmap) free(lodmap);
	return (retcode);
}

/*
 * Take a key like sccs_sdelta makes and find it in the tree.
 */
delta *
sccs_findKey(sccs *s, char *key)
{
	char	*parts[6];	/* user, host, path, date as integer */
	char	*user, *host, *path, *random;
	sum_t	cksum;
	sum_t	*cksump = 0;
	time_t	date;
	delta	*e;
	char	buf[MAXPATH];

	unless (s && s->tree) return (0);
	debug((stderr, "findkey(%s)\n", key));
	strcpy(buf, key);
	explodeKey(buf, parts);
	user = parts[0];
	host = parts[1];
	path = parts[2];
	date = date2time(&parts[3][2], 0, EXACT);
	if (parts[4]) {
		cksum = atoi(parts[4]);
		cksump = &cksum;
	};
	random = parts[5];
	if (random) { /* then sfile must have random and it must match */
		unless (s->random && streq(s->random, random)) return (0);
	}
	if (samekey(s->tree, user, host, path, date, cksump))
		return (s->tree);
	for (e = s->table;
	    e && !samekey(e, user, host, path, date, cksump);
	    e = e->next);
	return (e);
}

void
sccs_print(delta *d)
{
	fprintf(stderr, "%c %s %s%s %s%s%s %d %d %u/%u/%u %s 0x%x\n",
	    d->type, d->rev,
	    d->sdate, d->zone ? d->zone : "",
	    d->user,
	    d->hostname ? "@" : "", d->hostname ? d->hostname : "",
	    d->serial, d->pserial,
	    d->added, d->deleted, d->same,
	    d->pathname ? d->pathname : "", d->flags);
}

/* return the time of the delta in UTC.
 * Do not change times without time zones to localtime.
 */
char *
sccs_utctime(delta *d)
{
	struct	tm *tp;
	static	char sdate[30];

	tp = utc2tm(getDate(d));
	sprintf(sdate, "%d%02d%02d%02d%02d%02d",
	    tp->tm_year + 1900,
	    tp->tm_mon + 1,
	    tp->tm_mday,
	    tp->tm_hour,
	    tp->tm_min,
	    tp->tm_sec);
	return (sdate);
}

void
sccs_pdelta(sccs *s, delta *d, FILE *out)
{
	assert(d);
	fprintf(out, "%s%s%s|%s|%s|%05u",
	    d->user,
	    d->hostname ? "@" : "",
	    d->hostname ? d->hostname : "",
	    d->pathname ? d->pathname : "",
	    sccs_utctime(d),
	    d->sum);
	unless (s && s->tree && sccs_ino(s) == d && s->random) return;
	fprintf(out, "|%s", s->random);
}

/* Get the checksum of the 5 digit checksum */

int
sccs_sdelta(sccs *s, delta *d, char *buf)
{
	char	*tail;
	int	len;

	assert(d);
	len = sprintf(buf, "%s%s%s|%s|%s|%05u",
	    d->user,
	    d->hostname ? "@" : "",
	    d->hostname ? d->hostname : "",
	    d->pathname ? d->pathname : "",
	    sccs_utctime(d),
	    d->sum);
	assert(len);
	unless (s && s->tree && sccs_ino(s) == d && s->random) return (len);
	for (tail = buf; *tail; tail++);
	len += sprintf(tail, "|%s", s->random);
	return (len);
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
	if (*key == '@') {
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

	if (streq(d->sdate, "70/01/01 00:00:00") && streq(d->user, "Fake")) {
		d = d->kid;
	}
	return (d);
}

int
sccs_reCache(void)
{
	char buf[MAXPATH];
	char	*av[4];

	/* sfiles -r */
	sprintf(buf, "%s%s", getenv("BK_BIN"), SFILES);
	av[0] = SFILES; av[1] = "-r"; av[2] = 0;
	return spawnvp_ex(_P_WAIT, buf, av);
}

/*
 * Figure out if the file is gone from the DB.
 * XXX - currently only works with keys, not filenames.
 */
int
gone(char *key, MDBM *db)
{
	unless (strchr(key, '|')) return (0);
	unless (db) return (0);
	return (mdbm_fetch_str(db, key) != 0);
}

MDBM	*
loadDB(char *file, int (*want)(char *), int style)
{
	MDBM	*DB = 0;
	FILE	*f = 0;
	char	*v;
	int	first = 1;
	int	flags;
	char	buf[MAXLINE];
	char	*av[4];

	// XXX awc->lm: we should check the z lock here
	// someone could be updating the file...
again:	unless (f = fopen(file, "rt")) {
		if (first && streq(file, IDCACHE)) {
			first = 0;
			if (sccs_reCache()) goto out;
			goto again;
		}
		if (first && streq(file, GONE) && exists(SGONE)) {
			first = 0;
			/* get -s */
			sprintf(buf, "%s%s", getenv("BK_BIN"), GET);
			av[0] = GET; av[1] = "-s"; av[2] = GONE; av[3] = 0;
			spawnvp_ex(_P_WAIT, buf, av);
			goto again;
		}
out:		if (f) fclose(f);
		if (DB) mdbm_close(DB);
		return (0);
	}
	DB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	assert(DB);
	switch (style & (DB_NODUPS|DB_USEFIRST|DB_USELAST)) {
	    case DB_NODUPS:	flags = MDBM_INSERT; break;
	    case DB_USEFIRST:	flags = MDBM_INSERT; break;
	    case DB_USELAST:	flags = MDBM_REPLACE; break;
	    default:
		fprintf(stderr, "Bad option to loadDB: %x\n", style);
		exit(1);
	}
	while (fnext(buf, f)) {
		if (buf[0] == '#') continue;
		if (want && !want(buf)) continue;
		if (chop(buf) != '\n') {
			fprintf(stderr, "bad path: <%s> in %s\n", buf, file);
			return (0);
		}
		if (style & DB_KEYSONLY) {
			v = "";
		} else {
			v = strchr(buf, ' ');
			assert(v);
			*v++ = 0;
		}
		switch (mdbm_store_str(DB, buf, v, flags)) {
		    case 0: break;
		    case 1:
		    	if ((style == DB_NODUPS)) {
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
	fclose(f);
	return (DB);
}

/*
 * Get all the ids associated with a changeset.
 * The db is db{root rev Id} = cset rev Id.
 *
 * Note: does not call sccs_restart, the caller of this sets up "s".
 */
int
csetIds(sccs *s, char *rev)
{
	kvpair	kv;
	char	*t;

	assert(s->state & S_HASH);
	if (sccs_get(s, rev, 0, 0, 0, SILENT|GET_HASHONLY, 0)) {
		sccs_whynot("get", s);
		return (-1);
	}
	unless (s->mdbm) {
		fprintf(stderr, "get: no mdbm found\n");
		return (-1);
	}

	/* If we are the new key format, then we shouldn't have mixed keys */
	if (s->state & S_KEY2) return (0);

	/*
	 * If there are both long and short keys, then use the long form
	 * and delete the short form (the long form is later).
	 */
	for (kv = mdbm_first(s->mdbm); kv.key.dsize; kv = mdbm_next(s->mdbm)) {
		unless (t = sccs_iskeylong(kv.key.dptr)) continue;
		*t = 0;
		if (mdbm_fetch_str(s->mdbm, kv.key.dptr)) {
			mdbm_delete_str(s->mdbm, kv.key.dptr);
		}
		*t = '|';
	}
	return (0);
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
sccs_keyinit(char *key, u32 flags, MDBM *idDB)
{
	datum	k, v;
	char	*p;
	sccs	*s;
	char	buf[MAXPATH];
	char	*localkey = 0;

	/* Id cache contains long and short keys */
	k.dptr = key;
	k.dsize = strlen(key) + 1;
	v = mdbm_fetch(idDB, k);
	if (v.dsize) {
		p = name2sccs(v.dptr);
	} else {
		char	*t, *r;

		for (t = k.dptr; *t++ != '|'; );
		for (r = t; *r != '|'; r++);
		assert(*r == '|');
		*r = 0;
		p = name2sccs(t);
		*r = '|';
	}
	s = sccs_init(p, flags, 0);
	free(p);
	unless (s && HAS_SFILE(s))  goto out;
	sccs_sdelta(s, sccs_ino(s), buf);
	/* modifies buf and key, so copy key to local key */
	localkey = strdup(key);
	assert(localkey);
	unless (samekeystr(buf, localkey))  goto out;
	free(localkey);
	return (s);

out:	if (s) sccs_free(s);
	if (localkey) free(localkey);
	return (0);
}

/*
 * Print a unique key for each delta.
 *
 * If we are doing this for resync, all print the list of leaves.
 * Note: if there is only one delta, i.e., 1.1 is also a leaf, I don't
 * print it twice.
 *
 * XXX - probably obsolete
 */
void
sccs_ids(sccs *s, u32 flags, FILE *out)
{
	delta	*d = s->tree;

	sccs_pdelta(s, sccs_ino(s), out);
	for (d = s->table; d; d = d->next) {
		if (!d->kid && (d->type == 'D')) {
			fprintf(out, " ");
			sccs_pdelta(s, d, out);
		}
	}
	fprintf(out, "\n");
}

#ifdef	DEBUG
debug_main(char **av)
{
	fprintf(stderr, "===<<<");
	do {
		fprintf(stderr, " %s", av[0]);
		av++;
	} while (av[0]);
	fprintf(stderr, " >>>===\n");
}
#endif

/*
 * Return an alloced copy of the name of the sfile after the removed deltas
 * are gone.
 * Note that s->gfile may not be the same as s->table->pathname because we
 * may be operating on RESYNC/foo/SCCS/s.bar.c instead of foo/SCCS/s.bar.c
 *
 * XXX - I'm using s->table to mean TOT.
 */
char	*
currentName(sccs *s)
{
	delta	*d;
	char	*t;
	int	len;
	char	path[MAXPATH];

	/*
	 * LODXXX - needs to restrict this to the current LOD.
	 */
	for (d = s->table; d && (d->flags & D_GONE); d = d->next);
	assert(d);

	/* If the current and old name are the same, use what we have */
	if (streq(d->pathname, s->table->pathname)) return (strdup(s->sfile));

	/* If we have no prefix, just convert to an s.file */
	if (streq(s->gfile, s->table->pathname)) 
	return (name2sccs(d->pathname));

	/*
	 * Figure out how much space we need
	 */
	t = strstr(s->table->pathname, s->gfile);
	assert(t);
	len = t - s->gfile;
	strncpy(path, s->gfile, len);
	path[len] = 0;
	strcat(path, d->pathname);
	fprintf(stderr, "RMDEL %s\n", path);
	return (name2sccs(path));
}

/*
 * Given an SCCS structure with a list of marked deltas, strip them from
 * the delta table and place the striped body in out
 */
int
stripDeltas(sccs *s, FILE *out)
{
	serlist *state = 0;
	ser_t	*slist = 0;
	int	print = 0;
	char	*buf;
	int	ser;

	slist = setmap(s, D_SET, 1);

	state = allocstate(0, 0, s->nextserial);
	seekto(s, s->data);
	if (s->encoding & E_GZIP) {
		zgets_init(s->where, s->size - s->data);
		zputs_init();
	}
	while (buf = nextdata(s)) {
		if (isData(buf)) {
			unless (print) fputdata(s, buf, out);
			continue;
		}
		debug2((stderr, "%.*s", linelen(buf), buf));
		ser = atoi(&buf[3]);
		if (slist[ser] == 0) fputdata(s, buf, out);
		changestate(state, buf[1], ser);
		print =
		    visitedstate((const serlist*)state, (const ser_t*)slist);
	}
	free(state);
	free(slist);
	if (fflushdata(s, out)) return (1);
	if (s->encoding & E_GZIP) zgets_done();
	fseek(out, 0L, SEEK_SET);
	fprintf(out, "\001h%05u\n", s->cksum);
	sccs_close(s);
	fclose(out);
	unlink(s->sfile);		/* Careful. */
	buf = sccsXfile(s, 'x');
	if (rename(buf, s->sfile)) {
		fprintf(stderr,
		    "stripdel: can't rename(%s, %s) left in %s\n",
		    buf, s->sfile, buf);
		return (1);
	}
	Chmod(s->sfile, 0444);
	return (0);
}

/*
 * Strip out the deltas marked with D_SET.
 */
int
sccs_stripdel(sccs *s, char *who)
{
	FILE	*sfile = 0;
	int	error = 0;
	int	locked;

	assert(s && s->tree && !HAS_PFILE(s));
	debug((stderr, "stripdel %s %x\n", s->gfile, flags));
	unless (locked = sccs_lock(s, 'z')) {
		fprintf(stderr, "%s: can't get lock on %s\n", who, s->sfile);
		error = -1; s->state |= S_WARNED;
out:
		/* sfile closed by stripDeltas() */
		if (locked) sccs_unlock(s, 'z');
		debug((stderr, "stripdel returns %d\n", error));
		return (error);
	}
#define	OUT	\
	do { error = -1; s->state |= S_WARNED; goto out; } while (0)


	if (stripChecks(s, 0, who)) OUT;

	unless (sfile = fopen(sccsXfile(s, 'x'), "w")) {
		fprintf(stderr,
		    "%s: can't create %s: ", who, sccsXfile(s, 'x'));
		perror("");
		OUT;
	}

	/* write out upper half */
	if (delta_table(s, sfile, 0, 0)) {  /* 0 means as-is, so chksum works */
		fprintf(stderr,
		    "%s: can't write delta table for %s\n", who, s->sfile);
		sccs_unlock(s, 'x');
		fclose(sfile);
		OUT;
	}

	/* write out the lower half */
	if (stripDeltas(s, sfile)) {
		fprintf(stderr,
		    "%s: can't write delta body for %s\n", who, s->sfile);
		sccs_unlock(s, 'x');
		OUT;
	}
	goto out;
#undef	OUT
}

/*
 * Remove the delta, leaving a delta of type 'R' behind.
 */
int
sccs_rmdel(sccs *s, delta *d, u32 flags)
{
	d->flags |= D_SET;
	if (stripChecks(s, d, "rmdel")) return (1);

	d->type = 'R';	/* mark delta as Removed */
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
	unless (d->parent) {	/* don't remove if this is 1.1 (no parent) */
		fprintf(stderr,
			"%s: can't remove root change %s in %s.\n",
			who, d->rev, s->sfile);
		s->state |= S_WARNED;
		return (1);
	}
	return (0);
}

int
smartUnlink(char *file)
{
	int	rc;
	int	save = 0;

#undef	unlink
	unless (rc = unlink(file)) return (0);
	save = errno;
	chmod(file, S_IWRITE);
	unless (rc = unlink(file)) return (0);
	unless (access(file, 0)) {
		fprintf(stderr, "smartUnlink:can not unlink %s, errno = %d\n",
		    file, save);
	}
	errno = save;
	return (rc);
}

int
smartRename(char *old, char *new)
{
	int	rc;
	int	save = 0;

#undef	rename
	unless (rc = rename(old, new)) return (0);
	save = errno;
	if (smartUnlink(new)) {
		debug((stderr, "smartRename: unlink fail for %s, errno=%d\n",
		    new, errno));
		errno = save;
		return (rc);
	}
	unless (rc = rename(old, new)) return (0);
	fprintf(stderr, "smartRename: can not rename from %s to %s, errno=%d\n",
	    old, new, errno);
	errno = save;
	return (rc);
}
