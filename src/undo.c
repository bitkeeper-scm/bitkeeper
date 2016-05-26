/*
 * Copyright 2000-2016 BitMover, Inc
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
#include "nested.h"
#include "progress.h"

#define	BACKUP_SFIO "BitKeeper/tmp/undo_backup_sfio"
#define	UNDO_CSETS  "BitKeeper/tmp/undo_csets"
#define	SFILES	    "BitKeeper/tmp/sfiles"
#define SDMSG	    "stripdel: can't remove committed delta ChangeSet@"
#define SDMSGLEN    (sizeof(SDMSG) - 1)

typedef struct {
	u32	verbose:1;
	u32	quiet:1;
	u32	fromclone:1;
	u32	force_unpopulate:1;
	int	tick_cur;
	ticker	*tick;
	char	*patch;
	hash	*empty;		/* dirs that may need to be deleted */
} options;

private char	**getrev(char *rev, int aflg);
private int	clean_file(char **, options *opts);
private	int	moveAndSave(options *opts, char **fileList);
private int	move_file(options *opts, char ***checkfiles);
private int	renumber_rename(options *opts, char **sfiles);
private int	check_patch(char *patch);
private int	doit(options *opts, char **filesNrevs, char **sfiles,
    char ***);
private	int	undo_ensemble1(nested *n, options *ops,
		    char **nav, char ***comp_list);
private	int	undo_ensemble2(nested *n, options *ops);
private	void	undo_ensemble_rollback(nested *n, options *opts,
    char **comp_list);

