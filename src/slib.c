/*
 * SCCS library - all of SCCS is implemented here.  All the other source is
 * no more than front ends that call entry points into this library.
 * It's one big file so I can hide a bunch of stuff as statics (private).
 *
 * XXX - I don't handle memory allocation failures well.
 *
 * Copyright (c) 1997-1998 Larry McVoy.	 All rights reserved.
 */
#include "sccs.h"
WHATSTR("%W%");

delta	*sfind(sccs *s, int serial);
private delta	*rfind(sccs *s, char *rev);
private void	dinsert(sccs *s, int flags, delta *d);
private int	samebranch(delta *a, delta *b);
private char	*sccsXfile(sccs *sccs, char type);
private int	badcksum(sccs *s);
private int	printstate(const serlist *state, const ser_t *slist);
private void	changestate(register serlist *state, char type, int serial);
private serlist *allocstate(serlist *old, int oldsize, int n);
private void	end(sccs *, delta *, FILE *, int, int, int, int);
private void	date(delta *d, time_t tt);
#ifndef	ANSIC
private int	sig(int what, int sig);
#endif
private int	getflags(sccs *s, char *buf);
private int	addsym(sccs *s, delta *d, delta *reald, char *a, char *b);
private void	inherit(sccs *s, int flags, delta *d);
private void	linktree(sccs *s, delta *l, delta *r);
private sum_t	fputsum(sccs *s, char *buf, FILE *out);
private void	putserlist(sccs *sc, ser_t *s, FILE *out);
private ser_t*	getserlist(sccs *sc, int isSer, char *s, int *ep);
private int	read_pfile(char *who, sccs *s, pfile *pf);
private int	hasComments(delta *d);
private int	checkRev(char *file, delta *d, int flags);
private int	checkrevs(sccs *s, int flags);
private delta*	csetArg(delta *d, char *name);
private delta*	csetFileArg(delta *d, char *name);
private delta*	hostArg(delta *d, char *arg);
private delta*	pathArg(delta *d, char *arg);
private delta*	zoneArg(delta *d, char *arg);
private delta*	modeArg(delta *d, char *arg);
private delta*	mergeArg(delta *d, char *arg);
private delta*	sumArg(delta *d, char *arg);
private	void	symArg(sccs *s, delta *d, char *name);
private void	lodArg(sccs *s, delta *d, char *name);
private int	delta_rm(sccs *s, delta *d, FILE *sfile, int flags);
private int	delta_rmchk(sccs *s, delta *d);
private int	delta_destroy(sccs *s, delta *d);
private int	delta_strip(sccs *s, delta *d, FILE *sfile, int flags);
void		explodeKey(char *key, char *parts[4]);
private time_t	getDate(delta *d);
private	void	unlinkGfile(sccs *s);
private time_t	date2time(char *asctime, char *z, int roundup);
private	char	*sccsrev(delta *d);
private int	addLod(char *name, sccs *sc, int flags, admin *l, int *ep);
private int	addSym(char *name, sccs *sc, int flags, admin *l, int *ep);
int		smartUnlink(char *name);
int		smartRename(char *old, char *new);
private void	updatePending(sccs *s, delta *d);

private unsigned int u_mask = 0x5eadbeef;

int
executable(char *f)
{
	return (access(f, X_OK) == 0);
}

int
isdir(char *s)
{
	struct	stat sbuf;

	if (stat(s, &sbuf) == -1) return 0;
	return (S_ISDIR(sbuf.st_mode));
}

int
isreg(char *s)
{
	struct	stat sbuf;

	if (stat(s, &sbuf) == -1) return 0;
	return (S_ISREG(sbuf.st_mode));
}

int
size(char *s)
{
	struct	stat sbuf;

	if (stat(s, &sbuf) == -1) return 0;
	return (sbuf.st_size);
}

int
mkexecutable(char *fname)
{
	struct	stat sbuf;

	if (stat(fname, &sbuf)) {
		perror("stat");
		return (-1);
	}
	sbuf.st_mode |= S_IXUSR|S_IXGRP|S_IXOTH;
	return (chmod(fname, UMASK(sbuf.st_mode & 0777)));
}

int
Chmod(char *fname, mode_t mode)
{
	return (chmod(fname, UMASK(mode)));
}

/*
 * Dup up to but not including the newline.
 */
private char	*
strnonldup(char *s)
{
	register char *t = s;
	int	len;

	while (*t++ && (t[-1] != '\n'));
	len = t - s;
	len--;
	t = malloc(len + 1);
	assert(t);
	strncpy(t, s, len);
	t[len] = 0;
	return (t);
}

/*
 * Compare up to but not including the newline.
 * They should be newlines or nulls.
 */
private int
strnonleq(register char *s, register char *t)
{
	while (*s && *t && (*s == *t) && (*s || (*s != '\n'))) s++, t++;
	return ((!*s || (*s == '\n')) && (!*t || (*t == '\n')));
}

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
 * Return the length of the buffer until a newline.
 */
private int
linelen(char *s)
{
	char	*t = s;

	while (*t && (*t++ != '\n'));
	return (t-s);
}

/*
 * Save a line in an array.  If the array is out of space, reallocate it.
 * The size of the array is in array[0].
 */
char	**
addLine(char **space, char *line)
{
	int	i;

	if (!space) {
		space = calloc(32, sizeof(char *));
		assert(space);
		space[0] = (char *)32;
	} else if (space[(int)space[0]-1]) {	/* full up, dude */
		int	size = (int)space[0];
		char	**tmp = calloc(size*2, sizeof(char*));

		assert(tmp);
		bcopy(space, tmp, size*sizeof(char*));
		tmp[0] = (char *)(size * 2);
		free(space);
		space = tmp;
	}
	EACH(space);	/* I want to get to the end */
	assert(i < (int)space[0]);
	assert(space[i] == 0);
	space[i] = line;
	return (space);
}

