/* Copyright (c) 2001 L.W.McVoy */
#include "system.h"
#include "sccs.h"
#include "logging.h"

/*
 * Verbosity:
 *   0 - quiet
 *   1 - default: only names of files followed by \r
 *   2 - alterations: mapping version numbers
 *   3 - everything, including things that worked with no alterations.
 *       XXX: level 3 not reachable through import -t SCCS interface
 */

private int sccs2bk(sccs *s, int verbose, char *csetkey);
private void regen(sccs *s, int verbose, char *key);
private int verify = 1;
private	int mkinit(sccs *s, delta *d, char *file, char *key);
private int makeMerge(sccs *s, int verbose);
private void collapse(sccs *s, int verbose, delta *d, delta *m);
private void fixTable(sccs *s, int verbose);
private void handleFake(sccs *s);
private void ptable(sccs *s);

/*
 * Convert an SCCS (including Sun Teamware) file
 */
int
sccs2bk_main(int ac, char **av)
{
	sccs	*s;
	int	c, errors = 0;
	int	verbose = 1;
	char	*csetkey = 0;
	char	*name;
	int	licChk = 0;

	while ((c = getopt(ac, av, "c|hLqv", 0)) != -1) {
		switch (c) {
		    case 'c': csetkey = optarg; break;
		    case 'h': verify = 0; break;
		    case 'L': licChk = 1; break;
		    case 'q': verbose = 0; break;
		    case 'v': verbose++; break;
		    default: bk_badArg(c, av);
		}
	}

	if (bk_notLicensed(0, LIC_IMPORT, 1)) {
		if (verbose) {
			// XXX - need a name
			fputs("sccs2bk: operation requires "
			    "a license with import feature.\n", stderr);
		}
		return (1);
	}
	if (licChk) return (0);

	unless (csetkey) usage();
	bk_setConfig("checkout", "none");
	for (name = sfileFirst("sccs2bk", &av[optind], 0);
	    name; name = sfileNext()) {
		unless (s = sccs_init(name, 0)) continue;
		unless (HASGRAPH(s)) {
			perror(s->sfile);
			sccs_free(s);
			errors |= 1;
			continue;
		}
		if (verbose) {
			fprintf(stderr, "Converting %-65s%c", s->gfile,
			    verbose == 1 ? '\r' : '\n');
		}
		errors |= sccs2bk(s, verbose, csetkey);
		s = 0;	/* freed by sccs2bk */
	}
	if (sfileDone()) errors |= 2;
	if (verbose == 1) fprintf(stderr, "\n");
	return (errors);
}

/*
 * sccs2bk to BK.
 * 1. Restructure graph to uphold BK invariants
 * 2. Issue a series of shell command which rebuilds the file.
 */
private int
sccs2bk(sccs *s, int verbose, char *csetkey)
{
	delta	*d;
	int	i;

	if (makeMerge(s, verbose)) return (1);

	/*
	 * Strip the dangling branches by coloring the graph and
	 * then losing anything which is not marked.
	 */
	sccs_color(s, sccs_top(s));
	for (d = s->table; d; d = NEXT(d)) {
		if (d->flags & D_RED) continue;
		d->flags |= D_SET|D_GONE;
	}

	/*
	 * 3par had some BitKeeper files that Teamware had munged,
	 * strip the root if that's the case.
	 */
	if (streq(REV(s, s->tree), "1.0")) {
		d = s->tree;
		EACH_COMMENT(s, d) {
			if (strneq("BitKeeper file", d->cmnts[i], 14)) {
				d->flags |= D_SET|D_GONE;
				break;
			}
		}
		if ((d->flags & D_GONE) && verbose > 1) {
			fprintf(stderr,
			    "Stripping old BitKeeper data in %s\n", s->gfile);
		}
	}

	/*
	 * Fix delta with date == 0 (teamware fakes)
	 * And fix the tree structure so that trunk delta after fork
	 * is older than branch delta after fork and that
	 * all time is monotonically increasing.
	 */
	handleFake(s);
	fixTable(s, verbose);

	/*
	 * Go spit out the commands to regen this
	 */
	regen(s, verbose, csetkey);

	return (0);
}

