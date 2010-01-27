/* Copyright (c) 1999 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
#include "nested.h"

typedef	struct cset {
	/* bits */
	int	mixed;		/* if set, then both long and short keys */
	int	makepatch;	/* if set, act like makepatch */
	int	listeach;	/* if set, list revs 1/line */
	int	mark;		/* act like csetmark used to act */
	int	doDiffs;	/* prefix with unified diffs */
	int	force;		/* if set, then force past errors */
	int	remark;		/* clear & redo all the ChangeSet marks */
	int	dash;
	int	historic;	/* list the historic name */
	int	hide_comp;	/* exclude comp from file@rev list */
	int	include;	/* create new cset with includes */
	int	exclude;	/* create new cset with excludes */
	int	serial;		/* the revs passed in are serial numbers */
	int	md5out;		/* the revs printed are as md5keys */
	int	doBAM;		/* send BAM data */
	int	compat;		/* do not send new sfiles in sfio */
	int	fastpatch;	/* enable fast patch mode */

	/* numbers */
	int	tooMany;	/* send whole sfiles if # deltas > tooMany */
	int	verbose;
	int	notty;
	int	fromStart;	/* if we did -R1.1..1.2 */
	int	ndeltas;
	int	ncsets;
	int	nfiles;
	pid_t	pid;		/* adler32 process id */
	int	lasti;		/* last idx in cweave for cset_diffs */

	char	*csetkey;	/* lie about the cset rootkey */
	char	**cweave;	/* weave of cset file for this patch */
	char	**BAM;		/* list of keys we need to send */
	char	**sfiles;	/* list of whole sfiles we need to send */
} cset_t;

private	void	csetlist(cset_t *cs, sccs *cset);
private	int	marklist(char *file);
private	delta	*mkChangeSet(sccs *cset, char *files, FILE *diffs);
private	void	doSet(sccs *sc);
private	void	doMarks(cset_t *cs, sccs *sc);
private	void	doDiff(sccs *sc, char kind);
private	void	sccs_patch(sccs *, cset_t *);
private	int	cset_diffs(cset_t *cs, ser_t ser);
private	void	cset_exit(int n);
private	char	csetFile[] = CHANGESET; /* for win32, need writable buffer */
private	cset_t	copts;

int
makepatch_main(int ac, char **av)
{
	int	dash, c, i;
	char	*nav[15];
	char	*range = 0;

	dash = streq(av[ac-1], "-");
	nav[i=0] = "makepatch";
	while ((c = getopt(ac, av, "BCdFM;P:qr|sv", 0)) != -1) {
		if (i == 14) usage();
		switch (c) {
		    case 'B': copts.doBAM = 1; break;
		    case 'C': copts.compat = 1; break;
		    case 'd':					/* doc 2.0 */
			nav[++i] = "-d";
			break;
		    case 'F':					/* undoc */
			nav[++i] = "-F";
			break;
		    case 'M': copts.tooMany = atoi(optarg); break;
		    case 'P':
			copts.csetkey = optarg;
			break;
		    case 'r':					/* doc 2.0 */
		    	c = 'm';
			if (range) usage();
			/* makepatch idiom: -r1.0.. means -r.. */
			if (optarg && streq(optarg, "1.0..")) {
				optarg = &optarg[3];
			}
			range = malloc((optarg ? strlen(optarg) : 0) + 10);
			sprintf(range, "-%c%s", c, optarg ? optarg : "");
			nav[++i] = range;
		    	break;
		    case 's':					/* undoc? 2.0 */
			copts.serial = 1;
			break;
		    case 'q':					/* undoc? 2.0 */
			nav[++i] = "-q";
			break;
		    case 'v':					/* doc 2.0 */
			nav[++i] = "-v";
			break;
		    default: bk_badArg(c, av);
		}
	}
	if (getenv("_BK_NO_PATCHSFIO")) {
		copts.compat = 1;
		copts.tooMany = 0;
	}
	unless (range) {
		nav[++i] = "-m";
		nav[++i] = "-";
		dash = 0;
	}
	if (dash) nav[++i] = "-";
	nav[++i] = 0;
	getoptReset();
	i = cset_main(i, nav);
	if (range) free(range);
	return (i);
}

private	sccs	*cset;

/*
 * cset.c - changeset command
 */
