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

/*
 * resolve.c - multi pass resolver for renames, contents, etc.
 *
 * a) pull everything out of RESYNC into RENAMES which has pending name changes
 * b) build a DBM of rootkeys true/false of all the files in the RESYNC dir
 * c) process the files in RENAMES, resolving any conflicts, and put the file
 *    in RESYNC - if there isn't a file there already.  If there is, ask if 
 *    they want to move it and if so, move it to RENAMES.
 * d) before putting the file in RESYNC, if the same pathname is taken in the
 *    repository and the rootkey of that file is not in our list, then ask
 *    the user if they want to move the other file out of the way or move
 *    this file somewhere else.  If somewhere else is chosen, repeat process
 *    using that name.  If move is chosen, then copy the file to RENAMES and
 *    put this file in RESYNC.
 * e) repeat RENAMES processing until RENAMES dir is empty.  Potentially all
 *    files could pass through RENAMES.
 * f) move each file from RESYNC to repository, making sure that if the file
 *    exists in the repository, we have either overwritten it or deleted it.
 *
 * Invariants:
 * opts->rootDB{key} = <path>.  If the key is there then the inode is in the
 * 	RESYNC directory or the RENAMES directory under <path>.
 */
#include "resolve.h"
#include "progress.h"
#include "nested.h"

private	void	commit(opts *opts);
private	void	conflict(opts *opts, char *sfile);
private	int	create(resolve *rs);
private	void	edit_tip(resolve *rs, char *sf, ser_t d, int which);
private	void	freeStuff(opts *opts);
private	int	nameOK(opts *opts, sccs *s);
private	void	pass1_renames(opts *opts, sccs *s);
private	int	pass2_renames(opts *opts);
private	int	pass3_resolve(opts *opts);
private	int	pass4_apply(opts *opts);
private	int	passes(opts *opts);
private	int	pending(int checkComments);
private	int	pendingEdits(void);
private	int	pendingRenames(void);
private void	checkins(opts *opts, char *comment);
private	void	rename_delta(resolve *rs, char *sf, ser_t d, int w);
private	int	rename_file(resolve *rs);
private void	resolve_post(opts *o, int c);
private void	unapply(char **applied);
private int	writeCheck(sccs *s, MDBM *db);
private	void	listPendingRenames(void);
private	int	noDiffs(void);
private char	**find_files(opts *opts, int pfiles);
private	int	moveupComponent(void);
private	int	resolve_components(opts *opts);

private	jmp_buf	cleanup_jmp;
private MDBM	*localDB;	/* real name cache for local tree */
private MDBM	*resyncDB;	/* real name cache for resyn tree */
private int	nfiles;		/* # files in RESYNC */
private ticker	*tick;		/* progress-bar ticker state */
private u64	nticks = 0;	/* current progress-bar tick count */