int
undo_main(int ac,  char **av)
{
	int	c, rc, 	force = 0, save = 1;
	comp	*comp;
	char	buf[MAXLINE];
	char	undo_list[MAXPATH] = { 0 };
	FILE	*f;
	nested	*n = 0;
	project	*proj = 0;
	int	i, match = 0, lines = 0, limitwarning = 0, ncsetrevs = 0;
	int	status;
	int	rmresync = 1;
	int	standalone = 0;
	char	**sfiles = 0;		// sfiles to undo
	char	**filesNrevs = 0;	// sfile|key to undo
	char	**csetrevs = 0;		// revs as keys of csets to undo
	char	**comp_list = 0; /* list of comp*'s we have rolled back */
	char	*cmd = 0, *rev = 0;
	int	aflg = 0;
	char	**checkfiles = 0;	/* list of files to check */
	char	*p;
	char	*must_have = 0;
	char	**nav = 0;
	options	*opts;
	longopt	lopts[] = {
		{ "force-unpopulate", 310 },

		/* aliases */
		{ "standalone", 'S' },
		{ 0, 0 }
	};

	opts = new(options);
	opts->patch = "BitKeeper/tmp/undo.patch";
	opts->fromclone = 0;
	while ((c = getopt(ac, av, "a:Cfqp;r:Ssv", lopts)) != -1) {
		/* We make sure component undo's always save a patch */
		unless ((c == 'a') || (c == 'r') || (c == 's')) {
			nav = bk_saveArg(nav, av, c);
		}
		switch (c) {
		    case 'a': aflg = 1;				/* doc 2.0 */
			/* fall though */
		    case 'r': rev = optarg; break;		/* doc 2.0 */
		    case 'C': opts->fromclone = 1; break;
		    case 'f': force  =  1; break;		/* doc 2.0 */
		    case 'q': opts->quiet = 1; break;			/* doc 2.0 */
		    case 'p': opts->patch = optarg; break;
		    case 'S': standalone = 1; break;
		    case 's': save = 0; break;			/* doc 2.0 */
		    case 'v': opts->verbose = 1; break;
		    case 310: opts->force_unpopulate = 1; break;
		    default:
			freeLines(nav, free);
			bk_badArg(c, av);
		}
	}
	unless (rev) usage();
	if (proj_isProduct(0) && standalone) usage();
	bk_nested2root(standalone);

	if (proj_isEnsemble(0) && !getenv("_BK_TRANSACTION")) {
		if (nested_isGate(0)) {
			fprintf(stderr, "undo: not allowed in a gate\n");
			goto err;
		}
		if (proj_isProduct(0) && nested_isPortal(0)) {
			fprintf(stderr,
			    "undo: not allowed for product in a portal\n");
			goto err;
		}
	}
	opts->empty = hash_new(HASH_MEMHASH);
	trigger_setQuiet(opts->quiet);
	cmdlog_lock(standalone ? CMD_WRLOCK : (CMD_WRLOCK|CMD_NESTED_WRLOCK));
	if (undoLimit(0, &must_have)) limitwarning = 1;
	// XXX - be nice to do this only if we actually are going to undo
	unlink(BACKUP_SFIO); /* remove old backup file */
	/*
	 * Get a list of <file>|<key> entries, one per delta,
	 * so it may have multiple entries for the same file.
	 */
	unless (filesNrevs = getrev(rev, aflg)) {
		/* No revs we are done. */
		freeLines(nav, free);
		if (must_have) free(must_have);
		rc = opts->fromclone ? UNDO_SKIP : 0;
		goto out2;
	}
	EACH (filesNrevs) {
		p = strchr(filesNrevs[i], '|');
		assert(p);
		*p = 0;
		if (streq("SCCS/s.ChangeSet", filesNrevs[i])) {
			csetrevs = addLine(csetrevs, p+1);
			++ncsetrevs;
			if (must_have && streq(must_have, p+1)) {
				limitwarning = 1;
			}
		}
		sfiles = addLine(sfiles, strdup(filesNrevs[i]));
		*p = '|';
	}
	if (limitwarning) {
		fprintf(stderr,
		    "%s: Warning: undo is rolling back before the partition\n"
		    "tip, and may be missing some files that were moved or\n"
		    "deleted at the time of the partition, but present now.\n"
		    "The partition tip\n\t%s\n", prog, must_have);
		unless (getenv("_BK_UNDO_OK")) goto err;
	}
	if (must_have) {
		free(must_have);
		must_have = 0;
	}
	uniqLines(sfiles, free);

	/*
	 * Only now do we know how many iterations we will make, so
	 * finally start the progress bar:
	 *   bk cset: csetrevs (if save==1)
	 *   moveAndSave: sfiles (uniq of filesNrevs)
	 *   bk stripdel: filesNrevs
	 *   bk renumber: sfiles that exist
	 *   bk names: sfiles that exist
	 */
	unless (opts->quiet || opts->verbose) {
		opts->tick_cur = 0;
		opts->tick = progress_start(PROGRESS_BAR,
				(save ? ncsetrevs : 0) +
				(nLines(filesNrevs) + 3*nLines(sfiles)));
	}

	bktmp(undo_list);
	cmd = aprintf("bk -?BK_NO_REPO_LOCK=YES stripdel -%sc - 2> '%s'",
	    (proj_isComponent(0) && !getenv("_BK_TRANSACTION")) ? "" : "C",
	    undo_list);
	f = popen(cmd, "w");
	free(cmd);
	unless (f) {
err:		if (undo_list[0]) unlink(undo_list);
		unlink(UNDO_CSETS);
		if (comp_list) {
			undo_ensemble_rollback(n, opts, comp_list);
			freeLines(comp_list, 0);
		}
		if (n) nested_free(n);
		rc = UNDO_ERR;
		if ((size(BACKUP_SFIO) > 0) && restore_backup(BACKUP_SFIO,0)) {
			goto out;
		}
		unlink(BACKUP_SFIO);
		if (rmresync && exists("RESYNC")) rmtree("RESYNC");
		freeLines(nav, free);
		freeLines(filesNrevs, free);
		freeLines(sfiles, free);
		freeLines(csetrevs, 0);
		goto out;
	}
	EACH (csetrevs) fprintf(f, "ChangeSet|%s\n", csetrevs[i]);
	status = pclose(f);
	unless (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
		f = fopen(undo_list, "r");
		while (fnext(buf, f)) {
			if (strneq(buf, SDMSG, SDMSGLEN)) match = 1;
			lines++;
		}
		fclose(f);
		if ((lines == 1) && (match == 1)) {
			getMsg("undo_error2", bin, 0, stdout);
		} else {
			getMsg("undo_error", bin, 0, stdout);
		}
		cat(undo_list);
		goto err;
	}
	unless (force) {
		for (i = 0; i<79; ++i) putchar('-'); putchar('\n');
		fflush(stdout);
		// LMXXX - if changes ever locks this breaks
		f = popen(opts->verbose ?
		    "bk changes -Sav -" : "bk changes -Sa -", "w");
		EACH (csetrevs) fprintf(f, "%s\n", csetrevs[i]);
		pclose(f);
		printf("Remove these [y/n]? ");
		unless (fgets(buf, sizeof(buf), stdin)) buf[0] = 'n';
		if ((buf[0] != 'y') && (buf[0] != 'Y')) {
			unlink(undo_list);
			rc = UNDO_ERR;
			goto out;
		}
	}

	unless (opts->fromclone) {
		f = fopen(UNDO_CSETS, "w");
		EACH (csetrevs) fprintf(f, "%s\n", csetrevs[i]);
		fclose(f);
		putenv("BK_CSETLIST=" UNDO_CSETS);
		if (trigger("undo", "pre")) goto err;
	}
	if (proj_isProduct(0)) {
		unless (n = nested_init(0, 0, csetrevs, NESTED_MARKPENDING)) {
			fprintf(stderr, "undo: ensemble failed.\n");
			goto err;
		}
		if (n->cset) sccs_close(n->cset); /* win32 */
		unless (n->oldtip) {	/* tag only undo */
			if (opts->verbose) {
				puts("#### Undo tags in Product ####");
				fflush(stdout);
			}
			nested_free(n);
			n = 0;
			goto prod;
		}
		if (undo_ensemble1(n, opts, nav, &comp_list)) {
			goto err;
		}
	}
prod:
	if (save) {
		unless (isdir(BKTMP)) mkdirp(BKTMP);
		/* like bk makepatch but skips over missing files/keys */
		if (opts->quiet || opts->verbose) {
			cmd = aprintf("bk -?BK_NO_REPO_LOCK=YES "
			    "cset -Bfm - > '%s'", opts->patch);
		} else {
			cmd = aprintf("bk -?BK_NO_REPO_LOCK=YES "
			    "cset -Bfm -N%d - > '%s'",
			    ncsetrevs, opts->patch);
			progress_inherit(opts->tick);
			progress_nlneeded();
		}
		f = popen(cmd, "w");
		free(cmd);
		if (f) {
			EACH(csetrevs) fprintf(f, "%s\n", csetrevs[i]);
			pclose(f);
		}
		unless (opts->quiet || opts->verbose) {
			progress_inheritEnd(opts->tick, ncsetrevs);
			opts->tick_cur += ncsetrevs;
		}
		if (check_patch(opts->patch)) {
			printf("Failed to create undo backup %s\n",
			    opts->patch);
			goto err;
		}
	}
	/*
	 * clean_file - builds up a COdb in proj struct.
	 * We need to have a local copy of the proj struct so that
	 * chdir RESYNC won't free it.
	 */
	proj = proj_init(".");
	if (clean_file(sfiles, opts)) goto err;

	/*
	 * Move file to RESYNC and save a copy in a sfio backup file
	 */
	switch (moveAndSave(opts, sfiles)) {
	    case -2: rmresync = 0; goto err;
	    case -1: goto err;
	}

	chdir(ROOT2RESYNC);
	if (doit(opts, filesNrevs, sfiles, &checkfiles)) {
		chdir(RESYNC2ROOT);
		freeLines(checkfiles, free);
		goto err;
	}
	chdir(RESYNC2ROOT);

	rmEmptyDirs(opts->empty);
	hash_free(opts->empty);
	if (opts->verbose && save) {
		printf("Backup patch left in \"%s\".\n", opts->patch);
	}

	idcache_update(checkfiles);
	proj_restoreAllCO(proj, 0, 0, opts->fromclone);

	rmtree("RESYNC");

	EACH_STRUCT(comp_list, comp, i) {
		/* check comp cset files for poly cset unmarking */
		checkfiles = addLine(checkfiles, 
		    aprintf("%s/%s", comp->path, CHANGESET));
	}
	if (n) {
		if (undo_ensemble2(n, opts)) goto err;
		urlinfo_write(n); /* don't write urllist on failure */
		nested_free(n);
		n = 0;
	}
	rc = run_check(opts->quiet, opts->verbose, checkfiles,
	    opts->fromclone ? "-fuT" : "-fu", 0);
	freeLines(checkfiles, free);

	unlink(undo_list);
	freeLines(comp_list, 0);
	freeLines(nav, free);
	freeLines(filesNrevs, free);
	freeLines(sfiles, free);
	freeLines(csetrevs, 0);
	unless (opts->fromclone) unlink(UNDO_CSETS);
	unless (rc) {
		/* do not remove backup if check failed */
		unlink(BACKUP_SFIO);
		unlink(CSETS_IN);	/* no longer valid */
		unless (opts->fromclone) {
			putenv("BK_CSETLIST=");
			putenv("BK_STATUS=OK");
			trigger("undo", "post");
		}
	}
out2:	unless (opts->fromclone || opts->verbose || opts->quiet) {
		progress_end(PROGRESS_BAR, rc ? "FAILED" : "OK", PROGRESS_MSG);
	}
out:	free(opts);
	if (proj) proj_free(proj);
	return (rc);
}

