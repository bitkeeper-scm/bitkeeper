#include "system.h"
#include "sccs.h"
#include "logging.h"
#include <time.h>

/*
 * commit options
 */
typedef struct {
	u32	quiet:1;
	u32	resync:1;
} c_opts;

private int	do_commit(char **av, c_opts opts, char *sym,
					char *pendingFiles, int dflags);

int
commit_main(int ac, char **av)
{
	int	c, doit = 0, force = 0;
	char	buf[MAXLINE], s_cset[MAXPATH] = CHANGESET;
	char	pendingFiles[MAXPATH] = "";
	char	*sym = 0;
	int	dflags = 0;
	c_opts	opts  = {0, 0};

	while ((c = getopt(ac, av, "df:FRqsS:y:Y:")) != -1) {
		switch (c) {
		    case 'd': 	doit = 1; break;		/* doc 2.0 */
		    case 'f':					/* undoc 2.0 */
			strcpy(pendingFiles, optarg); break;
		    case 'F':	force = 1; break;		/* undoc */
		    case 'R':	BitKeeper = "../BitKeeper/";	/* doc 2.0 */
				opts.resync = 1;
				break;
		    case 's':	/* fall thru  *//* internal */	/* undoc 2.0 */
		    case 'q':	opts.quiet = 1; break;		/* doc 2.0 */
		    case 'S':	sym = optarg;
				if (sccs_badTag("commit", sym, 0)) exit (1);
				break;		/* doc 2.0 */
		    case 'y':					/* doc 2.0 */
			dflags |= DELTA_DONTASK;
			comments_save(optarg);
			break;
		    case 'Y':					/* doc 2.0 */
			if (comments_savefile(optarg)) {
				fprintf(stderr,
				    "commit: can't read comments from %s\n",
				    optarg);
				exit(1);
			}
			dflags |= DELTA_DONTASK;
			break;
		    default:	system("bk help -s commit");
				return (1);
		}
	}

	if (proj_cd2root()) {
		fprintf(stderr, "Cannot find root directory\n");
		return (1);
	}
	/*
	 * Check for licensing problems before we get buried in a bunch
	 * of subprocesses.  This process will need the result anyway so
	 * this isn't any slower.
	 */
	lease_check(0, O_WRONLY, 0);

	if (pendingFiles[0]) {
		if (av[optind] && streq("-", av[optind])) {
			fprintf(stderr,
			    "commit: can't use -f when using \"-\"\n");
			return (1);
		}
	} else {
		if (av[optind] && streq("-", av[optind])) {
			FILE	*f;

			unless (dflags & DELTA_DONTASK) {
				fprintf(stderr,
				    "You must use the -Y or -y "
				    "option when using \"-\"\n");
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
			if (sysio(0,
			    pendingFiles, 0, "bk", "sfiles", "-pC", SYS)) {
				unlink(pendingFiles);
				getMsg("duplicate_IDs", 0, 0, stdout);
				return (1);
			}
		}
	}
	unless (force) {
		if (size(pendingFiles) == 0) {
			unless (opts.quiet) {
				fprintf(stderr, "Nothing to commit\n");
			}
			unlink(pendingFiles);
			return (0);
		}
		/* check is skipped with -F so 'bk setup' works */
		if (sysio(pendingFiles, 0, 0, "bk", "check", "-c", "-", SYS)) {
			unlink(pendingFiles);
			return (1);
		}
	}
	unless (dflags & DELTA_DONTASK) {
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
		cmd = aprintf("bk _sort -u | "
			"bk sccslog -DA - >> %s", commentFile);
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
			printf("Commit aborted.\n");
			unlink(pendingFiles);
			unlink(commentFile);
			return (1);
		}
		dflags |= DELTA_DONTASK;
		comments_savefile(commentFile);
		unlink(commentFile);
	}
	do_clean(s_cset, SILENT);
	return (do_commit(av, opts, sym, pendingFiles, dflags));
}

private int
do_commit(char **av,
	c_opts opts, char *sym, char *pendingFiles, int dflags)
{
	int	rc, i;
	sccs	*cset;
	char	**syms = 0;
	FILE 	*f;
	char	commentFile[MAXPATH];
	char	buf[MAXLINE];

	if (enforceLicense()) {
		if (pendingFiles) unlink(pendingFiles);
		return (1);
	}
	/*
	 * XXX Do we want to fire the trigger when we are in RESYNC ?
	 */
	safe_putenv("BK_PENDING=%s", pendingFiles);

	/* XXX could avoid if we knew if a trigger would fire... */
	bktmp(commentFile, "comments");
	comments_writefile(commentFile);
	safe_putenv("BK_COMMENTFILE=%s", commentFile);

	if (rc = trigger(opts.resync ? "merge" : av[0], "pre")) goto done;
	comments_done();
	comments_savefile(commentFile);
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

	cset = sccs_csetInit(0);
	rc = csetCreate(cset, dflags, pendingFiles, syms);

	putenv("BK_STATUS=OK");
	if (rc) putenv("BK_STATUS=FAILED");
	trigger(opts.resync ? "merge" : av[0], "post");
done:	if (unlink(pendingFiles)) perror(pendingFiles);
	unlink(commentFile);
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
