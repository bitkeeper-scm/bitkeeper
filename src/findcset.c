#include "system.h"
#include "sccs.h"
#include "range.h"
WHATSTR("@(#)%K%");

#define	MINUTE	(60)
#define	HOUR	(60*60)
#define	DAY	(24*HOUR)
#define	MONTH	(30*DAY)
#define	YEAR	(365*DAY)
#define	MIN_GAP	(2*HOUR)
#define	MAX_GAP	(30*DAY)

#define	f_index merge		/* Overload d->merge with d->f_index */

private	int	compar(const void *a, const void *b);
private	void	sortDelta(int flags);
private	void	findcset(void);
private	void	mkList(sccs *s, int fileIdx);
private	void	freeList(void);
private void	dumpCsetMark(void);
private delta	*findFirstDelta(sccs *s, delta *first);
private void	mkDeterministicKeys(void);
private	int	openTags(char *tagfile);
private	char *	readTag(time_t *tagDate);
private	void	closeTags(void);

private	delta	*list, *freeme, **sorted;
private	char	**flist, **keylist;
private	MDBM	*csetBoundary;
private	int	n;

typedef	struct {
	u32	timeGap;		/* Time gap (in minutes) */
	u32	singleUserCset:1;	/* Force one user per cset */
	u32	noSkip:1;		/* do not skip recent deltas */
	u32	verbose;		/* 1: basic, 2:debug */
	u32	ignoreCsetMarker:1;	/*
					 * Strip/re-do existing
					 * Cset & Cset Marker
					 * for debugging only
					 */
	u32	blackOutTime;		/*
					 * Ignore delta Younger then
					 * "blackOutTime" (in month)
					 * for debugging only.
					 */
} fopts;

fopts	opts;
delta	*oldest;			/* The oldest user delta */


int
findcset_main(int ac, char **av)
{
	sccs	*s;
	char	*name;
	char	*tagFile = 0;
	char	*cmd;
	char	key[MAXKEY];
	int	save, c, flags = SILENT;
	int	fileIdx;

	debug_main(av);
	if (ac == 2 && streq("--help", av[1])) {
		//system("bk help findcset");
		return (0);
	}

	while ((c = getopt(ac, av, "b:kt:T:ivu")) != -1) {
		switch (c)  {
		    case 'b' : opts.blackOutTime = atoi(optarg); break;
		    case 'k'  : opts.noSkip++; break;
		    case 't' : opts.timeGap = atoi(optarg); break;
		    case 'T' : tagFile = optarg; break;
		    case 'i' : opts.ignoreCsetMarker = 1; break; /* for debug */
		    case 'u' : opts.singleUserCset = 1; break;
		    case 'v' : opts.verbose++; flags &= ~SILENT; break;
		    default:
			//system("bk help findcset");
			return (1);
		}
	}

	has_proj("findcset");

	if (tagFile && openTags(tagFile)) return (1);

	/*
	 * XXX TODO
	 * If -i option is set, we need to:
	 * a) clear D_CSET mark for all delta after 1.1 in the ChangeSet file
	 * b) strip for all delta after 1.1 in the ChangeSet file
	 */
	if (opts.ignoreCsetMarker) {
		delta	*d;
		int	didone = 0;

		s = sccs_init(CHANGESET, INIT_NOCKSUM|flags);
		assert(s);
		for (d = s->table; d; d = d->next) {
			if (!streq(d->rev, "1.0") && !streq(d->rev, "1.1")) {
				d->flags &= ~D_CSET;
				d->flags |= D_SET|D_GONE;
				didone = 1;
			}
		}
		if (didone) {
			if (sccs_stripdel(s, "findcset")) {
				sccs_free(s);
				return (1);
			}
		}
		sccs_free(s);
	}

	fileIdx = 0;
	csetBoundary = mdbm_mem();
	for (name = sfileFirst("findcset", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, INIT_NOCKSUM|flags)) {
			continue;
		}
		unless (HASGRAPH(s)) continue;
		unless (sccs_userfile(s)) {
			sccs_free(s);
			verbose((stderr, "Skipping non-user file %s.\n", name));
			continue;
		}
		sccs_close(s);
		unless (HASGRAPH(s)) goto next;
		save = n;
		sccs_sdelta(s, sccs_ino(s), key);
		keylist = addLine(keylist, strdup(key));
		flist = addLine(flist, strdup(s->sfile));
		oldest = findFirstDelta(s, oldest);
		mkList(s, ++fileIdx);
		//verbose((stderr, "%s: %d deltas\n", s->sfile, n - save));
next:		sccs_free(s);
	}
	sfileDone();
	verbose((stderr, "Total %d deltas\n", n));
	if (n) {
		sortDelta(flags);
		mkDeterministicKeys();
		findcset();
		dumpCsetMark();
		freeList();
	}
	closeTags();
	mdbm_close(csetBoundary);
	sysio(NULL, DEV_NULL, DEV_NULL, "bk", "-R", "sfiles", "-P", SYS);

	/* update rootkey embedded in files */
	unlink("BitKeeper/tmp/ROOTKEY");
	proj_reset(0);
	cmd = aprintf("bk -r admin -C'%s'", proj_rootkey(0));
 	if (system(cmd)) {
 		fprintf(stderr, "bk -r admin -Cid: failed\n");
 		exit(1);
	}
	free(cmd);
	return (0);
}