/*
 * do the first 1/2 of the undo work for components.
 *
 * Check for as many errors as we can ahead of time.
 *
 * Then, for each component that won't be deleted complete call undo
 * and save the list of components in comp_list.
 *
 * Don't delete or rename components until part2
 */
private int
undo_ensemble1(nested *n, options *opts,
    char **nav, char ***comp_list)
{
	comp	*c;
	char	**vp;
	char	*p, *cmd;
	int	rc;
	int	i, j, errs, num = 0, which = 1;
	project	*proj;
	popts	ops = {0};

	/* make sure we can explain the aliases at the new cset */
	if (nested_aliases(n, n->oldtip, &n->here, 0, 0)) {
		fprintf(stderr, "%s: current aliases not valid after undo.\n",
		    prog);
		goto err;
	}
	assert(n->product->alias);

	/*
	 * If we are the product, then don't do the work here, call subprocesses
	 * to do it here and in each component.  We know that it is OK to do it
	 * here so unless the other guys are pending we should be cool.
	 *
	 * Meaning of comp fields in this context:
	 *  c->included		modifed in csets being removed
	 *  c->present		currently populated
	 *  c->alias		in HERE at final rev (should be populated)
	 *  c->new		created in csets being removed
	 *  c->pending		has pending csets
	 */
	errs = 0;
	EACH_STRUCT(n->comps, c, i) {
		/* handle changing aliases */
		if (c->included && C_PENDING(c)) {
			fprintf(stderr,
			    "%s: The component %s includes pending "
			    "csets that cannot be undone.\n",
			    prog, c->path);
			++errs;
		}
		/* foreach populate */
		if (c->alias && !C_PRESENT(c)) which++;

		/* count number of calls to undo needed */
		if (c->included && !c->new && C_PRESENT(c)) num++;

		unless (c->new && c->included && C_PRESENT(c)) continue;

		/* foreach repo to be deleted... */

		proj = proj_init(c->path);
		/* may fail */
		removeLine(n->here, proj_rootkey(proj), free);
		proj_free(proj);

		sysio(0, SFILES, 0, "bk", "gfiles", "-cxp", c->path, SYS);
		if (size(SFILES) > 0) {
			fprintf(stderr,
			    "Changed/extra files in '%s'\n", c->path);
			p = aprintf("/bin/cat < '%s' 1>&2", SFILES);
			system(p);
			free(p);
			++errs;
		}
		unlink(SFILES);

		/*
		 * We don't want nested_populate() to try and clean this
		 * component because we don't care if it is unique or not.
		 * Just set c->alias so nested_populate won't notice a
		 * problem.
		 */
		c->alias = 1;
	}
	if (errs) goto err;
	ops.quiet = opts->quiet;
	ops.verbose = opts->verbose;
	if (opts->force_unpopulate) ops.noURLprobe = 1;
	ops.comps = num;
	if (errs = nested_populate(n, &ops)) goto err;
	num = ops.comps;
	START_TRANSACTION();
	errs = 0;
	EACH_STRUCT(n->comps, c, j) {
		if (c->product || c->new) continue;
		unless (c->included && C_PRESENT(c)) continue;
		vp = addLine(0, strdup("bk"));
		unless (opts->quiet || opts->verbose) {
			vp = addLine(vp, aprintf("--title=%u/%u %s",
				which++, num, c->path));
		}
		vp = addLine(vp, strdup("undo"));
		EACH(nav) vp = addLine(vp, strdup(nav[i]));
		cmd = aprintf("-Sfa%s", c->lowerkey);
		vp = addLine(vp, cmd);
		vp = addLine(vp, 0);
		if (opts->verbose) {
			printf("#### Undo in %s (%d of %d) ####\n",
			    c->path, which++, num);
			fflush(stdout);
		}
		if (chdir(c->path)) {
			fprintf(stderr, "Could not chdir %s\n", c->path);
			exit(199);
		}
		rc = spawnvp(_P_WAIT, "bk", &vp[1]);
		if (WIFEXITED(rc)) rc = WEXITSTATUS(rc);
		if (rc && !(opts->fromclone && (rc == UNDO_SKIP))) {
			progress_nldone();
			fprintf(stderr, "Could not undo %s to %s.\n",
			    c->path, c->lowerkey);

			++errs;
		} else {
			*comp_list = addLine(*comp_list, c);
		}
		freeLines(vp, free);
		proj_cd2product();
		if (errs) break;
	}
	STOP_TRANSACTION();
	if (errs) return (1);
	if (opts->verbose) {
		printf("#### Undo in Product (%d of %d) ####\n",
		    which++, num);
		fflush(stdout);
	} else unless (opts->quiet) {
		title = aprintf("%u/%u %s", num, num, PRODUCT);
	}
	return (0);
err:	return (1);
}

