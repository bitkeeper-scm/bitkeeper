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
				unlink(commentFile);
				strcpy(commentFile, optarg);
				break;
		    case 'A':	/* internal option for regression test only */
				/* do not document */		/* undoc 2.0 */
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
		char	*cmd, *p;
		FILE	*f, *f1;

		cmd = aprintf("bk _sort -u | bk sccslog -fA - > %s",
								commentFile);
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
logs_pending(int ptype, int skipRecentCset, int grace)
{
#define DAY (60 * 60 * 24)
	sccs 	*s;
	delta	*d;
	char 	s_cset[] = CHANGESET;
	int	i = 0;
	time_t	now;
	time_t	graceInSeconds;

	graceInSeconds = grace * DAY; 
	s = sccs_init(s_cset, 0, 0);
	assert(s && HASGRAPH(s));
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
		if (skipRecentCset && ((now - d->date) < graceInSeconds)) {
			continue;
		}
		i++;
	}
	sccs_free(s);
	return (i);
}

private
csetCount()
{
	sccs 	*s;
	delta	*d;
	char 	s_cset[] = CHANGESET;
	int 	i = 0;

	s = sccs_init(s_cset, 0, 0);
	assert(s && HASGRAPH(s));

	for (d = s->table; d; d = d->next) i++;
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
inGracePeriod(int l)
{
	/*
	 * Eval licence and single-user license have no grace period
	 */
	if ((l&LOG_LIC_GRACE) && 
	    !(l&LOG_LIC_EXPIRED) && !(l&LOG_LIC_SINGLE) && !isEvalLicense()) {
		return (1);
	}
	return (0);
}

private int
is_VIP(int l)
{
	if ((l&LOG_LIC_OK) && !(l&LOG_LIC_SINGLE) &&
	    !inGracePeriod(l) && !(l&LOG_LIC_EXPIRED) &&
	    !isEvalLicense()) {
		return (1);
	}
	return (0);
}

private int
hasPendingClog(void)
{

	if (getenv("_BK_FORCE_PENDING_CLOG")) return (1);
	if (logs_pending(1, 1, 7) > 0) return (1);
	return (0);
}

private int
enforceConfigLog(int l)
{

	if (is_VIP(l) || !hasPendingClog()) return (0);

	if (inGracePeriod(l)) {
		printf(
"============================================================================\n"
"Warning:  You have config logs pending for a long time.\n\n" 
"\"commit\" will try to send the pending logs automatically before it exit.\n"
"Please verify the pending count is zero after bk commit is done.\n"
"To get a count of the pending logs: use the following commnd:\n"
"\t\"bk _lconfig -p\"\n\n"
"If pedning count is still non-zero after bk commit is done,\n"
"please run the following command and email its output to \n"
"support@bitmover.com:\n"
"\t\"bk _lconfig -d\"\n\n"
"============================================================================\n"
);
		return (0);
	}

	printf(
"============================================================================\n"
"Error: Max pending config log exceeded, commit aborted\n\n"
"This error indicates that the BitKeeper program is unable to contact \n"
"www.bitkeeper.com to send the configuration log.\n"
"\n"
"There are two possible causes for this error:\n"
"a) Network connectivity problem\n"
"b) Problems in the logging server at www.bitkeeper.com\n"
"\n"
"Please run the following command and email its output to \n"
"support@bitmover.com:\n"
"\t\"bk _lconfig -d\"\n\n"
"============================================================================\n"
);
	return (-1);
}

private int
enforceCsetLog()
{
#define	MAX_PENDING_LOG 40
	int l, ptype;
	int	max_pending = MAX_PENDING_LOG, log_quota;

	if (getenv("BK_NEEDMORECSETS")) max_pending += 10;
	ptype = 0;
	log_quota = max_pending - logs_pending(ptype, 1, 7);
	if (log_quota <= 0) {
		printf("Commit: forcing pending logs\n");
		system("bk log -qc2");
		if ((logs_pending(ptype, 1, 7) >= max_pending)) {
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
"\t\"bk log  -d\"\n\n"
"If you have ruled out a network connectivity problem at your local network,\n"
"please email the following information to support@bitmover.com:\n"
"a) Output of the above logging command\n"
"b) Root key of your project using the following command:\n"
"\t\"bk -R inode ChangeSet\" command.\n"
"============================================================================\n"
, MAX_PENDING_LOG);
			return (-1);
		}
	} else if (log_quota <= 10) {
		printf(
"============================================================================\n"
"Warning: BitKeeper was unable to transmit log for the previous commit.\n"
"Your log quota is now down to %d. You will not be able to commit ChangeSet\n"
"if yor log quota is down to zero. Please check your network configuration\n"
"and make sure logs are transmitted properly. You can test your log\n"
"transmission with the command \"bk log -d\".\n"
"============================================================================\n"
, log_quota);
	}

	/* log quote is greater then 10, we are OK */
	return (0);
}

