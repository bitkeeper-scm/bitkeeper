#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "nested.h"
#include <time.h>

/*
 * commit options
 */
typedef struct {
	u32	quiet:1;
	u32	resync:1;
	u32	standalone:1;
	u32	clean_PENDING:1;	// if successful, clean PENDING marks
} c_opts;


private int	do_commit(char **av, c_opts opts, char *sym,
					char *pendingFiles, int dflags);
private	void	commitSnapshot(void);
private	void	commitRestore(int rc);

private	int	csetCreate(sccs *cset, int flags, char *files, char **syms);

int
commit_main(int ac, char **av)
{
	int	i, c, doit = 0, force = 0;
	int	subCommit = 0;	/* commit called itself */
	char	**aliases = 0;
	char	*cmtsFile;
	char	*sym = 0;
	int	dflags = 0;
	char	**nav = 0;
	int	do_stdin;
	int	nested;		/* commiting all components? */
	c_opts	opts  = {0, 0};
	char	*sfopts;
	char	pendingFiles[MAXPATH] = "";
	longopt	lopts[] = {
		{ "standalone", 'S' },		/* new -S option */
		{ "subset", 's' },		/* aliases */
		{ "tag:", 300 },		/* old -S option */
		{ "sub-commit", 301 },		/* calling myself */
		{ 0, 0 }
	};
	char	buf[MAXLINE];

	while ((c = getopt(ac, av, "cdfFl:qRs|S|y:Y:", lopts)) != -1) {
		unless ((c == 's') || (c == 'S') || (c == 'Y')) {
			nav = bk_saveArg(nav, av, c);
		}
		switch (c) {
		    case 'c': dflags |= DELTA_CFILE; break;
		    case 'd': 
		    case 'f':
			doit = 1; break;			/* doc 2.0 */
		    case 'l':					/* doc */
			strcpy(pendingFiles, optarg); break;
		    case 'F':	force = 1; break;		/* undoc */
		    case 'R':	opts.resync = 1;		/* doc 2.0 */
				break;
		    case 's':
			unless (optarg) {
				fprintf(stderr,
				    "bk %s -sALIAS: ALIAS "
				    "cannot be omitted\n", prog);
				return (1);
			}
			unless (aliases) {
				/* LM3DI: always commit product */
				aliases = addLine(aliases, strdup("PRODUCT"));
			}
			aliases = addLine(aliases, strdup(optarg));
			break;
		    case 'q':	opts.quiet = 1; break;		/* doc 2.0 */
		    case 'S':
			if (optarg) {
				fprintf(stderr, "%s: commit -S<tag> is now "
				    "commit --tag=<tag>\n", prog);
				return (1);
			}
			opts.standalone = 1;
			break;
		    case 'y':					/* doc 2.0 */
			dflags |= DELTA_DONTASK;
			if (comments_save(optarg)) return (1);
			break;
		    case 'Y':					/* doc 2.0 */
			/*
			 * Turn to a full path here and _then_ save
			 * it. This is so that the iterator below
			 * doesn't have relative path problems.
			 */
			unless (cmtsFile = fullname(optarg, 0)) {
				fprintf(stderr,
				    "%s: can't read comments from %s\n",
				    prog, optarg);
				exit(1);
			}
			optarg = cmtsFile;
			nav = bk_saveArg(nav, av, c);
			if (comments_savefile(optarg)) {
				fprintf(stderr,
				    "commit: can't read comments from %s\n",
				    optarg);
				exit(1);
			}
			dflags |= DELTA_DONTASK;
			free(cmtsFile);
			break;
		    case 300:	/* --tag=TAG */
			sym = optarg;
			if (sccs_badTag("commit", sym, 0)) exit (1);
			break;
		    case 301:	/* --sub-commit */
			opts.standalone = subCommit = 1;
			break;
		    default: bk_badArg(c, av);
		}
	}

	trigger_setQuiet(opts.quiet);

	nested = bk_nested2root(opts.standalone);
	if (opts.standalone && aliases) {
		fprintf(stderr, "%s: options -S and -sALIAS "
		    "cannot be combined.\n", prog);
		return (1);
	}
	if (aliases && !nested) {
		EACH(aliases) {
			unless (streq(aliases[i], ".") ||
			    strieq(aliases[i], "HERE") ||
			    strieq(aliases[i], "PRODUCT")) {
				fprintf(stderr,
				    "%s: -sALIAS only allowed in product\n",
				    prog);
				return (1);
			}
		}
		freeLines(aliases, free);
		aliases = 0;
	}
	if (sym && proj_isComponent(0)) {
		unless (subCommit) {
			fprintf(stderr,
			    "%s: component tags not yet supported.\n", prog);
			return (1);
		}
		sym = 0;
	}
	T_PERF("start");

	/*
	 * Check for licensing problems before we get buried in a bunch
	 * of subprocesses.  This process will need the result anyway so
	 * this isn't any slower.
	 */
	lease_check(0, O_WRONLY, 0);
	do_stdin = (av[optind] && streq("-", av[optind]));
	if (nested) {
		int	rc;

		if (pendingFiles[0] || do_stdin) {
			fprintf(stderr,
			    "%s: Must use -S with -l or \"-\"\n", prog);
			return (1);
		}
		cmdlog_lock(CMD_NESTED_WRLOCK);
		nav = unshiftLine(nav, strdup("--sub-commit"));
		nav = unshiftLine(nav, strdup("commit"));
		nav = unshiftLine(nav, strdup("bk"));

		unless (aliases) aliases = modified_pending(DS_PENDING);
		rc = nested_each(opts.quiet, nav, aliases);
		freeLines(aliases, free);
		freeLines(nav, free);
		if (rc) {
			fprintf(stderr,
			    "%s: failed to commit some components\n", prog);
		}
		return (rc);
	}

	unless (opts.resync) cmdlog_lock(CMD_WRLOCK|CMD_NESTED_WRLOCK);

	if (pendingFiles[0] && do_stdin) {
		fprintf(stderr, "commit: can't use -l when using \"-\"\n");
		return (1);
	} else {
		if (pendingFiles[0] || do_stdin) {
			FILE	*f, *fin;

			unless (dflags & (DELTA_DONTASK|DELTA_CFILE)) {
				fprintf(stderr,
				    "You must use one of the -c, -Y or -y "
				    "options when using \"-\"\n");
				return (1);
			}
			if (pendingFiles[0]) {
				unless (fin = fopen(pendingFiles, "r")) {
					perror(pendingFiles);
					return (1);
				}
			} else {
				fin = stdin;
			}
			bktmp(pendingFiles);
			setmode(0, _O_TEXT);
			f = fopen(pendingFiles, "w");
			assert(f);
			while (fgets(buf, sizeof(buf), fin)) {
				fputs(buf, f);
			}
			fclose(f);
			if (fin != stdin) fclose(fin);
		} else {
			bktmp(pendingFiles);
			sfopts = opts.resync ? "-rpC" : "-pC";
			if (sysio(0,
			    pendingFiles, 0, "bk", "sfiles", sfopts, SYS)) {
				unlink(pendingFiles);
				getMsg("duplicate_IDs", 0, 0, stdout);
				return (1);
			}
			opts.clean_PENDING = 1;
		}
	}
	if (!force && (size(pendingFiles) == 0)) {
		unless (opts.quiet) fprintf(stderr, "Nothing to commit\n");
		unlink(pendingFiles);
		return (0);
	}
	/*
	 * Auto pickup a c.ChangeSet unless they already gave us comments.
	 * Prompt though, that's what we do in delta.
	 */
	unless (dflags & (DELTA_DONTASK|DELTA_CFILE)) {
		if (size("SCCS/c.ChangeSet") > 0) {
			bktmp_local(buf);
			fileCopy("SCCS/c.ChangeSet", buf);
			if (!doit && comments_prompt(buf)) {
				fprintf(stderr, "Commit aborted.\n");
				return (1);
			}
			rename(buf, "SCCS/c.ChangeSet");
			dflags |= DELTA_CFILE;
		}
	}

	unless (dflags & (DELTA_DONTASK|DELTA_CFILE)) {
		char	*cmd, *p;
		FILE	*f, *f1;
		char	commentFile[MAXPATH];
		char	buf[512];

		bktmp(commentFile);
		f = popen("bk cat BitKeeper/templates/commit", "r");
		assert(f);
		if (fnext(buf, f)) {
			f1 = fopen(commentFile, "w");
			fputs(buf, f1);
			while (fnext(buf, f)) {
				fputs(buf, f1);
			}
			fclose(f1);
		}
		pclose(f);
		cmd = aprintf("bk sort -u | "
			"bk sccslog -DA - >> '%s'", commentFile);
		f = popen(cmd, "w");
		f1 = fopen(pendingFiles, "rt");
		assert(f); assert (f1);
		while (fnext(buf, f1)) {
			p = strrchr(buf, BK_FS);
			assert(p);
			*p = 0;
			fputs(buf, f);
			fputs("\n", f);
		}
		fclose(f1);
		pclose(f);
		free(cmd);

		if (!doit && comments_prompt(commentFile)) {
			fprintf(stderr, "Commit aborted.\n");
			unlink(pendingFiles);
			unlink(commentFile);
			return (1);
		}
		dflags |= DELTA_DONTASK;
		if (comments_savefile(commentFile)) return (1);
		unlink(commentFile);
	}
	unlink("ChangeSet");
	T_PERF("before do_commit");
	return (do_commit(av, opts, sym, pendingFiles, dflags));
}