/*
 * read map key <serial> => val <rev> in new imported system
 * no entry if <rev> -eq <d->rev>
 */
private char	*
rev(MDBM *revs, sccs *s, delta *r)
{
	datum	k, v;

	k.dsize = sizeof(ser_t);
	k.dptr = (void*)&r->serial;
	v = mdbm_fetch(revs, k);
	if (v.dsize) return ((char*)v.dptr);
	return (REV(s, r));
}

private void
regen(sccs *s, int verbose, char *key)
{
	delta	*d;
	delta	**table = malloc(s->nextserial * sizeof(delta*));
	int	n = 0;
	int	i;
	int	src_flags = PRINT|GET_FORCE|SILENT;
	int	dest_flags = PRINT|SILENT;
	char	*sfile = s->sfile;
	char	*gfile = s->gfile;
	char	*tmp;
	char	*a1, *a2, *a3;
	MDBM	*revs = mdbm_mem();
	sccs	*s2, *sget;
	pfile	pf;
	int	crnl;

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, (ser_t) i)) continue;
		comments_load(s, d);
		if (!(d->flags & D_GONE) && (d->type == 'D')) {
			table[n++] = d;
		}
	}
	sccs_close(s);
	tmp = strdup(sfile);
	a1 = strrchr(tmp, '/');
	a1[3] = ',';
	a1[4] = 0;
	rename(sfile, tmp);
	/* Get a clean copy just to use for 'get' on the original */
	sget = sccs_init(tmp, 0);
	assert(sget);
	sget->xflags &= ~X_EOLN_NATIVE;
	close(creat(gfile, 0664));
	/* Teamware uses same uuencode flag, so read it */
	if (mkinit(s, s->tree, tmp, key) || (sget->encoding & E_UUENCODE)) {
		sys("bk", "delta",
		    "-fq", "-Ebinary", "-RiISCCS/.init", gfile, SYS);
	} else {
		sys("bk", "delta", "-fq", "-RiISCCS/.init", gfile, SYS);
	}
	for (i = 0; i < n; ++i) {
		d = table[i];
		mkinit(s, d, 0, 0);
		if (d->merge) {
			delta	*m = MERGE(s, d);

			assert(m);
			a1 = aprintf("-egr%s", rev(revs, s, PARENT(s, d)));
			a2 = aprintf("-M%s", rev(revs, s, m));
			sys("bk", "_get", "-q", a1, a2, gfile, SYS);
			free(a1);
			free(a2);
		} else {
			a1 = aprintf("-egr%s",
			    i ? rev(revs, s, PARENT(s, d)) : "1.0");
			sys("bk", "_get", "-q", a1, gfile, SYS);
			free(a1);
		}
		if (sccs_read_pfile("sccs2bk", s, &pf)) exit(1);
		unless (streq(REV(s, d), pf.newrev)) {
			datum	k, v;

			if (verbose > 1) {
				fprintf(stderr,
				    "MAP %s(%d)@%s -> %s\n", gfile, d->serial,
				    REV(s, d), pf.newrev);
			}
			k.dsize = sizeof(ser_t);
			k.dptr = (void*)&d->serial;
			v.dsize = strlen(pf.newrev) + 1;
			v.dptr = (void*)pf.newrev;
			if (mdbm_store(revs, k, v, MDBM_INSERT)) {
				v = mdbm_fetch(revs, k);
				fprintf(stderr, "%s: serial %d in use by "
				    "%s and %s.\n", s->sfile, d->serial,
				    pf.newrev, (char*)v.dptr);
				/* XXX: what should errors do? */
				exit(1);
			}
		}
		free_pfile(&pf);
		/*
		 * use GET_FORCE because we want to 'get' even if bad graph.
		 * '-r=<serial>' because some sfile have duplicate revisions
		 * Note: if change options here, then change in loop below
		 */
		a1 = aprintf("=%d", d->serial);
		if (sccs_get(sget, a1, 0, 0, 0, src_flags, gfile)) {
			fprintf(stderr, "FAIL: get -kPr%s %s\n",
			    a1, gfile);
			exit (1);
		}
		free(a1);
		sys("bk", "delta", "-fq", "-RISCCS/.init", gfile, SYS);
	}

        s2 = sccs_init(s->sfile, 0);
        assert(s2);
	unless (1 || verify) goto out;

        if (sccs_admin(s2, 0,
            ADMIN_FORMAT|ADMIN_BK|ADMIN_TIME, 0, 0, 0, 0, 0, 0, 0)) {
                perror(s->sfile);
                exit(1);
        }
	if (verbose) {
		fprintf(stderr, "Verifying %-66s%c", gfile,
		    verbose > 1 ? '\n' : '\r');
	}
	crnl = 0;
	s2->xflags &= ~X_EOLN_NATIVE;
	for (i = 0; i < n; ++i) {
		d = table[i];
		a1 = aprintf("%s", rev(revs, s, d));
		a2 = bktmp(0, "diffA");
		assert(a2 && a2[0]);
		if (sccs_get(s2, a1, 0, 0, 0, dest_flags, a2)) {
			fprintf(stderr, "FAIL: get -kpr%s %s\n", a1, gfile);
			exit (1);
		}

		free(a1);
		a1 = aprintf("=%d", d->serial);
		a3 = bktmp(0, "diffB");
		assert(a3 && a3[0]);
		if (sccs_get(sget, a1, 0, 0, 0, src_flags, a3)) {
			fprintf(stderr, "FAIL: get -kPr%s %s\n", a1, gfile);
			exit (1);
		}
		unless (sameFiles(a2, a3)) {
			/* check to see if only diff because of \r\n in orig */
			free(a1);
			a1 = bktmp(0, "diffC");
			assert(a1 && a1[0]);
			if (diff(a2, a3, DF_DIFF, a1)) {
				fprintf(stderr, "\n%s@%s != orig@%s\n",
				    gfile, rev(revs, s, d), REV(s, d));
				sys("echo", "diff", a2, a3, ">", a1, SYS);
				sys("cat", a1, SYS);
				/* exit(1); */
			}
			unlink(a1);
			crnl = 1;
			if (verbose > 2) {
				fprintf(stderr,
				    "%s@%s != orig@%s - <cr><lf> only\n",
				    gfile, rev(revs, s, d), REV(s, d));
			}
		}
		if (verbose > 2) {
			fprintf(stderr, "%s@%s OK\n", gfile, rev(revs, s, d));
		}
		unlink(a2);
		unlink(a3);
		free(a1);
		free(a2);
		free(a3);
	}
	if (verbose == 2 && crnl) {
		fprintf(stderr, "%s: differs only by <cr><lf>\n", gfile);
	}
