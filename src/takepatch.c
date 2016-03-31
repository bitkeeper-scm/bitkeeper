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


#include "sccs.h"
#include "bam.h"
#include "nested.h"
#include <time.h>
#include "range.h"
#include "progress.h"
#include "graph.h"

/*
 * takepatch - apply a BitKeeper patch file
 *
 * XXX - should take an option directory name or read the environment for
 * a directory.
 *
 * Patch format:
 *
 * == filename ==
 * key_of_root_delta
 * key_of_parent_delta
 *
 * D 1.159 99/02/06 16:57:48 lm 160/55/6481
 * ....
 *
 * diffs
 *
 * Repeats.
 */
#define	CLEAN_RESYNC	1	/* blow away the RESYNC dir */
#define	CLEAN_PENDING	2	/* blow away the PENDING dir */
#define	CLEAN_OK	4	/* but exit 0 anyway */
#define	CLEAN_FLAGS	8	/* Note that nway return are clean flags */
#define	SHOUT() fputs("\n=================================== "\
		    "ERROR ====================================\n", stderr);
#define	SHOUT2() fputs("======================================="\
		    "=======================================\n", stderr);
#define	NOTICE() fputs("------------------------------------"\
		    "---------------------------------------\n", stderr);

private	ser_t	getRecord(sccs *s, FILE *f);
private void	errorMsg(char *msg, char *arg1, char *arg2);
private	int	extractPatch(char *name, FILE *p);
private	int	extractDelta(char *name, sccs *sreal, sccs *scratch,
		    int newFile, FILE *f, hash *countFiles, int *np);
private	int	applyCsetPatch(sccs *s, int *nfound, int newFile);
private	void	insertPatch(patch *p, int strictOrder);
private	int	reversePatch(void);
private	void	initProject(void);
private	FILE	*init(char *file);
private	void	cleanup(int what);
private	void	freePatchList(void);
private	void	badpath(sccs *s, ser_t tot);
private	int	skipPatch(FILE *p);
private	void	getChangeSet(void);
private	int	sfio(FILE *m, int files);
private	int	parseFeatures(char *next);
private	int	sendPatch(char *name, FILE *in);
private	int	stopNway(int n, int *conflicts, int *numcsets);

typedef struct {
	/* command line options */
	int	pbars;		/* --progress: progress bars, default is off */
	int	mkpatch;	/* -m: act like makepatch verbose */
	int	collapsedups;	/* -D: allow csets in BitKeeper/etc/collapsed */
	int	parallel;	/* -j: do N takepatches in parallel */
	int	nway;		/* This is one of the parallel threads */
	int	automerge;	/* do automerging */

	/* global state */
	u64	N;		/* number of ticks */
	FILE	**outlist;	/* parallel popen file handles */
	int	*sent;		/* how many bytes written to each channel */

	/* modes enabled in patch */
	u8	fast;		/* Fast patch mode */
	u8	port;		/* patch created with 'bk port' */
	u8	bkmerge;	/* bk-style includes */

	/* The patch from stdin or file */
	FILE 	*p;

	/* old globals */
	int	echo;		/* verbose level, more means more diagnostics */
	int	line;		/* line number in the patch file */
	patch	*patchList;	/* patches for a file */
	int	conflicts;	/* number of conflicts over all files */
	int	newProject;	/* command line opt to create a new repo */
	int	saveDirs;	/* save directories even if errors */
	MDBM	*idDB;		/* key to pathname db, set by init or rebuilt */
	MDBM	*goneDB;	/* key to gone database */
	int	noConflicts;	/* if set, abort on conflicts */
	char	pendingFile[MAXPATH];
	char	*input;		/* input file name,
				 * either "-" or a patch file */
	char	*comments;	/* -y'comments', pass to resolve. */
	char	**errfiles;	/* files had errors during apply */
	char	**edited;	/* files that were in modified state */

	int	needlock;	/* we're top-level, grab a lock */
} Opts;
private Opts *opts;

/*
 * Table of feature names mapping to the option it enables.
 */
private struct {
	char	*name;
	int	offset;
} features[] = {
	{"PORT", offsetof(Opts, port)},
	{"BKMERGE", offsetof(Opts, bkmerge)},
	{0}
};