int
resolve_main(int ac, char **av)
{
	char	**aliases = 0;
	int	c;
	opts	opts;
	longopt	lopts[] = {
		{ "auto-only", 305}, /* different than --batch */
		{ "batch", 310},
		{ "progress", 320 },

		/* aliases */
		{ "standalone", 'S'},
		{ "subset;", 's'},
		{ 0, 0 }
	};

	bzero(&opts, sizeof(opts));
	localDB = resyncDB = 0;
	nfiles = 0;
	tick = 0;
	nticks = 0;
	opts.pass1 = opts.pass2 = opts.pass3 = opts.pass4 = 1;
	setmode(0, _O_TEXT);
	while ((c = getopt(ac, av, "l|y|m;aAcdFi;qrSs|tTx;1234v", lopts)) != -1) {
		unless ((c == 's') || (c == 'S')) {
			opts.nav = bk_saveArg(opts.nav, av, c);
		}
		switch (c) {
		    case 'a': opts.automerge = 1; break;	/* doc 2.0 */
		    case 'A': opts.advance = 1; break;		/* doc 2.0 */
		    case 'c': opts.noconflicts = 1; break;	/* doc 2.0 */
		    case 'd': 					/* doc 2.0 */
			opts.debug = 1; putenv("BK_DEBUG_CMD=YES"); break;
		    case 'F': opts.force = 1; break;		/* undoc? 2.0 */
		    case 'l':					/* doc 2.0 */
			if (optarg) {
				opts.log = fopen(optarg, "a");
			} else {
				opts.log = stderr;
			}
			break;
		    case 'm': opts.mergeprog = optarg; break;	/* doc 2.0 */
		    case 'q': opts.quiet = 1; break;		/* doc 2.0 */
		    case 'r': opts.remerge = 1; break;		/* doc 2.0 */
		    case 's':	/* --subset */
			unless (optarg) {
				fprintf(stderr,
				    "bk %s -sALIAS: ALIAS "
				    "cannot be omitted\n", prog);
				return (1);
			}
			aliases = addLine(aliases, strdup(optarg));
			break;
		    case 'S': opts.standalone = 1; break;
		    case 'T': opts.textOnly = 1; break;		/* doc 2.0 */
		    case 'i':
			opts.partial = 1;
			opts.includes = addLine(opts.includes, strdup(optarg));
			break;
		    case 'x':
			opts.partial = 1;
			opts.excludes = addLine(opts.excludes, strdup(optarg));
			break;
		    case 'y': 					/* doc 2.0 */
			unless (opts.comment = optarg) opts.comment = "";
			break;
		    case '1': opts.pass1 = 0; break;		/* doc 2.0 */
		    case '2': opts.pass2 = 0; break;		/* doc 2.0 */
		    case '3': opts.pass3 = 0; break;		/* doc 2.0 */
		    case '4': opts.pass4 = 0; break;		/* doc 2.0 */
		    case 'v': opts.verbose = 1; break;
		    case 305: // --auto-only
			opts.autoOnly = 1;
			break;
		    case 310: // --batch
			opts.batch = opts.automerge = opts.autoOnly = 1;
			break;
		    case 320: // --progress
			opts.progress = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}
	trigger_setQuiet(opts.quiet);
	/*
	 * It is the responsibility of the calling code to set this env
	 * var to indicate that we were not run standalone, we are called
	 * from a higher level
	 */
	if (getenv("FROM_PULLPUSH") && streq(getenv("FROM_PULLPUSH"), "YES")) {
		opts.from_pullpush = 1;
	}
	if (opts.progress && !opts.verbose) {
		opts.quiet = 1;
	}
	T_PERF("resolve_main");
	unless (opts.mergeprog) opts.mergeprog = getenv("BK_RESOLVE_MERGEPROG");
	if ((av[optind] != 0) && isdir(av[optind])) chdir(av[optind++]);
	while (av[optind]) {
		opts.includes = addLine(opts.includes, strdup(av[optind++]));
		opts.partial = 1;
	}
	if (opts.partial) opts.pass4 = 0;

	unless (opts.textOnly) {
		char	*p = getenv("BK_GUI");

		/* use citool for commit if environment allows it */
		unless (p) putenv("BK_GUI=1");
		unless (gui_useDisplay()) opts.textOnly = 1; 
		unless (p) putenv("BK_GUI=");
	}

	/*
	 * Load the config file before we may delete it in the apply pass
	 * and then go looking for it (and not find it).
	 * XXX - If Wayne ever starts looking for the backing file before
	 * returning stuff from the DB, this hack will fail.
	 */
	proj_checkout(0);

	/* Globbing is a user interface, not one we want in RESYNC. */
	putenv("BK_NO_FILE_GLOB=TRUE");

	/*
	 * Make sure we are where we think we are.
	 */
	opts.nested = bk_nested2root(opts.standalone);
	unless (exists(ROOT2RESYNC)) {
		unless (opts.quiet) {
			fprintf(stderr, "resolve: "
			    "Nothing to resolve.\n");
		}
		freeStuff(&opts);
		return (0);
	}
	unless (writable(".")) {
		fprintf(stderr,
		    "resolve: repository root directory is not writable.\n");
		freeStuff(&opts);
		return (1);
	}
	if (proj_isCaseFoldingFS(0)) {
		localDB = mdbm_mem();
		resyncDB = mdbm_mem();
	}
	opts.complist = nested_complist(0, 0);
	putenv("_BK_MV_OK=1");

	if (c = setjmp(cleanup_jmp)) {
		c = (c >= 1000 ? c-1000 : 1);
		goto err;
	}

	c = passes(&opts);
	resolve_post(&opts, c);

err:	if (localDB) mdbm_close(localDB);
	if (resyncDB) mdbm_close(resyncDB);
	freeLines(opts.complist, free);
	freeLines(opts.nav, free);
	return (c);
}

private void
resolve_post(opts *opts, int c)
{
	/* XXX - there can be other reasons */
	if (c) {
		putenv("BK_STATUS=CONFLICTS");
	} else {
		putenv("BK_STATUS=OK");
	}
	trigger(getenv("_BK_IN_BKD") ? "remote push" : "pull", "post");
}

private void
listPendingRenames(void)
{
	int fd1;

	fprintf(stderr, "List of name conflicts:\n");
	fflush(stderr);
	fd1 = dup(1); dup2(2, 1); /* redirect stdout to stderr */
	system("bk _find . -name 'm.*' | xargs cat");
	dup2(fd1, 1); close(fd1);
}

/*
 * Do the setup and then work through the passes.
 */
private	int
passes(opts *opts)
{
	sccs	*s = 0;
	FILE	*p = 0;
	char	*t;
	int	rc;
	char	buf[MAXPATH];
	char	path[MAXPATH];
	char	flist[MAXPATH];

	/* Make sure only one user is in here at a time. */
	if (sccs_lockfile(RESOLVE_LOCK, 0, 0)) {
		pid_t	pid;
		char	*host = "UNKNOWN";
		time_t	time;
		sccs_readlockf(RESOLVE_LOCK, &pid, &host, &time);
		fprintf(stderr, "Could not get lock on RESYNC directory.\n");
		fprintf(stderr, "lockfile: %s\n", RESOLVE_LOCK);
		fprintf(stderr, "Lock held by pid %d on host %s since %s\n",
		    pid, host, ctime(&time));
		return (1);
	}

	if (exists("SCCS/s.ChangeSet")) {
		unless (opts->idDB = loadDB(IDCACHE, 0, DB_IDCACHE)) {
			fprintf(stderr, "resolve: can't open %s\n", IDCACHE);
			resolve_cleanup(opts, 0);
		}
	} else {
		/* fake one for new repositories */
		opts->idDB = mdbm_mem();
	}
	chdir(ROOT2RESYNC);
	unless (opts->from_pullpush || opts->quiet) {
		fprintf(stderr,
		    "Verifying consistency of the RESYNC tree...\n");
	}
	/*
	 * resolve_components() will either run the resolve for the
	 * components or check that they are resolved before allowing
	 * a product resolve to proceed.
	 */
	if (opts->nested && resolve_components(opts)) {
		fprintf(stderr, "%s: Unresolved components.\n", prog);
		resolve_cleanup(opts, 0);
	}
	T_PERF("before check");
	unless (opts->from_pullpush ||
	    (sys("bk", "-?BK_NO_REPO_LOCK=YES",
		"-r", "check", "-acR", SYS) == 0)) {
		syserr("failed.  Resolve not even started.\n");
		/* XXX - better help message */
		resolve_cleanup(opts, 0);
	}

	/*
	 * Pass 1 - move files to RENAMES and/or build up rootDB
	 */
	T_PERF("pass1");
	opts->pass = 1;
	bktmp(flist);
	/* needs to be sfiles not gfiles */
	if (sysio(0, flist, 0, "bk", "sfiles", SYS)) {
		perror("sfiles list");
err:		if (p) fclose(p);
		if (flist[0]) unlink(flist);
		return (1);
	}
	unless (p = fopen(flist, "r")) {
		perror(flist);
		goto err;
	}
	unless (opts->rootDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
		perror("mdbm_open");
		goto err;
	}
	if (opts->log) {
		fprintf(opts->log,
		    "==== Pass 1%s ====\n",
		    opts->pass1 ? "" : ": build DB for later passes");
	}
	if (opts->progress) {
		/*
		 * Resolve loops seven times (over sfiles, renames,
		 * conflicts, etc) and we tick the progress bar during
		 * each.  We don't know yet the # iterations of each
		 * loop, but we do know that each is bounded by the #
		 * sfiles.  So assume the max here, then scale each
		 * tick accordingly once we know the real loop counts
		 * (which we always know right before each loop runs).
		 */
		for (nfiles = 0; fnext(buf, p); ++nfiles) ;
		rewind(p);
		progress_delayStderr();
		tick = progress_start(PROGRESS_BAR, 8*nfiles);
	}
	while (fnext(buf, p)) {
		if (opts->progress) progress(tick, ++nticks);
		chop(buf);
		unless (s = sccs_init(buf, INIT_NOCKSUM)) continue;

		/* save the key for ALL sfiles, RESYNC and RENAMES alike */
		sccs_sdelta(s, sccs_ino(s), path);
		saveKey(opts, path, s->sfile);

		/* if files are already resolved, remember that */
		if (!CSET(s) && xfile_exists(s->gfile, 'd')) opts->resolved++;

		/* If we aren't doing pass 1, just remember the key */
		unless (opts->pass1) {
			sccs_free(s);
			continue;
		}

		/* skip stuff we've already moved */
		if (strneq("BitKeeper/RENAMES/SCCS/s.", buf, 25)) {
			sccs_free(s);
			continue;
		}

		unless (CSET(s)) sccs_close(s);
		pass1_renames(opts, s);
	}
	fclose(p);
	p = 0;
	unlink(flist);
	flist[0] = 0;
	if (opts->pass1 && !opts->quiet && opts->renames) {
		fprintf(stdlog,
		    "resolve: found %d renames in pass 1\n", opts->renames);
	}

	/*
	 * If there are edited files, bail.
	 * This shouldn't happen, takepatch looks for them and 
	 * the repository is locked, but just in case...
	 */
	if (opts->edited) {
		SHOUT();
		fprintf(stderr,
"There are modified files in the repository which are also in the patch.\n\
BitKeeper is aborting this patch until you check in those files.\n\
You need to check in the edited files and run takepatch on the file\n\
in the PENDING directory.  Alternatively, you can rerun pull or resync\n\
that will work too, it just gets another patch.\n");
		resolve_cleanup(opts, 0);
	}

	/*
	 * Pass 2 - move files back into RESYNC
	 */
	T_PERF("pass2");
	if (opts->pass2) {
		int	save, old, n = -1;

		if (opts->log) fprintf(opts->log, "==== Pass 2 ====\n");
		opts->pass = 2;

		/*
		 * Keep going as long as we are reducing the list.
		 * We can hit cases where there is a conflict that would
		 * go away if we processed the whole list.
		 */
		do {
			old = n;
			n = pass2_renames(opts);
		} while (n && ((old == -1) || (n < old)));

		unless (n) {
			unless (opts->quiet || !opts->renames2) {
				fprintf(stdlog,
				    "resolve: resolved %d renames in pass 2\n",
				    opts->renames2);
			}
			goto pass3;
		}

		if (opts->noconflicts) {
			SHOUT();
			fprintf(stderr,
			    "Did not resolve %d renames, "
			    "no conflicts causing abort.\n", n);
			/*
			 * Call it again to get the error messages.
			 * This means that all the code that is called
			 * below this has to respect noconflicts.
			 */
			opts->resolveNames = 1;
			pass2_renames(opts);
			resolve_cleanup(opts, CLEAN_ABORT|CLEAN_PENDING);
		}

		if (opts->automerge) {
			t = aprintf("| %d unresolved name conflict[s] |", n);
			opts->notmerged = addLine(opts->notmerged, t);
			opts->hadConflicts = n;
			goto pass3;
		}

		/*
		 * Now do the same thing, calling the resolver.
		 */
		if (opts->progress) {
			progress_done(tick, "OK");
			tick = 0;
			progress_restoreStderr();
		}
		opts->resolveNames = 1;
		save = opts->renames2;
		n = -1;
		do {
			old = n;
			n = pass2_renames(opts);
		} while (n && ((old == -1) || (n < old)));
		if (n && (opts->pass4 || opts->partial)) {
			fprintf(stderr,
			    "Did not resolve %d renames, abort\n", n);
			listPendingRenames();
			resolve_cleanup(opts, 0);
		}
		unless (opts->quiet) {
			fprintf(stdlog,
			    "resolve: resolved %d renames in pass 2\n",
			    opts->renames2);
		}
		opts->renamed = opts->renames2 - save;
	}

	/*
	 * Pass 3 - resolve content/permissions/flags conflicts.
	 */
pass3:	T_PERF("pass3");
	if (opts->pass3 && (rc = pass3_resolve(opts))) return (rc);

	/*
	 * Pass 4 - apply files to repository.
	 */
	T_PERF("pass4");
	if (opts->pass4) pass4_apply(opts);  /* never returns */

	freeStuff(opts);

	/* Unlock the RESYNC dir so someone else can get in */
	chdir(RESYNC2ROOT);
	sccs_unlockfile(RESOLVE_LOCK);

	/*
	 * Whoohooo...
	 */
	if (opts->progress) {
		progress_done(tick, "OK");
		progress_restoreStderr();
	}
	return (0);
}

/*
 * This function checks whether it's safe to resolve the product and
 * create a merge cset there. If any components are unresolved, it
 * returns non-zero. As a side effect, if opts->nested is true, bk
 * resolve will be run for any components in opts->aliases (or "HERE"
 * if opts->aliases is zero)
 */
private int
resolve_components(opts *opts)
{
	char	**unresolved = 0, **revs = 0;
	char	*compCset = 0, *resync = 0, *compCset2 = 0;
	char	*cwd;
	comp	*c;
	int	errors = 0, status = 0;
	int	i;
	sccs	*s, *cs;
	nested	*n;
	char	buf[MAXKEY];

	cwd = strdup(proj_cwd());
	proj_cd2product();
	s = sccs_init(ROOT2RESYNC "/" CHANGESET, INIT_MUSTEXIST|INIT_NOCKSUM);
	assert(s);
	revs = file2Lines(0, ROOT2RESYNC "/" CSETS_IN);
	n = nested_init(s, 0, revs, NESTED_PULL|NESTED_FIXIDCACHE);
	assert(n);
	unless (n->tip) goto out; /* tag only */
	sccs_close(s);
	freeLines(revs, free);
	unless (opts->aliases) opts->aliases = addLine(0, strdup("HERE"));

	nested_aliases(n, 0, &opts->aliases, start_cwd, NESTED_PENDING);
	assert(n->alias);
	opts->nav = unshiftLine(opts->nav, strdup("-S"));
	opts->nav = unshiftLine(opts->nav, strdup("resolve"));
	opts->nav = unshiftLine(opts->nav, strdup("--sigpipe"));
	opts->nav = unshiftLine(opts->nav, strdup("-?FROM_PULLPUSH="));
	opts->nav = unshiftLine(opts->nav, strdup("bk"));
	opts->nav = addLine(opts->nav, 0);

	EACH_STRUCT(n->comps, c, i) {
		proj_cd2product();
		if (c->alias && !C_PRESENT(c)) {
missing:		fprintf(stderr, "%s: component %s is missing!\n", prog,
			    c->path);
			++errors;
			break;
		}
		if (c->product || !c->included || !C_PRESENT(c)) continue;
		FREE(compCset);
		compCset = aprintf(ROOT2RESYNC "/%s/" CHANGESET, c->path);
		FREE(resync);
		resync = aprintf("%s/" ROOT2RESYNC, c->path);

		if (isdir(resync) && opts->nested && c->alias) {
			if (chdir(c->path)) goto missing;
			unless (opts->quiet) printf("### %s ###\n", c->path);
			status = spawnvp(_P_WAIT, "bk", opts->nav+1);
			if (WIFEXITED(status)) {
				T_DEBUG("Resolve failed");
				errors += !!WEXITSTATUS(status);
				if (WEXITSTATUS(status) == 2) break;
			} else if (WIFSIGNALED(status) &&
			    (WTERMSIG(status) == SIGPIPE)) {
				T_DEBUG("Resolve aborted");
				++errors;
				break;
			} else {
				T_DEBUG("fail");
				++errors;
			}
			proj_cd2product();
		}
		/*
		 * Verify this component is okay.
		 */
		if (isdir(resync)) {
			/*
			 * Not resolved yet.
			 */
			unresolved = addLine(unresolved, c->path);
			++errors;
			continue;
		}

		FREE(compCset2);
		compCset2 = aprintf("%s/" CHANGESET, c->path);
		if (exists(compCset)) {
			unless (xfile_exists(compCset, 'd')) {
				/*
				 * A component's ChangeSet file is here
				 * with no dfile means that this
				 * component is no longer pending.
				 * The sameFiles() won't match
				 * because this copy has a cset mark.
				 */
				continue;
			}
			/*
			 * Already resolved with merge. Verify that
			 * the component's actual ChangeSet file
			 * matches what we have in the product's
			 * RESYNC directory
			 */
			T_NESTED("sameFiles(%s, %s)", compCset, compCset2);
			unless (sameFiles(compCset, compCset2)) {
				fprintf(stderr, "%s: component ChangeSet "
				    "mismatch for %s\n",
				    prog, c->path);
				++errors;
				break;
			}
			opts->comps++;
		} else {
			/*
			 * Check for product RESYNC and if none, assume
			 * that a full abort has been done, and just
			 * go home quietly. buf == full path to prod resync
			 */
			concat_path(buf,
			    proj_root(proj_product(0)), ROOT2RESYNC);
			unless (exists(buf)) {
				proj_cd2product();
				fprintf(stderr, "Aborting\n");
				/* exit resolve_main() */
				longjmp(cleanup_jmp, 1000+1);
				/* NOT REACHED */
			}
			/*
			 * No RESYNC and no ChangeSet file in the product's
			 * RESYNC?  Just verify that the key is what we
			 * expect.
			 */
			cs = sccs_init(compCset2, INIT_MUSTEXIST|INIT_NOCKSUM);
			assert(cs);
			sccs_sdelta(cs, sccs_top(cs), buf);
			sccs_free(cs);
			unless (streq(buf, C_DELTAKEY(c))) {
				fprintf(stderr, "KEY MISMATCH for %s:\nwanted: %s\n   got: %s\n",
				    c->path, C_DELTAKEY(c), buf);
				++errors;
				break;
			}
		}
	}
	if (unresolved) {
		fprintf(stderr, "%d unresolved component%s:\n",
		    nLines(unresolved),
		    (nLines(unresolved) > 1) ? "s": "");
		EACH(unresolved) fprintf(stderr, " %s\n", unresolved[i]);
	}
	FREE(compCset);
	FREE(compCset2);
	FREE(resync);
	freeLines(unresolved, 0);
out:	chdir(cwd);
	free(cwd);
	sccs_free(s);
	nested_free(n);
	return (errors);
}

/* ---------------------------- Pass1 stuff ---------------------------- */

/*
 * Save the key away so we know what we have.
 * This is called for each file in (or added to) the RESYNC dir.
 */
void
saveKey(opts *opts, char *key, char *file)
{
	if (opts->debug) fprintf(stderr, "saveKey(%s)->%s\n", key, file);
	if (mdbm_store_str(opts->rootDB, key, file, MDBM_INSERT)) {
		fprintf(stderr, "Duplicate key: %s\n", key);
		fprintf(stderr, "\twanted by %s\n", file);
		fprintf(stderr,
		    "\tused by %s\n", mdbm_fetch_str(opts->rootDB, key));
		resolve_cleanup(opts, 0);
	}
}

/*
 * If there is a file in the repository in the same location,
 * and it is the same file (root keys match),
 * then return true. 
 *
 * If there is no matching file in local (both path slot and key slot), then
 * return 0.
 */
private	int
nameOK(opts *opts, sccs *s)
{
	char	path[MAXPATH], realname[MAXPATH];
	char	buf[MAXPATH];
	sccs	*local = 0;
	int	ret, i;
	project	*proj;

	if (CSET(s)) return (1); /* ChangeSet won't have conflicts */

	/*
	 * Same path slot and key?
	 */
	sprintf(path, "%s/%s", RESYNC2ROOT, s->sfile);
	if (resyncDB) {
		getRealName(path, resyncDB, realname);
		assert(strcasecmp(path, realname) == 0);
	} else {
		strcpy(realname, path);
	}

	if (opts->complist) {
		// 3 == "../" at start
		char	*gfile = sccs2name(realname+3);

		if (i = comp_overlap(opts->complist, gfile)) {
			if (opts->debug) {
				fprintf(stderr,
				    "nameOK(%s) => comp conflict with %s\n",
				    s->gfile, opts->complist[i]);
			}
			free(gfile);
			return (0);
		}
		free(gfile);
	}

	proj = proj_init(RESYNC2ROOT);
	if (streq(path, realname) &&
	    (local = sccs_init(path, INIT_NOCKSUM|INIT_MUSTEXIST)) &&
	    HAS_SFILE(local) && (local->proj == proj)) {
		sccs_sdelta(local, sccs_ino(local), path);
		sccs_sdelta(s, sccs_ino(s), buf);
		if (EDITED(local) && streq(path, buf) &&
		    sccs_hasDiffs(local, SILENT, 1)) {
			fprintf(stderr, "Warning: "
			    "%s is modified, will not overwrite it.\n",
			    local->gfile);
			opts->edited = 1;
		}
		if (opts->debug) {
		    fprintf(stderr,
			"nameOK(%s) => paths match, keys %smatch\n",
			s->gfile, streq(path, buf) ? "" : "don't ");
		}
		sccs_free(local);
		proj_free(proj);
		return (streq(path, buf));
	}
	proj_free(proj);
	if (local) sccs_free(local);

	chdir(RESYNC2ROOT);
	sccs_sdelta(s, sccs_ino(s), buf);
	local = sccs_keyinit(0, buf, INIT_NOCKSUM, opts->idDB);
	if (local) {
		if (EDITED(local) && sccs_hasDiffs(local, SILENT, 1)) {
			fprintf(stderr,
			    "Warning: %s is modified, will not overwrite it.\n",
			    local->gfile);
			opts->edited = 1;
		}
		sccs_free(local);
		if (opts->debug) {
			fprintf(stderr,
			    "nameOK(%s) => keys match, paths don't match\n",
			    s->gfile);
		}
		ret = 0;
	} else if (exists(s->gfile)) {
		if (opts->debug) {
			fprintf(stderr,
			    "nameOK(%s) => no sfile, but has local gfile\n",
			    s->gfile);
		}
		ret = 0;
	} else {
		if (opts->debug) fprintf(stderr, "nameOK(%s) = 1\n", s->gfile);
		ret = 1;
	}
	chdir(ROOT2RESYNC);
	return (ret);
}

/*
 * Pass 1 - move to RENAMES
 */
private	void
pass1_renames(opts *opts, sccs *s)
{
	char	path[MAXPATH];
	static	int filenum;

	if (nameOK(opts, s)) {
		xfile_delete(s->gfile, 'm');
		sccs_free(s);
		return;
	}

	unless (isdir("BitKeeper/RENAMES/SCCS")) {
		mkdir("BitKeeper/RENAMES", 0777);
		mkdir("BitKeeper/RENAMES/SCCS", 0777);
	}
	do {
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++filenum);
	} while (exists(path));
	if (opts->debug) {
		fprintf(stderr, "%s -> %s\n", s->sfile, path);
	}
	if (sfile_move(s->proj, s->sfile, path)) {
		fprintf(stderr, "Unable to rename(%s, %s)\n", s->sfile, path);
		resolve_cleanup(opts, 0);
	} else if (opts->log) {
		fprintf(opts->log, "rename(%s, %s)\n", s->sfile, path);
	}
	sccs_free(s);
	opts->renames++;
}