private int
enforceLicense(int l)
{
#define	MAX_EVAL_CSET  150
	if (l & LOG_OPEN) {
		return (enforceCsetLog());
	} else {
		if ((l&LOG_LIC_SINGLE) && !smallTree(BK_SINGLE_THRESHOLD)) {
			printf(
"============================================================================\n"
"Error: Single user repositories are limited to %d files. If you need more\n"
"files than that in one repository, you need to buy a commercial license or\n"
"turn on open logging.\n"
"============================================================================\n"
, BK_SINGLE_THRESHOLD);
			return (-1);
		}
		if (isEvalLicense()) {
			int	cset_quota = MAX_EVAL_CSET - csetCount(); 

			if (getenv("BK_REGRESSION")) {
				cset_quota -= (MAX_EVAL_CSET - 3);
			}

			if (cset_quota <= 0) {
				printf(
"============================================================================\n"
"Error: Evaluation license are limited to %d ChangeSets. If you need to make\n"
"ChangeSet, you need to buy a commercial license or turn on open logging.\n"
"============================================================================\n"
, MAX_EVAL_CSET);
				return (-1);
			} else if (cset_quota <= 50) {
				printf(
"============================================================================\n"
"Warning: Evaluation license are limited to %d ChangeSets. Your ChangeSet\n"
"quota is now down to %d. You will not be able to commit ChangeSet if your\n"
"ChangeSet quota is down to zero. Please purchase a commercial license.\n"
"============================================================================\n"
, MAX_EVAL_CSET, cset_quota);
			}
		}
		return (enforceConfigLog(l));
	}
}