/*
 * Now that undo has been successful in all components and the
 * product we can delete components that are no longer needed.
 * We also handle renames here.
 */
private int
undo_ensemble2(nested *n, options *opts)
{
	int	i;
	char	*cmd;
	comp	*c;
	FILE	*f;

	EACH_STRUCT(n->comps, c, i) {
		if (c->new && c->included && C_PRESENT(c)) {
			rmtree(c->path);
			c->_present = 0;
		}
	}

	/*
	 * We may have undone some renames so just call names on
	 * the set of repos and see what happens.
	 */
	cmd = aprintf("bk names %s -", (opts->verbose ? "" : "-q"));
	f = popen(cmd, "w");
	free(cmd);
	EACH_STRUCT(n->comps, c, i) {
		if (c->product) continue;
		unless (C_PRESENT(c)) continue; /* not here */
		unless (c->included) continue;	/* wasn't modified */

		fprintf(f, "%s/SCCS/s.ChangeSet\n", c->path);
	}
	pclose(f);

	nested_writeHere(n);
	return (0);
}

/*
 * We have previously called 'undo' in several components and then hit
 * a problem.  Now go back to those components and reapply the saved
 * backup patch.
 */
private void
undo_ensemble_rollback(nested *n, options *opts, char **comp_list)
{
	int	i, ncomps=0, rc;
	comp	*c;

	unless (opts->verbose) {
		fprintf(stderr, "Reverting components to original version\n");
	}
	EACH(comp_list) ++ncomps;

	START_TRANSACTION();
	EACH(comp_list) {
		char	*opt = "--progress";

		c = (comp *)comp_list[i];

		if (opts->verbose) {
			fprintf(stderr, "Reverting %s to original version\n",
				c->path);
			opt = "-v";
		}
		if (chdir(c->path)) {
			fprintf(stderr, "undo: unable to find %s to revert\n",
			    c->path);
			return;
		}
		unless (opts->verbose) progress_nlneeded();
		if (rc = systemf("bk -Lw -?FROM_PULLPUSH=YES "
		    "takepatch %s -af'%s'", opt, opts->patch)) {
			fprintf(stderr, "undo: restoring backup patch in %s "
			    "failed\n", c->path);
		}
		unless (opts->verbose) {
			title = aprintf("%u/%u %s", i, ncomps, c->path);
			progress_end(PROGRESS_BAR, rc ? "FAILED" : "OK",
				     PROGRESS_MSG);
		}
		proj_cd2product();
		if (rc) break;
	}
	STOP_TRANSACTION();
}

