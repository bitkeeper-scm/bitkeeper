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
	u32	lod:1;
	u32	resync:1;
	u32	no_autoupgrade:1;
} c_opts;

extern	char	*editor, *bin, *BitKeeper;
extern	int	do_clean(char *, int);
extern	int	loggingask_main(int, char**);
private	int	make_comment(char *cmt, char *commentFile);
private int	do_commit(c_opts opts, char *sym,
					char *pendingFiles, char *commentFile);

int
commit_main(int ac, char **av)
{
	int	c, doit = 0, force = 0, getcomment = 1;
	char	buf[MAXLINE], s_cset[MAXPATH] = CHANGESET;
	char	commentFile[MAXPATH], pendingFiles[MAXPATH] = "";
	char	*sym = 0, *commit_list = 0;
	c_opts	opts  = {0, 0, 0, 0, 0};

	if (ac > 1 && streq("--help", av[1])) {
		system("bk help commit");
		return (1);
	}

	sprintf(commentFile, "%s/bk_commit%d", TMP_PATH, getpid());
	while ((c = getopt(ac, av, "aAdf:FLRqsS:y:Y:")) != -1) {
		switch (c) {
		    case 'a':	opts.alreadyAsked = 1; break;
		    case 'd': 	doit = 1; break;
		    case 'f':	strcpy(pendingFiles, optarg); break;
		    case 'F':	force = 1; break;
		    case 'L':	opts.lod = 1; break;
		    case 'R':	BitKeeper = "../BitKeeper/";
				opts.resync = 1;
				break;
		    case 's':	/* fall thru  */ 	/* internal option */
		    case 'q':	opts.quiet = 1; break;
		    case 'S':	sym = optarg; break;
		    case 'y':	doit = 1; getcomment = 0;
				if (make_comment(optarg, commentFile)) {
					return (1);
				}
				break;
		    case 'Y':	doit = 1; getcomment = 0;
				strcpy(commentFile, optarg);
				break;
		    case 'A':	/* internal option for regression test only */
				/* do not document			    */
				opts.no_autoupgrade = 1; break;
		    default:	system("bk help -s commit");
				return (1);
		}
	}

	/* XXX: stub out lod operations */
	if (opts.lod) {
		fprintf(stderr,
		    "commit: commit -L currently non-operational.\n"
		    "        To commit to a new lod, first run: bk createlod\n"
		    "        Then commit as you normally would, without -L\n"
		    "        See: bk createlod --help\n");
		return(1);
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
			sprintf(pendingFiles,
					"%s/bk_list%d", TMP_PATH, getpid());
			setmode(0, _O_TEXT);
			f = fopen(pendingFiles, "wb");
			assert(f);
			while (fgets(buf, sizeof(buf), stdin)) {
				fputs(buf, f);
			}
			fclose(f);
		}
	} else {
		sprintf(pendingFiles, "%s/bk_list%d", TMP_PATH, getpid());
		sprintf(buf, "bk sfind -s,,p -C > %s", pendingFiles);
		if (system(buf) != 0) {
			unlink(pendingFiles);
			unlink(commentFile);
			getmsg("duplicate_IDs", 0, 0, stdout);
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
		sprintf(buf,
		    "bk sccslog -CA - < %s > %s", pendingFiles, commentFile);
		system(buf);
	}
	do_clean(s_cset, SILENT);
	if (doit) return (do_commit(opts, sym, pendingFiles, commentFile));

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
			return(do_commit(opts, sym, pendingFiles, commentFile));
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

int
cat(char *file)
{
	MMAP	*m = mopen(file, "r");

	unless (m) return (-1);
	unless (write(1, m->mmap, m->size) == m->size) {
		mclose(m);
		return (-1);
	}
	mclose(m);
	return (0);
}

private void
notice(char *key)
{
	printf(
	    "==============================================================\n");
	getmsg(key, 0, 0, stdout);
	printf(
	    "==============================================================\n");
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

private int
do_commit(c_opts opts, char *sym, char *pendingFiles, char *commentFile)
{
	int	hasComment = (exists(commentFile) && (size(commentFile) > 0));
	int	rc;
	int	l;
	char	buf[MAXLINE], sym_opt[MAXLINE] = "";
	char	s_cset[MAXPATH] = CHANGESET;
	sccs	*s;
	delta	*d;
	FILE 	*f;

	l = logging(0, 0, 0);
	unless (ok_commit(l, opts.alreadyAsked)) {
		if (commentFile) unlink(commentFile);
		if (pendingFiles) unlink(pendingFiles);
		return (1);
	}
	if (pending(LOGGING_OK)) {
		int	len = strlen(LOGGING_OK);

		/*
		 * Redhat 5.2 cannot handle opening a file
		 * in both read and write mode fopen(file, "rt+") at the
		 * same time. Win32 is likely to have same problem too
		 * So we open the file in read mode close it and re-open
		 * it in write mode
		 */
		f = fopen(pendingFiles, "rb");
		assert(f);
		while (fnext(buf, f)) {
			if (strneq(LOGGING_OK, buf, len) && buf[len] == '@') {
				goto out;
			}
		}
		fclose (f);
		f = fopen(pendingFiles, "ab");
		fprintf(f, "%s@+\n", LOGGING_OK);
out:		fclose(f);
	}
	if (sym) sprintf(sym_opt, "-S\"%s\"", sym);
	sprintf(buf, "bk cset %s %s %s %s%s < %s",
		opts.lod ? "-L": "", opts.quiet ? "-q" : "", sym_opt,
		hasComment? "-Y" : "", hasComment ? commentFile : "",
		pendingFiles);
	rc = system(buf);
/*
 * Do not enable this until
 * new BK binary is fully deployed
 */
#ifdef NEXT_RELEASE
	unless (opts.resync) {
		sprintf(buf, "%setc/SCCS/x.dfile", BitKeeper);
		close(open(buf, O_CREAT|O_APPEND|O_WRONLY, GROUP_MODE));
	}
#endif
	if (unlink(commentFile)) perror(commentFile);
	if (unlink(pendingFiles)) perror(pendingFiles);
	if (rc) return (rc); /* if commit failed do not send log */
	notify();
	s = sccs_init(s_cset, 0, 0);
	assert(s);
	d = findrev(s, 0);
	assert(d);
	strcpy(buf, d->rev);
	sccs_free(s);
	logChangeSet(l, buf, opts.quiet);
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
	char	commit_log[MAXPATH], buf[MAXLINE], *p;
	char	subject[MAXLINE];
	char	start_rev[1024];
	char	*to = logAddr();
	FILE	*f;
	int	dotCount = 0, junk, n;
	pid_t	pid;
	char 	*http_av[] = {
		"bk",
		"log",
		"http://www.bitkeeper.com/cgi-bin/logit",
		to,
		subject, 
		commit_log,
		0
	};
	char	*mail_av[] = {
		"bk",
		"_mail",
		to,
		subject,
		commit_log,
		0
	};

	/*
	 * Allow up to 20 ChangeSets with $REGRESSION set to not be logged.
	 */
	if (getenv("BK_REGRESSION") &&
	    (sscanf(rev, "%d.%d", &junk, &n) == 2)) {
		if (n <= 20) return;
	}

	unless (l & LOG_OPEN) sendConfig("config@openlogging.org", rev);
	if (streq("none", to)) return;

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
	config(0, f);
	fclose(f);
	sprintf(buf, "bk cset -c -r%s..%s >> %s", start_rev, rev, commit_log);
	system(buf);
	if (getenv("BK_TRACE_LOG") && streq(getenv("BK_TRACE_LOG"), "YES")) {
		printf("Sending ChangeSet to %s...\n", logAddr());
		fflush(stdout);
	}

	sprintf(subject, "BitKeeper ChangeSet log: %s", package_name());
	if (l & LOG_OPEN) {
		pid = spawnvp_ex(_P_NOWAIT, http_av[0], http_av);
		fprintf(stdout, "Sending ChangeSet log via http...\n");
	} else {
		pid = spawnvp_ex(_P_NOWAIT, mail_av[0], mail_av);
		fprintf(stdout, "Sending ChangeSet log via mail...\n");
	}
	if (pid == -1) unlink(commit_log);
	fflush(stdout); /* needed for citool */
}

void
config(char *rev, FILE *f)
{
	char	*dspec;
	kvpair	kv;
	time_t	tm;
	FILE	*f1;
	MDBM	*db = loadConfig(".", 1);
	char	buf[MAXLINE], aliases[MAXPATH];
	char	s_cset[MAXPATH] = CHANGESET;

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
	if (rev) fprintf(f, "%-10s %s\n", "Revision:", rev);
	fprintf(f, "%-10s ", "Cset:");
	do_prsdelta(s_cset, rev, 0, ":KEY:\n", f);
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
		sprintf(aliases, "%s/bk_aliasesX%d", TMP_PATH, getpid());
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
	char	*rev = av[1] && strneq("-r", av[1], 2) ? &av[1][2] : 0;

	if (ac == 2 && streq("--help", av[1])) {
		system("bk help config");
		return (1);
	}
	if (sccs_cd2root(0, 0)) {
		fprintf(stderr, "Can't find package root\n");
		return (1);
	}
	config(rev, stdout);
	return (0);
}

void
sendConfig(char *to, char *rev)
{
	char	subject[MAXLINE], config_log[MAXPATH];
	FILE	*f;
	int	n, junk;
	char 	*av[] = {
		"bk",
		"log",
		"http://www.bitkeeper.com/cgi-bin/logit",
		to,
		subject, 
		config_log,
		0
	};

	/*
	 * Allow up to 20 ChangeSets with $REGRESSION set to not be logged.
	 */
	if (getenv("BK_REGRESSION") &&
	    (sscanf(rev, "%d.%d", &junk, &n) == 2)) {
		if (n <= 20) return;
	}

	gettemp(config_log, "config");
	unless (f = fopen(config_log, "wb")) return;
	status(0, f);
	config(rev, f);
	fclose(f);
	sprintf(subject, "BitKeeper config: %s", package_name());
	n = spawnvp_ex(_P_NOWAIT, av[0], av);
	unless (WIFEXITED(n)) unlink(config_log);
}
