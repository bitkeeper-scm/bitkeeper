/* Copyright (c) 1999 Larry McVoy */
#include "system.h"
#include "sccs.h"
#include "range.h"
#include "zlib/zlib.h"

WHATSTR("@(#)%K%");

typedef	struct cset {
	/* bits */
	int	mixed;		/* if set, then both long and short keys */
	int	csetOnly;	/* if set, do logging ChangeSet */
	int     metaOnly;	/* if set, do only metadata */
	int	makepatch;	/* if set, act like makepatch */
	int	listeach;	/* if set, list revs 1/line */
	int	mark;		/* act like csetmark used to act */
	int	doDiffs;	/* prefix with unified diffs */
	int	force;		/* if set, then force past errors */
	int	remark;		/* clear & redo all the ChangeSet marks */
	int	dash;
	int	historic;	/* list the historic name */
	int	hide_cset;	/* exclude cset from file@rev list */
	int	include;	/* create new cset with includes */
	int	exclude;	/* create new cset with excludes */
	int	compat;		/* Do PATCH_COMPAT patches */
	int	serial;		/* the revs passed in are serial numbers */

	/* numbers */
	int	verbose;
	int	fromStart;	/* if we did -R1.1..1.2 */
	int	ndeltas;
	int	nfiles;
	pid_t	pid;		/* adler32 process id */
} cset_t;

private int	csetCreate(sccs *cset, int flags, char **syms);
private	void	csetlist(cset_t *cs, sccs *cset);
private	int	marklist(char *file);
private	void	csetDeltas(cset_t *cs, sccs *sc, delta *start, delta *d);
private	delta	*mkChangeSet(sccs *cset, FILE *diffs);
private	char	*file2str(char *f);
private	void	doSet(sccs *sc);
private	void	doMarks(cset_t *cs, sccs *sc);
private	void	doDiff(sccs *sc, char kind);
private	void	sccs_patch(sccs *, cset_t *);
private	void	cset_exit(int n);
private	char	csetFile[] = CHANGESET; /* for win32, need writable buffer */
private	cset_t	copts;
private char	*spin = "|/-\\";
int		cset_main(int ac, char **av);
extern	int	sane_main(int ac, char **av);