int
cset_main(int ac, char **av)
{
	int	flags = 0;
	int	c, list = 0;
	int	ignoreDeleted = 0;
	char	*cFile = 0;
	int	rc = 1;
	RANGE	rargs = {0};

	if (streq(av[0], "makepatch")) copts.makepatch = 1;
	copts.notty = (getenv("BK_NOTTY") != 0);

	while ((c = getopt(ac, av, "5BCd|DFfhi;lm|M|qr|svx;", 0)) != -1) {
		switch (c) {
		    case 'B': copts.doBAM = 1; break;
		    case 'D': ignoreDeleted++; break;		/* undoc 2.0 */
		    case 'f': copts.force++; break;		/* undoc? 2.0 */
		    case 'F': copts.fastpatch++; break;		/* undoc */
		    case 'h': copts.historic++; break;		/* undoc? 2.0 */
		    case 'i':					/* doc 2.0 */
			if (copts.include || copts.exclude) usage();
			copts.include++;
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    case 'r':					/* doc 2.0 */
			if (streq(av[0], "makepatch")) {
				/* pretend -r<rev> is -m<rev> */
				c = 'm';
			} else {
				copts.listeach++;
			}
		    	/* fall through */
		    case 'l':
			if (c == 'l') copts.listeach++;
		    	/* fall through */
		    case 'd':					/* undoc? 2.0 */
			if (c == 'd') copts.doDiffs++;
		    	/* fall through */
		    case 'M':					/* doc 2.0 */
			if (c == 'M') {
				copts.mark++;
				/* documented idiom: -M1.0.. means -M.. */
				if (optarg && streq(optarg, "1.0..")) {
					optarg = &optarg[3];
				}
			}
			/* fall through */
		    case 'm':					/* undoc? 2.0 */
			if (c == 'm') copts.makepatch = 1;
		    	list |= 1;
			if (optarg && range_addArg(&rargs, optarg, 0)) {
				usage();
			}
			break;
		    case 'C':					/* doc 2.0 */
			unless (streq(av[0], "makepatch")) {
				/* XXX - this stomps on everyone else */
		    		list |= 1;
				copts.mark++;
				copts.remark++;
				copts.force++;
			}
			break;
		    case 'q':					/* doc 2.0 */
		    case 's': flags |= SILENT; break;		/* undoc? 2.0 */
		    case '5': copts.md5out = 1; break;		/* undoc 4.0 */
		    case 'v': copts.verbose++; break;		/* undoc? 2.0 */
		    case 'x':					/* doc 2.0 */
			if (copts.include || copts.exclude) usage();
			copts.exclude++;
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    default: bk_badArg(c, av);
		}
	}

	if (proj_isProduct(0) && !copts.remark) copts.hide_comp++;

	if (rargs.rstop && (list != 1)) {
		fprintf(stderr, "%s: only one rev allowed with -t\n", av[0]);
		usage();
	}
	if ((copts.include || copts.exclude) &&
	    (copts.doDiffs || copts.makepatch ||
	    copts.listeach || copts.mark || copts.force || copts.remark ||
	    copts.historic || av[optind])) {
	    	fprintf(stderr, "cset -x|-i must be stand alone.\n");
		usage();
	}
	if (av[optind] && streq(av[optind], "-")) {
		optind++;
		copts.dash++;
	}
	if (av[optind]) {
		unless (isdir(av[optind])) {
			if (flags & NEWFILE) {
				mkdirp(av[optind]);
			}
		}
		if (chdir(av[optind])) {
			perror(av[optind]);
			return (1);
		}
	} else if (flags & NEWFILE) {
		fprintf(stderr, "cset: must specify package root.\n");
		return (1);
	} else if (proj_cd2root()) {
		fprintf(stderr, "cset: cannot find package root.\n");
		return (1);
	}
	if (getenv("_BK_NO_FASTPATCH")) {
		copts.fastpatch = 0;
	}
	/* with a onepass patch, nothing is too much */
	if (copts.fastpatch) copts.tooMany = 0;

	/*
	 * If doing include/exclude, go do it.
	 */
	if (copts.include) return (cset_inex(flags, "-i", rargs.rstart));
	if (copts.exclude) return (cset_inex(flags, "-x", rargs.rstart));

	cset = sccs_init(csetFile, 0);
	if (!cset) return (101);
	if (copts.csetkey) cset->state |= S_READ_ONLY;
	copts.mixed = !LONGKEY(cset);

	if (list && !rargs.rstart && !copts.dash && !copts.remark) {
		fprintf(stderr, "cset: must specify a revision.\n");
		sccs_free(cset);
		cset_exit(1);
	}

	if (list) {
		unless (copts.dash) {
		    if (range_process("cset", cset, RANGE_SET, &rargs)) {
			    goto err;
		    }
		    range_markMeta(cset);	/* all ranges include tags */
		}
		sig_default();
		csetlist(&copts, cset);
		rc = 0;
err:
		sccs_free(cset);
		if (cFile) free(cFile);
		return (rc);
	}
	fprintf(stderr, "cset: bad options\n");
	return (1);
}

private void
cset_exit(int n)
{
	if (copts.pid) {
		printf(PATCH_ABORT);
		fclose(stdout);
		waitpid(copts.pid, 0, 0);
	}
	fflush(stdout);
	_exit(n);
}

private pid_t
spawn_checksum_child(void)
{
	pid_t	pid;
	int	pfd;
	char	*av[3] = {"bk", "_adler32", 0};

	/*
	 * spawn a child with a write pipe
	 */
	pid = spawnvpio(&pfd, 0, 0, av);

	/*
	 * Connect our stdout to the write pipe
	 * i.e parent | child
	 */
	close(1);
	dup2(pfd, 1);
	close(pfd);
	return pid;
}

/*
 * Create the initial empty cset.
 */