private	int
compar(const void *a, const void *b)
{
	register	delta *d1, *d2;

	d1 = *((delta**)a);
	d2 = *((delta**)b);
	return (d1->date - d2->date);
}

private	void
sortDelta(int flags)
{
	int	i = n;
	delta	*d;

	verbose((stderr, "Sorting...."));
	sorted = malloc(n * sizeof(sorted));
	if (!sorted) {
		perror("malloc");
		exit(1);
	}
	for (d = list; d; d = d->next) {
		assert(i > 0);
		unless (d->date || streq("70/01/01 00:00:00", d->sdate)) {
			assert(d->date);
		}
		sorted[--i] = d;
	}
	assert(i == 0);
	qsort(sorted, n, sizeof(sorted), compar);
	verbose((stderr, "done.\n"));
}

/*
 * Save the rev that needs a cset marker
 * This assumes the rev associated with a node stay constant during the
 * the findcset processing. This is safe because we only rename a rev
 * when we are in bk resolve.
 */
private void
saveCsetMark(ser_t findex, char *rev)
{
	datum	k, v, tmp;
	char	**revlist, **tmplist = 0;

	k.dptr = (char *) &findex;
	k.dsize = sizeof (ser_t);
	tmp = mdbm_fetch(csetBoundary, k);
	if (tmp.dptr) memcpy(&tmplist, tmp.dptr, sizeof (char **));
	revlist = addLine(tmplist, strdup(rev));
	v.dptr = (char *) &revlist;
	v.dsize = sizeof (char **);
	if (revlist != tmplist) {
		int ret = mdbm_store(csetBoundary, k, v, MDBM_REPLACE);
		assert(ret == 0);
	}
}

/*
 * Walk thru all files and update cset mark
 */
private void
dumpCsetMark(void)
{
	kvpair	kv;
	char	**revlist;
	char	*gfile;
	sccs	*s;
	delta	*d;
	int	flags = INIT_NOCKSUM|SILENT;
	int	i;
	ser_t	findex;

	if (opts.verbose > 1) fprintf(stderr, "Updating cset mark...\n");
	EACH_KV(csetBoundary) {
		memcpy(&findex, kv.key.dptr, sizeof (ser_t));
		memcpy(&revlist, kv.val.dptr, sizeof (char **));

		gfile = sccs2name(flist[findex]);
		if (opts.verbose > 1) fprintf(stderr, "%s\n", gfile);
		free(gfile);

		s = sccs_init(flist[findex], flags);
		assert(s);

		if (opts.ignoreCsetMarker) {
			/*
			 * Clear all old cset marker.
			 */
			for (d = s->table; d; d = d->next) d->flags &= ~D_CSET;
		}

		/*
		 * Set new cset mark
		 */
		EACH(revlist) {
			d = sccs_findrev(s, revlist[i]);
			assert(d);
			d->flags |= D_CSET;
			if (opts.verbose > 1) fprintf(stderr, "\t%s\n", d->rev);
		}
		sccs_newchksum(s);
		sccs_free(s);
		freeLines(revlist, free);
	}
	mdbm_close(csetBoundary);
	csetBoundary = 0;
}

