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
#include "logging.h"

extern	char	*bin;
private	void	commit(opts *opts);
private	void	conflict(opts *opts, char *rfile);
private	int	create(resolve *rs);
private	void	edit_tip(resolve *rs, char *sf, delta *d, char *rf, int which);
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
private	void	rename_delta(resolve *rs, char *sf, delta *d, char *rf, int w);
private	int	rename_file(resolve *rs);
private	void	restore(opts *o);
private void	resolve_post(opts *o, int c);
private void	unapply(FILE *f);
private int	copyAndGet(opts *opts, char *from, char *to);
private int	writeCheck(sccs *s, MDBM *db);
private	void	listPendingRenames(void);
private	int	noDiffs(void);
private void	save_checkout_state(MDBM *DB, sccs *s);
private void	restore_checkouts(opts *opts);

private MDBM	*localDB;	/* real name cache for local tree */
private MDBM	*resyncDB;	/* real name cache for resyn tree */

private	int	default_getFlags;

int
resolve_main(int ac, char **av)
{
	int	c;
	int	comment = 0;	/* set if they used -y */
	static	opts opts;	/* so it is zero */
	char	*checkout;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help resolve");
		return (0);
	}

	opts.pass1 = opts.pass2 = opts.pass3 = opts.pass4 = 1;
	setmode(0, _O_TEXT);
	unless (localDB) localDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	unless (resyncDB) resyncDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	while ((c = getopt(ac, av, "l|y|m;aAcdFqrtv1234")) != -1) {
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
		    case 't': opts.textOnly = 1; break;		/* doc 2.0 */
		    case 'y': 					/* doc 2.0 */
			opts.comment = optarg; comment = 1; break;
		    case '1': opts.pass1 = 0; break;		/* doc 2.0 */
		    case '2': opts.pass2 = 0; break;		/* doc 2.0 */
		    case '3': opts.pass3 = 0; break;		/* doc 2.0 */
		    case '4': opts.pass4 = 0; break;		/* doc 2.0 */
		    default:
		    	fprintf(stderr, "resolve: bad opt %c\n", optopt);
			system("bk help -s resolve");
			exit(1);
		}
    	}
	/*
	 * It is the responsibility of the calling code to set this env
	 * var to indicate that we were not run standalone, we are called
	 * from a higher level and they will run the triggers.
	 */
	if (getenv("FROM_PULLPUSH") && streq(getenv("FROM_PULLPUSH"), "YES")) {
		opts.from_pullpush = 1;
	}
	unless (opts.mergeprog) opts.mergeprog = getenv("BK_RESOLVE_MERGEPROG");
	if ((av[optind] != 0) && isdir(av[optind])) {
		chdir(av[optind]);
		if (bk_proj) proj_free(bk_proj);
		bk_proj = proj_init(0);
	}

	if (opts.pass3 && !opts.textOnly && !hasGUIsupport()) {
		opts.textOnly = 1; 
	}
	if (opts.pass3 && !opts.textOnly && !opts.quiet && hasGUIsupport()) {
		fprintf(stderr,
		    "Using %s as graphical display\n", GUI_display());
	}

	if (opts.automerge) {
		unless (comment || opts.comment) opts.comment = "Automerge";
		opts.textOnly = 1;	/* Good idea??? */
	}
	unless (comment || opts.comment) {
		opts.comment = "Merge";
	}

	checkout = user_preference("checkout");
	if (strieq(checkout, "edit")) {
		default_getFlags = GET_EDIT;
	} else if (strieq(checkout, "get")) {
		default_getFlags = GET_EXPAND;
	}

	c = passes(&opts);
	mdbm_close(localDB);
	mdbm_close(resyncDB);
	resolve_post(&opts, c);
	return (c);
}

private void
resolve_post(opts *opts, int c)
{
	if (opts->from_pullpush) return;

	/* XXX - there can be other reasons */
	if (c) {
		putenv("BK_STATUS=CONFLICTS");
	} else {
		putenv("BK_STATUS=OK");
	}
	trigger("resolve", "post");
}

private void
listPendingRenames()
{
	int fd1;

	fprintf(stderr, "List of name conflicts:\n");
	fflush(stderr);
	fd1 = dup(1); dup2(2, 1); /* redirect stdout to stderr */
	system("bk _find . -name 'm.*' | xargs cat");
	dup2(fd1, 1); close(fd1);
}

private void
setRepoType(opts *opts)
{
	if (exists(LOG_TREE)) {
		opts->logging = 1;
		putenv("BK_TRIGGER_PATH=/etc");
	}
}

/*
 * For logging repository, we defer resolving path conflict
 * by moving  the conflicting remote file to the BitKeeper/conflicts
 * directory.
 * XXX - older inode should win so we converge?
 */
private void
removePathConflict(opts *opts, resolve *rs)
{
	char	*t, path[MAXPATH], encpath[MAXPATH];
	int	n = 0;

	if (slotTaken(opts, rs->dname)) {
		sprintf(path, "BitKeeper/conflicts/SCCS/%s", basenm(rs->dname));
		sprintf(encpath, "%s/%s", RESYNC2ROOT, path);
		while (exists(path) || exists(encpath)) {
			sprintf(path, "BitKeeper/conflicts/SCCS/%s~%d",
			    basenm(rs->dname), n++);
			sprintf(encpath, "%s/%s", RESYNC2ROOT, path);
		}
	} else {
		strcpy(path, rs->dname);
	}
	mkdirf(path);
	sccs_close(rs->s); /* for win32 */
	if (rename(rs->s->sfile, path)) {
		perror(path);
		resolve_cleanup(opts, CLEAN_RESYNC);
	}
	if (opts->log) {
		fprintf(opts->log, "rename(%s, %s)\n", rs->s->sfile, path);
	}
	unlink(sccs_Xfile(rs->s, 'm'));
	t = strrchr(path, '/');
	t[1] = 'r';
	/* OK if this fails because the file isn't there */
	rename(sccs_Xfile(rs->s, 'r'), path);
}

/*
 * Do the setup and then work through the passes.
 */
private	int
passes(opts *opts)
{
	sccs	*s = 0;
	char	buf[MAXPATH];
	char	path[MAXPATH];
	FILE	*p;

	/*
	 * Make sure we are where we think we are.  
	 */
	unless (exists("BitKeeper/etc")) sccs_cd2root(0, 0);
	unless (exists("BitKeeper/etc")) {
		fprintf(stderr, "resolve: can't find package root.\n");
		resolve_cleanup(opts, 0);
	}
	unless (exists(ROOT2RESYNC)) {
	    	fprintf(stderr,
		    "resolve: can't find RESYNC dir, nothing to resolve?\n");
		freeStuff(opts);
		exit(0);
	}

	if (exists("SCCS/s.ChangeSet")) {
		unless (opts->idDB =
		    loadDB(IDCACHE, 0, DB_KEYFORMAT|DB_NODUPS)) {
			fprintf(stderr, "resolve: can't open %s\n", IDCACHE);
			resolve_cleanup(opts, 0);
		}
	} else {
		/* fake one for new repositories */
		opts->idDB = mdbm_mem();
	}
	opts->local_proj = proj_init(0);
	chdir(ROOT2RESYNC);
	opts->resync_proj = proj_init(0);
	unless (opts->from_pullpush || opts->quiet) {
		fprintf(stderr,
		    "Verifying consistency of the RESYNC tree...\n");
	}
	unless (opts->from_pullpush || 
	    (sys("bk", "-r", "check", "-cR", SYS) == 0)) {
		syserr("failed.  Resolve not even started.\n");
		/* XXX - better help message */
		resolve_cleanup(opts, 0);
	}

	setRepoType(opts);

	/*
	 * Pass 1 - move files to RENAMES and/or build up rootDB
	 */
	opts->pass = 1;
	unless (p = popen("bk sfiles .", "r")) {
		perror("popen of bk sfiles");
		return (1);
	}
	unless (opts->rootDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
		perror("mdbm_open");
		pclose(p);
		return (1);
	}
	unless (opts->checkoutDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE)) {
		perror("mdbm_open");
		pclose(p);
		return (1);
	}
	if (opts->log) {
		fprintf(opts->log,
		    "==== Pass 1%s ====\n",
		    opts->pass1 ? "" : ": build DB for later passes");
	}
	while (fnext(buf, p)) {
		chop(buf);
		unless (s = sccs_init(buf, INIT, opts->resync_proj)) continue;

		/* save the key for ALL sfiles, RESYNC and RENAMES alike */
		sccs_sdelta(s, sccs_ino(s), path);
		saveKey(opts, path, s->sfile);

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

		sccs_close(s);
		pass1_renames(opts, s);
	}
	pclose(p);
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
		resolve_cleanup(opts, CLEAN_RESYNC);
	}

	/*
	 * Pass 2 - move files back into RESYNC
	 */
	if (opts->pass2) {
		int	old, n = -1;

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
			resolve_cleanup(opts, CLEAN_RESYNC|CLEAN_PENDING);
		}

		if (bk_mode() == BK_BASIC) {
			SHOUT();
			fprintf(stderr,
			   "BitKeeper/Basic should not have rename conflict\n");
			opts->resolveNames = 1;
			pass2_renames(opts);
			resolve_cleanup(opts, CLEAN_RESYNC|CLEAN_PENDING);
		}

		/*
		 * Now do the same thing, calling the resolver.
		 */
		opts->resolveNames = 1;
		n = -1;
		do {
			old = n;
			n = pass2_renames(opts);
		} while (n && ((old == -1) || (n < old)));
		if (n && opts->pass4) {
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
	}

	/*
	 * Pass 3 - resolve content/permissions/flags conflicts.
	 */