private int
doit(options *opts, char **filesNrevs, char **sfiles, char ***checkfiles)
{
	int	i, nfiles = 0, rc;
	FILE	*f;
	char	buf[MAXLINE];

	unlink("BitKeeper/tmp/run_names");

	if (opts->quiet) {
		sprintf(buf, "bk stripdel -q -C -");
	} else if (opts->verbose) {
		sprintf(buf, "bk stripdel -C -");
	} else if (opts->tick) {
		nfiles = nLines(filesNrevs);
		sprintf(buf, "bk stripdel -C -N%u -", nfiles);
		progress_inherit(opts->tick);
		progress_nlneeded();
	}
	f = popen(buf, "w");
	EACH(filesNrevs) fprintf(f, "%s\n", filesNrevs[i]);
	rc = pclose(f);
	if (opts->tick) {
		progress_inheritEnd(opts->tick, nfiles);
		opts->tick_cur += nfiles;
	}
	if (!f || (rc != 0)) {
		fprintf(stderr, "Undo failed\n");
		return (-1);
	}

	/*
	 * Make sure stripdel
	 * did not delete BitKeeper/etc when removing empty dir
	 */
	assert(exists(BKROOT));

	/*
	 * Handle any renames.  Done outside of stripdel because names only
	 * make sense at cset boundries.
	 * Also, run all sfiles through renumber.
	 */
	if (renumber_rename(opts, sfiles)) {
		return (-1);
	}
	/* mv from RESYNC to user tree */
	if (move_file(opts, checkfiles)) return (-1);
	return (0);
}