int
cset_setup(int flags)
{
	sccs	*cset;
	delta	*d = 0;
	int	fd;

	cset = sccs_init(csetFile, 0);
	assert(cset && cset->proj);

	if (flags & DELTA_DONTASK) unless (d = comments_get(d)) goto intr;
	unless (d = host_get(d)) goto intr;
	unless (d = user_get(d)) goto intr;
	cset->state |= S_CSET;
	cset->xflags |= X_LONGKEY;
	if (sccs_delta(cset, flags|DELTA_EMPTY|NEWFILE, d, 0, 0, 0) == -1) {
		sccs_whynot("cset", cset);
intr:		sccs_free(cset);
		sfileDone();
		comments_done();
		host_done();
		user_done();
		return (1);
	}
	fd = creat(IDCACHE, GROUP_MODE);
	write(fd, "#$sum$ 0\n", 9);
	close(fd);
	sccs_free(cset);
	comments_done();
	host_done();
	user_done();
	sfileDone();
	return (0);
}

/*
 * Do whatever it is they want.
 */
private void
doit(cset_t *cs, sccs *sc)
{
	unless (sc) {
		if (cs->doDiffs && cs->makepatch) printf("\n");
		return;
	}
	if (copts.hide_comp && CSET(sc) && proj_isComponent(sc->proj)) return;
	cs->nfiles++;
	if (cs->doDiffs) {
		doDiff(sc, DF_UNIFIED);
	} else if (cs->makepatch) {
		sccs_patch(sc, cs);
	} else if (cs->mark) {
		doMarks(cs, sc);
	} else {
		doSet(sc);
	}
}

private void
header(sccs *cset, int diffs)
{
	char	*dspec =
		"$each(:FD:){# Proj:\t(:FD:)\n}# ID:\t:KEY:\n";
	int	save = cset->state;
	char	pwd[MAXPATH];

	if (diffs) {
		printf("\
# This is a BitKeeper patch.  What follows are the unified diffs for the\n\
# set of deltas contained in the patch.  The rest of the patch, the part\n\
# that BitKeeper cares about, is below these diffs.\n");
	}
	sccs_prsdelta(cset, sccs_ino(cset), 0, dspec, stdout);
	printf("# User:\t%s\n", sccs_getuser());
	printf("# Host:\t%s\n", sccs_gethost() ? sccs_gethost() : "?");
	getcwd(pwd, sizeof(pwd));
	printf("# Root:\t%s\n", pwd);
	cset->state = save;
}

/*
 * Depending on what we are doing, either
 * mark all deltas in this cset, or
 * just mark the cset boundry.
 */
private void
markThisCset(cset_t *cs, sccs *s, delta *d)
{
	if (cs->mark || TAG(d)) {
		d->flags |= D_SET;
		return;
	}
	do {
		d->flags |= D_SET;
		if (d->merge) {
			delta	*e = MERGE(s, d);

			assert(e);
			unless (e->flags & (D_SET|D_CSET)) {
				markThisCset(cs, s, e);
			}
		}
		d = PARENT(s, d);
	} while (d && !(d->flags & (D_SET|D_CSET)));
}

/*
 * Return true if the two keys describe the same file.
 * If we are in X_LONGKEY it's easy, they match or they don't.
 * Otherwise we'll try short versions.
 */
private int
sameFile(cset_t *cs, char *key1, char *key2)
{
	char	*a, *b;
	int	ret;

	if (streq(key1, key2)) return (1);
	unless (cs->mixed) return (0);
	if (a = sccs_iskeylong(key1)) *a = 0;
	if (b = sccs_iskeylong(key2)) *b = 0;
	ret = streq(key1, key2);
	if (a) *a = '|';
	if (b) *b = '|';
	return (ret);
}

private int
doKey(cset_t *cs, char *key, char *val, MDBM *goneDB)
{
	static	MDBM *idDB;
	static	sccs *sc;
	static	char *lastkey;
	delta	*d;

	/*
	 * Cleanup code, called to reset state.
	 */
	unless (key) {
		if (idDB) {
			mdbm_close(idDB);
			idDB = 0;
		}
		if (lastkey) {
			free(lastkey);
			lastkey = 0;
		}
		if (sc) {
			doit(cs, sc);
			unless (sc == cset) sccs_free(sc);
			sc = 0;
		}
		doit(cs, 0);
		return (0);
	}

	/*
	 * If we have a match, just mark the delta and return,
	 * we'll finish later.
	 *
	 * With long/short keys mixed, we have to be a little careful here.
	 */
	if (lastkey && sameFile(cs, lastkey, key)) goto markkey;

	/*
	 * This would be later - do the last file and clean up.
	 */
	if (sc) {
		doit(cs, sc);
		unless (sc == cset) sccs_free(sc);
		free(lastkey);
		sc = 0;
		lastkey = 0;
	}

	/*
	 * Set up the new file.
	 */
	lastkey = strdup(key);
	unless (idDB || (idDB = loadDB(IDCACHE, 0, DB_IDCACHE))) {
		perror("idcache");
	}
	if (cset && streq(lastkey, proj_rootkey(0))) {
		sc = cset;
	} else {
		sc = sccs_keyinit(0, lastkey, INIT_NOWARN, idDB);
	}
	if (sc) {
		unless (sc->cksumok) {
			sccs_free(sc);
			return (-1);
		}
	} else {
		if (gone(lastkey, goneDB)) {
			free(lastkey);
			lastkey = 0;
			return (0);
		}
		fprintf(stderr, "cset: unable to keyinit %s\n", lastkey);
		free(lastkey);
		lastkey = 0;
		return (cs->force ? 0 : -1);
	}
markkey:
	unless (d = sccs_findKey(sc, val)) {
		/* OK to have missing keys if the gone file told us so */
		if (gone(key, goneDB) || gone(val, goneDB)) return (0);

		fprintf(stderr,
		    "cset: cannot find\n\t%s in\n\t%s\n", val, sc->sfile);
		return (cs->force ? 0 : -1);
	}
	markThisCset(cs, sc, d);
	return (0);
}