pass3:	if (opts->pass3) pass3_resolve(opts);

	/*
	 * Pass 4 - apply files to repository.
	 */
	if (opts->pass4) pass4_apply(opts);
	
	freeStuff(opts);

	/*
	 * Whoohooo...
	 */
	return (0);
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

	/*
	 * Are we in the right sfile? (through LOD shuffling, might not be)
	 * XXX: still true with single tip LOD design
	 */
	sccs_setpathname(s);
	unless (streq(s->spathname, s->sfile)) {
		if (opts->debug) {
			fprintf(stderr, "nameOK(%s) => sfile %s is not same "
			    "path as current top %s\n", s->gfile, s->sfile,
			    s->spathname);
		}
		return (0);
	}

	/*
	 * Same path slot and key?
	 */
	sprintf(path, "%s/%s", RESYNC2ROOT, s->sfile);
	getRealName(path, resyncDB, realname);
	assert(realname);
	assert(strcasecmp(path, realname) == 0);
	if (streq(path, realname) &&
	    (local = sccs_init(path, INIT, opts->local_proj)) &&
	    HAS_SFILE(local)) {
		save_checkout_state(opts->checkoutDB, local);
		if (IS_EDITED(local) && sccs_clean(local, SILENT)) {
			fprintf(stderr,
			    "Warning: %s is modified, will not overwrite it.\n",
			    local->gfile);
			opts->edited = 1;
		}
		sccs_sdelta(local, sccs_ino(local), path);
		sccs_sdelta(s, sccs_ino(s), buf);
		if (opts->debug) {
		    fprintf(stderr,
			"nameOK(%s) => paths match, keys %smatch\n",
			s->gfile, streq(path, buf) ? "" : "don't ");
		}
		sccs_free(local);
		return (streq(path, buf));
	}
	if (local) sccs_free(local);

	chdir(RESYNC2ROOT);
	sccs_sdelta(s, sccs_ino(s), buf);
	local = sccs_keyinit(buf, INIT, opts->local_proj, opts->idDB);
	if (local) {
		save_checkout_state(opts->checkoutDB, local);
		if (IS_EDITED(local) &&
		    sccs_clean(local, SILENT|CLEAN_SHUTUP)) {
			fprintf(stderr,
			    "Warning: %s is modified, will not overwrite it.\n",
			    local->gfile);
			opts->edited = 1;
		}
		sccs_free(local);
		chdir(ROOT2RESYNC);
		if (opts->debug) {
			fprintf(stderr,
			    "nameOK(%s) => keys match, paths don't match\n",
			    s->gfile);
		}
		return (0);
	} else if (exists(s->gfile)) {
		chdir(ROOT2RESYNC);
		if (opts->debug) {
			fprintf(stderr,
			    "nameOK(%s) => no sfile, but has local gfile\n",
			    s->gfile);
		}
		return (0);
	}
	chdir(ROOT2RESYNC);
	if (opts->debug) fprintf(stderr, "nameOK(%s) = 1\n", s->gfile);
	return (1);
}

/*
 * Pass 1 - move to RENAMES
 */