private int
check_patch(char *patch)
{
	MMAP	*m = mopen(patch, "");

	unless (m && m->size) return (1);
	unless (memmem(m->mmap, m->size, "\n\n# Patch checksum=", 19)) {
		mclose(m);
		return (1);
	}
	mclose(m);
	return (0);
}

private char **
getrev(char *top_rev, int aflg)
{
	char	*cmd, *rev;
	int	status;
	char	**list = 0;
	FILE	*f;
	char	*revline;

	if (aflg) {
		rev = aprintf("-r'%s..'", top_rev);
	} else if (IsFullPath(top_rev) && isreg(top_rev)) {
		rev = aprintf("- < '%s'", top_rev);
	} else {
		rev = aprintf("-r'%s'", top_rev);
	}
	/*
	 * XXX - this could do an annotate on the set of revs in question,
	 * read the results, do the idcache translation, and build the
	 * output itself but that's a lot of work to save some sfile inits.
	 */
	cmd = aprintf(
	    "bk changes -Safvnd':SFILE:|:KEY:' %s 2>" DEVNULL_WR, rev);
	free(rev);
	f = popen(cmd, "r");
	free(cmd);
	while (revline = fgetline(f)) {
		list = addLine(list, strdup(revline));
	}
	status = pclose(f);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		fprintf(stderr, "No such rev '%s' in ChangeSet\n", top_rev);
		exit(UNDO_ERR);
	}
	sortLines(list, 0);
	return (list);
}

