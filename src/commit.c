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
private	int	make_comment(char *cmt, char *commentFile);
private int	do_commit(char **av, c_opts opts, char *sym,
					char *pendingFiles, char *commentFile);

int
commit_main(int ac, char **av)
{
	int	c, doit = 0, force = 0, getcomment = 1;
	char	buf[MAXLINE], s_cset[MAXPATH] = CHANGESET;
	char	commentFile[MAXPATH], pendingFiles[MAXPATH] = "";
	char	*sym = 0;
	c_opts	opts  = {0, 0, 0, 0};

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help commit");
		return (1);
	}

	gettemp(commentFile, "bk_commit");
	while ((c = getopt(ac, av, "aAdf:FRqsS:y:Y:")) != -1) {
		switch (c) {
		    case 'a':	opts.alreadyAsked = 1; break;	/* doc 2.0 */
		    case 'd': 	doit = 1; break;		/* doc 2.0 */
		    case 'f':					/* undoc 2.0 */
			strcpy(pendingFiles, optarg); break;
		    case 'F':	force = 1; break;		/* doc 2.0 */
		    case 'R':	BitKeeper = "../BitKeeper/";	/* doc 2.0 */
				opts.resync = 1;
				break;
		    case 's':	/* fall thru  *//* internal */	/* undoc 2.0 */
		    case 'q':	opts.quiet = 1; break;		/* doc 2.0 */
		    case 'S':	sym = optarg; break;		/* doc 2.0 */
		    case 'y':	doit = 1; getcomment = 0;	/* doc 2.0 */
				if (make_comment(optarg, commentFile)) {
					return (1);
				}
				break;
		    case 'Y':	doit = 1; getcomment = 0;	/* doc 2.0 */
				if (fileCopy(optarg, commentFile)) {
					fprintf(stderr,
					    "commit: cannot copy to comment "
					    "file %s\n", commentFile);
					unlink(commentFile);
					return (1);
				}
				break;
		    case 'A':	/* internal option for regression test only */
				/* do not document */		/* undoc 2.0 */
				opts.no_autoupgrade = 1; break;
		    default:	system("bk help -s commit");
				return (1);
		}
	}

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "Cannot find root directory\n");
		return (1);
	}
	unless(opts.resync) remark(opts.quiet);
	if (pendingFiles[0] || (av[optind] && streq("-", av[optind]))) {
		FILE *f;

		if (getcomment && !pendingFiles[0]) {
			fprintf(stderr,
			"You must use the -Y or -y option when using \"-\"\n");
			return (1);
		}
		unless (pendingFiles[0]) {
			gettemp(pendingFiles, "bk_list");
			setmode(0, _O_TEXT);
			f = fopen(pendingFiles, "wb");
			assert(f);
			while (fgets(buf, sizeof(buf), stdin)) {
				fputs(buf, f);
			}
			fclose(f);
		}
	} else {
		gettemp(pendingFiles, "bk_pending");
		if (sysio(0,
		    pendingFiles, 0, "bk", "sfind", "-s,,p", "-C", SYS)) {
			unlink(pendingFiles);
			unlink(commentFile);
			getMsg("duplicate_IDs", 0, 0, stdout);
			return (1);
		}
	}
	if ((force == 0) && (size(pendingFiles) == 0)) {
		unless (opts.quiet) fprintf(stderr, "Nothing to commit\n");
		unlink(pendingFiles);
		unlink(commentFile);
		return (0);
	}
	if (sysio(pendingFiles, 0, 0, "bk", "check", "-c", "-", SYS)) {
		unlink(pendingFiles);
		unlink(commentFile);
		return (1);
	}
	if (getcomment) {
		char	*cmd, *p;
		FILE	*f, *f1;

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
	}
	do_clean(s_cset, SILENT);
	if (doit) return (do_commit(av, opts, sym, pendingFiles, commentFile));

	while (1) {
		printf("\n-------------------------------------------------\n");
		cat(commentFile);
		printf("-------------------------------------------------\n");
		printf("Use these comments (e)dit, (a)bort, (u)se? ");
		fflush(stdout);
		unless (getline(0, buf, sizeof(buf)) > 0) goto Abort;
		switch (buf[0]) {
		    case 'y':  /* fall thru */
		    case 'u':
			return(do_commit(av, opts, sym,
						pendingFiles, commentFile));
			break;
		    case 'e':
			sprintf(buf, "%s %s", editor, commentFile);
			system(buf);
			break;
		    case 'a':
Abort:			printf("Commit aborted.\n");
			unlink(pendingFiles);
			unlink(commentFile);
			return(1);
		}
	}
}