/* ---------------------------- Pass2 stuff ---------------------------- */

/*
 * pass2_renames - for each file in RENAMES, try to move it in place.
 * Return the number of files found in RENAMES.
 * This is called in two passes - the first pass just moves nonconflicting
 * files into place.  The second pass, indicated by opts->resolveNames,
 * handles all conflicts.
 * This routine may be called multiple times in each pass, until it gets to
 * a pass where it does no work.
 */
private	int
pass2_renames(opts *opts)
{
	sccs	*s = 0;
	int	n = 0;
	int	i, conf;
	resolve	*rs;
	char	*t;
	char	**names;
	char	path[MAXPATH];

	if (opts->debug) fprintf(stderr, "pass2_renames\n");

	unless (exists("BitKeeper/RENAMES/SCCS")) return (0);

	/*
	 * This needs to be sorted or the regressions don't pass
	 * because some filesystems do not do FIFO ordering on directory
	 * files (think hashed directories.
	 */
	names = getdir("BitKeeper/RENAMES/SCCS");
	progress_adjustMax(tick, nfiles - nLines(names));
	EACH(names) {
		if (opts->progress) progress(tick, ++nticks);
		sprintf(path, "BitKeeper/RENAMES/SCCS/%s", names[i]);

		/* may have just been deleted it but be in readdir cache */
		unless (exists(path)) continue;

		t = strrchr(path, '/');
		unless (t[1] == 's') continue;

		unless ((s = sccs_init(path, INIT_NOCKSUM)) && HASGRAPH(s)) {
			if (s) sccs_free(s);
			fprintf(stderr, "Ignoring %s\n", path);
			continue;
		}
		if (opts->debug) {
			fprintf(stderr,
			    "pass2.%u %s %d done\n",
				opts->resolveNames, path, opts->renames2);
		}

		n++;
		rs = resolve_init(opts, s);

		/*
		 * This code is ripped off from the create path.
		 * We are looking for the case that a directory component is
		 * in use for a file name that we want to put there.
		 * If that's true, then we want to call the resolve_create()
		 * code and fall through if they said EAGAIN or just skip
		 * this one.
		 */
		if (rs->d) conf = slotTaken(rs, rs->dname, 0);
		if (rs->d && ((conf == DIR_CONFLICT) ||
			(conf == COMP_CONFLICT))) {
			/* If we are just looking for space, skip this one */
			unless (opts->resolveNames) goto out;
			if (opts->noconflicts) {
				fprintf(stderr,
				    "resolve: dir/file conflict for ``%s'',\n",
				    PATHNAME(rs->s, rs->d));
				goto out;
			}
			if (resolve_create(rs, conf) != EAGAIN) {
				goto out;
			}
		}

		/*
		 * If we have a "names", then we have a conflict or a 
		 * rename, so do that.  Otherwise, this must be a create.
		 */
		if (rs->gnames) {
			rename_file(rs);
		} else {
			assert(rs->d); /* no rs->gnames implies single tip */
			create(rs);
		}
out:		resolve_free(rs);
	}
	freeLines(names, free);
	return (n);
}

/*
 * Extract the names from either a
 * m.file: rename local_name gca_name remote_name
 * or an
 * r.file: merge deltas 1.491 1.489 1.489.1.21 lm 00/01/08 10:39:11
 */
names	*
res_getnames(sccs *sc, int type)
{
	names	*names = 0;
	char	*s, *t;
	char	buf[MAXPATH*3];

	unless (t = xfile_fetch(sc->gfile, type)) return (0);
	strcpy(buf, t);
	free(t);

	unless (names = calloc(1, sizeof(*names))) goto out;
	if (type == 'm') {
		unless (strneq("rename ", buf, 7)) {
			fprintf(stderr, "BAD m.file: %s", buf);
			goto out;
		}
		s = &buf[7];
	} else {
		assert(type == 'r');
		unless (strneq("merge deltas ", buf, 13)) {
			fprintf(stderr, "BAD r.file: %s", buf);
			goto out;
		}
		s = &buf[13];
	}

	t = s;
	if (type == 'm') { /* local_name gca_name remote_name */
		s = strchr(t, '|');
		*s++ = 0;
		unless (names->local = strdup(t)) goto out;
		t = s;
		s = strchr(t, '|');
		*s++ = 0;
		unless (names->gca = strdup(t)) goto out;
		chop(s);
		unless (names->remote = strdup(s)) goto out;
	} else { /* 1.491 1.489 1.489.1.21[ lm 00/01/08 10:39:11] */
		unless (s = strchr(s, ' ')) goto out;
		*s++ = 0;
		unless (names->local = strdup(t)) goto out;
		t = s;
		unless (s = strchr(s, ' ')) goto out;
		*s++ = 0;
		unless (names->gca = strdup(t)) goto out;
		t = s;
		if (s = strchr(t, ' ')) {
			*s = 0;
		} else {
			chop(t);
		}
		unless (names->remote = strdup(t)) goto out;
	}
	return (names);
out:	freenames(names, 0);
	return (0);
}

/*
 * We think it is a create, but it might be a rename because an earlier
 * invocation of resolve already renamed it and threw away the m.file.
 * In that case, treat it like a non-conflicting rename with local & gca
 * the same and the remote side whatever it is in the RESYNC dir.
 *
 * It's a create if we can't find the file using the key and there is no
 * other file in that path slot.
 *
 * The lower level routines may make space for us and ask us to move it
 * with EAGAIN.
 */
private	int
create(resolve *rs)
{
	char	buf[MAXKEY];
	opts	*opts = rs->opts;
	sccs	*local;
	int	ret = 0;
	int	how;
	static	char *cnames[] = {
			"Huh?",
			"an SCCS file",
			"a regular file without an SCCS file",
			"another SCCS file in the patch",
			"another file already in RESYNC",
			"an SCCS file that is marked gone",
			"another component",
		};

	if (opts->debug) {
		fprintf(stderr,
		    ">> create(%s) in pass %d.%d\n",
		    PATHNAME(rs->s, rs->d), opts->pass, opts->resolveNames);
	}

	/* 
	 * See if this name is taken in the repository.
	 */
again:	if (how = slotTaken(rs, rs->dname, 0)) {
		if (!opts->noconflicts &&
		    (how == GFILE_CONFLICT) && (ret = gc_sameFiles(rs))) {
			if (ret == EAGAIN) goto again;
			return (ret);
		}

		/* If we are just looking for space, skip this one */
		unless (opts->resolveNames) return (-1);

		if (opts->noconflicts) {
	    		fprintf(stderr,
			    "resolve: name conflict for ``%s'',\n",
			    PATHNAME(rs->s, rs->d));
	    		fprintf(stderr,
			    "\tpathname is used by %s.\n", cnames[how]);
			opts->errors = 1;
			return (-1);
		}
      		ret = resolve_create(rs, how);
		if (opts->debug) {
			fprintf(stderr, "resolve_create = %d %s\n",
			    ret, ret == EAGAIN ? "EAGAIN" : "");
		}
		if (ret == EAGAIN) goto again;
		return (ret);
	}
	
	/*
	 * Mebbe we resolved this rename already.
	 */
	sccs_sdelta(rs->s, sccs_ino(rs->s), buf);
	chdir(RESYNC2ROOT);
	local = sccs_keyinit(0, buf, INIT_NOCKSUM, opts->idDB);
	chdir(ROOT2RESYNC);
	if (local) {
		if (opts->debug) {
			fprintf(stderr, "%s already renamed to %s\n",
			    local->gfile, PATHNAME(rs->s, rs->d));
		}
		/* dummy up names which make it a remote move */
		if (rs->gnames) freenames(rs->gnames, 1);
		if (rs->snames) freenames(rs->snames, 1);
		rs->gnames	   = new(names);
		rs->gnames->local  = strdup(local->gfile);
		rs->gnames->gca    = strdup(local->gfile);
		rs->gnames->remote = strdup(PATHNAME(rs->s, rs->d));
		rs->snames	   = new(names);
		rs->snames->local  = name2sccs(rs->gnames->local);
		rs->snames->gca    = name2sccs(rs->gnames->gca);
		rs->snames->remote = name2sccs(rs->gnames->remote);
		sccs_free(local);
		assert(!xfile_exists(rs->s->gfile, 'm'));
		return (rename_file(rs));
	}

	/*
	 * OK, looking like a real create.
	 */
	sccs_close(rs->s);
	ret = sfile_move(rs->s->proj, rs->s->sfile, rs->dname);
	if (rs->opts->log) {
		fprintf(rs->opts->log, "rename(%s, %s) = %d\n",
		    rs->s->gfile, PATHNAME(rs->s, rs->d), ret);
	}
	if (opts->debug) {
		fprintf(stderr, "%s -> %s = %d\n",
		    rs->s->gfile, PATHNAME(rs->s, rs->d), ret);
	}
	opts->renames2++;
	return (ret);
}

void
freenames(names *names, int free_struct)
{
	unless (names) return;
	if (names->local) free(names->local);
	if (names->gca) free(names->gca);
	if (names->remote) free(names->remote);
	if (free_struct) free(names);
}

/*
 * Handle renames.
 * May be called twice on the same file, when called with resolveNames
 * set, then we have to resolve the file somehow.
 */
private	int
rename_file(resolve *rs)
{
	opts	*opts = rs->opts;
	char	*to = 0;
	int	how = 0;

again:
	if (opts->debug) {
		fprintf(stderr, ">> rename_file(%s - %s)\n",
		    rs->s->gfile, rs->d ? PATHNAME(rs->s, rs->d) : "<conf>");
	}

	/*
	 * We can handle local or remote moves in no conflict mode,
	 * but not both.
	 * Only error on this once, so hence the check on resolveNames.
	 */
	if (opts->noconflicts &&
	    (!streq(rs->gnames->local, rs->gnames->gca) &&
	    !streq(rs->gnames->gca, rs->gnames->remote))) {
		unless (opts->resolveNames) return (-1);	/* shhh */
	    	fprintf(stderr, "resolve: can't process rename conflict\n");
		fprintf(stderr,
		    "%s ->\n\tLOCAL:  %s\n\tGCA:    %s\n\tREMOTE: %s\n",
		    rs->s->gfile,
		    rs->gnames->local, rs->gnames->gca, rs->gnames->remote);
		opts->errors = 1;
		return (-1);
	}

	if (opts->debug) {
		fprintf(stderr,
		    "%s ->\n\tLOCAL:  %s\n\tGCA:    %s\n\tREMOTE: %s\n",
		    rs->s->gfile,
		    rs->gnames->local, rs->gnames->gca, rs->gnames->remote);
	}

	/*
	 * See if we can just slide the file into place.
	 * If remote moved, and local didn't, ok if !slotTaken.
	 * If local is moved and remote isn't, ok if RESYNC slot is open.
	 */
	if (rs->d) {
		to = rs->dname;
		if (how = slotTaken(rs, to, 0)) to = 0;
	}
	if (to) {
		sccs_close(rs->s); /* for win32 */
		if (sfile_move(rs->s->proj, rs->s->sfile, to)) return (-1);
		sccs_writeHere(rs->s , to);
		xfile_delete(to, 'm');
		if (rs->revs) {
			ser_t	d;
			char	*t;

			d = sccs_findrev(rs->s, rs->revs->local);
			assert(d);
			t = name2sccs(PATHNAME(rs->s, d));
			unless (streq(t, to)) {
				rename_delta(rs, to, d, LOCAL);
			}
			free(t);
			d = sccs_findrev(rs->s, rs->revs->remote);
			assert(d);
			t = name2sccs(PATHNAME(rs->s, d));
			unless (streq(t, to)) {
				rename_delta(rs, to, d, REMOTE);
			}
			free(t);
		}
		if (rs->opts->log) {
			fprintf(rs->opts->log,
			    "rename(%s, %s)\n", rs->s->sfile, to);
		}
		opts->renames2++;
	    	return (0);
	}

	/*
	 * If we have a name conflict, we know we can't do anything at first.
	 */
	unless (opts->resolveNames) return (-1);

	/*
	 * Figure out why we have a conflict, if it is like a create
	 * conflict, try that.
	 */
	if (how) {
		int	ret;

		if ((ret = resolve_create(rs, how)) == EAGAIN) {
			how = 0;
			goto again;
		}
		return (ret);
	} else {
		return (resolve_renames(rs));
	}
}

/*
 * Move a file into the RESYNC dir in a new location and delta it such that
 * it has recorded the new location in the s.file.
 * If there is an associated r.file, update that to have the new tip[s].
 */