/*
 *  Compute the time gap that seperate two csets.
 */
private int
time_gap(time_t t)
{
	static time_t now = 0;
	time_t	i, gap;

	/*
	 * Easy case, user wants constant time gap
	 */
	if (opts.timeGap) {
		gap = (opts.timeGap * MINUTE);
		goto done;
	}

	/*
	 * Increase the time gap as we go further into the pass
	 */
	unless (now) now = time(0);
	t = now - t;

	i = 3 * MONTH;
	gap = MIN_GAP;
	while (i < t) {
		gap *= 2;
		i *= 2;
		if (i >= MAX_GAP) {
			i = MAX_GAP;	/* Cap out at MAX_GAP */
			break;
		}
	}

done:	return (gap);
}

/*
 * Return true if d1 and d2 are separated by a cset boundary
 * "hasTag" is a outgoing parameter; set to one if the new cset is tagged.
 *
 * Cset boundary is determined by one of the following
 * a) tag bounary
 * b) time gap boundary
 * c) user boundary
 */
private int
isCsetBoundary(delta *d1, delta *d2, time_t tagDate, time_t now, int *skip)
{

	assert(d1->date <= d2->date);

	/*
	 * For debugging:
	 * Ignore delta older than N months.
	 * N = opts.blackOutTime.
	 */

	if (opts.blackOutTime &&
	    (d2->date < (now - (opts.blackOutTime * MONTH)))) {
		(*skip)++;
		return (0);
	}

	/* (tagDate == 0) => no tag */
	if (tagDate && d1->date <= tagDate && d2->date > tagDate) return (1);

	/*
	 * Ignore delta with fake user
	 */
	if (streq(d1->sdate, "70/01/01 00:00:00") && streq(d1->user, "Fake")) {
		return (0);
	}
	if (streq(d2->sdate, "70/01/01 00:00:00") && streq(d2->user, "Fake")) {
		return (0);
	}

	/*
	 * Check time gap
	 */
	if ((d2->date - d1->date) >= time_gap(d2->date)) {
		return (1);
	}

	/*
	 * Change user boundary
	 */
	if (opts.singleUserCset && !streq(d1->user, d2->user)) {
		if (opts.verbose > 1) {
			fprintf(stderr,
			    "Splitting cset on user boundary: %s/%s\n",
			    d1->user, d2->user);
		}
		return (1);
	}

	return (0);
}

/*
 * Return true if blank line
 */
private int
isBlank(char *p)
{
	while (*p) {
		unless (isspace(*p++))  return (0);
	}
	return (1);
}

/*
 * Convert a string into line array
 */
private char **
str2line(char **lines, char *prefix, char *str)
{
	char	*p, *q;

	q = p = str;
	while (1) {
		if (!*q) break;
		if (*q == '\n') {
			*q = 0;
			lines = addLine(lines, aprintf("%s%s", prefix, p));
			*q++ = '\n';
			p = q;
		} else {
			q++;
		}
	}
	return (lines);
}

/*
 * Convert line array into a regular string
 */
private char *
line2str(char **comments)
{
	int	i, len = 0;
	char	*buf, *p, *q;

	EACH(comments) {
		len += strlen(comments[i]) + 1;
	}

	p = buf = malloc(++len);
	EACH(comments) {
		q = comments[i];
		if (isBlank(q)) continue; /* skip blank line */
		while (*q) *p++ = *q++;
		*p++ = '\n';
	}
	*p = 0;
	assert(buf + len > p);
	return (buf);
}

/*
 * Convert cset commnent strore in a mdbm into lines format
 */