/*
 * Return the md5keys for all of the components that have 'flags'
 * set in the scanComps file.
 */
char **
modified_pending(u32 flags)
{
	int	i;
	int	product = 0;
	char	**dirs;
	project	*p;
	char	**aliases = 0;
	char	buf[MAXPATH];

	unless (dirs = proj_scanComps(0, flags)) return (0);
	EACH(dirs) {
		concat_path(buf, proj_root(proj_product(0)), dirs[i]);
		unless (p = proj_init(buf)) {
			unless (getenv("_BK_DEVELOPER")) continue;
			die("%s should be a repo", dirs[i]);
		}
		// Ah, I see.  If we have a standalone don't
		// add it.
		unless (proj_isEnsemble(p)) {
			if (getenv("_BK_DEVELOPER")) {
				die("standalone %s\n", dirs[i]);
			}
			proj_free(p);
			continue;
		}
		if (proj_isProduct(p)) product = 1;
		aliases = addLine(aliases, strdup(proj_md5rootkey(p)));
		T_NESTED("SCAN %s", dirs[i]);
		proj_free(p);
	}
	unless (product) {
		aliases = addLine(aliases, strdup("PRODUCT"));
		T_NESTED("SCAN PRODUCT");
	}
	freeLines(dirs, 0);
	return (aliases);
}