private void
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
				while ((++i < (int)space[0]) && space[i]) {
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

/* XXX - make this private once tkpatch is part of slib.c */
char	*
getuser(void)
{
	static	char	*s;

	if (s) return (s);
	s = getenv("USER");
	if (!s) {
		s = getlogin();
	}
	if (!s) {
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

/*
 * Free the delta tree.
 */
void
sccs_freetree(delta *tree)
{
	if (!tree) return;

	debug((stderr, "freetree(%s %s %d)\n",
	    tree->rev, tree->sdate, tree->serial));
	sccs_freetree(tree->siblings);
	sccs_freetree(tree->kid);
	if (tree->comments) {
		freeLines(tree->comments);
	}
	if (tree->mr) freeLines(tree->mr);
	if (tree->rev) free(tree->rev);
	if (tree->user) free(tree->user);
	if (tree->sdate) free(tree->sdate);
	if (tree->include) free(tree->include);
	if (tree->exclude) free(tree->exclude);
	if (tree->ignore) free(tree->ignore);
	if (tree->hostname && !(tree->flags & D_DUPHOST)) free(tree->hostname);
	if (tree->pathname && !(tree->flags & D_DUPPATH)) free(tree->pathname);
	if (tree->zone && !(tree->flags & D_DUPZONE)) free(tree->zone);
	if (tree->csetFile && !(tree->flags & D_DUPCSETFILE)) {
		free(tree->csetFile);
	}
	if (tree->cset) free(tree->cset);
	if (tree->sym) free(tree->sym);
	free(tree);
}

/*
 * Generate the rev numbers for the lod.
 * Also sets d->lod to the lod to which this delta belongs.
 */
void
lodrevs(delta *d, delta *zero)
{
	assert(d);
	if (d->flags & D_DUPLOD) return;
	d->flags |= D_DUPLOD;
	unless (d->flags & D_LODHEAD) {
		lodrevs(d->parent, zero);
		memcpy(d->lodr, d->parent->lodr, sizeof(d->lodr));
		d->lodr[0]++;
	} else {
		d->lodr[0] = 1;
		d->lodr[1] = d->lodr[2] = 0;
	}
	/* Head lod pointers are already set, these are the kids */
	unless (d->lod) {
		d->lod = d->parent->lod;
	}
	debug((stderr, "LOD %s.%d\n", d->lod->name, d->lodr[0]));
	free(d->rev);
	if (d->lodr[2]) {
		d->rev = malloc(strlen(d->lod->name) + 20);
		sprintf(d->rev, "%s.%d.%d.%d", 
		    d->lod->name, d->lodr[0], d->lodr[1], d->lodr[2]);
	} else {
		d->rev = malloc(strlen(d->lod->name) + 7);
		sprintf(d->rev, "%s.%d", d->lod->name, d->lodr[0]);
	}
}

/*
 * Set up the LOD numbering.  This is a little complicated because the
 * numbering is inherited but stops when you run into the next LOD. 
 * So the first thing we do is tag all the heads/branches in each LOD.
 */
void
lods(sccs *s)
{
	lod	*l;
	int	i;
	delta	*h;
	ser_t	ser;
	int	top = 0;

	for (l = s->lods; l; l = l->next) {
		debug((stderr, "LODZERO %s %s\n", l->d->rev, l->name));
		EACH(l->heads) {
			h = sfind(s, l->heads[i]);
			assert(h);
			if (i == 1) {
				h->flags |= D_LODHEAD;
				debug((stderr,
				    "HEAD %s %s\n", h->rev, l->name));
			} else {
				h->flags |= D_LODCONT;
				debug((stderr,
				    "CONT %s %s\n", h->rev, l->name));
			}
			h->lod = l;
		}
	}
	for (l = s->lods; l; l = l->next) {
		ser = 0;
		EACH(l->heads) ser = l->heads[i];
		unless (ser) continue;
		h = sfind(s, ser);
		debug((stderr, "HEAD %s %s\n", h->rev, l->name));
		assert(h);
		if (h->serial == 1) {
			top = 1;
		}

		/*
		 * XXX - does not handle LOD branches.
		 * It seems like a recursive 
		 */
		while (h->kid && !(h->kid->flags & (D_LODHEAD|D_LODCONT)) &&
		    samebranch(h, h->kid)) {
		    	h = h->kid;
		}
		lodrevs(h, top ? 0 : l->d);
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
	    samebranch(p, p->kid)) {	/* in right place */
		d->siblings = p->kid->siblings;
		p->kid->siblings = d;
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
sfind(sccs *s, int serial)
{
	delta	*t;

	assert(serial <= s->numdeltas);
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
	s->ser2dsize = s->numdeltas+10;
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
time_t  yearSecs[] = {
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
int monthSecs[] = { 0, 31*DSECS, 59*DSECS, 90*DSECS, 120*DSECS, 151*DSECS,
		    181*DSECS, 212*DSECS, 243*DSECS, 273*DSECS, 304*DSECS,
		    334*DSECS, 365*DSECS };

char days[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

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
 */
private inline int
scandiff(char *s, int *where, char *what)
{
	if (!isdigit(*s)) return (-1);
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
	scanrev(d->rev, &d->r[0], &d->r[1], &d->r[2], &d->r[3]);
}

private char *
sccsrev(delta *d)
{
	static	char buf[MAXREV];

	unless (d->lod) return (d->rev);
	if (d->r[2]) {
		sprintf(buf, "%d.%d.%d.%d", d->r[0], d->r[1], d->r[2], d->r[3]);
	} else {
		sprintf(buf, "%d.%d", d->r[0], d->r[1]);
	}
	return (buf);
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
sameLODbranch(delta *a, delta *b)
{
	if (a->lod != b->lod) return (0);
	if (!a->lodr[1] && !b->lodr[1]) return (1);
	return ((a->lodr[0] == b->lodr[0]) && (a->lodr[1] == b->lodr[1]));
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

private inline int
samefile(char *a, char *b)
{
	struct	stat sa, sb;

	if (stat(a, &sa) == -1) return 0;
	if (stat(b, &sb) == -1) return 0;
	return ((sa.st_dev == sb.st_dev) && (sa.st_ino == sb.st_ino));
}

/*
 * Translate SCCS/s.foo.c to /u/lm/smt/sccs/SCCS/s.foo.c
 * This is weird because I do not trust the gfile.  admin -i/dev/null
 * XXX - this is slow.
 * XXX - I made it faster by looking at $PWD.  This works as long as we
 * only do operations on `pwd`/SCCS.
 */
char	*
fullname(sccs *s, int withsccs)
{
	static	char new[1024];
	char	here[1024];
	char	*t;
	char	*gfile = sccs2name(s->sfile);
	char	*sfile;

	/*
	 * If they have a full path name, then just use that.
	 * It's quicker than calling getcwd.
	 */
	if (IsFullPath(gfile)) {
		if (withsccs) {
			char	*sfile = name2sccs(gfile);

			free(gfile);
			strcpy(new, sfile);
			free(sfile);
			return (new);
		}
		strcpy(new, gfile);
		free(gfile);
		debug((stderr, "fullname1: %s\n", new));
		return (new);
	}

	/*
	 * If we have a relative name and we are where we think
	 * we are, then use that.  Again, quicker.
	 */
	if ((strncmp("SCCS/", s->sfile, 5) == 0) &&
	    (t = getenv("PWD")) && samefile(".", t)) {
		if (withsccs) {
			sfile = name2sccs(gfile);
			sprintf(new, "%s/%s", t, sfile);
			free(sfile);
		} else {
			sprintf(new, "%s/%s", t, gfile);
		}
		free(gfile);
		debug((stderr, "fullname2: %s\n", new));
		return (new);
	}

	getcwd(here, sizeof(here));

	/*
	 * If there is no slash in gfile, it's here.
	 */
	if (!strchr(gfile, '/')) {
		strcpy(new, here);
		strcat(new, "/");
		if (withsccs) {
			sfile = name2sccs(gfile);
			strcat(new, sfile);
			free(sfile);
		} else {
			strcat(new, gfile);
		}
		free(gfile);
		debug((stderr, "fullname3: %s\n", new));
		return (new);
	}

	/*
	 * We have a partial name like foo/bar/blech or
	 * ../fs/ufs/bmap.c
	 */
	t = strrchr(gfile, '/');
	*t = 0;
	if (chdir(gfile)) {
		/* shouldn't happen, if it does, we have bigger problems */
		perror(gfile);
		return (0);
	} else {
		getcwd(new, sizeof(new));
		chdir(here);
	}
	*t = '/';
	if (withsccs) strcat(new, "/SCCS");
	strcat(new, "/");
	strcat(new, basenm(gfile));
	free(gfile);
	debug((stderr, "fullname4: %s\n", new));
	return (new);
}

/*
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * All of this pathname/changeset shit needs to be reworked.
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 */
/*
 * Change directories to the project root or return -1.
 */
int
sccs_cd2root(sccs *s, char *root)
{
	char	*r = sccs_root(s, root);

	if (r && (chdir(r) == 0)) {
		unless (exists(BKROOT)) {
			perror(BKROOT);
			return (-1);
		}
		return (0);
	}
	return (-1);
}

/*
 * Only works for SCCS/s.* layout
 */
char	*
sccs_root(sccs *s, char *root)
{
	static	char buf[1024];
	char	file[1024];
	ino_t	slash = 0;
	struct	stat sb;
	int	i, j;
	char	*t;

	if (s && (s->state & NOSCCSDIR)) return (0);
	if (root) return (root);
	if (s && s->root) return (s->root);

	if (stat("/", &sb)) {
		perror("stat of /");
		return (0);
	}
	slash = sb.st_ino;

	/*
	 * Now work backwards up the tree until we find a BKROOT or /
	 *
	 * Note: this is a little weird because the s->sfile pathname could
	 * be /foo/bar/blech/SCCS/s.file.c
	 * which means we want to start our search in /foo/bar/blech,
	 * not in ".".
	 *
	 * Note: can't use s->gfile, admin stomps on that.
	 */
	for (i = 0; ; i++) {		/* CSTYLED */
		if (s) {
			strcpy(buf, s->sfile);
			t = strrchr(buf, '/');	/* SCCS/s.foo.c */
			*t = 0;
			debug((stderr, "sccs_root %s ", buf));
			if (t = strrchr(buf, '/')) {
				*t = 0;
			} else {
				buf[0] = 0;
			}
		} else {
			buf[0] = 0;
		}
		unless (buf[0]) strcpy(buf, ".");
		for (j = 0; j < i; ++j) strcat(buf, "/..");
		sprintf(file, "%s/%s", buf, BKROOT);
		debug((stderr, "%s\n", file));
		if (exists(file)) {
			unless (buf[0]) strcpy(buf, ".");
			unless (isdir(buf)) return (0);
			if (s) s->root = strdup(buf);
			debug((stderr, "sccs_root() -> %s\n", buf));
			return (s ? s->root : buf);
		}
		if (stat(buf, &sb) == -1) {
			perror(buf);
			return (0);
		}
		if (sb.st_ino == slash) return (0);
	}
	/* NOTREACHED */
}

void
sccs_mkroot(char *path)
{
	char	buf[1024];

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
}

/*
 * Return the id file as a FILE *
 */
FILE	*
idFile(sccs *s)
{
	char	file[1024];
	char	*root;
	
	unless (root = sccs_root(s, 0)) return (0);
	sprintf(file, "%s/SCCS/x.id_cache", root);
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
	char	file[1024];
	char	*root;
	sccs	*sc;
	
	unless (root = sccs_root(s, 0)) return (0);
	sprintf(file, "%s/%s", root, CHANGESET);
	if (exists(file)) {
		sc = sccs_init(file, NOCKSUM);
		assert(sc->tree);
		sccs_sdelta(file, sc->tree);
		sccs_free(sc);
		return (strdup(file));
	}
	return (0);
}

/*
 * Return the pathname relative to the first ChangeSet file found.
 *
 * Note: this only works for SCCS/s.foo style pathnames.
 *
 * XXX - this causes a 5-10% slowdown, more if the tree is very deep.
 * I need to cache changeset lookups.
 */
char	*
_relativeName(sccs *sc, int withsccs, int mustHaveCS)
{
	char	*t, *s, *top;
	int	i, j;
	static	char buf[1024];

	t = fullname(sc, 1);
	strcpy(buf, t);
	top = buf;
	if (buf[0] && buf[1] == ':') top = &buf[2]; /* account for WIN32 path */
	assert(top[0] == '/');
	s = strrchr(buf, '/');
	for (--s; (*s != '/') && (s > top); s--);
	strcpy(++s, BKROOT);
	if (exists(buf)) {
		debug((stderr, "relname1: %s\n", basenm(sc->gfile)));
		return (basenm(sc->gfile));
	}
	for (--s; (*s != '/') && (s > t); s--);

	/*
	 * Now work backwards up the tree until we find a BKROOT
	 * or root (i.e. / or  <driver_letter>: )
	 * XXX - I strongly suspect this needs to be rewritten.
	 */
	for (i = 0; s >= top; i++) {
		debug((stderr, "rname: %s\n", buf));
		if (--s <= top) {
			if (mustHaveCS) return (0);
			debug((stderr, "relname2: %s\n", t));
			return (t);
		}
		/* s -> / in .../foo/SCCS/s.foo.c */
		for (--s; (*s != '/') && (s > top); s--);
		assert(s >= top);
		strcpy(++s, BKROOT);
		unless (exists(buf)) {
			if (streq(top, "/BitKeeper/etc")) {
				strcpy(buf, t);
				debug((stderr, "relname3: %s\n", buf));
				return (buf);
			}
			continue;
		}
		/*
		 * go back in other buffer to this point and copy forwards,
		 * then remove the SCCS/ part.
		 */
		s = strrchr(t, '/');
		for (j = -1; j <= i; ++j) {
			for (--s; (*s != '/') && (s > t); s--);
		}
		strcpy(buf, ++s);
		if (withsccs) {
			debug((stderr, "relname4: %s\n", buf));
			return (buf);
		}
		s = strrchr(buf, '/');
		for (--s; *s != '/'; s--);
		/*
		 * This is weird because of admin -i<whatever>.
		 * We want to take the name from the s.file, that's all
		 * we can trust.
		 */
		strcpy(++s, basenm(sc->sfile) + 2);
		debug((stderr, "relname5: %s\n", buf));
		return (buf);
	}
	assert("relativeName screwed up" == 0);
	return (0);	/* lint */
}

/*
 * Trim off the RESYNC/ part of the pathname, that's garbage.
 */
char	*
relativeName(sccs *sc, int withsccs, int mustHaveCS)
{
	char	*s = _relativeName(sc, withsccs, mustHaveCS);

	unless (s) return (0);

	if (strncmp("RESYNC/", s, 7) == 0) s += 7;
	return (s);
}

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

/*
 * Take an LOD of any form and return the lod * and the numbers.
 */
private inline lod *
lod2rev(sccs *s, char *rev, u16 revs[3])
{
	lod	*l;
	char	*t;
	u16	junk;

	revs[0] = revs[1] = revs[2] = 0;
	if (!rev) return (0);
	if (isdigit(rev[0])) return (0);
	if (t = strchr(rev, '.')) *t++ = 0;

	debug((stderr, "lod2rev(rev=%s t=%s)\n", rev, t));
	for (l = s->lods; l; l = l->next) {
		if (streq(rev, l->name)) break;
	}
	unless (l) {
		if (t) t[-1] = '.';
		return (0);
	}
	if (t && *t) {
		scanrev(t, &revs[0], &revs[1], &revs[2], &junk);
	}
	debug((stderr, "lod2rev(rev=%s t=%s) = %d %d %d\n",
	    rev, t, revs[0], revs[1], revs[2]));
	return (l);
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

private inline delta *
nextlod(delta *d)
{
	delta	*k = d->kid;

	unless (k) return (0);
	for ( ; k; k = k->siblings) {
		if (sameLODbranch(d, k)) return (k);
	}
	return (0);
}

/*
 * This will work if and only if we put all heads of branches in the heads list.
 */
private delta *
lfind(sccs *s, char *rev)
{
	lod	*l;
	delta	*d;
	delta	revs;
	int	i;

	debug((stderr, "lfind(%s, %s)\n", s->gfile, rev));
	bzero(&revs, sizeof(revs));
	unless (l = lod2rev(s, rev, revs.lodr)) return (0);
	revs.lod = l;
	debug((stderr, "lfind got lod %s\n", l->name));
	EACH(l->heads) {
		d = sfind(s, l->heads[i]);
		debug((stderr, "lfind search %s\n", d->rev));
		unless (sameLODbranch(d, &revs)) continue;
		while (d) {
			debug((stderr, "lfind search2 %s\n", d->rev));
			if ((d->lodr[0] == revs.lodr[0]) &&
			    (d->lodr[1] == revs.lodr[1]) &&
			    (d->lodr[2] == revs.lodr[2])) {
				return (d);
			}
			d = nextlod(d);
		}
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
	if (d = lfind(s, rev)) return (d);
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
	if (s->tree->lod) {
		return (s->tree->lod->name);
	} else {
		return ("100000");
	}
}

/*
 * Find the specified delta.  The rev must be
 *	LOD		get top of trunk for that LOD
 *	LOD.d		get that revision or error if not there
 *	LOD.d.d		get top of branch that matches those three digits
 *	LOD.d.d.d	get that revision or error if not there.
 */
delta *
findlod(sccs *s, char *rev)
{
	delta	*d, *e;
	u16	revs[3];
	lod	*l;
	int	n;

	if (!s->tree) return (0);
	if (!rev || !*rev) rev = defbranch(s);
	if (isdigit(rev[0])) return (0);
	unless (l = lod2rev(s, rev, revs)) return (0);
	n = 0;
	if (revs[0]) n++;
	if (revs[1]) n++;
	if (revs[2]) n++;
	debug((stderr, "findlod: %s n=%d\n", l->name, n));
	unless (l->heads) return (n == 0 ? l->d : 0);
	if (n > 1) assert("LOD BRANCHES NOT DONE" == 0);
	switch (n) {
	    case 0:	/* LOD */
		for (d = sfind(s, l->heads[1]); e = nextlod(d); d = e);
		debug((stderr, "findlod(%s) = %s\n", rev, d->rev));
		return (d ? d : l->d);
	    case 1:	/* LOD.3 */
		debug((stderr, "findlod: %s wants %d\n", l->name, revs[0]));
		for (d = sfind(s, l->heads[1]); d; d = nextlod(d)) {
			debug((stderr, "%s %d\n", d->rev, d->lodr[0]));
			if (d->lodr[0] == revs[0]) {
				debug((stderr, "findlod(%s) =  %s\n",
				    rev, d->rev));
				return (d);
			}
		}
		return (0);
	}
	return (0);	/* NOTREACHED: gcc -Wall */
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
	delta	*e;
	char	buf[20];
	lod	*l;

	debug((stderr,
	    "findrev(%s in %s def=%s)\n", rev, s->sfile, defbranch(s)));
	if (!s->tree) return (0);
	if (!rev || !*rev) rev = defbranch(s);

	if (e = findlod(s, rev)) return (e);
	if (name2rev(s, &rev)) return (0);
	switch (scanrev(rev, &a, &b, &c, &d)) {
	    case 1:
		for (e = s->tree, l = s->tree->lod;
		    e->kid &&
		    (l == e->kid->lod) &&
		    (e->kid->type == 'D') &&
		    (e->kid->r[2] == 0) &&
		    (e->kid->r[0] <= a);
		    e = e->kid)
			;
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
	time_t	date, d2;
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
		unless (sc->state & RANGE2) {	/* this is first call */
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

	unless (sc->state & RANGE2) {	/* first call */
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
		if (d->date == date) return (d);
		/*
		 *                v date
		 * big date   1.4   1.3   1.2    little date
		 *             ^tmp  ^d
		 */
		if (d->date < date) {
			unless (sc->state & RANGE2) {	/* first call */
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
morekids(delta *d)
{
	return (d->kid && (d->kid->type != 'R') && samebranch(d, d->kid));
}

/*
 * Get the delta that is the basis for this edit.
 * Get the revision name of the new delta.
 */
private delta *
getedit(sccs *s, char **revp, char **lrev, int branch)
{
	char	*rev = *revp;
	u16	a = 0, b = 0, c = 0, d = 0;
	delta	*e, *t;
	lod	*l = 0;
	static	char buf[MAXREV];
	static	char lbuf[MAXREV];

	debug((stderr, "getedit(%s, %s, b=%d)\n", s->gfile, *revp, branch));
	/*
	 * use the findrev logic to get to the delta.
	 */
	unless (e = findrev(s, rev)) {
		/*
		 * If that didn't work, see if we are starting a new LOD.
		 */
		if (!rev || !*rev) rev = defbranch(s);
		unless (isdigit(rev[0])) {
			debug((stderr, "Look for %s\n", rev));
			for (l = s->lods; l; l = l->next) {
				if (streq(rev, l->name)) break;
			}
			if (l) {
				debug((stderr, "Found LOD %s\n", l->name));
				e = l->d;
			}
		}
	}
	debug((stderr, "getedit(e=%s, l=%s)\n", e?e->rev:"", l?l->name:""));

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
	if (!branch && !morekids(e)) {
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
		debug((stderr, "getedit1(%s) -> %s\n", rev, buf));
		*revp = buf;
		goto lod;
	}

	/*
	 * For whatever reason (they asked, or there is a kid in the way),
	 * we need a branch.
	 * Branches are all based, in their /name/, off of the closest
	 * trunk node.	Go backwards up the tree until we hit the trunk
	 * and then use that rev as a basis.
	 * Because all branches are below that trunk node, we don't have
	 * to search the whole tree.
	 */
	for (t = e; isbranch(t); t = t->parent);
	R[0] = t->r[0]; R[1] = t->r[1]; R[2] = 1; R[3] = 1;
	while (_rfind(t)) R[2]++;
	sprintf(buf, "%d.%d.%d.%d", R[0], R[1], R[2], R[3]);
	debug((stderr, "getedit2(%s) -> %s\n", rev, buf));
	*revp = buf;

lod:
	/*
	 * We use different logic for LOD's than the other stuff.
	 */
	if (l) {	/* this is the first node in the LOD */
		assert(!branch);
		/* XXXXXXXXXXX */
		assert(sizeof(lbuf) > (strlen(l->name) + 5));
		sprintf(lbuf, "%s.1", l->name);
		debug((stderr, "getedit(%s) -> %s & %s\n", rev, buf, lbuf));
		*lrev = lbuf;
		return (l->d);
	}
	
	if (e->lod) {
		/* XXXXXXXXXXX */
		assert(sizeof(buf) > (strlen(e->rev) + 5));
		sprintf(lbuf, "%s.%d", e->lod->name, e->lodr[0]+1);
		debug((stderr, "getedit(%s) -> %s & %s\n", rev, buf, lbuf));
		*lrev = lbuf;
		return (e);
		//XXX - doesn't handle branches
	}
	return (e);
}

#ifndef	USE_STDIO
private char	*
fastnext(sccs *s)
{
	register char *t = s->where;
	register char *tmp = s->mmap + s->size;

	if (s->where >= tmp) return (0);
	/* I tried unrolling this a couple of ways and it got worse */
	while (t < tmp && *t++ != '\n');
	tmp = s->where;
	s->where = t;
	return (tmp);
}
#endif

/*
 * This does standard SCCS expansion, it's almost 100% here.
 * New stuff added:
 * %@%	user@host
 */
private char *
expand(sccs *s, delta *d, char *l)
{
	static	char buf[1024];
	char	*t = buf;
	char	*tmp;
	time_t	now = 0;
	struct	tm *tm;
	u16	a[4];

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
			break;

		    case 'B':	/* branch name: XXX */
			tmp = branchname(d); strcpy(t, tmp); t += strlen(tmp);
			break;

		    case 'C':	/* line number - XXX */
			*t++ = '%'; *t++ = 'C'; *t++ = '%'; break;

		    case 'D':	/* today: 97/06/22 */
			if (!now) { time(&now); tm = localtime(&now); }
			assert(tm);
			if (s->state & YEAR4) {
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
			break;

		    case 'E':	/* most recent delta: 97/06/22 */
			if (s->state & YEAR4) {
				if (atoi(d->sdate) > 69) {
					*t++ = '1'; *t++ = '9';
				} else {
					*t++ = '2'; *t++ = '0';
				}
			}
			strncpy(t, d->sdate, 8); t += 8;
			break;

		    case 'F':	/* s.file name */
			strcpy(t, "SCCS/"); t += 5;
			tmp = basenm(s->sfile);
			strcpy(t, tmp); t += strlen(tmp);
			break;

		    case 'G':	/* most recent delta: 06/22/97 */
			*t++ = d->sdate[3]; *t++ = d->sdate[4]; *t++ = '/';
			*t++ = d->sdate[6]; *t++ = d->sdate[7]; *t++ = '/';
			if (s->state & YEAR4) {
				if (atoi(d->sdate) > 69) {
					*t++ = '1'; *t++ = '9';
				} else {
					*t++ = '2'; *t++ = '0';
				}
			}
			*t++ = d->sdate[0]; *t++ = d->sdate[1];
			break;

		    case 'H':	/* today: 06/22/97 */
			if (!now) { time(&now); tm = localtime(&now); }
			assert(tm);
			if (s->state & YEAR4) {
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
			break;

		    case 'I':	/* name of revision: 1.1 or 1.1.1.1 */
			strcpy(t, d->rev); t += strlen(d->rev);
			break;

		    case 'L':	/* 1.2.3.4 -> 2 */
			scanrev(d->rev, &a[0], &a[1], 0, 0);
			sprintf(t, "%d", a[1]); t += strlen(t);
			break;

		    case 'M':	/* mflag or filename: slib.c */
			tmp = basenm(s->gfile);
			strcpy(t, tmp); t += strlen(tmp);
			break;

		    case 'P':	/* full: /u/lm/smt/sccs/SCCS/s.slib.c */
			tmp = fullname(s, 1);
			strcpy(t, tmp); t += strlen(tmp);
			break;

		    case 'Q':	/* qflag */
			*t++ = '%'; *t++ = 'Q'; *t++ = '%'; break;

		    case 'R':	/* release 1.2.3.4 -> 1 */
			scanrev(d->rev, &a[0], 0, 0, 0);
			sprintf(t, "%d", a[0]); t += strlen(t);
			break;

		    case 'S':	/* rev number: 1.2.3.4 -> 4 */
			a[3] = 0;
			scanrev(d->rev, &a[0], &a[1], &a[2], &a[3]);
			sprintf(t, "%d", a[3]); t += strlen(t);
			break;

		    case 'T':	/* time: 23:04:04 */
			if (!now) { time(&now); tm = localtime(&now); }
			assert(tm);
			sprintf(t, "%02d:%02d:%02d",
			    tm->tm_hour, tm->tm_min, tm->tm_sec);
			t += 8;
			break;

		    case 'U':	/* newest delta: 23:04:04 */
			strcpy(t, &d->sdate[9]); t += 8;
			break;

		    case 'W':	/* @(#)%M% %I%: @(#)slib.c 1.1 */
			strcpy(t, "@(#) "); t += 4;
			tmp = basenm(s->gfile);
			strcpy(t, tmp); t += strlen(tmp); *t++ = ' ';
			strcpy(t, d->rev); t += strlen(d->rev);
			break;

		    case 'Y':	/* tflag */
			*t++ = '%'; *t++ = 'Y'; *t++ = '%'; break;

		    case 'Z':	/* @(#) */
			strcpy(t, "@(#)"); t += 4; break;

		    case '@':	/* user@host */
			strcpy(t, d->user);
			t += strlen(d->user);
			if (d->hostname) {
				*t++ = '@';
				strcpy(t, d->hostname);
				t += strlen(d->hostname);
			}
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
rcsexpand(sccs *s, delta *d, char *l)
{
	static	char buf[1024];
	char	*t = buf;
	char	*tmp;
	delta	*h;

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
		} else if (strneq("$Locker$", l, 8)) {
			strcpy(t, "$Locker: <Not implemented> $"); t += 28;
			l += 8;
		} else if (strneq("$Log$", l, 5)) {
			strcpy(t, "$Log: <Not implemented> $"); t += 25;
			l += 5;
		} else if (strneq("$Name$", l, 6)) {
			strcpy(t, "$Name: <Not implemented> $"); t += 26;
			l += 6;
		} else if (strneq("$RCSfile$", l, 9)) {
			strcpy(t, "$RCSfile: "); t += 10;
			tmp = basenm(s->sfile);
			strcpy(t, tmp); t += strlen(tmp);
			*t++ = ' '; *t++ = '$';
			l += 9;
		} else if (strneq("$Revision$", l, 10)) {
			strcpy(t, "$Revision: "); t += 11;
			strcpy(t, d->rev); t += strlen(d->rev);
			*t++ = ' ';
			*t++ = '$';
			l += 10;
		} else if (strneq("$Source$", l, 8)) {
			strcpy(t, "$Source: "); t += 9;
			tmp = fullname(s, 1);
			strcpy(t, tmp); t += strlen(tmp);
			*t++ = ' '; *t++ = '$';
			l += 8;
		} else if (strneq("$State$", l, 7)) {
			strcpy(t, "$State: "); t += 8;
			*t++ = ' ';
			strcpy(t, "<unknown>");
			t += 9;
			*t++ = ' '; *t++ = '$';
			l += 7;
		} else {
			*t++ = *l++;
		}
	}
	*t++ = '\n'; *t = 0;
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
		csetArg(d, &buf[3]);
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
	    case 'L':
		lodArg(s, d, &buf[3]);
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
	    case 'S':
		symArg(s, d, &buf[3]);
		break;
	    case 'Z':
		zoneArg(d, &buf[3]);
		break;
	    default:
		fprintf(stderr, "Ignoring %.5s...\n", buf);
		/* got unknown field, force read only mode */
		s->state |= READ_ONLY;
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
	char	*p;
	char	tmp[100];
	int	i;
	int	line = 1;
	char	*expected = "?";
	BUF(	buf);			/* CSTYLED */

	seekto(s, 0);
	next(buf, s);			/* checksum */
	line++;
	debug((stderr, "mkgraph(%s)\n", s->sfile));
	for (;;) {
nextdelta:	unless (next(buf, s)) {
bad:
			fprintf(stderr,
			    "%s: bad delta on line %d, expected `%s', "
			    "line follows:\n\t",
			    s->sfile, line, expected);
			fprintf(stderr, "``%.*s''\n", linelen(buf)-1, buf);
			sccs_freetree(s->table);
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
		d->added = atoi(&buf[3]);
		d->deleted = atoi(&buf[9]);
		d->same = atoi(&buf[15]);
		/* ^Ad D 1.2.1.1 97/05/15 23:11:46 lm 4 2 */
		/* ^Ad R 1.2.1.1 97/05/15 23:11:46 lm 4 2 */
		next(buf, s);
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
			if (!next(buf, s) || buf[0] != '\001') {
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
			if (!next(buf, s) || buf[0] != '\001') {
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

		assert(d->serial <= s->numdeltas);
		d->kid = 0;
		dinsert(s, flags, d);
		d = therest;
	}
	if (checkrevs(s, flags) & 1) s->state |= BADREVS;

	/*
	 * For all the metadata nodes, go through and propogate the data up to
	 * the real node.
	 */
	metaSyms(s);
	
	/*
	 * Go build the lod numbering.
	 */
	lods(s);

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
	BUF(	buf);	/* CSTYLED */

	/* Save the users / groups list */
	for (; next(buf, s) && !strneq(buf, "\001U\n", 3); ) {
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
	for (; next(buf, s) && !strneq(buf, "\001t\n", 3); ) {
		if (strneq(buf, "\001f R\n", 5)) {	/* XXX - obsolete */
			s->state |= RCS;
			continue;
		} else if (strneq(buf, "\001f b\n", 5)) {
			s->state |= BRANCHOK;
			continue;
		} else if (strneq(buf, "\001f Y\n", 5)) { /* XXX - obsolete */
			s->state |= YEAR4;
			continue;
		} else if (strneq(buf, "\001f x ", 5)) {
			int	bits = atoi(&buf[5]);

			if (bits & X_BITKEEPER) s->state |= BITKEEPER;
			if (bits & X_YEAR4) s->state |= YEAR4;
			if (bits & X_RCSEXPAND) s->state |= RCS;
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

	if (s->state & REINHERIT) reinherit(s, s->tree);

	/* Save descriptive text. */
	for (; next(buf, s) && !strneq(buf, "\001T\n", 3); ) {
		s->text = addLine(s->text, strnonldup(buf));
	}
	s->data = tell(s);
	return (0);
}

/*
 * Reads until a space without a preceeding \.
 * XXX - counts on s[-1] being a valid address.
 */
private char *
gettok(char **sp)
{
	static char	copy[1024];
	char	*t;
	char	*s = *sp;

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
		s->state |= REINHERIT;
		hostArg(d, t);
		return (1);
	    case 'p':	/* pathname */
		s->state |= REINHERIT;
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
		s->state |= REINHERIT;
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
addsym(sccs *s, delta *d, delta *reald, char *rev, char *val)
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
	sym->reald = reald;
	d->flags |= D_SYMBOLS;
	reald->flags |= D_SYMBOLS;
	if (!d->date) getDate(d);
	assert(d->date);

	/*
	 * Insert in sorted order, most recent first.
	 */
	if (!s->symbols || !s->symbols->d) {
		sym->next = s->symbols;
		s->symbols = sym;
	} else if (d->date >= s->symbols->d->date) {
		sym->next = s->symbols;
		s->symbols = sym;
	} else {
		for (s3 = 0, s2 = s->symbols; s2; s3 = s2, s2 = s2->next) {
			if (!s2->d || (d->date >= s2->d->date)) {
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
	debug((stderr, "Added symbol %s->%s in %s\n", val, rev, s->sfile));
	return (1);
}

sccs *
check_gfile(sccs *s, int flags)
{
	struct	stat sbuf;

	if (stat(s->gfile, &sbuf) == 0) {
		if (!S_ISREG(sbuf.st_mode)) {
			verbose((stderr, "Not a regular file: %s\n", s->gfile));
			free(s->gfile);
			free(s->sfile);
			free(s);
			return (0);
		}
		s->state |= GFILE;
		s->mode = sbuf.st_mode & 0777;
		if (flags & GTIME) s->gtime = sbuf.st_mtime;
	} else {
		s->state &= ~GFILE;
	}
	return (s);
}

/*
 * Initialize an SCCS file.  Do this before anything else.
 * If the file doesn't exist, the graph isn't set up.
 * It should be OK to have multiple files open at once.
 */
sccs*
sccs_init(char *name, int flags)
{
	sccs	*s = calloc(1, sizeof(*s));
	struct	stat sbuf;
	char	*t;

	/* this should just go away and be the default */
	assert(s);
	platformSpecificInit(name, flags); 
	if (u_mask == 0x5eadbeef) {
		u_mask = ~umask(0);
		umask(~u_mask);
	}
	if (is_sccs(name)) {
		s->sfile = strdup(name);
		s->gfile = sccs2name(name);
	} else {
		fprintf(stderr, "Not an SCCS file: %s\n", name);
		free(s);
		return (0);
	}
	t = strrchr(s->sfile, '/');
	if (t) {
		if (streq(t, "/s.ChangeSet")) s->state |= CSET;
	} else {
		if (streq(s->sfile, "SCCS/s.ChangeSet")) s->state |= CSET;
	}
	unless (t && (t >= s->sfile + 4) && strneq(t - 4, "SCCS/s.", 7)) {
		s->state |= NOSCCSDIR;
	}
	if (t = getenv("BK_LOD")) s->defbranch = strdup(t);
	unless (check_gfile(s, flags)) return (0);
	if (stat(s->sfile, &sbuf) == 0) {
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
		s->state |= SFILE;
#ifndef	USE_STDIO
		s->size = sbuf.st_size;
#endif
	}
	s->pfile = strdup(sccsXfile(s, 'p'));
	s->zfile = strdup(sccsXfile(s, 'z'));
	if (isreg(s->pfile)) s->state |= PFILE;
	if (isreg(s->zfile)) s->state |= ZFILE;
	debug((stderr, "init(%s) -> %s, %s\n", name, s->sfile, s->gfile));
	s->nextserial = 1;
#ifdef	USE_STDIO
	s->file = fopen(s->sfile, "r");
	if (!s->file) {
		s->cksumok = 1;
		return (s);
	}
#else
	if (flags & MAP_WRITE) {
		sbuf.st_mode |= 0200;
		chmod(s->sfile, UMASK(sbuf.st_mode & 0777));
		s->fd = open(s->sfile, 2, 0);
		s->mmap = (char *) -1;
		if (s->fd >= 0) {
			s->mmap = mmap(0,
			    s->size, PROT_READ|PROT_WRITE,
			    MAP_SHARED, s->fd, 0);
			if ((int)s->mmap == -1){
				/*
				 * MAP_SHARED not supported on SAMBA
				 * file system yet
				 * So we have to use MAP_PIVATE for now
				 */
				debug((stderr,
				    "MAP_SHARED failed, trying MAP_PRIVATE\n"));
				s->mmap = mmap(0,
				    s->size, PROT_READ|PROT_WRITE,
				    MAP_PRIVATE, s->fd, 0);
				s->state |= MAPPRIVATE;
			}
		}
		s->state |= CHMOD;
	} else {
		s->fd = open(s->sfile, 0, 0);
		s->mmap = mmap(0, s->size, PROT_READ, MAP_SHARED, s->fd, 0);
	}
	if ((int)s->mmap == -1) {
		s->cksumok = 1;
		return (s);
	}
	debug((stderr, "mapped %s for %d at 0x%x\n",
	    s->sfile, s->size, s->mmap));
#endif
	s->state |= SOPEN;
	if (((flags&NOCKSUM) == 0) && badcksum(s)) {
		fprintf(stderr, "Bad checksum for %s\n", s->sfile);
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
	if (stat(s->gfile, &sbuf) == 0) {
		if (!S_ISREG(sbuf.st_mode)) {
bad:			sccs_free(s);
			return (0);
		}
		s->state |= GFILE;
		s->mode = sbuf.st_mode & 0777;
	}
	if (stat(s->sfile, &sbuf) == 0) {
		if (!S_ISREG(sbuf.st_mode)) goto bad;
		if (sbuf.st_size == 0) goto bad;
		s->state |= SFILE;
	}
#ifndef	USE_STDIO
	if ((s->size != sbuf.st_size) || (s->fd == -1)) {
		BUF(	buf);	/* CSTYLED */

		if (s->fd == -1) {
			s->fd = open(s->sfile, 0, 0);
		}
		if (s->mmap != (caddr_t)-1L) munmap(s->mmap, s->size);
		s->size = sbuf.st_size;
		s->mmap = mmap(0, s->size, PROT_READ, MAP_SHARED, s->fd, 0);
		if (s->mmap != (caddr_t)-1L) s->state |= SOPEN;
		seekto(s, 0);
		for (; next(buf, s) && !strneq(buf, "\001T\n", 3); );
		s->data = tell(s);
	}
#endif
	if (isreg(s->pfile)) s->state |= PFILE;
	if (isreg(s->zfile)) s->state |= ZFILE;
	return (s);
}

/*
 * close all open file stuff associated with an sccs structure.
 */
void
sccs_close(sccs *s)
{
	munmap(s->mmap, s->size);
	close(s->fd);
	s->mmap = (caddr_t) -1;
	s->fd = -1;
	s->state &= ~SOPEN;
}

/*
 * Free up all resources associated with the file.
 * This is the last thing you do.
 */
void
sccs_free(sccs *s)
{
	symbol	*sym, *t;
	lod	*l, *l2;

	assert(s);
	assert(s->sfile);
	assert(s->gfile);
	sccsXfile(s, 0);
	if (s->tree) sccs_freetree(s->tree);
	for (sym = s->symbols; sym; sym = t) {
		t = sym->next;
		if (sym->name) free(sym->name);
		if (sym->rev) free(sym->rev);
		free(sym);
	}
	free(s->sfile);
	free(s->gfile);
	free(s->zfile);
	free(s->pfile);
#ifdef	USE_STDIO
	if (s->file) fclose(s->file);
#else
	if (s->mmap != (caddr_t)-1L) munmap(s->mmap, s->size);
	if (s->state & CHMOD) {
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
#endif
	close(s->fd);
	if (s->defbranch) free(s->defbranch);
	if (s->ser2delta) free(s->ser2delta);
	freeLines(s->usersgroups);
	freeLines(s->flags);
	freeLines(s->text);
	for (l = s->lods; l; l = l2) {
		l2 = l->next;
		free(l->name);
		free(l);
	}
	if (s->root) free(s->root);
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
 * Since we do a pretty good job of putting stuff in SCCS/s.*, we skip
 * that check here.
 */
int
is_sccs(char *name)
{
	char	*s = rindex(name, '/');

	if (!s) {
		if (name[0] == 's' && name[1] == '.') return (1);
		return (0);
	}
	if (s[1] == 's' && s[2] == '.') return (1);
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
mksccsdir(sccs *sc)
{
	char	*s = rindex(sc->sfile, '/');

	if (!s) return;
	if ((s >= sc->sfile + 4) &&
	    s[-1] == 'S' && s[-2] == 'C' && s[-3] == 'C' && s[-4] == 'S') {
		*s = 0;
		mkdir(sc->sfile, 0775);
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
		if ((s[1] != 's') && (s[2] == '.')) {
			switch (s[1]) {
			    case 'p':
			    case 'r':
			    case 'x':
			    case 'z':
			    	break;
			    default:
				assert(name == "Bad name");
			}
			name = strdup(name);
			s = strrchr(name, '/');
			s[1] = 's';
			return (name);
		} else {
			return (strdup(name));
		}
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
private int
lock(sccs *sccs, char type)
{
	char	*s;
	int	islock;

	if ((type == 'z') && (sccs->state & READ_ONLY)) return (0);
	s = sccsXfile(sccs, type);
	islock = open(s, O_CREAT|O_WRONLY|O_EXCL, type == 'z' ? 0444 : 0644);
	close(islock);
	if (islock > 0) sccs->state |= Z_LOCKED;
	debug((stderr, "lock(%s) = %d\n", sccs->sfile, islock > 0));
	return (islock > 0);
}

/*
 * Take SCCS/s.foo.c and unlink SCCS/<type>.foo.c
 */
private int
unlock(sccs *sccs, char type)
{
	char	*s;
	int	failed;

	debug((stderr, "unlock(%s, %c)\n", sccs->sfile, type));
	s = sccsXfile(sccs, type);
	failed  = unlink(s);
	unless (failed) sccs->state &= ~Z_LOCKED;
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
	} else if (len < strlen(sccs->sfile) + 3) {
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
	struct	tm *tm;
	char	tmp[50];
	extern	long timezone;
	extern	int daylight;
	int	hwest, mwest;
	char	sign = '-';

	// XXX - fix this before release 1.0 - make it be 4 digits
	tm = localtime(&tt);
	strftime(tmp, sizeof(tmp), "%y/%m/%d %H:%M:%S", tm);
	d->sdate = strdup(tmp);
	strftime(tmp, sizeof(tmp), "%z", tm);
	if (strlen(tmp) == 5) {
		tmp[6] = 0;
		tmp[5] = tmp[4];
		tmp[4] = tmp[3];
		tmp[3] = ':';
	} else {
		/*
		 * What I want is to have 8 hours west of GMT to be -08:00.
		 */
		hwest = timezone / 3600;
		mwest = timezone % 3600;
		if (hwest < 0) {
			sign = '+';
			hwest = -hwest;
			mwest = -mwest;
		}
		/*
		 * XXX - I have not thought this through.
		 * This is blindly following what /bin/date does.
		 */
		if (daylight) hwest--;
		sprintf(tmp, "%c%02d:%02d", sign, hwest, mwest);
	}
	zoneArg(d, tmp);
	getDate(d);
}

/*
 * Return an at most 5 digit !0 integer.
 */
long
almostUnique()
{
	struct	timeval tv;
	int	max = 100;
	int	val;

	do {
		gettimeofday(&tv, NULL);
		val = tv.tv_usec % 100000;
	} while (max-- && !val);
	while (!val) val = time(0) % 100000;
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

/* XXX - takes 100 usecs in a hot cache */
char	*
sccs_gethost(void)
{
	static char host[257];
	static	done = 0;
	struct	hostent *hp;
	char 	*h;

	if (done) return (host[0] ? host : 0);
	done = 1;

	if (h = getenv("BK_HOST")) {
		assert(strlen(h) <= 256);
		strcpy(host, h);
		return(host);
	}	
	/*
	 * Some system (e.g. win32)
	 * reuires loading a library
	 * before we call gethostbyname()
	 */
	loadNetLib();
	if (gethostname(host, sizeof(host)) == -1) {
		unLoadNetLib();
		return (0);
	}
	unless (hp = gethostbyname(host)) {
		unLoadNetLib();
		return (0);
	}
	unLoadNetLib();
	unless (hp->h_name) goto out;
	unless (strchr(hp->h_name, '.')) {
		int	i;

		for (i = 0; hp->h_aliases && hp->h_aliases[i]; ++i) {
			if (strchr(hp->h_aliases[i], '.')) {
				strcpy(host, hp->h_aliases[i]);
				break;
			}
		}
	} else if (hp) strcpy(host, hp->h_name);
out:	if (streq(host, "localhost") || streq(host, "localhost.localdomain")) {
		host[0] = 0;
		return (0);
	}
	return (host);
}

/*
 * Save a serial in an array.  If the array is out of space, reallocate it.
 * The size of the array is in array[0].
 */
ser_t *
addSerial(ser_t *space, ser_t s)
{
	int	i;

	if (!space) {
		space = calloc(16, sizeof(ser_t));
		assert(space);
		space[0] = (ser_t)16;
	} else if (space[(int)space[0]-1]) {	/* full up, dude */
		int	size = (int)space[0];
		ser_t	*tmp = calloc(size*2, sizeof(ser_t));

		assert(tmp);
		bcopy(space, tmp, size*sizeof(ser_t));
		tmp[0] = (ser_t)(size * 2);
		free(space);
		space = tmp;
	}
	EACH(space);
	assert(i < (int)space[0]);
	assert(space[i] == 0);
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
			l = addSerial(l, atoi(s));
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
	/* This is not EACH because I want to go backwards */
	for (i = (int)s[0] - 1; i > 0; i--) {
		if (s[i]) {
			sertoa(buf, s[i]);
			if (!first) fputsum(sc, " ", out);
			fputsum(sc, buf, out);
			first = 0;
		}
	}
}

private void
putlodlist(sccs *sc, ser_t *s, FILE *out)
{
	int	first = 1, i;
	char	buf[20];

	if (!s) return;
	EACH(s) {
		sertoa(buf, s[i]);
		if (!first) fputsum(sc, " ", out);
		fputsum(sc, buf, out);
		first = 0;
	}
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
			assert(t->serial <= s->numdeltas);
			slist[t->serial] = S_INC;
 		}
		verbose((stderr, "\n"));
		if (*errp) goto bad;
	}

	if (xLst) {
		verbose((stderr, "Excluded:"));
		for (t = walkList(s, xLst, errp);
		    !*errp && t; t = walkList(s, 0, errp)) {
			assert(t->serial <= s->numdeltas);
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

 		assert(t->serial <= s->numdeltas);
		
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
	register serlist *s;
	register serlist *n;
	int	i;

	debug2((stderr, "chg(%c, %d)\n", type, serial));
	/*
	 * Find the item and delete it if it is an 'E'.
	 */
	if (type == 'E') {	/* free this item */

		for (i = 0, s = state[SLIST].next; s; s = s->next, i++) {
			if (s->serial == serial) break;
		}
		assert(s && (s->serial == serial));
		if (s->prev) {
			s->prev->next = s->next;
		} else {
			state[SLIST].next = s->next;
		}
		if (s->next) {
			s->next->prev = s->prev;
		}
		s->next = state[SFREE].next;
		state[SFREE].next = s;
		return;
	}
	if (!state[SFREE].serial) {
		assert("Ran out of serial numbers" == 0);
	}
	n = state[SFREE].next;
	state[SFREE].next = n->next;
	for (i = 0, s = state[SLIST].next; s; s = s->next, i++) {
		if ((s->serial < serial) || !s->next) break;
	}
	/*
	 * We're either at the head of the (empty) list,
	 * at the right place (insert before s), or
	 * or we're at the end of the list (insert after).
	 */
	if (!s) {
		state[SLIST].next = n;
		n->prev = 0;
		n ->next = 0;
	} else if (s->serial < serial) {
		n->next = s;
		if (s->prev) {
			s->prev->next = n;
		} else {
			state[SLIST].next = n;
		}
		n->prev = s->prev;
		s->prev = n;
	} else {
		assert(!s->next);
		n->next = 0;
		n->prev = s;
		s->next = n;
	}
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
		s[i].prev = 0;
	}
	s[SFREE].next = &s[2];
	s[SFREE].serial = n-2;
	s[SLIST].next = 0;
	s[SLIST].serial = 0;
	return (s);
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
	char	fbuf[1024];
	register char *p = fbuf;

	do {
		*p++ = *t;
		if (p == &fbuf[1023]) {
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

private sum_t
fputsum(sccs *s, char *buf, FILE *out)
{
	register char	*t = buf;
	register sum_t	 sum = 0;
	char	fbuf[1024];
	register char *p = fbuf;

	for (; *t; t++) {
		sum += *t;
		*p++ = *t;
		if (p == &fbuf[1023]) {
			*p = 0;
			p = fbuf;
			fputs(fbuf, out);
		}
		if (*t == '\n') break;
	}
	s->cksum += sum;
	if (p != fbuf) {
		*p = 0;
		fputs(fbuf, out);
	}
	return (sum);
}

//typedef	unsigned char uchar;
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
	register uchar *p;
	register int n;
	register int length;
	int	added = 0;

	while ((length = fread(ibuf, 1, 450, in)) > 0) {
		p = ibuf;
		while (length > 0) {
			n = (length > 45) ? 45 : length;
			length -= n;
			uuencode1(p, obuf, n);
			fputsum(s, obuf, out);
			p += n;
			added++;
		}
	}
	fputsum(s, " \n", out);
	return (++added);
}

int
uuencode(FILE *in, FILE *out)
{
	uchar	ibuf[450];
	char	obuf[650];
	register uchar *buf;
	register char *p = obuf;
	register int n;
	register int length;
	int	added = 0;

	while ((length = fread(ibuf, 1, 450, in)) > 0) {
		p = obuf;
		buf = ibuf;
		while (length > 0) {
			n = (length > 45) ? 45 : length;
			length -= n;
			p += uuencode1(buf, p, n);
			added++;
			buf += n;
		}
		*p = 0;
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
	switch (encode) {
	    case E_ASCII:
	    case E_UUENCODE:
		*op = toStdout ? stdout : fopen(file, "w");
		break;
	    case E_UUGZIP:
		if (toStdout) {
			*op = popen("gzip -d", "wb");
		} else {
			sprintf(buf, "gzip -d > %s", file);
			*op = popen(buf, "wb");
		}
		break;
	    default:
		*op = NULL;
		return (-1);
	}
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
err:		s->state |= WARNED;
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
	char *rev, char *lrev, char *iLst, char *i2, char *xLst, char *mRev)
{
	int	fd, len;
	char	*tmp, *tmp2;

	if (IS_WRITABLE(s) && !(flags & SKIPGET)) {
		fprintf(stderr,
		    "Writeable %s exists, skipping it.\n", s->gfile);
		s->state |= WARNED;
		return (-1);
	}
	if (!lock(s, 'z')) {
		fprintf(stderr, "get: can't zlock %s\n", s->gfile);
		return (-1);
	}
	if (!lock(s, 'p')) {
		fprintf(stderr, "get: can't plock %s\n", s->gfile);
		unlock(s, 'z');
		return (-1);
	}
	fd = open(s->pfile, 2, 0);
	tmp2 = now();
	assert(getuser() != 0);
	len = strlen(d->rev) 
	    + MAXREV + 2
	    + strlen(rev)
	    + strlen(getuser())
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
	if (lrev) {
		sprintf(tmp, "%s %s(%s) %s %s",
			    d->rev, lrev, rev, getuser(), tmp2);
	} else {
		sprintf(tmp,
		    "%s %s %s %s", d->rev, rev, getuser(), tmp2);
	}
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
	return (0);
}

/*
 * get the specified revision.
 * The output file is passed in so that callers can redirect it.
 * iLst and xLst are malloced and get() frees them.
 */
int
sccs_get(sccs *s, char *rev,
	char *mRev, char *iLst, char *xLst, int flags, char *printOut)
{
	serlist *state = 0;
	ser_t	*slist = 0;
	delta	*d;
	int	print = 0;
	int	lines = 0;
	FILE	*out = 0;
	int	popened = 0;
	int	encoding = (flags&FORCEASCII) ? E_ASCII : s->encoding;
	int	error = 0;
	char	*lrev = 0, *tmp, *base, *i2 = 0;
	sum_t	sum;
	BUF	(buf);

	debug((stderr, "get(%s, %s, %s, %s, %s, %x, %s)\n",
	    s->sfile, rev, mRev, iLst, xLst, flags, printOut));
	unless (s->state & SOPEN) {
		fprintf(stderr, "get: couldn't open %s\n", s->sfile);
err:		if (slist) free(slist);
		if (state) free(state);
		if (i2) free(i2);
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
	if ((s->state & BADREVS) && !(flags & FORCE)) {
		fprintf(stderr,
		    "get: bad revisions, run renumber on %s\n", s->sfile);
		s->state |= WARNED;
		goto err;
	}
	/* this has to be above the getedit() - that changes the rev */
	if (mRev) {
		tmp = sccs_impliedList(s, "get", rev, mRev);
		unless (tmp) goto err;
		i2 = strconcat(tmp, iLst, ",");
		if (i2 != tmp) free(tmp);
	}
	if (flags & EDIT) {
		int	f = (s->state & BRANCHOK) ? flags&FORCEBRANCH : 0;

		d = getedit(s, &rev, &lrev, f);
		if (!d) {
			fprintf(stderr, "get: can't find revision %s in %s\n",
			    rev, s->sfile);
			s->state |= WARNED;
		}
	} else {
		d = findrev(s, rev);
		if (!d) {
			fprintf(stderr,
			    "get: can't find revision like %s in %s\n",
			rev, s->sfile);
			s->state |= WARNED;
		}
	}
	unless (d) goto err;
	/* moved this up above the opens so that I can bail easily */
	unless (flags & SKIPGET) {
		if (i2) {
			slist = serialmap(s, d, flags, i2, xLst, &error);
		} else {
			slist = serialmap(s, d, flags, iLst, xLst, &error);
		}
		if (error == 1) {
			assert(!slist);
			fprintf(stderr,
			    "Malformed include/exclude list for %s\n",
			    s->sfile);
			s->state |= WARNED;
			goto err;
		}
		if (error == 2) {
			assert(!slist);
			fprintf(stderr,
		"Can't find specified rev in include/exclude list for %s\n",
			    s->sfile);
			s->state |= WARNED;
			goto err;
		}
	}
	if (flags & EDIT) {
		if (write_pfile(s, flags, d, rev, lrev, iLst, i2, xLst, mRev)) {
			goto err;
		}
		if (flags & SKIPGET) goto skip_get;
		unlinkGfile(s);
		popened = openOutput(encoding, s->gfile, &out);
		if (!out) {
			fprintf(stderr, "Can't open %s for writing\n",
			    s->gfile);
			unlock(s, 'p');
			unlock(s, 'z');
			goto err;
		}
	} else if (flags&SKIPGET) {
		goto skip_get;
	} else if (!(flags & PRINT)) {
		if (IS_WRITABLE(s)) {
			fprintf(stderr, "Writeable %s exists\n", s->gfile);
			s->state |= WARNED;
			goto err;
		}
		unlinkGfile(s);
		popened = openOutput(encoding, s->gfile, &out);
	} else {
		popened = openOutput(encoding, printOut, &out);
	}
	if ((s->state & RCS) && (flags & EXPAND)) flags |= RCSEXPAND;
	if ((s->state & BITKEEPER) && d->sum && !iLst && !xLst && !i2) {
		flags |= NEWCKSUM;
	}
	/* Think carefully before changing this */
	if (s->encoding != E_ASCII) {
		flags &= ~(REVNUMS|PREFIXDATE|USER|EXPAND|RCSEXPAND|LINENUM);
	}
	state = allocstate(0, 0, s->nextserial);
	if (flags & MODNAME) base = basenm(s->gfile);
	seekto(s, s->data);
	sum = 0;
	while (next(buf, s)) {
		register char *e;

		if (isData(buf)) {
			if (!print) continue;
			lines++;
			if (flags & NEWCKSUM) {
				for (e = buf; *e != '\n'; sum += *e++);
				sum += '\n';
			}
			if (flags & (LINENUM|PREFIXDATE|REVNUMS|USER|MODNAME)) {
				delta *tmp = sfind(s, print);

				if (flags&MODNAME)
					fprintf(out, "%s\t", base);
				if (flags&PREFIXDATE)
					fprintf(out, "%.8s\t", tmp->sdate);
				if (flags&USER)
					fprintf(out, "%s\t", tmp->user);
				if (flags&REVNUMS)
					fprintf(out, "%s\t", tmp->rev);
				if (flags&LINENUM)
					fprintf(out, "%6d\t", lines);
			}
			e = buf;
			if (flags & EXPAND) {
				for (e = buf; *e != '%' && *e != '\n'; e++);
				if (*e == '%') {
					e = expand(s, d, buf);
				} else {
					e = buf;
				}
			}
			if (flags & RCSEXPAND) {
				char	*t;

				for (t = buf; *t != '$' && *t != '\n'; t++);
				if (*t == '$') {
					e = rcsexpand(s, d, e);
				}
			}
			if (encoding != E_ASCII) {
				uchar	obuf[50];
				int	n = uudecode1(e, obuf);

				fwrite(obuf, n, 1, out);
			} else {
				fnlputs(e, out);
			}
			continue;
		}

		debug2((stderr, "%.*s", linelen(buf), buf));
		changestate(state, buf[1], atoi(&buf[3]));
		print = printstate((const serlist*)state, (const ser_t*)slist);
	}
	if ((flags & NEWCKSUM) && lines && (sum != d->sum)) {
		fprintf(stderr,
		    "get: bad delta cksum %u:%u for %s in %s, gotten anyway.\n",
		    d->sum, sum, d->rev, s->sfile);
	}
	debug((stderr, "GET done\n"));
	if (popened) {
		pclose(out);
	} else if (flags & PRINT) {
		unless (streq("-", printOut)) fclose(out);
	} else {
		fclose(out);
	}
	if (flags&EDIT) {
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
skip_get:
	if (flags&EDIT) {
		unlock(s, 'z');
	}
	if (!(flags&SILENT)) {
		fprintf(stderr, "%s %s", s->gfile, d->rev);
		if (flags & EDIT) {
			fprintf(stderr, " -> %s", lrev ? lrev : rev);
		}
		if (!(flags & SKIPGET)) fprintf(stderr, ": %d lines", lines);
		fprintf(stderr, "\n");
	}
	if (slist) free(slist);
	if (state) free(state);
	if (i2) free(i2);
	return (0);
}

/*
 * Get the diffs of the specified revision.
 * The diffs are only in terms of deletes and adds, no changes.
 * The output file is passed in so that callers can redirect it.
 */
int
sccs_getdiffs(sccs *s, char *rev, int flags, char *printOut)
{
	serlist *state = 0;
	ser_t	*slist = 0;
	ser_t	old = 0;
	delta	*d;
	int	with = 0, without = 0;
	int	count = 0, left = 0, right = 0;
	FILE	*out = 0;
	int	popened = 0;
	int	encoding = (flags&FORCEASCII) ? E_ASCII : s->encoding;
	int	error = 0;
	char	*prefix = "";
#define	NEITHER 0
#define	LEFT	1
#define	RIGHT	2
#define	BOTH	3
	int	side, nextside;
	BUF	(buf);
	char	tmpfile[100];
	FILE	*lbuf;

	sprintf(tmpfile, "/tmp/gdiffsU%d", getpid());
	unless (lbuf = fopen(tmpfile, "w+")) {
		fprintf(stderr, "getdiffs: couldn't open %s\n", tmpfile);
		s->state |= WARNED;
		return (-1);
	}
	unless (s->state & SOPEN) {
		fprintf(stderr, "getdiffs: couldn't open %s\n", s->sfile);
		s->state |= WARNED;
		return (-1);
	}
	unless (s->cksumok) {
		fprintf(stderr, "getdiffs: bad chksum on %s\n", s->sfile);
		s->state |= WARNED;
		return (-1);
	}
	unless (s->tree) {
		fprintf(stderr,
		    "getdiffs: no/bad delta tree in %s\n", s->sfile);
		s->state |= WARNED;
		return (-1);
	}
	if (s->state & BADREVS) {
		fprintf(stderr,
		    "getdiffs: bad revisions, run renumber on %s\n", s->sfile);
		s->state |= WARNED;
		return (-1);
	}
	d = findrev(s, rev);
	if (!d) {
		fprintf(stderr, "get: can't find revision like %s in %s\n",
		    rev, s->sfile);
		s->state |= WARNED;
	}
	unless (d) return (-1);
	slist = serialmap(s, d, flags, 0, 0, &error);
	popened = openOutput(encoding, printOut, &out);
	state = allocstate(0, 0, s->nextserial);
	seekto(s, s->data);
	side = NEITHER;
	nextside = NEITHER;

	while (next(buf, s)) {
		if (isData(buf)) {
			if (nextside == NEITHER) continue;
			if (count && nextside != side &&
			    (side == LEFT || side == RIGHT)) {
				/* print out command line */
				if (side == RIGHT) {
					fprintf(out, "%da%d", left, right+1);
					right += count;
					if (count != 1)
						fprintf(out, ",%d", right);
					fputc('\n', out);
					prefix = "> ";
				} else {
					assert(side == LEFT);
					fprintf(out, "%d", left+1);
					left += count;
					if (count != 1)
						fprintf(out, ",%d", left);
					fprintf(out, "d%d\n", right);
					prefix = "< ";
				}
				fseek(lbuf, 0L, SEEK_SET);
				while (count--) {
					int	c;
					fputs(prefix, out);
					while ((c = fgetc(lbuf)) != EOF) {
						fputc(c, out);
						if (c == '\n') break;
					}
					/* XXX: EOF is error condition */
					if (c == EOF) break;
				}
				count = 0;
				fseek(lbuf, 0L, SEEK_SET);
			}
			side = nextside;
			switch (side) {
			    case LEFT:
			    case RIGHT:
				count++;
				fnlputs(buf, lbuf);
				break;
			    case BOTH:	left++, right++; break;
			}
			continue;
		}
		debug2((stderr, "%.*s", linelen(buf), buf));
		changestate(state, buf[1], atoi(&buf[3]));
		with = printstate((const serlist*)state, (const ser_t*)slist);
		old = slist[d->serial];
		slist[d->serial] = 0;
		without = printstate((const serlist*)state,
				    (const ser_t*)slist);
		slist[d->serial] = old;

		nextside = with ? (without ? BOTH : RIGHT)
				: (without ? LEFT : NEITHER);

	}
	if (count) { /* there is something left in the buffer */
		/* print out command line */
		if (side == RIGHT) {
			fprintf(out, "%da%d", left, right+1);
			right += count;
			if (count != 1) fprintf(out, ",%d", right);
			fputc('\n', out);
			prefix = "> ";
		} else {
			assert(side == LEFT);
			fprintf(out, "%d", left+1);
			left += count;
			if (count != 1) fprintf(out, ",%d", left);
			fprintf(out, "d%d\n", right);
			prefix = "< ";
		}
		fseek(lbuf, 0L, SEEK_SET);
		while (count--) {
			int	c;
			fputs(prefix, out);
			while ((c = fgetc(lbuf)) != EOF) {
				fputc(c, out);
				if (c == '\n') break;
			}
			/* XXX: fixme: EOF is error condition */
			if (c == EOF) break;
		}
		count = 0;
		fseek(lbuf, 0L, SEEK_SET);
	}
	fclose(lbuf);
	unlink(tmpfile);
	if (popened) {
		pclose(out);
	} else {
		unless (streq("-", printOut)) fclose(out);
	}
	if (slist) free(slist);
	if (state) free(state);
	return (0);
}

/*
 * Return true if bad cksum
 */
#ifdef	USE_STDIO
private int
badcksum(sccs *s)
{
	char	buf[1024];
	register sum_t sum = 0;
	int	filesum;

	assert(s);
	seekto(s, 0);
	next(buf, s);
	if (sscanf(buf, "\001h%05d\n", &filesum) != 1) return (1);
	while (next(buf, s)) {
		char	*t = buf;

		while (*t) sum += *t++;
	}
	if (sum == filesum) s->cksumok = 1;
	debug((stderr,
	    "%s has %s cksum\n", s->sfile, s->cksumok ? "OK" : "BAD"));
	return (sum != filesum);
}
#else
private int
badcksum(sccs *s)
{
	register char *t;
	register char *end = s->mmap + s->size;
	register sum_t sum = 0;
	int	filesum;

	debug((stderr, "Checking sum from %x to %x (%d)\n",
	    s->mmap, end, end - s->mmap));
	assert(s);
	seekto(s, 0);
	filesum = atoi(&s->mmap[2]);
	debug((stderr, "File says sum is %d\n", filesum));
	t = s->mmap + 8;
	while (t < (end - 16)) {
		sum += t[0] + t[1] + t[2] + t[3] + t[4] + t[5] + t[6] + t[7] +
		    t[8] + t[9] + t[10] + t[11] + t[12] + t[13] + t[14] + t[15];
		t += 16;
	}
	while (t < end) sum += *t++;
	if (sum == filesum) s->cksumok = 1;
	debug((stderr,
	    "%s has %s cksum\n", s->sfile, s->cksumok ? "OK" : "BAD"));
	return (sum != filesum);
}
#endif	/* !USE_STDIO */

/*
 * Check for bad characters in the file.
 */
#ifdef	USE_STDIO
private int
badchars(sccs *s)
{
	char	buf[1024];

	assert(s);
	seekto(s, 0);
	while (next(buf, s)) {
		register char	*t = buf;

		while (*t) {
			unless (isascii(*t) || ((*t == '\001') && t == buf)) {
				fprintf(stderr,
				    "admin: bad line in %s follows\n%s", buf);
				return (1);
			}
			t++;
		}
	}
	return (0);
}
#else
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
	t += 8;
	for (t = s->mmap; t < end; t++) {
		unless (isascii(*t) || ((*t == '\001') && (t[-1] == '\n'))) {
			char	*r = t;

			while ((r > s->mmap) && (r[-1] != '\n')) r--;
			fprintf(stderr,
			    "admin: bad line in %s follows\n%.*s",
			    s->sfile, linelen(r), r);
			return (1);
		}
	}
	return (0);
}
#endif	/* !USE_STDIO */

/*
 * The table is all here in order, just print it.
 * New in Feb, '99: remove duplicates of metadata.
 */
private void
delta_table(sccs *s, FILE *out, int willfix)
{
	delta	*d;
	int	i;	/* used by EACH */
	int	first = willfix;
	char	buf[1024];
	int	bits = 0;

	assert((s->state & READ_ONLY) == 0);
	assert(s->state & Z_LOCKED);
	fprintf(out, "\001hXXXXX\n");
	s->cksum = 0;
	for (d = s->table; d; d = d->next) {
		if (d->flags & D_GONE) {
			while (d = d->next) assert(d->flags & D_GONE);
			break;
		}
		assert(d->date);
		if (d->next) {
			assert(d->next->date);
			if (d->date <= d->next->date) {
				sccs_fixDates(s);
			}
		}
		sprintf(buf, "\001s %05d/%05d/%05d\n",
		    d->added, d->deleted, d->same);
		if (first)
			fputs(buf, out);
		else
			fputsum(s, buf, out);
		sprintf(buf, "\001d %c %s %s %s %d %d\n",
		    d->type, sccsrev(d), d->sdate, d->user,
		    d->serial, d->pserial);
		fputsum(s, buf, out);
		if (d->include) {
			fputsum(s, "\001i ", out);
			putserlist(s, d->include, out);
			fputsum(s, "\n", out);
		}
		if (d->exclude) {
			fputsum(s, "\001x ", out);
			putserlist(s, d->exclude, out);
			fputsum(s, "\n", out);
		}
		if (d->ignore) {
			fputsum(s, "\001g ", out);
			putserlist(s, d->ignore, out);
			fputsum(s, "\n", out);
		}
		EACH(d->mr) {
			fputsum(s, d->mr[i], out);
			fputsum(s, "\n", out);
		}
		EACH(d->comments) {
			/* metadata */
			if (d->comments[i][0] == '\001') {
				fputsum(s, d->comments[i], out);
			} else {
				fputsum(s, "\001c ", out);
				fputsum(s, d->comments[i], out);
			}
			fputsum(s, "\n", out);
		}
		if (d->csetFile && !(d->flags & D_DUPCSETFILE)) {
			fputsum(s, "\001cB", out);
			fputsum(s, d->csetFile, out);
			fputsum(s, "\n", out);
		}
		if (d->cset) {
			fputsum(s, "\001cC", out);
			fputsum(s, d->cset, out);
			fputsum(s, "\n", out);
		}
		if (d->dateFudge) {
			fputsum(s, "\001cF", out);
			sprintf(buf, "%u", (unsigned int)d->dateFudge);
			fputsum(s, buf, out);
			fputsum(s, "\n", out);
		}
		if (d->hostname && !(d->flags & D_DUPHOST)) {
			fputsum(s, "\001cH", out);
			fputsum(s, d->hostname, out);
			fputsum(s, "\n", out);
		}
		if (s->state & BITKEEPER) {
			if (first) {
				fputsum(s, "\001cK", out);
				s->sumOff = ftell(out);
				fputs("XXXXX", out);
				fputsum(s, "\n", out);
			} else if (d->flags & D_CKSUM) {
				assert(d->type == 'D');
				sprintf(buf, "\001cK%u\n", d->sum);
				fputsum(s, buf, out);
			}
		}
		first = 0;
		if (d->flags & D_LODZERO) {
			lod	*l;

			for (l = s->lods; l; l = l->next) {
				unless (l->d == d) continue;
				fputsum(s, "\001cL", out);
				fputsum(s, l->name, out);
				if (l->heads) {
					fputsum(s, " ", out);
					putlodlist(s, l->heads, out);
				}
				fputsum(s, "\n", out);
				/* No break, there can be more than one */
			}
		}
		if (d->merge) {
			sprintf(buf, "\001cM%d\n", d->merge);
			fputsum(s, buf, out);
		}
		if (d->pathname && !(d->flags & D_DUPPATH)) {
			fputsum(s, "\001cP", out);
			fputsum(s, d->pathname, out);
			fputsum(s, "\n", out);
		}
		if (d->flags & D_MODE) {
		    	unless (d->parent && (d->parent->mode == d->mode)) {
				sprintf(buf, "\001cO%o\n", d->mode);
				fputsum(s, buf, out);
			}
		}
		if (d->flags & D_SYMBOLS) {
			symbol	*sym;

			for (sym = s->symbols; sym; sym = sym->next) {
				unless (sym->reald == d) continue;
				fputsum(s, "\001cS", out);
				fputsum(s, sym->name, out);
				fputsum(s, "\n", out);
			}
		}
		if (d->zone && !(d->flags & D_DUPZONE)) {
			fputsum(s, "\001cZ", out);
			fputsum(s, d->zone, out);
			fputsum(s, "\n", out);
		}
		if (!d->next) {
			/* Landing pad for fast rewrites */
			fputsum(s, "\001c", out);
			for (i = 0; i < LPAD_SIZE; i += 30) {
				fputsum(s,
				    "______________________________", out);
			}
			if (s->state & BIGPAD) {
				for (i = 0; i < 10*LPAD_SIZE; i += 30) {
					fputsum(s,
					    "______________________________",
					    out);
				}
			}
			fputsum(s, "\n", out);
		}
		fputsum(s, "\001e\n", out);
	}

	fputsum(s, "\001u\n", out);
	EACH(s->usersgroups) {
		fputsum(s, s->usersgroups[i], out);
		fputsum(s, "\n", out);
	}
	fputsum(s, "\001U\n", out);
	sprintf(buf, "\001f e %d\n", s->encoding);
	fputsum(s, buf, out);
	if (s->state & BRANCHOK) {
		fputsum(s, "\001f b\n", out);
	}
	if (s->state & BITKEEPER) bits |= X_BITKEEPER;
	if (s->state & YEAR4) bits |= X_YEAR4;
	if (s->state & RCS) bits |= X_RCSEXPAND;
	if (bits) {
		char	buf[40];

		sprintf(buf, "\001f x %u\n", bits);
		fputsum(s, buf, out);
	}
	if (s->defbranch) {
		fputsum(s, "\001f d ", out);
		fputsum(s, s->defbranch, out);
		fputsum(s, "\n", out);
	}
	EACH(s->flags) {
		fputsum(s, "\001f ", out);
		fputsum(s, s->flags[i], out);
		fputsum(s, "\n", out);
	}
#if	0
	for (sym = s->symbols; sym; sym = sym->next) {
		if (!sym->name) continue;
		fputsum(s, "\001f s ", out);
		fputsum(s, sym->rev, out);
		fputsum(s, " ", out);
		fputsum(s, sym->name, out);
		fputsum(s, "\n", out);
	}
#endif
	fputsum(s, "\001t\n", out);
	EACH(s->text) {
		fputsum(s, s->text[i], out);
		fputsum(s, "\n", out);
	}
	fputsum(s, "\001T\n", out);
}

/*
 * If we are trying to compare with expanded strings, do so.
 */
private inline int
expandnleq(sccs *s, delta *d, char *fbuf, char *sbuf, int flags)
{
	char	*e = fbuf;

	if (!(flags & (EXPAND|RCSEXPAND))) return 0;
	if (flags & EXPAND) {
		e = expand(s, d, e);
	}
	if (flags & RCSEXPAND) {
		e = rcsexpand(s, d, e);
	}
	return strnleq(e, sbuf);
}

/*
 * This is an expensive call but not as expensive as running diff.
 */
int
sccs_hasDiffs(sccs *s, int flags)
{
	FILE	*tmp = fopen(s->pfile, "r");
	serlist *state = 0;
	ser_t	*slist = 0;
	int	print = 0, different;
	delta	*d;
	char	rev[40];
	BUF	(fbuf);
	char	sbuf[1024];

#define	RET(x)	different = x; goto out;

	unless (tmp) return (-1);
	if (fscanf(tmp, "%s", rev) != 1) {
		verbose((stderr,
		    "can't get revision info from %s\n", s->pfile));
		RET(-1);
	}
	fclose(tmp); tmp = 0;
	unless (d = findrev(s, rev)) {
		verbose((stderr, "can't find %s in %s\n", rev, s->gfile));
		RET(-1);
	}
	unless (tmp = fopen(s->gfile, "r")) {
		verbose((stderr, "can't open %s\n", s->gfile));
		RET(-1);
	}
	assert(s->state & SOPEN);
	slist = serialmap(s, d, 0, 0, 0, 0);
	state = allocstate(0, 0, s->nextserial);
	seekto(s, s->data);
	while (next(fbuf, s)) {
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
	if (tmp) fclose(tmp);
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
int
deflate(sccs *s, char *tmpfile)
{
	FILE	*in, *out;
	char	cmd[1024];
	int	n;

	unless (out = fopen(tmpfile, "w")) return (-1);
	switch (s->encoding) {
	    case E_UUENCODE:
		in = fopen(s->gfile, "r");
		n = uuencode(in, out);
		fclose(in);
		fclose(out);
		break;
	    case E_UUGZIP:
		sprintf(cmd, "gzip -nq4 < %s", s->gfile);
		in = popen(cmd, "rb");
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
	
	if (stat(gfile, &sb)) {
		fprintf(stderr, "stat: ");
		perror(gfile);
		return (-1);
	}               
	if (sb.st_size > 0) {
		if ((fd = open(gfile, 2, 0660)) == -1) {
			perror(gfile);
			return (-1);
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

/*
* Returns:
*	-1 for some already bitched about error
*	0 if there were differences
*	1 if no differences
*/
private int
diff_gfile(sccs *s, pfile *pf, char *tmpfile)
{
	char	old[100];	/* the version from the s.file */
	char	new[100];	/* the new file, usually s->gfile */
	int	ret;

	debug((stderr, "diff_gfile(%s, %s)\n", pf->oldrev, s->gfile));
	if (s->encoding > E_ASCII) {
		sprintf(new, "%s/getU%d", TMP_PATH, getpid());
		if (IS_WRITABLE(s)) {
			if (deflate(s, new)) {
				unlink(new);
				return (-1);
			}
		} else {
/* XXX - I'm not sure when this would ever be used. */
			if (sccs_get(s,
			    0, 0, 0, 0, FORCEASCII|SILENT|PRINT, new)) {
				unlink(new);
				return (-1);
			}
		}
	} else {
		if (fix_lf(s->gfile) == -1) return (-1); 
		strcpy(new, s->gfile);
	}
	sprintf(old, "%s/get%d", TMP_PATH, getpid());
	if (sccs_get(s, pf->oldrev, pf->mRev, pf->iLst, pf->xLst,
	    FORCEASCII|SILENT|PRINT, old)) {
		unlink(old);
		return (-1);
	}
	ret = diff(old, new, D_DIFF, tmpfile);
	unlink(old);
	unless (streq(new, s->gfile)) unlink(new);	/* careful */
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

private void
unlinkGfile(sccs *s)
{
	unlink(s->gfile);	/* Careful */
	s->mode = 0;
}

private void
unedit(sccs *s, int flags)
{
	unlink(s->pfile);
	unless (flags&SAVEGFILE) unlinkGfile(s);
}

private void
pdiffs(char *gfile, char *left, char *right, FILE *diffs)
{
	int	first = 1;
	char	buf[1024];

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
sccs_clean(sccs *s, int flags)
{
	pfile	pf;
	char	tmpfile[50];

	unless (HAS_SFILE(s)) {
		verbose((stderr, "%s not under SCCS control\n", s->gfile));
		return (0);
	}
	unless (s->tree) return (-1);
	unless (HAS_PFILE(s)) {
		if (!IS_WRITABLE(s)) {
			verbose((stderr, "Clean %s\n", s->gfile));
			unlinkGfile(s);
			return (0);
		}
		fprintf(stderr, "%s writable but not edited?\n", s->gfile);
		return (1);
	}
	if (flags & UNEDIT) {
		unedit(s, flags);
		return (0);
	}
	unless (HAS_GFILE(s)) {
		verbose((stderr, "%s not checked out\n", s->gfile));
		return (0);
	}
	/*
	 * XXX - there is a bug somewhere that leaves stuff edited when it
	 * isn't.  I suspect some interactions with make, but I'm not
	 * sure.  The difference ends up being on a line with the keywords.
	 */
	if (access(s->gfile, W_OK)) {
		verbose((stderr, "%s edited but not writeable?\n", s->gfile));
		flags |= EXPAND;
		if (s->state & RCS) flags |= RCSEXPAND;
	}
	if (read_pfile("clean", s, &pf)) return (1);
	sprintf(tmpfile, "%s/diffg%d", TMP_PATH, getpid());
	/*
	 * hasDiffs() ignores keyword expansion differences.
	 * And it's faster.
	 */
	unless (sccs_hasDiffs(s, flags)) goto nodiffs;
	switch (diff_gfile(s, &pf, tmpfile)) {
	    case 1:		/* no diffs */
nodiffs:	verbose((stderr, "Clean %s\n", s->gfile));
		unedit(s, flags);
		free_pfile(&pf);
		unlink(tmpfile);
		return (0);
	    case 0:		/* diffs */
		unless (flags & PRINT) {
			fprintf(stderr,
			    "%s has been modified, needs delta.\n", s->gfile);
		} else {
			pdiffs(s->gfile,
			    pf.oldrev, "edited", fopen(tmpfile, "r"));
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
 * provide information about the editing status of a file.
 * XXX - does way too much work for this, shouldn't sccs init.
 *
 * Return codes are passed out to exit() so don't error on warnings.
 */
int
sccs_info(sccs *s, int flags)
{
	FILE	*f;
	char	buf[200];

	unless (HAS_SFILE(s)) {
		verbose((stderr, "%s not under SCCS control\n", s->gfile));
		return (0);
	}
	GOODSCCS(s);
	if (!HAS_PFILE(s)) {
		return (0);
	}
	unless (HAS_GFILE(s)) {
		verbose((stderr, "%s not checked out\n", s->gfile));
		return (0);
	}
	sprintf(buf, "%s:", s->gfile);
	printf("%-16s", s->gfile);
	f = fopen(s->pfile, "r");
	if (fgets(buf, sizeof(buf), f)) {
		char	*s;
		for (s = buf; *s && *s != '\n'; ++s)
			;
		*s = 0;
		printf(buf);
	}
	fclose(f);
	switch (sccs_hasDiffs(s, flags)) {
	    case 1:
		printf(" (modified, needs delta)\n");
		return (1);
	    case -1:
		fprintf(stderr,
		    "couldn't compute diffs on %s, skipping\n", s->gfile);
		return (1);
	    default: printf("\n");
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
		unless (isascii(buf[i])) return (0);
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
	char	*file = (flags&EMPTY) ? DEV_NULL : s->gfile;
	char	buf[1024+80];
	char	*mode = "rb";	/* default mode is binary mode */

	unless (flags & EMPTY) {
		unless (HAS_GFILE(s)) {
			*inp = NULL;
			return (-1);
		}
	}
	switch (s->encoding) {
	    default:
	    case E_ASCII:
		mode = "rt"; /* read in text mode */
	    case E_UUENCODE:
		if (streq("-", file)) {
			*inp = stdin;
			return (0);
		}
		*inp = fopen(file, mode);
		if ((s->encoding == E_ASCII) && ascii(*inp)) return (0);
		s->encoding = E_UUENCODE;
		return (0);
	    case E_UUGZIP:
		/*
		 * Some very seat of the pants testing showed that -4 was
		 * the best time/space tradeoff.
		 */
		if (streq("-", file)) {
			*inp = popen("gzip -nq4", "rb");
		} else {
			sprintf(buf, "gzip -nq4 < %s", file);
			*inp = popen(buf, "rb");
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
	if ((s->state & BITKEEPER) && (type == 'D')) d->flags |= D_CKSUM;
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
		unless (d->user) d->user = strdup(getuser());
		unless (d->hostname && sccs_gethost()) {
			hostArg(d, sccs_gethost());
		}
		unless (d->pathname && s) pathArg(d, relativeName(s, 0, 0));
#ifdef	AUTO_MODE
		assert("no" == 0);
		unless (d->flags & D_MODE) {
			if (s->state & GFILE) {
				d->mode = s->mode;
				d->flags |= D_MODE;
			} else {
				modeArg(d, "0664");
			}
		}
#endif
	}
	return (d);
}

private void
updatePending(sccs *s, delta *d)
{
	int fd;
	char *bk_root, buf[2048];
	
	assert(s);
	assert(d);
	if (d->pathname && streq("ChangeSet", d->pathname)) return;
	bk_root = getenv("BK_ROOT");
	unless (bk_root) return;
	sprintf(buf, "%s/SCCS/x.pending", bk_root);
	fd = open(buf, O_CREAT|O_APPEND|O_WRONLY, 0660);
	unless (fd > 0) return;
	sccs_sdelta(buf, sccs_ino(s));
	strcat(buf, " ");
	sccs_sdelta(&buf[strlen(buf)], d);
	strcat(buf, "\n");
	if (write(fd, buf, strlen(buf)) == -1) {
		perror("Can't write to pending file");
	}
	close(fd);
}

/*
 * Check in initial gfile.
 */
/* ARGSUSED */
private int
checkin(sccs *s, int flags, delta *prefilled, int nodefault, FILE *diffs)
{
	FILE	*id, *sfile, *gfile;
	delta	*n;
	int	added = 0;
	int	popened, len;
	char	*t;
	char	buf[1024];
	admin	l[2];

	assert(s);
	debug((stderr, "checkin %s %x\n", s->gfile, flags));
	unless (flags & NEWFILE) {
		verbose((stderr,
		    "%s not checked in, use -i flag.\n", s->gfile));
		unlock(s, 'z');
		if (prefilled) sccs_freetree(prefilled);
		return (1);
	}
	unless (diffs) {
		popened = openInput(s, flags, &gfile);
		unless (gfile) {
			perror(s->gfile);
			unlock(s, 'z');
			if (prefilled) sccs_freetree(prefilled);
			return (1);
		}
	}
	if (exists(s->sfile)) {
		fprintf(stderr, "delta: lost checkin race on %s\n", s->sfile);
		if (prefilled) sccs_freetree(prefilled);
		if (!diffs && (gfile != stdin)) {
			if (popened) pclose(gfile); else fclose(gfile);
		}
		return (1);
	}
	sfile = fopen(s->sfile, "wb"); /* open in binary mode */
	if (prefilled) {
		n = prefilled;
	} else {
		n = calloc(1, sizeof(*n));
	}
	n = sccs_dInit(n, 'D', s, nodefault);
	unless (n->flags & D_MODE) {
		if (s->state & GFILE) {
			n->mode = s->mode;
			n->flags |= D_MODE;
		}
	}
	if (!n->rev) n->rev = strdup("1.1");
	explode_rev(n);
	unless (s->state & NOSCCSDIR) {
		if (s->state & CSET) {
			sccs_sdelta(buf, n);
			n->csetFile = strdup(buf);
			s->state |= BITKEEPER;	
			n->flags |= D_CKSUM;
		} else {
			t = relativeName(s, 0, 0);
			assert(t);
			if (t[0] != '/') {
				n->csetFile = getCSetFile(s);
				s->state |= BITKEEPER;	
			}
		}
	} 
	n->serial = s->nextserial++;
	n->next = s->table;
	s->table = n;
	if (n->flags & D_BADFORM) {
		unlock(s, 'z');
		if (prefilled) sccs_freetree(prefilled);
		fprintf(stderr, "checkin: bad revision: %s for %s\n",
		    n->rev, s->sfile);
		return (1);
	}
	unless (hasComments(n)) {
		sprintf(buf, "%s created on %s by %s",
		    s->gfile, n->sdate, n->user);
		n->comments = addLine(n->comments, strdup(buf));
	}
	if (t = getenv("BK_LOD")) {
		unless (n->flags & D_LODSTR) {
			n->flags |= D_LODSTR;
			n->lod = (lod *)t;
		}
	}
	if (n->flags & D_LODSTR) {
		l[1].flags = 0;
		l[0].flags = A_ADD;
		l[0].thing = (char *)n->lod;
		n->lod = 0;
		n->flags &= ~D_LODSTR;
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
	addLod("delta", s, flags, l, 0);
	delta_table(s, sfile, 1);
	buf[0] = 0;
	fputsum(s, "\001I 1\n", sfile);
	s->dsum = 0;
	if (s->encoding > E_ASCII) {
		/* XXX - this is incorrect, it needs to do it depending on
		 * what the encoding is.
		 */
		added = uuencode_sum(s, gfile, sfile);
	} else {
		if (diffs) {
			fnext(buf, diffs);	/* skip diff header */
			while (fnext(buf, diffs)) {
				s->dsum += fputsum(s, &buf[2], sfile);
				added++;
			}
			fclose(diffs);
		} else {
			while (fnext(buf, gfile)) {
				s->dsum += fputsum(s, buf, sfile);
				added++;
			}
		}
		/*
		 * For ascii files, add missing line feeds automagically.
		 */
		len = strlen(buf);
		if (len && (buf[len - 1] != '\n')) {
			s->dsum += fputsum(s, "\n", sfile);
		}
	}
	fputsum(s, "\001E 1\n", sfile);
	end(s, n, sfile, flags, added, 0, 0);
	if (s->state & BITKEEPER) {
		if (id = idFile(s)) {
			char	*path = s->tree->pathname;

			sccs_pdelta(s->tree, id);
			unless (path) path = relativeName(s, 0, 0);
			if (streq(path, s->gfile)) {
				fprintf(id, "\n");
			} else {
				fprintf(id, " %s\n", path);
			}
			fclose(id);
		}
	} 
	if (!diffs && (gfile != stdin)) {
		if (popened) pclose(gfile); else fclose(gfile);
	}
#ifdef	PARANOID
	unless (flags&SAVEGFILE) {
		rename(s->gfile, sccsXfile(s, 'G'));
		s->state &= ~WRITABLE;
	}
#else
	unless (flags&SAVEGFILE) {
		unlinkGfile(s);	/* Careful */
	}
#endif
	Chmod(s->sfile, 0444);
	fclose(sfile);
	if (s->state & BITKEEPER) updatePending(s, n);
	unlock(s, 'z');
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

private int
checkrevs(sccs *s, int flags)
{
	delta	*d;
	int	e;

	for (e = 0, d = s->table; d; d = d->next) {
		e |= checkRev(s->sfile, d, flags);
	}
	return (e);
}

private int
checkRev(char *file, delta *d, int flags)
{
	int	error = 0;
	delta	*e;

	if (d->type == 'R') return (0);

	if (d->flags & D_BADFORM) {
		fprintf(stderr, "%s: bad rev '%s'\n", file, d->rev);
	}

	/*
	 * Make sure that the revision is well formed.
	 */
	if (!d->r[0] || (!d->r[1] && (d->r[0] != 1)) || (d->r[2] && !d->r[3])) {
		unless (flags & SHUTUP) {
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
			unless (flags & SHUTUP) {
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
			unless (flags & SHUTUP) {
				fprintf(stderr,
				    "%s: rev %s not connected to trunk\n",
				    file, d->rev);
			}
			error = 1;
		}
		if ((p->r[0] != d->r[0]) || (p->r[1] != d->r[1])) {
			unless (flags & SHUTUP) {
				fprintf(stderr,
				    "%s: rev %s has incorrect parent %s\n",
				    file, d->rev, p->rev);
			}
			error = 1;
		}
		/* if it's a x.y.z.q and not a .1, then check parent */
		if ((d->r[3] > 1) && (d->parent->r[3] != d->r[3]-1)) {
			unless (flags & SHUTUP) {
				fprintf(stderr,
				    "%s: rev %s has incorrect parent %s\n",
				    file, d->rev, p->rev);
			}
			error = 1;
		}
#ifdef	crazy_wow
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
			unless (flags & SHUTUP) {
				fprintf(stderr,
				    "%s: rev %s has incorrect parent %s\n",
				    file, d->rev, d->parent->rev);
			}
			error = 1;
		}
	} else {
		/* Otherwise, this should be a .1 node */
		if (d->r[1] != 1) {
			unless (flags & SHUTUP) {
				fprintf(stderr, "%s: rev %s should be a .1 rev"
				    " since parent %s is a different release\n",
				    file, d->rev, d->parent->rev);
			}
			error = 1;
		}
	}
	/* If there is a parent, make sure the dates increase. */
	/* XXX - this now needs to look at fudge. */
	// FIXME
time:	if (d->parent && (d->date < d->parent->date)) {
		if ((flags & (SHUTUP|VERBOSE)) == VERBOSE) {
			fprintf(stderr,
			    "%s: time goes backwards between %s and %s\n",
			    file, d->rev, d->parent->rev);
			fprintf(stderr, "\t%s: %s    %s: %s -> %d seconds\n",
			    d->rev, d->sdate, d->parent->rev, d->parent->sdate,
			    (int)(d->date - d->parent->date));
		}
		error |= 2;
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
	struct	tm *tm;
	time_t	tt;
	char	tmp[50];
	extern	long timezone;
	int	year, month, day, hour, minute, second, msec, hwest, mwest;
	int	rcs = 0;

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
			getit(hwest);
			getit(mwest);
		}
	} else if (*arg && arg[2] == '-') {
		getit(hwest);
		/* I don't know if RCS ever puts in the minutes, but I'll
		 * take 'em if they give 'em.
		 */
		if (*arg && arg[2] == ':') getit(mwest);
	} else if (rcs) {	/* then UTC */
		/* This is a bummer because we can't figure out in which
		 * timezone the delta was performed.
		 * So we assume here.
		 * XXX - maybe not the right answer?
		 */
		tt = time(0);
		tm = localtime(&tt);
		hwest = timezone/3600;
		mwest = timezone%3600;
	} else if (defaults) {		/* then local time */
		tt = time(0);
		tm = localtime(&tt);
		hwest = timezone/3600;
		mwest = timezone%3600;
	}
	sprintf(tmp, "%02d/%02d/%02d %02d:%02d:%02d",
	    year, month, day, hour, minute, second);
	d->sdate = strdup(tmp);
	if (hwest || mwest) {
		char	sign = '-';

		if (hwest < 0) {
			hwest = -hwest;
			mwest = -mwest;
			sign = '+';
		}
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
csetArg(delta *d, char *arg) { ARG(cset, 0, 0); }

private delta *
hostArg(delta *d, char *arg) { ARG(hostname, D_NOHOST, D_DUPHOST); }

private delta *
pathArg(delta *d, char *arg) { ARG(pathname, D_NOPATH, D_DUPPATH); }

private delta *
modeArg(delta *d, char *arg)
{
	unsigned int m;

	assert(d);
	for (m = 0; isdigit(*arg); m <<= 3, m |= (*arg - '0'), arg++);
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
	sym->d = sym->reald = d;
	d->flags |= D_SYMBOLS;
	return;
}

private void
lodArg(sccs *s, delta *d, char *name)
{
	lod	*l = calloc(1, sizeof(*l));
	lod	*m;
	char	*t;

	assert(d);
	l->name = strnonldup(name);

	/*
	 * Extract all the serial numbers of all the heads, if any.
	 */
	for (t = l->name; *t; t++) {
		if (*t == ' ') {
			*t++ = 0;
			l->heads = getserlist(s, 1, t, 0);
			debug((stderr, "LOD head: %d ...\n", l->heads[1]));
			break;
		}
	}
	l->d = d;
	l->next = s->lods;
	s->lods = l;
	d->flags |= D_LODZERO;
	/*
	 * Special case for rev 1.1.  It can be in an LOD, the LOD points
	 * to itself.
	 */
	if ((d->r[0] == 1) && (d->r[1] == 1) && !d->r[2] && 
	    l->heads && (l->heads[1] == d->serial)) {
	    	d->flags |= D_LODHEAD;
		d->lod = l;
	}

	/* There should be no duplicates */
	for (m = s->lods; m; m = m->next) {
		if (m == l) continue;
		if (streq(m->name, l->name)) assert("duplicate LOD" == 0);
	}

	/* We do not set the other D_* flags here, we haven't finished the
	 * graph yet.
	 */
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
 * Return true iff the LOD already exists.
 */
private int
dupLod(lod *lods, char *s)
{
	while (lods) {
		if (streq(lods->name, s)) return (1);
		lods = lods->next;
	}
	return (0);
}

#ifndef	USE_STDIO
/*
 * Try and stuff the symbol into the landing pad,
 * return 1 if it fails.
 * XXX - needs to insist on a revision.
 */
int
sccs_addSym(sccs *sc, int flags, char *s)
{
	char	*rev;
	delta	*d = 0;
	char	*t, *r;
	sum_t	sum;
	int	len;
	char	buf[1024];

	if (!lock(sc, 'z')) {
		fprintf(stderr, "sccs_addSym: can't zlock %s\n", sc->gfile);
		return -1;
	}
	assert((sc->state & READ_ONLY) == 0);
	assert(sc->state & Z_LOCKED);
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
			unlock(sc, 'z');
			return (-1);
		}
		rev = d->rev;
	}
	if (dupSym(sc->symbols, s, rev)) {
		verbose((stderr,
		    "admin (fast add): symbol %s exists on %s\n", s, rev));
		free(s);
		unlock(sc, 'z');
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
	sprintf(buf,
"\001s 00000/00000/00000\n\001d R %s %s %s %d %d\n\001cS%s\n\001e\n",
	    rev, now(), getuser(), sc->nextserial++, d->serial, s);
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
		free(s);
		unlock(sc, 'z');
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
	if (sc->state & MAPPRIVATE) {
		lseek(sc->fd, 0, SEEK_SET);
		write(sc->fd, sc->mmap, sc->landingpad + len + 8 - sc->mmap);
	}
	verbose((stderr, "admin: fast add symbol %s->%s in %s\n",
	    s, rev, sc->sfile));
	free(s);
	unlock(sc, 'z');
	return (0);
}
#endif

private int
addLod(char *me, sccs *sc, int flags, admin *l, int *ep)
{
	int	added = 0, i, error = 0;

	/*
	 * "lod" means TOT.
	 * "lod:" means TOT.
	 * "lod:1.2" means that rev.
	 * "lod:1" or "sym:1.2.1" means TOT of that branch.
	 *
	 * lod= is like lod: except it also says set the default branch.
	 *
	 * Special case for 1.1.  If setting a symbol, also set the first
	 * serial to 1.1.
	 */
	for (i = 0; l && l[i].flags; ++i) {
		char	*rev;
		char	*name = strdup(l[i].thing);
		lod	*lp;
		delta	*d;
		int	setDefault = 0;

		if ((rev = strrchr(name, ':'))) {
			*rev++ = 0;
		} else if ((rev = strrchr(name, '='))) {
			*rev++ = 0;
			setDefault = 1;
		}

		d = findrev(sc, rev);
		/*
		 * If we find nothing, see if we are an empty LOD, in which
		 * case add the LOD to our parent.
		 */
		unless (d || (rev && isdigit(rev[0]))) {
			debug((stderr, "addLod(rev=%s)\n", rev));
			if (!rev &&
			    sc->defbranch && !isdigit(sc->defbranch[0])) {
				unless (rev = sc->defbranch) {
					if (sc->tree && sc->tree->lod) {
						rev = sc->tree->lod->name;
					}
				}
			}
			if (rev) {
				lod	*lp;
				char	*t;

				if (t = strchr(rev, '.')) *t = 0;
				debug((stderr, "addLod2(rev=%s)\n", rev));
				for (lp = sc->lods; lp; lp = lp->next) {
					if (streq(lp->name, rev)) break;
				}
				if (t) *t = '.';
				if (lp) {
					d = lp->d;
					debug((stderr, "parent=%s", d->rev));
				}
			}
		}
		unless (d) {
			fprintf(stderr,
			    "%s: can't find %s in %s\n", me, rev, sc->sfile);
lod_err:		error = 1; sc->state |= WARNED;
			free(name);
			continue;
		}
		if (!rev || !*rev) rev = d->rev;
		if (isdigit(name[0])) {
			fprintf(stderr,
			    "%s: %s: can't start with a digit.\n", me, name);
			goto lod_err;
		}
		if (strchr(name, '.') ||
		    strchr(name, '(') ||
		    strchr(name, ')') ||
		    strchr(name, '{') ||
		    strchr(name, '}')) {
			fprintf(stderr,
			    "%s: can't have [{}().] in ``%s''\n", me, name);
			goto lod_err;
		}
		if (dupLod(sc->lods, name)) {
			fprintf(stderr, "%s: LOD %s already exists in %s\n",
			    me, name, sc->sfile);
			goto lod_err;
		}
		if (dupSym(sc->symbols, name, 0)) {
			fprintf(stderr,
			    "%s: LOD %s already exists as a symbol\n",
			    me, name);
			goto lod_err;
		}
		lp = calloc(1, sizeof(lod));
		lp->name = name;
		lp->d = d;
		lp->next = sc->lods;
		sc->lods = lp;
		d->flags |= D_LODZERO;
		if (flags & NEWFILE) lp->heads = addSerial(0, 1);
		if (setDefault) {
		    	if (sc->defbranch) free(sc->defbranch);
			sc->defbranch = strdup(name);
			verbose((stderr,
			    "%s: add default LOD %s.0->%s in %s\n",
			    me, name, rev, sc->sfile));
		} else {
			verbose((stderr,
			    "%s: add LOD %s.0->%s in %s\n",
			    me, name, rev, sc->sfile));
		}
		added++;
	}
	if (ep) *ep = error;
	return (added);
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
sym_err:		error = 1; sc->state |= WARNED;
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
		if (dupLod(sc->lods, rev)) {
			fprintf(stderr, "%s: LOD %s already exists in %s\n",
			    me, rev, sc->sfile);
			goto sym_err;
		}
		n = calloc(1, sizeof(delta));
		n->next = sc->table;
		sc->table = n;
		n = sccs_dInit(n, 'R', sc, 0);
		n->rev = strdup(d->rev);
		explode_rev(n);
		n->pserial = d->serial;
		n->serial = sc->nextserial++;
		sc->numdeltas++;
		dinsert(sc, 0, n);
		if (addsym(sc, n, d, rev, sym) == 0) {
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

/*
 * admin the specified file.
 *
 * Note: flag values are optional.
 *
 * XXX - this could do the insert hack for paths/users/whatever.
 * For large files, this is a win.
 */
int
sccs_admin(sccs *sc, int flags,
	admin *f, admin *l, admin *u, admin *s, char *text)
{
	FILE	*sfile = 0;
	int	error = 0, locked, i;
	char	*t;
	BUF	(buf);

	GOODSCCS(sc);
	unless (flags & CHECKFILE) {
		unless (locked = lock(sc, 'z')) {
			verbose((stderr,
			    "admin: can't get lock on %s\n", sc->sfile));
			error = -1; sc->state |= WARNED;
out:
			if (sfile) fclose(sfile);
			if (locked) unlock(sc, 'z');
			debug((stderr, "admin returns %d\n", error));
			return (error);
		}
	}
#define	OUT	error = -1; sc->state |= WARNED; goto out;

	unless (HAS_SFILE(sc)) {
		verbose((stderr, "admin: no SCCS file: %s\n", sc->sfile));
		OUT;
	}

	if (flags & CHECKFILE) {
		if (checkrevs(sc, flags) || checkdups(sc) ||
		    ((flags & CHECKASCII) && badchars(sc))) {
			OUT;
		}
		verbose((stderr, "admin: %s checks out OK\n", sc->sfile));
		goto out;
	}

	if (addLod("admin", sc, flags, l, &error)) flags |= NEWCKSUM;
	if (addSym("admin", sc, flags, s, &error)) flags |= NEWCKSUM;

	if (text) {
		FILE	*desc;
		char	dbuf[200];

		if (!text[0]) {
			if (sc->text) {
				freeLines(sc->text);
				sc->text = 0;
				flags |= NEWCKSUM;
			}
			goto user;
		}
		desc = fopen(text, "r");
		if (!desc) {
			fprintf(stderr, "admin: can't open %s\n", text);
			error = 1; sc->state |= WARNED;
			goto user;
		}
		if (sc->text) {
			freeLines(sc->text);
			sc->text = 0;
		}
		while (fgets(dbuf, sizeof(dbuf), desc)) {
			sc->text = addLine(sc->text, strnonldup(dbuf));
		}
		fclose(desc);
		flags |= NEWCKSUM;
	}

user:	for (i = 0; u && u[i].flags; ++i) {
		flags |= NEWCKSUM;
		if (u[i].flags & A_ADD) {
			sc->usersgroups =
			    addLine(sc->usersgroups, strdup(u[i].thing));
		} else {
			unless (removeLine(sc->usersgroups, u[i].thing)) {
				verbose((stderr,
				    "admin: user/group %s not found in %s\n",
				    u[i].thing, sc->sfile));
				error = 1; sc->state |= WARNED;
			}
		}
	}

	/*
	 * b	turn on branching support (BRANCHOK)
	 * d	default branch (sc->defbranch)
	 * R	turn on rcs keyword expansion (RCSEXPAND)
	 * Y	turn on 4 digit year printouts
	 *
	 * Anything else, just eat it.
	 */
	for (i = 0; f && f[i].flags; ++i) {
		int	add = f[i].flags & A_ADD;
		char	*v = &f[i].thing[1];

		flags |= NEWCKSUM;
		switch (f[i].thing[0]) {
		    char	buf[500];

		    case 'b':	if (add)
					sc->state |= BRANCHOK;
				else
					sc->state &= ~BRANCHOK;
				break;
		    case 'd':	if (sc->defbranch) free(sc->defbranch);
				sc->defbranch = *v ? strdup(v) : 0;
				break;
		    case 'R':	if (add)
					sc->state |= RCS;
				else
					sc->state &= ~RCS;
				break;
		    case 'Y':	if (add)
					sc->state |= YEAR4;
				else
					sc->state &= ~YEAR4;
				break;
		    default:	sprintf(buf, "%c %s", v[-1], v);
				if (add) {
					sc->flags =
					    addLine(sc->flags, strdup(buf));
				} else {
					unless (removeLine(sc->flags, buf)) {
						verbose((stderr,
						    "admin: flag %s not "
						    "found in %s\n",
						    buf, sc->sfile));
						error = 1; sc->state |= WARNED;
					}
				}
				break;
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
	delta_table(sc, sfile, 0);

	seekto(sc, sc->data);
	assert(sc->state & SOPEN);
	while (next(buf, sc)) {
		fputsum(sc, buf, sfile);
	}
	fseek(sfile, 0L, SEEK_SET);
	fprintf(sfile, "\001h%05u\n", sc->cksum);

	sccs_close(sc), fclose(sfile), sfile = NULL;
	/*
	 * XXX - right here, I should look at old checksum and if no different,
	 * leave well enough alone.
	 */
#ifdef	PARANOID
	t = sccsXfile(sc, 'S');
	unlink(t);
	rename(sc->sfile, t);
#else
	unlink(sc->sfile);		/* Careful. */
#endif
	t = sccsXfile(sc, 'x');
	if (rename(t, sc->sfile)) {
		fprintf(stderr,
		    "delta: can't rename(%s, %s) left in %s\n",
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
	char	small[10];

	sertoa(small, val);
	fputsum(s, pre, out);
	fputsum(s, small, out);
	fputsum(s, "\n", out);
}

#define	nextline(inc)	nxtline(s, &inc, 0, &lines, &print, out, state, slist)
#define	beforeline(inc) nxtline(s, &inc, 1, &lines, &print, out, state, slist)

void
nxtline(sccs *s, int *ip, int before, int *lp, int *pp, FILE *out,
	register serlist *state, ser_t *slist)
{
	int	c, print = *pp, incr = *ip, lines = *lp;
	sum_t	sum;
	register BUF	(buf);

	debug((stderr, "nxtline(@%d, before=%d print=%d, sum=%d) ",
	    lines, before, print, s->dsum));
	while (!eof(s)) {
		if (before && print) { /* if move upto next printable line */
			peekc(c, s);
			if (c != '\001')  break;
		}
		if (!next(buf, s)) break;
		debug2((stderr, "[%d] ", lines));
		debug2((stderr, "G> %.*s", linelen(buf), buf));
		sum = fputsum(s, buf, out);
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

int
delta_body(sccs *s, delta *n, FILE *diffs, FILE *out, int *ap, int *dp, int *up)
{
	serlist *state = 0;
	ser_t	*slist = 0;
	int	print = 0;
	int	lines = 0;
	int	added = 0, deleted = 0, unchanged = 0;
	sum_t	sum;
	char	buf2[1024];

	assert((s->state & READ_ONLY) == 0);
	assert(s->state & Z_LOCKED);
	*ap = *dp = *up = 0;
	/*
	 * Do the actual delta.
	 */
	seekto(s, s->data);
	slist = serialmap(s, n, 0, 0, 0, 0);	/* XXX - -gLIST */
	s->dsum = 0;
	assert(s->state & SOPEN);
	state = allocstate(0, 0, s->nextserial);
	while (fnext(buf2, diffs)) {
		int	where;
		char	what;

newcmd:
		if (scandiff(buf2, &where, &what) != 0) {
			fprintf(stderr, "delta: can't figure out '%s'\n", buf2);
			if (state) free(state);
			if (slist) free(slist);
			return (-1);
		}
		debug2((stderr, "where=%d what=%c\n", where, what));

#define	ctrl(pre, val)	doctrl(s, pre, val, out)

		if (what != 'a') where--;
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
			while (fnext(buf2, diffs)) {
				if (isdigit(buf2[0])) {
					ctrl("\001E ", n->serial);
					goto newcmd;
				}
				s->dsum += fputsum(s, &buf2[2], out);
				debug2((stderr, "INS %s", &buf2[2]));
				added++;
			}
			break;
		    case 'd':
			beforeline(unchanged);
			ctrl("\001D ", n->serial);
			sum = s->dsum;
			while (fnext(buf2, diffs)) {
				if (isdigit(buf2[0])) {
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
			while (fnext(buf2, diffs)) {
				if (!strcmp(buf2, "---\n")) break;
				nextline(deleted);
			}
			s->dsum = sum;
			ctrl("\001E ", n->serial);
			/* add the new stuff */
			ctrl("\001I ", n->serial);
			while (fnext(buf2, diffs)) {
				if (isdigit(buf2[0])) {
					ctrl("\001E ", n->serial);
					goto newcmd;
				}
				s->dsum += fputsum(s, &buf2[2], out);
				debug2((stderr, "INS %s", &buf2[2]));
				added++;
			}
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
	return (0);
}

/*
 * Initialize as much as possible from the file.
 * Don't override any information which is already set.
 * XXX - this needs to track do_prs/do_patch closely.
 */
delta *
sccs_getInit(sccs *sc, delta *d, FILE *f, int patch, int *errorp, int *linesp)
{
	char	*s, *t;
	char	buf[500];
	int	nocomments = d && d->comments;
	int	error = 0;
	int	lines = 0;
	char	type;

	unless (f) return (d);

#define	WANT(c) ((buf[0] == c) && (buf[1] == ' '))
	unless (fnext(buf, f)) {
		fprintf(stderr, "Warning: no delta line in init file.\n");
		error++;
		goto out;
	}
	lines++;
	chop(buf);
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
	if (patch) goto comments;	/* skip the rest of this line */
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
	unless (d->added) {
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

	/*
	 * Comments are optional and look like:
	 * C added 4.x etc targets
	 */
comments:
	buf[0] = 0;
	while (fnext(buf, f) && WANT('C')) {
		lines++;
		if (buf[2] == '\001') {
			fprintf(stderr, "Warning: skipping %s\n", buf);
		} else unless (nocomments) {
			chop(buf);
			d->comments = addLine(d->comments, strdup(&buf[2]));
		}
		buf[0] = 0;
	}
	if (!buf[0]) goto out;
	lines++;
	chop(buf);

	/* Excludes are optional and are specified as keys.
	 * If there is no sccs* ignore them.
	 */
	while (WANT('E')) {
		if (sc) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			unless (e) {
				fprintf(stderr, "Can't find ex %s in %s\n",
				    &buf[2], sc->sfile);
			} else {
				d->exclude = addSerial(d->exclude, e->serial);
			}
		}
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
	}

	/* date fudges are optional */
	if (WANT('F')) {
		d->dateFudge = atoi(&buf[2]);
		d->date += d->dateFudge;
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
	}

	/* hostnames are optional */
	if (WANT('H')) {
		if (d) d->flags &= ~D_NOHOST;
		unless (d->hostname) d = hostArg(d, &buf[2]);
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
	}

	/* Includes are optional and are specified as keys.
	 * If there is no sccs* ignore them.
	 */
	while (WANT('I')) {
		if (sc) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			unless (e) {
				fprintf(stderr, "Can't find inc %s in %s\n",
				    &buf[2], sc->sfile);
			} else {
				d->include = addSerial(d->include, e->serial);
			}
		}
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
	}

	/* cksums are optional but shouldn't be */
	if (WANT('K')) {
		d = sumArg(d, &buf[2]);
		unless (fnext(buf, f)) goto out;
		d->flags |= D_ICKSUM;
		lines++;
		chop(buf);
	}

	/* lods are optional */
	if (WANT('L')) {
		d->lod = (lod *)strdup(&buf[2]);
		d->flags |= D_LODSTR;
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
	}

	/* merge deltas are optional */
	if (WANT('M')) {
		if (sc) {
			delta	*e = sccs_findKey(sc, &buf[2]);

			unless (e) {
				fprintf(stderr, "Can't find merge %s in %s\n",
				    &buf[2], sc->sfile);
				assert(0);
			} else {
				d->merge = e->serial;
			}
		}
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
	}

	/* modes are optional */
	if (WANT('O')) {
		unless (d->mode) d = modeArg(d, &buf[2]);
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
	}

	/* pathnames are optional */
	if (WANT('P')) {
		unless (d->pathname) d = pathArg(d, &buf[2]);
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
	}

	/* symbols are optional */
	if (WANT('S')) {
		d->sym = strdup(&buf[2]);
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
	}

	/* zones are optional */
	if (WANT('Z')) {
		unless (d->zone) d = zoneArg(d, &buf[2]);
		unless (fnext(buf, f)) goto out;
		lines++;
		chop(buf);
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
	char	*iLst = malloc(fsize), *xLst = malloc(fsize);
	char	*mRev = malloc(MAXREV+1);
	char	c1 = 0, c2 = 0, c3 = 0;
	char	*t;
	int	e;
	FILE	*tmp;
	char	date[10], time[10], user[40];

	assert(fsize);
	bzero(pf, sizeof(*pf));
	unless (tmp = fopen(s->pfile, "r")) {
		fprintf(stderr, "delta: can't open %s\n", s->pfile);
		if (iLst) free(iLst);
		if (xLst) free(xLst);
		free(mRev);
		return (-1);
	}
	iLst[0] = xLst[0] = 0;
	e = fscanf(tmp, "%s %s %s %s %s -%c%s -%c%s -%c%s",
	    pf->oldrev, pf->newrev, user, date, time, &c1, iLst, &c2, xLst,
	    &c3, mRev);
	pf->user = strdup(user);
	strcpy(pf->date, date);
	strcat(pf->date, " ");
	strcat(pf->date, time);
	fclose(tmp);
	/*
	 * Symbolic revs are of the form "LOD.1(1.2.34)"
	 */
	pf->sccsrev[0] = 0;
	if (t = strchr(pf->newrev, '(')) {
		lod	*l;

		*t++ = 0;
		strcpy(pf->sccsrev, t);
		t = strchr(pf->sccsrev, ')');
		assert(t);
		*t = 0;
		t = strchr(pf->newrev, '.');
		assert(t);
		*t = 0;
		for (l = s->lods; l; l = l->next) {
			if (streq(pf->newrev, l->name)) break;
		}
		pf->l = l;
		assert(l);
		*t = '.';
	}

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
	pf->mRev= mRev;
	debug((stderr, "pfile(%s, %s, %s, %s, %s, %s, %s)\n",
	    pf->oldrev, pf->newrev, user, pf->date, iLst, xLst, mRev));
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

#ifndef	WIN32
#ifdef	ANSIC
private jmp_buf jmp;

void	abort_ci() { longjmp(jmp, 1); }
#endif

int
sccs_getComments(char *file, char *rev, delta *n)
{
	char	buf2[1024];

	fprintf(stderr,
	    "End comments with \".\" by itself, "
	    "blank line, or EOF.\n");
	assert(file);
	if (rev) {
		fprintf(stderr, "%s %s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}
#ifdef	ANSIC
	if (setjmp(jmp)) {
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		sccs_freetree(n);
		return (-1);
	}
	signal(SIGINT, abort_ci);
#else
	sig(UNBLOCK, SIGINT);
#endif
	while (fnext(buf2, stdin)) {
		char	*t;

		if (buf2[0] == '\n' || streq(buf2, ".\n"))
			break;
		/* Null the newline */
		for (t = buf2; *t; t++);
		t[-1] = 0;
		n->comments = addLine(n->comments, strdup(buf2));
		if (rev) {
			fprintf(stderr, "%s %s>>  ", file, rev);
		} else {
			fprintf(stderr, "%s>>  ", file);
		}
	}
#ifndef	ANSIC
	if (sig(CAUGHT, SIGINT)) {
		sig(BLOCK, SIGINT);
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		sccs_freetree(n);
		return (-1);
	}
#endif
	return (0);
}

int
sccs_getHostName(char *file, char *rev, delta *n)
{
	char	buf2[1024];

	assert(file);
	if (rev) {
		fprintf(stderr, "%s %s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}
#ifdef	ANSIC
	if (setjmp(jmp)) {
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		sccs_freetree(n);
		return (-1);
	}
	signal(SIGINT, abort_ci);
#else
	sig(UNBLOCK, SIGINT);
#endif
	while (fnext(buf2, stdin)) {
		char	*t;

		/* Null the newline */
		for (t = buf2; *t; t++);
		t[-1] = 0;
		if (isValidHost(buf2)) {
			n->hostname = strdup(buf2);
			break;
		}
		fprintf(stderr, "hostname of your machine>>  ");
	}
#ifndef	ANSIC
	if (sig(CAUGHT, SIGINT)) {
		sig(BLOCK, SIGINT);
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		sccs_freetree(n);
		return (-1);
	}
#endif
	return (0);
}


int
sccs_getUserName(char *file, char *rev, delta *n)
{
	char	buf2[1024];

	assert(file);
	if (rev) {
		fprintf(stderr, "%s %s>>  ", file, rev);
	} else {
		fprintf(stderr, "%s>>  ", file);
	}
#ifdef	ANSIC
	if (setjmp(jmp)) {
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		sccs_freetree(n);
		return (-1);
	}
	signal(SIGINT, abort_ci);
#else
	sig(UNBLOCK, SIGINT);
#endif
	while (fnext(buf2, stdin)) {
		char	*t;

		/* Null the newline */
		for (t = buf2; *t; t++);
		t[-1] = 0;
		if (isValidUser(buf2)) {
			n->user = strdup(buf2);
			break;
		}
		fprintf(stderr, "user name>>  ");
	}
#ifndef	ANSIC
	if (sig(CAUGHT, SIGINT)) {
		sig(BLOCK, SIGINT);
		fprintf(stderr,
		    "\nCheck in aborted due to interrupt.\n");
		sccs_freetree(n);
		return (-1);
	}
#endif
	return (0);
}
#endif	/* WIN32 */

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
sccs_meta(sccs *s, delta *parent, char *initFile)
{
	delta	*m;
	int	e = 0;
	FILE	*iF;
	FILE	*sfile = 0;
	char	*sccsXfile();
	char	*t;
	BUF	(buf);

	unless (lock(s, 'z')) {
		fprintf(stderr,
		    "meta: can't get lock on %s\n", s->sfile);
		s->state |= WARNED;
		debug((stderr, "meta returns -1\n"));
		return (-1);
	}
	unless (iF = fopen(initFile, "r")) {
		unlock(s, 'z');
		return (-1);
	}
	m = sccs_getInit(s, 0, iF, 1, &e, 0);
	fclose(iF);
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
	assert(!m->lod);

	/*
	 * Do the delta table & misc.
	 */
	unless (sfile = fopen(sccsXfile(s, 'x'), "w")) {
		fprintf(stderr, "admin: can't create %s: ", sccsXfile(s, 'x'));
		perror("");
		unlock(s, 'z');
		exit(1);
	}
	delta_table(s, sfile, 0);
	seekto(s, s->data);
	assert(s->state & SOPEN);
	while (next(buf, s)) {
		fputsum(s, buf, sfile);
	}
	fseek(sfile, 0L, SEEK_SET);
	fprintf(sfile, "\001h%05u\n", s->cksum);
	sccs_close(s); fclose(sfile); sfile = NULL;
#ifdef	PARANOID
	t = sccsXfile(s, 'S');
	unlink(t);
	rename(s->sfile, t);
#else
	unlink(s->sfile);		/* Careful. */
#endif
	t = sccsXfile(s, 'x');
	if (rename(t, s->sfile)) {
		fprintf(stderr,
		    "takepatch: can't rename(%s, %s) left in %s\n",
		    t, s->sfile, t);
		unlock(s, 'z');
		exit(1);
	}
	Chmod(s->sfile, 0444);
	unlock(s, 'z');
	return (0);
}

/*
 * delta the specified file.
 *
 * Init file implies the old NODEFAULT flag, i.e., if there was an init
 * file, that is the truth.  The truth can be over ridden with a prefilled
 * delta but even that is questionable.
 */
int
sccs_delta(sccs *s, int flags, delta *prefilled, FILE *init, FILE *diffs)
{
	FILE	*sfile = 0;	/* the new s.file */
	int	error = 0;
	char	*t;
	delta	*d = 0, *n = 0;
	char	*tmpfile = tmpnam(0);
	int	added, deleted, unchanged;
	int	locked;
	char	buf2[1024];
	pfile	pf;

	assert(s);
	debug((stderr, "delta %s %x\n", s->gfile, flags));
	if (flags & NEWFILE) mksccsdir(s);
	bzero(&pf, sizeof(pf));
	unless(locked = lock(s, 'z')) {
		fprintf(stderr, "delta: can't get lock on %s\n", s->sfile);
		error = -1; s->state |= WARNED;
out:
		if (prefilled) sccs_freetree(prefilled);
		if (sfile) fclose(sfile);
		if (diffs) fclose(diffs);
		free_pfile(&pf);
		unlink(tmpfile);
		if (locked) unlock(s, 'z');
		debug((stderr, "delta returns %d\n", error));
		return (error);
	}
#define	OUT	{ error = -1; s->state |= WARNED; goto out; }

	if (init) {
		int	e;

		prefilled =
		    sccs_getInit(s, prefilled, init, flags&PATCH, &e, 0);
		unless (prefilled && !e) {
			fprintf(stderr, "delta: bad init file\n");
			goto out;
		}
		debug((stderr, "delta got prefilled %s\n", prefilled->rev));
		if ((flags & PATCH) && !(s->state & ONE_ZERO)) {
			free(prefilled->rev);
			prefilled->rev = 0;
		}
	}

	if ((flags & NEWFILE) || (!HAS_SFILE(s) && HAS_GFILE(s))) {
		return (checkin(s, flags, prefilled, init != 0, diffs));
	}

	if (!HAS_PFILE(s) && HAS_SFILE(s) && HAS_GFILE(s) && IS_WRITABLE(s)) {
		fprintf(stderr,
		    "delta: %s writable but not checked out?\n", s->gfile);
		s->state |= WARNED;
		OUT;
	}
	if (HAS_GFILE(s)) {
		if (diffs) {
			fprintf(stderr,
			    "delta: diffs or gfile, but not both.\n");
			s->state |= WARNED;
			goto out;
		}
	} else unless (diffs) {
		goto out;
	}
	unless (IS_WRITABLE(s) || diffs) {
		unless (HAS_PFILE(s)) {
			verbose((stderr, "Clean %s (not edited)\n", s->gfile));
			unedit(s, flags);
			goto out;
		}
		verbose((stderr, "delta: %s locked but not writable?\n",
		    s->gfile));
		OUT;
	}
	unless (s->tree) {
		fprintf(stderr, "delta: bad delta table in %s\n", s->sfile);
		OUT;
	}

	/*
	 * OK, checking done, start the delta.
	 */
	if (read_pfile("delta", s, &pf)) OUT;
	unless (d = findrev(s, pf.oldrev)) {
		fprintf(stderr,
		    "delta: can't find %s in %s\n", pf.oldrev, s->gfile);
		OUT;
	}
	if (pf.mRev) flags |= FORCE;
	debug((stderr, "delta found rev\n"));
	unless (diffs) {
		switch (diff_gfile(s, &pf, tmpfile)) {
		    case 1:		/* no diffs */
						    /* CSTYLED */
			if (flags & FORCE) break;     /* forced 0 sized delta */
			if (!(flags & SILENT))
				fprintf(stderr,
				    "Clean %s (no diffs)\n", s->gfile);
			unedit(s, flags);
			goto out;
		    case 0:		/* diffs */
			break;
		    default: OUT;
		}
		unless (diffs = fopen(tmpfile, "rt")) { /* open in text mode */
			fprintf(stderr,
			    "delta: can't open diff file %s\n", tmpfile);
			OUT;
		}
	} else {
		debug((stderr, "delta using diffs passed in\n"));
	}
	if (flags & PRINT) {
		fprintf(stdout, "==== Changes to %s ====\n", s->gfile);
		while (fnext(buf2, diffs)) fputs(buf2, stdout);
		fputs("====\n\n", stdout);
		fseek(diffs, 0L, SEEK_SET);
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
	    !(flags & DONTASK) && !(n->flags & D_NOCOMMENTS)) {
		/*
		 * XXX - andrew make sure host/user is correct right here.
		 */
		if (sccs_getComments(s->gfile, pf.newrev, n)) OUT;
	}
	if (pf.l) {
		n->lod = pf.l;
		n->flags |= D_DUPLOD;
		if ((d->lod != pf.l) || !samebranch(d, n)) {
			pf.l->heads = addSerial(pf.l->heads, n->serial);
		}
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
	 * Do the delta table & misc.
	 */
	unless (sfile = fopen(sccsXfile(s, 'x'), "w")) {
		fprintf(stderr, "delta: can't create %s: ", sccsXfile(s, 'x'));
		perror("");
		OUT;
	}
	delta_table(s, sfile, 1);

	assert(d);
	if (delta_body(s, n, diffs, sfile, &added, &deleted, &unchanged)) {
		OUT;
	}
	end(s, n, sfile, flags, added, deleted, unchanged);

	sccs_close(s), fclose(sfile), sfile = NULL;
#ifdef	PARANOID
	t = sccsXfile(s, 'S');
	unlink(t);
	rename(s->sfile, t);
	t = sccsXfile(s, 'G');
	unless (flags&SAVEGFILE) {
		rename(s->gfile, t);
		s->state &= ~WRITABLE;
	}
#else
	unlink(s->sfile);		/* Careful. */
	unless (flags&SAVEGFILE) {
		unlinkGfile(s);		/* Careful */
	}
#endif
	t = sccsXfile(s, 'x');
	if (rename(t, s->sfile)) {
		fprintf(stderr,
		    "delta: can't rename(%s, %s) left in %s\n",
		    t, s->sfile, t);
		OUT;
	}
	Chmod(s->sfile, 0444);
	unlink(s->pfile);
	if (s->state & BITKEEPER) updatePending(s, n);
	goto out;
}

/*
 * Print the summary and go and fix up the top.
 */
private void
end(sccs *s, delta *n, FILE *out, int flags, int add, int del, int same)
{
	char	buf[100];

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
	fputsum(s, buf, out);
	if (s->state & BITKEEPER) {
		if ((add || del || same) && (n->flags & D_ICKSUM)) {
			assert(s->dsum == n->sum);
		}
		unless (n->flags & D_ICKSUM) {
			if (!add && !del && !same) {
				n->sum = almostUnique();
			} else {
				n->sum = s->dsum;
			}
		}
		fseek(out, s->sumOff, SEEK_SET);
		sprintf(buf, "%05u", n->sum);
		fputsum(s, buf, out);
	}
	fseek(out, 0L, SEEK_SET);
	fprintf(out, "\001h%05u\n", s->cksum);
}

/*
 * diffs - diff the gfile or the specified (or implied) rev
 */
int
sccs_diffs(sccs *s, char *r1, char *r2, int flags, char kind, FILE *out)
{
	FILE	*diffs = 0;
	char	*left, *right;
	char	*leftf, *rightf;
	char	tmpfile[30];
	char	diffFile[30];
	char	*columns;
	char	tmp2[32];
	char	buf[1024];
	pfile	pf;
	int	first = 1;
	char	spaces[80];

	bzero(&pf, sizeof(pf));
	GOODSCCS(s);
	if (kind == D_SDIFF) {
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
			s->state |= WARNED;
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
	sprintf(tmpfile, "%s/diffget%d", TMP_PATH, getpid());
	if (sccs_get(s, left, pf.mRev, pf.iLst, pf.xLst, flags|SILENT|PRINT, tmpfile)) {
		unlink(tmpfile);
		free_pfile(&pf);
		return (-1);
	}
	if (r2 || !HAS_GFILE(s)) {
		sprintf(tmp2, "%s-2", tmpfile);
		if (sccs_get(s, right, 0, 0, 0, flags|SILENT|PRINT, tmp2)) {
			unlink(tmpfile);
			unlink(tmp2);
			free_pfile(&pf);
			return (-1);
		}
		leftf = tmpfile;
		rightf = tmp2;
	} else {
		tmp2[0] = 0;
		leftf = tmpfile;
		rightf = s->gfile;
	}
	if (!right) right = findrev(s, 0)->rev;
	if (!left) left = findrev(s, 0)->rev;
	if (kind == D_SDIFF) {
		int	i, c = atoi(columns);

		for (i = 0; i < c/2 - 18; ) spaces[i++] = '=';
		spaces[i] = 0;
		sprintf(buf, "%s -w%s %s %s", SDIFF, columns, leftf, rightf);
		diffs = fastPopen(buf, "rt");
		if (!diffs) {
			unlink(tmpfile);
			if (tmp2[0]) unlink(tmp2);
			free_pfile(&pf);
			return (-1);
		}
		diffFile[0] = 0;
	} else {
		strcpy(spaces, "=====");
		sprintf(diffFile, "%s/diffs%d", TMP_PATH, getpid());
		diff(leftf, rightf, kind, diffFile);
		diffs = fopen(diffFile, "rt");
	}
	while (fnext(buf, diffs)) {
		if (first) {
			fprintf(out, "%s %s %s vs %s %s\n",
			    spaces, s->gfile, left, right, spaces);
			first = 0;
		}
		fputs(buf, out);
	}
	if (kind == D_SDIFF) {
		fastPclose(diffs);
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
	private void	fprintDelta(FILE *, char *, const char *, const char *,
				    sccs *, delta *);
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
		/* serial number of included, exclude and ignoed deltas */
		/* :DI: = :Dn:/:Dx:/:Dg: */
		if (!out) return (strVal);
		KW("Dn"); fc('/'); KW("Dx"); fc('/'); KW("Dg");
		return (strVal);
	}

	if (streq(kw, "Dn")) {
		/* serial number of included deltas */
		int i;
		EACH(d->include) {
			fd(d->include[i]);
			if (i > 0) fc('~');
		}
		if (i) return (strVal);
		return (nullVal);
	}

	if (streq(kw, "Dx")) {
		/* serial number of excluded deltas */
		int i;
		EACH(d->exclude) {
			fd(d->exclude[i]);
			if (i > 0) fc('~');
		}
		if (i) return (strVal);
		return (nullVal);
	}

	if (streq(kw, "Dg")) {
		/* ignored delta - definition unknow, not implemented	*/
		/* always return null					*/
		return (nullVal);
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

			if (s->state & YEAR4) {
				q = &val[2];
			} else {
				q = val;
			}
			for (p = d->sdate; *p && *p != '/'; )
				*q++ = *p++;
			*q = '\0';
			if (s->state & YEAR4) {
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
		fs(s->gfile);
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
		sccs_get(s, d->rev, 0, 0, 0, EXPAND|SILENT|PRINT, "-");  
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
		char	buf[MAXREV];

		if (d->r[2]) {
			sprintf(buf,
			    "%d.%d.%d.%d", d->r[0], d->r[1], d->r[2], d->r[3]);
		} else {
			sprintf(buf, "%d.%d", d->r[0], d->r[1]);
		}
		fs(buf);
		return (strVal);
	}

	if (streq(kw, "LOD")) {
		if (d->lod) {
			fs(d->lod->name);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "KEY")) {
		if (out) sccs_pdelta(sccs_ino(s), out);
		return (strVal);
	}

	if (streq(kw, "ID")) {
		if (out) sccs_pdelta(d, out);
		return (strVal);
	}

	if (streq(kw, "HT")) {
		/* host */
		if (d->hostname) {
			fs(d->hostname);
			return (strVal);
		}
		return (nullVal);
	}

	if (streq(kw, "TZ")) {
		/* time zone */
		if (d->zone) {
			fs(d->zone);
			return (strVal);
		}
		return (nullVal);
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

	if (streq(kw, "MODE")) {
		char	buf[20];

		sprintf(buf, "%o", (int)d->mode);
		fs(buf);
		return (strVal);
	}

	if (streq(kw, "TYPE")) {
		if (s->state & BITKEEPER) { 
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
	int	len;

	/*
	 * XXX TODO, this does not support
	 * compound statement inside $each{...} yet
	 * We may need to support this later
	 */
	for (t = b; t <= end; t++) {
		while ((t <= end) && (*t != '(')) t++;
		if ((t <= end) && !strncmp(&t[1], kwbuf, strlen(kwbuf)) &&
		    t[strlen(kwbuf) + 1] == ')') {
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
				for (t = b; *t && *t != '}'; t++);
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
		    (q[4] == 'h') && (q[5] == '(')) {
			const char *prefix, *suffix;
			int	plen, slen;
			b = &q[6];
			len = extractKeyword(b, end, ")", kwbuf);
			if (len < 0) { return (printLF); } /* error */
			if (b[len + 1] != '{') {
				/* syntax error */
				fprintf(stderr,
			    "must have '{' in conditional prefix/suffix\n");
				return (printLF);
			}
			prefix = b = &b[len + 2];
			plen = extractPrefix(b, end, kwbuf);
			suffix = &b[plen+len+2];
			slen = extractSuffix(&b[plen+len+2], end);
			kw2val(
			    out, NULL, prefix, plen, kwbuf, suffix, slen, s, d);
			q = &b[plen + len + slen + 3];
		} else {
			fc(*q++);
		}
	}
	return (printLF);
}

private void
do_prs(sccs *s, delta *d, int flags, const char *dspec, FILE *out)
{
	const char *end;

	if (d->type != 'D') return;
	if (fprintDelta(
		    out, NULL,  dspec, end = &dspec[strlen(dspec) - 1], s, d))
		fputc('\n', out);
}

private void
do_patch(sccs *s, delta *start, delta *stop, int flags, FILE *out)
{
	int	i;	/* used by EACH */
	lod	*lod;
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
	type =start->type;
	if ((start->type == 'R') &&
	    start->parent && streq(start->rev, start->parent->rev)) {
	    	type = 'M';
	}
	fprintf(out, "%c %s %s%s %s%s%s +%d -%d\n",
	    type, start->rev, start->sdate,
	    start->zone ? start->zone : "",
	    start->user,
	    start->hostname ? "@" : "",
	    start->hostname ? start->hostname : "",
	    start->added, start->deleted);
	EACH(start->comments) {
		assert(start->comments[i][0] != '\001');
		fprintf(out, "C %s\n", start->comments[i]);
	}
	EACH(start->exclude) {
		delta	*d = sfind(s, start->exclude[i]);
		assert(d);
		fprintf(out, "E ");
		sccs_pdelta(d, out);
		fprintf(out, "\n");
	}
	if (start->dateFudge) {
		fprintf(out, "F %d\n", (int)start->dateFudge);
	}
	EACH(start->include) {
		delta	*d = sfind(s, start->include[i]);
		assert(d);
		fprintf(out, "I ");
		sccs_pdelta(d, out);
		fprintf(out, "\n");
	}
	if (start->flags & D_CKSUM) {
		fprintf(out, "K %u\n", start->sum);
	}
	if (start->flags & D_DUPLOD) {
		fprintf(out, "L %s\n", start->lod->name);
	}
	if (start->merge) {
		delta	*d = sfind(s, start->merge);
		assert(d);
		fprintf(out, "M ");
		sccs_pdelta(d, out);
		fprintf(out, "\n");
	}
	if (start->flags & D_MODE) fprintf(out, "O %o\n", start->mode);
	if (s->tree->pathname) assert(start->pathname);
	if (start->pathname) fprintf(out, "P %s\n", start->pathname);
	if (start->flags & D_SYMBOLS) {
		for (sym = s->symbols; sym; sym = sym->next) {
			unless (sym->reald == start) continue;
			fprintf(out, "S %s\n", sym->name);
		}
	}
	if (s->tree->zone) assert(start->zone);
	fprintf(out, "------------------------------------------------\n");
}

int
sccs_prs(sccs *s, int flags, char *dspec, FILE *out)
{
	delta	*d;
#define	DEFAULT_DSPEC \
"D :I: :D: :T::TZ: :P:$if(:HT:){@:HT:} :DS: :DP: :Li:/:Ld:/:Lu:\n\
$if(:DPN:){P :DPN:\n}$each(C){C (C)}\n\
------------------------------------------------"

	if (!dspec) dspec = DEFAULT_DSPEC;
	GOODSCCS(s);
	if (flags & PATCH) {
		do_patch(s,
		    s->rstart ? s->rstart : s->tree,
		    s->rstop ? s->rstop : 0, flags, out);
		return (0);
	}
	/* print metadata if they asked */
	unless (flags & SILENT) {
		symbol	*sym;
		lod	*l;

		for (l = s->lods; l; l = l->next) {
			fprintf(out, "L %s, parent is %s\n",
			    l->name, sccsrev(l->d));
		}
		for (sym = s->symbols; sym; sym = sym->next) {
			fprintf(out, "S %s %s\n", sym->name, sym->rev);
		}
	}

	for (d = s->rstop; d; d = d->next) {
		do_prs(s, d, flags, dspec, out);
		if (d == s->rstart) break;
	}
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
private int
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

	left = sccs_init(lfile, 0);
	right = sccs_init(rfile, 0);
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
samekey(delta *d, char *user, char *host, char *path, time_t date)
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
//printf("MATCH\n");
	return (1);
}

static inline int
isleaf(register delta *d)
{
	if (d->type != 'D') return (0);
	for (d = d->kid; d; d = d->siblings) {
		if (d->type == 'D') return (0);
	}
	return (1);
}

/*
 * Create resolve file.
 * The order of the deltas in the file is important - the "branch"
 * should be last.
 * This currently only works for the trunk (i.e., there is one LOD).
 * XXX - this is also where we would handle pathnames, symbols, etc.
 */
int
sccs_resolveFile(sccs *s, char *lpath, char *gpath, char *rpath)
{
	FILE	*f = 0;
	delta	*d, *a = 0, *b = 0;
	char	*n[3];
	
	for (d = s->table; d; d = d->next) {
		d->flags &= ~D_VISITED;
		if ((d->flags & D_MERGED) || !isleaf(d)) continue;
		if (!a) {
			a = d;
		} else {
			assert(!b);
			b = d;
			/* Could break but I like the error checking */
		}
	}
	if (b) {
		/* find the GCA and put it in d */
		for (d = b; d; d = d->parent) d->flags |= D_VISITED;
		for (d = a; d; d = d->parent) {
			if (d->flags & D_VISITED) break;
		}
		assert(d);
		unless (f = fopen(sccsXfile(s, 'r'), "w")) {
			perror("r.file");
			return (-1);
		}
		if (samebranch(d, a)) {
			fprintf(f, "merge deltas %s %s %s %s %s\n",
				a->rev, d->rev, b->rev, getuser(), now());
		} else {
			fprintf(f, "merge deltas %s %s %s %s %s\n",
				b->rev, d->rev, a->rev, getuser(), now());
		}
		fclose(f);
		if (lpath) {
			unless (f = fopen(sccsXfile(s, 'R'), "w")) {
				perror("R.file");
				return (-1);
			}
			n[0] = name2sccs(lpath);
			n[1] = name2sccs(gpath);
			n[2] = name2sccs(rpath);
			fprintf(f, "rename %s %s %s\n", n[0], n[1], n[2]);
			fclose(f);
			free(n[0]);
			free(n[1]);
			free(n[2]);
		}
		return (1);
	}
	if (lpath) {
		unless (f = fopen(sccsXfile(s, 'R'), "w")) {
			perror("R.file");
			return (-1);
		}
		n[0] = name2sccs(lpath);
		n[1] = name2sccs(gpath);
		n[2] = name2sccs(rpath);
		fprintf(f, "rename %s %s %s\n", n[0], n[1], n[2]);
		fclose(f);
		free(n[0]);
		free(n[1]);
		free(n[2]);
	}
	return (0);
}

/*
 * Take a key like sccs_sdelta makes and find it in the tree.
 */
delta *
sccs_findKey(sccs *s, char *key)
{
	char	*parts[4];	/* user, host, path, date as integer */
	char	*user, *host, *path;
	time_t	date;
	delta	*e;
	char	buf[1024];

	unless (s->tree) return (0);
//printf("findkey(%s)\n", key);
	strcpy(buf, key);
	explodeKey(buf, parts);
	user = parts[0];
	host = parts[1];
	path = parts[2];
	date = date2time(&parts[3][2], 0, EXACT);
	if (samekey(s->tree, user, host, path, date)) return (s->tree);
	for (e = s->table;
	    e && !samekey(e, user, host, path, date); e = e->next);
	return (e);
}

void
sccs_print(delta *d)
{
	fprintf(stderr, "%c %s %s%s %s%s%s %d %d %d/%d/%d %s 0x%x\n",
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
sccs_pdelta(delta *d, FILE *out)
{
	assert(d);
	fprintf(out, "%s%s%s|%s|%s",
	    d->user,
	    d->hostname ? "@" : "",
	    d->hostname ? d->hostname : "",
	    d->pathname ? d->pathname : "",
	    sccs_utctime(d));
}

void
sccs_sdelta(char *buf, delta *d)
{
	assert(d);
	sprintf(buf, "%s%s%s|%s|%s",
	    d->user,
	    d->hostname ? "@" : "",
	    d->hostname ? d->hostname : "",
	    d->pathname ? d->pathname : "",
	    sccs_utctime(d));
}

/*
 * Take in a string like what pdelta spits out and break it into the
 * parts.
 */
void
explodeKey(char *key, char *parts[4])
{
	char	*s;

	/* user@host|sccs/slib.c|19970518232929 */
	for (s = key; *key && (*key != '|'); key++);
	parts[0] = s;
	*key++ = 0;
	for (s = key; *key && (*key != '|'); key++);
	parts[2] = s == key ? 0 : s;
	*key++ = 0;
	parts[3] = key;
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

MDBM	*
loadDB(char *file, int (*want)(char *))
{
	MDBM	*DB = 0;
	FILE	*f = 0;
	char	*v;
	char	buf[1024];
	int	first = 1;

again:	unless (f = fopen(file, "rt")) {
		if (first) {
			first = 0;
			fprintf(stderr, "Rebuilding caches...\n");
			system("bk sfiles -r");
			goto again;
		}
out:		if (f) fclose(f);
		if (DB) mdbm_close(DB);
		return (0);
	}
	DB = mdbm_open(NULL, 0, 0, 4096);
	assert(DB);
	mdbm_pre_split(DB, 1<<10);
	while (fnext(buf, f)) {
		if (buf[0] == '#') continue;
		if (want && !want(buf)) continue;
		if (chop(buf) != '\n') {
			fprintf(stderr, "bad path: <%s>\n", buf);
			assert("pathname overflow in cache" == 0);
		}
		v = strchr(buf, ' ');
		assert(v);
		*v++ = 0;
		if (mdbm_store_str(DB, buf, v, MDBM_INSERT)) {
			fprintf(stderr,
			    "Duplicate name '%s' in %s.\n", buf, file);
			goto out;
		}
	}
	fclose(f);
	return (DB);
}

int
findpipe(register char *s)
{
	while (*s) if (*s++ == '|') return (1);
	return (0);
}

/*
 * Get all the ids associated with a changeset.
 * The db is db{fileId} = csetId.
 *
 * Note: does not call sccs_restart, the caller of this sets up "s".
 */
MDBM	*
csetIds(sccs *s, char *rev, int all)
{
	FILE	*f;
	MDBM	*db;
	char	name[1024];

	sprintf(name, "/tmp/cs%d", getpid());
	if (all) {
all:		if (sccs_get(s, rev, 0, 0, 0, SILENT|PRINT, name)) {
			sccs_whynot("get", s);
			exit(1);
		}
	} else {
		delta	*d;

		unless (d = findrev(s, rev)) {
			perror(rev);
			exit(1);
		}
		unless (d->parent) goto all;
		f = fopen(name, "wt");
		if (sccs_diffs(s, d->parent->rev, rev, SILENT, D_RCS, f)) {
			sccs_whynot("get", s);
			exit(1);
		}
		fclose(f);
	}
	db = loadDB(name, findpipe);
	unlink(name);
	return (db);
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
sccs_ids(sccs *s, int flags, FILE *out)
{
	delta	*d = s->tree;

	sccs_pdelta(sccs_ino(s), out);
	if (flags & TOP) {
		fprintf(out, "\n");
		return;
	}
	for (d = s->table; d; d = d->next) {
		if (!d->kid && (d->type == 'D')) {
			fprintf(out, " ");
			sccs_pdelta(d, out);
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
 * RMDEL - remove delta if the weather is right
 *
 * This code from Rick Smith.
 */
int
sccs_rmdel(sccs *s, char *rev, int destroy, int flags)
{
	FILE	*sfile = 0;
	int	error = 0;
	char	*t;
	delta	*d = 0;
	int	locked;
	pfile	pf;

	assert(s);
	debug((stderr, "rmdel %s %x\n", s->gfile, flags));
	bzero(&pf, sizeof (pf));
	unless(locked = lock(s, 'z')) {
		fprintf(stderr, "rmdel: can't get lock on %s\n", s->sfile);
		error = -1; s->state |= WARNED;
rmdelout:
		if (sfile) fclose(sfile);
		free_pfile(&pf);
		if (locked) unlock(s, 'z');
		debug((stderr, "rmdel returns %d\n", error));
		return (error);
	}
#define	RMDELOUT	\
	do { error = -1; s->state |= WARNED; goto rmdelout; } while (0)

	if (!HAS_SFILE(s)) {
		fprintf(stderr, "rmdel: no sfile\n");
		RMDELOUT;
	}

	unless (s->tree) {
		fprintf(stderr, "rmdel: bad delta table in %s\n", s->sfile);
		RMDELOUT;
	}

	unless (d = findrev(s, rev)) {
		fprintf(stderr, "rmdel: can't find revision like %s in %s\n",
			rev, s->sfile);
		RMDELOUT;
	}

	if (HAS_PFILE(s)) {
		if (read_pfile("rmdel", s, &pf)) RMDELOUT;
		if (streq(d->rev, pf.oldrev)) {
			fprintf(stderr,
				"rmdel: revision %s is locked in %s.\n"
				"       Nothing removed.\n",
				d->rev, s->sfile);
			RMDELOUT;
		}
	}

	if (destroy ? delta_destroy(s, d) : delta_rmchk(s, d))
		RMDELOUT;

	if (!destroy)  {
		d->type = 'R';	/* mark delta as Removed */
		d->flags &= ~D_CKSUM;
	}

	/*
	 * OK, checking or destroying done.
	 * Write out the new s.file.
	 */
	unless (sfile = fopen(sccsXfile(s, 'x'), "w")) {
		fprintf(stderr, "rmdel: can't create %s: ", sccsXfile(s, 'x'));
		perror("");
		RMDELOUT;
	}

	/* write out upper half */
	delta_table(s, sfile, 0);  /* 0 means as-is, so checksum works */

	/* write out lower half */
	if (destroy ? delta_strip(s, d, sfile, flags)
		    : delta_rm(s, d, sfile, flags)) {
		RMDELOUT;
	}

	fseek(sfile, 0L, SEEK_SET);
	fprintf(sfile, "\001h%05u\n", s->cksum);

	sccs_close(s), fclose(sfile), sfile = NULL;
#ifdef	PARANOID
	t = sccsXfile(s, 'S');
	unlink(t);
	rename(s->sfile, t);
#else
	unlink(s->sfile);		/* Careful. */
#endif
	t = sccsXfile(s, 'x');
	if (rename(t, s->sfile)) {
		fprintf(stderr,
		    "rmdel: can't rename(%s, %s) left in %s\n",
		    t, s->sfile, t);
		RMDELOUT;
	}
	Chmod(s->sfile, 0444);
	goto rmdelout;
}

/*
 * Make sure it is OK to remove a delta.
 */
private int
delta_rmchk(sccs *s, delta *d)
{
	delta	*n;
	int	i, found;
	char	*user = getuser();

	/*
	 * Reject the command if not by the same user who added the delta.
	 * - must own the delta
	 * - must be the tip of a branch (O'Reilly, HP-UX)
	 * - must not be included or excluded by other delta (me)
	 * - must not be rev 1.1 (more precisely: no parent) (me)
	 */
	unless (streq(d->user, user)) {
		fprintf(stderr,
		    "rmdel: hey, %s, only %s can rmdel %s from %s\n",
			    user, d->user, d->rev, s->sfile);
		return (1);
	}

	/* Must be tip of branch */
	for (n = d->kid; n; n = n->siblings) {
		if (n->type == 'D') {
			fprintf(stderr,
			    "rmdel: revision %s not at tip of branch in %s.\n",
			    d->rev, s->sfile);
			return (1);
		}
	}

	/* Can't be included or excluded by any existing delta */
	for (n = s->table; n; n = n->next) {
		unless (n->type == 'D') continue;
		found = 0;
		EACH(n->include) {
			if (n->include[i] == d->serial) {
				found++;
				break;
			}
		}
		EACH(n->exclude) {
			if (n->exclude[i] == d->serial) {
				found++;
				break;
			}
		}
		if (found) {
			fprintf(stderr,
			    "rmdel: rev %s of %s is in inc/exc list of %s\n",
			    d->rev, s->sfile, n->rev);
			return (1);
		}
	}

	/*
	 * Do not let them remove the root.
	 */
	unless (d->parent) {	/* don't remove if this is 1.1 (no parent) */
		fprintf(stderr,
			"rmdel: can't remove root change %s in %s.\n"
			"       Nothing removed.\n", d->rev, s->sfile);
		return (1);
	}
	return (0);
}

private int
delta_rm(sccs *s, delta *d, FILE *sfile, int flags)
{
	register BUF	(buf);
	ser_t 		serial = d->serial;

	/* XXX: What kind of graceful failure would be desired?
	 * 	Now is assert or die
	 */

	while (!eof(s)) {
		if (!next(buf, s)) break;

		if (buf[0] == '\001' && serial == atoi(&buf[3])) {
			register int checker = 0;
			register int skip = 0;
			if (buf[1] == 'D' || buf[1] == 'E') continue;
			assert(buf[1] == 'I');
			while (next(buf, s)) {
				checker = 1;
				if (buf[0] == '\001') {
					ser_t	num;
					num = atoi(&buf[3]);
					assert(num);
					if (num == serial) break;
					if (num == skip) {
						assert(buf[1] == 'E');
						skip = 0;
					}
					if (!skip && buf[1] == 'I')
						skip = num;
					fputsum(s, buf, sfile);
				} else if (skip) {
					fputsum(s, buf, sfile);
				}
				checker = 0;
			}
			assert(checker); /* eof may not be good enough */
			assert(buf[1] == 'E' && serial == atoi(&buf[3]));
			continue;
		}
		fputsum(s, buf, sfile);
	}
	return 0;
}

/*
 * Remove all deltas after the specified delta.
 * After means in table order, not graph order.
 */
private int
delta_destroy(sccs *s, delta *d)
{
	delta	*e;

	/*
	 * Mark all the nodes we want gone.
	 * delta_table() respects the D_GONE flag.
	 */
	for (e = s->table; e != d; e = e->next) {
		assert(e);
		e->flags |= D_GONE;
	}
	s->table = d;
	return 0;
}

private int
delta_strip(sccs *s, delta *d, FILE *sfile, int flags)
{
	register BUF	(buf);
	ser_t		serial = d->serial;
	ser_t		stop;

	/* algo: strip every serial > base_serial
	 * 	eat *everything* between I serial and E serial
	 *	(only serials of greater number can be contained within)
	 *	remove all D and E serial outside of this.
	 */

	while (!eof(s) && next(buf, s)) {
		if (buf[0] == '\001' && (stop = atoi(&buf[3])) > serial) {
			if (buf[1] == 'I') {
				while (!eof(s) && next(buf, s)) {
					if ((buf[0] == '\001') &&
					    (atoi(&buf[3]) == stop)) {
						assert(buf[1] == 'E');
						break;
					    }
				}
				continue;
			}
			assert(buf[1] == 'D' || buf[1] == 'E');
			continue;
		}
		fputsum(s, buf, sfile);
	}
	return 0;
}

int
smartUnlink(char *file)
{
	int rc;
	extern int errno;
#undef	unlink
	if ((rc = unlink(file))) {
		chmod(file, S_IWRITE);
		rc = unlink(file);
	}
	if ((rc) && (!access(file, 0))) {
		fprintf(stderr,
			"smartUnlink:can not unlink %s, errno = %d\n",
			file, errno);
	}
	return (rc);
}

int
smartRename(char * old, char *new)
{
	int rc;
	extern int errno;
#undef	rename
	if ((rc = rename(old, new))) {
		if (smartUnlink(new)) {
			debug((stderr,
				"smartRename: unlink fail for %s, errno=%d\n",
				new, errno));
			return (rc);
		}
		rc = rename(old, new);
	}
	if (rc < 0) {
		fprintf(stderr,
		    "smartRename: can not rename from %s to %s, errno=%d\n",
			old, new, errno);
	}
	return (rc);
}
