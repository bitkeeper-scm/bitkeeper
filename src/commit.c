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
	c_opts	opts  = {0, 0, 0, 0, 0};

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help commit");
		return (1);
	}

	gettemp(commentFile, "bk_commit");
	while ((c = getopt(ac, av, "aAdf:FRqsS:y:Y:")) != -1) {
		switch (c) {
		    case 'a':	opts.alreadyAsked = 1; break;	/* doc 2.0 */
		    case 'd': 	doit = 1; break;	/* doc 2.0 */
		    case 'f':	/* undoc 2.0 */
			strcpy(pendingFiles, optarg); break;
		    case 'F':	force = 1; break;	/* doc 2.0 */
		    case 'R':	BitKeeper = "../BitKeeper/";	/* doc 2.0 */
				opts.resync = 1;
				break;
		    case 's':	/* fall thru  *//* internal *//* undoc 2.0 */
		    case 'q':	opts.quiet = 1; break;	/* doc 2.0 */
		    case 'S':	sym = optarg; break;	/* doc 2.0 */
		    case 'y':	doit = 1; getcomment = 0;	/* doc 2.0 */
				if (make_comment(optarg, commentFile)) {
					return (1);
				}
				break;
		    case 'Y':	doit = 1; getcomment = 0;	/* doc 2.0 */
				strcpy(commentFile, optarg);
				break;
		    case 'A':	/* internal option for regression test only */
				/* do not document */	/* undoc 2.0 */
				opts.no_autoupgrade = 1; break;
		    default:	system("bk help -s commit");
				return (1);
		}
	}

	if (sccs_cd2root(0, 0) == -1) {
		printf("Cannot find root directory\n");
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
		if (sysio(0, pendingFiles,  0,
					"bk", "sfind", "-s,,p", "-C", SYS)) {
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
	if (getcomment) {
		sysio(pendingFiles, commentFile, 0,
					"bk", "sccslog", "-CA", "-", SYS);
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

private void
notice(char *key)
{
	printf(
	    "==============================================================\n");
	getMsg(key, 0, 0, stdout);
	printf(
	    "==============================================================\n");
}

int
logs_pending(int ptype, int skipRecentCset)
{
#define MAX_LOG_DELAY (60 * 60 * 24 * 7) /* 7 days */
	sccs 	*s;
	delta	*d;
	char 	s_cset[] = CHANGESET;
	int	i = 0;
	time_t	now;
	time_t	max_delay = MAX_LOG_DELAY;

	s = sccs_init(s_cset, 0, 0);
	assert(s && s->tree);
	for (d = sccs_top(s); d; d = d->next) {
		if (d->published && (d->ptype == ptype)) sccs_color(s, d);
	}

	if (skipRecentCset) {
		now = time(0);
		assert(now >= 0);
	}

	for (d = s->table; d; d = d->next) {
		if (d->type != 'D') continue; 
		if (d->flags & D_RED) continue; 
		if (skipRecentCset && ((now - d->date) < max_delay)) continue;
		i++;
	}
	sccs_free(s);
	return (i);
}

int
ok_commit(int l, int alreadyAsked)
{
	if (alreadyAsked) {	/* has to be OK or error */
		/* if open, then OK */
		if ((l & (LOG_OPEN|LOG_OK)) == (LOG_OPEN|LOG_OK)) return (1);
		/* if logging, then must have OKed or error */
		if ((l & LOG_CLOSED) && !(l & LOG_OK)) return (0);
		/* must have license or error */
		if (l & (LOG_LIC_OK|LOG_LIC_GRACE)) return (1);
		return (0);
	}

	if ((l & (LOG_OPEN|LOG_OK)) == (LOG_OPEN|LOG_OK)) {
		return (1);
	}

	/*
	 * We're interactive so it is OK to ask if we need to.
	 */
	if ((l & (LOG_OPEN|LOG_OK)) == LOG_OPEN) {
		return (loggingask_main(0, 0) == 0);
	}

	if ((l & (LOG_CLOSED|LOG_OK)) == LOG_CLOSED)  {
		unless (loggingask_main(0, 0) == 0) return (0);
		/* fall through to license checks */
	}

	if (l & LOG_LIC_OK) return (1);
	if (l & LOG_LIC_GRACE) {
		notice("license_grace");
		return (1);
	}
	if (l & LOG_LIC_EXPIRED) {
		notice("license_expired");
		return (0);
	}
	if (l & LOG_LIC_NONE) {
		notice("license_none");
		return (0);
	}
	assert("ok_commit" == 0);
	return (-1);	/* lint */
}

private int
pending(char *sfile)
{
	sccs	*s = sccs_init(sfile, 0, 0);
	delta	*d;
	int	ret;

	unless (s) return (0);
	unless (s->tree) {
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
do_commit(char **av, c_opts opts, char *sym,
				char *pendingFiles, char *commentFile)
{
	int	hasComment = (exists(commentFile) && (size(commentFile) > 0));
	int	status, rc, i;
	int	l, ptype, fd, fd0;
	char	buf[MAXLINE], sym_opt[MAXLINE] = "", cmt_opt[MAXPATH + 3], *p;
	char	pendingFiles2[MAXPATH] = "";
	char    s_logging_ok[] = LOGGING_OK;
	char	*cset[10] = {"bk", "cset", 0};
	FILE 	*f, *f2;
#define	MAX_PENDING_LOG 20

	l = logging(0, 0, 0);
	unless (ok_commit(l, opts.alreadyAsked)) {
out:		if (commentFile) unlink(commentFile);
		if (pendingFiles) unlink(pendingFiles);
		return (1);
	}

	/*
	 * If no package_name, nag user to update config file
	 */
	if ((l&LOG_OPEN) && !goodPackageName(package_name())) {
		printf(
"============================================================================\n"
"Warning: Package name is Null or Blank. Please add an entry to the \n"
"\"Description:\" field in the \"BitKeeper/config\" file\n"
"The entry should be a short statement describing the nature of this package\n"
"============================================================================\n"
);
		sleep(1);
		
	}

	/*
	 * Note: We print to stdout, not stderr, because citool
	 * 	 monitors our stdout via a pipe.
	 */
	ptype = (l&LOG_OPEN) ? 0 : 1;
	unless (opts.resync) {
		if (logs_pending(ptype, 1) >= MAX_PENDING_LOG) {
			printf("Commit: forcing pending logs\n");
			if (l&LOG_OPEN) {
				system("bk _log -qc2");
			} else {
				system("bk _lconfig");
			}
			if ((logs_pending(ptype, 1) >= MAX_PENDING_LOG)) {
				printf(
"============================================================================\n"
"Error: Max pending log exceeded, commit aborted\n\n"
"This error indicates that the BitKeeper program is unable to contact \n"
"www.openlogging.org to send the ChangeSet log. The number of unlogged \n"
"changesets have exceeded the allowed threshold of %d changesets. \n"
"\n"
"There are two possible causes for this error:\n"
"a) Network connectivity problem\n"
"b) Problems in the logging tree at www.openlogging.org\n"
"\n"
"Please run the logging command in debug mode to see what the problem is:\n"
"\t\"bk _log  -d\"\n\n"
"If you have ruled out a network connectivity problem at your local network,\n"
"please email the following information to support@bitmover.com:\n"
"a) Output of the above logging command\n"
"b) Root key of your project using the following command:\n"
"\t\"bk -R inode ChangeSet\" command.\n"
"============================================================================\n"
, MAX_PENDING_LOG);
				goto out;
			}
		}
		if (!(l&LOG_OPEN) && (l&LOG_LIC_SINGLE) &&
					!smallTree(BK_SINGLE_THRESHOLD)) {
			printf(
"============================================================================\n"
"Error: Single user repositories are limited to %d files. If you need more\n"
"files than that in one repository, you need to buy a commercial license or\n"
"turn on open logging.\n"
"============================================================================\n"
, BK_SINGLE_THRESHOLD);
			goto out;
		}
	}
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
		sprintf(buf, "BK_STATUS=%d", rc);
		putenv(buf);
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
	unless (opts.resync) logChangeSet(l, 0, 0);
	return (rc ? 1 : 0);
}

private	int
make_comment(char *cmt, char *commentFile)
{
	int fd;
	int flags = O_CREAT|O_TRUNC|O_WRONLY;

#ifdef WIN32
	flags |= _O_TEXT;
#endif
	if ((fd = open(commentFile, flags, 0664)) == -1)  {
		perror("commit");
		return (1);
	}
	write(fd, cmt, strlen(cmt));
	close(fd);
	return (0);
}

void
logChangeSet(int l, char *rev, int quiet)
{
	char	commit_log[MAXPATH], buf[MAXLINE], rev_buf[20], *p;
	char	s_cset[] = CHANGESET, subject[MAXLINE];
	char	start_rev[1024];
	char	*to = logAddr();
	FILE	*f;
	int	dotCount = 0, n;
	pid_t	pid;
	sccs	*s;
	delta	*d;
	char	*log_av[] = {"bk", "_log", "-q", OPENLOG_URL, 0};
	char	*mail_av[] = {"bk", "_mail", to, subject, commit_log, 0};

	/*
	 * Allow up to 20 ChangeSets with $REGRESSION set to not be logged.
	 */
	if (getenv("BK_REGRESSION") && (logs_pending(0, 0) < 20)) return;

	unless (rev) {
		s = sccs_init(s_cset, 0, 0);
		assert(s);
		d = sccs_top(s);
		assert(d);
		rev = rev_buf;
		strcpy(rev_buf, d->rev);
		sccs_free(s);
	}

	unless (l & LOG_OPEN) sendConfig("config@openlogging.org", rev);
	if (streq("none", to)) return;
	if (getenv("BK_TRACE_LOG") && streq(getenv("BK_TRACE_LOG"), "YES")) {
		printf("Sending ChangeSet to %s...\n", logAddr());
		fflush(stdout);
	}
	if (l & LOG_OPEN) {
		pid = spawnvp_ex(_P_NOWAIT, log_av[0], log_av);
		unless (quiet) {
			if (pid == -1) {
				printf("Error: cannot spawn bk _log\n");
			} else {
				printf("Sending ChangeSet log ...\n");
			}
		}
		fflush(stdout);
		return;
	}

	/*
	 * If we get here, we are doing old style mail base logging
	 * (for close logging project)
	 */
	strcpy(start_rev, rev);
	p = start_rev;
	while (*p) { if (*p++ == '.') dotCount++; }
	p--;
	while (*p != '.') p--;
	p++;
	if (dotCount == 4) {
		n = atoi(p) - 5;
	} else {
		n = atoi(p) - 10;
	}
	if (n < 0) n = 1;
	sprintf(p, "%d", n);
	sprintf(commit_log, "%s/commit_log%d", TMP_PATH, getpid());
	f = fopen(commit_log, "wb");
	fprintf(f, "---------------------------------\n");
	fclose(f);
	sprintf(buf, "bk cset -r+ | bk sccslog - >> %s", commit_log);
	system(buf);
	f = fopen(commit_log, "ab");
	fprintf(f, "---------------------------------\n\n");
	status(0, f);
	config(f);
	fclose(f);
	sprintf(buf, "bk cset -c -r%s..%s >> %s", start_rev, rev, commit_log);
	system(buf);

	sprintf(subject, "BitKeeper ChangeSet log: %s", package_name());
	pid = spawnvp_ex(_P_NOWAIT, mail_av[0], mail_av);
	unless (quiet) fprintf(stdout, "Sending ChangeSet log via mail...\n");
	if (pid == -1) unlink(commit_log);
	fflush(stdout); /* needed for citool */
}

void
config(FILE *f)
{
	kvpair	kv;
	time_t	tm;
	FILE	*f1;
	MDBM	*db = loadConfig(".", 1);
	char	buf[MAXLINE], aliases[MAXPATH], *dspec;
	char	s_cset[MAXPATH] = CHANGESET;
	sccs	*s;
	delta	*d;

	dspec = "$each(:FD:){Proj:      (:FD:)\\n}ID:        :KEY:\n";
	do_prsdelta(s_cset, "1.0", 0, dspec, f);
	fprintf(f, "%-10s %s", "User:", sccs_getuser());
	fprintf(f, "\n%-10s %s", "Host:", sccs_gethost());
	fprintf(f, "\n%-10s %s\n", "Root:", fullname(".", 0));
	sprintf(buf, "%slog/parent", BitKeeper);
	if (exists(buf)) {
		FILE	*f1;

		f1 = fopen(buf, "rt");
		if (fgets(buf, sizeof(buf), f1)) {
			fprintf(f, "%-10s %s", "Parent:", buf);
		}
		fclose(f1);
	}
 	s = sccs_init(s_cset, INIT_NOCKSUM, NULL);
	assert(s && s->tree);
	s->state &= ~S_SET;
	d = sccs_top(s);
	fprintf(f, "%-10s %s\n", "Revision:", d->rev);
	fprintf(f, "%-10s ", "Cset:");
	sccs_pdelta(s, d, f);
	fputs("\n", f);
	sccs_free(s);
	tm = time(0);
	fprintf(f, "%-10s %s", "Date:", ctime(&tm));
	assert(db);
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		sprintf(buf, "%s:", kv.key.dptr);
		fprintf(f, "%-10s %s\n", buf, kv.val.dptr);
	}
	mdbm_close(db);
	if (db = loadOK()) {
		fprintf(f, "Logging OK:\n");
		for (kv = mdbm_first(db);
		    kv.key.dsize != 0; kv = mdbm_next(db)) {
			fprintf(f, "\t%s\n", kv.key.dptr);
		}
		mdbm_close(db);
	}
	fprintf(f, "User List:\n");
	bkusers(0, 0, "\t", f);
	sprintf(buf, "%setc/SCCS/s.aliases", BitKeeper);
	if (exists(buf)) {
		fprintf(f, "Alias  List:\n");
		gettemp(aliases, "bk_aliases");
		sprintf(buf, "%setc/SCCS/s.aliases", BitKeeper);
		get(buf, SILENT|PRINT, aliases);
		f1 = fopen(aliases, "r");
		while (fgets(buf, sizeof(buf), f1)) {
			if ((buf[0] == '#') || (buf[0] == '\n')) continue;
			fprintf(f, "\t%s", buf);
		}
		fclose(f1);
		unlink(aliases);
	}
}

int
config_main(int ac, char **av)
{
	if (ac == 2 && streq("--help", av[1])) {
		system("bk help config");
		return (1);
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "Can't find package root\n");
		return (1);
	}
	config(stdout);
	return (0);
}

void
sendConfig(char *to, char *rev)
{
	char 	*av[] = { "bk", "_lconfig", 0 };

	/*
	 * Allow up to 20 ChangeSets with $REGRESSION set to not be logged.
	 */
	if (getenv("BK_REGRESSION") && (logs_pending(1, 0) < 20)) return;

	spawnvp_ex(_P_NOWAIT, av[0], av);
}