int
move_remote(resolve *rs, char *sfile)
{
	int	ret;
	char	*gfile;
	ser_t	d;
	MDBM	*idDB;

	if (rs->opts->debug) {
		fprintf(stderr, "move_remote(%s, %s)\n", rs->s->sfile, sfile);
	}

	sccs_close(rs->s);
	if (ret = sfile_move(rs->s->proj, rs->s->sfile, sfile)) return (ret);
	sccs_writeHere(rs->s, sfile);
	xfile_delete(sfile, 'm');
	if (rs->opts->resolveNames) rs->opts->renames2++;

	/*
	 * If we have revs, then there is an r.file, so move it too.
	 * And delta the tips if they need it.
	 */
	if (rs->revs) {
		char	*t;

		d = sccs_findrev(rs->s, rs->revs->local);
		assert(d);
		t = name2sccs(PATHNAME(rs->s, d));
		unless (streq(t, sfile)) {
			rename_delta(rs, sfile, d, LOCAL);
		}
		free(t);
		d = sccs_findrev(rs->s, rs->revs->remote);
		assert(d);
		t = name2sccs(PATHNAME(rs->s, d));
		unless (streq(t, sfile)) {
			rename_delta(rs, sfile, d, REMOTE);
		}
		free(t);
	} else unless (streq(rs->dname, sfile)) {
		rename_delta(rs, sfile, rs->d, 0);
	}

	/* idcache -- so post-commit check can find if a path conflict */
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	gfile = sccs2name(sfile);
	if (idcache_item(idDB, rs->key, gfile)) idcache_write(0, idDB);
	free(gfile);
	mdbm_close(idDB);
	/* Nota bene: *s is may be out of date */
	return (0);
}

/*
 * Add a null rename delta to the specified tip.
 */
private	void
rename_delta(resolve *rs, char *sfile, ser_t d, int which)
{
	char	*t;
	char	buf[MAXPATH+100];

	if (rs->opts->debug) {
		fprintf(stderr, "rename_delta(%s, %s, %s, %s)\n", sfile,
		    REV(rs->s, d), PATHNAME(rs->s, d),
		    which == LOCAL ? "local" : "remote");
	}
	edit_tip(rs, sfile, d, which);
	t = sccs2name(sfile);
	sprintf(buf, "-PyMerge rename: %s -> %s", PATHNAME(rs->s, d), t);
	free(t);

	/*
	 * We are holding this open in the calling process, have to close it
	 * for winblows.
	 * XXX - why not do an internal delta?
	 */
	win32_close(rs->s);
	if (rs->opts->log) {
		sys("bk", "-?_BK_MV_OK=1", "delta", "-f", buf, sfile, SYS);
	} else {
		sys("bk", "-?_BK_MV_OK=1", "delta", "-qf", buf, sfile, SYS);
	}
}

/*
 * Resolve a file type by taking the file the winning delta and checking it
 * over the losing delta.
 * winner == REMOTE means the local is delta-ed with the remote's data.
 */
void
type_delta(resolve *rs,
	char *sfile, ser_t l, ser_t r, int winner)
{
	char	buf[MAXPATH+100];
	char	*g = sccs2name(sfile);
	ser_t	o, n;		/* old -> new */
	sccs	*s;
	int	loser;

	if (rs->opts->debug) {
		fprintf(stderr, "type(%s, %s, %s, %s)\n",
		    sfile, REV(rs->s, l), REV(rs->s, r),
		    winner == LOCAL ? "local" : "remote");
	}

	/*
	 * XXX - there is some question here as to should I do a get -i.
	 * This is not clear.
	 */
	if (winner == LOCAL) {
		loser = -REMOTE;
		o = r;
		n = l;
	} else {
		loser = -LOCAL;
		o = l;
		n = r;
	}
	edit_tip(rs, sfile, o, loser);
	if (S_ISREG(MODE(rs->s, n))) {
		/* bk _get -kpqr{n->rev} sfile > g */
		sprintf(buf, "-kpqr%s", REV(rs->s, n));
		if (sysio(0, g, 0, "bk", "_get", buf, sfile, SYS)) {
			fprintf(stderr, "%s failed\n", buf);
			resolve_cleanup(rs->opts, 0);
		}
		chmod(g, MODE(rs->s, n));
	} else if (S_ISLNK(MODE(rs->s, n))) {
		assert(HAS_SYMLINK(rs->s, n));
		if (symlink(SYMLINK(rs->s, n), g)) {
			perror(g);
			resolve_cleanup(rs->opts, 0);
		}
	} else {
		fprintf(stderr,
		    "type_delta called on unknown file type %o\n", MODE(rs->s, n));
		resolve_cleanup(rs->opts, 0);
	}
	free(g);
	/* bk delta -qPy'Merge file types: {MODE(rs->s, o)} {MODE(rs->s, n)} sfile */
	sprintf(buf, "-qPyMerge file types: %s -> %s", 
	    mode2FileType(MODE(rs->s, o)), mode2FileType(MODE(rs->s, n)));
	if (sys("bk", "delta", "-f", buf, sfile, SYS)) {
		syserr("failed\n");
		resolve_cleanup(rs->opts, 0);
	}
	strcpy(buf, sfile);	/* it's from the sccs we are about to free */
	sfile = buf;
	sccs_free(rs->s);
	s = sccs_init(sfile, INIT_NOCKSUM);
	assert(s);
	if (rs->opts->debug) {
		fprintf(stderr, "type reopens %s (%s)\n", sfile, s->gfile);
	}
	rs->s = s;
	rs->d = sccs_top(s);
	assert(rs->d);
	free(rs->dname);
	rs->dname = name2sccs(PATHNAME(rs->s, rs->d));
}

/*
 * Add a null permissions delta to the specified tip.
 */
void
mode_delta(resolve *rs, char *sfile, ser_t d, mode_t m, int which)
{
	char	*a = mode2a(m);
	char	buf[MAXPATH], opt[100];
	sccs	*s;

	if (rs->opts->debug) {
		fprintf(stderr, "mode(%s, %s, %s, %s)\n",
		    sfile, REV(rs->s, d), a,
		    which == LOCAL ? "local" : "remote");
	}
	edit_tip(rs, sfile, d, which);
	/* bk delta -[q]Py'Change mode to {a}' -M{a} sfile */
	sprintf(buf, "-%sPyChange mode to %s", rs->opts->log ? "" : "q", a);
	sprintf(opt, "-M%s", a);
	if (sys("bk", "delta", "-f", buf, opt, sfile, SYS)) {
		syserr("failed\n");
		resolve_cleanup(rs->opts, 0);
	}
	strcpy(buf, sfile);	/* it's from the sccs we are about to free */
	sfile = buf;
	sccs_free(rs->s);
	s = sccs_init(sfile, INIT_NOCKSUM);
	assert(s);
	if (rs->opts->debug) {
		fprintf(stderr, "mode reopens %s (%s)\n", sfile, s->gfile);
	}
	rs->s = s;
	rs->d = sccs_top(s);
	assert(rs->d);
	free(rs->dname);
	rs->dname = name2sccs(PATHNAME(rs->s, rs->d));
}

/*
 * Add a null flags delta to the specified tip.
 */
void
flags_delta(resolve *rs,
	char *sfile, ser_t d, int flags, int which)
{
	char	*av[40];
	char	buf[MAXPATH];
	char	fbuf[10][25];	/* Must match list below */
	int	n, i, f;
	sccs	*s;
	int	bits = flags & X_MAYCHANGE;

	if (rs->opts->debug) {
		fprintf(stderr, "flags(%s, %s, 0x%x, %s)\n",
		    sfile, REV(rs->s, d), bits,
		    which == LOCAL ? "local" : "remote");
	}
	edit_tip(rs, sfile, d, which);
	sys("bk", "clean", sfile, SYS);
	av[n=0] = "bk";
	av[++n] = "admin";
	sprintf(buf, "-qr%s", REV(rs->s, d));
	av[++n] = buf;

#define	add(s)		{ sprintf(fbuf[f], "-f%s", s); av[++n] = fbuf[f]; }
#define	del(s)		{ sprintf(fbuf[f], "-F%s", s); av[++n] = fbuf[f]; }
#define	doit(bit,s)	if (bits&bit) add(s) else del(s); f++ 
	f = 0;
	doit(X_RCS, "RCS");
	doit(X_YEAR4, "YEAR4");
#ifdef X_SHELL
	doit(X_SHELL, "SHELL");
#endif
	doit(X_EXPAND1, "EXPAND1");
	doit(X_SCCS, "SCCS");
	doit(X_EOLN_NATIVE, "EOLN_NATIVE");
	doit(X_EOLN_WINDOWS, "EOLN_WINDOWS");
	doit(X_NOMERGE, "NOMERGE");
	doit(X_MONOTONIC, "MONOTONIC");
	for (i = 0; i < f; ++i) av[++n] = fbuf[i];
	av[++n] = sfile;
	assert(n < 38);	/* matches 40 in declaration */
	av[++n] = 0;

	n = spawnvp(_P_WAIT, av[0], av);
	if (!WIFEXITED(n) || WEXITSTATUS(n)) {
		for (i = 0; av[i]; ++i) fprintf(stderr, "%s ", av[i]);
		fprintf(stderr, "failed\n");
		resolve_cleanup(rs->opts, 0);
	}
	strcpy(buf, sfile);	/* it's from the sccs we are about to free */
	sfile = buf;
	sccs_free(rs->s);
	s = sccs_init(sfile, INIT_NOCKSUM);
	assert(s);
	if (rs->opts->debug) {
		fprintf(stderr, "flags reopens %s (%s)\n", sfile, s->gfile);
	}
	rs->s = s;
	rs->d = sccs_top(s);
	assert(rs->d);
	free(rs->dname);
	rs->dname = name2sccs(PATHNAME(rs->s, rs->d));
}

/*
 * Set up to add a delta to the file, such as a rename or mode delta.
 * If which is set, update the r.file with the new data.
 *	which == LOCAL means do local rev, REMOTE means do remote rev.
 * If the which value is negative, it means don't get the data.
 */
private	void
edit_tip(resolve *rs, char *sfile, ser_t d, int which)
{
	char	buf[MAXPATH+100];
	char	opt[100];
	char	*t;
	pfile	pf;

	if (rs->opts->debug) {
		fprintf(stderr, "edit_tip(%s %s %s)\n",
		    sfile, REV(rs->s, d),
		    abs(which) == LOCAL ? "local" : "remote");
	}
	/* bk _get -e[g][q] -r{d->rev} sfile */
	sprintf(buf, "-e%s%s", which < 0 ? "g" : "", rs->opts->log ? "" : "q");
	sprintf(opt, "-r%s", REV(rs->s, d));
	if (sys("bk", "_get", buf, opt, sfile, SYS)) {
		syserr("failed\n");
		resolve_cleanup(rs->opts, 0);
	}
	if (which) {
		sccs_restart(rs->s);
		if (sccs_read_pfile(rs->s, &pf)) {
			perror(rs->s->sfile);
			resolve_cleanup(rs->opts, 0);
		}

		if (abs(which) == LOCAL) {
			free(rs->revs->local);
			rs->revs->local = strdup(pf.newrev);
		} else {
			free(rs->revs->remote);
			rs->revs->remote = strdup(pf.newrev);
		}
		free_pfile(&pf);
		t = aprintf("merge deltas %s %s %s\n",
		    rs->revs->local, rs->revs->gca, rs->revs->remote);
		if (xfile_store(sfile, 'r', t)) assert(0);
		free(t);
	}
}

/*
 * Walk a directory tree and determine if it is a directory conflict or
 * it can be removed because the only contents are sfiles that will be
 * renamed as part of the incoming patch.
 */
private int
findDirConflict(char *file, char type, void *token)
{
	opts	*opts = token;
	char	*t;
	char	*sfile;
	char	buf[MAXKEY];

	if (type == 'd') return (0);
	if (t = strstr(file, "/SCCS/")) {
		if (strneq(t+6, "s.", 2)) {
			sccs	*local = sccs_init(file, INIT_NOCKSUM);

			sccs_sdelta(local, sccs_ino(local), buf);
			sccs_free(local);
			unless (mdbm_fetch_str(opts->rootDB, buf)) {
				if (opts->debug) {
					fprintf(stderr,
					    "%s new sfile in local repo\n",
					    file);
				}
				return (DIR_CONFLICT);
			}
		}
		/* ignore other files in SCCS */
		return (0);
	}
	/*
	 * Ignore gfiles with sfiles, because we'll pick them up above
	 */
	sfile = name2sccs(file);
	if (exists(sfile)) {
		free(sfile);
		return (0);
	}
	free(sfile);
	return (DIR_CONFLICT);
}

private int
pathConflict(opts *opts, char *gfile)
{
	char	*t, *s;
	
	for (t = strrchr(gfile, '/'); t; ) {
		*t = 0;
		if (exists(gfile) && !isdir(gfile)) {
			char	*sfile = name2sccs(gfile);

			/*
			 * If the gfile has a matching sfile and that file
			 * doesn't exist in the RESYNC dir, then it is not
			 * a conflict, the file has been successfully moved.
			 */
			unless (exists(sfile) && 
			    !mdbm_fetch_str(opts->rootDB, sfile)) {
				if (opts->debug) {
					fprintf(stderr,
					    "%s exists in local repository\n", 
					    gfile);
				}
				*t = '/';
				free(sfile);
				return (1);
			}
			free(sfile);
		}
		s = t;
		t = strrchr(t, '/');
		*s = '/';
	}
	return (0);
}

private	int
deleteDirs(char *dir, void *data)
{
	if (rmdir(dir) == 0) {
		fprintf(stderr,
		    "resolve: removing empty directory: %s\n", dir);
	}
	return (0);
}


/*
 * Return 1 if the pathname in question is in use in the repository by
 * an SCCS file that is not already in the RESYNC/RENAMES dirs.
 * Return 2 if the pathname in question is in use in the repository by
 * a file without an SCCS file.
 * Return 3 if the pathname in question is in use in the RESYNC dir.
 */