private char **
db2line(MDBM *db)
{
	kvpair	kv;
	char	**lines = 0;
	char	*comment, *gfiles, *lastg, *p, *q;
	char	*many = "Many files:";
	int	i, len = 0;
	MDBM	*gDB;

	lastg = "";
	EACH_KV(db) {
		/*
		 * Compute length of file list
		 */
		comment = kv.key.dptr;
		memcpy(&gDB, kv.val.dptr, sizeof (MDBM *));
		len = 0;
		EACH_KV(gDB) {
			len += strlen(kv.key.dptr) + 2;
		}
		len += 2;

		/*
		 * Extract gfile list and set it as the section header
		 * Skip gfile list if too long
		 */
		if (len <= 80) {
			p = gfiles = malloc(len);
			i = 1;
			EACH_KV(gDB) {
				q = kv.key.dptr;
				if (i++ > 1) {
					*p++ = ',';
					*p++ = ' ';
				}
				while (*q) *p++ = *q++;
			}
			*p++ = ':';
			*p = 0;
			assert(gfiles + len > p);

		} else {
			gfiles = strdup(many);
		}

		/*
		 * If gfile list is same as previous, skip
		 */
		if (!streq(lastg, gfiles)) {
			lines = addLine(lines, gfiles);
		}
		lastg = gfiles;

		/*
		 * Now extract the comment block
		 */
		lines = str2line(lines, "  ", comment);
	}
	return (lines);
}

/*
 * Return TURE if s1 is same as s2
 */
private int
sameStr(char *s1, char *s2)
{
	if (s1 == NULL) {
		if (s2 == NULL) return (1);
		return (0);
	}

	/* When we get here, s1 != NULL */
	if (s2 == NULL) return (0);

	/* When we get here, s1 != NULL && s2 != NULL */
	return (streq(s1, s2));
}

/*
 * Fix up date/timezone/user/hostname of delta 'd' to match 'template'
 */
private  void
fix_delta(delta *template, delta *d, int fudge)
{
	delta *parent;

	if ((opts.verbose > 1) && (fudge != -1)) {
		if (sameStr(template->sdate, d->sdate) &&
		    sameStr(template->zone, d->zone) &&
		    sameStr(template->user, d->user) &&
		    sameStr(template->hostname, d->hostname)) {
			return;
		}
		fprintf(stderr,
		    "Fixing ChangeSet rev %s to match oldest delta: %s %s",
		    d->rev, template->sdate, template->user);
		fprintf(stderr, " %s\n",
				template->hostname ? template->hostname : "");
	}

	/*
	 * Free old values
	 */
	if (d->sdate) free(d->sdate);
	if (d->user) free(d->user);
	if (d->hostname && !(d->flags & D_DUPHOST)) free(d->hostname);
	if (d->zone && !(d->flags & D_DUPZONE)) free(d->zone);

	/*
	 * Fix time zone and date
	 */
	parent = d->parent;
	if (template->zone) {
		if (parent && sameStr(template->zone, parent->zone)) {
			d->zone = parent->zone;
			d->flags |= D_DUPZONE;
		} else {
			d->zone = strdup(template->zone);
			d->flags &= ~D_DUPZONE;
		}
	} else {
		d->zone = NULL;
		d->flags |= D_NOZONE;
		d->flags &= ~D_DUPZONE; /* paraniod */
	}

	assert(template->sdate);
	d->sdate =  strdup(template->sdate);
	if (fudge != -1) { /* -1 => do not fudge the date */
		d->dateFudge = template->dateFudge + fudge;
	} else {
		assert(d->dateFudge == 0);
	}
	d->date = sccs_date2time(d->sdate, d->zone);
	d->date += d->dateFudge;

	/*
	 * Copy user
	 */
	assert(template->user);
	d->user = strdup(template->user);


	/*
	 * Copy hostname
	 */
	if (template->hostname) {
		if (parent && sameStr(template->hostname, parent->hostname)) {
			d->hostname = parent->hostname;
			d->flags |= D_DUPHOST;
		} else {
			d->hostname = strdup(template->hostname);
			d->flags &= ~D_DUPHOST;
		}
	} else {
		d->hostname = NULL;
		d->flags |= D_NOHOST;
		d->flags &= ~D_DUPHOST; /* paraniod */
	}
}

/*
 * Make sure the 1.0 and 1.1 delta
 * in the ChangeSet file is looks like is created around the same time
 * as the first delta
 */
