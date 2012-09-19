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
} c_opts;


private int	do_commit(char **av, c_opts opts, char *sym,
					char *pendingFiles, int dflags);
private	void	commitSnapshot(void);
private	void	commitRestore(int rc);

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

	/*
	 * Check for licensing problems before we get buried in a bunch
	 * of subprocesses.  This process will need the result anyway so
	 * this isn't any slower.
	 */
	lease_check(0, O_WRONLY, 0);

	if (nested) {
		int	rc;

		if (pendingFiles[0] ||
		    (av[optind] && streq("-", av[optind]))) {
			fprintf(stderr,
			    "%s: Must use -S with -l or \"-\"\n", prog);
			return (1);
		}
		cmdlog_lock(CMD_NESTED_WRLOCK);
		nav = unshiftLine(nav, strdup("--sub-commit"));
		nav = unshiftLine(nav, strdup("commit"));
		nav = unshiftLine(nav, strdup("bk"));

		rc = nested_each(opts.quiet, nav, aliases);
		freeLines(aliases, free);
		freeLines(nav, free);
		if (rc) {
			fprintf(stderr, "%s: failed to commit some components\n",
			    prog);
		}
		return (rc);
	}

	cmdlog_lock(CMD_WRLOCK|CMD_NESTED_WRLOCK);

	if (pendingFiles[0]) {
		if (av[optind] && streq("-", av[optind])) {
			fprintf(stderr,
			    "commit: can't use -l when using \"-\"\n");
			return (1);
		}
	} else {
		if (av[optind] && streq("-", av[optind])) {
			FILE	*f;

			unless (dflags & (DELTA_DONTASK|DELTA_CFILE)) {
				fprintf(stderr,
				    "You must use one of the -c, -Y or -y "
				    "options when using \"-\"\n");
				return (1);
			}
			bktmp(pendingFiles, "list");
			setmode(0, _O_TEXT);
			f = fopen(pendingFiles, "w");
			assert(f);
			while (fgets(buf, sizeof(buf), stdin)) {
				fputs(buf, f);
			}
			fclose(f);
		} else {
			bktmp(pendingFiles, "pending");
			sfopts = opts.resync ? "-rpC" : "-pC";
			if (sysio(0,
			    pendingFiles, 0, "bk", "sfiles", sfopts, SYS)) {
				unlink(pendingFiles);
				getMsg("duplicate_IDs", 0, 0, stdout);
				return (1);
			}
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
			bktmp_local(buf, "cfile");
			fileCopy("SCCS/c.ChangeSet", buf);
			if (comments_prompt(buf)) {
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

		bktmp(commentFile, "commit");
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
	return (do_commit(av, opts, sym, pendingFiles, dflags));
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
		rc = 1;
		goto done;
	}
	(void)sccs_defRootlog(cset);	/* if no rootlog, make one */
	if (!opts.resync && attr_update()) {
		bktmp(pendingFiles2, "pending2");
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
		bktmp(commentFile, "comments");
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
	commitSnapshot();
	rc = csetCreate(cset, dflags, pendingFiles, syms);

	// run check
	unless (rc) {
		if (sysio(pendingFiles, 0, 0,
		    "bk", "check", opts.resync ? "-cMR" : "-cM", "-", SYS)) {
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
	if ((dflags & DELTA_CFILE) && !rc) unlink(commentFile);
	if (rc) return (rc); /* if commit failed do not send log */
	/*
	 * If we are doing a commit in RESYNC
	 * do not log the cset. Let the resolver
	 * do it after it moves the stuff in RESYNC to
	 * the real tree. 
	 */
	unless (opts.resync) logChangeSet();
	return (rc ? 1 : 0);
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

	for (i = 0; ext[i]; i++) {
		sprintf(file, "SCCS/%c.ChangeSet", ext[i]);
		if (exists(file)) {
			sprintf(save, CSET_BACKUP ".%c", ext[i]);
			fileLink(file, save);
		}
	}
}

/*
 * If rc!=0, then commit failed and we need to store the state saved by
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
			} else {
				unlink(save);
			}
		}
	}
}