private int
do_commit(char **av,
	c_opts opts, char *sym, char *pendingFiles, char *commentFile)
{
	int	hasComment = (exists(commentFile) && (size(commentFile) > 0));
	int	status, rc, i;
	int	l, ptype, fd, fd0;
	char	buf[MAXLINE], sym_opt[MAXLINE] = "", cmt_opt[MAXPATH + 3], *p;
	char	pendingFiles2[MAXPATH] = "";
	char    s_logging_ok[] = LOGGING_OK;
	char	*cset[100] = {"bk", "cset", 0};
	FILE 	*f, *f2;

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
	cset_lock();
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
	cset_unlock();
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

	if ((fd = open(commentFile, flags, 0664)) == -1)  {
		perror("commit");
		return (1);
	}
	setmode(fd, O_TEXT);
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
	char	*log_av[] = {"bk", "log", "-q", OPENLOG_URL, 0};
	char	*mail_av[] = {"bk", "_mail", to, subject, commit_log, 0};

	/*
	 * Allow up to 20 ChangeSets with $REGRESSION set to not be logged.
	 */
	if (getenv("BK_REGRESSION") && (logs_pending(0, 0, 0) < 20)) return;

	unless (rev) {
		s = sccs_init(s_cset, 0, 0);
		assert(s);
		d = sccs_top(s);
		assert(d);
		rev = rev_buf;
		strcpy(rev_buf, d->rev);
		sccs_free(s);
	}

	unless (l & LOG_OPEN) sendConfig();
	if (streq("none", to)) return;
	if (getenv("BK_TRACE_LOG") && streq(getenv("BK_TRACE_LOG"), "YES")) {
		printf("Sending ChangeSet to %s...\n", logAddr());
		fflush(stdout);
	}
	if (l & LOG_OPEN) {
		pid = spawnvp_ex(_P_NOWAIT, log_av[0], log_av);
		unless (quiet) {
			if (pid == -1) {
				printf("Error: cannot spawn bk log\n");
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

private void
printConfig(FILE *f, char *root, char *header)
{
	MDBM *db;
	kvpair	kv;

	unless (db = loadConfig(root)) return;
	fputs(header, f);
	for (kv = mdbm_first(db); kv.key.dsize != 0; kv = mdbm_next(db)) {
		if (streq("description", kv.key.dptr) ||
		    streq("bkweb", kv.key.dptr) ||
		    streq("master", kv.key.dptr) ||
		    streq("homepage", kv.key.dptr)) {
			fprintf(f, "%-10s: %u\n", kv.key.dptr,
					adler32(0, kv.val.dptr, kv.val.dsize));
		} else {
			fprintf(f, "%-10s: %s\n", kv.key.dptr, kv.val.dptr);
		}
	}
	mdbm_close(db);
}

/*
 * Print the user list associated with the new delats in a cset
 */
int
cset_user(FILE *f, sccs *s, delta *d1, char *keylist)
{
	kvpair	kv;
	MDBM	*uDB;
	char	*cmd, *p, *q;
	char	buf[MAXLINE];
	FILE 	*f1;
	int	i = 0;
	
	/*
	 * Do a "bk sccs_cat -h -rrev ChangeSet > keylist" to extract
	 * key list for the new delta in this cset.
	 * We don't want to spawn a child process here, it is too slow.
	 * So we do this via the internal interface.
	 */
	d1->flags |= D_SET;
	sccs_cat(s, SILENT|PRINT|GET_NOHASH, keylist);
	d1->flags &= ~D_SET;

	/*
	 * Extract user@host from the delta key
	 * Input looks like this: root_key delta_key
	 */
	uDB = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	f1 = fopen(keylist,  "rt");
	while (fnext(buf, f1)) {
		p = separator(buf); /* skip root key */
		assert(p);
		p++;
		q = strchr(p, '|');
		assert(q);
		*q = 0;
		mdbm_store_str(uDB,  p, "", MDBM_INSERT);
	}
	fclose(f1);

	/*
	 * Subpress user@host if same as user@host in cset key
	 */
	p = aprintf("%s@%s", d1->user, d1->hostname);
	for (kv = mdbm_first(uDB); kv.key.dsize; kv = mdbm_next(uDB)) {
		if (streq(p, kv.key.dptr)) continue;
		fprintf(f, "\t%s\n", kv.key.dptr);
		i++;
	}
	free(p);
	mdbm_close(uDB);
	return (i);
}

void
config(FILE *f)
{
	kvpair	kv;
	time_t	tm;
	FILE	*f1;
	MDBM	*db;
	char	buf[MAXLINE], tmpfile[MAXPATH];
	char	s_cset[] = CHANGESET;
	char	*p, *dspec, *license;
	sccs	*s;
	delta	*d;

	fprintf(f, "Time_t:\t%s\n", bk_time);
	getMsg("version", bk_model(buf, sizeof(buf)), 0, f);
	fprintf(f,
	   "%6d people have made deltas.\n", bkusers(1, 0, 0, 0));
	f1 = popen("bk sfind -S -sx,c,p,n", "r");
	while (fgets(buf, sizeof (buf), f1)) fputs(buf, f);
	pclose(f1);

	tm = time(0);
	fprintf(f, "Date:\t%s", ctime(&tm));
	fprintf(f, "User:\t%s\n", sccs_getuser());
	fprintf(f, "Host:\t%s\n", sccs_gethost());
	p = sccs_root(0);
	fprintf(f, "Root:\t%u\n", adler32(0, p, strlen(p)));
	free(p);
	sprintf(buf, "%slog/parent", BitKeeper);
	if (exists(buf)) {
		f1 = fopen(buf, "rt");
		if (fgets(buf, sizeof(buf), f1)) {
			chop(buf);
			fprintf(f, "Parent:\t%u\n",
						adler32(0, buf, strlen(buf)));
		}
		fclose(f1);
	}

	license = bk_license();
	if (license) {
		if (strneq("EVAL", license, 4) ||
		    isEvalLicense() ||
		    (bk_options() & BKOPT_MONTHLY)) {
			fprintf(f, "License:\t%s\n", license);
		}
	}

 	s = sccs_init(s_cset, INIT_NOCKSUM, NULL);
	assert(s && HASGRAPH(s));
	fprintf(f, "ID:\t");
	sccs_pdelta(s, sccs_ino(s), f);
	fputs("\n", f);


	/*
	 * Mark all the cset which have been logged
	 */
	for (d = sccs_top(s); d; d = d->next) {
		if (d->published && (d->ptype == 1)) sccs_color(s, d);
	}

	/*
	 * Send info for unlogged cset
	 */
	gettemp(tmpfile, "bk_keylist");
	for (d = s->table; d; d = d->next) {
		if (d->type != 'D') continue; 
		if (d->flags & D_RED) continue; 
		fprintf(f, "Cset:\t");
		sccs_pdelta(s, d, f);
		fprintf(f," %u", d->dateFudge);
		fputs("\n", f);
		cset_user(f, s, d, tmpfile);
		fclose(fopen(tmpfile,  "w")); /* truncate it */
	}
	sccs_free(s);
	unlink(tmpfile);
	fputs("\n", f);

	printConfig(f, globalroot(), "== Global Config ==\n");
	printConfig(f, ".", "== Local Config ==\n");
	fputs("\n", f);
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
		gettemp(tmpfile, "bk_aliases");
		sprintf(buf, "%setc/SCCS/s.aliases", BitKeeper);
		get(buf, SILENT|PRINT, tmpfile);
		f1 = fopen(tmpfile, "r");
		while (fgets(buf, sizeof(buf), f1)) {
			if ((buf[0] == '#') || (buf[0] == '\n')) continue;
			fprintf(f, "\t%s", buf);
		}
		fclose(f1);
		unlink(tmpfile);
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

/*
 * Note: must run config log process in background
 */
void
sendConfig()
{
	char	cmd[1024];

	/*
	 * Allow up to 20 ChangeSets with $REGRESSION set to not be logged.
	 */
	if (getenv("BK_REGRESSION") && (logs_pending(1, 0, 0) < 20)) return;

	sprintf(cmd, "bk _lconfig > %s 2> %s &", DEV_NULL, DEV_NULL);
	system(cmd);
}
