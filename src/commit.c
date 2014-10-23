#include "system.h"
#include "sccs.h"
#include "logging.h"
#include "nested.h"
#include "range.h"
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
		if (xfile_exists(CHANGESET, 'c')) {
			char	*t;

			bktmp_local(buf);
			t = xfile_fetch(CHANGESET, 'c');
			Fprintf(buf, "%s", t);
			free(t);
			if (!doit && comments_prompt(buf)) {
				fprintf(stderr, "Commit aborted.\n");
				return (1);
			}
			t = loadfile(buf, 0);
			xfile_store(CHANGESET, 'c', t);
			free(t);
			unlink(buf);
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
	char	*p;
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
	bktmp(commentFile);
	if (dflags & DELTA_CFILE) {
		unless (p = xfile_fetch(cset->sfile, 'c')) {
			fprintf(stderr, "commit: saved comments not found.\n");
			rc = 1;
			goto done;
		}
		cset->used_cfile = 1;
		f = fopen(commentFile, "w");
		fputs(p, f);
		free(p);
		fclose(f);
	} else {
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
	T_PERF("after_check");
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
	// Someone else created the c.file, unlink only on success
	if ((dflags & DELTA_CFILE) && !rc) comments_cleancfile(cset);
	if (*commentFile) unlink(commentFile);
	sccs_free(cset);
	freeLines(list, free);
	commitRestore(rc);
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
	ser_t	d;
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

	sccs_sdelta(s, sccs_ino(s), buf);
	*keys = addLine(*keys, strdup(buf));
	sccs_sdelta(s, d, buf);
	*keys = addLine(*keys, strdup(buf));
	sccs_free(s);
	return (0);
}

/*
 * If lots of gone open tips, then we'll want to keep the gone DB open.
 * Similar logic to open tip error handling in check.c:buildKeys().
 */
private	int
missingMerge(sccs *cset, u32 rkoff)
{
	MDBM	*goneDB = loadDB(GONE, 0, DB_GONE);
	int	ret = 0;

	unless (mdbm_fetch_str(goneDB, HEAP(cset, rkoff))) {
		fprintf(stderr,
		    "commit: ChangeSet is a merge but is missing\n"
		    "a required merge delta for this rootkey\n");
		fprintf(stderr, "\t%s\n", HEAP(cset, rkoff));
		ret = 1;
	}
	mdbm_close(goneDB);
	return (ret);
}

/*
 * We are about to create a new cset 'd' with 'keys' in the weave.
 * This function computes the new SUM() for 'd'.  The changeset checksum
 * is the sum of the list of all rk/dk pairs for the current tip, so
 * we need to add the new files and then for updated files subtract the
 * old keys and add the new keys.
 *
 * SUM(cset, d) has been preset to the PARENTs checksum.  That's why
 * the MERGE case is asymmetrical - scanning RED for new and BLUE for
 * matches.
 *
 * Where possible we as little of the existing weave as possible to get
 * the data needed.
 *
 * seen hash: keys are rootkey offset in heap; values are 0, RED, BLUE
 * corresponding to tip, branch and trunk as where this rootkey was
 * first spotted.  While in one region we see it was first seen in
 * another region, report error.
 */
private int
updateCsetChecksum(sccs *cset, ser_t d, char **keys)
{
	hash	*h = hash_new(HASH_MEMHASH);
	int	i, cnt = 0, todo = 0;
	u32	sum = 0;
	u32	color = 0;
	char	*rk, *dk;
	u32	rkoff, dkoff;
	u8	*p;
	ser_t	e, old;
	hash	*seen = 0;
	int	ret = 0;

	if (MERGE(cset, d)) {
		/*
		 * Add in some MERGE side keys (colored RED)
		 * to todo list.  Also color PARENT side BLUE and
		 * use the seen hash both for computing what csets in RED
		 * to add to todo, as well as look for non merged tips.
		 */
		cset->rstart = 0;
		range_walkrevs(cset,
		    PARENT(cset, d), 0, MERGE(cset, d), WR_BOTH, 0, 0);
		if (cset->rstart) seen = hash_new(HASH_MEMHASH);
	}
	EACH(keys) {
		++cnt;
		rk = keys[i++];
		dk = keys[i];

		for (p = dk; *p; p++) sum += *p; /* sum of new deltakey */
		if (rkoff = sccs_hasRootkey(cset, rk)) {
			++todo;
			hash_insert(h, &rkoff, sizeof(rkoff), 0, 0);
			if (seen) {
				hash_insert(seen,
				    &rkoff, sizeof(rkoff), 0, sizeof(color));
			}
		} else {
			/* new file, just add rk now */
			for (p = rk; *p; p++) sum += *p;
			sum += ' ' + '\n';
		}
	}
	if (todo || seen) {
		sccs_rdweaveInit(cset);
		old = d;
		while (e = cset_rdweavePair(cset, 0, &rkoff, &dkoff)) {
			if (seen && (old != e)) {
				/* also clear colors for csets with no data */
				for (old--; old >= e; old--) {
					color =
					    FLAGS(cset, old) & (D_RED|D_BLUE);
					FLAGS(cset, old) &= ~(D_RED|D_BLUE);
				}
				old = e;
				if (e < cset->rstart) {
					/* all clean and done with merge */
					hash_free(seen);
					seen = 0;
				}
				/* leave with color set to cset e */
			}
			if (color) { /* in non-gca region of merge */
				if (hash_insert(seen,
				    &rkoff, sizeof(rkoff),
				    &color, sizeof(color))) {
					/* first time this rk was seen... */
					if ((color & D_RED) &&
					    hash_insert(h, &rkoff,sizeof(rkoff),
						0, 0)) {
						/* In merge and not in commit */
						++todo;
						p = HEAP(cset, dkoff);
						while (*p) sum += *p++;
					}
				} else if (!ret && *(u32 *)seen->vptr &&
				    (color != *(u32 *)seen->vptr)) {
					/*
					 * Not in commit, but included in both
					 * local and remote side of merge
					 */
					ret = missingMerge(cset, rkoff);
				}
				if (color & D_RED) continue;
			}
			unless (hash_delete(h, &rkoff, sizeof(rkoff))) {
				/*
				 * found previous deltakey for one of my files,
				 * subtract off old key
				 */
				for (p = HEAP(cset, dkoff); *p; sum -= *p++);
				--todo;
			}
			if (seen) {
				/* save if GCA region (no color) first to see */
				unless (color) {
					hash_insert(seen, &rkoff,
					    sizeof(rkoff), 0, sizeof(color));
				}
			} else if (!todo) {
				/* no more rootkeys to find, done. */
				break;
			}
		}
		if (seen) hash_free(seen);
		sccs_rdweaveDone(cset);
	}
	/*
	 * Any keys that remain in my hash must be new files so we
	 * need to add in the rootkey checksums.
	 */
	EACH_HASH(h) {
		rkoff = *(u32 *)h->kptr;
		for (p = HEAP(cset, rkoff); *p; p++) sum += *p;
		sum += ' ' + '\n';
	}
	hash_free(h);

	/* update delta checksum */
	if (!cnt && !MERGE(cset, d) && !HAS_CLUDES(cset, d)) {
		sum = almostUnique();
	} else {
		sum = (sum_t)(SUM(cset, d) + sum);
	}
	SUM_SET(cset, d, sum);
	SORTSUM_SET(cset, d, sum);
	ADDED_SET(cset, d, cnt);
	return (ret);
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
	ser_t	d, d2, p;
	char	*line, *rev, *t;
	FILE	*f;
	pfile	pf;
	char	buf[MAXLINE];

	/*
	 * Edit the ChangeSet file - we need it edited to modify it as well
	 */
	if (LOCKED(cset)) {
		if (sccs_read_pfile("commit", cset, &pf)) return (0);
	} else {
		memset(&pf, 0, sizeof(pf));
		pf.oldrev = strdup("+");
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

		/*
		 * set initial sum to parent, in updateCsetChecksum we update
		 */
		if (d2 = sccs_getCksumDelta(cset, p)) {
			SUM_SET(cset, d, SUM(cset, d2));
		} else {
			SUM_SET(cset, d, 0);
		}

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
		if (updateCsetChecksum(cset, d, *keys)) return (0);
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
	xfile_delete(cset->gfile, 'p');
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

#define	CSET_BACKUP	"BitKeeper/tmp/SCCS/commit.cset.backup"

static	char	*save_pfile;
static	char	*save_cfile;

/*
 * Save SCCS/?.ChangeSet files so we can restore them later if commit
 * fails.
 */
private void
commitSnapshot(void)
{
	char	*t;

	/*
	 * We don't save a backup of the ChangeSet heaps in
	 * [12].ChangeSet.  Since we will only append to these files
	 * if we rollback the s.ChangeSet file everything will still
	 * work.
	 *
	 * Saving a hardlink back wouldn't work because bk would copy
	 * the file when it need to append new data to the end.
	 */
	mkdir("BitKeeper/tmp/SCCS", 0777);
	fileLink("SCCS/s.ChangeSet", CSET_BACKUP ".s");
	if (t = xfile_fetch(CHANGESET, 'p')) save_pfile = t;
	if (t = xfile_fetch(CHANGESET, 'c')) save_cfile = t;
	if (exists(SATTR)) fileLink(SATTR, CSET_BACKUP "attr.s");
}

/*
 * If rc!=0, then commit failed and we need to restore the state saved by
 * commitSnapshot().  Otherwise delete that state.
 */
private void
commitRestore(int rc)
{
	char	save[MAXPATH];

	if (rc) {
		if (exists(CSET_BACKUP ".s")) {
			fileMove(CSET_BACKUP ".s", "SCCS/s.ChangeSet");
		}
		unlink("BitKeeper/log/TIP");
		if (save_pfile) xfile_store(CHANGESET, 'p', save_pfile);
		if (save_cfile) xfile_store(CHANGESET, 'c', save_cfile);
	} else {
		unlink(CSET_BACKUP ".s");
	}
	FREE(save_pfile);
	FREE(save_cfile);

	strcpy(save, CSET_BACKUP "attr.s");
	if (exists(save)) {
		if (rc) {
			fileMove(save, SATTR);
		} else {
			unlink(save);
		}
	}
}