private int
renumber_rename(options *opts, char **sfiles)
{
	FILE	*f;
	int 	i, rc = 0, status;
	u32	nfiles = 0, notexist = 0;
	char	*flist, *flag = "--";

	flist = bktmp(0);
	assert(flist);
	f = fopen(flist, "w");
	assert(f);
	EACH (sfiles) {
		if (exists(sfiles[i])) {
			fprintf(f, "%s\n", sfiles[i]);
			nfiles++;
		} else {
			notexist++;
		}
	}
	fclose(f);
	unless (nfiles) goto out;

	if (opts->quiet) {
		flag = "-q";
	} else if (opts->verbose) {
		flag = "-v";
	} else if (opts->tick) {
		flag = aprintf("-N%u", nfiles);
		progress_adjustMax(opts->tick, notexist);
		progress_inherit(opts->tick);
		progress_nlneeded();
	}
	status = sysio(flist, 0, 0, "bk", "renumber", flag, "-", SYS);
	rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
	if (opts->tick) {
		progress_inheritEnd(opts->tick, nfiles);
		opts->tick_cur += nfiles;
	}
	if ((rc == 0) && exists("BitKeeper/tmp/run_names")) {
		if (opts->tick) {
			progress_adjustMax(opts->tick, notexist);
			progress_inherit(opts->tick);
			progress_nlneeded();
		}
		status = sysio(flist, 0, 0, "bk", "names", flag, "-", SYS);
		rc = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
		if (opts->tick) {
			progress_inheritEnd(opts->tick, nfiles);
			opts->tick_cur += nfiles;
		}
	}

out:	unlink(flist);
	free(flist);
	return (rc);
}

private int
clean_file(char **sfiles, options *opts)
{
	sccs	*s;
	char	*name;
	int	i;

	/*
	 * First loop checks for diffs on any writable file,
	 * second loop actually cleans.  Done this way so that we don't
	 * clean partway and then exit, that will leave files not checked out.
	 */
	EACH(sfiles) {
		if (streq(sfiles[i], CHANGESET)) continue;
		s = sccs_init(sfiles[i], INIT_NOCKSUM);
		assert(s && HASGRAPH(s));
		if (sccs_hasDiffs(s, SILENT, 1)) {
			fprintf(stderr,
			    "Cannot clean %s, undo aborted\n", s->gfile);
			sccs_free(s);
			return (-1);
		}
		proj_saveCO(s);
		sccs_free(s);
	}
	EACH(sfiles) {
		name = sccs2name(sfiles[i]);
		if (!writable(name) && (unlink(name) == 0)) {
			free(name);
			continue;
		} /* else let clean try and sort it out */
		free(name);
		s = sccs_init(sfiles[i], INIT_NOCKSUM);
		assert(s && HASGRAPH(s));
		if (sccs_clean(s, SILENT)) {
			fprintf(stderr,
			    "Cannot clean %s, undo aborted\n", s->gfile);
			sccs_free(s);
			proj_restoreAllCO(0, 0, 0, opts->fromclone);
			return (-1);
		}
		sccs_free(s);
	}
	return (0);
}

/*
 * Move file to RESYNC and save a backup copy in sfio file
 * Return: 0 on success; -2 if RESYNC directory already exists; -1 on err
 */
private int
moveAndSave(options *opts, char **sfiles)
{
	FILE	*f;
	char	tmp[MAXPATH];
	int	i, rc = 0;

	if (isdir("RESYNC") && !nested_mine(0, getenv("_BK_NESTED_LOCK"), 1)){
		fprintf(stderr, "Repository locked by RESYNC directory\n");
		return (-2);
	}

	mkdir("RESYNC", 0777);
	sccs_mkroot("RESYNC");

	/*
	 * Run sfio under RESYNC, because we pick up the files _after_
	 * it is moved to RESYNC. This works better for win32; We usually
	 * detect access conflict when we moves file.
	 */
	chdir(ROOT2RESYNC);
	f = popen("bk sfio -omq > " "../" BACKUP_SFIO, "w");
	chdir(RESYNC2ROOT);
	unless (f) {
		perror("sfio");
		return (-1);
	}
	EACH (sfiles) {
		if (opts->tick) progress(opts->tick, ++opts->tick_cur);
		sprintf(tmp, "RESYNC/%s", sfiles[i]);
		if (streq(sfiles[i], CHANGESET)) {
			if (fileLink(sfiles[i], tmp)) {
				fprintf(stderr,
				    "Cannot link %s to %s\n", sfiles[i], tmp);
				rc = -1;
				break;
			}
		} else if (fileMove(sfiles[i], tmp)) {
			fprintf(stderr,
			    "Cannot mv %s to %s\n", sfiles[i], tmp);
			rc = -1;
			break;
		} else {
			/* Important: save a copy only if mv is successful */
			fprintf(f, "%s\n", sfiles[i]);

			/* remember dirs we emptied */
			strcpy(tmp, sfiles[i]);
			dirname(tmp);
			hash_insertStrSet(opts->empty, tmp);
		}
	}
	pclose(f);
	return (rc);
}