out:	unlink("SCCS/.init");
	sccs_close(sget);	/* WIN 32 : close before unlinking tmp */
	unlink(tmp);
	free(table);
	mdbm_close(revs);
	if (do_checkout(s2)) exit(1);
	sccs_free(s2);
	sccs_free(sget);
	sccs_free(s);
}

/*
 * From Wayne.
 */
private int
mkinit(sccs *s, delta *d, char *file, char *key)
{
	char	randstr[17];
	int	chksum = 0;
	FILE	*fh;
	int	size;
	char	buf[4096];
	u32	randbits = 0;
	int	i;
	int	binary = 0;

	if (file) {
		char	*p;
		p = aprintf("bk get -qkPr1.1 '%s'", file);
		fh = popen(p, "r");
		free(p);
		while (size = fread(buf, 1, sizeof(buf), fh)) {
			randbits = adler32(randbits, buf, size);
			for (i = 0; i < size; i++) {
				unless (buf[i]) {
					binary = 1;
					break;
				}
			}
		}
		pclose(fh);
		sprintf(randstr, "%08x", randbits);
		chksum = randbits & 0xffff;
	}
	fh = fopen("SCCS/.init", "w");
	if (file) {
		struct	tm *tp = utc2tm(d->date - 1);

		assert(key);
		fprintf(fh, "D 1.0 %02d/%02d/%02d %02d:%02d:%02d %s@%s\n",
		    tp->tm_year % 100,
		    tp->tm_mon + 1,
		    tp->tm_mday,
		    tp->tm_hour,
		    tp->tm_min,
		    tp->tm_sec,
		    USER(s, d),
		    d->hostname ? HOSTNAME(s, d) : sccs_gethost());
		fprintf(fh,
			"B %s\n"
			"c sccs2bk\n"
			"K %05u\n"
			"O %o\n"
			"P %s\n"
			"R %.8s\n",
			key,
			chksum,
			0664,
			s->gfile,
			randstr);
	} else {
		fprintf(fh, "D %s %s %s@%s\n",
		    REV(s, d), d->sdate, USER(s, d),
		    d->hostname ? HOSTNAME(s, d) : sccs_gethost());
		EACH_COMMENT(s, d) {
			fprintf(fh, "c %s\n", d->cmnts[i]);
		}
		if (d->dateFudge) fprintf(fh, "F %lu\n", d->dateFudge);
	}
	fprintf(fh, "------------------------------------------------\n");
	fclose(fh);
	return (binary);
}