private int
do_commit(char **av,
	c_opts opts, char *sym, char *pendingFiles, int dflags)
{
	int	rc, i;
	sccs	*cset;
	char	**syms = 0;
	FILE 	*f, *f2;
	char	*t;
	char	**list = 0;
	char	commentFile[MAXPATH] = "";
	char	pendingFiles2[MAXPATH];
	char	buf[MAXLINE];

	cset = sccs_csetInit(0);
	if (enforceLicense(cset)) {
err:		rc = 1;
		goto done;
	}
	(void)sccs_defRootlog(cset);	/* if no rootlog, make one */
	commitSnapshot();
	if (!opts.resync && (rc = attr_update())) {
		if (rc < 0) goto err;
		bktmp(pendingFiles2);
		f = fopen(pendingFiles, "r");
		f2 = fopen(pendingFiles2, "w");
		i = strlen(SATTR);
		while (t = fgetline(f)) {
			if (strneq(SATTR "|", t, i+1)) {
				/* skip ATTR file */
				continue;
			}
			fprintf(f2, "%s\n", t);
		}
		fclose(f);
		fprintf(f2, "%s%c+\n", SATTR, BK_FS);
		fclose(f2);
		if (unlink(pendingFiles)) perror(pendingFiles);
		fileMove(pendingFiles2, pendingFiles);
	}
	/*
	 * XXX Do we want to fire the trigger when we are in RESYNC ?
	 */
	safe_putenv("BK_PENDING=%s", pendingFiles);

	/* XXX could avoid if we knew if a trigger would fire... */
	if (dflags & DELTA_CFILE) {
		strcpy(commentFile, sccs_Xfile(cset, 'c'));
		unless (size(commentFile) > 0) {
			fprintf(stderr, "commit: saved comments not found.\n");
			rc = 1;
			goto done;
		}
	} else {
		bktmp(commentFile);
		comments_writefile(commentFile);
	}
	safe_putenv("BK_COMMENTFILE=%s", commentFile);

	if (rc = trigger(opts.resync ? "merge" : av[0], "pre")) goto done;
	comments_done();
	if (comments_savefile(commentFile)) {
		rc = 1;
		goto done;
	}
	if (opts.quiet) dflags |= SILENT;
	if (sym) syms = addLine(syms, strdup(sym));
	if (f = fopen("SCCS/t.ChangeSet", "r")) {
		while (fnext(buf, f)) {
			chop(buf);
			syms = addLine(syms, strdup(buf));
		}
		fclose(f);
		unlink("SCCS/t.ChangeSet");
	}

	/*
	 * I don't think it makes sense to prevent tags in the RESYNC tree.
	 * We need them to handle the tag merge.
	 * If we really want to prevent them then I think we need a way of
	 * listing them when we are at the pre-resolve stage so that a trigger
	 * could be written which detects that and fails the resolve.
	 */
	unless (opts.resync) {
		EACH (syms) {
			safe_putenv("BK_TAG=%s", syms[i]);
			rc = trigger("tag", "pre");
			switch (rc) {
			    case 0: break;
			    case 2:
				removeLineN(syms, i, free);
				/* we left shifted one, go around again */
				i--;
				break;
			    default: goto done;
			}
		}
	}
	rc = csetCreate(cset, dflags, pendingFiles, syms);

	// run check
	unless (rc) {
		/*
		 * Note: by having no |+, the check.c:check() if (doMark...)
		 * code will skip trying to mark the ChangeSet file.
		 * Every other file will have a |<rev> and so will get marked.
		 */
		f = fopen(pendingFiles, "a");
		fprintf(f, CHANGESET "\n");
		fclose(f);
		if (sysio(pendingFiles, 0, 0,
		    "bk", "-?BK_NO_REPO_LOCK=YES", "check",
		    opts.resync ? "-cMR" : "-cM", "-", SYS)) {
			rc = 1;
		}
	}
	if (!rc && opts.resync) {
		char	key[MAXPATH];
		FILE	*f;

		/*
		 * We created a commit in the RESYNC directory,
		 * probably a merge cset closing the open tips of the
		 * ChangeSet file. Log it in BitKeeper/etc/csets-in so
		 * that 'bk abort' can pick it up if we fail.
		 */
		sccs_sdelta(cset, sccs_top(cset), key);
		if (f = fopen(CSETS_IN, "a")) {
			fprintf(f, "%s\n", key);
			fclose(f);
		}
	}
	if (!rc && proj_isComponent(0)) {
		hash	*urllist;
		char	*file = proj_fullpath(proj_product(0), NESTED_URLLIST);

		/*
		 * Created a new cset for this component, the saved URLs
		 * for this component are now all invalid.
		 */
		if (urllist = hash_fromFile(0, file)) {
			hash_deleteStr(urllist, proj_rootkey(0));
			if (hash_toFile(urllist, file)) perror(file);
			hash_free(urllist);
		}
	}
	putenv("BK_STATUS=OK");
	if (rc) {
		fprintf(stderr, "The commit is aborted.\n");
		putenv("BK_STATUS=FAILED");
	} else if (opts.clean_PENDING) {
		proj_dirstate(0, "*", DS_PENDING, 0);
		if (proj_isComponent(0)) {
			/* this component is still pending */
			proj_dirstate(0, ".", DS_PENDING, 1);
		}
	}
	trigger(opts.resync ? "merge" : av[0], "post");
done:	if (unlink(pendingFiles)) perror(pendingFiles);
	sccs_free(cset);
	freeLines(list, free);
	commitRestore(rc);
	unless (*commentFile) {
		// don't try to unlink
	} else if (dflags & DELTA_CFILE) {
		// Someone else created the c.file, unlink only on success
		unless (rc) unlink(commentFile);
	} else {
		// we created it, always unlink
		unlink(commentFile);
	}
	/*
	 * If we are doing a commit in RESYNC or the commit failed, do
	 * not log the cset. Let the resolver do it after it moves the
	 * stuff in RESYNC to the real tree.
	 */
	unless (opts.resync || rc) logChangeSet();
	T_PERF("done");
	return (rc);
}