/*
 * Move file from RESYNC tree to the user tree
 */
private int
move_file(options *opts, char ***checkfiles)
{
	char	*from;
	FILE	*f;
	int	rc = 0;
	int	sync = proj_sync(0);
	int	i, fd;
	char	**files = 0;
	char	to[MAXPATH];

	/*
	 * Cannot trust fileList, because file may be renamed
	 *
	 * Paranoid, check for name conflict before we throw it over the wall
	 *
	 * needs to be sfiles not gfiles
	 */
	f = popen("bk sfiles", "r");
	while (from = fgetline(f)) {
		sprintf(to, "../%s", from);
		/*
		 * This should never happen if the repo is in a sane state
		 */
		if (exists(to) && !streq(from, CHANGESET)) {
			fprintf(stderr, "%s: name conflict\n", to);
			rc = -1;
			break;
		}
		files = addLine(files, strdup(from));
	}
	pclose(f);
	if (rc) return (rc); /* if error, abort */

	/*
	 * Throw the file "over the wall"
	 */
	EACH(files) {
		from = files[i];
		*checkfiles = addLine(*checkfiles, from);
		sprintf(to, "%s/%s", RESYNC2ROOT, from);
		if (fileMove(from, to)) {
			fprintf(stderr,
			    "Cannot move %s to %s\n", from, to);
			rc = -1;
			break;
		}
		if (sync) {
		    	fd = open(to, O_RDONLY, 0);
			fsync(fd);
			close(fd);
		}

		/* we know this directory is not empty */
		dirname(to);
		hash_deleteStr(opts->empty, to + strsz(RESYNC2ROOT));
	}
	freeLines(files, 0);
	if (exists("BitKeeper/log/TIP")) {
		fileMove("BitKeeper/log/TIP", RESYNC2ROOT "/BitKeeper/log/TIP");
	}
	return (rc);
}

int
undoLimit(sccs *cs, char **limit)
{
	sccs	*s;
	int	i, ret = 0, in_log = 0;
	char	*p;

	if (getenv("_BK_TRANSACTION")) return (0); /* let top level chk */
	unless ((s = cs) || (s = sccs_csetInit(INIT_MUSTEXIST))) return (0);
	EACH(s->text) {
		if (s->text[i][0] == '\001') continue;
		unless (in_log) {
			if (streq(s->text[i], "@ROOTLOG")) in_log = 1;
			continue;
		}
		if (s->text[i][0] == '@') break;
		unless (p = strchr(s->text[i], ':')) continue;

		if (strneq(s->text[i], "csetprune command", p - s->text[i])) {
			assert(p[1] == ' ');
			*limit = strdup(p+2);
			unless (sccs_findKey(s, p+2)) ret = 1;
			break;
		}
	}
	if (s != cs) sccs_free(s);
	return (ret);
}

void
rmEmptyDirs(hash *empty)
{
	char	*t;
	char	buf[MAXPATH];

	/*
	 * These directories have had something removed, and so
	 * _may_ be empty.  If rmdir() succeeds then it must have been.
	 * if so keep walking up to root.
	 *
	 * Removing BitKeeper/etc makes a repository not a repository, so
	 * we need to preserve that.
	 */
	EACH_HASH(empty) {
		strcpy(buf, empty->kptr);
		while (!streq(buf, "BitKeeper/etc") && !rmdir(buf)) {
			t = dirname(buf);
			if (streq(t, ".")) break;
		}
	}
}

