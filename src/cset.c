/*
 * Copyright 1999-2016 BitMover, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "system.h"
#include "sccs.h"
#include "range.h"
#include "graph.h"
#include "nested.h"
#include "progress.h"
#include "poly.h"

typedef	struct cset {
	/* bits */
	u32	makepatch:1;	/* if set, act like makepatch */
	u32	listeach:1;	/* if set, list revs 1/line */
	u32	mark:1;		/* act like csetmark used to act */
	u32	doDiffs:1;	/* prefix with unified diffs */
	u32	force:1;	/* if set, then force past errors */
	u32	remark:1;	/* clear & redo all the ChangeSet marks */
	u32	dash:1;
	u32	historic:1;	/* list the historic name */
	u32	hide_comp:1;	/* exclude comp from file@rev list */
	u32	include:1;	/* create new cset with includes */
	u32	exclude:1;	/* create new cset with excludes */
	u32	serial:1;	/* the revs passed in are serial numbers */
	u32	md5out:1;	/* the revs printed are as md5keys */
	u32	doBAM:1;	/* send BAM data */
	u32	compat:1;	/* do not send BKMERGE patches */
	u32	fail:1;		/* let all failures be flushed out */
	u32	standalone:1;	/* this repo and not product */

	/* numbers */
	int	bkmerge;	/* -1: default; 1 bkmerge; 0 sccsmerge */
	int	verbose;
	int	progress;	/* progress bar max */
	int	notty;
	int	fromStart;	/* if we did -R1.1..1.2 */
	int	ndeltas;
	int	ncsets;
	int	nfiles;
	pid_t	pid;		/* adler32 process id */

	char	*csetkey;	/* lie about the cset rootkey */
	char	**BAM;		/* list of keys we need to send */
} cset_t;

private	void	csetlist(cset_t *cs, sccs *cset);
private	void	doSet(sccs *sc);
private	int	doMarks(cset_t *cs, sccs *sc);
private	void	doDiff(sccs *sc);
private	void	sccs_patch(sccs *, cset_t *);
private	void	cset_exit(int n);
private	int	bykeys(const void *pa, const void *pb);
private	char	csetFile[] = CHANGESET; /* for win32, need writable buffer */
private	cset_t	copts;