private void
mkDeterministicKeys(void)
{
	int	iflags = INIT_NOCKSUM|SILENT;
	sccs	*cset;
	delta	*d2;

	cset = sccs_init(CHANGESET, iflags);

	/*
	 * Fix 1.0 delta of the ChangeSet file
	 * We need to do this first, so "bk new" of IGNORE
	 * and CONFIG picks up the correct back pointer to
	 * the ChangeSet file
	 */
	assert(oldest);
	d2 = cset->tree;
	fix_delta(oldest, d2, 0);
	d2->sum = oldest->sum;
	d2 = d2->kid;
	assert(d2);
	fix_delta(oldest, d2, 1);
	sccs_admin(cset, 0, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0, 0);
	sccs_free(cset);
}

typedef struct {
	MDBM	*view;
	MDBM	*db;
	MDBM	*csetComment;
	FILE	*patch;
	sum_t	sum;
	int	rev;
	char	parent[MAXKEY];
	char	patchFile[MAXPATH];
	sccs	*cset;
	int	date;
} mkcs_t;

private void
mkCset(mkcs_t *cur, delta *d)
{
	int	v;
	ser_t	k;
	kvpair	kv, vk;
	delta	*top;
	delta	*e = calloc(sizeof(delta), 1);
	char	dkey[MAXKEY];
	char	dline[2 * MAXKEY + 4];
	char	**lines;
	int	prs_flags = (PRS_PATCH|SILENT);
	int	i;
	int	added;


	/*
	 * Force check in date/user/host to match delta "d", the last delta.
	 * The person who make the last delta is likely to be the person
	 * making the cset.
	 */
	fix_delta(d, e, -1);

	/* Need to set parent right somehow */
	e = sccs_dInit(e, 'D', cur->cset, 1);
	e->flags |= D_CSET; /* copied from cset.c, probably redundant */
	e->comments = db2line(cur->csetComment);

	/*
	 * Need to do diff into a file before outputting the
	 * header, because we need lines added in the header
	 * even though the importer just sets it to be one.
	 * also need to get the delta checksum.
	 */
	lines = 0;
	added = 0;
// fprintf(stderr, "cur=%u\n", cur->sum);
	EACH_KV(cur->db) {
		sum_t	linesum, sumch;
		u8	*ch;

		/*
		 * Extract filename and top delta for this cset
		 */
		memcpy(&k, kv.key.dptr, sizeof(ser_t));
		memcpy(&v, kv.val.dptr, sizeof(int));
		top = sorted[v];

		saveCsetMark(k, top->rev);

		sccs_sdelta(0, top, dkey);
		sprintf(dline, "%s %s\n", keylist[k], dkey);
		lines = addLine(lines, strdup(dline));
		linesum = 0;
		for (ch = dline; *ch; ch++) {
			sumch = *ch;
			linesum += sumch;
		}
		/*
		 * XXX Do the checksum thing here
		 * Key is in keylist[k].
		 * value is checksum of whole line
		 * dbval = mdbm_fetch(cur->view, dbkey);
		 * then memcpy
		 */
		if (ch = mdbm_fetch_str(cur->view, keylist[k])) {
			memcpy(&sumch, ch, sizeof(sum_t));
			cur->sum -= sumch;
// fprintf(stderr, "--- cur=%u %u\n", cur->sum, sumch);
		}
		cur->sum += linesum;
// fprintf(stderr, "+++ cur=%u %u\n", cur->sum, linesum);
		vk.key.dptr = keylist[k];
		vk.key.dsize = strlen(keylist[k])+1;
		vk.val.dptr = (void *)&linesum;
		vk.val.dsize = sizeof(linesum);
		if (mdbm_store(cur->view, vk.key, vk.val, MDBM_REPLACE)) {
			assert("insert failed" == 0);
		}
		added++;
	}
	e->added = added;
	e->sum = cur->sum;
//  fprintf(stderr, "SUM=%u\n", e->sum);

	/* Some hacks to get sccs_prs to do some work for us */
	cur->cset->rstart = cur->cset->rstop = e;
	if (cur->cset->tree->pathname && !e->pathname) {
		e->pathname = cur->cset->tree->pathname;
		e->flags |= D_DUPPATH;
	}
	if (cur->cset->tree->zone && !e->zone) {
		e->zone = cur->cset->tree->zone;
		e->flags |= D_DUPZONE;
	}
	sprintf(dkey, "1.%u", cur->rev++);
	assert(!e->rev);
	e->rev = strdup(dkey);

	unless (cur->date < e->date) {
		int	fudge = (cur->date - e->date) + 1;

		e->date += fudge;
		e->dateFudge += fudge;
	}
	cur->date = e->date;

	/* spit out parent key */
	fputs(cur->parent, cur->patch);
	fputs("\n", cur->patch);
	sccs_sdelta(cur->cset, e, cur->parent);
	/* need checksum */
	if (sccs_prs(cur->cset, prs_flags, 0, NULL, cur->patch)) exit(1);
	sortLines(lines, 0);
	fputs("\n0a0\n", cur->patch); /* Fake diff header */
	EACH(lines) {
		fputs("> ", cur->patch);
		fputs(lines[i], cur->patch);
	}
	fputs("\n", cur->patch);
	freeLines(lines, free);
}