/*
 * Mark the deltas listed in the diff file.  Ignore first line.
 * XXX: change from 0a0 format to I0 0 format
 */
private int
marklist(char *file)
{
	char	*t;
	FILE	*list;
	char	buf[MAXPATH*2];
	cset_t	cs;

	bzero(&cs, sizeof(cs));
	cs.mark++;
	cs.fastpatch = copts.fastpatch;	/* sccs_patch */

	unless (list = fopen(file, "r")) {
		perror(file);
		return (-1);
	}

	/* eat the first line ... */
	unless (fnext(buf, list)) {
		fprintf(stderr, "cset: marking new list: empty file\n");
		fclose(list);
		return (-1);
	}
	/*
	 * Now do the real data.
	 * XXX: fix when replace 0a0 with I0 0.  &buf[2] => buf
	 */
	while (fnext(buf, list)) {
		chop(buf);
		t = separator(&buf[2]);
		assert(t);
		*t++ = 0;
		if (doKey(&cs, &buf[2], t, 0)) {
			fclose(list);
			return (-1);
		}
	}
	doKey(&cs, 0, 0, 0);
	fclose(list);
	return (0);
}

/*
 * sort rootkeys by pathname first
 * lines:
 *    <serial>\t<rootkey> <deltakey>
 */
int
cset_bykeys(const void *a, const void *b)
{
	char	*s1 = *(char**)a;
	char	*s2 = *(char**)b;
	char	*p1 = strchr(s1, '\t');	/* start of rootkey */
	char	*p2 = strchr(s2, '\t');
	char	*d1 = separator(p1); /* start of delta key */
	char	*d2 = separator(p2);
	int	rc;

	*d1 = 0;
	*d2 = 0;
	rc = keycmp(p1+1, p2+1);
	*d1 = ' ';
	*d2 = ' ';
	return (rc);
}

/*
 * sfile order: sort rootkeys by serial first, then whole rootkey
 * lines:
 *    <serial>\t<rootkey> <deltakey>\0
 *    initial \0 - means line is deleted
 */
int
cset_byserials(const void *a, const void *b)
{
	char	*s1 = *(char**)a;
	char	*s2 = *(char**)b;
	char	*p1, *p2;
	char	*d1, *d2;
	int	rc;

	if (!*s1 || !*s2) return (!*s1 - !*s2);	/* deleted go at end */

	unless (rc = (atoi(s2) - atoi(s1))) {
		p1 = strchr(s1, '\t');	/* start of rootkey */
		p2 = strchr(s2, '\t');
		d1 = separator(p1); /* start of delta key */
		d2 = separator(p2);
		*d1 = 0;
		*d2 = 0;
		rc = keycmp(p1+1, p2+1);
		*d1 = ' ';
		*d2 = ' ';
		assert(rc);
	}
	return (rc);
}

/*
 * List all the revisions which make up a range of changesets.
 * The list is sorted for performance.
 */