int
slotTaken(resolve *rs, char *slot, char **why)
{
	opts	*opts = rs->opts;
	int	i;

	assert(slot);
	if (opts->debug) fprintf(stderr, "slotTaken(%s) = ", slot);

	if (exists(slot)) {
		if (opts->debug) fprintf(stderr, "%s exists in RESYNC\n", slot);
		return (RESYNC_CONFLICT);
	}

	/* load gone from RESYNC */
	unless (opts->goneDB) {
		opts->goneDB = loadDB(GONE, 0, DB_GONE);
	}

	chdir(RESYNC2ROOT);
	if (exists(slot)) {
		char	buf2[MAXKEY];
		sccs	*local = sccs_init(slot, INIT_NOCKSUM);

		/*
		 * If we can find this key in the RESYNC dir then it is
		 * not a conflict, the file has been successfully moved.
		 */
		sccs_sdelta(local, sccs_ino(local), buf2);
		sccs_free(local);
		if (mdbm_fetch_str(opts->rootDB, buf2)) {
			/* in resync */
		} else if (mdbm_fetch_str(opts->goneDB, buf2)) {
			/* gone file */
			if (opts->debug) {
				fprintf(stderr,
				    "%s exists and is gone\n", slot);
			}
			chdir(ROOT2RESYNC);
			return (GONE_SFILE_CONFLICT);
		} else {
			if (opts->debug) {
				fprintf(stderr,
				    "%s exists in local repository\n", slot);
			}
			chdir(ROOT2RESYNC);
			return (SFILE_CONFLICT);
		}
	} else {
		char	*gfile = sccs2name(slot);

		if (i = comp_overlap(opts->complist, gfile)) {
			if (opts->debug) {
				fprintf(stderr,
				    "%s is a new component\n",
				    opts->complist[i]);
			}
			if (why) *why = opts->complist[i];
			free(gfile);
			chdir(ROOT2RESYNC);
			return (COMP_CONFLICT);
		}
		if (exists(gfile)) {
			int	conf;

			if (isdir(gfile)) {
				conf = walkdir(gfile,
				    (walkfns){.file = findDirConflict,
					      .tail = deleteDirs}, opts);
			} else {
				conf = GFILE_CONFLICT;
			}
			if (conf && opts->debug) {
				fprintf(stderr,
				    "%s %s exists in local repository\n",
				    conf == DIR_CONFLICT ? "dir" : "file",
				    gfile);
			}
			free(gfile);
			chdir(ROOT2RESYNC);
			return (conf);
		} else if (pathConflict(opts, gfile)) {
			if (opts->debug) {
			    	fprintf(stderr,
				    "directory %s exists in local repository\n",
				    gfile);
			}
			free(gfile);
			chdir(ROOT2RESYNC);
			return (DIR_CONFLICT);
		}
		free(gfile);
	}
	chdir(ROOT2RESYNC);
	if (opts->debug) fprintf(stderr, "0\n");
	return (0);
}

int
comp_overlap(char **complist, char *path)
{
	int	i;

	EACH(complist) {
		if (paths_overlap(complist[i], path)) return (i);
	}
	return (0);
}

/* ---------------------------- Pass3 stuff ---------------------------- */

private int
noDiffs(void)
{
	FILE	*p = popen("bk -r diffs", "r");
	int	none;

	unless (p) return (0);
	none = (fgetc(p) == EOF);
	pclose(p);
	return (none);
}

/*
 * Handle all the non-rename conflicts.
 * Handle committing anything which needs a commit.
 *
 * Idempotency is handled as follows:
 * a) if there is an r.file, we know that we need to do a merge unless there
 *    is also a p.file, in which case we've been here already.
 * b) if there is not an r.file, we should validate that there are no
 *    open branches in this LOD - if there are, reconstruct the r.file.
 *    XXX - not currently implemented.
 *
 * Text vs graphics:  if resolve is run in text only mode, then each commit
 * of a merge will cause resolve to run "ci" and prompt the user for comments.
 * If we are not run in text mode, then a commit of a merge just leaves the
 * file there and we run citool at the end.
 * In either case, the last step is to find out if we need a commit by looking
 * for the r.ChangeSet, or for any modified files, or for any files with
 * pending deltas.
 */
private	int
pass3_resolve(opts *opts)
{
	char	buf[MAXPATH], **conflicts, s_cset[] = CHANGESET;
	int	n = 0, i;
	int	mustCommit = 0, pc, pe;
	project	*proj;
	char	*prefix;

	if (opts->log) fprintf(opts->log, "==== Pass 3 ====\n");
	opts->pass = 3;

	unless (opts->automerge) {
		FILE	*p;

		if (opts->progress) {
			/*
			 * all conflicts are manual, so drop out for now.
			 * leaving opts->progress set for progbars on check
			 */
			progress_done(tick, "OK");
			tick = 0;
			progress_restoreStderr();
		}
		unless (p = popen("bk _find . -name 'm.*'", "r")) {
			perror("popen of find");
			resolve_cleanup(opts, 0);
		}
		while (fnext(buf, p)) {
			fprintf(stderr, "Needs rename: %s", buf);
			n++;
		}
		pclose(p);
	}
	if (n) {
		fprintf(stderr,
"There are %d pending renames which need to be resolved before the conflicts\n\
can be resolved.  Please rerun resolve and fix these first.\n", n);
		resolve_cleanup(opts, 0);
	}

	/*
	 * Process any conflicts.
	 * Note: this counts on the rename resolution having bumped these
	 * revs if need be.
	 * XXX Note: if we are going to be really paranoid, we should just
	 * do an sfiles, open up each file, check for conflicts, and
	 * reconstruct the r.file if necessary.  XXX - not done.
	 */
	conflicts = find_files(opts, 0);
	progress_adjustMax(tick, nfiles - nLines(conflicts));
	EACH(conflicts) {
		if (opts->progress) progress(tick, ++nticks);
		if (opts->debug) fprintf(stderr, "pass3: %s\n", conflicts[i]);
		if (streq(conflicts[i], "SCCS/s.ChangeSet")) continue;

		if (opts->debug) fprintf(stderr, "pass3: %s", conflicts[i]);

		conflict(opts, conflicts[i]);
		if (opts->errors) {
			goto err;
		}
	}
	resolve_tags(opts);
	unless (opts->quiet || !opts->resolved) {
		fprintf(stdlog,
		    "resolve: resolved %d conflicts in pass 3\n",
		    opts->resolved);
	}

	if (opts->errors) {
err:		unless (opts->autoOnly) {
			fprintf(stderr,
			    "resolve: had errors, nothing is applied.\n");
		}
		resolve_cleanup(opts, 0);
	}

	/* hadConflicts only gets touched by automerge */
	if (opts->hadConflicts) {
		char	*nav[20];
		int	i, rc;

		if (opts->autoOnly) {
			char	*pf;
			char	*str1, *str2;
			unless (opts->quiet) {
				fprintf(stderr,
				    "resolve: %d conflicts "
				    "will need manual resolve.\n",
				    opts->hadConflicts);
			}
			/*
			 * update progressbar info (we're in RESYNC here)
			 */
			pf = aprintf("%s/" RESYNC2ROOT
			    "/BitKeeper/log/progress-sum",
			    proj_root(0));
			if (str1 = loadfile(pf, 0)) {
				chomp(str1);
				str2 = aprintf("%s (%d conflict%s)",
				    str1, opts->hadConflicts,
				    (opts->hadConflicts > 1) ? "s" : "");
				free(str1);
				Fprintf(pf, "%s\n", str2);
				free(str2);
			}
			free(pf);
			resolve_cleanup(opts, 0);
			/* NOT REACHED */
		}

		if (getenv("_BK_PREVENT_RESOLVE_RERUN")) {
			fprintf(stderr,
			    "resolve: %d unresolved conflicts, "
			    "nothing is applied.\n",
			    opts->hadConflicts);
			resolve_cleanup(opts, 0);
			exit(1);	/* Shouldn't get here */
		}

		if (opts->progress) {
			progress_done(tick, "OK");
			progress_restoreStderr();
		}
		/* no textOnly means gui is supported - useDisplay() passed */
		if (!opts->textOnly &&
		    !opts->quiet && !win32()) {
			fprintf(stderr,
			    "Using %s as graphical display\n",
			    gui_displayName());
		}

		nav[i=0] = strdup("bk");
		nav[++i] = strdup("resolve");
		nav[++i] = strdup("-S");
		if (opts->mergeprog) {
			nav[++i] = aprintf("-m%s", opts->mergeprog);
		}
		if (opts->quiet) nav[++i] = strdup("-q");
		/* run progress bars on final check */
		if (opts->progress) nav[++i] = strdup("--progress");
		if (opts->textOnly) nav[++i] = strdup("-T");
		if (opts->comment) nav[++i] = aprintf("-y%s", opts->comment);
		nav[++i] = 0;
		fprintf(stderr,
		    "resolve: %d unresolved conflicts, "
		    "starting manual resolve process for:\n",
		    opts->hadConflicts);
		if (proj_comppath(0)) {
			prefix = aprintf("%s/", proj_comppath(0));
		} else {
			prefix = strdup("");
		}
		EACH(opts->notmerged) {
			fprintf(stderr, "\t%s%s\n", prefix, opts->notmerged[i]);
		}
		free(prefix);
		freeLines(opts->notmerged, free);
		opts->notmerged = 0;
		chdir(RESYNC2ROOT);
		sccs_unlockfile(RESOLVE_LOCK);
		rc = spawnvp(P_WAIT, "bk", nav);
		for (i = 0; nav[i]; i++) free(nav[i]);
		proj_restoreAllCO(0, opts->idDB, 0, 0);
		rc = WIFEXITED(rc) ? WEXITSTATUS(rc) : 1;
		longjmp(cleanup_jmp, 1000+rc); /* exit resolve_main() */
	}

	/*
	 * Get -M the ChangeSet file if we need to, before calling citool
	 * or commit.
	 */
	if (opts->partial) goto nocommit;
	if (mustCommit = xfile_exists(CHANGESET, 'r')) {
		sccs	*s;
		resolve	*rs;
		char	*t;

		s = sccs_init(s_cset, INIT_NOCKSUM);
		rs = resolve_init(opts, s);
		edit(rs);
		/* We may restore it if we bail early */
		t = xfile_fetch(CHANGESET, 'r');
		xfile_store("BitKeeper/tmp/SCCS/r.ChangeSet", 'r', t);
		free(t);
		xfile_delete(CHANGESET, 'r');
		resolve_free(rs);
	} else if (xfile_exists(CHANGESET, 'p')) {
		/*
		 * The only way I can think of this happening is if we are
		 * coming back in after passing the above clause on an
		 * earlier run.  I suppose this could happen if we asked
		 * about open logging and they aborted, got a key, and reran.
		 */
		mustCommit = 1;
	}

	/*
	 * Since we are about to commit, clean up the r.files
	 */
	freeLines(conflicts, free);
	opts->remerge = 1;  /* Make sure we catch all r.files */
	conflicts = find_files(opts, 0);
	EACH(conflicts) xfile_delete(conflicts[i], 'r');
nocommit:


	/*
	 * If there is nothing to do, bail out.
	 */
	pc = pending(0);
	pe = pendingEdits();
	unless (mustCommit || pc || pe) return (0);

	if (pe && noDiffs()) {
		if (opts->log) {
			fprintf(opts->log, "==== Pass 3 autocheckins ====\n");
		}
		checkins(opts, SCCS_MERGE);
		pe = 0;		/* saves a call to pendingEdits() below */
	}

	/*
	 * Always do autocommit if there are no pending changes.
	 * We supply a default comment because there is little point
	 * in bothering a user for a merge.
	 */
	unless (opts->partial || (pe && pendingEdits())) {
		if (opts->log) {
			fprintf(opts->log, "==== Pass 3 autocommits ====\n");
		}
		commit(opts);
		return (0);
	}

	/*
	 * Unless we are in textonly mode, let citool do all the work.
	 */
	unless (opts->textOnly) {
		/*
		 * if in product's RESYNC, then compute deep nest
		 * XXX: for now, compute shallow + deep as sfiles.c:
		 * print_components() calls uniqLines, so dups are okay.
		 */
		proj = proj_init(".");
		assert(proj_isResync(proj));
		if (proj_isProduct(proj)) {
			system("bk gfiles -Rr > BitKeeper/log/deep-nests");
		}
		proj_free(proj);

		if (opts->partial) {
			char	**av, **pfiles = 0;
			int	j = 0;

			/* Save off the files that need deltas so we
			 * can remove the r.file after the delta is done
			 */
			pfiles = find_files(opts, 1);
			/* Run citool across any files in conflict that
			 * have pending deltas.
			 */
			unless (nLines(pfiles)) {
				unless (opts->resolved) {
					fprintf(stderr,
					    "No files listed need deltas\n");
				}
				return (0);
			}
			av = (char **)malloc((nLines(pfiles) + 5) *
			    sizeof(*av));
			av[j++] = "bk";
			av[j++] = "citool";
			av[j++] = "-R";
			av[j++] = "-P";
			EACH(pfiles) av[j++] = pfiles[i];
			av[j] = 0;
			spawnvp(_P_WAIT, av[0], av);
			free(av);
			/* Now that citool is done, remove r.files for
			 * files that no longer need a delta.
			 */
			EACH(pfiles) {
				unless (xfile_exists(pfiles[i], 'p')) {
					xfile_delete(pfiles[i], 'r');
				}
			}
			freeLines(pfiles, free);
		} else {
			if (sys("bk", "citool", "-R", SYS)) {
				syserr("failed, aborting.\n");
				resolve_cleanup(opts, 0);
			}
			if (pending(0) || pendingEdits()) {
				fprintf(stderr, "Failed to check in/commit "
				    "all files, aborting.\n");
				resolve_cleanup(opts, 0);
			}
		}
		return (0);
	}

	/*
	 * We always go look for pending and/or locked files even when
	 * in textonly mode, because an earlier partial run could have
	 * been in a different mode.  So be safe.
	 */
	checkins(opts, 0);
	if (opts->errors) {
		fprintf(stderr, "resolve: had errors, nothing is applied.\n");
		resolve_cleanup(opts, 0);
	}

	if (!opts->partial && pending(0)) {
		assert(!opts->noconflicts);
		commit(opts);
	}

	return (0);
}