private void
ptable(sccs *s)
{
	delta	*d;
	int	i;

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, (ser_t) i)) continue;
		unless (!(d->flags & D_GONE) && (d->type == 'D')) {
			continue;
		}
		fprintf(stderr, "%-10.10s %10s i=%2u s=%2u d=%s f=%lu\n",
		    s->gfile, REV(s, d), i, d->serial, d->sdate, d->dateFudge);
	}
}

/*
 * Teamware grafts files together by putting a delta in by user "Fake"
 * with a time_t of 0 (Jan 1, 1970).  We use the earliest name for the file.
 * Having it be 0 isn't useful.
 * Sometimes there are multiple fakes.
 * Our (really lm's) solution: No real code should be before 1971.
 * Find first delta after 1971.  This is the first real delta.
 * Move all fakes to near this first real delta.
 * And set the username for the fakes to the username in the first real delta.
 *
 * If no real deltas exist, then move fakes to near 1971 and use user BKFake.
 */

#define	CUTOFFDATE	31536000	/* Fri Jan  1  0:00:00 GMT 1971 */

private void
handleFake(sccs *s)
{
	delta	*d;
	int	i;
	time_t	date = CUTOFFDATE;
	u32	user = 0;

	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, (ser_t) i)) continue;
		unless (!(d->flags & D_GONE) && (d->type == 'D')) {
			continue;
		}
		unless (d->date >= CUTOFFDATE) continue;
		date = d->date;
		user = d->user;
		break;
	}
	/* If no user, then there's no BKFake already in heap -- it is uniq */
	unless (user) user = sccs_addUniqStr(s, "BKFake");

	/* only fix the first 'i' deltas in count down fashion */
	while (i > 1) {
		i--;
		unless (d = sfind(s, (ser_t) i)) continue;
		unless (!(d->flags & D_GONE) && (d->type == 'D')) {
			continue;
		}
		date--;
		d->date = date;
		strftime(d->sdate, strlen(d->sdate)+1,
		    "%y/%m/%d %H:%M:%S", gmtime(&d->date));
		assert(d->user != user);
		d->user = user;
	}
}

/*
 * See if d->merge ancestory of d contains d->parent.
 * If yes, collapse merge branch onto same branch as d
 * 
 * fixes problem of non bk structure (note: 1.1.1.1 is older than 1.2):
 *  1.1 ------------- 1.2
 *   +---- 1.1.1.1 ----+
 *
 * becomes instead:
 *  1.1 ---- 1.1.1.1 ---- 1.2
 *
 * The numbering gets fixed as we export this and import into a bk setup.
 */
private void
collapse(sccs *s, int verbose, delta *d, delta *m)
{
	delta	*b, *p, *e;
	ser_t	*ep;

	p = PARENT(s, d);
	assert(m && p);

	/*
	 * See if merge ancestory contains parent.
	 * b == base of merge ancestory whose parent is (possibly) p
	 */
	for (b = m; b && b->pserial; b = PARENT(s, b)) {
		unless (b->pserial > p->serial) break;
	}
	unless (b && b->pserial == p->serial) return;
	if (verbose > 1) {
		fprintf(stderr,
		    "Inlining graph: new parent of %s(%d) => %s(%d)\n",
		    REV(s, d), d->serial,
		    REV(s, m), m->serial);
	}

	/*
	 * Do surgery on 'p' kids list:
	 *   1. delete b from current location in list
	 *   2. put b where ever d was, removing d from list
	 */
	for (ep = &p->kid; e = SFIND(s, *ep); ep = &e->siblings) {
		if (e == b) break;
	}
	assert(e);
	*ep = b->siblings;	/* delete b */
	for (ep = &p->kid; e = SFIND(s, *ep); ep = &e->siblings) {
		if (d == e) break;
	}
	assert(e);
	*ep = b->serial;	/* put b in place of p; deleting p */
	b->siblings = d->siblings;

	/* surgery at other end of graph: merge becomes parent of d */
	d->siblings = m->kid;
	m->kid = d->serial;
	d->pserial = m->serial;
	d->merge = 0;
	assert(d->include);
	free(d->include);
	d->include = 0;
}