private void
csetlist(cset_t *cs, sccs *cset)
{
	char	*rk, *t;
	char	buf[MAXPATH*2];
	char	*csetid;
	int	status, i;
	delta	*d;
	MDBM	*goneDB = 0;

	if (cs->dash) {
		while(fgets(buf, sizeof(buf), stdin)) {
			chop(buf);
			if (copts.serial) {
				d = sfind(cset, atoi(buf));
			} else {
				d = sccs_findrev(cset, buf);
			}
			unless (d) {
				fprintf(stderr,
				    "cset: no rev like %s in %s\n",
				    buf, cset->gfile);
				cset_exit(1);
			}
			d->flags |= D_SET;
		}
	}

	/* Save away the cset id */
	sccs_sdelta(cset, sccs_ino(cset), buf);
	csetid = strdup(buf);

	if ((cs->cweave = cset_mkList(cset)) == (char **)-1) {
		goto fail;
	}
	if (cs->verbose > 5) {;
		EACH(cs->cweave) printf("%s\n", cs->cweave[i]);
	}
	if (!cs->mark && hasLocalWork(GONE)) {
		fprintf(stderr,
		    "cset: must commit local changes to %s\n", GONE);
		cs->makepatch = 0;
		goto fail;
	}
	if (!cs->mark && hasLocalWork(ALIASES)) {
		fprintf(stderr,
		    "cset: must commit local changes to %s\n", ALIASES);
		cs->makepatch = 0;
		goto fail;
	}
	goneDB = loadDB(GONE, 0, DB_GONE);

	/* checksum the output */
	if (cs->makepatch) {
		cs->pid = spawn_checksum_child();
		if (cs->pid == -1) goto fail;
	}
	if (cs->makepatch || cs->doDiffs) header(cset, cs->doDiffs);
	if (cs->doDiffs && cs->makepatch) {
		fputs("\n", stdout);
		fputs(PATCH_DIFFS, stdout);
	}
again:	/* doDiffs can make it two pass */
	if (!cs->doDiffs && cs->makepatch) {
		fputs("\n", stdout);
		fputs(PATCH_PATCH, stdout);
		fputs(cs->fastpatch ? PATCH_FAST : PATCH_CURRENT, stdout);
		fputs(PATCH_REGULAR, stdout);
		fputs("\n", stdout);
	}

	/*
	 * Do the ChangeSet deltas first, takepatch needs it to be so.
	 */
	sortLines(cs->cweave, number_sort); /* sort by serials */
	cs->lasti = 0;
	doit(cs, cset);

	/*
	 * Now do the real data.
	 */
	sortLines(cs->cweave, cset_bykeys); /* sort by rootkeys */
	EACH (cs->cweave) {
		rk = strchr(cs->cweave[i], '\t');
		++rk;
		t = separator(rk); *t++ = 0;
		if (sameFile(cs, csetid, rk)) goto next; /* skip ChangeSet */
		if (doKey(cs, rk, t, goneDB)) {
			fprintf(stderr,
			    "File named by key\n\t%s\n\tis missing and key is "
			    "not in gone file, aborting.\n", rk);
			fflush(stderr); /* for win32 */
			goto fail;
		}
next:		t[-1] = ' ';
	}
	if (cs->doDiffs && cs->makepatch) {
		doKey(cs, 0, 0, goneDB);
		cs->doDiffs = 0;
		fputs(PATCH_END, stdout);
		goto again;
	}
	doKey(cs, 0, 0, goneDB);
	freeLines(cs->cweave, free);
	if (cs->verbose && cs->makepatch) {
		fprintf(stderr,
		    "makepatch: patch contains %d changesets from %d files\n",
		    cs->ncsets, cs->nfiles);
	} else if (cs->verbose && cs->mark && cs->ndeltas) {
		fprintf(stderr,
		    "cset: marked %d revisions in %d files\n",
		    cs->ndeltas, cs->nfiles);
	}
	if (cs->makepatch) {
		if (cs->BAM || cs->sfiles) fputs("== @SFIO@ ==\n\n", stdout);
		fputs(PATCH_END, stdout);
		fflush(stdout);
		if (cs->BAM || cs->sfiles) {
			int	i;
			FILE	*f;

			/* try and fetch from my local BAM pool but recurse
			 * through to the server.
			 */
			f = popen("bk sfio -oqB -", "w");
			EACH(cs->BAM) fprintf(f, "%s\n", cs->BAM[i]);
			freeLines(cs->BAM, free);
			cs->BAM = 0;
			sortLines(cs->sfiles, 0);
			EACH(cs->sfiles) fprintf(f, "%s\n", cs->sfiles[i]);
			freeLines(cs->sfiles, free);
			cs->sfiles = 0;
			if (pclose(f)) {
				fprintf(stderr, "BAM sfio -o failed.\n");
				goto fail;
			}
		}
		fclose(stdout);
		if (waitpid(cs->pid, &status, 0) != cs->pid) {
			perror("waitpid");
		}
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
			fprintf(stderr,
		  "makepatch: checksum process exited abnormally, status %d\n",
				status);
		}
		cs->pid = 0;
	}
	free(csetid);
	if (goneDB) mdbm_close(goneDB);
	return;

fail:
	if (cs->makepatch) {
		printf(PATCH_ABORT);
		fclose(stdout);
		waitpid(cs->pid, &status, 0);	/* for win32: child inherited */
						/* a low level csort handle */
	}
	free(csetid);
	if (goneDB) mdbm_close(goneDB);
	cset_exit(1);
}

/*
 * Spit out the diffs.
 */
private void
doDiff(sccs *sc, char kind)
{
	delta	*d, *e = 0;

	if (CSET(sc)) return;		/* no changeset diffs */
	for (d = sc->table; d; d = NEXT(d)) {
		if (d->flags & D_SET) {
			e = d;
		} else if (e) {
			break;
		}
	}
	for (d = sc->table; d && !(d->flags & D_SET); d = NEXT(d));
	if (!d) return;
	unless (PARENT(sc, e)) {
		printf("--- New file ---\n+++ %s\t%s\n",
		    sc->gfile, sccs_ino(sc)->sdate);
		sccs_get(sc, 0, 0, 0, 0, PRINT|SILENT, "-");
		printf("\n");
		return;
	}
	e = PARENT(sc, e);
	if (e == d) return;
	sccs_diffs(sc, e->rev, d->rev, 0, kind, stdout);
}

#if 0
/*
 * Print a range suitable for diffs.
 * XXX - does not make sure that they are both on the trunk.
 */