/*
 * We dump the comments into a mdbm to factor out repeated comments
 * that came from different files.
 */
private void
saveComment(MDBM *db, char *rev, char *comment_str, char *gfile)
{
	datum	k, v, tmp;
	MDBM	*gDB = 0;
	int	ret;

#define	CVS_NULL_COMMENT "*** empty log message ***\n"
#define	SCCS_REV_1_1_DEFAULT_COMMENT	"date and time created "
#define	CODE_MGR_DEFAULT_COMMENT	"CodeManager Uniquification: "

	if (isBlank(comment_str)) return;
	if (streq(CVS_NULL_COMMENT, comment_str)) return;
	if ((streq("1.1", rev) || streq(rev, "1.2")) &&
	    strneq(SCCS_REV_1_1_DEFAULT_COMMENT, comment_str, 22)) {
		return;
	}
	/*
	 * For 3pardate's teamware repository
	 */
	if ((streq("1.1", rev) || streq(rev, "1.2")) &&
	    strneq(CODE_MGR_DEFAULT_COMMENT, comment_str, 28)) {
		return;
	}

	k.dptr = (char *) comment_str;
	k.dsize = strlen(comment_str) + 1;
	tmp = mdbm_fetch(db, k);
	if (tmp.dptr) memcpy(&gDB, tmp.dptr, sizeof (MDBM *));
	unless (gDB) gDB = mdbm_mem();
	ret = mdbm_store_str(gDB, basenm(gfile), "", MDBM_INSERT);
	/* This should work, or it will be another file with the same
	 * basename.
	 */
	assert(ret == 0 || (ret == 1 && errno == EEXIST));
	unless (tmp.dptr) {
		v.dptr = (char *) &gDB;
		v.dsize = sizeof (MDBM *);
		ret = mdbm_store(db, k, v, MDBM_REPLACE);
	}
}

private void
freeComment(MDBM *db)
{
	kvpair	kv;
	MDBM	*gDB;

	EACH_KV(db) {
		memcpy(&gDB, kv.val.dptr, sizeof (MDBM *));
		mdbm_close(gDB);
	}
	mdbm_close(db);
}

private int
isBreakPoint(time_t now, delta *d)
{
	if (opts.noSkip) return (0);

	/*
	 * Do not make a cset of recent deltas, becuase we may be
	 * missing delta that will be added in the near future
	 */
	if (d->date >= (now - time_gap(d->date))) {
		if (opts.verbose > 1) {
			fprintf(stderr,
			    "Skipping delta younger than %d minutes\n",
			    opts.timeGap);
		}
		return (1);
	}
	return (0);
}