/*
 * Read in sfile and extend the cset weave
 */
private int
getfilekey(char *sfile, char *rev, sccs *cset, ser_t cset_d, char ***keys)
{
	sccs	*s;
	u8	*v, *p;
	ser_t	d;
	u32	sum = 0;
	u8	buf[MAXLINE];

	unless (s = sccs_init(sfile, 0)) {
		fprintf(stderr, "cset: can't init %s\n", sfile);
		return (1);
	}

	/*
	 * Only component cset files get added to the weave
	 */
	if (CSET(s) && !proj_isComponent(s->proj)) {
		sccs_free(s);
		return (0);
	}
	unless (d = sccs_findrev(s, rev)) {
		fprintf(stderr, "cset: can't find %s in %s\n", rev, sfile);
		return (1);
	}
	assert(!(FLAGS(s, d) & D_CSET));

	/*
	 * XXX For non-merge deltas we don't need cset->mdbm so we can
	 * avoid reading the old weave.  Instead we can just find the latest
	 * marked delta key and use that.
	 */
	sccs_sdelta(s, sccs_ino(s), buf);
	*keys = addLine(*keys, strdup(buf));
	if (v = mdbm_fetch_str(cset->mdbm, buf)) {
		/* subtract off old value */
		for (p = v; *p; sum -= *p++);
	} else {
		/* new file (need sum of rootkey) */
		for (p = buf; *p; sum += *p++);
		sum += ' ' + '\n';
	}
	sccs_sdelta(s, d, buf);
	*keys = addLine(*keys, strdup(buf));
	for (p = buf; *p; p++) sum += *p; /* sum of new deltakey */
	sccs_free(s);

	/* update delta checksum */
	SUM_SET(cset, cset_d, (u16)(SUM(cset, cset_d) + sum));
	SORTSUM_SET(cset, cset_d, SUM(cset, cset_d));
	ADDED_SET(cset, cset_d, ADDED(cset, cset_d) + 1);
	return (0);
}