int
takepatch_main(int ac, char **av)
{
	FILE 	*f;
	char	*buf;
	int	c;
	int	rc;
	int	files = 0;
	char	*t;
	int	error = 0;
	int	remote = 0;	/* remote csets */
	int	resolve = 0;
	int	textOnly = 0;
	ticker	*tick = 0;
	longopt	lopts[] = {
		{ "progress", 300, },
		{ "Nway", 310, },
		{ "port", 315, },
		{ "fast", 320, },
		{ "bkmerge", 321, },
		{ "no-automerge", 325, },
		{ 0, 0 },
	};

	setmode(0, O_BINARY); /* for win32 */
	opts = new(Opts);
	opts->input = "-";
	opts->automerge = 1;
	while ((c = getopt(ac, av, "acDFf:ij;LmqsStTvy;", lopts)) != -1) {
		switch (c) {
		    case 'q':					/* undoc 2.0 */
		    case 's':					/* undoc 2.0 */
			opts->echo = opts->pbars = 0;
			break;
		    case 'a': resolve++; break;			/* doc 2.0 */
		    case 'c': opts->noConflicts++; break;	/* doc 2.0 */
		    case 'D': opts->collapsedups++; break;
		    case 'F': break;				/* obsolete */
		    case 'f':					/* doc 2.0 */
			    opts->input = optarg;
			    break;
		    case 'i': opts->newProject++; break;	/* doc 2.0 */
		    case 'j':
			if ((opts->parallel = atoi(optarg)) <= 1) {
				/* if they set it to <= 1 then disable */
				opts->parallel = -1;
			} else if (opts->parallel > PARALLEL_MAX) {
				opts->parallel = PARALLEL_MAX;	/* cap it */
			}
			break;
		    case 'm': opts->mkpatch++; break;		/* doc 2.0 */
		    case 'S': opts->saveDirs++; break;		/* doc 2.0 */
		    case 'T': textOnly++; break;		/* doc 2.0 */
		    case 'v':					/* doc 2.0 */
		        opts->pbars = 0;
			opts->echo++;
			break;
		    case 'y': opts->comments = optarg; break;
		    case 300: opts->pbars = 1; break;
		    case 310: opts->nway = 1; break;
		    case 315: opts->port = 1; break;
		    case 320: opts->fast = 1; break;
		    case 321: opts->bkmerge = 1; break;
		    case 325: opts->automerge = 0; break;
		    default: bk_badArg(c, av);
		}
	}
	if (opts->newProject) putenv("_BK_NEWPROJECT=YES");
	if (opts->nway) {
		unless (getenv("_BK_CALLSTACK")) {
			fprintf(stderr, "--Nway is internal option\n");
			free(opts);
			return (-1);
		}
		opts->p = stdin;
		goto doit;
	}
	unless (opts->parallel) opts->parallel = parallel(".", WRITER);
	/*
	 * I want to revisit this, this change breaks regressions but
	 * I think it was good for 20% on pulls.  Need to remeasure.
	unless (opts->parallel) opts->parallel = cpus();
	 */
	if (getenv("TAKEPATCH_SAVEDIRS")) opts->saveDirs++;
	if ((t = getenv("BK_NOTTY")) && *t && (opts->echo == 3)) {
		opts->echo = 2;
	}
	if (av[optind]) usage();

	/* we're takepatch on the cmdline, we need a lock */
	if (streq(getenv("_BK_CALLSTACK"), "takepatch")) opts->needlock = 1;

	opts->p = init(opts->input);
	if (sane(0, 0)) exit(1);	/* uses _BK_NEWPROJECT */

doit:	if (opts->newProject) {
		unless (opts->idDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
			perror("mdbm_open");
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
	} else {

		/* OK if this returns NULL */
		opts->goneDB = loadDB(GONE, 0, DB_GONE);

		unless (opts->idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
			perror("SCCS/x.id_cache");
			exit(1);
		}
	}

	/*
	 * Find a file and go do it.
	 */
	while (buf = fgetline(opts->p)) {
		++opts->line;
		unless (strncmp(buf, "== ", 3) == 0) {
			if (opts->echo > 7) {
				fprintf(stderr, "skipping: %s", buf);
			}
			continue;
		}

		/*
		 * If we see the SFIO header then consider the patch ended.
		 * We'll fall through to sfio to unpack.
		 */
		if (streq(buf, "== @SFIO@ ==")) {
			if (tick) {
				progress_done(tick,
				    error < 0 ? "FAILED" : "OK");
				tick = 0;
			}
			if (rc = sfio(opts->p, (int)(opts->N - files))) {
				error = -1;
			}
			break;
		}
		if (opts->echo>4) fprintf(stderr, "%s\n", buf);
		unless ((t = strrchr(buf, ' ')) && streq(t, " ==")) {
			SHOUT();
			fprintf(stderr, "Bad patch: %s\n", buf);
			cleanup(CLEAN_RESYNC);
		}
		*t = 0;
		t = &buf[3];	/* gfile */
		/* SFIO needs rootkey, so unpack ChangeSet in this thread */
		if ((opts->parallel > 0) &&
		    (!streq(t, GCHANGESET) || !opts->newProject)) {
			rc = sendPatch(t, opts->p);
		} else {
			t = name2sccs(t);
			rc = extractPatch(t, opts->p);
			free(t);
		}
		if (!files && opts->pbars) {
			tick = progress_start(PROGRESS_BAR, opts->N);
		}
		files++;
		if (tick) progress(tick, files);
		if (rc < 0) {
			error = rc;
			continue;
		}
		remote += rc;
	}
	if (opts->parallel > 0) {
		if (rc = stopNway(opts->parallel, &opts->conflicts, &remote)) {
			error = rc;
		}
	}
	if ((opts->p != stdin) && fclose(opts->p)) {
		perror(opts->input);
		opts->p = 0;	/* don't try to close again */
		cleanup(CLEAN_RESYNC);
	}
	opts->p = 0;
	if (opts->idDB) { mdbm_close(opts->idDB); opts->idDB = 0; }
	if (opts->goneDB) { mdbm_close(opts->goneDB); opts->goneDB = 0; }
	if (tick) progress_done(tick, error < 0 ? "FAILED" : "OK");
	if (error < 0) {
		/* XXX: Save?  Purge? */
		cleanup(CLEAN_RESYNC);
	}
#ifdef	SIGXFSZ
	if (getenv("_BK_SUICIDE")) kill(getpid(), SIGXFSZ);
#endif
	if (opts->nway) {
		if (remote) printf("r %d\n", remote);
		if (opts->conflicts) printf("c %u\n", opts->conflicts);
		goto done;
	}
	if (opts->echo || opts->pbars) {
		files = 0;
		if (f = popen("bk gfiles RESYNC", "r")) {
			while (t = fgetline(f)) ++files;
			pclose(f);
		}
		if (opts->pbars) {
			t = opts->conflicts ?
			    aprintf("%3d", opts->conflicts) : strdup(" no");
			Fprintf("BitKeeper/log/progress-sum",
			    "%3d changeset%s %s merge%s in %3d file%s\n",
			    remote, remote == 1 ? ", " : "s,",
			    t, opts->conflicts == 1 ? " " : "s",
			    files, files == 1 ? "" : "s");
		} else {
			fprintf(stderr,
			    "takepatch: "
			    "%d new changeset%s, %d conflicts in %d files\n",
			    remote, remote == 1 ? "" : "s",
			    opts->conflicts, files);
		}
	}

	unless (remote) {
		/*
		 * The patch didn't contain any new csets and so we don't
		 * need to run resolve.
		 * We should still run the post resolve trigger.
		 */
		if (resolve) {
			putenv("BK_STATUS=REDUNDANT");
			trigger("resolve", "post");
		}
		cleanup(CLEAN_RESYNC | CLEAN_PENDING | CLEAN_OK);
	}

	/*
	 * The ideas here are to (a) automerge any hash-like files which
	 * we maintain, and (b) converge on the oldest inode for a
	 * particular file.  The converge code will make sure all of the
	 * inodes are present.
	 */
	if (opts->conflicts) converge_hash_files();

	/*
	 * There are instances (contrived) where the ChangeSet
	 * file will not be in the RESYNC tree.  Make sure that
	 * it is.  This will prevent resolve from failing and
	 * hopefully those scary support emails.
	 */
	getChangeSet();

	if (resolve) {
		char	*resolve[] = {"bk", "-?BK_NO_REPO_LOCK=YES", "resolve",
				      "-S", 0, 0, 0, 0, 0, 0};
		int	i = 3;

		if (opts->echo) {
			fprintf(stderr,
			    "Running resolve to apply new work...\n");
		}
		unless (opts->echo) resolve[++i] = "-q";
		if (textOnly) resolve[++i] = "-T";
		if (opts->noConflicts) resolve[++i] = "-c";
		if (opts->comments) {
			resolve[++i] = aprintf("-y%s", opts->comments);
		}
		i = spawnvp(_P_WAIT, resolve[0], resolve);
		unless (WIFEXITED(i)) return (-1);
		error = WEXITSTATUS(i);
	}
done:	freeLines(opts->errfiles, free);
	freeLines(opts->edited, free);
	free(opts);
	return (error);
}

private void
getChangeSet(void)
{
	if (exists(CHANGESET)) {
		unless (exists(ROOT2RESYNC "/" CHANGESET)) {
			fileCopy(CHANGESET, ROOT2RESYNC "/" CHANGESET);
		}
    	}
}

private hash *
loadCollapsed(void)
{
	FILE	*f;
	hash	*db = 0;
	char	*p;
	char	buf[MAXLINE];

	if (opts->collapsedups) return (0);
	f = popen("bk annotate -ar " COLLAPSED, "r");
	while (fnext(buf, f)) {
		chomp(buf);
		unless (db) db = hash_new(HASH_MEMHASH);
		/* ignore dups, we don't care */
		p = strchr(buf, '\t');
		assert(p);
		*p++ = 0;
		hash_insertStr(db, p, buf);
	}
	pclose(f);
	return (db);
}

/*
 * XXX: when this function evolves to be called with a real sccs *
 * take out the hack in sccs_getInit to check sc->version != 0xff.
 */
private	ser_t
getRecord(sccs *s, FILE *f)
{
	int	e = 0;
	ser_t	d;

	d = sccs_getInit(s, 0, f, DELTA_PATCH|DELTA_TAKEPATCH, &e, 0, 0);
	if (!d || e) {
		fprintf(stderr,
		    "takepatch: bad delta record near line %d\n", opts->line);
		exit(1);
	}
	return (d);
}

private void
shout(void)
{
	static	int	shouted = 0;

	if (shouted) return; /* already done shouting */
	SHOUT();
	shouted = 1;
}

/*
 * Extract a contiguous set of deltas for a single file from the patch file.
 * "name" is the cset name, i.e., the most recent name in the csets being
 * sent; it might be different than the local name.
 */
private	int
extractPatch(char *name, FILE *p)
{
	ser_t	tmp;
	sccs	*s = 0, *new_s = 0, *scratch = 0;
	FILE	*perfile = 0;
	int	newFile = 0;
	int	cset;
	char	*gfile = 0;
	int	nfound = 0, rc;
	hash	*countFiles = 0; /* hash of rootkeys seen in cset patches */
	char	*t;
	int	newsfile;

	/*
	 * Patch format for continuing file:
	 * == filename ==
	 * lm||19970518232929
	 * lm@lm.bitmover.com|sccs/slib.c|19990130201757
	 * D 1.159 99/02/06 16:57:48-08:00 lm@lm.bitmover.com +160 -55
	 * etc.
	 *
	 * Patch format for new file:
	 * == filename ==
	 * New file: filename as of creation time
	 * perfile information (encoding, etc)
	 *
	 * lm||19970518232929
	 * D 1.1 99/02/23 00:29:01-08:00 lm@lm.bitmover.com +128 -0
	 * etc.
	 *
	 * Tue Mar 28 2000
	 * == filename ==
	 * Grafted file: filename as of creation time
	 * {same as above, no perfile.}
	 */
	t = fgetline(p);
	opts->line++;
	if (strneq("New file: ", t, 10)) {
		char	*resyncFile;

		newFile = 1;
		perfile = fmem();
		while (t = fgetline(p)) {
			fputs(t, perfile);
			fputc('\n', perfile);
			unless (*t) break;
		}
		rewind(perfile);
		resyncFile = aprintf(ROOT2RESYNC "/%s", name);
		unless (new_s = sccs_init(resyncFile, SILENT)) {
			SHOUT();
			fprintf(stderr,
			    "takepatch: can't create %s\n", resyncFile);
			free(resyncFile);
			goto error;
		}
		new_s->bitkeeper = 1;	/* XXX: not set in sccs_init ?? */
		free(resyncFile);
		if (sccs_getperfile(new_s, perfile, &opts->line)) goto error;
		rewind(perfile);
		t = fgetline(p);
		opts->line++;
	}
	if (strneq("Grafted file: ", t, 14)) {
		t = fgetline(p);
		opts->line++;
	}
	if (opts->newProject && !newFile) errorMsg("tp_notfirst", 0, 0);

	t = strdup(t);
	if (opts->echo>4) fprintf(stderr, "%s\n", t);
	s = sccs_keyinit(0, t, SILENT, opts->idDB);
	if (s && !s->cksumok) goto error;
	/*
	 * Unless it is a brand new workspace, or a new file,
	 * rebuild the id cache if look up failed.
	 *
	 * If the file is gone, it's an error to get updates to that file.
	 *
	 * XXX - we need some test cases in the regression scripts for this.
	 * a) move the file and make sure it finds it
	 * b) move the file and send a new file over and make sure it finds
	 *    it.
	 */
	unless (s || new_s) {
		if (gone(t, opts->goneDB)) {
			if (getenv("BK_GONE_OK")) {
				skipPatch(p);
				return (0);
			} else {
				errorMsg("tp_gone_error", t, 0);
			}
		}
		SHOUT();
		fprintf(stderr,
		    "takepatch: can't find key '%s' in id cache\n", t);
error:		if (new_s && (new_s != s)) sccs_free(new_s);
		if (perfile) fclose(perfile);
		if (gfile) free(gfile);
		free(t);
		if (s) sccs_free(s);
		opts->errfiles = addLine(opts->errfiles, sccs2name(name));
		return (-1);
	}

	/*
	 * They may have sent us a patch from 1.0, so the patch looks like a
	 * new file.  But if we have a match, we want to use it.
	 */
	if (s) {
		if (newFile && (opts->echo > 4)) {
			fprintf(stderr,
			    "takepatch: new file %s already exists.\n", name);
		}
		if (opts->echo > 7) {
			fprintf(stderr, "takepatch: file %s found.\n",
			s->sfile);
		}
		if (EDITED(s)) {
			int cleanflags = SILENT|CLEAN_SHUTUP|CLEAN_CHECKONLY;

			if (sccs_clean(s, cleanflags)) {
				opts->edited =
				    addLine(opts->edited, sccs2name(s->sfile));
				sccs_free(s);
				s = 0;
				goto error;
			} else {
				sccs_restart(s);
			}
		} else if (s->state & S_PFILE) {
			if (xfile_delete(s->gfile, 'p')) {
				fprintf(stderr,
				    "takepatch: unlink(%s): %s\n",
				    s->sfile, strerror(errno));
				goto error;
			}
			s->state &= ~S_PFILE;
		}
		tmp = sccs_top(s);
		unless (CSET(s)) {
			unless (sccs_patheq(PATHNAME(s, tmp), s->gfile)) {
				badpath(s, tmp);
				goto error;
			}
		}
		unless (tmp = sccs_findKey(s, t)) {
			shout();
			fprintf(stderr,
			    "takepatch: can't find root delta '%s' in %s\n",
			    t, name);
			goto error;
		}
		unless (sccs_ino(s) == tmp) {
			shout();
			fprintf(stderr,
			    "takepatch: root deltas do not match in %s\n",
			    name);
			goto error;
		}
	} else {	/* create a new file */
		/*
		 * ChangeSet is special, don't rename it,
		 * because "resolve" expects to find a ChangeSet
		 * file in its normal location.
		 * ChangeSet is never renamed, so
		 * it is safe to assume there is no
		 * name conflict for the ChangeSet file.
		 */
		assert(new_s);
		if (streq(name, "SCCS/s.ChangeSet") &&
		    exists("SCCS/s.ChangeSet")) {
			errorMsg("tp_changeset_exists", 0, 0);
		}
		if (opts->echo > 3) {
			fprintf(stderr,
			    "takepatch: new file %s\n", t);
		}
	}
	cset = s ? CSET(s) : CSET(new_s);
	if (opts->pbars && cset) {
		countFiles = hash_new(HASH_MEMHASH);
		opts->N++;	/* count the cset file */
	}
	scratch = new(sccs);
	while (extractDelta(
	    name, s, scratch, newFile, p, countFiles, &nfound)) {
		if (newFile) newFile = 2;
	}
	sccs_free(scratch);
	if (countFiles) {
		hash_free(countFiles);
		countFiles = 0;
	}
	gfile = sccs2name(name);
	if (opts->echo > 1) fprintf(stderr, "Updating %s", gfile);

	newsfile = !s;

	unless (s) {
		s = new_s;
		new_s = 0;
	}
	rc = applyCsetPatch(s, &nfound, newsfile);
	s = 0;		/* applyCsetPatch calls sccs_free */
	unless (cset) nfound = 0;	/* only count csets */

	if (opts->echo > 1) fputc('\n', stderr);
	if (perfile) fclose(perfile);
	if (streq(gfile, "BitKeeper/etc/config")) {
		/*
		 * If we have rewritten the config file we need to
		 * flush the cache of config data in the project
		 * struct.
		 */
		proj_reset(0);
	}
	free(gfile);
	free(t);
	if (new_s) sccs_free(new_s);
	if (s) sccs_free(s);
	if (rc < 0) {
		cleanup(CLEAN_RESYNC);
		return (rc);
	}
	return (nfound);
}

/*
 * Extract one delta from the patch file.
 * Deltas end on the first blank line.
 */
private	int
extractDelta(char *name, sccs *sreal, sccs *scratch,
    int newFile, FILE *f, hash *countFiles, int *np)
{
	ser_t	d, tmp;
	char	buf[MAXPATH];
	char	*b, *sep;
	char	*pid = 0;
	int	c, i;
	int	ignore = 0;
	size_t	len;
	patch	*p;
	FILE	*init = 0;

	if (newFile == 1) goto delta1;

	b = fgetline(f); opts->line++;
	if (opts->echo>4) fprintf(stderr, "%s\n", b);
	if (strneq(b, "# Patch checksum=", 17)) return 0;
	pid = strdup(b);

	/* buffer the required init block */
delta1:	while ((b = fgetline(f)) && *b) {
		unless (init) init = fmem();
		fputs(b, init);
		fputc('\n', init);
		opts->line++;
		if (opts->echo>4) fprintf(stderr, "%s\n", b);
	}
	if (b) opts->line++;
	assert(init);
	rewind(init);

	/* go get the delta table entry for this delta */
	d = getRecord(scratch, init);
	rewind(init);
	/*
	 * 11/29/02 - we fill in dangling deltas by pretending they are
	 * incoming deltas which we do not already have.  In the patch
	 * they are not going to be dangling so the flag is clear;
	 * applying the patch will clear the flag and the code paths
	 * are all happier this way.
	 */
	if (!opts->fast && sreal && sccs_sdelta(scratch, d, buf) &&
	    (tmp = sccs_findKey(sreal, buf)) && !DANGLING(sreal, tmp)) {
		if (opts->echo > 3) {
			fprintf(stderr,
			    "takepatch: delta %s already in %s, skipping it.\n",
			    REV(sreal, tmp), sreal->sfile);
		}
		free(pid);
		fclose(init);
		init = 0;
		/* Eat diffs */
		while ((b = fgetln(f, &len)) && (len > 1)) opts->line++;
		opts->line++;
	} else {
		if (ignore) {
			/* fastpatch - ignore init block, keep diffs */
			fclose(init);
			init = 0;
		}
		p = new(patch);
		if (init) {
			p->initMem.buf = fmem_close(init, &len);
			p->initMem.len = len;
			p->initMem.size = len; /* really +1 */
			init = 0;
		}
		if (opts->echo>5) fprintf(stderr, "\n");
		p->remote = 1;
		p->pid = pid;
		sccs_sdelta(scratch, d, buf);
		p->me = strdup(buf);
		sccs_sortkey(scratch, d, buf);
		p->sortkey = strdup(buf);
		p->localFile = sreal ? strdup(sreal->sfile) : 0;
		sprintf(buf, "RESYNC/%s", name);
		p->resyncFile = strdup(buf);
		p->order = DATE(scratch, d);
		c = opts->line;
		while (b = fgetln(f, &len)) {
			if (len && (b[len-1] == '\n')) --len;
			b[len] = 0;
			unless (len) break;
			unless (p->diffMem) p->diffMem = fmem();
			if (countFiles && (*b == '>')) {
				i = opts->fast ? 1 : 2;
				sep = separator(&b[i]);
				assert(sep);
				*sep = 0;
				if (!changesetKey(&b[i]) &&
				    hash_insertStr(countFiles, &b[i], 0)) {
					opts->N++;  /* for progress bar */
				}
				*sep = ' ';
			}
			fputs(b, p->diffMem);
			fputc('\n', p->diffMem);
			opts->line++;
			if (opts->echo>5) fprintf(stderr, "%s\n", b);
		}
		if (p->diffMem) rewind(p->diffMem);
		if (opts->fast) {
			/* header and diff are not connected */
			if (FLAGS(scratch, d) & D_META) p->meta = 1;
		} else if (FLAGS(scratch, d) & D_META) {
			p->meta = 1;
			assert(c == opts->line);
			assert(!p->diffMem);
		}
		opts->line++;
		if (opts->echo>5) fprintf(stderr, "\n");
		(*np)++;
		insertPatch(p, 1);
	}
	sccs_freedelta(scratch, d);
	if ((c = getc(f)) != EOF) {
		ungetc(c, f);
		return (c != '=');
	}
	return (0);
}

/*
 * Skip to the next file start.
 * Deltas end on the first blank line.
 */
private	int
skipPatch(FILE *p)
{
	char	*b;
	int	c;

	do {
		b = fgetline(p); opts->line++;
		if (strneq(b, "# Patch checksum=", 17)) return 0;
		/* Eat metadata */
		while ((b = fgetline(p)) && *b) opts->line++;
		opts->line++;
		/* Eat diffs */
		while ((b = fgetline(p)) && *b) opts->line++;
		opts->line++;
		if ((c = getc(p)) == EOF) return (0);
		ungetc(c, p);
	} while (c != '=');
	return (0);
}

private void
errorMsg(char *msg, char *arg1, char *arg2)
{
	SHOUT();
	getMsg2(msg, arg1, arg2, 0, stderr);
    	cleanup(CLEAN_RESYNC|CLEAN_PENDING);
}

private	void
badpath(sccs *s, ser_t tot)
{
	SHOUT();
	getMsg2("tp_badpath", s->gfile, PATHNAME(s, tot), 0, stderr);
}

private	void
badXsum(int a, int b)
{
	SHOUT();
	if (opts->echo > 3) {
		char	*p = aprintf(" (%x != %x)", a, b);

		getMsg("tp_badXsum", p, 0, stderr);
		free(p);
	} else {
		getMsg("tp_badXsum", "", 0, stderr);
	}
	cleanup(CLEAN_PENDING|CLEAN_RESYNC);
	/* XXX - should clean up everything if this was takepatch -i */
}

/*
 * This tries within a reasonable amount of effort to write the
 * SCCS file only once.  This can be done because the sccs struct
 * is in memory and can be fiddled with carefully.  The weaving
 * of new information is done when we are done fiddling with the sccs
 * file and write out the table, then weave the new deltas into the
 * body.  While this is doable for the general weave case, it is
 * very doable for the ChangeSet file (or any hash file) because
 * the weave is an ordered set of blocks, youngest to oldest.
 * So by building a table of blocks to weave, as well as a way to
 * know how to increment the serial number of existing blocks, it
 * is possible to only write the file once.
 */
private	int
applyCsetPatch(sccs *s, int *nfound, int newFile)
{
	patch	*p;
	FILE	*iF;
	FILE	*dF;
	ser_t	d = 0;
	ser_t	top;
	ser_t	remote_tagtip = 0;
	int	n = 0;
	int	j;
	int	psize = *nfound;
	int	confThisFile;
	FILE	*csets = 0, *f;
	hash	*cdb;
	char	*topkey = 0;
	char	csets_in[MAXPATH];
	char	buf[MAXKEY];
	char	*resyncFile;

	assert(s);
	if (CSET(s)) T_PERF("applyCsetPatch(start)");
	reversePatch();
	unless (p = opts->patchList) {
		sccs_free(s);
		return (0);
	}

	if (opts->echo > 7) {
		fprintf(stderr, "L=%s\nR=%s\nP=%s\nM=%s\n",
		    p->localFile, p->resyncFile, p->pid, p->me);
	}
	resyncFile = p->resyncFile;
	if (newFile) {
		/* good to go */
	} else if (getenv("_BK_COPY_SFILE")) {
		if (fileCopy(s->sfile, p->resyncFile)) {
			fprintf(stderr, "Copy of %s to %s failed",
			    s->sfile, p->resyncFile);
			perror("takepatch-copy");
			goto err;
		}
		sccs_free(s);
		unless (s = sccs_init(p->resyncFile,
		    INIT_MUSTEXIST|SILENT)) {
			SHOUT();
			fprintf(stderr,
			    "takepatch: can't init %s\n", p->resyncFile);
			goto err;
		}
		if (CSET(s)) {
			cmdlog_addnote(
			    "_BK_COPY_SFILE", getenv("_BK_COPY_SFILE"));
		}
	} else {
		sccs_writeHere(s, p->resyncFile);
	}
	/* serial is not stable */
	if (top = sccs_top(s)) {
		sccs_sdelta(s, top, buf);
		topkey = strdup(buf);
		top = 0;
	}
	assert(s);
	if (opts->bkmerge != BKMERGE(s)) {
		/*
		 * Take the patch in the remote repo's format.
		 * sccs_startWrite will put the file back in the local form.
		 */
		if (graph_convert(s, 0)) goto err;
	}
	cweave_init(s, psize);
	*nfound = 0;
	while (p) {
		n++;
		if (opts->echo>9) {
			fprintf(stderr, "PID: %s\nME:  %s\n",
			    p->pid ? p->pid : "none", p->me);
		}
		assert(p->initMem.buf);
		iF = fmem_buf(p->initMem.buf, p->initMem.len);
		dF = p->diffMem;
		d = 0;	/* in this case, parent */
		if (p->pid) {
			unless (d = sccs_findKey(s, p->pid)) {
				if ((opts->echo == 2) || (opts->echo == 3)) {
					fprintf(stderr, " \n");
				}
				errorMsg("tp_ahead", p->pid, s->sfile);
				/*NOTREACHED*/
			}
		}
		if (opts->echo>9) {
			fprintf(stderr, "Child of %s", d ? REV(s, d) : "none");
			if (p->meta) {
				fprintf(stderr, " meta\n");
			} else {
				fprintf(stderr, " data\n");
			}
		}
		/* passing in d = parent, setting d = new or existing */
		if (d = cset_insert(s, iF, dF, d, opts->fast)) {
			if (d == D_INVALID) cleanup(CLEAN_RESYNC|CLEAN_PENDING);
			(*nfound)++;
			p->serial = d;
			if ((FLAGS(s, d) & D_REMOTE) && SYMGRAPH(s, d)) {
				remote_tagtip = d;
			}
		}
		fclose(iF); /* dF needs to stick around until write */
		p = p->next;
	}
	unless (*nfound) goto done;
	/*
	 * pull -r can propagate a non-tip tag element as the tip.
	 * We have to mark it here before writing the file out.
	 */
	if (remote_tagtip) {
		d = remote_tagtip;
		assert(FLAGS(s, d));
		if (!SYMLEAF(s, d)) {
			assert(CSET(s));
			FLAGS(s, d) |= D_SYMLEAF;
			debug((stderr,
				"takepatch: adding leaf to tag "
				"delta %s (serial %d)\n",
				REV(s, d), d));
		}
	}
	/*
	 * When porting in a csetfile, need to ignore path names
	 * XXX: component moves looks will break this.
	 */
	if (topkey) top = sccs_findKey(s, topkey);
	if (opts->port && CSET(s)) {
		int	portLocal = 0;
		char	*path;

		assert(topkey);	// no porting a new component; use attach
		for (d = 0, p = opts->patchList; p; p = p->next) {
			d = p->serial;
			unless (d && (FLAGS(s, d) & D_REMOTE)) continue;
			unless (PARENT(s, d)) continue;	/* rootkey untouched */
			/*
			 * We don't yet support component rename.
			 */
			if (!streq(PATHNAME(s, top),
			    (path = PATHNAME(s, PARENT(s, d)))) ||
			    (MERGE(s, d) &&
			    !streq(PATHNAME(s, top),
			    (path = PATHNAME(s, MERGE(s, d)))))) {
				getMsg2("tp_portrename",
				    PATHNAME(s, top), path, 0, stderr);
    				cleanup(CLEAN_RESYNC|CLEAN_PENDING);
				/*NOTREACHED*/
			}
			sccs_setPath(s, d, PATHNAME(s, PARENT(s, d)));
			if (TAG(s, d)) continue;
			if (proj_isComponent(s->proj)) {
				/* Sanity assertion - can't port to self */
				if (streq(CSETFILE(s, d),
				    proj_rootkey(proj_product(s->proj)))) {
					portLocal = 1;
				}
			} else {
				/*
				 * If porting to a standalone, we need to add
				 * back the cset marks.
				 */
				if (d != TREE(s)) FLAGS(s, d) |= D_CSET;
			}
		}
		if (portLocal) {	/* illegal to port local */
			reversePatch();	/* new to old */
			SHOUT();
			for (d = 0, p = opts->patchList; p; p = p->next) {
				unless ((d = p->serial) &&
				    (FLAGS(s, d) & D_REMOTE) && !TAG(s, d)) {
					continue;
				}
				unless ((FLAGS(s, d) & D_SET) ||
				    streq(CSETFILE(s, d),
				    proj_rootkey(proj_product(s->proj)))) {
					continue;
				}
				EACH_PARENT(s, p->serial, d, j) {
					FLAGS(s, d) |= D_SET;
				}
				d = p->serial;
				if (FLAGS(s, d) & D_SET) continue;
				/* only print error message for tips */
				sccs_sdelta(s, d, buf);
				/*
				 * I'd like to use md5root but I just added
				 * support for md5keys to key2path so not yet.
				 */
				getMsg2("tp_portself",
				    proj_rootkey(s->proj), buf, 0, stderr);
			}
    			cleanup(CLEAN_RESYNC|CLEAN_PENDING);
			/*NOTREACHED*/
		}
	}
	if (CSET(s) && (opts->echo == 3)) fputs(", ", stderr);
	assert(opts->bkmerge == BKMERGE(s));
	if (mkdirf(resyncFile) == -1) {
		if (errno == EINVAL) {
			getMsg("reserved_name", resyncFile, '=', stderr);
			return (-1);
		}
	}
	if (cset_write(s, (opts->echo == 3), opts->fast)) {
		SHOUT();
		fprintf(stderr, "takepatch: can't update %s\n", s->sfile);
		goto err;
	}
	if (CSET(s)) T_PERF("cset_write");
	s = sccs_restart(s);
	assert(s);

	unless (CSET(s)) goto markup;

	if (cdb = loadCollapsed()) {
		for (p = opts->patchList; p; p = p->next) {
			d = p->serial;
			unless (d && (FLAGS(s, d) & D_REMOTE)) continue;
			sccs_md5delta(s, d, buf);
			if (hash_fetchStr(cdb, buf)) {
				/* find cset that added that entry */
				sprintf(buf, "bk -R r2c -r%s " COLLAPSED,
				    (char *)cdb->vptr);
				f = popen(buf, "r");
				fnext(buf, f);
				chomp(buf);
				pclose(f);
				errorMsg("takepatch-collapsed", cdb->kptr, buf);
				hash_free(cdb);
				goto err;
			}
		}
		hash_free(cdb);
	}
	/* examine incoming csets and looks for updates to components with
	 * pending deltas
	 */
	if (proj_isProduct(0)) {
		for (p = opts->patchList; p; p = p->next) {
			char	*t, *s;
			char	key[MAXKEY];

			unless (p->diffMem) continue;
			rewind(p->diffMem);
			while (t = fgetline(p->diffMem)) {
				unless (*t == '>') continue;
				++t;		      /* skip '>' */
				unless (opts->fast) ++t;    /* skip space */
				s = separator(t);
				strncpy(key, t, s-t);
				key[s-t] = 0;

				unless (changesetKey(key)) continue;

				t = key2path(key, opts->idDB, opts->goneDB, 0);
				if (sccs_isPending(t)) {
					dirname(t); /* strip /ChangeSet */
					getMsg("tp_uncommitted",
					    t, 0, stderr);
					free(t);
					goto err;
				}
				free(t);
			}
		}
	}
	sprintf(csets_in, "%s/%s", ROOT2RESYNC, CSETS_IN);
	csets = fopen(csets_in, "w");
	assert(csets);
	for (p = opts->patchList; p; p = p->next) {
		d = p->serial;
		unless (d && (FLAGS(s, d) & D_REMOTE)) continue;
		sccs_sdelta(s, d, buf);
		fprintf(csets, "%s\n", buf);
	}
	fclose(csets);
markup:
	/*
	 * D_REMOTE used in sccs_resolveFiles()
	 * D_SET used in cset_resum()
	 */
	for (d = 0, p = opts->patchList; p; p = p->next) {
		/*
		 * In fastpatch, diffMem is not related to initMem, so
		 * just clear all of them.
		 * win32: must fclose after cset_write
		 */
		if (p->diffMem) fclose(p->diffMem);
		p->diffMem = 0;
		d = p->serial;
		unless (d && (FLAGS(s, d) & D_REMOTE)) continue;
		FLAGS(s, d) |= D_SET; /* for resum() */
	}
	if (!CSET(s) && top &&
	    (DANGLING(s, top) || !(FLAGS(s, top) & D_CSET))) {
		ser_t	a, b;

		if (DANGLING(s, top) && sccs_findtips(s, &a, &b)) {
			fprintf(stderr, "takepatch: monotonic file %s "
			    "has dangling deltas\n", s->sfile);
			goto err;
		}
		if (!(FLAGS(s, top) & D_CSET) && sccs_isleaf(s, top)) {
			/* uncommitted error for dangling is backward compat */
			char	*t = sccs2name(opts->patchList->localFile);

			SHOUT();
			getMsg("tp_uncommitted", t, 0, stderr);
			free(t);
			goto err;
		}
	}
	/*
	 * Make a new changeset node in resolve if no new node created
	 */
	s->state |= S_SET;
	if (CSET(s)) {
		if (opts->echo == 3) fputs(", ", stderr);
		if (cset_resum(s, 0, 0, opts->echo == 3, 1)) {
			getMsg("takepatch-chksum", 0, '=', stderr);
			goto err;
		}
		if (opts->echo == 3) progress_nldone();
	} else if (!BAM(s)) {
		for (d = TABLE(s); d >= TREE(s); d--) {
			unless ((FLAGS(s, d) & D_SET) && !TAG(s, d)) continue;
			if (sccs_resum(s, d, 0, 0)) {
				getMsg("takepatch-chksum", 0, '=', stderr);
				goto err;
			}
		}
	}

	if ((confThisFile = sccs_resolveFiles(s, opts->automerge)) < 0) {
		goto err;
	}
	if (!confThisFile && (s->state & S_CSET) && 
	    sccs_adminFlag(s, SILENT|ADMIN_BK)) {
	    	confThisFile++;
		/* yeah, the count is slightly off if there were conflicts */
	}
	opts->conflicts += confThisFile;
	if (BAM(s) && !bp_hasBAM()) {
		/* this shouldn't be needed... */
		if (touch(BAM_MARKER, 0664)) perror(BAM_MARKER);
	}
done:	if (CSET(s)) T_PERF("done cset");
	sccs_free(s);
	s = 0;
	if (opts->noConflicts && opts->conflicts) {
		errorMsg("tp_noconflicts", 0, 0);
	}
	freePatchList();
	if (topkey) free(topkey);
	opts->patchList = 0;
	return (0);
err:
	if (topkey) free(topkey);
	if (s) sccs_free(s);
	return (-1);
}

/*
 * Return true if 'a' is earlier than 'b'
 */
private	int
earlierPatch(patch *a, patch *b)
{
	int ret;

	if (a->order < b->order) return 1;
	if (a->order > b->order) return 0;
	ret = strcmp(a->sortkey, b->sortkey);
	if (ret < 0)   return 1;
	if (ret > 0)   return 0;
	assert("Can't figure out the order of deltas\n" == 0);
	return (-1); /* shut off VC++ comipler warning */
}

/*
 * Insert the delta in the list in sorted time order.
 * The global patchList points at the newest/youngest.
 */
private	void
insertPatch(patch *p, int strictOrder)
{
	patch	*t;

	if (!opts->patchList || earlierPatch(opts->patchList, p)) {
		p->next = opts->patchList;
		opts->patchList = p;
		return;
	} else if (strictOrder) {
		fprintf(stderr,
		    "%s: patch not in old to new order:\n%s\n%s\n",
		    prog, opts->patchList->me, p->me);
		cleanup(CLEAN_RESYNC);
	}
	/*
	 * We know that t is pointing to a node that is younger than us.
	 */
	for (t = opts->patchList; t->next; t = t->next) {
		if (earlierPatch(t->next, p)) {
			p->next = t->next;
			t->next = p;
			return;
		}
	}

	/*
	 * There is no next field and we know that t->order is > date.
	 */
	assert(earlierPatch(p, t));
	t->next = p;
}

/*
 * Reverse order to optimize reading
 * patchList will be left pointing at the oldest delta in the patch
 */
private	int
reversePatch(void)
{
	int	n = 0;

	/* t - temp; f - forward; p - previous */
	patch	*t, *f, *p;

	if (!opts->patchList) return (n);
	for (p = 0, t = opts->patchList; t; t = f) {
		f = t->next;
		t->next = p;
		p = t;
		++n;
	}
	assert(p);
	opts->patchList = p;
	return (n);
}

private	void
freePatchList(void)
{
	patch	*p;

	for (p = opts->patchList; p; ) {
		patch	*next = p->next;

		if (p->initFile) {
			unlink(p->initFile);
			free(p->initFile);
		}
		if (p->diffFile) {
			unlink(p->diffFile);
			free(p->diffFile);
		}
		if (p->localFile) free(p->localFile);
		if (p->initMem.buf) free(p->initMem.buf);
		free(p->resyncFile);
		if (p->pid) free(p->pid);
		if (p->me) free(p->me);
		if (p->sortkey) free(p->sortkey);
		free(p);
		p = next;
	}
}

/*
 * Create enough stuff that the tools can find the project root.
 */
private	void
initProject(void)
{
	unless (emptyDir(".")) {
		SHOUT();
		fprintf(stderr,
		    "takepatch: -i can only be used in an empty directory\n");
		SHOUT2();
		/*
		 * We MUST exit here.  It is an invariant that if we are not
		 * empty we abort.  See cleanup().
		 */
		exit(1);
	}
	sccs_mkroot(".");
	/*
	 * Make new repo with new features
	 * Who uses takepatch -i to make a repo?
	 * XXX: doesn't work correctly with nested.
	 *
	 * From setup.c, in the non-compat, non-component case.
	 * There's more code in setup.c to do nested component.
	 */
	features_set(0, (FEAT_FILEFORMAT & ~FEAT_BWEAVEv2) | FEAT_SCANDIRS, 1);
}

private void
resync_lock(void)
{
	FILE	*f;

	if (opts->needlock) cmdlog_lock(CMD_WRLOCK);

	/*
	 * See if we can lock the tree.
	 * We assume that a higher level program called repository_wrlock(),
	 * we're just doing the RESYNC part.
	 *
	 * Note: bk's mkdir will pass if RESYNC already exists so we need
	 * to test that separately.
	 */
	if ((isdir("RESYNC") && !nested_mine(0, getenv("_BK_NESTED_LOCK"), 1))
	    || mkdir("RESYNC", 0777)) {
		fprintf(stderr, "takepatch: cannot create RESYNC dir.\n");
		repository_lockers(0);
		cleanup(0);
	}
	unless (mkdir("RESYNC/BitKeeper", 0777) == 0) {
		SHOUT();
		perror("mkdir");
		cleanup(CLEAN_RESYNC);
	}
	sccs_mkroot("RESYNC");
	unless (f = fopen("RESYNC/BitKeeper/tmp/pid", "w")) {
		SHOUT();
		perror("RESYNC/BitKeeper/tmp/pid");
		cleanup(CLEAN_RESYNC);
	}
	fprintf(f, "%u\n", getpid());
	if (proj_isProduct(0)) {
		mkdirp("RESYNC/BitKeeper/log");
		touch("RESYNC/BitKeeper/log/PRODUCT", 0644);
	}
	fclose(f);
}

/*
 * Next line is patch checksum, then SFIO data follows.
 */
private int
sfio(FILE *m, int files)
{
	char	*t;
	FILE	*f = 0;
	sccs	*s = 0, *sr = 0;
	ser_t	d;
	size_t	n;
	char	*flist;
	int	rc = -1, rlen;
	int	bkfile = (features_test(0, FEAT_BKFILE) != 0);
	int	bkmerge = (features_test(0, FEAT_BKMERGE) != 0);
	char	buf[MAXLINE];
	char	key[MAXKEY];

	unless ((t = fgetline(m)) && !*t &&
	    (t = fgetline(m)) && strneq(t, "# Patch checksum=", 17)) {
		return (-1);
	}

	flist = bktmp(0);
	if (opts->pbars && (files > 0)) {
		sprintf(key, "-N%d --takepatch", files);
	} else if (opts->echo > 1) {
		sprintf(key, "-v -P'Updating '");
	} else {
		sprintf(key, "-q");
	}
	sprintf(buf, "bk sfio -eigB %s - > '%s'", key, flist);

	fflush(stdout);
	chdir(ROOT2RESYNC);
	f = popen(buf, "w");
	chdir(RESYNC2ROOT);
	unless (f) {
		perror(prog);
		goto err;
	};
	while ((n = fread(buf, 1, sizeof(buf), m)) > 0) {
		fwrite(buf, 1, n, f);
	}
	if (opts->echo > 1) progress_nldone();
	if (pclose(f)) {
		fprintf(stderr, "takepatch: BAM sfio -i failed.\n");
		f = 0;
		goto err;
	}
	/* process each file unpacked */
	unless (f = fopen(flist, "r")) {
		perror(prog);
		goto err;
	}
	strcpy(buf, ROOT2RESYNC "/");
	t = buf + strlen(buf);
	rlen = sizeof(buf) - strlen(buf); /* space in buf after RESYNC/ */
	while (fgets(t, rlen, f)) {
		chomp(t);
		if (strchr(t, '|')) continue; /* skip BAM keys */

		unless (sr = sccs_init(buf,
			SILENT|INIT_NOCKSUM|INIT_MUSTEXIST)) {
			fprintf(stderr, "takepatch: can't open %s\n", buf);
			goto err;
		}
		if ((bkfile != BKFILE(sr)) || (bkmerge != BKMERGE(sr))) {
			sccs_newchksum(sr);	/* fix format */
		}
		sccs_sdelta(sr, sccs_ino(sr), key); /* rootkey */

		/* Check the original version of this file */
		unless (s = sccs_keyinit(0, key, INIT_NOCKSUM, opts->idDB)) {
			/* must be new file? */
			sccs_free(sr);
			sr = 0;
			continue;
		}
		assert(!CSET(s));  /* some more logic is needed for cset */

		/* local diffs are bad */
		if (sccs_clean(s, SILENT|CLEAN_SHUTUP|CLEAN_CHECKONLY)) {
			opts->edited =
			    addLine(opts->edited, sccs2name(s->sfile));
			goto err;
		}
		if (DANGLING(s, TABLE(s))) {
			fprintf(stderr, "takepatch: monotonic file %s "
			    "has dangling deltas\n", s->sfile);
			goto err;
		}
		sccs_sdelta(s, TABLE(s), key); /* local tipkey */

		unless (d = sccs_findKey(sr, key)) {
			/*
			 * I can't find local tipkey in resync file.
			 * This might be because it is a pending delta, if
			 * so complain about that, otherwise just complain.
			 * Note, it is OK to have a pending delta that
			 * has been committed in the resync version, the old
			 * code used to ignore these duplicate deltas.
			 */

			/* because of pending deltas? */
			unless (FLAGS(s, TABLE(s)) & D_CSET) {
				SHOUT();
				getMsg("tp_uncommitted", s->gfile, 0, stderr);
			} else {
				fprintf(stderr,
				    "takepatch: key '%s' not found "
				    "in sfile %s\n", key, buf);
			}
			goto err;
		}
		/* mark remote-only deltas */
		range_walkrevs(sr, L(d), 0, 0,
		    walkrevs_setFlags, (void*)D_REMOTE);
		/*
		 * techically, FLAGS(s, d) |= D_LOCAL, but D_LOCAL goes away
		 * in /home/bk/bk and the way resolveFiles is written, it
		 * does the right thing with or without D_LOCAL.
		 */
		FLAGS(s, d) |= D_LOCAL;
		if (sccs_resolveFiles(sr, opts->automerge) < 0) goto err;
		sccs_free(s);
		sccs_free(sr);
		s = sr = 0;
	}
	rc = 0;

err:	if (f) fclose(f);
	unlink(flist);
	free(flist);
	if (s) sccs_free(s);
	if (sr) sccs_free(sr);
	return (rc);
}

/*
 * The verifier engine from the old init.  It used to be a bubble
 * in the pipeline to read and verify the stdin, then takepatch it.
 * Now it is is done in parallel though I/O layering.
 *
 * Init builds it up and gets it going.  fclose() tears it down.
 * Note: need to fclose this stuff.  Also think about checksumming
 * in a different thread or in this one.
 *
 * The I/O layer has some braindeadness to make it easier to read.
 * We are reading and parsing lines, which might not fit into the
 * buffer handed to us to fill.  Save in a 'leftover' fmem, and
 * the next readPatch call, only return what is in that buffer until
 * it is empty, then fclose the fmem().  We could keep the fmem open,
 * but this easier to know it works: if fmem, return it, else read
 * new data.  In reality, fmem never used.
 */

typedef struct {
	FILE	*leftover;	/* fmem of extra crud for next time */
	FILE	*pending;	/* fmem of extra crud for next time */
	FILE	*fin;		/* the file it is sitting on */
	long	bytecount;	/* size of pending file for logs */
	uLong	sumC, sumR;	/* The computed and real checksum */

	/* state bits */
	u32	longline:1;	/* a continuation of earlier line */
	u32	newline:1;	/* current line is a newline */
	u32	preamble_nl:1;	/* previous line was a newline */
	u32	preamble:1;	/* looking for patch version */
	u32	version:1;	/* previous line was Patch vers: ... */
	u32	type:1;		/* looking for patch type */
	u32	versionblank:1;	/* previous line was \n after vers */
	u32	filename:1;	/* previous line was == file == */
	u32	first:1;	/* previous line was == file == */
	u32	perfile:1;	/* previous line was perfile stuff */
	u32	perblank:1;	/* previous line was per blank */
	u32	metadata:1;	/* previous line was meta data block */
	u32	metaline:1;	/* previous line was 48x- */
	u32	metablank:1;	/* previous line was \n after meta */
	u32	diffs:1;	/* previous line was diffs */
	u32	diffsblank:1;	/* previous line was \n after diffs */
	u32	sfiopatch:1;	/* previous line was == @SFIO == */
	u32	sfiopending:1;	/* saw SFIO, but wait until checksum */
	u32	sfio:1;		/* parse the sfio */
	u32	nosum:1;	/* turn off checksumming */
} pstate;

private	int
patchRead(void *cookie, char *outbuf, int outlen)
{
	pstate	*st = (pstate *)cookie;
	char	*p;
	size_t	left = outlen, len = 0, count = 0;
	char	buf[MAXLINE];

	if (st->leftover &&
	   (p = fmem_peek(st->leftover, &len)) && len) {
		/* If we have overflow from last time, just return it */
		count = min(len, left);
		memcpy(outbuf, p, count);
		outbuf += count;
		left -= count;
		if (len -= count) {
			memmove(p, &p[count], len);
			ftrunc(st->leftover, len);
		} else {
			fclose(st->leftover);
			st->leftover = 0;
		}
		return (outlen - left);
	}
	if (st->sfio) {
		len = fread(outbuf, 1, left, st->fin);
		left -= len;
		st->bytecount += len;
		if (st->pending &&
		    (len != fwrite(outbuf, 1, len, st->pending))) {
			// XXX: die or keep going with no pending file?
			fprintf(stderr, "error saving patch\n");
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
		return (outlen - left);
	}
again:	assert(left);
	unless (fnext(buf, st->fin)) {
		if (st->preamble) errorMsg("tp_nothingtodo", 0, 0);
		return (outlen - left);
	}
	st->newline = (!st->longline && streq("\n", buf));
	if (opts->echo > 10) {
		fprintf(stderr, "ST: ");
		if (st->newline) fprintf(stderr, "nl ");
		if (st->longline) fprintf(stderr, "ll ");
		if (st->preamble_nl) fprintf(stderr, "p_nl ");
		if (st->preamble) fprintf(stderr, "p ");
		if (st->version) fprintf(stderr, "v ");
		if (st->versionblank) fprintf(stderr, "vb ");
		if (st->filename) fprintf(stderr, "f ");
		if (st->first) fprintf(stderr, "1 ");
		if (st->perfile) fprintf(stderr, "pf ");
		if (st->perblank) fprintf(stderr, "pb ");
		if (st->metadata) fprintf(stderr, "m ");
		if (st->metaline) fprintf(stderr, "ml ");
		if (st->metablank) fprintf(stderr, "mb ");
		if (st->diffs) fprintf(stderr, "d ");
		if (st->diffsblank) fprintf(stderr, "db ");
		if (st->sfiopatch) fprintf(stderr, "patch ");
		if (st->sfiopending) fprintf(stderr, "psfio ");
		if (st->sfio) fprintf(stderr, "sfio ");
		fputs(buf, stderr);
	}
	if (opts->echo > 7) fprintf(stderr, "P: %s", buf);
	
	if (st->longline) {
		/* do nothing but pass through the rest of the line */
	} else if (st->preamble) {
		if (st->newline) {
			st->preamble_nl = 1;
		}
		if (st->preamble_nl) {
			if (streq(buf, PATCH_CURRENT) ||
			    (opts->fast = streq(buf, PATCH_FAST))) {
				st->type = 1;
				st->preamble = 0;
				st->preamble_nl = 0;
			}
		}
	} else if (st->type) {
		if (strneq(buf, PATCH_FEATURES, strsz(PATCH_FEATURES))) {
			st->type = 0;
			st->version = 1;
		} else if (streq(buf, PATCH_REGULAR)) {
			st->type = 0;
			st->version = 1;
		} else {
			fprintf(stderr, "Expected type\n");
			goto error;
		}
	} else if (st->version) {
		if (st->newline) {
			st->version = 0;
			st->versionblank = 1;
		} else {	/* false alarm */
			if (st->pending) rewind(st->pending);
			st->preamble = 1;
			st->version = 0;
		}
	} else if (st->versionblank) {
		if (strneq("== ", buf, 3)) {
			st->versionblank = 0;
			st->filename = 1;
			st->first = 1;
		} else if (strneq(buf, "# Patch checksum=", 17)) {
			goto dosum;
		} else {
			fprintf(stderr, "Expected '== f =='\n");
error:			fprintf(stderr, "GOT: %s", buf);
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
	} else if (st->filename) {
		if (st->newline) {
			fprintf(stderr, "Expected metadata\n");
			goto error;
		} else if (strneq(buf, "New file: ", 10)) {
			st->perfile = 1;
		} else {
			st->metadata = 1;
		}
		st->filename = 0;
	} else if (st->perfile) {
		if (st->newline) {
			st->perfile = 0;
			st->perblank = 1;
		}
	} else if (st->perblank) {
		if (st->newline) {
			fprintf(stderr, "Expected metadata\n");
			goto error;
		}
		st->metadata = 1;
		st->perblank = 0;
	} else if (st->metadata) {
#define	DIVIDER	"------------------------------------------------\n"
		if (st->newline) {
			fprintf(stderr, "Expected metadata\n");
			goto error;
		} else if (streq(buf, DIVIDER)) {
			st->metadata = 0;
			st->metaline = 1;
		}
	} else if (st->metaline) {
		if (st->newline) {
			st->metaline = 0;
			st->metablank = 1;
		} else {
			fprintf(stderr, "Expected metablank\n");
			goto error;
		}
	} else if (st->metablank) {
		st->metablank = 0;
		if (st->newline) {	/* no diffs */
			st->diffsblank = 1;
		} else {
			st->diffs = 1;
		}
	} else if (st->diffs || st->sfiopatch) {
		if (st->newline) {
			st->sfiopatch = st->diffs = 0;
			st->diffsblank = 1;
		} else if (st->sfiopatch) {
			fprintf(stderr, "Expected newline after @SFIO@\n");
			goto error;
		}
	} else if (st->diffsblank) {
		if (strneq("== @SFIO@ ==", buf, 12)) {
			st->sfiopatch = st->sfiopending = 1;
			st->diffsblank = 0;
		} else if (strneq("== ", buf, 3)) {
			st->diffsblank = 0;
			st->filename = 1;
			st->first = 0;
		} else if (strneq(buf, "# Patch checksum=", 17)) {
dosum:			st->sumR = strtoul(buf+17, 0, 16);
			assert(st->sumR != 0);
			st->nosum = 1;
			st->diffsblank = 0;
			if (st->sfiopending) {
				st->sfiopending = 0;
				st->sfio = 1;
			}
		} else if (st->newline) {
			fprintf(stderr, "Expected '== f ==' or key\n");
		    	goto error;
		} else {
			st->diffsblank = 0;
			st->metadata = 1;
		}
	}
	st->longline = (strchr(buf, '\n') == 0);
	if (st->preamble) goto again;

	if (st->pending) {
		if (fputs(buf, st->pending) == EOF) {
			perror("fputs on patch");
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
	}
	// write buf to outbuf and overflow to the fmembuf
	len = strlen(buf);
	st->bytecount += len;
	unless (st->nosum) st->sumC = adler32(st->sumC, buf, len);
	count = min(len, left);
	memcpy(outbuf, buf, count);
	outbuf += count;
	left -= count;
	if (len -= count) {
		unless (st->leftover) st->leftover = fmem();
		fwrite(&buf[count], 1, len, st->leftover);
	}

	return (outlen - left);
}

private	int
patchClose(void *cookie)
{
	pstate	*st = (pstate *)cookie;
	char	*note, incoming[MAXPATH];
	int	rc = !feof(st->fin);

	opts->p = 0;	/* XXX: Hack: block recursive close badXsum/cleanup */

	/* save byte count for logs */
	Fprintf("BitKeeper/log/byte_count", "%lu\n", st->bytecount);
	if (st->pending) {
		assert(st->fin == stdin);
		rc |= fclose(st->pending);
		note = aprintf("%u", (u32)st->bytecount);
		cmdlog_addnote("psize", note);
		free(note);
		// and do the rename stuff if all is okay.
		strcpy(incoming, opts->pendingFile);
		unless (savefile("PENDING", 0, opts->pendingFile)) {
			SHOUT();
			perror("PENDING");
			cleanup(CLEAN_RESYNC);
		}
		rename(incoming, opts->pendingFile);
		if (Fprintf(
		    "RESYNC/BitKeeper/tmp/patch", "%s\n", opts->pendingFile)
		    < 0) {
			perror("RESYNC/BitKeeper/tmp/patch");
			exit(1);
		}
		if (opts->echo) {
			NOTICE();
			fprintf(stderr,
			    "takepatch: saved entire patch in %s\n",
			    opts->pendingFile);
			NOTICE();
		}
	} else {
		rc |= fclose(st->fin);
	}
	unless (rc || (st->sumR == st->sumC)) badXsum(st->sumR, st->sumC);

	if (st->leftover) fclose(st->leftover);
	free(st);
	return (rc);
}

/*
 * Go find the change set file and do this relative to that.
 * Create the RESYNC dir or bail out if it exists.
 * Put our pid in that dir so that we can figure out if
 * we are still here.
 *
 * This function creates patches in the PENDING directory when the
 * patches are read from stdin.
 */
private	FILE	*
init(char *inputFile)
{
	char	buf[BUFSIZ];		/* used ONLY for input I/O */
	char	*root, *t;
	char	*saveFile;
	int	i;
	FILE	*f = 0;
	pstate	*patch;

	/*
	 * If we are reading from stdin and we get ERROR/Nothing,
	 * then bail out before creating any state.
	 */
	if (streq(inputFile, "-")) {
		if (fnext(buf, stdin)) {
			if (streq(buf, "ERROR\n")) exit(1);
			if (streq(buf, "OK-Nothing to resync.\n")) {
				if (opts->echo) fputs(buf, stderr);
				exit(0);
			}
			for (i = strlen(buf); i; i--) ungetc(buf[i-1], stdin);
		} else {
			errorMsg("tp_nothingtodo", 0, 0);
		}
	}

	if (opts->newProject) {
		initProject();
	} else {
		root = proj_root(0);
		if (!root && emptyDir(".")) {
			/* If we are invoked in an empty directory,
			 * assume they meant -i.
			 */
			if (opts->echo > 0) {
				fputs("takepatch: creating new repository.\n",
				      stderr);
			}
			initProject();
			opts->newProject = 1;
		} else if (proj_cd2root()) {
			SHOUT();
			fputs("takepatch: can't find package root.\n", stderr);
			SHOUT2();
			exit(1);
		}
	}
	resync_lock();
	patch = new(pstate);
	patch->preamble = 1;
	if (streq(inputFile, "-")) {
		patch->fin = stdin;
		/*
		 * Save the patch in the pending dir
		 * and record we're working on it.  We use a .incoming
		 * file and then rename it later.
		 */
		unless (savefile("PENDING", ".incoming", opts->pendingFile)) {
			SHOUT();
			perror("PENDING");
			cleanup(CLEAN_RESYNC);
		}
		saveFile = opts->pendingFile;
		patch->pending = fopen(opts->pendingFile, "wb");
		assert(patch->pending);
	} else {
		patch->preamble_nl = 1;	/* end of pre-amble */
		unless (patch->fin = fopen(inputFile, "r")) {
			perror(inputFile);
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
		saveFile = "";	/* Don't let abort or resolve rm patch file */
	}
	if (Fprintf("RESYNC/BitKeeper/tmp/patch", "%s\n", saveFile) < 0) {
		perror("RESYNC/BitKeeper/tmp/patch");
		exit(1);
	}
	unless (f = funopen(patch, patchRead, 0, 0, patchClose)) {
		perror("funopen patch");
		exit(1);
	}
	while (t = fgetline(f)) {
		if (strneq(t, PATCH_CURRENT, strsz(PATCH_CURRENT)-1) ||
		    (opts->fast = strneq(t, PATCH_FAST, strsz(PATCH_FAST)-1))) {
			break;
		}
	}
	unless (t) errorMsg("tp_noversline", opts->input, 0);
	unless (t = fgetline(f)) {
		cleanup(CLEAN_PENDING|CLEAN_RESYNC);
	}
	if (strneq(t, PATCH_FEATURES, strsz(PATCH_FEATURES))) {
		if (parseFeatures(t + strsz(PATCH_FEATURES))) {
			cleanup(CLEAN_PENDING|CLEAN_RESYNC);
		}
	} else unless (strneq(t, PATCH_REGULAR, strsz(PATCH_REGULAR)-1)) {
		cleanup(CLEAN_PENDING|CLEAN_RESYNC);
	}
	return (f);
}

private	void
cleanup(int what)
{
	int	i, rc = 1;

	if (opts->p) {
		if ((opts->p != stdin) && fclose(opts->p)) {
			fprintf(stderr,
			    "takepatch: failed closing input stream.\n");
		}
		opts->p = 0;
	}
	if (opts->patchList) freePatchList();
	if (opts->idDB) mdbm_close(opts->idDB);
	if (opts->goneDB) mdbm_close(opts->goneDB);
	for (i = 3; i < 20; ++i) close(i);
	if (opts->nway) {
		EACH(opts->errfiles) printf("e %s\n", opts->errfiles[i]);
		freeLines(opts->errfiles, free);
		EACH(opts->edited) printf("p %s\n", opts->edited[i]);
		freeLines(opts->edited, free);
		exit(what | CLEAN_FLAGS);
	}
	if (opts->saveDirs) {
		fprintf(stderr, "takepatch: neither directory removed.\n");
		goto done;
	}
	if (what & CLEAN_RESYNC) {
		rmtree(ROOT2RESYNC);
	} else {
		fprintf(stderr, "takepatch: RESYNC directory left intact.\n");
	}
	unless (streq(opts->input, "-")) goto done;
	if (what & CLEAN_PENDING) {
		unlink(opts->pendingFile);
		if (emptyDir("PENDING")) {
			rmdir("PENDING");
		} else {
			fprintf(stderr,
			    "takepatch: other patches left in PENDING\n");
		}
	} else {
		if (exists(opts->pendingFile)) {
			fprintf(stderr, "takepatch: patch left in %s\n",
			    opts->pendingFile);
		}
	}
done:
	if (what & CLEAN_OK) {
		rc = 0;
	} else {
		if (opts->edited) {
			shout();
			fprintf(stderr,
			    "The following files are modified locally and "
			    "in the patch:\n");
			EACH(opts->edited) {
				fprintf(stderr, "\t%s\n", opts->edited[i]);
			}
			fprintf(stderr,
			    "For more information run \"bk help tp1\"\n");
			freeLines(opts->edited, free);
		}
		SHOUT2();
		if (opts->errfiles) {
			fprintf(stderr,
			    "Errors during update of the following files:\n");
			EACH(opts->errfiles) {
				fprintf(stderr, "%s\n", opts->errfiles[i]);
			}
			SHOUT2();
			freeLines(opts->errfiles, free);
		}
	}
	exit(rc);
}

private int
parseFeatures(char *next)
{
	char	*t;
	int	i;
	char	buf[MAXLINE];

	/* copy upto newline */
	t = buf;
	while (*t++ = *next++) {
		if (*next == '\n') {
			*t = 0;
			break;
		}
	}
	next = buf;
	while (t = strsep(&next, ", \n")) {
		unless (*t) continue;
		for (i = 0;; i++) {
			unless (features[i].name) {
				fprintf(stderr,
				    "%s: patch feature %s unknown\n",
				    prog, t);
				return (-1);
			}
			if (streq(features[i].name, t)) {
				((u8 *)opts)[features[i].offset] = 1;
				break;
			}
		}
	}
	return (0);
}

private	FILE **
startNway(int n)
{
	int	i;
	char	*cmd;
	char	*counter;

	opts->outlist = calloc(n, sizeof(FILE *));
	opts->sent = calloc(n, sizeof(int));
	cmd = aprintf("bk takepatch --Nway%s%s%s%s%s%s "
	    "> RESYNC/BitKeeper/tmp/nwayXXXXX",
	    opts->collapsedups ? " -D" : "",
	    opts->newProject ? " -i" : "",
	    opts->fast ? " --fast" : "",
	    opts->bkmerge ? " --bkmerge" : "",
	    opts->port ? " --port" : "",
	    opts->automerge ? "" : " --no-automerge");
	counter = cmd + strlen(cmd) - 5;
	for (i = 0; i < n; i++) {
		sprintf(counter, "%05d", i);
		unless (opts->outlist[i] = popen(cmd, "w")) {
			stopNway(n, 0, 0);
			break;
		}
	}
	free(cmd);
	return (opts->outlist);
}

private	int
stopNway(int n, int *confp, int *numcsetsp)
{
	int	i, j, conf = 0, numcsets = 0;
	int	cleanFlags = 0;
	int	ret, rc = 0;
	char	file[] = "RESYNC/BitKeeper/tmp/nwayXXXXX";
	char	*counter, **data = 0;
	char	cmd, *param;

	FREE(opts->sent);
	unless (opts->outlist) return (0);
	counter = file + strlen(file) - 5;

	for (i = 0; i < n; i++) {
		if (opts->outlist[i] && (ret = pclose(opts->outlist[i]))) {
			unless (WIFEXITED(ret)) {
				rc = -1;
				continue;
			}
			ret = WEXITSTATUS(ret);
			if (ret & CLEAN_FLAGS) {
				ret &= ~CLEAN_FLAGS;
				/* prefer ERROR to OK */
				if (!cleanFlags ||
				    ((cleanFlags & CLEAN_OK) &&
				    !(ret & CLEAN_OK))) {
					cleanFlags = ret;
				}
			} else {
				rc = -1;
			}
		}
		sprintf(counter, "%05d", i);
		data = file2Lines(data, file);
		EACH_INDEX(data, j) {
			cmd = *data[j];
			param = &data[j][2];
			switch (cmd) {
			    case 'c' :	/* conflict */
				conf += atoi(param);
				break;
			    case 'e' :	/* error */
				opts->errfiles =
				    addLine(opts->errfiles, strdup(param));
			    	break;
			    case 'p' :	/* edited (pfile) */
				opts->edited =
				    addLine(opts->edited, strdup(param));
			    	break;
			    case 'r' :	/* csets taken in from remote */
				numcsets += atoi(param);
				break;
			}
			free(data[j]);
		}
		truncLines(data, 0);
	}
	freeLines(data, 0);
	if (confp) *confp += conf;
	if (numcsetsp) *numcsetsp += numcsets;
	free(opts->outlist);
	opts->outlist = 0;
	if (cleanFlags) cleanup(cleanFlags);
	return (rc);
}

/*
 * See if parallel started.
 *   start it up.
 * Find one with smallest sent (sfio code), send to it.
 * if ChangeSet, count files in patch.
 * Stop with = or # as first char.
 * XXX: Can a key begin with # ?
 */
private	int
sendPatch(char *name, FILE *in)
{
	int	i, c, n, cur, sent;
	int	rc = -1;
	char	*line;
	size_t	len;
	hash	*rk = 0;
	FILE	*out;

	n = opts->parallel;
	if (!opts->outlist && !(opts->outlist = startNway(n))) return (-1);

	if (streq(name, GCHANGESET)) {
		opts->N++;
		rk = hash_new(HASH_MEMHASH);
	}

	cur = 0;
	for (i = 0; i < n; i++) {
		if (opts->sent[i] < opts->sent[cur]) cur = i;
	}
	out = opts->outlist[cur];

	if (opts->echo > 1) fprintf(stderr, "Updating %s\n", name);
	sent = fprintf(out, "== %s ==\n", name);

again:
	line = fgetln(in, &len);
	unless (len) {
err:	
		if (ferror(out)) {
			char	*buf[1024];

			fprintf(stderr, "Draining patch file (%d)\n", rc);
			while (fread(buf, 1, sizeof(buf), in));
		}
		goto done;
	}
	if ((len > 17) &&
	    strneq(line, "# Patch checksum=", 17)) {
		rc = 0;
		goto done;
	}
	unless (len == fwrite(line, 1, len, out)) goto done;
	sent += len;

	/* Per file block */
	if ((len > 10) && strneq(line, "New file: ", 10)) {
		while(line = fgetln(in, &len)) {
			assert(len);
			unless (len == fwrite(line, 1, len, out)) goto err;
			sent += len;
			if (*line == '\n') break;
		}
		unless (len) goto err;
	}

	/* delta meta data block */
	while(line = fgetln(in, &len)) {
		assert(len);
		unless (len == fwrite(line, 1, len, out)) goto err;
		sent += len;
		if (*line == '\n') break;
	}
	unless (len) goto err;

	/* delta data block */
	while(line = fgetln(in, &len)) {
		assert(len);
		unless (len == fwrite(line, 1, len, out)) goto err;
		sent += len;
		if (*line == '\n') break;
		if (rk && (*line == '>')) {
			char	*rkstart, *rkend;

			assert(line[len - 1] == '\n');
			line[len - 1] = 0;	/* just a safety net */
			rkend = separator(line);
			assert(rkend);
			*rkend = 0;
			rkstart = line + (opts->fast ? 1 : 2);
			if (!changesetKey(rkstart) &&
			    hash_insert(rk, rkstart, rkend-rkstart+1, 0, 0)) {
				opts->N++;
			}
			*rkend = ' ';
			line[len - 1] = '\n';
		}
	}
	unless (len) goto err;
	fflush(out);

	if ((c = getc(in)) == EOF) goto done;
	ungetc(c, in);
	if (c != '=') goto again;
	rc = 0;
done:
	opts->sent[cur] += sent;

	// opts->sent[cur]++;	/* round robin */

	// From sfio.c - see comment in sfio_in_Nway() - look for '20'
	// opts->sent[cur] += 20;
	// opts->sent[cur] += sent / (25<<10);

	if (rk) hash_free(rk);
	return (rc);
}