private	void
findcset(void)
{
	int	i, j;
	delta	*d = 0, *previous;
	datum	key, val;
	char	*p;
	FILE	*f;
	delta	*d2;
	time_t	now, tagDate = 0;
	char	*nextTag;
	int	ret;
	char	**syms = 0;
	int	skip = 0;
	mkcs_t  cur;
	int     flags = 0;
	char	buf[MAXLINE];

	cur.db = mdbm_mem();
	cur.view = mdbm_mem();
	cur.csetComment = mdbm_mem();
	cur.sum = 0;
	cur.parent[0] = 0;
	cur.rev = 2;
	cur.date = 0;
	bktmp(cur.patchFile, "cpatch");
	sprintf(buf, "bk _adler32 > %s", cur.patchFile);
	unless (cur.patch = popen(buf, "w")) {
		perror("findcset");
		exit (1);
	}

	unless (opts.verbose) flags |= SILENT;
	cur.cset = sccs_init(CHANGESET, INIT_NOCKSUM|flags);
	assert(cur.cset && cur.cset->tree);

	/*
	 * Force 1.0 and 1.1 cset delta to match oldest
	 * delta in the repository. Warning; this changes the cset root key!
	 */
	assert(oldest);
	d2 = cur.cset->tree;
	fix_delta(oldest, d2, 0);
	d2 = d2->kid;
	assert(d2);
	fix_delta(oldest, d2, 1);
	sccs_newchksum(cur.cset);

	fputs("\001 Patch start\n", cur.patch);
	fputs("# Patch vers:\t1.3\n# Patch type:\tREGULAR\n\n", cur.patch);
	fputs("== ChangeSet ==\n", cur.patch);
	sccs_pdelta(cur.cset, sccs_ino(cur.cset), cur.patch);
	fputs("\n", cur.patch);

	d2 = sccs_top(cur.cset);
	sccs_sdelta(cur.cset, d2, cur.parent);
	cur.sum = d2->sum;
	cur.date = d2->date;

	nextTag = readTag(&tagDate);

	f = stderr;
	now = time(0);
	for (j = 0; j < n; ++j) {
		d = sorted[j];
		unless (d->type == 'D') continue;
		if (j > 0) {
			previous = sorted[j - 1];
			/* skip tags that are too early */
			while (nextTag && tagDate < previous->date) {
				free(nextTag);
				nextTag = readTag(&tagDate);
			}
			if (isCsetBoundary(previous, d, tagDate, now, &skip)) {
				mkCset(&cur, previous);
				while (nextTag && tagDate >= previous->date &&
				    tagDate < d->date) {
					syms = addLine(syms,
					   aprintf("%d %s",
					       cur.rev-1, nextTag));
					nextTag = readTag(&tagDate);
				}
				if (isBreakPoint(now, d)) goto done;
				freeComment(cur.csetComment);
				cur.csetComment = mdbm_mem();
				mdbm_close(cur.db);
				cur.db = mdbm_mem();
			}
		}

		/*
		 * Find the top delta of a file for this cset.
		 * This is done by stuffing the f_index/delta pair into a mdbm
		 */
		assert(d->f_index > 0);
		key.dptr = (char *) &(d->f_index);
		key.dsize = sizeof (ser_t);
		val.dptr = (char *) &j;
		val.dsize = sizeof (int);
		/* last entry wins */
		ret = mdbm_store(cur.db, key, val, MDBM_REPLACE);
		assert(ret == 0);

		/*
		 * Extract per file comment and copy them to cset comment
		 */
		p = line2str(d->comments);
		saveComment(cur.csetComment, d->rev, p, d->pathname);
		free(p);
		/* pathname will be freeed in freeComment() */
	}
	if (skip && (opts.verbose > 1)) {
		fprintf(stderr,
		    "Skipping %d delta%s older than %d month.\n",
		    skip, ((skip == 1) ? "" : "s"), opts.blackOutTime);
	}
	assert(d == sorted[n - 1]);
	if (isBreakPoint(now, d)) goto done;
	mkCset(&cur, d);
	while (nextTag && tagDate >= d->date) {
		syms = addLine(syms, aprintf("%d %s", cur.rev-1, nextTag));
		nextTag = readTag(&tagDate);
	}
done:	freeComment(cur.csetComment);
	fputs("\001 End\n", cur.patch);
	pclose(cur.patch);
	/* currently patch is in PPP, and patchfile not used */
	sccs_free(cur.cset);
	mdbm_close(cur.db);
	mdbm_close(cur.view);
	sys("cp", cur.patchFile, "/tmp/PPP", SYS); // XXX
	if (sys("bk", "takepatch", "-f", cur.patchFile, SYS)) {
		sys("cat", cur.patchFile, SYS);
		exit(1);
	}
	sys("mv", "RESYNC/SCCS/s.ChangeSet", "SCCS/s.ChangeSet", SYS);
	system("rm -rf RESYNC");
	unlink(cur.patchFile);
	EACH (syms) {
		p = strchr(syms[i], ' ');
		*p++ = 0;
		sprintf(buf, "-r1.%s", syms[i]); /* rev */
		sys("bk", "tag", "-q", buf, p, SYS);
	}
	freeLines(syms, free);
}