private int
pending(char *sfile)
{
	sccs	*s = sccs_init(sfile, 0, 0);
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

/*
 * Return true if pname is NULL or all blank
 */
private int
goodPackageName(char *pname)
{
	unless (pname) return (0);
	while (*pname) unless (isspace(*pname++)) return (1);
	return (0);
}

private int
do_commit(char **av,
	c_opts opts, char *sym, char *pendingFiles, char *commentFile)
{
	int	hasComment = (exists(commentFile) && (size(commentFile) > 0));
	int	status, rc, i;
	int	l, fd, fd0;
	char	buf[MAXLINE], sym_opt[MAXLINE] = "", cmt_opt[MAXPATH + 3], *p;
	char	pendingFiles2[MAXPATH] = "";
	char    s_logging_ok[] = LOGGING_OK;
	char	*cset[100] = {"bk", "cset", 0};
	FILE 	*f, *f2;

	l = logging(0);
	unless (ok_commit(l, opts.alreadyAsked)) {
out:		if (commentFile) unlink(commentFile);
		if (pendingFiles) unlink(pendingFiles);
		return (1);
	}

	/*
	 * If no package_name, nag user to update config file
	 */
	if (is_openlogging(l) && !goodPackageName(package_name())) {
		fprintf(stderr,
"============================================================================\n"
"Warning: Package name is Null or Blank. Please add an entry to the \n"
"\"Description:\" field in the \"BitKeeper/etc/config\" file\n"
"The entry should be a short statement describing the nature of this package\n"
"============================================================================\n"
);
		sleep(1);
		
	}

	if (!opts.resync && (enforceLicense(l) == -1)) goto out;

	if (pending(s_logging_ok)) {
		int     len = strlen(s_logging_ok); 

		/*
		 * Redhat 5.2 cannot handle opening a file
		 * in both read and write mode fopen(file, "rt+") at the
		 * same time. Win32 is likely to have same problem too
		 * So we open the file in read mode close it and re-open
		 * it in write mode
		 */
		gettemp(pendingFiles2, "bk_pending2");
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
	sprintf(buf, "BK_PENDING=%s", p);
	putenv((strdup)(buf));
	if (trigger(av, "pre")) {
		rc = 1;
		goto done;
	}
	i = 2;
	if (opts.quiet) cset[i++] = "-q";
	if (sym) {
		sprintf(sym_opt, "-S%s", sym);
		cset[i++] = sym_opt;
	}
	if (f = fopen("SCCS/t.ChangeSet", "r")) {
		while (fnext(buf, f)) {
			char	*t;
			
			chop(buf);
			t = aprintf("-S%s", buf);
			cset[i++] = t;
			assert(i < 90);
		}
		fclose(f);
		unlink("SCCS/t.ChangeSet");
	}
	if (hasComment) {
		sprintf(cmt_opt, "-Y%s", commentFile);
		cset[i++] = cmt_opt;
	}
	cset[i] = 0;
	fd0 = dup(0); close(0);
	fd = open(p, O_RDONLY, 0);
	assert(fd == 0);
	status = spawnvp_ex(_P_WAIT, cset[0], cset);
	close(0); dup2(fd0, 0); close(fd0);

	putenv("BK_STATUS=OK");
	if (!WIFEXITED(status)) {
		putenv("BK_STATUS=SIGNALED");
		rc = 1;
	} else if (rc = WEXITSTATUS(status)) {
		putenv("BK_STATUS=FAILED");
	}
	trigger(av, "post");
done:	if (unlink(commentFile)) perror(commentFile);
	if (unlink(pendingFiles)) perror(pendingFiles);
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
	unless (opts.resync) logChangeSet(l, opts.quiet);
	return (rc ? 1 : 0);
}

private	int
make_comment(char *cmt, char *commentFile)
{
	int fd;
	int flags = O_CREAT|O_TRUNC|O_WRONLY;

	if ((fd = open(commentFile, flags, 0664)) == -1)  {
		perror("commit");
		return (1);
	}
	setmode(fd, O_TEXT);
	write(fd, cmt, strlen(cmt));
	close(fd);
	return (0);
}