void
do_delta(opts *opts, sccs *s, char *comment)
{
	int     flags = DELTA_FORCE;
	ser_t	d = 0;

	comments_done();
	if (comment) {
		comments_save(comment);
		d = comments_get(0, 0, s, 0);
		flags |= DELTA_DONTASK;
	}
	if (opts->quiet) flags |= SILENT;
	sccs_restart(s);
	if (sccs_delta(s, flags, d, 0, 0, 0)) {
		fprintf(stderr, "Delta of %s failed\n", s->gfile);
		resolve_cleanup(opts, 0);
	} else {
		xfile_delete(s->gfile, 'r');
	}
}

private void
checkins(opts *opts, char *comment)
{
	sccs	*s;
	FILE	*p;
	char	buf[MAXPATH];

	unless (p = popen("bk gfiles -c", "r")) {
		perror("popen of find");
		resolve_cleanup(opts, 0);
	}
	while (fnext(buf, p)) {
		chop(buf);
		s = sccs_init(buf, INIT_NOCKSUM);
		assert(s);
		do_delta(opts, s, comment);
		sccs_free(s);
	}
	pclose(p);
}
	
/*
 * Merge a conflict, manually or automatically.
 * We handle permission conflicts here as well.
 *
 * XXX - we need to handle descriptive text and symbols.
 * The symbol case is weird: a conflict is when the same symbol name
 * is in both branches without one below the trunk.
 */
private	void
conflict(opts *opts, char *sfile)
{
	sccs	*s;
	resolve	*rs;
	deltas	d;

	s = sccs_init(sfile, INIT_NOCKSUM);

	rs = resolve_init(opts, s);
	assert(rs->dname && streq(rs->dname, s->sfile));

	d.local = sccs_findrev(s, rs->revs->local);
	d.gca = sccs_findrev(s, rs->revs->gca);
	d.remote = sccs_findrev(s, rs->revs->remote);
	/*
	 * The smart programmer will notice that the case where both sides
	 * changed the mode to same thing is just silently merged below.
	 *
	 * The annoyed programmer will note that the resolver may
	 * replace the sccs pointer.
	 */
	unless (fileType(MODE(s, d.local)) == fileType(MODE(s, d.remote))) {
		rs->opaque = (void*)&d;
		if (resolve_filetypes(rs) == EAGAIN) {
			resolve_free(rs);
			return;
		}
		s = rs->s;
		if (opts->errors) return;
		/* Need to re-init these, the resolve stomped on the s-> */
		d.local = sccs_findrev(s, rs->revs->local);
		d.gca = sccs_findrev(s, rs->revs->gca);
		d.remote = sccs_findrev(s, rs->revs->remote);
	}

	unless (MODE(s, d.local) == MODE(s, d.remote)) {
		rs->opaque = (void*)&d;
		if (resolve_modes(rs) == EAGAIN) {
			resolve_free(rs);
			return;
		}
		s = rs->s;
		if (opts->errors) return;
		/* Need to re-init these, the resolve stomped on the s-> */
		d.local = sccs_findrev(s, rs->revs->local);
		d.gca = sccs_findrev(s, rs->revs->gca);
		d.remote = sccs_findrev(s, rs->revs->remote);
	}

	/*
	 * Merge changes to the flags (RCS/SCCS/EXPAND1/etc).
	 */
	unless (XFLAGS(s, d.local) == XFLAGS(s, d.remote)) {
		rs->opaque = (void*)&d;
		resolve_flags(rs);
		s = rs->s;
		if (opts->errors) return;
		/* Need to re-init these, the resolve stomped on the s-> */
		d.local = sccs_findrev(s, rs->revs->local);
		d.gca = sccs_findrev(s, rs->revs->gca);
		d.remote = sccs_findrev(s, rs->revs->remote);
	}

	if (opts->debug) {
		fprintf(stderr, "Conflict: %s: %s %s %s\n",
		    s->gfile,
		    rs->revs->local, rs->revs->gca, rs->revs->remote);
	}
	if (opts->noconflicts) {
		unless (opts->autoOnly) {
	    		fprintf(stderr,
			    "resolve: can't process conflict in %s\n",
			    s->gfile);
		}
err:		resolve_free(rs);
		opts->errors = 1;
		return;
	}

	/*
	 * See if we merged some symlink conflicts, if so
	 * there can't be any conflict left.
	 */
	if (S_ISLNK(fileType(MODE(s, d.local)))) {
		int	flags;
		ser_t	e;

		assert(MODE(s, d.local) == MODE(s, d.remote));
		if (sccs_get(rs->s, 0, 0, 0, 0, SILENT, s->gfile, 0)) {
			sccs_whynot("delta", rs->s);
			goto err;
		}
		if (!LOCKED(rs->s) && edit(rs)) goto err;
		comments_save("Auto merged");
		e = comments_get(0, 0, rs->s, 0);
		sccs_restart(rs->s);
		flags = DELTA_DONTASK|DELTA_FORCE|(rs->opts->quiet? SILENT : 0);
		if (sccs_delta(rs->s, flags, e, 0, 0, 0)) {
			sccs_whynot("delta", rs->s);
			goto err;
		}
	    	rs->opts->resolved++;
		xfile_delete(rs->s->gfile , 'r');
		resolve_free(rs);
		return;
	}

	/*
	 * If the a.file (a for again) was created, delete here as
	 * we have reached the point where we are doing it again.
	 */
	xfile_delete(sfile, 'a');

	if (opts->automerge) {
		automerge(rs, 0, 0);
		resolve_free(rs);
		return;
	}
	resolve_contents(rs);
	resolve_free(rs);
}

int
get_revs(resolve *rs, names *n)
{
	int	flags = (rs->opts->debug ? 0 : SILENT);

	if (sccs_get(rs->s, rs->revs->local, 0, 0, 0, flags, n->local, 0)) {
		fprintf(stderr, "Unable to get %s\n", n->local);
		return (-1);
	}

	if (sccs_get(rs->s, rs->revs->gca, 0, 0, 0, flags, n->gca, 0)) {
		fprintf(stderr, "Unable to get %s\n", n->gca);
		return (-1);
	}
	if (sccs_get(rs->s, rs->revs->remote, 0, 0, 0, flags, n->remote, 0)) {
		fprintf(stderr, "Unable to get %s\n", n->remote);
		return (-1);
	}
	return (0);
}

/*
 * Wrap resolve.c: automerge() so takepatch can use it without knowing
 * about resolve data structures.
 */
int
resolve_automerge(sccs *s, ser_t local, ser_t remote)
{
	int	ret;
	resolve	*rs;
	names	*revs;
	opts	opts = {0};

	rs = new(resolve);
	revs = new(names);

	revs->local = strdup(REV(s, local));
	revs->remote = strdup(REV(s, remote));
	revs->gca = strdup(REV(s, sccs_gca(s, local, remote, 0, 0)));
	rs->revs = revs;
	rs->opts = &opts;
	rs->s = s;
	rs->d = sccs_top(s);
	opts.quiet = 1;
	opts.autoOnly = 1;
	opts.mergeprog = getenv("BK_RESOLVE_MERGEPROG");

	ret = automerge(rs, 0, 0);

	/* keep the 's', clean everything else */
	rs->s = 0;
	resolve_free(rs);
	if (opts.notmerged) freeLines(opts.notmerged, free);
	return (ret);
}

/*
 * Try to automerge. 0 == auto-merged, 1 == did not auto-merge, -1 == error
 */
int
automerge(resolve *rs, names *n, int identical)
{
	char	cmd[MAXPATH*4];
	int	ret;
	char	*name = basenm(PATHNAME(rs->s, rs->d));
	char	*merge_msg = 0;
	char	*l, *r;
	names	tmp;
	int	do_free = 0;
	int	automerged = -1;
	int	flags;
	ser_t	a, b;
	char	**av = 0;
	int	fd, oldfd;

	unless (sccs_findtips(rs->s, &a, &b)) {
		unless (rs->opts->quiet) {
			fprintf(stderr, "'%s' already merged\n",
			    rs->s->gfile);
		}
		return (0);
	}
	if (rs->opts->debug) fprintf(stderr, "automerge %s\n", name);

	if (!n &&
	    (rs->opts->mergeprog ||
	     identical || BINARY(rs->s) || NOMERGE(rs->s))) {
		sprintf(cmd, "BitKeeper/tmp/%s@%s", name, rs->revs->local);
		tmp.local = strdup(cmd);
		sprintf(cmd, "BitKeeper/tmp/%s@%s", name, rs->revs->gca);
		tmp.gca = strdup(cmd);
		sprintf(cmd, "BitKeeper/tmp/%s@%s", name, rs->revs->remote);
		tmp.remote = strdup(cmd);
		if (get_revs(rs, &tmp)) {
			rs->opts->errors = 1;
			freenames(&tmp, 0);
			return (-1);
		}
		n = &tmp;
		do_free = 1;
	}

	merge_msg = strdup("Auto merged");
	unless (unlink(rs->s->gfile)) rs->s = sccs_restart(rs->s);
	if (identical ||
	    ((BINARY(rs->s) || NOMERGE(rs->s)) &&
	      sameFiles(n->local, n->remote))) {
		assert(n);
		fileCopy(n->local, rs->s->gfile);
		goto merged;
	}

	if (BINARY(rs->s) || NOMERGE(rs->s)) {
		if (BINARY(rs->s) && !rs->opts->quiet) {
			fprintf(stderr,
			    "Not automerging binary '%s'\n", rs->s->gfile);
		}
		rs->opts->hadConflicts++;
		automerged = 1;
		goto out;
	}

	/* Run smerge first */
	fflush(stdout);
	oldfd = dup(1);
	close(1);
	fd = open(rs->s->gfile, O_CREAT|O_TRUNC|O_WRONLY, 0666);
	assert(fd == 1);
	av = addLine(av, "smerge");
	av = addLine(av, (l = aprintf("-l%s", rs->revs->local)));
	av = addLine(av, (r = aprintf("-r%s", rs->revs->remote)));
	av = addLine(av, rs->s->gfile);
	av = addLine(av, 0);
	getoptReset();
	ret = smerge_main(nLines(av), &av[1]);
	free(l);
	free(r);
	freeLines(av, 0);
	fflush(stdout);
	close(fd);
	dup2(oldfd, 1);
	close(oldfd);

	if ((ret != 0) && rs->opts->mergeprog) {
		/*
		 * If smerge didn't work, and the user gave us a merge
		 * program, try that.
		 */
		ret = sys(rs->opts->mergeprog,
		    n->local, n->gca, n->remote, rs->s->gfile, SYS);
		ret = WIFEXITED(ret) ? WEXITSTATUS(ret) : 2;
		if (ret == 0) {
			free(merge_msg);
			merge_msg = aprintf("Auto merged using: %s",
			    rs->opts->mergeprog);
		}
	}

	if (ret == 0) {
		ser_t	d;

		unless (rs->opts->quiet) {
			fprintf(stderr,
			    "Content merge of %s OK\n", rs->s->gfile);
		}
merged:		if (!LOCKED(rs->s) && edit(rs)) goto out;
		comments_save(merge_msg);
		d = comments_get(0, 0, rs->s, 0);
		rs->s = sccs_restart(rs->s);
		flags = DELTA_DONTASK|DELTA_FORCE|SILENT;
		if (sccs_delta(rs->s, flags, d, 0, 0, 0)) {
			sccs_whynot("delta", rs->s);
			rs->opts->errors = 1;
			goto out;
		}
		rs->opts->resolved++;
		xfile_delete(rs->s->gfile, 'r');
		automerged = 0;
		goto out;
	}

	/* We could not merge this one. */
	rs->opts->notmerged =
	    addLine(rs->opts->notmerged,strdup(rs->s->gfile));
	if (ret == 1) {
		unless (rs->opts->autoOnly) {
			fprintf(stderr,
			    "Conflicts during automerge of %s\n",
			    rs->s->gfile);
		}
		rs->opts->hadConflicts++;
		unlink(rs->s->gfile);
		automerged = 1;
		goto out;
	} else if (ret != 0) {
		fprintf(stderr, "Unknown merge status: 0x%x\n", ret);
		rs->opts->errors = 1;
		unlink(rs->s->gfile);
		goto out;
	}
	fprintf(stderr,
	    "Automerge of %s failed for unknown reasons\n", rs->s->gfile);
	rs->opts->errors = 1;
	unlink(rs->s->gfile);

out:	if (do_free) {
		unlink(tmp.local);
		unlink(tmp.gca);
		unlink(tmp.remote);
		freenames(&tmp, 0);
	}
	free(merge_msg);
	return (automerged);
}

/*
 * Figure out which delta is the branch one and merge it in.
 */
int
edit(resolve *rs)
{
	int	flags = GET_EDIT|GET_SKIPGET|SILENT;
	char	*branch;

	branch = strchr(rs->revs->local, '.');
	assert(branch);
	if (strchr(++branch, '.')) {
		branch = rs->revs->local;
	} else {
		branch = rs->revs->remote;
	}
	if (sccs_get(rs->s, 0, branch, 0, 0, flags, 0, 0)) {
		fprintf(stderr,
		    "resolve: cannot edit/merge %s\n", rs->s->sfile);
		rs->opts->errors = 1;
		return (1);
	}
	sccs_restart(rs->s);
	return (0);
}

/* ---------------------------- Pass4 stuff ---------------------------- */