/*
 * Find the oldest user delta
 */
private delta *
findFirstDelta(sccs *s, delta *first)
{
	delta	*d = sccs_findrev(s, "1.1");

	unless (d) return (first);

	/*
	 * Skip teamware dummy user
	 * XXX - there can be more than one.
	 */
	if (streq(d->sdate, "70/01/01 00:00:00") && streq(d->user, "Fake")) {
		d = d->kid;
	}
	unless (d) return (first);

	/*
	 * XXX TODO If d->date == first->date
	 * we need to sort on sfile name
	 */
	if ((first == NULL) || (d->date < first->date)) {
		unless (first) first = calloc(sizeof(*first), 1);
		if (first->zone) free(first->zone);
		if (first->sdate) free(first->sdate);
		if (first->user) free(first->user);
		if (first->hostname) free(first->hostname);

		first->sdate = strdup(d->sdate);
		if (d->zone) {
			first->zone = strdup(d->zone);
			first->flags &= ~D_NOZONE;
		} else {
			first->zone = NULL;
			first->flags |= D_NOZONE;
		}
		first->date = sccs_date2time(first->sdate, first->zone);
		first->dateFudge = 0;

		first->user = strdup(d->user);
		if (d->hostname) {
			first->hostname = strdup(d->hostname);
			first->flags &= ~D_NOHOST;
		} else {
			first->hostname = NULL;
			first->flags |= D_NOHOST;
		}
	}
	return (first);
}

/*
 * Collect the delta that we need into "list"
 */
private	void
mkList(sccs *s, int fileIdx)
{
	delta	*d, *e;

	assert(fileIdx > 0);

	/*
	 * Mark the delta in pending state on the main trunk
	 */
	d = sccs_top(s);
	while (d) {
		assert(!d->r[2]);
		if (!opts.ignoreCsetMarker) {
			if (d->flags & D_CSET) break;
		}

		/*
		 * Skip 1.0 delta, we do not want a 1.0 delta
		 * show up as the top delta in a cset. A top
		 * delta in a cset should be 1.1 or higher.
		 */
		if (d == s->tree) break;
		d->flags |= D_SET;
		d  = d->parent;
	}

	for (d = s->table; d; ) {
		e = d->next;
		if (d->flags & D_SET) {
			/*
			 * Collect marked delta into "list"
			 */
			d->f_index = fileIdx; /* needed in findcset() */
			d->next = list;
			list = d;
			n++;
		} else {
			/*
			 * Unwanted delta;
			 * Cannot free these here, because
			 * d->hostname and d->zone may be shared
			 * with node in the pending delta "list",
			 * so stick them in a free list to be freed later
			 */
			d->next = freeme;
			freeme = d;
		}
		d = e;
	}
	s->table = s->tree = 0;
}

private	void
freeList()
{
	delta	*d;

	for (d = list; d; d = list) {
		n--;
		list = list->next;
		d->siblings = d->kid = 0;
		sccs_freetree(d);
	}
	for (d = freeme; d; d = freeme) {
		freeme = freeme->next;
		d->siblings = d->kid = 0;
		sccs_freetree(d);
	}
	if (sorted) free(sorted);
}

FILE	*tf;

private int
openTags(char *tagfile)
{
	tf = fopen (tagfile, "rt");
	unless (tf) perror(tagfile);
	return (tf == 0);
}

private char *
readTag(time_t *tagDate)
{
	char	nextTag[2048];
	char	buf[MAXLINE];

	*tagDate = 0;	/* default */
	unless (tf) return (0);
	if (fnext(buf, tf)) {
		int	rc;
		rc = sscanf(buf, "%s %lu", nextTag, tagDate);
		assert(rc == 2);
		assert(strlen(nextTag) < sizeof(nextTag));
	} else {
		closeTags();
		return (0);
	}
	return (strdup(nextTag));
}

private void
closeTags(void)
{
	if (tf) fclose(tf);
	tf = 0;
}