/*
 * makeMerge - when seeing a list of includes, make it a merge pointer.
 * teamware does a -i on the tip of a merge
 */
private int
makeMerge(sccs *s, int verbose)
{
	delta	*d, *m;
	int	i, j;
	ser_t	mser;

	/* table order, mark things to ignore, and find merge markers */
	for (d = s->table; d; d = NEXT(d)) {
		if (d->type == 'R') {
			d->flags |= D_GONE;
			continue;
		}
		unless (d->include) continue;
		assert(!d->merge);

		j = 0;
		EACH(d->include) {
			if ((m = sfind(s, d->include[i])) && (m->type == 'D')) {
				if (j) d->include[j++] = d->include[i];
			} else {	/* delete it */
				unless (j) j = i;
			}
		}
		if (j) d->include[0] = (d->include[0] & ~LMASK) | (j-1);
		if (emptyLines(d->include)) {
			free(d->include);
			d->include = 0;
			continue;
		}
		mser = d->include[d->include[0] & LMASK];
		m = sfind(s, mser);
		assert(m && m->type == 'D');
		d->merge = mser;
		collapse(s, verbose, d, m);
	}
	return (0);
}

/*
 * fixTable - set "fudge" so time marches forward
 * and possibly swap branch and trunk.
 * 
 * Teamware files do not converge based on dates.
 * When I (lm) wrote smoosh, the destination was the side that moved onto
 * a branch.
 * Since what they are importing is likely to be their "main" tree, they
 * will want to preserve their branch structure.
 *
 * This code sometimes swaps parent and merge pointers to achieve:
 *   1. keep the same table order
 *   2. pass the bitkeeper invariants
 *   3. leave the tip of the trunk as the tip of the trunk
 */
private void
fixTable(sccs *s, int verbose)
{
	delta	*d = 0, *e = 0, *m = 0;
	int	i;
	u32	maxfudge = 0;

	/* reverse table order of just the deltas being imported */
	for (i = 1; i < s->nextserial; i++) {
		unless (d = sfind(s, (ser_t) i)) continue;
		unless (!(d->flags & D_GONE) && (d->type == 'D')) {
			continue;
		}
		/*
		 * Fudge dates to support the BK invariant
		 * that the table order dates are always increasing.
		 * Ties in time are resolved by alphabetized keys.
		 * Don't worry about keys here, because we are setting time
		 * always going forward.
		 */
		if (e && d->date <= e->date) {
			int	f = (e->date - d->date) + 1;

			d->dateFudge += f;
			d->date += f;
			if (f > maxfudge) maxfudge = f;
		}
		e = d;
		/*
		 * Swap the merge and parent if needed.
		 * The outcome is to keep the trunk tip
		 * on the trunk after import.
		 */
		unless (d && d->merge) continue;
		m = MERGE(s, d);
		assert(m);
		if (sccs_needSwap(s, PARENT(s, d), m)) {
			if (verbose > 1) {
				fprintf(stderr,
				    "Need to swap in %s => %s & %s\n",
				    REV(s, d), REV(s, PARENT(s, d)), REV(s, m));
			}
			/*
			 * XXX: This is a hack of the graph optimized
			 * for our needs.  See renumber.c:parentSwap()
			 * for a complete rewiring job.
			 */
			d->merge = d->pserial;
			d->pserial = m->serial;
		}
	}
	if (verbose > 2) ptable(s);
	if (verbose > 1 && e) {
		fprintf(stderr, "%s: maxfudge=%u lastfudge=%lu\n",
		    s->gfile, maxfudge, e->dateFudge);
	}
}