private void
doEndpoints(cset_t *cs, sccs *sc)
{
	delta	*d, *earlier = 0, *later = 0;

	if (CSET(sc)) return;		
	for (d = sc->table; d; d = NEXT(d)) {
		unless (d->flags & D_SET) continue;
		unless (later) {
			later = d;
		} else {
			earlier = d;
		}
	}
	assert(later);
	if (!earlier) earlier = later->parent;
	printf("%s%c%s..%s\n",
	    sc->gfile, BK_FS, earlier ? earlier->rev : "1.0", later->rev);
}
#endif

private void
doMarks(cset_t *cs, sccs *s)
{
	delta	*d;
	int	did = 0;

	/*
	 * Throw away the existing marks if we are rebuilding.
	 */
	if (cs->remark) sccs_clearbits(s, D_CSET);

	for (d = s->table; d; d = NEXT(d)) {
		if ((d->type == 'D') && (d->flags & D_SET)) {
			if (cs->force || !(d->flags & D_CSET)) {
				if (cs->verbose > 2) {
					fprintf(stderr, "Mark %s%c%s\n",
					    s->gfile, BK_FS, d->rev);
				}
				d->flags |= D_CSET;
				cs->ndeltas++;
				did++;
			}
		}
	}
	if (did) {
		sccs_newchksum(s);
		if ((cs->verbose > 1) && did) {
			fprintf(stderr,
			    "Marked %d csets in %s\n", did, s->gfile);
		} else if (cs->verbose > 1) {
			fprintf(stderr,
			    "Set CSETMARKED flag in %s\n", s->sfile);
		}
	}
}

/*
 * Do the set listing
 */
private void
doSet(sccs *sc)
{
	delta	*d;
	char	key[MD5LEN];

	for (d = sc->table; d; d = NEXT(d)) {
		if (d->flags & D_SET) {
			printf("%s", sc->gfile);
		    	if (copts.historic) printf("%c%s", BK_FS, d->pathname);
			if (copts.md5out) {
				sccs_md5delta(sc, d, key);
				printf("%c%s\n", BK_FS, key);
			} else {
				printf("%c%s\n", BK_FS, d->rev);
			}
		}
	}
}

/*
 * Add a delta to the ChangeSet.
 *
 * XXX - this could check to make sure we are not adding 1.3 to a cset LOD
 * which already has 1.5 from the same file.
 */
private void
add(FILE *diffs, char *buf)
{
	sccs	*s;
	char	*p, *rev = 0;	/* lint */
	delta	*d;

	unless (chomp(buf) && (rev = strrchr(buf, BK_FS))) {
		fprintf(stderr, "cset: bad file:rev format: %s\n", buf);
		system("bk clean ChangeSet");
		cset_exit(1);
	}
	*rev++ = 0;

	/*
	 * XXX Optimization note: We should probably check for ChangeSet
	 * file first before we waste cpu to call sccs_init()
	 * This should be a win if we have complex graph and large 
	 * ChangeSet file. Since we ae going to sccs_free() and
	 * return anyway..
	 */
	unless (s = sccs_init(buf, 0)) {
		fprintf(stderr, "cset: can't init %s\n", buf);
		system("bk clean -q ChangeSet");
		cset_exit(1);
	}

	/*
	 * This is really testing two things:
	 * a) If I'm a ChangeSet file and not in an ensemble
	 * b) If I'm a ChangeSet file and not a component
	 */
	if (CSET(s) && !proj_isComponent(s->proj)) {
		sccs_free(s);
		return;
	}
	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "cset: can't find %s in %s\n", rev, buf);
		system("bk clean -q ChangeSet");
		cset_exit(1);
	}

	p = basenm(buf);
	*p = 'd';
	if (d == sccs_top(s)) unlink(buf); /* remove d.file */

	sccs_sdelta(s, sccs_ino(s), buf);
	fprintf(diffs, "> %s ", buf);
	sccs_sdelta(s, d, buf);
	fprintf(diffs, "%s\n", buf);
	sccs_free(s);

}

/*
 * Read file:rev from stdin and apply those to the changeset db.
 * Edit the ChangeSet file and add the new stuff to that file and
 * leave the file sorted.
 * Close the cset sccs* when done.
 */