private	int
pendingRenames(void)
{
	char	**d;
	int	i, n = 0;

	unless (d = getdir("BitKeeper/RENAMES/SCCS")) return (0);
	EACH (d) {
		fprintf(stderr, "Pending: BitKeeper/RENAMES/SCCS/%s\n", d[i]);
		n++;
	}
	freeLines(d, free);
	return (n);
}

/*
 * Return true if there are pending deltas (not in a changeset yet).
 * If checkComments is set, do not return true if all of the comments
 * are either the automerge or the sccs no diffs merge.
 */
private	int
pending(int checkComments)
{
	FILE	*f;
	char	buf[MAXPATH];
	char	*t;
	int	ret;

	unless (checkComments) {
		f = popen("bk gfiles -rpC", "r");
		ret = fgetc(f) != EOF;
		pclose(f);
		return (ret);
	}
	f = popen("bk gfiles -rpCA | bk sccslog -CA -", "r");
	while (fnext(buf, f)) {
		unless (t = strchr(buf, '\t')) {
			pclose(f);
			return (1);	/* error means we assume pending */
		}
		t++;
		chop(t);
		unless (streq(t, AUTO_MERGE) || streq(t, SCCS_MERGE)) {
			pclose(f);
			return (1);
		}
	}
	pclose(f);
	return (0);
}

/*
 * Return true if there are modified files not yet checked in (except ChangeSet)
 */
private	int
pendingEdits(void)
{
	char	buf[MAXPATH];
	FILE	*f;

	f = popen("bk gfiles -c", "r");

	while (fnext(buf, f)) {
		chop(buf);
		/* Huh? */
		if (streq(GCHANGESET, buf)) continue;
		pclose(f);
		return (1);
	}
	pclose(f);
	return (0);
}

/*
 * Commit the changeset.
 */
private	void
commit(opts *opts)
{
	int	i;
	char	*cmds[12], *cmt = 0;

	cmds[i = 0] = "bk";
	cmds[++i] = "commit";
	cmds[++i] = "-S";
	cmds[++i] = "-R";
	/* force a commit if we are a null merge */
	unless (opts->resolved || opts->renamed || opts->comps) {
		cmds[++i] = "-F";
	}
	if (opts->quiet) cmds[++i] = "-q";
	if (opts->comment) {
	    	/* Only do comments if they really gave us one */
		if (opts->comment[0]) {
			cmt = cmds[++i] = aprintf("-y%s", opts->comment);
		}
	} else if (xfile_exists(CHANGESET, 'c')) {
		cmds[++i] = "-c";
	} else {
		xfile_store(CHANGESET, 'c', "Merge\n");
	}
	cmds[++i] = 0;
	i = spawnvp(_P_WAIT, "bk", cmds);
	if (cmt) free(cmt);
	if (WIFEXITED(i) && !WEXITSTATUS(i)) return;
	fprintf(stderr, "Commit aborted, no changes applied.\n");
	resolve_cleanup(opts, 0);
}

private void
unfinished(opts *opts)
{
	FILE	*p;
	int	n = 0;
	char	buf[MAXPATH];

	if (pendingRenames()) resolve_cleanup(opts, 0);
	unless (p = popen("bk _find . -name '[mr].*'", "r")) {
		perror("popen of find");
		resolve_cleanup(opts, 0);
	}
	while (fnext(buf, p)) {
		if (strneq(buf, "BitKeeper/tmp/", 14)) continue;
		fprintf(stderr, "Pending: %s", buf);
		n++;
	}
	pclose(p);
	if (n) resolve_cleanup(opts, 0);
}

/*
 * Remove a sfile, and matching gfile.
 * Remember the directories so we can remove them too
 */
private int
rm_sfile(char *sfile, hash *emptydirs)
{
	char	*p;
	char	buf[MAXPATH];

	assert(!IsFullPath(sfile));
	if (unlink(sfile)) {
		perror(sfile);
		return (-1);
	}
	/* remove gfile too */
	p = sccs2name(sfile);
	unlink(p);
	free(p);

	strcpy(buf, sfile);
	hash_storeStr(emptydirs, dirname(buf), 0);
	return (0);
}

/*
 * Make sure the file name did not change case orientation after we copied it
 */
private int
chkCaseChg(char **applied)
{
	int	i;
	char	realname[MAXPATH];

	assert(localDB);
	EACH(applied) {
		getRealName(applied[i], localDB, realname);
		unless (streq(applied[i], realname)) {
			if (strcasecmp(applied[i], realname) == 0) {
				getMsg2("case_folding",
				    applied[i], realname, '=', stderr);
			} else {
				assert("Unknown error" == 0);
			}
			return (-1);
		}
	}
	return (0);
}

/*
 * Make sure there are no edited files, no RENAMES, and {r,m}.files and
 * apply all files.
 */
private	int
pass4_apply(opts *opts)
{
	sccs	*r, *l;
	int	offset = strlen(ROOT2RESYNC) + 1;	/* RESYNC/ */
	int	eperm = 0, flags, nold = 0, ret;
	int	i;
	FILE	*f = 0;
	FILE	*save = 0;
	char	**applied = 0;
	char	buf[MAXPATH];
	char	key[MAXKEY];
	MDBM	*permDB = mdbm_mem();
	char	*cmd;
	hash	*emptydirs;
	char	**dirlist;
	int	did_partial;

	if (opts->log) fprintf(opts->log, "==== Pass 4 ====\n");
	opts->pass = 4;

	/*
	 * If the user called us directly we don't have a repo lock and need
	 * to get one.
	 */
	unless (opts->from_pullpush) {
		chdir(RESYNC2ROOT);
		flags = CMD_WRLOCK|CMD_IGNORE_RESYNC;
		unless (opts->standalone) flags |= CMD_NESTED_WRLOCK;
		cmdlog_lock(flags);
		flags = 0;		/* reused below */
		chdir(ROOT2RESYNC);
	}

	unfinished(opts);

	/*
	 * Call the pre-apply trigger if there is one so that we have
	 * one last chance to bail out.
	 */
	putenv("BK_CSETLIST=" CSETS_IN);
	if (getenv("_BK_IN_BKD")) {
		cmd = "remote apply";
	} else {
		cmd = "apply";
	}
	if (ret = trigger(cmd,  "pre")) {
		switch (ret) {
		    case 3: flags = CLEAN_MVRESYNC; break;
		    case 2: flags = CLEAN_ABORT; break;
		    default: flags = CLEAN_ABORT|CLEAN_PENDING; break;
		}
		mdbm_close(permDB);
		resolve_cleanup(opts, CLEAN_NOSHOUT|flags);
	}

	/*
	 * Pass 4a - check for edited files and build up a list of files to
	 * backup and remove.
	 */
	chdir(RESYNC2ROOT);
	save = fopen(BACKUP_LIST, "w+");
	assert(save);
	unlink(PASS4_TODO);
	/* needs to be sfiles not gfiles */
	sprintf(key, "bk sfiles -r '%s' > " PASS4_TODO, ROOT2RESYNC);
	if (system(key) || !(f = fopen(PASS4_TODO, "r+")) || !save) {
		fprintf(stderr, "Unable to create|open " PASS4_TODO);
		fclose(save);
		mdbm_close(permDB);
		resolve_cleanup(opts, 0);
	}
	chmod(PASS4_TODO, 0666);
	while (fnext(buf, f)) {
		if (opts->progress) progress(tick, ++nticks);
		chop(buf);
		/*
		 * We want to check the checksum here and here only
		 * before we throw it over the wall.
		 */
		unless (r = sccs_init(buf, 0)) {
			fprintf(stderr,
			    "resolve: can't init %s - abort.\n", buf);
			fclose(save);
			fprintf(stderr, "resolve: no files were applied.\n");
			resolve_cleanup(opts, 0);
		}
#define	F ADMIN_FORMAT|ADMIN_TIME|ADMIN_BK
		if (sccs_adminFlag(r, SILENT|F)) {
			sccs_adminFlag(r, F);
			fprintf(stderr, "resolve: bad file %s;\n", r->sfile);
			fprintf(stderr, "resolve: no files were applied.\n");
			fclose(save);
			mdbm_close(permDB);
			resolve_cleanup(opts, 0);
		}
		/* Can I write where the file wants to go? */
		if (writeCheck(r, permDB)) eperm = 1;

		sccs_sdelta(r, sccs_ino(r), key);
		sccs_free(r);
		if (l = sccs_keyinit(0, key, INIT_NOCKSUM, opts->idDB)) {
			proj_saveCO(l);
			/*
			 * This should not happen, the repository is locked.
			 */
			if (sccs_clean(l, SILENT)) {
				fprintf(stderr,
				    "\nWill not overwrite edited %s\n",
				    l->gfile);
				fclose(save);
				resolve_cleanup(opts, 0);
			}
			/* Can I write where it used to live? */
			if (writeCheck(l, permDB)) eperm = 1;

			fprintf(save, "%s\n", l->sfile);
			sccs_free(l);
			/* buf+7 == skip RESYNC/ */
			idcache_item(opts->idDB, key, buf + 7);
			++nold;
		} else {
			/*
			 * handle new files.
			 * This little chunk of magic is to detect BAM files
			 * and respect BAM_checkout.
			 */
			if (BAMkey(key)) {
				proj_saveCOkey(0, key, proj_checkout(0) >> 4);
			} else {
				proj_saveCOkey(0, key, proj_checkout(0) & 0xf);
			}
		}
	}
	fclose(save);
	mdbm_close(permDB);

	if (eperm) {
		getMsg("write_perms", 0, '=', stderr);
		resolve_cleanup(opts, 0);
	}

	/*
	 * Pass 4b.
	 * Save the list of files and then remove them.
	 */
	if (size(BACKUP_LIST) > 0) {
		unlink(BACKUP_SFIO);
		if (sysio(BACKUP_LIST,
		    BACKUP_SFIO, 0, "bk", "sfio", "-omq", SYS)) {
			fprintf(stderr,
			    "Unable to create backup %s from %s\n",
			    BACKUP_SFIO, BACKUP_LIST);
			resolve_cleanup(opts, 0);
		}
		save = fopen(BACKUP_LIST, "rt");
		assert(save);
		progress_adjustMax(tick, nfiles - nold);
		emptydirs = hash_new(HASH_MEMHASH);
		while (fnext(buf, save)) {
			if (opts->progress) progress(tick, ++nticks);
			chop(buf);
			if (opts->log) fprintf(stdlog, "unlink(%s)\n", buf);
			if (rm_sfile(buf, emptydirs)) {
				fclose(save);
				restore_backup(BACKUP_SFIO, 0);
				resolve_cleanup(opts, 0);
			}
		}
		fclose(save);

		/* remove empty directories, but keep ones used in RESYNC */
		dirlist = 0;
		EACH_HASH(emptydirs) {
			concat_path(buf, ROOT2RESYNC, emptydirs->kptr);
			if (isdir(buf)) {
				dirlist = addLine(dirlist, emptydirs->kptr);
			}
		}
		EACH(dirlist) hash_deleteStr(emptydirs, dirlist[i]);
		freeLines(dirlist, 0);
		rmEmptyDirs(emptydirs);
		hash_free(emptydirs);
	}

	/*
	 * Pass 4c - apply the files.
	 */
	fflush(f);
	rewind(f);
	progress_adjustMax(tick, nfiles - nold);
	while (fnext(buf, f)) {
		if (opts->progress) progress(tick, ++nticks);
		chop(buf);
		/*
		 * We want to get the part of the path without the RESYNC.
		 */
		if (exists(&buf[offset])) {
			fprintf(stderr,
			    "resolve: failed to remove conflict %s\n",
			    &buf[offset]);
			unapply(applied);
			restore_backup(BACKUP_SFIO, 0);
			resolve_cleanup(opts, 0);
		}
		if (opts->log) {
			fprintf(stdlog, "copy(%s, %s)\n", buf, &buf[offset]);
		}
		if (fileLink(buf, &buf[offset])) {
			fprintf(stderr,
			    "copy(%s, %s) failed\n", buf, &buf[offset]);
err:			unapply(applied);
			restore_backup(BACKUP_SFIO, 0);
			resolve_cleanup(opts, 0);
		} else {
			opts->applied++;
		}
		applied = addLine(applied, strdup(buf + offset));
	}
	fclose(f);

	if (xfile_exists("RESYNC/ChangeSet", 'd')) {
		xfile_store("ChangeSet", 'd', "");
		proj_dirstate(0, ".", DS_PENDING, 1);
	}
	if (exists("RESYNC/BitKeeper/log/TIP")) {
		fileLink("RESYNC/BitKeeper/log/TIP", "BitKeeper/log/TIP");
	} else {
		unlink("BitKeeper/log/TIP");
	}

	/*
	 * If the HERE file changed as a result of the pull, throw it over
	 * the wall.
	 */
	if (exists("RESYNC/BitKeeper/log/HERE")) {
		if (unlink("BitKeeper/log/HERE")) {
			perror("unlink");
			goto err;
		}
		if (fileMove("RESYNC/BitKeeper/log/HERE","BitKeeper/log/HERE")){
			perror("move");
			goto err;
		}
	}

 	/*
	 * If case folding file system , make sure file name
	 * did not change case after we copied it
	 */
	if (localDB && chkCaseChg(applied)) goto err;

	unless (opts->quiet) {
		fprintf(stderr,
		    "resolve: applied %d files in pass 4\n", opts->applied);
	}
	proj_restoreAllCO(0, opts->idDB, tick, 0);
	if (proj_sync(0)) {
		/*
		 * It's worth pointing out that we still call this when we
		 * are resolving the creation of a new project.  It doesn't
		 * happen in the real world, it's a makepatch -r.. |
		 * takepatch -i type of thing but I ran into it when doing
		 * some perf tests.
		 */
		sync();
	}
	if (run_check(opts->quiet && !opts->progress,
		opts->verbose && !opts->progress, applied, 0, &did_partial)) {
		fprintf(stderr, "Check failed.  Resolve not completed.\n");
		goto err;
	} else {
		/* remember we passed a full repo check */
		unless (did_partial) opts->fullCheck = 1;
	}
	if (xfile_exists("ChangeSet", 'd') && proj_isComponent(0) &&
	    !exists("RESYNC/BitKeeper/log/port")) {
		opts->moveup = 1;
	}
	unlink(BACKUP_LIST);
	unlink(BACKUP_SFIO);
	freeLines(applied, free);
	unlink(PASS4_TODO);

	if (opts->progress) progress_done(tick, "OK");

	flags = CLEAN_OK|CLEAN_RESYNC|CLEAN_PENDING;
	resolve_cleanup(opts, flags);
	/* NOTREACHED */
	return (0);
}

