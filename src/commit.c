#include "system.h"
#include "sccs.h"
#include "logging.h"
#include <time.h>

/*
 * commit options
 */
typedef struct {
	u32	alreadyAsked:1;
	u32	quiet:1;
	u32	resync:1;
	u32	no_autoupgrade:1;
} c_opts;

extern	char	*editor, *bin, *BitKeeper;
extern	int	do_clean(char *, int);
extern	int	loggingask_main(int, char**);
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
	c_opts	opts  = {0, 0, 0, 0};

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help commit");
		return (1);
	}

	while ((c = getopt(ac, av, "aAdf:FRqsS:y:Y:")) != -1) {
		switch (c) {
		    case 'a':	opts.alreadyAsked = 1; break;	/* doc 2.0 */
		    case 'd': 	doit = 1; break;		/* doc 2.0 */
		    case 'f':					/* undoc 2.0 */
			strcpy(pendingFiles, optarg); break;
		    case 'F':	force = 1; break;		/* undoc */
		    case 'R':	BitKeeper = "../BitKeeper/";	/* doc 2.0 */
				opts.resync = 1;
				break;
		    case 's':	/* fall thru  *//* internal */	/* undoc 2.0 */
		    case 'q':	opts.quiet = 1; break;		/* doc 2.0 */
		    case 'S':	sym = optarg; break;		/* doc 2.0 */
		    case 'y':					/* doc 2.0 */
			dflags |= DELTA_DONTASK;
			comments_save(optarg);
			break;
		    case 'Y':					/* doc 2.0 */
			unless (exists(optarg) && (size(optarg) > 0)) {
				fprintf(stderr,
				    "commit: can't read comments from %s\n",
				    optarg);
				exit(1);
			}
			dflags |= DELTA_DONTASK;
			comments_savefile(optarg);
			break;
		    case 'A':	/* internal option for regression test only */
				/* do not document */		/* undoc 2.0 */
				opts.no_autoupgrade = 1; break;
		    default:	system("bk help -s commit");
				return (1);
		}
	}

	if (proj_cd2root()) {
		fprintf(stderr, "Cannot find root directory\n");
		return (1);
	}
	unless(opts.resync) remark(opts.quiet);
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
			f = fopen(pendingFiles, "wb");
			assert(f);
			while (fgets(buf, sizeof(buf), stdin)) {
				fputs(buf, f);
			}
			fclose(f);
		} else {
			bktmp(pendingFiles, "bk_pending");
			if (sysio(0, pendingFiles, 0,
				"bk", "sfind", "-s,,p", "-C", SYS)) {
				unlink(pendingFiles);
				getMsg("duplicate_IDs", 0, 0, 0, stdout);
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

		bktmp(commentFile, "commit");
		cmd = aprintf("bk _sort -u | "
			"bk sccslog -DA - > %s", commentFile);
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
pending(char *sfile)
{
	sccs	*s = sccs_init(sfile, 0);
	delta	*d;
	int	ret;

	unless (s) return (0);
	unless (HASGRAPH(s)) {
		sccs_free(s);
		return (0);
	}
	d = sccs_top(s);
	ret = (d->flags & D_CSET) == 0;	/* pending if not set */
	sccs_free(s);
	return (ret);
}

private int
do_commit(char **av,
	c_opts opts, char *sym, char *pendingFiles, int dflags)
{
	int	rc, i;
	char	buf[MAXLINE], *p;
	char	pendingFiles2[MAXPATH] = "";
	char    s_logging_ok[] = LOGGING_OK;
	sccs	*cset;
	char	**syms = 0;
	FILE 	*f, *f2;
	char	commentFile[MAXPATH];

	unless (ok_commit(opts.alreadyAsked)) {
out:		if (pendingFiles) unlink(pendingFiles);
		return (1);
	}

	if (!opts.resync && enforceLicense(opts.quiet)) goto out;

	if (pending(s_logging_ok)) {
		int     len = strlen(s_logging_ok); 

		/*
		 * Redhat 5.2 cannot handle opening a file
		 * in both read and write mode fopen(file, "rt+") at the
		 * same time. Win32 is likely to have same problem too
		 * So we open the file in read mode close it and re-open
		 * it in write mode
		 */
		bktmp(pendingFiles2, "bk_pending2");
		f = fopen(pendingFiles, "rb");
		f2 = fopen(pendingFiles2, "wb");
		assert(f); assert(f2);
		while (fnext(buf, f)) {
			/*
			 * Skip the logging_ok files
			 * We'll add it back when we exit this loop
			 */
			if (strneq(s_logging_ok, buf, len) &&
			    buf[len] == BK_FS) {
				continue;
			}
			fputs(buf, f2);
		}
		fprintf(f2, "%s%c+\n", s_logging_ok, BK_FS); 
		fclose(f);
		fclose(f2);
	}
	/*
	 * XXX Do we want to fire the trigger when we are in RESYNC ?
	 */
	p = pendingFiles2[0] ? pendingFiles2 : pendingFiles;
	safe_putenv("BK_PENDING=%s", p);

	/* XXX could avoid if we knew if a trigger would fire... */
	bktmp(commentFile, "comments");
	comments_writefile(commentFile);
	safe_putenv("BK_COMMENTFILE=%s", commentFile);

	if (rc = trigger(av[0], "pre")) goto done;
	i = 2;
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
	cset = sccs_csetInit(0,0);
	rc = csetCreate(cset, dflags, p, syms);

	putenv("BK_STATUS=OK");
	if (rc) putenv("BK_STATUS=FAILED");
	trigger(av[0], "post");
done:	if (unlink(pendingFiles)) perror(pendingFiles);
	unlink(commentFile);
	if (pendingFiles2[0]) {
		if (unlink(pendingFiles2)) perror(pendingFiles2);
	}
	if (rc) return (rc); /* if commit failed do not send log */
	/*
	 * If we are doing a commit in RESYNC
	 * do not log the cset. Let the resolver
	 * do it after it moves the stuff in RESYNC to
	 * the real tree. 
	 */
	unless (opts.resync) logChangeSet(opts.quiet);
	return (rc ? 1 : 0);
}