private delta	*
mkChangeSet(sccs *cset, char *files, FILE *diffs)
{
	delta	*d;
	FILE	*f = fopen(files, "rt");
	char	buf[MAXPATH];

	/*
	 * Edit the ChangeSet file - we need it edited to modify it as well
	 * as load in the current state.
	 * If the edit flag is off, then make sure the file is already edited.
	 */
	unless ((cset->state & (S_SFILE|S_PFILE)) == (S_SFILE|S_PFILE)) {
		int flags = GET_EDIT|GET_SKIPGET|SILENT;
		if (sccs_get(cset, 0, 0, 0, 0, flags, "-")) {
			unless (BEEN_WARNED(cset)) {
				fprintf(stderr,
				    "cset: get -eg of ChangeSet failed\n");
				cset_exit(1);
			}
		}
	}
	d = sccs_dInit(0, 'D', cset, 0);
	/*
	 * XXX we need to insist d->hostname is non-null here,
	 * otherwise it will inherit hostname from its ancestor
	 * which will cause cset -i/-L to fail since
	 * the signiture do not match
	 */
	assert(d->hostname && d->hostname[0]);

	fprintf(diffs, "0a0\n"); /* fake diff header */

	/*
	 * Read each file:rev from files and add that to the cset.
	 * add() will ignore the ChangeSet entry itself.
	 */
	assert(f && files);
	while (fgets(buf, sizeof(buf), f)) {
		add(diffs, buf);
	}
	fclose(f);

#ifdef CRAZY_WOW
	Actually, this isn't so crazy wow.  I don't know what problem this
	caused but I believe the idea was that we wanted time increasing
	across all deltas in all files.  Sometimes the ChangeSet timestamp
	is behind the deltas in that changeset which is clearly wrong.

	Proposed fix is to record the highest fudged timestamp in global
	file in the repo and make sure the cset file is always >= that one.
	Should be done in the proj struct and written out when we free it
	if it changed.

	/*
	 * Adjust the date of the new rev, scripts can make this be in the
	 * same second.  It's OK that we adjust it here, we are going to use
	 * this delta * as part of the checkin on this changeset.
	 */
	if (d->date <= cset->table->date) {
		d->dateFudge = (cset->table->date - d->date) + 1;
		d->date += d->dateFudge;
	}
	/* Add ChangeSet entry */
	sccs_sdelta(cset, sccs_ino(cset), buf);
	fprintf(diffs, "> %s", buf);
	sccs_sdelta(cset, d, buf);
	fprintf(diffs, " %s\n", buf);
#endif
	return (d);
}

int
csetCreate(sccs *cset, int flags, char *files, char **syms)
{
	delta	*d;
	int	error = 0;
	int	fd0;
	MMAP	*diffs;
	FILE	*fdiffs;
	char	filename[MAXPATH];

	if ((cset->nextserial > 200) && getenv("BK_REGRESSION")) {
		fprintf(stderr, "Too many changesets for regressions.\n");
		exit(1);
	}

	bktmp(filename, "cdif");
	unless (fdiffs = fopen(filename, "w")) {
		perror(filename);
		sccs_free(cset);
		cset_exit(1);
	}

	d = mkChangeSet(cset, files, fdiffs); /* write change set to diffs */

	fclose(fdiffs);
	unless (diffs = mopen(filename, "b")) {
		perror(filename);
		sccs_free(cset);
		unlink(filename);
		cset_exit(1);
	}

	/* for compat with old versions of BK not using ensembles */
	if (proj_isComponent(cset->proj)) {
		touch("SCCS/d.ChangeSet", 0644);
	} else {
		d->flags |= D_CSET;
	}


	/*
	 * Make /dev/tty where we get input.
	 * XXX This really belongs in port/getinput.c
	 *     We shouldn't do this if we are not getting comments
	 *     interactively.
	 */
	fd0 = dup(0);
	close(0);
	if (open(DEV_TTY, 0, 0) < 0) {
		dup2(fd0, 0);
		close(fd0);
		fd0 = -1;
	}
	if (flags & DELTA_DONTASK) d = comments_get(d);
	if (sccs_delta(cset, flags, d, 0, diffs, syms) == -1) {
		sccs_whynot("cset", cset);
		error = -1;
		goto out;
	}
	if (fd0 >= 0) {
		dup2(fd0, 0);
		close(fd0);
		fd0 = -1;
	}
	if (marklist(filename)) {
		error = -1;
		goto out;
	}

out:	unlink(filename);
	comments_done();
	return (error);
}

/*
 * All the deltas we want are marked so print them out.
 * Note: takepatch depends on table order so don't change that.
 */