/*
 * Read file|rev from stdin and apply those to the changeset db.
 * Edit the ChangeSet file and add the new stuff to that file and
 * leave the file sorted.
 * Close the cset sccs* when done.
 */
private ser_t
mkChangeSet(sccs *cset, char *files, char ***keys)
{
	ser_t	d, p;
	char	*line, *rev, *t;
	int	flags = GET_HASHONLY|GET_SUM|GET_SHUTUP|SILENT;
	FILE	*f;
	pfile	pf;
	char	buf[MAXLINE];

	/*
	 * Edit the ChangeSet file - we need it edited to modify it as well
	 */
	if (LOCKED(cset)) {
		if (sccs_read_pfile("commit", cset, &pf)) return (0);
	} else {
		flags |= GET_EDIT;
		memset(&pf, 0, sizeof(pf));
		pf.oldrev = strdup("+");
	}
	if (HASGRAPH(cset) &&
	    sccs_get(cset, pf.oldrev, pf.mRev, 0, 0, flags, "-")) {
		unless (BEEN_WARNED(cset)) {
			fprintf(stderr, "cset: get -eg of ChangeSet failed\n");
		}
		free_pfile(&pf);
		return (0);
	}
	d = sccs_dInit(0, 'D', cset, 0);
	if (d == TREE(cset)) {
		if (t = getenv("BK_RANDOM")) {
			strcpy(buf, t);
		} else {
			randomBits(buf);
		}
		RANDOM_SET(cset, d, buf);

		/* a rootkey can't have a realuser or realhost */
		sccs_parseArg(cset, d, 'U', sccs_getuser(), 0);
		sccs_parseArg(cset, d, 'H', sccs_gethost(), 0);

		/* nor a pathname for a component */
		sccs_parseArg(cset, d, 'P', "ChangeSet", 0);

		t = fullname(cset->gfile, 0);
		sprintf(buf, "BitKeeper file %s\n", t);
		free(t);
		COMMENTS_SET(cset, d, buf);

		cset->bitkeeper = 1;
		XFLAGS(cset, d) |= X_REQUIRED|X_LONGKEY;
		R0_SET(cset, d, 1);

		SUM_SET(cset, d, almostUnique());
	} else {
		p = sccs_findrev(cset, pf.oldrev);
		assert(p);
		PARENT_SET(cset, d, p);
		R0_SET(cset, d, R0(cset, p));	/* so renumber() is happy */
		XFLAGS(cset, d) = XFLAGS(cset, p);

		/* set initial key, getfilekey() updates diff from merge */
		SUM_SET(cset, d, cset->dsum);

		/*
		 * bk normally doesn't set the MODE() for the
		 * ChangeSet file so the following line usually just
		 * sets MODE=0.  But on some repos like bk source we
		 * do have a mode and this propagates the existing
		 * value.
		 */
		MODE_SET(cset, d, MODE(cset, p));
	}
	SORTSUM_SET(cset, d, SUM(cset, d));

	if (sccs_setCludes(cset, d, pf.iLst, pf.xLst)) {
		fprintf(stderr, "%s: bad iLst in pfile\n", prog);
		free_pfile(&pf);
		return (0);
	}
	if (pf.mRev) {
		p = sccs_findrev(cset, pf.mRev);
		MERGE_SET(cset, d, p);
	}
	free_pfile(&pf);

	if (uniq_open()) assert(0);
	if (TABLE(cset) && (DATE(cset, d) <= DATE(cset, TABLE(cset)))) {
		time_t	tdiff;

		tdiff = DATE(cset, TABLE(cset)) - DATE(cset, d) + 1;
		DATE_SET(cset, d, DATE(cset, d) + tdiff);
		DATE_FUDGE_SET(cset, d, DATE_FUDGE(cset, d) + tdiff);
	}
	uniq_adjust(cset, d);
	uniq_close();

	if (files) {
		/*
		 * Read each file|rev from files and add that to the cset.
		 * getfilekey() will ignore the ChangeSet entry itself.
		 */
		f = fopen(files, "rt");
		assert(f);
		while (line = fgetline(f)) {
			rev = strrchr(line, '|');
			*rev++ = 0;
			getfilekey(line, rev, cset, d, keys);
		}
		fclose(f);
	}

	if (d == TREE(cset)) {
		// set the CSETFILE backpointer for the 1.0 delta
		sccs_sdelta(cset, d, buf);
		sccs_parseArg(cset, d, 'B', buf, 0);
	} else {
		SAME_SET(cset, d, 1);
	}

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
	if (DATE(cset, d) <= DATE(cset, table)) {
		DATE_FUDGE(cset, d) =
		    (DATE(cset, table) - DATE(csets, d)) + 1;
		DATE_SET(cset, d, (DATE(cset, d) + DATE_FUDGE(cset, d)));
	}
#endif
	return (d);
}