private int
writeCheck(sccs *s, MDBM *db)
{
	char	*t;
	char	path[MAXPATH];
	struct	stat	sb;

	t = s->sfile;
	if (strneq(t, "RESYNC/", 7)) t += 7;
	strcpy(path, t);	/* RESYNC/SCCS want SCCS */
	if (t = strrchr(path, '/')) *t = 0;
	while (1) {
		t = strrchr(path, '/');
		if (mdbm_store_str(db, path, "", MDBM_INSERT)) return (0);
		unless (lstat(path, &sb)) {
			if (S_ISDIR(sb.st_mode)) {
				unless (writable(path) &&
				    (access(path, W_OK) == 0)) {
					fprintf(stderr,
					    "No write permission: %s\n", path);
					return (1);
				}
				/* Must check parent of SCCS dir */
				unless (t && streq(t, "/SCCS") ||
					streq(path, "SCCS")) {
					return (0);
				}
			} else {
				/*
				 * File where a directory needs to be,
				 * The rest of resolve will deal with
				 * this, just make sure we can write
				 * this directory so it can.
				 */
			}
		}
		if (t) {
			*t = 0;
		} else {
			if (streq(path, ".")) break;
			strcpy(path, ".");
		}
	}
	return (0);
}

/*
 * Unlink all the things we successfully applied.
 */
private void
unapply(char **applied)
{
	int	i;
	hash	*emptydirs = hash_new(HASH_MEMHASH);

	EACH(applied) rm_sfile(applied[i], emptydirs);
	rmEmptyDirs(emptydirs);
	hash_free(emptydirs);
}

private	void
freeStuff(opts *opts)
{
	/*
	 * Clean up allocations/files/dbms
	 */
	if (opts->log && (opts->log != stderr)) fclose(opts->log);
	if (opts->rootDB) mdbm_close(opts->rootDB);
	if (opts->idDB) mdbm_close(opts->idDB);
	if (opts->goneDB) mdbm_close(opts->goneDB);
}

private void
csets_in(opts *opts)
{
	char	buf[MAXPATH];

	sprintf(buf, "%s/%s", ROOT2RESYNC, CSETS_IN);
	/* May not exist if we pulled in tags only */
	if (exists(buf)) rename(buf, CSETS_IN);
}

void
resolve_cleanup(opts *opts, int what)
{
	char	buf[MAXPATH];
	char	pendingFile[MAXPATH];
	FILE	*f;
	char	*t;
	int	rc = 1;

	if (opts->progress) progress_restoreStderr();

	/*
	 * When resolve succeeds (defined by pass4_apply() running
	 * to successful completion), we'll be called with the
	 * CLEAN_OK bit set and you can ignore the rest of this
	 * comment.
	 *
	 * In general, on failure, this function is called
	 * with what == 0.
	 *
	 * However, if we have a pass2 failure, we'll get called
	 * with CLEAN_ABORT|CLEAN_PENDING.
	 *
	 * Now, if pull requested --auto-only, then we shouldn't
	 * automatically abort.  Resolve will be rerun interactively
	 * and the user will be give a chance to fix the problem.
	 */
	unless (what & CLEAN_OK) {
		if (!opts->batch && opts->autoOnly) {
			what &= ~(CLEAN_ABORT|CLEAN_PENDING);
		}
	}

	unless (exists(ROOT2RESYNC)) chdir(RESYNC2ROOT);
	unless (exists(ROOT2RESYNC)) {
		fprintf(stderr, "cleanup: can't find RESYNC dir\n");
		fprintf(stderr, "cleanup: nothing removed.\n");
		goto exit;
	}

	/*
	 * Get the patch file name from RESYNC before deleting RESYNC.
	 */
	sprintf(buf, "%s/%s", ROOT2RESYNC, "BitKeeper/tmp/patch");
	pendingFile[0] = 0;
	unless (f = fopen(buf, "r")) {
		fprintf(stderr, "Warning: no BitKeeper/tmp/patch\n");
	} else {
		fnext(pendingFile, f);
		chop(pendingFile);
		fclose(f);
	}

	/*
	 * If we are done, save the csets file.
	 */
	if ((what & CLEAN_OK) && exists(buf)) csets_in(opts);
	if (what & CLEAN_RESYNC) {
		assert(exists("RESYNC"));
		if (rmtree("RESYNC")) {
			perror("RESYNC");
			fprintf(stderr, "resolve: rmtree failed\n");
		}
	} else if (what & CLEAN_ABORT) {
		strcpy(buf, "bk -?BK_NO_REPO_LOCK=YES abort -qfp");
		if (exists("RESYNC/BitKeeper/log/port")) strcat(buf, "S");
		if (system(buf)) {
			fprintf(stderr, "Abort failed\n");
		}
	} else if (what & CLEAN_MVRESYNC) {
		char	*dir = savefile(".", "RESYNC-", 0);
		char	*cmd;

		assert(exists("RESYNC"));
		assert(dir);
		unlink(dir);
		/*
		 * We can't just delete (or move) the RESYNC
		 * directory because abort needs it to cleanup the other
		 * components, so we copy the directory and then call abort.
		 */
		mkdir(dir, 0777);
		sccs_mkroot(dir);
		cmd = aprintf("bk --cd='%s' _find . -type f | "
		    "bk --cd='%s' sfio --raw -qmo | "
		    "bk --cd='%s' sfio --raw -qmi",
		    ROOT2RESYNC,
		    ROOT2RESYNC,
		    dir);
		if (system(cmd)) perror("dircopy");
		free(cmd);
		strcpy(buf, "bk -?BK_NO_REPO_LOCK=YES abort -fp");
		if (exists("RESYNC/BitKeeper/log/port")) strcat(buf, "S");
		if (system(buf)) {
			fprintf(stderr, "Abort failed\n");
		}
		free(dir);
	} else {
		if (xfile_exists(ROOT2RESYNC "/" CHANGESET, 'p')) {
			assert(!exists("RESYNC/ChangeSet"));
			xfile_delete(ROOT2RESYNC "/" CHANGESET, 'p');
			if (t = xfile_fetch(ROOT2RESYNC
				"/BitKeeper/tmp/ChangeSet", 'r')) {
				xfile_store(ROOT2RESYNC "/" CHANGESET, 'r', t);
				free(t);
				xfile_delete(ROOT2RESYNC
				    "/BitKeeper/tmp/ChangeSet", 'r');
			}
		}
		unless (what & CLEAN_NOSHOUT) {
			unless (opts->progress) {
				fprintf(stderr,
				    "resolve: RESYNC directory "
				    "left intact.\n");
			}
		}
	}

	if (what & CLEAN_PENDING) {
		if (pendingFile[0] && !getenv("TAKEPATCH_SAVEDIRS")) {
			unlink(pendingFile);
		}
		rmdir(ROOT2PENDING);
	}

	/* XXX - shouldn't this be below the CLEAN_OK?
	 * And maybe claused on opts->pass4?
	 */
	proj_restoreAllCO(0, opts->idDB, 0, 0);

	freeStuff(opts);
	unless (what & CLEAN_OK) {
		unless ((what & CLEAN_NOSHOUT) || opts->progress) SHOUT2();
		goto exit;
	}



	/* Only get here from pass4_apply() */

	/*
	 * While we might have done a check, we might need a repack, which
	 * we can't do while a RESYNC is around because the heap files are
	 * shared between main repo and RESYNC ChangeSet files.  So a
	 * machine that only gets pushed to can accumulate crud.
	 */
	if (cset_needRepack()) {
		if (opts->fullCheck) {
			sccs	*cset = sccs_csetInit(0);

			bin_heapRepack(cset);
			if (rc = sccs_newchksum(cset)) {
				perror(cset->fullsfile);
			}
			sccs_free(cset);
		} else {
			rc = run_check(opts->quiet, opts->verbose, 0, 0, 0);
		}
	} else {
		rc = 0;
	}
	/* Do repack before post trigger, as write lock is downgraded here */
	resolve_post(opts, 0);
	/*
	 * Need to delay this until here, so that the repacked cset file
	 * is moved above, as after that, we have to hold off doing a
	 * repack.
	 */
	if (!rc && opts->moveup && moveupComponent()) rc = 1;
exit:
	sccs_unlockfile(RESOLVE_LOCK);
	longjmp(cleanup_jmp, rc+1000);
}

typedef	struct {
	char	**files;
	opts	*opts;
	u32	pfiles_only:1;
} cinfo;

/*
 * LMXXX - This does way too many things and is almost certainly going to
 * bite us someday, I'd like it split up to do one thing well.
 */
private int
resolvewalk(char *file, char type, void *data)
{
	char	*p, *q;
	cinfo	*ci = (cinfo *)data;
	opts	*opts = ci->opts;
	int	e;

	unless (p = strrchr(file, '/')) return (0);
	unless (strneq(file, "./", 2)) return (0);
	file += 2;
	assert(p[1] = 's');
	if (opts->partial && streq(file, "SCCS/s.ChangeSet")) return (0);
	if (opts->automerge && xfile_exists(file, 'm')) return (0);
	unless (ci->pfiles_only || xfile_exists(file, 'r')) return (0);
	e = xfile_exists(file, 'p');
	if (ci->pfiles_only && !e) return (0);
	if (!ci->pfiles_only && !opts->remerge && e) {
		/* Skip makes an a.file (a for again) */
		unless (xfile_exists(file, 'a')) return (0);
	}
	q = sccs2name(file);

	/* just like in export -tpatch */
	if (opts->excludes || opts->includes) {
		int	skip = 0;

		if (opts->excludes && match_globs(q, opts->excludes, 0)) {
			if (opts->debug) fprintf(stderr, "SKIPx %s\n", q);
			skip = 1;
		}
		if (opts->includes && !match_globs(q, opts->includes, 0)) {
			if (opts->debug) fprintf(stderr, "SKIPi %s\n", q);
			skip = 1;
		}
		unless (skip) {
			if (opts->debug) fprintf(stderr, "ADD(%s)\n", q);
			ci->files = addLine(ci->files, strdup(file));
		}
	} else {
		if (opts->debug) fprintf(stderr, "ADD(%s)\n", q);
		ci->files = addLine(ci->files, strdup(file));
	}
	free(q);
	return (0);
}

private char **
find_files(opts *opts, int pfiles)
{
	cinfo   cinfo;

	cinfo.files = 0;
	cinfo.opts = opts;
	cinfo.pfiles_only = pfiles;
	walksfiles(".", resolvewalk, &cinfo);
	return (cinfo.files);
}

/*
 * Called from a nested component's repo root after a sucessful resolve
 * that did a merge.
 * This function moves the new ChangeSet file to the product's RESYNC
 * so that it can be part of the resolve to run in the product.
 */
private int
moveupComponent(void)
{
	project	*comp = proj_init(".");
	char	*cpath = proj_comppath(comp);
	int	rc = 0;
	MDBM	*idDB;
	char	*t, *from, *to;
	char	buf[MAXPATH];

	concat_path(buf, proj_root(proj_product(comp)), ROOT2RESYNC);
	unless (isdir(buf)) return (0); /* no product RESYNC */
	chdir(buf);

	/*
	 * Copy all the component's ChangeSet files to the
	 * product's RESYNC directory
	 */
	if (mkdirp(cpath)) return (1);
	sccs_mkroot(cpath);
	t = aprintf("%s/BitKeeper/log/COMPONENT", cpath);
	Fprintf(t, "%s\n", cpath);
	free(t);

	to = aprintf("%s/%s", cpath, CHANGESET);
	from = aprintf("%s/%s/%s", RESYNC2ROOT,
	    cpath, CHANGESET);

	if (fileLink(from, to)) {
		fprintf(stderr, "Could not copy '%s' to "
		    "'%s'\n", from, to);
		rc = 1;
	} else {
		if (xfile_exists(from, 'd')) {
			xfile_store(to, 'd', "");
			proj_dirstate(comp, ".", DS_PENDING, 1);
		}
		free(from);
		from = aprintf("%s/%s/BitKeeper/log/TIP", RESYNC2ROOT,
		    cpath);
		free(to);
		to = aprintf("%s/BitKeeper/log/TIP", cpath);
		if (exists(from)) {
			fileLink(from, to);
		} else {
			unlink(to);
		}
	}
	free(from);
	free(to);
	if (rc) return (rc);

	/* update idcache with the changed location */
	idDB = loadDB(IDCACHE, 0, DB_IDCACHE);
	concat_path(buf, cpath, GCHANGESET);
	if (idcache_item(idDB, proj_rootkey(comp), buf)) {
		idcache_write(0, idDB);
	}
	mdbm_close(idDB);

	/* go back where we came from */
	chdir(proj_root(comp));
	proj_free(comp);
	return (rc);
}