private	void
pass1_renames(opts *opts, sccs *s)
{
	char	path[MAXPATH];
	static	int filenum;
	char	*mfile;
	char	*rfile;

	if (nameOK(opts, s)) {
		unlink(sccs_Xfile(s, 'm'));
		sccs_free(s);
		return;
	}

	unless (filenum) {
		mkdir("BitKeeper/RENAMES", 0777);
		mkdir("BitKeeper/RENAMES/SCCS", 0777);
	}
	do {
		sprintf(path, "BitKeeper/RENAMES/SCCS/s.%d", ++filenum);
	} while (exists(path));
	mfile = strdup(sccs_Xfile(s, 'm'));
	rfile = strdup(sccs_Xfile(s, 'r'));
	sccs_close(s);
	if (opts->debug) {
		fprintf(stderr, "%s -> %s\n", s->sfile, path);
	}
	if (rename(s->sfile, path)) {
		fprintf(stderr, "Unable to rename(%s, %s)\n", s->sfile, path);
		resolve_cleanup(opts, 0);
	} else if (opts->log) {
		fprintf(opts->log, "rename(%s, %s)\n", s->sfile, path);
	}
	if (exists(mfile)) {
		sprintf(path, "BitKeeper/RENAMES/SCCS/m.%d", filenum);
		if (rename(mfile, path)) {
			fprintf(stderr,
			    "Unable to rename(%s, %s)\n", mfile, path);
			resolve_cleanup(opts, 0);
		} else if (opts->log) {
			fprintf(opts->log, "rename(%s, %s)\n", mfile, path);
		}
	}
	if (exists(rfile)) {
		sprintf(path, "BitKeeper/RENAMES/SCCS/r.%d", filenum);
		if (rename(rfile, path)) {
			fprintf(stderr,
			    "Unable to rename(%s, %s)\n", rfile, path);
			resolve_cleanup(opts, 0);
		} else if (opts->log) {
			fprintf(opts->log, "rename(%s, %s)\n", rfile, path);
		}
	}
	sccs_free(s);
	free(mfile);
	free(rfile);
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
	char	path[MAXPATH];
	sccs	*s = 0;
	FILE	*f;
	int	n = 0;
	resolve	*rs;
	char	*t;

	if (opts->debug) fprintf(stderr, "pass2_renames\n");

	unless (exists("BitKeeper/RENAMES/SCCS")) return (0);

	/*
	 * This needs to be an find|sort or the regressions don't pass
	 * because some filesystems do not do FIFO ordering on directory
	 * files (think hashed directories.
	 */
	unless (f =
	    popen("bk _find BitKeeper/RENAMES/SCCS | bk _sort", "r")) {
	    	return (0);
	}
	while (fnext(path, f)) {
		chop(path);
		localName2bkName(path, path);

		/* may have just been deleted it but be in readdir cache */
		unless (exists(path)) continue;

		t = strrchr(path, '/');
		unless (t[1] == 's') continue;

		unless ((s = sccs_init(path, INIT, opts->resync_proj)) &&
		    HASGRAPH(s)) {
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

		if (opts->logging) {
			removePathConflict(opts, rs);
			goto out;
		}

		/*
		 * This code is ripped off from the create path.
		 * We are looking for the case that a directory component is
		 * in use for a file name that we want to put there.
		 * If that's true, then we want to call the resolve_create()
		 * code and fall through if they said EAGAIN or just skip
		 * this one.
		 */
		if (slotTaken(opts, rs->dname) == DIR_CONFLICT) {
			/* If we are just looking for space, skip this one */
			unless (opts->resolveNames) goto out;
			if (opts->noconflicts) {
				fprintf(stderr,
				    "resolve: dir/file conflict for ``%s'',\n",
				    rs->d->pathname);
				goto out;
			}
			if (resolve_create(rs, DIR_CONFLICT) != EAGAIN) {
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
			create(rs);
		}
out:		resolve_free(rs);
	}
	pclose(f);
	return (n);
}

/*
 * Extract the names from either a
 * m.file: rename local_name gca_name remote_name
 * or an
 * r.file: merge deltas 1.491 1.489 1.489.1.21 lm 00/01/08 10:39:11
 */
names	*
getnames(char	*path, int type)
{
	FILE	*f = fopen(path, "r");
	names	*names = 0;
	char	*s, *t;
	char	buf[MAXPATH*3];

	unless (f) return (0);
	unless (fnext(buf, f)) {
out:		fclose(f);
		freenames(names, 1);
		return (0);
	}
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
	fclose(f);
	return (names);
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
	static	char *cnames[4] = {
			"Huh?",
			"an SCCS file",
			"a regular file without an SCCS file",
			"another SCCS file in the patch"
		};

	if (opts->debug) {
		fprintf(stderr,
		    ">> create(%s) in pass %d.%d\n",
		    rs->d->pathname, opts->pass, opts->resolveNames);
	}

	/* 
	 * See if this name is taken in the repository.
	 */
again:	if (how = slotTaken(opts, rs->dname)) {
		/* If we are just looking for space, skip this one */
		unless (opts->resolveNames) return (-1);

		if (opts->noconflicts) {
	    		fprintf(stderr,
			    "resolve: name conflict for ``%s'',\n",
			    rs->d->pathname);
	    		fprintf(stderr,
			    "\tpathname is used by %s.\n", cnames[how]);
			opts->errors = 1;
			return (-1);
		}

		/*
		 * If this is the BitKeeper/etc/logging_ok file,
		 * automerge it.
		 */
		if (streq(GLOGGING_OK, rs->d->pathname)) {
			ret = resolve_create(rs, LOGGING_OK_CONFLICT);
		} else {
			ret = resolve_create(rs, how);
		}
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
	local = sccs_keyinit(buf, INIT, opts->local_proj, opts->idDB);
	chdir(ROOT2RESYNC);
	if (local) {
		if (opts->debug) {
			fprintf(stderr, "%s already renamed to %s\n",
			    local->gfile, rs->d->pathname);
		}
		/* dummy up names which make it a remote move */
		if (rs->gnames) freenames(rs->gnames, 1);
		if (rs->snames) freenames(rs->snames, 1);
		rs->gnames	   = calloc(1, sizeof(names));
		rs->gnames->local  = strdup(local->gfile);
		rs->gnames->gca    = strdup(local->gfile);
		rs->gnames->remote = strdup(rs->d->pathname);
		rs->snames	   = calloc(1, sizeof(names));
		rs->snames->local  = name2sccs(rs->gnames->local);
		rs->snames->gca    = name2sccs(rs->gnames->gca);
		rs->snames->remote = name2sccs(rs->gnames->remote);
		sccs_free(local);
		assert(!exists(sccs_Xfile(rs->s, 'm')));
		return (rename_file(rs));
	}

	/*
	 * OK, looking like a real create.
	 */
	sccs_close(rs->s);
	if (ret = rename(rs->s->sfile, rs->dname)) {
		mkdirf(rs->dname);
		ret = rename(rs->s->sfile, rs->dname);
	}
	if (rs->opts->log) {
		fprintf(rs->opts->log, "rename(%s, %s) = %d\n", 
		    rs->s->gfile, rs->d->pathname, ret);
	}
	if (opts->debug) {
		fprintf(stderr,
		    "%s -> %s = %d\n", rs->s->gfile, rs->d->pathname, ret);
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
	char	*to;
	int	how = 0;

again:
	if (opts->debug) {
		fprintf(stderr, ">> rename_file(%s)\n", rs->d->pathname);
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

	/* XXX: resolve across LODs can have this assert fail 
	 * remote: new lod, then rename, then commit
	 * pull back in, sfile has new name, but in this lod has no rename
	 * used to have:
	 * -> assert(!streq(rs->gnames->local, rs->gnames->remote));
	 */

	/*
	 * See if we can just slide the file into place.
	 * If remote moved, and local didn't, ok if !slotTaken.
	 * If local is moved and remote isn't, ok if RESYNC slot is open.
	 */
	if (streq(rs->snames->local, rs->snames->gca)) {
		to = rs->snames->remote;
		if (how = slotTaken(opts, to)) to = 0;
	} else if (streq(rs->snames->gca, rs->snames->remote)) {
		to = rs->snames->local;
		/* XXX - I'm not sure that this makes sense. */
		if (exists(to)) to = 0;
	} else {
		to = 0;
	}
	if (to) {
		sccs_close(rs->s); /* for win32 */
		if (rename(rs->s->sfile, to)) {
			mkdirf(to);
			if (rename(rs->s->sfile, to)) return (-1);
		}
		if (rs->revs) {
			delta	*d;
			char	rfile[MAXPATH];
			char	*t = strrchr(to, '/');

			t[1] = 'r';
			strcpy(rfile, to);
			t[1] = 's';
			if (rename(sccs_Xfile(rs->s, 'r'), rfile)) return (-1);
			if (rs->opts->log) {
				fprintf(rs->opts->log,
				    "rename(%s, %s)\n",
				    sccs_Xfile(rs->s, 'r'), rfile);
			}
			rs->s = sccs_restart(rs->s); 
			assert(rs->s);
			d = sccs_getrev(rs->s, rs->revs->local, 0, 0);
			assert(d);
			t = name2sccs(d->pathname);
			unless (streq(t, to)) {
				rename_delta(rs, to, d, rfile, LOCAL);
			}
			free(t);
			d = sccs_getrev(rs->s, rs->revs->remote, 0, 0);
			assert(d);
			t = name2sccs(d->pathname);
			unless (streq(t, to)) {
				rename_delta(rs, to, d, rfile, REMOTE);
			}
			free(t);
		}
		if (rs->opts->log) {
			fprintf(rs->opts->log,
			    "rename(%s, %s)\n", rs->s->sfile, to);
		}
		opts->renames2++;
		unlink(sccs_Xfile(rs->s, 'm'));
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
	delta	*d;
	char	rfile[MAXPATH];

	if (rs->opts->debug) {
		fprintf(stderr, "move_remote(%s, %s)\n", rs->s->sfile, sfile);
	}

	sccs_close(rs->s);
	if (ret = rename(rs->s->sfile, sfile)) {
		mkdirf(sfile);
		if (ret = rename(rs->s->sfile, sfile)) return (ret);
	}
	unlink(sccs_Xfile(rs->s, 'm'));
	if (rs->opts->resolveNames) rs->opts->renames2++;
	if (rs->opts->log) {
		fprintf(rs->opts->log, "rename(%s, %s)\n", rs->s->sfile, sfile);
	}

	/*
	 * If we have revs, then there is an r.file, so move it too.
	 * And delta the tips if they need it.
	 */
	if (rs->revs) {
		char	*t = strrchr(sfile, '/');

		t[1] = 'r';
		strcpy(rfile, sfile);
		t[1] = 's';
		if (ret = rename(sccs_Xfile(rs->s, 'r'), rfile)) return (ret);
		if (rs->opts->log) {
			fprintf(rs->opts->log,
			    "rename(%s, %s)\n", sccs_Xfile(rs->s, 'r'), rfile);
		}
		d = sccs_getrev(rs->s, rs->revs->local, 0, 0);
		assert(d);
		t = name2sccs(d->pathname);
		unless (streq(t, sfile)) {
			rename_delta(rs, sfile, d, rfile, LOCAL);
		}
		free(t);
		d = sccs_getrev(rs->s, rs->revs->remote, 0, 0);
		assert(d);
		t = name2sccs(d->pathname);
		unless (streq(t, sfile)) {
			rename_delta(rs, sfile, d, rfile, REMOTE);
		}
		free(t);
	} else unless (streq(rs->dname, sfile)) {
		rename_delta(rs, sfile, rs->d, 0, 0);
	}
	/* Nota bene: *s is may be out of date */
	return (0);
}

/*
 * Add a null rename delta to the specified tip.
 */
private	void
rename_delta(resolve *rs, char *sfile, delta *d, char *rfile, int which)
{
	char	*t;
	char	buf[MAXPATH+100];

	if (rs->opts->debug) {
		fprintf(stderr, "rename_delta(%s, %s, %s, %s)\n", sfile,
		    d->rev, d->pathname, which == LOCAL ? "local" : "remote");
	}
	edit_tip(rs, sfile, d, rfile, which);
	t = sccs2name(sfile);
	sprintf(buf, "-PyMerge rename: %s -> %s", d->pathname, t);
	free(t);
	if (rs->opts->log) {
		sys("bk", "delta", buf, sfile, SYS);
	} else {
		sys("bk", "delta", "-q", buf, sfile, SYS);
	}
}

/*
 * Resolve a file type by taking the file the winning delta and checking it
 * over the losing delta.
 * winner == REMOTE means the local is delta-ed with the remote's data.
 */
void
type_delta(resolve *rs,
	char *sfile, delta *l, delta *r, char *rfile, int winner)
{
	char	buf[MAXPATH+100];
	char	*g = sccs2name(sfile);
	delta	*o, *n;		/* old -> new */
	sccs	*s;
	int	loser;

	if (rs->opts->debug) {
		fprintf(stderr, "type(%s, %s, %s, %s)\n", sfile,
		    l->rev, r->rev, winner == LOCAL ? "local" : "remote");
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
	edit_tip(rs, sfile, o, rfile, loser);
	if (S_ISREG(n->mode)) {
		/* bk _get -kpqr{n->rev} sfile > g */
		sprintf(buf, "-kpqr%s", n->rev);
		if (sysio(0, g, 0, "bk", "_get", buf, sfile, SYS)) {
			fprintf(stderr, "%s failed\n", buf);
			resolve_cleanup(rs->opts, 0);
		}
		chmod(g, n->mode);
	} else if (S_ISLNK(n->mode)) {
		assert(n->symlink);
		if (symlink(n->symlink, g)) {
			perror(g);
			resolve_cleanup(rs->opts, 0);
		}
	} else {
		fprintf(stderr,
		    "type_delta called on unknown file type %o\n", n->mode);
		resolve_cleanup(rs->opts, 0);
	}
	free(g);
	/* bk delta -qPy'Merge file types: {o->mode} {n->mode} sfile */
	sprintf(buf, "-qPyMerge file types: %s -> %s", 
	    mode2FileType(o->mode), mode2FileType(n->mode));
	if (sys("bk", "delta", buf, sfile, SYS)) {
		syserr("failed\n");
		resolve_cleanup(rs->opts, 0);
	}
	strcpy(buf, sfile);	/* it's from the sccs we are about to free */
	sfile = buf;
	sccs_free(rs->s);
	s = sccs_init(sfile, INIT, rs->opts->resync_proj);
	assert(s);
	if (rs->opts->debug) {
		fprintf(stderr, "type reopens %s (%s)\n", sfile, s->gfile);
	}
	rs->s = s;
	rs->d = sccs_getrev(s, "+", 0, 0);
	assert(rs->d);
	free(rs->dname);
	rs->dname = name2sccs(rs->d->pathname);
}

/*
 * Add a null permissions delta to the specified tip.
 */
void
mode_delta(resolve *rs, char *sfile, delta *d, mode_t m, char *rfile, int which)
{
	char	*a = mode2a(m);
	char	buf[MAXPATH], opt[100];
	sccs	*s;

	if (rs->opts->debug) {
		fprintf(stderr, "mode(%s, %s, %s, %s)\n",
		    sfile, d->rev, a, which == LOCAL ? "local" : "remote");
	}
	edit_tip(rs, sfile, d, rfile, which);
	/* bk delta [-q] -Py'Change mode to {a}' -M{a} sfile */
	sprintf(buf, "-PyChange mode to %s", a);
	sprintf(opt, "-M%s", a);
	if (sys("bk",
	    "delta", rs->opts->log ? "" : "-q", buf, opt, sfile, SYS)) {
		syserr("failed\n");
		resolve_cleanup(rs->opts, 0);
	}
	strcpy(buf, sfile);	/* it's from the sccs we are about to free */
	sfile = buf;
	sccs_free(rs->s);
	s = sccs_init(sfile, INIT, rs->opts->resync_proj);
	assert(s);
	if (rs->opts->debug) {
		fprintf(stderr, "mode reopens %s (%s)\n", sfile, s->gfile);
	}
	rs->s = s;
	rs->d = sccs_getrev(s, "+", 0, 0);
	assert(rs->d);
	free(rs->dname);
	rs->dname = name2sccs(rs->d->pathname);
}

/*
 * Add a null flags delta to the specified tip.
 */
void
flags_delta(resolve *rs,
	char *sfile, delta *d, int flags, char *rfile, int which)
{
	char	*av[40];
	char	buf[MAXPATH];
	char	fbuf[10][25];	/* Must match list below */
	int	n, i, f;
	sccs	*s;
	int	bits = flags & X_MAYCHANGE;

	if (rs->opts->debug) {
		fprintf(stderr, "flags(%s, %s, 0x%x, %s)\n",
		    sfile, d->rev, bits, which == LOCAL ? "local" : "remote");
	}
	edit_tip(rs, sfile, d, rfile, which);
	sys("bk", "clean", sfile, SYS);
	av[n=0] = "bk";
	av[++n] = "admin";
	sprintf(buf, "-qr%s", d->rev);
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
	doit(X_NOMERGE, "NOMERGE");
	for (i = 0; i < f; ++i) av[++n] = fbuf[i];
	av[++n] = sfile;
	assert(n < 38);	/* matches 40 in declaration */
	av[++n] = 0;

	n = spawnvp_ex(_P_WAIT, av[0], av);
	if (!WIFEXITED(n) || WEXITSTATUS(n)) {
		for (i = 0; av[i]; ++i) fprintf(stderr, "%s ", av[i]);
		fprintf(stderr, "failed\n");
		resolve_cleanup(rs->opts, 0);
	}
	strcpy(buf, sfile);	/* it's from the sccs we are about to free */
	sfile = buf;
	sccs_free(rs->s);
	s = sccs_init(sfile, INIT, rs->opts->resync_proj);
	assert(s);
	if (rs->opts->debug) {
		fprintf(stderr, "flags reopens %s (%s)\n", sfile, s->gfile);
	}
	rs->s = s;
	rs->d = sccs_getrev(s, "+", 0, 0);
	assert(rs->d);
	free(rs->dname);
	rs->dname = name2sccs(rs->d->pathname);
}

/*
 * Set up to add a delta to the file, such as a rename or mode delta.
 * If which is set, update the r.file with the new data.
 *	which == LOCAL means do local rev, REMOTE means do remote rev.
 * If the which value is negative, it means don't get the data.
 */
private	void
edit_tip(resolve *rs, char *sfile, delta *d, char *rfile, int which)
{
	char	buf[MAXPATH+100];
	char	opt[100];
	FILE	*f;
	char	*t;
	char	*newrev;

	if (rs->opts->debug) {
		fprintf(stderr, "edit_tip(%s %s %s)\n",
		    sfile, d->rev, abs(which) == LOCAL ? "local" : "remote");
	}
	/* bk _get -e[g][q] -r{d->rev} sfile */
	sprintf(buf, "-e%s%s", which < 0 ? "g" : "", rs->opts->log ? "" : "q");
	sprintf(opt, "-r%s", d->rev);
	if (sys("bk", "_get", buf, opt, sfile, SYS)) {
		syserr("failed\n");
		resolve_cleanup(rs->opts, 0);
	}
	if (which) {
		t = strrchr(sfile, '/');
		assert(t && (t[1] == 's'));
		t[1] = 'p';
		f = fopen(sfile, "r");
		t[1] = 's';
		fnext(buf, f);
		fclose(f);
		newrev = strchr(buf, ' ');
		newrev++;
		t = strchr(newrev, ' ');
		*t = 0;
		f = fopen(rfile, "w");
		/* 0123456789012
		 * merge deltas 1.9 1.8 1.8.1.3 lm 00/01/15 00:25:18
		 */
		if (abs(which) == LOCAL) {
			free(rs->revs->local);
			rs->revs->local = strdup(newrev);
		} else {
			free(rs->revs->remote);
			rs->revs->remote = strdup(newrev);
		}
		fprintf(f, "merge deltas %s %s %s\n",
		    rs->revs->local, rs->revs->gca, rs->revs->remote);
		fclose(f);
	}
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


/*
 * Return 1 if the pathname in question is in use in the repository by
 * an SCCS file that is not already in the RESYNC/RENAMES dirs.
 * Return 2 if the pathname in question is in use in the repository by
 * a file without an SCCS file.
 * Return 3 if the pathname in question is in use in the RESYNC dir.
 */
int
slotTaken(opts *opts, char *slot)
{
	if (opts->debug) fprintf(stderr, "slotTaken(%s) = ", slot);

	if (exists(slot)) {
		if (opts->debug) fprintf(stderr, "%s exists in RESYNC\n", slot);
		return (RESYNC_CONFLICT);
	}

	chdir(RESYNC2ROOT);
	if (exists(slot)) {
		char	buf2[MAXKEY];
		sccs	*local = sccs_init(slot, INIT, opts->local_proj);

		/*
		 * If we can find this key in the RESYNC dir then it is
		 * not a conflict, the file has been successfully moved.
		 */
		sccs_sdelta(local, sccs_ino(local), buf2);
		sccs_free(local);
		unless (mdbm_fetch_str(opts->rootDB, buf2)) {
			if (opts->debug) {
				fprintf(stderr,
				    "%s exists in local repository\n", slot);
			}
			chdir(ROOT2RESYNC);
			return (SFILE_CONFLICT);
		}
	} else {
		char	*gfile = sccs2name(slot);

		if (exists(gfile)) {
			int	conf;

			conf = isdir(gfile) ? DIR_CONFLICT : GFILE_CONFLICT;
			if (opts->debug) {
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

/* ---------------------------- Pass3 stuff ---------------------------- */

private int
noDiffs()
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
	char	buf[MAXPATH], s_cset[] = CHANGESET;
	FILE	*p;
	int	n = 0;
	int	mustCommit, pc, pe;

	if (opts->log) fprintf(opts->log, "==== Pass 3 ====\n");
	opts->pass = 3;

	/*
	 * Make sure the renames are done first.
	 * XXX - doesn't look in RENAMES, should it?
	 */
	unless (p = popen("bk _find . -name 'm.*'", "r")) {
		perror("popen of find");
		resolve_cleanup(opts, 0);
	}
	while (fnext(buf, p)) {
		fprintf(stderr, "Needs rename: %s", buf);
		n++;
	}
	pclose(p);
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
	unless (p = popen("bk sfiles .", "r")) {
		perror("popen of sfiles");
		resolve_cleanup(opts, 0);
	}
	while (fnext(buf, p)) {
		char	*t = strrchr(buf, '/');

		if (opts->debug) fprintf(stderr, "pass3: %s", buf);

		chop(buf);
		assert(t[1] == 's');
		t[1] = 'r';
		unless (exists(buf)) continue;
		t[1] = 's';

		if (streq("SCCS/s.ChangeSet", buf)) continue;

		/*
		 * We leave the r.files there but ignore them if we've
		 * already merged these files.
		 * We also allow you to remerge if you want.
		 */
		unless (opts->remerge) {
			t[1] = 'p';
			if (exists(buf)) continue;
			t[1] = 's';
		}
		if (opts->logging) {
			t[1] = 'r';
			unlink(buf);
			t[1] = 's';
		} else {
			conflict(opts, buf);
		}
		if (opts->errors) {
			pclose(p);
			goto err;
		}
	}
	pclose(p);
	unless (opts->logging) resolve_tags(opts);
	unless (opts->quiet || !opts->resolved) {
		fprintf(stdlog,
		    "resolve: resolved %d conflicts in pass 3\n",
		    opts->resolved);
	}

	if (opts->errors) {
err:		fprintf(stderr, "resolve: had errors, nothing is applied.\n");
		resolve_cleanup(opts, 0);
	}

	if (opts->hadConflicts) {
		fprintf(stderr,
		    "resolve: %d unresolved conflicts, nothing is applied.\n",
		    opts->hadConflicts);
		resolve_cleanup(opts, 0);
	}

	/*
	 * Get -M the ChangeSet file if we need to, before calling citool
	 * or commit.
	 */
	if (mustCommit = exists("SCCS/r.ChangeSet")) {
		sccs	*s;
		resolve	*rs;
		
		unless (opts->logging) {
			s = sccs_init(s_cset, INIT, opts->resync_proj);
			rs = resolve_init(opts, s);
			edit(rs);
			/* We may restore it if we bail early */
			rename(sccs_Xfile(s, 'r'), "BitKeeper/tmp/r.ChangeSet");
			resolve_free(rs);
			opts->willMerge = 1;
		}
	} else if (exists("SCCS/p.ChangeSet")) {
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
	unless (p = popen("bk sfiles .", "r")) {
		perror("popen of sfiles");
		resolve_cleanup(opts, 0);
	}
	while (fnext(buf, p)) {
		char	*t = strrchr(buf, '/');

		chop(buf);
		assert(t[1] == 's');
		t[1] = 'r';
		unless (exists(buf)) continue;
		unlink(buf);
	}
	pclose(p);

	/*
	 * For logging repository, do not commit merge node
	 */ 
	if (opts->logging) return (0);


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
	 * Go ask about logging if we need to.  We never ask on a push.
	 */
	unless (pe && pendingEdits()) {
		if (opts->log) {
			fprintf(opts->log, "==== Pass 3 autocommits ====\n");
		}
		unless (opts->comment || pending(1)) opts->comment = "Merge";
		unless (opts->noconflicts) ok_commit(0);
		commit(opts);
		return (0);
	}
	
	/*
	 * Unless we are in textonly mode, let citool do all the work.
	 */
	unless (opts->textOnly) {
		if (sys("bk", "citool", "-R", SYS)) {
			syserr("failed, aborting.\n");
			resolve_cleanup(opts, 0);
		}
		if (pending(0) || pendingEdits()) {
			fprintf(stderr,
			    "Failed to check in/commit all files, aborting.\n");
			resolve_cleanup(opts, 0);
		}
		opts->didMerge = opts->willMerge;
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

	if (pending(0)) {
		assert(!opts->noconflicts);
		ok_commit(0);
		commit(opts);
	}

	return (0);
}

void
do_delta(opts *opts, sccs *s, char *comment)
{
	int     flags = DELTA_FORCE;
	delta	*d = 0;

	comments_done();
	if (comment) {
		comments_save(comment);
		d = comments_get(0);
		flags |= DELTA_DONTASK;
	}
	if (opts->quiet) flags |= SILENT;
	sccs_restart(s);
	if (sccs_delta(s, flags, d, 0, 0, 0)) {
		fprintf(stderr, "Delta of %s failed\n", s->gfile);
		resolve_cleanup(opts, 0);
	}
}

private void
checkins(opts *opts, char *comment)
{
	sccs	*s;
	FILE	*p;
	char	buf[MAXPATH];

	unless (p = popen("bk sfiles -c", "r")) {
		perror("popen of find");
		resolve_cleanup(opts, 0);
	}
	while (fnext(buf, p)) {
		chop(buf);
		s = sccs_init(buf, INIT, opts->resync_proj);
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
 * XXX - we need to handle descriptive text, lods, and symbols.
 * The symbol case is weird: a conflict is when the same symbol name
 * is in both branches without one below the trunk.
 */
private	void
conflict(opts *opts, char *sfile)
{
	sccs	*s;
	resolve	*rs;
	deltas	d;
	
	s = sccs_init(sfile, INIT, opts->resync_proj);

	rs = resolve_init(opts, s);
	assert(streq(rs->dname, s->sfile));

	d.local = sccs_getrev(s, rs->revs->local, 0, 0);
	d.gca = sccs_getrev(s, rs->revs->gca, 0, 0);
	d.remote = sccs_getrev(s, rs->revs->remote, 0, 0);
	/*
	 * The smart programmer will notice that the case where both sides
	 * changed the mode to same thing is just silently merged below.
	 *
	 * The annoyed programmer will note that the resolver may
	 * replace the sccs pointer.
	 */
	unless (fileType(d.local->mode) == fileType(d.remote->mode)) {
		rs->opaque = (void*)&d;
		resolve_filetypes(rs);
		s = rs->s;
		if (opts->errors) return;
		/* Need to re-init these, the resolve stomped on the s-> */
		d.local = sccs_getrev(s, rs->revs->local, 0, 0);
		d.gca = sccs_getrev(s, rs->revs->gca, 0, 0);
		d.remote = sccs_getrev(s, rs->revs->remote, 0, 0);
	}

	unless (d.local->mode == d.remote->mode) {
		rs->opaque = (void*)&d;
		resolve_modes(rs);
		s = rs->s;
		if (opts->errors) return;
		/* Need to re-init these, the resolve stomped on the s-> */
		d.local = sccs_getrev(s, rs->revs->local, 0, 0);
		d.gca = sccs_getrev(s, rs->revs->gca, 0, 0);
		d.remote = sccs_getrev(s, rs->revs->remote, 0, 0);
	}

	/*
	 * Merge changes to the flags (RCS/SCCS/EXPAND1/etc).
	 */
	unless (sccs_xflags(d.local) == sccs_xflags(d.remote)) {
		rs->opaque = (void*)&d;
		resolve_flags(rs);
		s = rs->s;
		if (opts->errors) return;
		/* Need to re-init these, the resolve stomped on the s-> */
		d.local = sccs_getrev(s, rs->revs->local, 0, 0);
		d.gca = sccs_getrev(s, rs->revs->gca, 0, 0);
		d.remote = sccs_getrev(s, rs->revs->remote, 0, 0);
	}

	if (opts->debug) {
		fprintf(stderr, "Conflict: %s: %s %s %s\n",
		    s->gfile,
		    rs->revs->local, rs->revs->gca, rs->revs->remote);
	}
	if (opts->noconflicts) {
	    	fprintf(stderr,
		    "resolve: can't process conflict in %s\n", s->gfile);
err:		resolve_free(rs);
		opts->errors = 1;
		return;
	}

	/*
	 * See if we merged some symlink conflicts, if so
	 * there can't be any conflict left.
	 */
	if (S_ISLNK(fileType(d.local->mode))) {
		int	flags;
		delta	*e;

		assert(d.local->mode == d.remote->mode);
		if (sccs_get(rs->s, 0, 0, 0, 0, SILENT, "-")) {
			sccs_whynot("delta", rs->s);
			goto err;
		}
		if (!IS_LOCKED(rs->s) && edit(rs)) goto err;
		comments_save("Auto merged");
		e = comments_get(0);
		sccs_restart(rs->s);
		flags = DELTA_DONTASK|DELTA_FORCE|(rs->opts->quiet? SILENT : 0);
		if (sccs_delta(rs->s, flags, e, 0, 0, 0)) {
			sccs_whynot("delta", rs->s);
			goto err;
		}
	    	rs->opts->resolved++;
		unlink(sccs_Xfile(rs->s, 'r'));
		resolve_free(rs);
		return;
	}

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
	int	flags = PRINT | (rs->opts->debug ? 0 : SILENT);

	if (sccs_get(rs->s, rs->revs->local, 0, 0, 0, flags, n->local)) {
		fprintf(stderr, "Unable to get %s\n", n->local);
		return (-1);
	}

	if (sccs_get(rs->s, rs->revs->gca, 0, 0, 0, flags, n->gca)) {
		fprintf(stderr, "Unable to get %s\n", n->gca);
		return (-1);
	}
	if (sccs_get(rs->s, rs->revs->remote, 0, 0, 0, flags, n->remote)) {
		fprintf(stderr, "Unable to get %s\n", n->remote);
		return (-1);
	}
	return (0);
}

/*
 * Try to automerge.
 */
void
automerge(resolve *rs, names *n, int identical)
{
	char	cmd[MAXPATH*4];
	int	ret;
	char	*name = basenm(rs->d->pathname);
	names	tmp;
	int	do_free = 0;
	int	flags;
	
	if (rs->opts->debug) fprintf(stderr, "automerge %s\n", name);

	unless (n) {
		sprintf(cmd, "BitKeeper/tmp/%s@%s", name, rs->revs->local);
		tmp.local = strdup(cmd);
		sprintf(cmd, "BitKeeper/tmp/%s@%s", name, rs->revs->gca);
		tmp.gca = strdup(cmd);
		sprintf(cmd, "BitKeeper/tmp/%s@%s", name, rs->revs->remote);
		tmp.remote = strdup(cmd);
		if (get_revs(rs, &tmp)) {
			rs->opts->errors = 1;
			freenames(&tmp, 0);
			return;
		}
		n = &tmp;
		do_free = 1;
	}

	if (identical || sameFiles(n->local, n->remote)) {
		assert(n);
		sys("cp", n->local, rs->s->gfile, SYS);
		goto same;
	}

	if (rs->s->encoding & E_BINARY) {
		unless (rs->opts->quiet) {
			fprintf(stderr,
			    "Not automerging binary '%s'\n", rs->s->gfile);
		}
nomerge:	rs->opts->hadConflicts++;
		unlink(rs->s->gfile);
		return;
	}
	if (NOMERGE(rs->s)) goto nomerge;

	/*
	 * The interface to the merge program is
	 * "smerge left_ver gca_ver right_ver"
	 * and the program must return as follows:
	 * 0 for no overlaps, 1 for some overlaps, 2 for errors.
	 */
	if (rs->opts->mergeprog) {
		ret = sys("bk", rs->opts->mergeprog,
		    n->local, n->gca, n->remote, rs->s->gfile, SYS);
	} else {
		ret = sysio(0, rs->s->gfile, 0, "bk", "smerge", rs->s->gfile, 
		    rs->revs->local, rs->revs->remote, SYS);
	}
		
	if (do_free) {
		unlink(tmp.local);
		unlink(tmp.gca);
		unlink(tmp.remote);
		freenames(&tmp, 0);
	}
	if (ret == 0) {
		delta	*d;

		unless (rs->opts->quiet) {
			fprintf(stderr,
			    "Content merge of %s OK\n", rs->s->gfile);
		}
same:		if (!IS_LOCKED(rs->s) && edit(rs)) return;
		comments_save("Auto merged");
		d = comments_get(0);
		sccs_restart(rs->s);
		flags = DELTA_DONTASK|DELTA_FORCE|(rs->opts->quiet? SILENT : 0);
		if (sccs_delta(rs->s, flags, d, 0, 0, 0)) {
			sccs_whynot("delta", rs->s);
			rs->opts->errors = 1;
			return;
		}
	    	rs->opts->resolved++;
		unlink(sccs_Xfile(rs->s, 'r'));
		return;
	}
	unless (WIFEXITED(ret)) {
		fprintf(stderr, "Unknown merge status: 0x%x\n", ret);
		rs->opts->errors = 1;
		unlink(rs->s->gfile);
		return;
	}
	if (WEXITSTATUS(ret) == 1) {
		fprintf(stderr,
		    "Conflicts during automerge of %s\n", rs->s->gfile);
		rs->opts->hadConflicts++;
		unlink(rs->s->gfile);
		return;
	}
	fprintf(stderr,
	    "Automerge of %s failed for unknown reasons\n", rs->s->gfile);
	rs->opts->errors = 1;
	unlink(rs->s->gfile);
	return;
}

/*
 * Figure out which delta is the branch one and merge it in.
 */
int
edit(resolve *rs)
{
	int	flags = GET_EDIT|GET_SKIPGET;
	char	*branch;

	branch = strchr(rs->revs->local, '.');
	assert(branch);
	if (strchr(++branch, '.')) {
		branch = rs->revs->local;
	} else {
		branch = rs->revs->remote;
	}
	if (rs->opts->quiet) flags |= SILENT;
	if (sccs_get(rs->s, 0, branch, 0, 0, flags, "-")) {
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
pendingRenames()
{
	DIR	*dir;
	struct dirent *d;
	int	n = 0;

	unless (dir = opendir("BitKeeper/RENAMES/SCCS")) return (0);
	while (d = readdir(dir)) {
		if (streq(d->d_name, ".") || streq(d->d_name, "..")) continue;
		fprintf(stderr,
		    "Pending: BitKeeper/RENAMES/SCCS/%s\n", d->d_name);
		n++;
	}
	closedir(dir);
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
		f = popen("bk sfiles -pC", "r");
		ret = fgetc(f) != EOF;
		pclose(f);
		return (ret);
	}
	f = popen("bk sfiles -pCA | bk sccslog -CA -", "r");
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
pendingEdits()
{
	char	buf[MAXPATH];
	FILE	*f;

	f = popen("bk sfiles -c", "r");

	while (fnext(buf, f)) {
		chop(buf);
		/* Huh? */
		if (streq("SCCS/s.ChangeSet", buf)) continue;
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
	char	*cmds[10], *cmt = 0;

	unless (ok_commit(1)) {
		fprintf(stderr,
		   "Commit aborted because of licensing, no changes applied\n");
		resolve_cleanup(opts, 0);
	}

	cmds[i = 0] = "bk";
	cmds[++i] = "commit";
	cmds[++i] = "-RFa";
	if (opts->quiet) cmds[++i] = "-s";
	if (opts->comment) {
		cmt = malloc(strlen(opts->comment) + 10);

		sprintf(cmt, "-y%s", opts->comment);
		cmds[++i] = cmt;
	}
	cmds[++i] = 0;
	i = spawnvp_ex(_P_WAIT, "bk", cmds);
	if (WIFEXITED(i) && !WEXITSTATUS(i)) {
		if (cmt) free(cmt);
		opts->didMerge = opts->willMerge;
		return;
	}
	if (cmt) free(cmt);
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
 * Remove a sfile, if its parent is empty, remove them too.
 * We leave stuff in place if we are being called from apply;
 * if we are called from unapply, we try and clean up everything.
 */
private void
rm_sfile(char *sfile, int leavedirs)
{
	char	*p;

	assert(!IsFullPath(sfile));
	sys("bk", "clean", sfile, SYS);
	if (unlink(sfile)) {
		perror(sfile);
		return;
	}
	p = strrchr(sfile, '/');
	unless (p) return;
	for (;;) {
		*p-- = 0;
		if (isdir(sfile)) {
			char	rdir[MAXPATH];

			if (leavedirs) {
				sprintf(rdir, "%s/%s", ROOT2RESYNC, sfile);
				if (isdir(rdir)) break;
			}
			if (emptyDir(sfile)) rmdir(sfile); /* careful */
		}
		while ((p > sfile) && (*p != '/')) p--;
		if (p >= sfile) continue;
		break;
	}
}

/*
 * Return true if we detected a case folding File system
 */
private int
isCaseFoldingFS(void)
{
	char	s_cset[] = CHANGESET;
	char	*p;

	p = strrchr(s_cset, '/');
	assert(p && (p[1] == 's'));
	p[1] = 'S';  /* change to upper case */
	return (exists(s_cset));
}

/*
 * Make sure the file name did not change case orientation after we copied it
 */
private int
chkCaseChg(FILE *f)
{
	char	buf[MAXPATH], realname[MAXPATH];

	fflush(f);
	rewind(f);
	while (fnext(buf, f)) {
		chomp(buf);
		getRealName(buf, localDB, realname);
		unless (streq(buf, realname)) {
			char	*case_folding_err =
"\n\
============================================================================\n\
BitKeeper has detected a \"Case-Folding file system\". e.g. FAT and NTFS.\n\
What this means is that your file system ignores case differences when it looks\n\
for directories and files. This also means that it is not possible to rename\n\
a path correctly if there exists a similar path with only upper/lower case\n\
differences.\n\
BitKeeper wants to rename:\n\
    %s -> %s\n\
Your file system is changing it to:\n\
    %s -> %s\n\
BitKeeper considers this an error, since this may not be what you have\n\
intended.  The recommended work around for this problem is as follows:\n\
a) Exit from this resolve session.\n\
b) Run \"bk mv\" to move the directory or file with upper/lower case\n\
   changes to a temporary location.\n\
c) Run \"bk mv\" again to move from the temporary location to\n\
   %s\n\
d) Run \"bk commit\" to record the new location in a changeset.\n\
e) Run \"bk resolve\" or \"bk pull\" again.\n\
f) You should also inform owners of other repositories to avoid using path\n\
   of similar names.\n\
============================================================================\n";
			char	*unknown_err =
"\n\
============================================================================\n\
Unknown rename error, wanted:\n\
    %s -> %s\n\
Got:\n\
    %s -> %s\n\
============================================================================\n";
			if (strcasecmp(buf, realname) == 0) {
				fprintf(stderr, case_folding_err, buf,
				    		buf, buf, realname, buf);
			} else {
				fprintf(stderr, unknown_err,
						buf, buf, buf, realname);
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
	int	eperm = 0, first = 1, flags, ret;
	FILE	*f = 0;
	FILE	*save = 0;
	char	buf[MAXPATH];
	char	key[MAXKEY];
	MDBM	*permDB = mdbm_mem();
	char	*cmd = "apply";

	if (opts->log) fprintf(opts->log, "==== Pass 4 ====\n");
	opts->pass = 4;

	unfinished(opts);

	/*
	 * Call the pre-apply trigger if there is one so that we have
	 * one last chance to bail out.
	 */
	putenv("BK_CSETLIST=BitKeeper/etc/csets-in");
	if (getenv("BK_REMOTE") && streq(getenv("BK_REMOTE"), "YES")) {
		cmd = "remote apply";
	}
	if (ret = trigger(cmd,  "pre")) {
		switch (ret) {
		    case 3: flags = CLEAN_MVRESYNC; break;
		    case 2: flags = CLEAN_RESYNC; break;
		    default: flags = CLEAN_RESYNC|CLEAN_PENDING; break;
		}
		mdbm_close(permDB);
		resolve_cleanup(opts, CLEAN_NOSHOUT|flags);
	}

	if (opts->logging) { /* hard-coded logging tree trigger */
		metaUnionResync1();
	}
	
	/*
	 * Pass 4a - check for edited files and build up a list of files to
	 * backup and remove.
	 */
	chdir(RESYNC2ROOT);
	save = fopen(BACKUP_LIST, "w+");
	unlink(PASS4_TODO);
	sprintf(key, "bk sfind %s > " PASS4_TODO, ROOT2RESYNC);
	if (system(key) || !(f = fopen(PASS4_TODO, "r+")) || !save) {
		fprintf(stderr, "Unable to create|open " PASS4_TODO);
		fclose(save);
		mdbm_close(permDB);
		resolve_cleanup(opts, 0);
	}
	chmod(PASS4_TODO, 0666);
	while (fnext(buf, f)) {
		chop(buf);
		/*
		 * We want to check the checksum here and here only
		 * before we throw it over the wall.
		 */
		unless (r = sccs_init(buf, INIT_SAVEPROJ, opts->resync_proj)) {
			fprintf(stderr,
			    "resolve: can't init %s - abort.\n", buf);
			fclose(save);
			fprintf(stderr, "resolve: no files were applied.\n");
			resolve_cleanup(opts, 0);
		}
#define	F ADMIN_FORMAT|ADMIN_TIME|ADMIN_BK
		if (sccs_admin(r, 0, SILENT|F, 0, 0, 0, 0, 0, 0, 0, 0)) {
			sccs_admin(r, 0, F, 0, 0, 0, 0, 0, 0, 0, 0);
			fprintf(stderr, "resolve: bad file %s;\n", r->sfile);
			fprintf(stderr, "resolve: no files were applied.\n");
			fclose(save);
			mdbm_close(permDB);
			resolve_cleanup(opts, 0);
		}
		if (writeCheck(r, permDB) && first) {
			first = 0;
			eperm = 1;
		}
		sccs_sdelta(r, sccs_ino(r), key);
		sccs_free(r);
		if (l = sccs_keyinit(key, INIT, opts->local_proj, opts->idDB)) {
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
			fprintf(save, "%s\n", l->sfile);
			sccs_free(l);
		}
	}
	fclose(save);
	mdbm_close(permDB);

	if (eperm) {
		getMsg("write_perms", 0, 0, '=', stderr);
		resolve_cleanup(opts, 0);
	}

	/*
	 * Pass 4b.
	 * Save the list of files and then remove them.
	 */
	if (size(BACKUP_LIST) > 0) {
		unlink(BACKUP_SFIO);
		if (sysio(BACKUP_LIST, BACKUP_SFIO, 0,
						"bk", "sfio", "-omq", SYS)) {
			fprintf(stderr,
			    "Unable to create backup %s from %s\n",
			    BACKUP_SFIO, BACKUP_LIST);
			fclose(save);
			resolve_cleanup(opts, 0);
		}
		save = fopen(BACKUP_LIST, "rt");
		assert(save);
		while (fnext(buf, save)) {
			chop(buf);
			if (opts->log) fprintf(stdlog, "unlink(%s)\n", buf);
			rm_sfile(buf, 1);
		}
		fclose(save);
	}

	/*
	 * Pass 4c - apply the files.
	 */
	unless (save = fopen(APPLIED, "w+")) {
		restore(opts);
		resolve_cleanup(opts, 0);
	}
	fflush(f);
	rewind(f);

	while (fnext(buf, f)) {
		chop(buf);
		/*
		 * We want to get the part of the path without the RESYNC.
		 */
		if (exists(&buf[offset])) {
			fprintf(stderr,
			    "resolve: failed to remove conflict %s\n",
			    &buf[offset]);
			unapply(save);
			restore(opts);
			resolve_cleanup(opts, 0);
		}
		if (opts->log) {
			fprintf(stdlog, "copy(%s, %s)\n", buf, &buf[offset]);
		}
		if (copyAndGet(opts, buf, &buf[offset])) {
			perror("copy");
			fprintf(stderr,
			    "copy(%s, %s) failed\n", buf, &buf[offset]);
err:			unapply(save);
			restore(opts);
			resolve_cleanup(opts, 0);
		} else {
			opts->applied++;
		}
		fprintf(save, "%s\n", &buf[offset]);
	}
	fclose(f);

	/* Remove cache of cset revisions */
	delete_cset_cache(".", 0);

 	/*
	 * If case folding file system , make sure file name
	 * did not change case after we copied it
	 */
	if (isCaseFoldingFS() && chkCaseChg(save)) goto err;

	if (opts->logging) { /* hard-coded logging tree trigger */
		metaUnionResync2();
	}

	unless (opts->quiet) {
		fprintf(stderr,
		    "resolve: applied %d files in pass 4\n", opts->applied);
	}
	unless (opts->quiet) {
		fprintf(stderr,
		    "resolve: running consistency check, please wait...\n");
	}

	if (strieq("yes", user_preference("partial_check"))) {
		fflush(save); /*  important */
		ret = run_check(APPLIED);
	} else {
		ret = run_check(0);
	}
	if (ret) {
		fprintf(stderr, "Check failed.  Resolve not completed.\n");
		/*
		 * Clean up any gfiles we may have pulled out to run check.
		 */
		system("bk clean BitKeeper/etc");
		goto err;
	}
	fclose(save);
	unlink(BACKUP_LIST);
	unlink(BACKUP_SFIO);
	unlink(APPLIED);
	unlink(PASS4_TODO);
	unless (opts->quiet) {
		fprintf(stderr,
		    "Consistency check passed, resolve complete.\n");
	}
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

	strcpy(path, s->sfile + 7);	/* RESYNC/SCCS want SCCS */
	if (t = strrchr(path, '/')) *t = 0;
	if (mdbm_fetch_str(db, path)) return (0);
	mdbm_store_str(db, path, "", MDBM_INSERT);
	if (isdir(path) && writable(path)) {
		if (streq(path, "SCCS")) return (0);
		t[-5] = 0;
		if (isdir(path) && writable(path)) return (0);
		unless (writable(path)) {
			fprintf(stderr, "No write permission: %s\n", path);
			return (1);
		}
		/* we can write the parent, so OK */
		return (0);
	}
	if (isdir(path)) {
		fprintf(stderr, "No write permission: %s\n", path);
		return (1);
	}
	if (exists(path)) {
		fprintf(stderr,
		    "File where a directory needs to be: %s\n", path);
		return (1);
	}
	return (0);
}

private int
copyAndGet(opts *opts, char *from, char *to)
{
	sccs	*s;
	char	key[MAXKEY];
	char	*co;
	int	getFlags;

	if (link(from, to)) {
		if (mkdirf(to)) {
			fprintf(stderr,
			    "mkdir: %s: %s\n",
			    to, strerror(errno));
			return (-1);
		}
		/*
		 * We need to fall back to fileCopy() if:
		 * a) The enclosing tree and the RESYNC tree are on
		 *    different file system. 
		 * b) it is Samba, which does not support hard link.
		 * Note: It is alos reported that in AFS file system,
		 *    the link() call return EXDEV when we try to
		 *    create hard link across different
		 *    directories, because AFS's ACL is per directory.
		 */
		if (link(from, to) && fileCopy(from, to)) return (-1);
	}

	s = sccs_init(to, INIT_SAVEPROJ, opts->local_proj);
	assert(s && HASGRAPH(s));
	sccs_sdelta(s, sccs_ino(s), key);
	co = mdbm_fetch_str(opts->checkoutDB, key);
	if (co) {
		//ttyprintf("checkout %s with %s\n", key, co);
		if (co[0] == 'e') {
			getFlags = GET_EDIT;
		} else if (co[0] == 'g') {
			getFlags = GET_EXPAND;
		} else if (co[0] == 'n') {
			getFlags = 0;
		} else {
			fprintf(stderr, "Illegal value of co (== %s)\n",
			    co);
			exit(1);
		}
		mdbm_delete_str(opts->checkoutDB, key);
	} else {
		getFlags = default_getFlags;
		//ttyprintf("checkout %s with defaults(%d)\n", key, getFlags);
	}
	if (opts->logging) {
		unless (strneq(to, "BitKeeper/etc/SCCS", 18)) {
			assert(!HAS_GFILE(s));
		}
	} else if (getFlags) {
		sccs_get(s, 0, 0, 0, 0, SILENT|getFlags, "-");
	} else {
		assert(!HAS_GFILE(s));
	}
	sccs_free(s);
	return (0);
}

/*
 * Unlink all the things we successfully applied.
 */
private void
unapply(FILE *f)
{
	char	buf[MAXPATH];

	fflush(f);
	rewind(f);
	while (fnext(buf, f)) {
		chop(buf);
		rm_sfile(buf, 0);
	}
	fclose(f);
}

/*
 * Go through and put everything back.
 */
private	void
restore(opts *o)
{
	if (sysio(BACKUP_SFIO, 0, 0, "bk", "sfio", "-im", SYS)) {
		fprintf(stderr,
"!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\
Your repository is only partially restored.   This is an error.  Please \n\
examine the list of failures above and find out why they were not renamed.\n\
You must move them into place by hand before the repository is usable.\n");
		fprintf(stderr, "\nA backup sfio is in %s\n", BACKUP_SFIO);
		fprintf(stderr, "\
!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
	} else {
		fprintf(stderr,
"Your repository should be back to where it was before the resolve started.\n\
We are running a consistency check to verify this.\n");
		if (sys("bk", "-r", "check", "-a", SYS) == 0) {
			fprintf(stderr, "Check passed.\n");
		} else {
			fprintf(stderr, "Check FAILED, contact BitMover.\n");
		}
	}
}

private	void
freeStuff(opts *opts)
{
	/*
	 * Clean up allocations/files/dbms
	 */
	if (opts->local_proj) proj_free(opts->local_proj);
	if (opts->resync_proj) proj_free(opts->resync_proj);
	if (opts->log && (opts->log != stderr)) fclose(opts->log);
	if (opts->rootDB) mdbm_close(opts->rootDB);
	if (opts->idDB) mdbm_close(opts->idDB);
}

private void
csets_in(opts *opts)
{
	sccs	*s;
	delta	*d;
	FILE	*in, *out;
	char	s_cset[] = CHANGESET;
	char	buf[MAXPATH];

	if (opts->didMerge) {
		chdir(ROOT2RESYNC);
		s = sccs_init(s_cset, 0, 0);
		assert(s && HASGRAPH(s));
		d = sccs_top(s);
		assert(d && d->merge);
		in = fopen(CSETS_IN, "r");	/* RESYNC one */
		assert(in);
		sprintf(buf, "%s/%s", RESYNC2ROOT, CSETS_IN);
		out = fopen(buf, "w");		/* real one */
		while (fnext(buf, in)) {
			unless (streq(buf, "\n")) fputs(buf, out);
		}
		fprintf(out, "%s\n", d->rev);
		fclose(out);
		fclose(in);
		sccs_free(s);
		unlink(CSETS_IN);
		chdir(RESYNC2ROOT);
	} else {
		sprintf(buf, "%s/%s", ROOT2RESYNC, CSETS_IN);
		/* May not exist if we pulled in tags only */
		if (exists(buf)) rename(buf, CSETS_IN);
	}
}

void
resolve_cleanup(opts *opts, int what)
{
	char	buf[MAXPATH];
	char	pendingFile[MAXPATH];
	FILE	*f;

	unless (exists(ROOT2RESYNC)) chdir(RESYNC2ROOT);
	unless (exists(ROOT2RESYNC)) {
		fprintf(stderr, "cleanup: can't find RESYNC dir\n");
		fprintf(stderr, "cleanup: nothing removed.\n");
		exit(1);
	}

	/*
	 * Get the patch file name from RESYNC before deleting RESYNC.
	 */
	sprintf(buf, "%s/%s", ROOT2RESYNC, "BitKeeper/tmp/patch");
	unless (f = fopen(buf, "rb")) {
		fprintf(stderr, "Warning: no BitKeeper/tmp/patch\n");
		pendingFile[0] = 0;
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
		sys(RM, "-rf", "RESYNC", SYS);
	} else if (what & CLEAN_MVRESYNC) {
		char	*dir = savefile(".", "RESYNC-", 0);

		assert(exists("RESYNC"));
		assert(dir);
		unlink(dir);
		sys("mv", "RESYNC", dir, SYS);
	} else {
		if (exists(ROOT2RESYNC "/SCCS/p.ChangeSet")) {
			assert(!exists("RESYNC/ChangeSet"));
			unlink(ROOT2RESYNC "/SCCS/p.ChangeSet");
			rename(ROOT2RESYNC "/BitKeeper/tmp/r.ChangeSet",
			    ROOT2RESYNC "/SCCS/r.ChangeSet");
		}
		fprintf(stderr,
		    "resolve: RESYNC directory left intact.\n");
	}

	if (what & CLEAN_PENDING) {
		if (pendingFile[0] && !getenv("TAKEPATCH_SAVEDIRS")) {
			unlink(pendingFile);
		}
		rmdir(ROOT2PENDING);
	}

	restore_checkouts(opts);

	freeStuff(opts);
	unless (what & CLEAN_OK) {
		unless (what & CLEAN_NOSHOUT) SHOUT2();
		exit(1);
	}

	resolve_post(opts, 0);

	/*
	 * Force a logging process even if we did not create a merge node
	 * This is needed because we do not carry the log marker in
	 * external patch, csets from remote tree would look like they
	 * have pending logs. If we don't force a loggging process now,
	 * we will get a bogus warning message on the next commit.
	 *
	 * if (opts->didMerge && !opts->logging) ...
	 */
	if (!opts->logging) logChangeSet(1);
	exit(0);
}

/*
 * This gets called for every local file that might get overwritten.
 * We need to record the checkout state of the file so we can restore it
 * after resolve is finished.
 */
private void
save_checkout_state(MDBM *DB, sccs *s)
{
	char	key[MAXKEY];
	char	*state;

	sccs_sdelta(s, sccs_ino(s), key);

	if (HAS_PFILE(s)) {
		state = "e";
	} else if (HAS_GFILE(s)) {
		state = "g";
	} else {
		state = "n";
	}
	mdbm_store_str(DB, key, state, MDBM_INSERT);
}

/*
 * After resolve is finished we look at every file that
 * we might have touched and restore the gfile if needed.
 */
private void
restore_checkouts(opts *opts)
{
	kvpair	kv;
	int	getFlags = 0;

	EACH_KV(opts->checkoutDB) {
		sccs	*s;
		s = sccs_keyinit(kv.key.dptr, INIT, 
		    opts->local_proj, opts->idDB);

		unless (s) continue;

		switch (kv.val.dptr[0]) {
		    case 'e':
			unless (HAS_PFILE(s)) getFlags = GET_EDIT;
			break;
		    case 'g':
			unless (HAS_GFILE(s)) getFlags = GET_EXPAND;
			break;
		    case 'n':
			assert(!HAS_GFILE(s));
			break;
		    default:
			assert(0);
			break;
		}
		if (getFlags) {
			sccs_get(s, 0, 0, 0, 0, SILENT|getFlags, "-");
		}
		sccs_free(s);
	}
	mdbm_close(opts->checkoutDB);
	opts->checkoutDB = 0;
}