int
makepatch_main(int ac, char **av)
{
	int	dash, c, i;
	char	*nav[15];
	char	*range = 0;
	longopt	lopts[] = {
		{ "bk-merge", 300 },		/* output bk-merge format */
		{ "no-bk-merge", 310 },		/* output sccs-merge format */
		{ 0, 0 }
	};

	dash = streq(av[ac-1], "-");
	nav[i=0] = "makepatch";
	copts.bkmerge = -1;	/* un-determined */
	while ((c = getopt(ac, av, "BCdFM;P:qr|sv", lopts)) != -1) {
		if (i == 14) usage();
		switch (c) {
		    case 'B': copts.doBAM = 1; break;
		    case 'C': copts.compat = 1; break;
		    case 'd':					/* doc 2.0 */
			nav[++i] = "-d";
			break;
		    case 'F': break; // ignored, old fastpatch
		    case 'M': break; // ignored, old tooMany
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
		    case 300:		/* --bk-merge */
			copts.bkmerge = 1;
			break;
		    case 310:		/* --no-bk-merge */
			copts.bkmerge = 0;
			break;
		    default: bk_badArg(c, av);
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
	int	show_comp = 0;
	char	*cFile = 0;
	int	rc = 1;
	RANGE	rargs = {0};
	longopt	lopts[] = {
		{ "show-comp", 300 },		/* undo hide_comp */
		{ "standalone", 'S' },		/* this repo only */
		{ 0, 0 }
	};

	if (streq(av[0], "makepatch")) copts.makepatch = 1;
	copts.notty = (getenv("BK_NOTTY") != 0);

	while (
	    (c = getopt(ac, av, "5BCd|DFfhi;lm|M;N;|qr|Ssvx;", lopts)) != -1){
		switch (c) {
		    case 'B': copts.doBAM = 1; break;
		    case 'D': ignoreDeleted++; break;		/* undoc 2.0 */
		    case 'f': copts.force++; break;		/* undoc? 2.0 */
		    case 'F': break; // ignored, old fastpatch
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
		    case 'N':
			copts.progress = atoi(optarg);
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
		    case 'S': copts.standalone = 1; break;	/* doc 7.0.1 */
		    case 'q':					/* doc 2.0 */
		    case 's': flags |= SILENT; break;		/* undoc? 2.0 */
		    case '5': copts.md5out = 1; break;		/* undoc 4.0 */
		    case 'v': copts.verbose++; break;		/* undoc? 2.0 */
		    case 'x':					/* doc 2.0 */
			if (copts.include || copts.exclude) usage();
			copts.exclude++;
			if (range_addArg(&rargs, optarg, 0)) usage();
			break;
		    case 300:		/* --show-comp */
		        show_comp = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}

	if (!show_comp && proj_isProduct(0) && !copts.remark) copts.hide_comp++;

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
		if (chdir(av[optind])) {
			perror(av[optind]);
			return (1);
		}
	} else if (proj_cd2root()) {
		fprintf(stderr, "cset: cannot find package root.\n");
		return (1);
	}

	/*
	 * If doing include/exclude, go do it.
	 */
	if (copts.include || copts.exclude) {
		bk_nested2root(copts.standalone);
		cmdlog_lock(CMD_NESTED_WRLOCK|CMD_WRLOCK);
	}
	if (copts.include) return (cset_inex(flags, "-i", rargs.rstart));
	if (copts.exclude) return (cset_inex(flags, "-x", rargs.rstart));
	if (copts.standalone) {
		fprintf(stderr, "cset: -S requires -i or -x\n");
		return (1);
	}
	unless (copts.makepatch) cmdlog_lock(CMD_WRLOCK);

	cset = sccs_init(csetFile, 0);
	if (!cset) return (101);
	if (copts.csetkey) cset->state |= S_READ_ONLY;

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
		    /* include tags in a patch */
		    if (copts.makepatch) range_markMeta(cset);
		}
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
	assert(n);		/* must be failure */
	if (copts.pid) {
		printf(PATCH_ABORT);
		fclose(stdout);
		waitpid(copts.pid, 0, 0);
	}
	exit(n);
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
 * Do whatever it is they want.
 */
private int
doit(cset_t *cs, sccs *sc)
{
	unless (sc) {
		if (cs->doDiffs && cs->makepatch) printf("\n");
		return (0);
	}
	if (copts.hide_comp && CSET(sc) && proj_isComponent(sc->proj)) {
		return (0);
	}
	cs->nfiles++;
	if (cs->doDiffs) {
		doDiff(sc);
	} else if (cs->makepatch) {
		sccs_patch(sc, cs);
	} else if (cs->mark) {
		return (doMarks(cs, sc));
	} else {
		doSet(sc);
	}
	return (0);
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
	strcpy(pwd, proj_cwd());
	printf("# Root:\t%s\n", pwd);
	cset->state = save;
}

/*
 * Depending on what we are doing, either
 * mark all deltas in this cset, or
 * just mark the cset boundry.
 */
private void
markThisCset(cset_t *cs, ser_t d_cset, sccs *s, ser_t d)
{
	if (cs->mark || TAG(s, d)) {
		FLAGS(s, d) |= D_SET;
		return;
	}
	/*
	 * Poly - if two product csets include the same component
	 * cset, and we are going to exclude one of the product
	 * csets, then exclude the component.
	 */
	if (!cs->hide_comp && CSET(s) && (s != cset)) {
		char	key[MAXKEY];

		sccs_sdelta(cset, d_cset, key);
		poly_range(s, d, key);
	} else {
		range_cset(s, d);
	}
}

private int
doKey(cset_t *cs, weave *item, MDBM *goneDB)
{
	static	MDBM *idDB;
	static	sccs *sc;
	static	u32  lastrkoff;
	char	*key, *val;
	ser_t	d;
	int	rc = 0;

	/*
	 * Cleanup code, called to reset state.
	 */
	unless (item) {
		if (idDB) {
			mdbm_close(idDB);
			idDB = 0;
		}
		lastrkoff = 0;
		if (sc) {
			rc |= doit(cs, sc);
			unless (sc == cset) sccs_free(sc);
			sc = 0;
		}
		rc |= doit(cs, 0);
		return (rc);
	}
	key = HEAP(cset, item->rkoff);

	/*
	 * If we have a match, just mark the delta and return,
	 * we'll finish later.
	 *
	 */
	if (lastrkoff && (lastrkoff == item->rkoff)) goto markkey;

	/*
	 * This would be later - do the last file and clean up.
	 */
	if (sc) {
		rc |= doit(cs, sc);
		unless (sc == cset) sccs_free(sc);
		sc = 0;
		lastrkoff = 0;
	}

	/*
	 * Set up the new file.
	 */
	lastrkoff = item->rkoff;
	unless (idDB || (idDB = loadDB(IDCACHE, 0, DB_IDCACHE))) {
		perror("idcache");
	}
	if (cset && streq(key, proj_rootkey(0))) {
		sc = cset;
	} else {
		sc = sccs_keyinit(0, key, INIT_NOWARN, idDB);
	}
	if (sc) {
		unless (sc->cksumok) {
			sccs_free(sc);
			return (-1);
		}
	} else {
		lastrkoff = 0;
		if (!cs->hide_comp &&
		    changesetKey(key) && proj_isProduct(0)) {
			nested	*n = nested_init(cset, 0, 0, 0);
			comp	*c = nested_findKey(n, key);

			fprintf(stderr,
			    "Component '%s' not populated.  "
			    "Populate and try again.\n",
			    c->path ? c->path : key);
			nested_free(n);
			cs->fail = 1;
			return (0);
		}
		if (gone(key, goneDB)) return (0);
		fprintf(stderr, "cset: unable to keyinit %s\n", key);
		return (cs->force ? 0 : -1);
	}
	if (cs->fail) {	/* in failure mode, just look for other failures */
		unless (sc == cset) sccs_free(sc);
		sc = 0;
		lastrkoff = 0;
		return (0);
	}
markkey:
	val = HEAP(cset, item->dkoff);
	unless (d = sccs_findKey(sc, val)) {
		/* OK to have missing keys if the gone file told us so */
		if (gone(key, goneDB) || gone(val, goneDB)) return (0);

		fprintf(stderr,
		    "cset: cannot find\n\t%s in\n\t%s\n", val, sc->sfile);
		return (cs->force ? 0 : -1);
	}
	unless (cs->hide_comp && CSET(sc) && proj_isComponent(sc->proj)) {
		markThisCset(cs, item->ser, sc, d);
	}
	return (rc);
}

/*
 * sort rootkeys by name for determining file order in the patch
 * Uses global 'cset' to get at heap state.
 * Note: don't care about order in matching case.
 */
private	int
bykeys(const void *a, const void *b)
{
	weave	*wa = (weave*)a;
	weave	*wb = (weave*)b;

	return (keycmp(HEAP(cset, wa->rkoff), HEAP(cset, wb->rkoff)));
}

/*
 * List all the revisions which make up a range of changesets.
 * The list is sorted for performance.
 */
private void
csetlist(cset_t *cs, sccs *cset)
{
	char	*rk;
	char	buf[MAXPATH*2];
	char	*csetid;
	int	status, i, n;
	ser_t	d;
	MDBM	*goneDB = 0;
	ticker	*tick = 0;
	weave	*cweave = 0;

	if (cs->dash) {
		while(fgets(buf, sizeof(buf), stdin)) {
			chop(buf);
			if (copts.serial) {
				d = atoi(buf);
				assert((d > 0) && (d <= TABLE(cset)));
				unless (FLAGS(cset, d)) d = 0;
			} else {
				d = sccs_findrev(cset, buf);
			}
			unless (d) {
				fprintf(stderr,
				    "cset: no rev like %s in %s\n",
				    buf, cset->gfile);
				cset_exit(1);
			}
			FLAGS(cset, d) |= D_SET;
		}
	}

	/* Save away the cset id */
	sccs_sdelta(cset, sccs_ino(cset), buf);
	csetid = strdup(buf);
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
	cweave = cset_mkList(cset);
	sortArray(cweave, bykeys);
	if (cs->verbose > 5) {;
		EACH(cweave) printf("%u\t%s %s\n",
		    cweave[i].ser,
		    HEAP(cset, cweave[i].rkoff),
		    HEAP(cset, cweave[i].dkoff));
	}
again:	/* doDiffs can make it two pass */
	if (!cs->doDiffs && cs->makepatch) {
		int	didfeature = 0;

		fputc('\n', stdout);
		fputs(PATCH_PATCH, stdout);
		fputs(PATCH_FAST, stdout);
		if (copts.csetkey) {
			fputs(((didfeature++) ? "," : PATCH_FEATURES), stdout);
			fputs("PORT", stdout);
		}
		if (copts.bkmerge == -1) copts.bkmerge = !copts.compat;
		if (copts.bkmerge) {
			fputs(((didfeature++) ? "," : PATCH_FEATURES), stdout);
			fputs("BKMERGE", stdout);
		}
		if (didfeature) {
			fputc('\n', stdout);
		} else {
			fputs(PATCH_REGULAR, stdout);
		}
		fputc('\n', stdout);
	}

	/*
	 * Do the ChangeSet deltas first, takepatch needs it to be so.
	 */
	doit(cs, cset);

	/*
	 * Now do the real data.
	 */
	if (cs->progress) {
		tick = progress_startScaled(PROGRESS_BAR,
					    nLines(cweave),
					    cs->progress);
	}
	n = 0;
	EACH (cweave) {
		if (tick) progress(tick, ++n);
		rk = HEAP(cset, cweave[i].rkoff);
		if (streq(csetid, rk)) continue; /* skip ChangeSet */
		if (doKey(cs, &cweave[i], goneDB)) {
			fprintf(stderr,
			    "File named by key\n\t%s\n\tis missing and key is "
			    "not in gone file, aborting.\n", rk);
			fflush(stderr); /* for win32 */
			goto fail;
		}
	}
	if (cs->fail) goto fail;
	if (cs->doDiffs && cs->makepatch) {
		if (doKey(cs, 0, goneDB)) goto fail;
		cs->doDiffs = 0;
		fputs(PATCH_END, stdout);
		goto again;
	}
	if (doKey(cs, 0, goneDB)) goto fail;
	FREE(cweave);
	if (cs->verbose && cs->makepatch) {
		fprintf(stderr,
		    "makepatch: patch contains %d changesets / %d files\n",
		    cs->ncsets, cs->nfiles);
	} else if (cs->verbose && cs->mark && cs->ndeltas) {
		fprintf(stderr,
		    "cset: marked %d revisions in %d files\n",
		    cs->ndeltas, cs->nfiles);
	}
	if (cs->makepatch) {
		if (cs->BAM) fputs("== @SFIO@ ==\n\n", stdout);
		fputs(PATCH_END, stdout);
		fflush(stdout);
		if (cs->BAM) {
			int	i;
			FILE	*f;

			/* try and fetch from my local BAM pool but recurse
			 * through to the server.
			 */
			f = popen("bk sfio -oqB -", "w");
			EACH(cs->BAM) fprintf(f, "%s\n", cs->BAM[i]);
			freeLines(cs->BAM, free);
			cs->BAM = 0;
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
	if (tick) progress_done(tick, "OK");
	return;

fail:
	free(cweave);
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
doDiff(sccs *sc)
{
	ser_t	d, e = 0;
	df_opt	dop = {0};

	dop.out_unified = 1;
	if (CSET(sc)) return;		/* no changeset diffs */
	for (d = TABLE(sc); d >= TREE(sc); d--) {
		if (FLAGS(sc, d) & D_SET) {
			e = d;
		} else if (e) {
			break;
		}
	}
	for (d = TABLE(sc); (d >= TREE(sc)) && !(FLAGS(sc, d) & D_SET); d--);
	if (!d) return;
	unless (PARENT(sc, e)) {
		printf("--- New file ---\n+++ %s\t%s\n",
		    sc->gfile, delta_sdate(sc, sccs_ino(sc)));
		sccs_get(sc, 0, 0, 0, 0, SILENT, 0, stdout);
		printf("\n");
		return;
	}
	e = PARENT(sc, e);
	if (e == d) return;
	sccs_diffs(sc, REV(sc, e), REV(sc, d), &dop, stdout);
}

#if 0
/*
 * Print a range suitable for diffs.
 * XXX - does not make sure that they are both on the trunk.
 */
private void
doEndpoints(cset_t *cs, sccs *sc)
{
	ser_t	d, earlier = 0, later = 0;

	if (CSET(sc)) return;		
	for (d = TABLE(sc); d >= TREE(sc); d--) {
		unless (FLAGS(s, d) & D_SET) continue;
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

private int
doMarks(cset_t *cs, sccs *s)
{
	ser_t	d;
	int	did = 0;
	char	*t;
	int	attip = 0;

	/*
	 * Throw away the existing marks if we are rebuilding.
	 */
	if (cs->remark) sccs_clearbits(s, D_CSET);

	for (d = TABLE(s); d >= TREE(s); d--) {
		if (!TAG(s, d) && (FLAGS(s, d) & D_SET)) {
			if (cs->force || !(FLAGS(s, d) & D_CSET)) {
				if (cs->verbose > 2) {
					fprintf(stderr, "Mark %s%c%s\n",
					    s->gfile, BK_FS, REV(s, d));
				}
				FLAGS(s, d) |= D_CSET;
				cs->ndeltas++;
				did++;
				if (!attip && (d == sccs_top(s))) attip = 1;
			}
		}
	}
	if (did) {
		if (sccs_newchksum(s)) {
			fprintf(stderr, "Could not mark %s. Perhaps it "
			    "is locked by some other process?\n",
			    s->gfile);
			t = sccs_Xfile(s, 'x');
			if (unlink(t)) {
				fprintf(stderr,
				    "Could not clean up %s\n", t);
			}
			return (1);
		} else if (attip) {
			// remove dfile
			xfile_delete(s->gfile, 'd');
		}
		if ((cs->verbose > 1) && did) {
			fprintf(stderr,
			    "Marked %d csets in %s\n", did, s->gfile);
		} else if (cs->verbose > 1) {
			fprintf(stderr,
			    "Set CSETMARKED flag in %s\n", s->sfile);
		}
	}
	return (0);
}

/*
 * Do the set listing
 */
private void
doSet(sccs *sc)
{
	ser_t	d;
	char	key[MD5LEN];

	for (d = TABLE(sc); d >= TREE(sc); d--) {
		if (FLAGS(sc, d) & D_SET) {
			printf("%s", sc->gfile);
		    	if (copts.historic) {
				printf("%c%s", BK_FS, PATHNAME(sc, d));
			}
			if (copts.md5out) {
				sccs_md5delta(sc, d, key);
				printf("%c%s\n", BK_FS, key);
			} else {
				printf("%c%s\n", BK_FS, REV(sc, d));
			}
		}
	}
}

/*
 * All the deltas we want are marked so print them out.
 * Note: takepatch depends on table order so don't change that.
 */
private	void
sccs_patch(sccs *s, cset_t *cs)
{
	ser_t	d;
	ser_t	*patchmap = 0;	/* patch map */
	int	rc = 0;
	int	deltas = 0, csets = 0, last = 0;
	int	outdiffs = 0;
	int	prs_flags = (PRS_PATCH|PRS_FASTPATCH|SILENT);
	int	i, n, newfile;
	ser_t	*list;
	char	*gfile = 0;
	ticker	*tick = 0;

        if (sccs_adminFlag(s, SILENT|ADMIN_BK)) {
		fprintf(stderr, "Patch aborted, %s has errors\n", s->sfile);
		fprintf(stderr,
		    "Run ``bk -r check -a'' for more information.\n");
		cset_exit(1);
	}

	if (cs->verbose>1) fprintf(stderr, "makepatch: %s", s->gfile);

	/* see that we are sending patches in all the same format */
	if ((cs->bkmerge != BKMERGE(s)) && graph_convert(s, 0)) {
		cset_exit(1);
	}

	/*
	 * Build a list of the deltas we're sending
	 * Clear the D_SET flag because we need to be able to do one at
	 * a time when sending the cset diffs.
	 */
	newfile = FLAGS(s, TREE(s)) & D_SET;
	list = 0;
	for (n = 0, d = TABLE(s); d >= TREE(s); d--) {
		unless (FLAGS(s, d) & D_SET) continue;
		unless (gfile) gfile = CSET(s) ? GCHANGESET : PATHNAME(s, d);
		n++;
		unless (last) last = n;
		addArray(&list, &d);
		if (HAS_BAMHASH(s, d) && BAM(s) && copts.doBAM) {
			cs->BAM = addLine(cs->BAM,
			    sccs_prsbuf(s, d, PRS_FORCE, BAM_DSPEC));
		}
		FLAGS(s, d) &= ~D_SET;
	}
	unless (n) return;
	assert(gfile);

	/*
	 * For each file, spit out file seperators when the filename
	 * changes.
	 * Spit out the root rev so we can find if it has moved.
	 */
	if ((cs->verbose == 2) && !cs->notty) {
		tick = progress_start(PROGRESS_SPIN, 0);
	}
	patchmap = calloc(TABLE(s) + 1, sizeof(ser_t));
	for (i = n; i > 0; i--) {
		d = list[i];
		if (patchmap) patchmap[d] = n - i + 1;
		assert(d);
		if (cs->verbose > 2) fprintf(stderr, " %s", REV(s, d));
		if (tick) progress(tick, 0);
		if (i == n) {
			unless (s->gfile) {
				fprintf(stderr, "\n%s%c%s has no path\n",
				    s->gfile, BK_FS, REV(s, d));
				cset_exit(1);
			}
			printf("== %s ==\n", gfile);
			if (newfile) {
				printf("New file: %s\n", PATHNAME(s, d));
				sccs_perfile(s, stdout, 1);
			}
			s->rstop = s->rstart = TREE(s);
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
		if (PARENT(s, d)) {
			sccs_pdelta(s, PARENT(s, d), stdout);
			printf("\n");
		}
		if (copts.csetkey && CSET(s)) FLAGS(s, d) &= ~D_CSET;
		s->rstop = s->rstart = d;
		if (sccs_prs(s, prs_flags, 0, NULL, stdout)) cset_exit(1);
		printf("\n");
		/* takepatch lists tags+commits so we do too */
		if (CSET(s)) csets++;
		/*
		 * put a multi-delta patch on the newest delta
		 * pedantically, put out a patch for I1-E1
		 * can make for better diff -r results.
		 */
		outdiffs += ADDED(s, d) + DELETED(s, d) + (d == 1);
		if ((i == last) && outdiffs) {
			rc = sccs_patchDiffs(s, patchmap, "-");
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

weave *
cset_mkList(sccs *cset)
{
	ser_t	d;
	u32	rkoff, dkoff;
	weave	*item, *list = 0;

	sccs_rdweaveInit(cset);
	while (d = cset_rdweavePair(cset, RWP_DSET, &rkoff, &dkoff)) {
		unless (dkoff) continue; /* last key */
		item = addArray(&list, 0);
		item->ser = d;
		item->rkoff = rkoff;
		item->dkoff = dkoff;
	}
	sccs_rdweaveDone(cset);
	return (list);
}