private	void
sccs_patch(sccs *s, cset_t *cs)
{
	delta	*d;
	ser_t	*patchmap = 0;	/* patch map */
	int	rc = 0;
	int	deltas = 0, csets = 0, last = 0;
	int	outdiffs = 0;
	int	prs_flags = (PRS_PATCH|SILENT);
	int	i, n, newfile, hastip;
	delta	**list;
	char	*gfile = 0;
	ticker	*tick = 0;

        if (sccs_admin(s, 0, SILENT|ADMIN_BK, 0, 0, 0, 0, 0, 0, 0)) {
		fprintf(stderr, "Patch aborted, %s has errors\n", s->sfile);
		fprintf(stderr,
		    "Run ``bk -r check -a'' for more information.\n");
		cset_exit(1);
	}

	if (cs->verbose>1) fprintf(stderr, "makepatch: %s", s->gfile);

	/*
	 * Build a list of the deltas we're sending
	 * Clear the D_SET flag because we need to be able to do one at
	 * a time when sending the cset diffs.
	 */
	newfile = s->tree->flags & D_SET;
	hastip = s->table->flags & D_SET;
	list = 0;
	for (n = 0, d = s->table; d; d = NEXT(d)) {
		unless (d->flags & D_SET) continue;
		unless (gfile) gfile = CSET(s) ? GCHANGESET : d->pathname;
		n++;
		unless (last) last = n;
		list = (delta **)addLine((char **)list, d);
		if (d->hash && BAM(s) && copts.doBAM) {
			cs->BAM = addLine(cs->BAM,
			    sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC));
		}
		d->flags &= ~D_SET;
	}
	unless (n) return;
	assert(gfile);

	/*
	 * If the receiver understands then just send the entire sfile
	 * for newfiles and for any files with more patches than
	 * threshold.  Don't send the ChangeSet file.  Also don't send
	 * a sfile if the tip delta is not part of the patch.
	 * (pull-r or pending deltas)
	 */
	if (!CSET(s) && hastip && !MONOTONIC(s) &&
	    ((!cs->compat && newfile) ||
		((cs->tooMany > 0) && (n >= cs->tooMany)))) {
		cs->sfiles = addLine(cs->sfiles, strdup(s->sfile));
		free(list);
		if (cs->verbose > 1) fprintf(stderr, "\n");
		return;
	}
	/*
	 * For each file, spit out file seperators when the filename
	 * changes.
	 * Spit out the root rev so we can find if it has moved.
	 */
	if ((cs->verbose == 2) && !cs->notty) {
		tick = progress_start(PROGRESS_SPIN, 0);
	}
	if (cs->fastpatch) {
		prs_flags |= PRS_FASTPATCH;
		patchmap = calloc(s->nextserial, sizeof(ser_t));
	}
	for (i = n; i > 0; i--) {
		d = list[i];
		if (patchmap) patchmap[d->serial] = n - i + 1;
		assert(d);
		if (cs->verbose > 2) fprintf(stderr, " %s", d->rev);
		if (tick) progress(tick, 0);
		if (i == n) {
			unless (s->gfile) {
				fprintf(stderr, "\n%s%c%s has no path\n",
				    s->gfile, BK_FS, d->rev);
				cset_exit(1);
			}
			printf("== %s ==\n", gfile);
			if (newfile) {
				printf("New file: %s\n", d->pathname);
				sccs_perfile(s, stdout);
			}
			s->rstop = s->rstart = s->tree;
			if (copts.csetkey && CSET(s)) {
				fputs(copts.csetkey, stdout);
			} else {
				sccs_pdelta(s, sccs_ino(s), stdout);
			}
			printf("\n");
		}

		/*
		 * For each file, also eject the parent of the rev.
		 */
		if (d->pserial) {
			sccs_pdelta(s, PARENT(s, d), stdout);
			printf("\n");
		}
		if (copts.csetkey && CSET(s)) d->flags &= ~D_CSET;
		s->rstop = s->rstart = d;
		if (sccs_prs(s, prs_flags, 0, NULL, stdout)) cset_exit(1);
		printf("\n");
		/* takepatch lists tags+commits so we do too */
		if (CSET(s)) csets++;
		if (cs->fastpatch) {
			/*
			 * put a multi-delta patch on the newest delta
			 * pedantically, put out a patch for I1-E1
			 * can make for better diff -r results.
			 */
			outdiffs += d->added + d->deleted + (d->serial == 1);
			if ((i == last) && outdiffs) {
				rc = sccs_patchDiffs(s, patchmap, "-");
			}
		} else unless (TAG(d)) {
			// Nested XXX
			if (CSET(s)) {
				if (d->added) rc = cset_diffs(cs, d->serial);
			} else if (!BAM(s)) {
				rc = sccs_getdiffs(s, d->rev, GET_BKDIFFS, "-");
			}
		}
		if (rc) { /* sccs_getdiffs errored */
			fprintf(stderr,
			    "Patch aborted, sccs_getdiffs %s failed\n",
			    s->sfile);
			cset_exit(1);
		}
		printf("\n");
		deltas++;
	}
	if (patchmap) {
		free(patchmap);
		patchmap = 0;
	}
	if (tick) {
		progress_done(tick, 0);
		fprintf(stderr, " %d revisions\n", deltas);
	} else if (cs->verbose > 1) {
		fprintf(stderr, "\n");
	}
	cs->ndeltas += deltas;
	cs->ncsets += csets;
	if (list) free(list);
}

private int
cset_diffs(cset_t *cs, ser_t ser)
{
	int	i;
	ser_t	n;
	char	*t;

	printf("0a0\n");
	/* walk annotated list from last match */
	EACH_START(cs->lasti, cs->cweave, i) {
		t = cs->cweave[i];
		n = atoi(t);
		if (ser == n) {
			t = strchr(t, '\t');
			printf("> %s\n", t + 1);
		} else if (n > ser) {
			break;
		}
	}
	cs->lasti = i;		/* save last match */
	return (0);
}

char **
cset_mkList(sccs *cset)
{
	char	**list;
	char	cat[MAXPATH];

	bktmp(cat, "catZ");
	/*
	 * Get the list of key tuples in a lines array
	 */
	if (sccs_cat(cset, GET_SERIAL|GET_NOHASH|PRINT, cat)) {
		sccs_whynot("cset", cset);
		unlink(cat);
		return ((char **)-1);
	}
	list = file2Lines(0, cat);
	unlink(cat);
	return (list);
}