int
makepatch_main(int ac, char **av)
{
	int	dash, c, i;
	char	*nav[15];
	char	*range = 0;

	if (ac == 2 && streq("--help", av[1])) {
usage:		system("bk help -s makepatch");
		return (0);
	}
	dash = streq(av[ac-1], "-");
	nav[i=0] = "makepatch";
	while ((c = getopt(ac, av, "c|e|dr|sCqv")) != -1) {
		if (i == 14) goto usage;
		switch (c) {
		    case 'd':					/* doc 2.0 */
			nav[++i] = "-d";
			break;
		    case 'r':					/* doc 2.0 */
		    	c = 'm';
		    case 'c':					/* doc 2.0 */
		    case 'e':					/* doc 2.0 */
			if (range) goto usage;
			range = malloc((optarg ? strlen(optarg) : 0) + 10);
			sprintf(range, "-%c%s", c, optarg ? optarg : "");
			nav[++i] = range;
		    	break;
		    case 's':					/* undoc? 2.0 */
			copts.serial = 1;
			break;
		    case 'C':					/* undoc? 2.0 */
			copts.compat = 1;
			break;
		    case 'q':					/* undoc? 2.0 */
			nav[++i] = "-q";
			break;
		    case 'v':					/* doc 2.0 */
			nav[++i] = "-v";
			break;
		    default:
		    	goto usage;
		}
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

/*
 * cset.c - changeset command
 */
int
cset_main(int ac, char **av)
{
	sccs	*cset;
	int	dflags = 0, flags = 0;
	int	c, list = 0;
	char	**syms = 0;
	int	ignoreDeleted = 0;
	char	*cFile = 0;
	char	allRevs[6] = "1.0..";
	char	buf[100];
	RANGE_DECL;

	debug_main(av);

	if (ac > 1 && streq("--help", av[1])) {
usage:		sprintf(buf, "bk help %s", av[0]);
		system(buf);
		return (1);
	}

	if (streq(av[0], "makepatch")) copts.makepatch = 1;

	while (
	    (c =
	    getopt(ac, av, "c|e|Cd|DfHhi;lm|M|pqr|sS;vx;y|Y|")) != -1) {
		switch (c) {
		    case 'D': ignoreDeleted++; break;		/* undoc 2.0 */
		    case 'f': copts.force++; break;		/* undoc? 2.0 */
		    case 'h': copts.historic++; break;		/* undoc? 2.0 */
		    case 'H': copts.hide_cset++; break;		/* undoc 2.0 */
		    case 'i':					/* doc 2.0 */
			if (copts.include || copts.exclude) goto usage;
			copts.include++;
			r[rd++] = optarg;
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
		    case 'e':					/* undoc? 2.0 */
			if (c == 'e') {
				copts.metaOnly++;
				copts.makepatch = 1;
			}
			/* fall through */
		    case 'c':					/* undoc? 2.0 */
			if (c == 'c') {
				copts.csetOnly++;
				copts.makepatch = 1;
			}
			/* fall through */
		    case 'M':					/* doc 2.0 */
			if (c == 'M') copts.mark++;
			/* fall through */
		    case 'm':					/* undoc? 2.0 */
			if (c == 'm') copts.makepatch = 1;
		    	list |= 1;
			if (optarg) {
				r[rd++] = notnull(optarg);
				things += tokens(notnull(optarg));
			}
			break;
		    case 'C':					/* doc 2.0 */
			unless (streq(av[0], "makepatch")) {
				/* XXX - this stomps on everyone else */
		    		list |= 1;
				copts.mark++;
				copts.remark++;
				copts.force++;
				r[0] = allRevs;
				rd = 1;
				things = tokens(notnull(optarg));
			}
			break;
		    case 'p': flags |= PRINT; break;		/* doc 2.0 */
		    case 'q':					/* doc 2.0 */
		    case 's': flags |= SILENT; break;		/* undoc? 2.0 */
		    case 'v': copts.verbose++; break;		/* undoc? 2.0 */
		    case 'x':					/* doc 2.0 */
			if (copts.include || copts.exclude) goto usage;
			copts.exclude++;
			r[rd++] = optarg;
			break;
		    case 'y':					/* doc 2.0 */
			comments_save(optarg);
			dflags |= DELTA_DONTASK;
			break;
		    case 'Y':					/* doc 2.0 */
			comments_save(cFile = file2str(optarg));
			dflags |= DELTA_DONTASK;
			break;
		    case 'S': 					/* doc 2.0 */
				syms = addLine(syms, strdup(optarg)); break;

		    default:
			sprintf(buf, "bk help -s %s", av[0]);
			system(buf);
			return (1);
		}
	}

	if (copts.doDiffs && (copts.csetOnly || copts.metaOnly)) {
		fprintf(stderr, "Warning: ignoring -d option\n");
		copts.doDiffs = 0;
	}
	if ((things > 1) && (list != 1)) {
		fprintf(stderr, "%s: only one rev allowed with -t\n", av[0]);
		goto usage;
	}
	if ((copts.include || copts.exclude) &&
	    (copts.doDiffs || copts.csetOnly || copts.makepatch ||
	    copts.listeach || copts.mark || copts.force || copts.remark ||
	    copts.historic || copts.metaOnly || av[optind])) {
	    	fprintf(stderr, "cset -x|-i must be stand alone.\n");
		goto usage;
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
	} else if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "cset: cannot find package root.\n");
		return (1);
	}

	/*
	 * If doing include/exclude, go do it.
	 */
	if (copts.include) return (cset_inex(flags, "-i", r[0]));
	if (copts.exclude) return (cset_inex(flags, "-x", r[0]));

	cset = sccs_init(csetFile, flags & SILENT, 0);
	if (!cset) return (101);
	copts.mixed = !LONGKEY(cset);

	if (list && (things < 1) && !copts.dash) {
		fprintf(stderr, "cset: must specify a revision.\n");
		sccs_free(cset);
		cset_exit(1);
	}

	if (list) {
		int	expand;

		if (copts.dash) {
			expand = 0;
		} else if (r[1]) {
			expand = 3; 
		} else if (closedRange(r[0]) == 1) {
			expand = 3;
		} else {
			expand = 2;
		}
		RANGE("cset", cset, expand, 1);
		sig_default();
		csetlist(&copts, cset);
next:		sccs_free(cset);
		if (cFile) free(cFile);
		freeLines(syms);
		return (0);
	}

	/*
	 * Otherwise, go figure out if we have anything to add to the
	 * changeset file.
	 * XXX - should allow them to pick and choose for multiple
	 * changesets from one pending file.
	 */
	c = csetCreate(cset, dflags|flags, syms);
	if (cFile) free(cFile);
	freeLines(syms);
	return (c);
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
	pid = spawnvp_wPipe(av, &pfd, BIG_PIPE);

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
cset_setup(int flags, int ask)
{
	sccs	*cset;
	delta	*d = 0;
	int	fd;

	cset = sccs_init(csetFile, flags & SILENT, 0);
	assert(cset->proj);

	if (flags & DELTA_DONTASK) unless (d = comments_get(d)) goto intr;
	unless (d = host_get(d, ask)) goto intr;
	unless (d = user_get(d, ask)) goto intr;
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
	cs->nfiles++;
	if (cs->doDiffs) {
		doDiff(sc, DF_UNIFIED);
	} else if (cs->makepatch) {
		if (!cs->csetOnly || (sc->state & S_CSET)) {
			sccs_patch(sc, cs);
		}
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
	getRealCwd(pwd, sizeof(pwd));
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
	if (cs->mark) {
		d->flags |= D_SET;
		return;
	}
	do {
		d->flags |= D_SET;
		if (d->merge) {
			delta	*e = sfind(s, d->merge);

			assert(e);
			unless (e->flags & (D_SET|D_CSET)) {
				markThisCset(cs, s, e);
			}
		}
		d = d->parent;
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
	static	int doneFullRebuild;
	static	int doneFullRemark;
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
			sccs_free(sc);
			sc = 0;
		}
		doneFullRebuild = 0;
		doneFullRemark = 0;
		doit(cs, 0);
		return (0);
	}

	/*
	 * If we have a match, just mark the delta and return,
	 * we'll finish later.
	 *
	 * With long/short keys mixed, we have to be a little careful here.
	 */
	if (lastkey && sameFile(cs, lastkey, key)) {
		unless (d = sccs_findKey(sc, val)) {
			if (gone(val, goneDB)) return (0);
			return (cs->force ? 0 : -1);
		}
		markThisCset(cs, sc, d);
		return (0);
	}

	/*
	 * This would be later - do the last file and clean up.
	 */
	if (sc) {
		doit(cs, sc);
		sccs_free(sc);
		free(lastkey);
		sc = 0;
		lastkey = 0;
	}

	/*
	 * Set up the new file.
	 */
	unless (idDB || (idDB = loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS))) {
		perror("idcache");
	}
	lastkey = strdup(key);
retry:	sc = sccs_keyinit(lastkey, INIT_FIXSTIME, 0, idDB);
	unless (sc) {
		if (gone(lastkey, goneDB)) {
			free(lastkey);
			lastkey = 0;
			return (0);
		}

		/* cache miss, rebuild cache */
		unless (doneFullRebuild) {
			mdbm_close(idDB);
			if (sccs_reCache(!cs->verbose)) {
				fprintf(stderr,
				    "cset: cannot build %s\n", IDCACHE);
				// XXX - exit or not?
			}
			doneFullRebuild = 1;
			unless (idDB =
			    loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
				perror("idcache");
			}
			goto retry;
		}
		free(lastkey);
		lastkey = 0;
		return (cs->force ? 0 : -1);
	}

	unless (d = sccs_findKey(sc, val)) {
		/* OK to have missing keys if the gone file told us so */
		if (gone(val, goneDB)) return (0);

		fprintf(stderr,
		    "cset: cannot find\n\t%s in\n\t%s\n", val, sc->sfile);
		return (-1);
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
 * List all the revisions which make up a range of changesets.
 * The list is sorted for performance.
 */
private void
csetlist(cset_t *cs, sccs *cset)
{
	char	*t;
	FILE	*list = 0;
	char	buf[MAXPATH*2];
	char	cat[MAXPATH], csort[MAXPATH];
	char	*csetid;
	int	status;
	delta	*d;
	MDBM	*goneDB = 0;

	if (cs->dash) {
		while(fgets(buf, sizeof(buf), stdin)) {
			chop(buf);
			if (copts.serial) {
				d = sfind(cset, atoi(buf));
			} else {
				d = sccs_getrev(cset, buf, NULL, 0);
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

	gettemp(cat, "catZ");
	gettemp(csort, "csort");
	unless (cs->csetOnly) {
		/*
		 * Get the list of key tuples in a sorted file.
		 */
		if (sccs_cat(cset, GET_NOHASH|PRINT, cat)) {
			sccs_whynot("cset", cset);
			unlink(cat);
			goto fail;
		}
		if (sysio(cat, csort, 0, "bk", "_keysort", SYS)) {
			unlink(cat);
			goto fail;
		}
		chmod(csort, TMP_MODE);		/* in case we don't unlink */
		unlink(cat);
		if (cs->verbose > 5) {;
			sys("cat", csort, SYS);
		}
		if (exists(SGONE)) {
			char tmp_gone[MAXPATH];

			bktemp(tmp_gone);
			sysio(0, tmp_gone, 0, "bk", "get", "-kpsC", GONE, SYS);
			goneDB = loadDB(tmp_gone, 0, DB_KEYSONLY|DB_NODUPS);
			unlink(tmp_gone);
		}
	} else {
		close(creat(csort, TMP_MODE));
	}
	unlink(cat);
	unless (list = fopen(csort, "rt")) { /* win32 sort used text mode */
		perror(buf);
		goto fail;
	}

	/* checksum the output */
	if (cs->makepatch && !cs->csetOnly) {
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
		unless (cs->csetOnly) {
			fputs("\n", stdout);
			fputs(PATCH_PATCH, stdout);
		}
		if (cs->metaOnly || LOGS_ONLY(cset)) {
			assert(!cs->compat);
			fputs(PATCH_CURRENT, stdout);
			fputs(PATCH_LOGGING, stdout);
		} else if (cs->compat) {
			fputs(PATCH_COMPAT, stdout);
		} else {
			fputs(PATCH_CURRENT, stdout);
			fputs(PATCH_REGULAR, stdout);
		}
		fputs("\n", stdout);
	}

	sccs_close(cset); /* for win32 */
	/*
	 * Do the ChangeSet deltas first, takepatch needs it to be so.
	 */
	for (d = cset->table; d; d = d->next) {
		if (d->flags & D_SET) {
			sccs_sdelta(cset, d, buf);
			if (doKey(cs, csetid, buf, goneDB)) goto fail;
		}
	}

	/*
	 * Now do the real data.
	 */
	while (fnext(buf, list)) {
		chop(buf);
		t = separator(buf); *t++ = 0;
		if (sameFile(cs, csetid, buf)) continue;
		if (doKey(cs, buf, t, goneDB)) {
			fprintf(stderr,
			    "File named by key\n\t%s\n\tis missing and key is "
			    "not in a committed gone delta, aborting.\n", buf);
			fflush(stderr); /* for win32 */
			goto fail;
		}
	}
	if (cs->doDiffs && cs->makepatch) {
		doKey(cs, 0, 0, goneDB);
		cs->doDiffs = 0;
		fputs(PATCH_END, stdout);
		rewind(list);
		goto again;
	}
	fclose(list);
	list = 0;
	doKey(cs, 0, 0, goneDB);
	if (cs->verbose && cs->makepatch) {
		fprintf(stderr,
		    "makepatch: patch contains %d revisions from %d files\n",
		    cs->ndeltas, cs->nfiles);
	} else if (cs->verbose && cs->mark && cs->ndeltas) {
		fprintf(stderr,
		    "cset: marked %d revisions in %d files\n",
		    cs->ndeltas, cs->nfiles);
	}
	if (cs->makepatch && !cs->csetOnly) {
		fputs(PATCH_END, stdout);
		fflush(stdout);
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
	unlink(csort);
	free(csetid);
	if (goneDB) mdbm_close(goneDB);
	return;

fail:
	if (cs->makepatch && !cs->csetOnly) {
		printf(PATCH_ABORT);
		fclose(stdout);
		waitpid(cs->pid, &status, 0);	/* for win32: child inherited */
						/* a low level csort handle */
	}
	if (list) fclose(list);
	unlink(csort);
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

	if (sc->state & S_CSET) return;	/* no changeset diffs */
	for (d = sc->table; d; d = d->next) {
		if (d->flags & D_SET) {
			e = d;
		} else if (e) {
			break;
		}
	}
	for (d = sc->table; d && !(d->flags & D_SET); d = d->next);
	if (!d) return;
	unless (e->parent) {
		printf("--- New file ---\n+++ %s\t%s\n",
		    sc->gfile, sccs_ino(sc)->sdate);
		sccs_get(sc, 0, 0, 0, 0, PRINT|SILENT, "-");
		printf("\n");
		return;
	}
	e = e->parent;
	if (e == d) return;
	sccs_diffs(sc, e->rev, d->rev, 0, kind, 0, stdout);
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

	if (sc->state & S_CSET) return;
	for (d = sc->table; d; d = d->next) {
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
	if (cs->remark) {
		for (d = s->table; d; d = d->next) d->flags &= ~D_CSET;
	}

	for (d = s->table; d; d = d->next) {
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
		sccs_admin(s, 0, NEWCKSUM, 0, 0, 0, 0, 0, 0, 0, 0);
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

	if (copts.hide_cset  && streq(CHANGESET, sc->sfile))  return;
	for (d = sc->table; d; d = d->next) {
		if (d->flags & D_SET) {
		    	if (copts.historic) {
				printf("%s%c%s%c%s\n", sc->gfile, BK_FS,
						d->pathname, BK_FS, d->rev);
			} else {
				printf("%s%c%s\n", sc->gfile, BK_FS, d->rev);
			}
		}
	}
}

/*
 * Print out everything leading from start to d, not including start.
 */
private void
csetDeltas(cset_t *cs, sccs *sc, delta *start, delta *d)
{
	int	i;

	unless (d) return;
	debug((stderr, "cD(%s, %s)\n", sc->gfile, d->rev));
	if ((d == start) || (d->flags & D_SET)) return;
	d->flags |= D_SET;
	csetDeltas(cs, sc, start, d->parent);
	/*
	 * We don't need the merge pointer, it is part of the include list.
	 * if (d->merge) csetDeltas(sc, start, sfind(sc, d->merge));
	 */
	EACH(d->include) {
		delta	*e = sfind(sc, d->include[i]);

		csetDeltas(cs, sc, e->parent, e);
	}
	// XXX - fixme - removed deltas not done.
	// Is this an issue?  I think makepatch handles them.
	unless (cs->makepatch) {
		if (d->type == 'D') printf("%s%c%s\n", sc->gfile, BK_FS, d->rev);
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

	unless ((chop(buf) == '\n') && (rev = strrchr(buf, BK_FS))) {
		fprintf(stderr, "cset: bad file:rev format: %s\n", buf);
		system("bk clean -u ChangeSet");
		cset_exit(1);
	}
	*rev++ = 0;

	/*
	 * XXX Optimazation note: We should probaly check for ChangeSet
	 * file first before we waste cpu to call sccs_init()
	 * This should be a win if we have complex graph and large 
	 * ChangeSet file. Since we ae going to sccs_free() and
	 * return anyway..
	 */
	unless (s = sccs_init(buf, SILENT, 0)) {
		fprintf(stderr, "cset: can't init %s\n", buf);
		system("bk clean -u ChangeSet");
		cset_exit(1);
	}
	if (s->state & S_CSET) {
		sccs_free(s);
		return;
	}
	unless (d = sccs_getrev(s, rev, 0, 0)) {
		fprintf(stderr, "cset: can't find %s in %s\n", rev, buf);
		system("bk clean -u ChangeSet");
		cset_exit(1);
	}

	p = basenm(buf);
	*p = 'd';
	unlink(buf); /* remove d.file */

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
mkChangeSet(sccs *cset, FILE *diffs)
{
	delta	*d;
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
	 * Read each file:rev from stdin and add that to the cset.
	 * add() will ignore the ChangeSet entry itself.
	 */
	while (fgets(buf, sizeof(buf), stdin)) {
		add(diffs, buf);
	}

#ifdef CRAZY_WOW
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
private	int
csetCreate(sccs *cset, int flags, char **syms)
{
	delta	*d;
	int	error = 0;
	MMAP	*diffs;
	FILE	*fdiffs;
	char	filename[MAXPATH];

	gettemp(filename, "cdif");
	unless (fdiffs = fopen(filename, "w+")) {
		perror(filename);
		sccs_free(cset);
		cset_exit(1);
	}

	d = mkChangeSet(cset, fdiffs); /* write change set to diffs */

	fclose(fdiffs);
	unless (diffs = mopen(filename, "b")) {
		perror(filename);
		sccs_free(cset);
		unlink(filename);
		cset_exit(1);
	}

	d->flags |= D_CSET;	/* XXX: longrun, don't tag cset file */

	/*
	 * Make /dev/tty where we get input.
	 */
#undef	close
#undef	open
	close(0);
	open(DEV_TTY, 0, 0);
	if (flags & DELTA_DONTASK) d = comments_get(d);
	if (sccs_delta(cset, flags, d, 0, diffs, syms) == -1) {
		sccs_whynot("cset", cset);
		error = -1;
		goto out;
	}

	if (marklist(filename)) {
		error = -1;
		goto out;
	}

out:	sccs_free(cset);
	unlink(filename);
	comments_done();
	return (error);
}

private	char	*
file2str(char *f)
{
	struct	stat sb;
	int 	n;
	int	fd = open(f, O_RDONLY, 0);
	char	*s;

	setmode(fd, O_TEXT);
	if ((fd == -1) || (fstat(fd, &sb) == -1) || (sb.st_size == 0)) {
		fprintf(stderr, "Can't get comments from %s\n", f);
		if (fd != -1) close(fd);
		return (0);
	}
	s = malloc(sb.st_size + 1);
	if (!s) {
		perror("malloc");
		close(fd);
		return (0);
	}
	/*
	 * Note: On win32, n may be smaller than sb.st_size
	 * because text mode remove \r when reading
	 */
	n = read(fd, s, sb.st_size);
	assert((n >= 0) && (n <= sb.st_size));
	s[n] = 0;
	close(fd);
	return (s);
}

/*
 * All the deltas we want are marked so print them out.
 * Note: takepatch depends on table order so don't change that.
 */
private	void
sccs_patch(sccs *s, cset_t *cs)
{
	delta	*d;
	int	deltas = 0, prs_flags = (PRS_PATCH|SILENT);
	int	i, n, newfile, empty, encoding = 0;
	delta	**list;

        if (sccs_admin(s, 0, SILENT|ADMIN_BK, 0, 0, 0, 0, 0, 0, 0, 0)) {
		fprintf(stderr, "Patch aborted, %s has errors\n", s->sfile);
		fprintf(stderr,
		    "Run ``bk -r check -a'' for more information.\n");
		cset_exit(1);
	}
	if (s->state & S_CSET) {
		encoding = s->encoding;
		if (encoding & E_GZIP) sccs_unzip(s);
	}
	if (cs->compat) prs_flags |= PRS_COMPAT;

	if (cs->verbose>1) fprintf(stderr, "makepatch: %s ", s->gfile);

	/*
	 * Build a list of the deltas we're sending
	 * Clear the D_SET flag because we need to be able to do one at
	 * a time when sending the cset diffs.
	 */
	for (n = 0, d = s->table; d; d = d->next) {
		if (d->flags & D_SET) n++;
	}
	list = calloc(n, sizeof(delta*));
	newfile = s->tree->flags & D_SET;
	for (i = 0, d = s->table; d; d = d->next) {
		if (d->flags & D_SET) {
			assert(i < n);
			list[i++] = d;
			d->flags &= ~D_SET;
		}
	}

	/*
	 * For each file, spit out file seperators when the filename
	 * changes.
	 * Spit out the root rev so we can find if it has moved.
	 */
	for (i = n - 1; i >= 0; i--) {
		d = list[i];
		assert(d);
		if (cs->verbose > 2) fprintf(stderr, "%s ", d->rev);
		if (cs->verbose == 2) fprintf(stderr, "%c\b", spin[deltas % 4]);
		if (i == n - 1) {
			unless (s->gfile) {
				fprintf(stderr, "\n%s%c%s has no path\n",
				    s->gfile, BK_FS, d->rev);
				cset_exit(1);
			}
			printf("== %s ==\n", s->gfile);
			if (newfile) {
				printf("New file: %s\n", d->pathname);
				if (cs->compat) {
					s->state |= S_READ_ONLY;
					s->version = SCCS_VERSION_COMPAT;
				}
				sccs_perfile(s, stdout);
			}
			s->rstop = s->rstart = s->tree;
			sccs_pdelta(s, sccs_ino(s), stdout);
			printf("\n");
		}

		/*
		 * For each file, also eject the parent of the rev.
		 */
		if (d->parent) {
			sccs_pdelta(s, d->parent, stdout);
			printf("\n");
		}
		s->rstop = s->rstart = d;
		/*
		 * XXX FIXME
		 * TODO  move the test out side this loop. This would
		 * be a little faster.
		 */
		empty = 0;
		if (cs->metaOnly) {
			int len1 = strlen(s->tree->pathname);
			int len2 = strlen("BitKeeper/");
			unless ((s->state & S_CSET) ||
			    ((len1 > len2) &&
			    strneq(s->tree->pathname, "BitKeeper/", len2))) {
				empty = 1;
			}
			prs_flags |= PRS_LOGGING;
		}
		if (sccs_prs(s, prs_flags, 0, NULL, stdout)) cset_exit(1);
		printf("\n");
		if (d->type == 'D') {
			int	rc = 0;

			if (s->state & S_CSET) {
				if (d->added) rc = cset_diffs(s, d->serial);
			} else unless (empty) {
				rc = sccs_getdiffs(s, d->rev, GET_BKDIFFS, "-");
			}
			if (rc) { /* sccs_getdiffs errored */
				fprintf(stderr,
				    "Patch aborted, sccs_getdiffs %s failed\n",
				    s->sfile);
				cset_exit(1);
			}
		}
		printf("\n");
		deltas++;
	}
	if (cs->verbose == 2) {
		fprintf(stderr, "%d revisions\r", deltas);
		fprintf(stderr, "%79s\r", "");
	} else if (cs->verbose > 1) {
		fprintf(stderr, "\n");
	}
	cs->ndeltas += deltas;
	if (list) free(list);
	if ((s->state & S_CSET) && (encoding & E_GZIP)) sccs_gzip(s);
}