private int
csetCreate(sccs *cset, int flags, char *files, char **syms)
{
	ser_t	d;
	int	i, error = 0;
	int	fd0;
	char	*line;
	char	**keys = 0;
	FILE	*out;

	T_PERF("csetCreate");

	if ((TABLE(cset) + 1 > 200) && getenv("BK_REGRESSION")) {
		fprintf(stderr, "Too many changesets for regressions.\n");
		exit(1);
	}

	/* write change set to diffs */
	unless (d = mkChangeSet(cset, files, &keys)) {
		freeLines(keys, free);
		return (-1);
	}

	/* for compat with old versions of BK not using ensembles */
	if (proj_isComponent(cset->proj)) {
		updatePending(cset);
	} else {
		if (d > TREE(cset)) FLAGS(cset, d) |= D_CSET;
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
	if ((flags & (DELTA_DONTASK|DELTA_CFILE)) &&
	    !(d = comments_get(0, 0, cset, d))) {
		error = -1;
		goto out;
	}
	if (fd0 >= 0) {
		dup2(fd0, 0);
		close(fd0);
		fd0 = -1;
	}
	sccs_insertdelta(cset, d, d);
	sccs_renumber(cset, 0);
	sccs_startWrite(cset);
	if (BWEAVE_OUT(cset)) weave_set(cset, d, keys);
	EACH (syms) addsym(cset, d, 1, syms[i]);
	if (delta_table(cset, 0)) {
		perror("table");
		error = -1;
		goto out;
	}
	unless (BWEAVE_OUT(cset)) {
		out = sccs_wrweaveInit(cset);
		fprintf(out, "\001I %d\n", d);
		EACH(keys) {
			fprintf(out, "%s %s\n", keys[i], keys[i+1]);
			i++;
		}
		fprintf(out, "\001E %d\n", d);
		unless (flags & DELTA_EMPTY) {
			sccs_rdweaveInit(cset);
			while (line = sccs_nextdata(cset)) {
				fputs(line, out);
				fputc('\n', out);
			}
			sccs_rdweaveDone(cset);
		}
		sccs_wrweaveDone(cset);
	}
	if (sccs_finishWrite(cset)) {
		error = -1;
		goto out;
	}
	T_PERF("wrote weave");
	unlink(cset->gfile);
	unlink(cset->pfile);
	cset->state &= ~(S_GFILE|S_PFILE);

out:	unless (error || (flags & SILENT)) {
		fprintf(stderr, "ChangeSet revision %s: +%d\n",
		    REV(cset, cset->tip), nLines(keys));
	}
	freeLines(keys, free);
	comments_done();
	return (error);
}

int
cset_setup(int flags)
{
	sccs	*cset;
	int	rc;

	flags |= DELTA_EMPTY;
	cset = sccs_csetInit(SILENT);
	assert(cset->state & S_CSET);
	cset->xflags |= X_LONGKEY;
	rc = csetCreate(cset, flags, 0, 0);
	sccs_free(cset);
	return (rc);
}

#define	CSET_BACKUP	"BitKeeper/tmp/commit.cset.backup"

/*
 * Save SCCS/?.ChangeSet files so we can restore them later if commit
 * fails.
 */
private void
commitSnapshot(void)
{
	char	*ext = "scp";	/* file exensions to backup */
	int	i;
	char	file[MAXPATH];
	char	save[MAXPATH];

	/*
	 * We don't save a backup of the ChangeSet heaps in
	 * [12].ChangeSet.  Since we will only append to these files
	 * if we rollback the s.ChangeSet file everything will still
	 * work.
	 *
	 * Saving a hardlink back wouldn't work because bk would copy
	 * the file when it need to append new data to the end.
	 */
	for (i = 0; ext[i]; i++) {
		sprintf(file, "SCCS/%c.ChangeSet", ext[i]);
		if (exists(file)) {
			sprintf(save, CSET_BACKUP ".%c", ext[i]);
			fileLink(file, save);
		}
	}
	if (exists(SATTR)) fileLink(SATTR, CSET_BACKUP "attr.s");
}

/*
 * If rc!=0, then commit failed and we need to restore the state saved by
 * commitSnapshot().  Otherwise delete that state.
 */
private void
commitRestore(int rc)
{
	char	*ext = "scp";	/* file exensions to backup */
	int	i;
	char	file[MAXPATH];
	char	save[MAXPATH];

	for (i = 0; ext[i]; i++) {
		sprintf(save, CSET_BACKUP ".%c", ext[i]);
		if (exists(save)) {
			sprintf(file, "SCCS/%c.ChangeSet", ext[i]);
			if (rc) {
				fileMove(save, file);
				unlink("BitKeeper/log/TIP");
			} else {
				unlink(save);
			}
		}
	}
	strcpy(save, CSET_BACKUP "attr.s");
	if (exists(save)) {
		if (rc) {
			fileMove(save, SATTR);
		} else {
			unlink(save);
		}
	}
}
