#include "system.h"
#include "sccs.h"
#include <time.h>

/*
 * commit options
 */
typedef struct {
	u32	checklog:1;
	u32	quiet:1;
	u32	lod:1;
	u32	resync:1;
} c_opts;

extern	char	*editor, *bin, *BitKeeper;
extern	int	do_clean(char *, int);

private	void	make_comment(char *cmt, char *commentFile);
private int	do_commit(c_opts opts, char *sym, char *commentFile);
private	int	checkConfig();
void	cat(char *file);

/*
 *  Note: -f -s are internal options, do not document.
 *  XXX	-L is part of the lod work, not done yet.
 */
private char    *commit_help = "\n\
usage: commit  [-dFRqS:y:Y:]\n\n\
    -d		don't run interactively, just do the commit with the \n\
		default comment\n\
    -F		force a commit even if no pending deltas\n\
    -R		this tells commit that it is processing the resync directory\n\
    -q		run quietly\n\
    -S<sym>	set the symbol to <sym> for the new changeset\n\
    -y<comment>	set the changeset comment to <comment>\n\
    -Y<file>	set the changeset comment to the content of <file>\n";

int
commit_main(int ac, char **av)
{
	int	c, doit = 0, force = 0, getcomment = 1;
	char	buf[MAXLINE], s_cset[MAXPATH] = CHANGESET;
	char	commentFile[MAXPATH], pendingDeltas[MAXPATH];
	char	*sym = 0;
	c_opts	opts  = {1, 0 , 0, 0};

	if (ac > 1 && streq("--help", av[1])) {
		fputs(commit_help, stderr);
		return (1);
	}

	sprintf(commentFile, "%s/bk_commit%d", TMP_PATH, getpid());
	while ((c = getopt(ac, av, "dfFLRqsS:y:Y:")) != -1) {
		switch (c) {
		    case 'd': 	doit = 1; break;
		    case 'f':	opts.checklog = 0; break;
		    case 'F':	force = 1; break;
		    case 'L':	opts.lod = 1; break;
		    case 'R':	BitKeeper = "../BitKeeper/";
				opts.resync = 1;
				break;
		    case 's':	/* fall thru  */ 	/* internal option */
		    case 'q':	opts.quiet = 1; break;
		    case 'S':	sym = optarg; break;
		    case 'y':	doit = 1; getcomment = 0;
				make_comment(optarg, commentFile);
				break;
		    case 'Y':	doit = 1; getcomment = 0;
				strcpy(commentFile, optarg);
				break;
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
		printf("Can not find root directory\n");
		exit(1);
	}
	unless(opts.resync) remark(opts.quiet);
	sprintf(pendingDeltas, "%s/bk_list%d", TMP_PATH, getpid());
	sprintf(buf, "bk sfiles -CA > %s", pendingDeltas);
	if (system(buf) != 0) {
		unlink(pendingDeltas);
		unlink(commentFile);
		gethelp("duplicate_IDs", "", stdout);
		exit(1);
	}
	if ((force == 0) && (size(pendingDeltas) == 0)) {
		unless (opts.quiet) fprintf(stderr, "Nothing to commit\n");
		unlink(pendingDeltas);
		unlink(commentFile);
		exit(0);
	}
	if (getcomment) {
		sprintf(buf,
		    "bk sccslog -C - < %s > %s", pendingDeltas, commentFile);
		system(buf);
	}
	unlink(pendingDeltas);
	do_clean(s_cset, SILENT);
	if (doit) exit(do_commit(opts, sym, commentFile));

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
			exit(do_commit(opts, sym, commentFile));
			break;
		    case 'e':
			sprintf(buf, "%s %s", editor, commentFile);
			system(buf);
			break;
		    case 'a':
Abort:			printf("Commit aborted.\n");
			unlink(pendingDeltas);
			unlink(commentFile);
			exit(1);
		}
	}
}

void
cat(char *file)
{
	MMAP	*m = mopen(file, "r");

	write(1, m->mmap, m->size);
	mclose(m);
}

private int
do_commit(c_opts opts, char *sym, char *commentFile)
{
	int	hasComment =  (exists(commentFile) && (size(commentFile) > 0));
	int	rc;
	char	buf[MAXLINE], sym_opt[MAXLINE] = "";
	char	s_cset[MAXPATH] = CHANGESET;
	char	commit_list[MAXPATH];
	sccs	*s;
	delta	*d;

	if (checkConfig() != 0) {
		unlink(commentFile);
		exit(1);
	}
#ifdef OLD_LICENSE
	if (opts.checklog) {
		if (checkLog(opts.quiet, opts.resync) != 0) {
			unlink(commentFile);
			exit(1);
		}
	} else {
		char *p = getlog(NULL , 1);
		unless (streq("commit_and_maillog", p) ||
			streq("commit_and_mailcfg", p)) {
			fprintf(stderr,
			    "do_commit: need to comfirm logging: <%s> \n", p);
			fprintf(stderr, "commit aborted\n");
			unlink(commentFile);
			exit(1);
		}
	}
#endif
	sprintf(commit_list, "%s/commit_list%d", TMP_PATH, getpid());
	if (sym) sprintf(sym_opt, "-S\"%s\"", sym);
	sprintf(buf, "bk sfiles -C > %s", commit_list);
	system(buf);
	sprintf(buf, "bk cset %s %s %s %s%s < %s",
		opts.lod ? "-L": "", opts.quiet ? "-q" : "", sym_opt,
		hasComment? "-Y" : "", hasComment ? commentFile : "",
		commit_list);
	rc = system(buf);
	unlink(commentFile);
	unlink(commit_list);
	notify();
	s = sccs_init(s_cset, 0, 0);
	assert(s);
	d = findrev(s, 0);
	assert(d);
	strcpy(buf, d->rev);
	sccs_free(s);
	logChangeSet(buf, opts.quiet);
	return (rc);
}

private	int
checkConfig()
{
	char	buf[MAXLINE], s_config[MAXPATH], g_config[MAXPATH];

	sprintf(s_config, "%setc/SCCS/s.config", BitKeeper);
	sprintf(g_config, "%setc/config", BitKeeper);
	unless (exists(s_config)) {
		gethelp("chkconfig_missing", bin, stdout);
		return (1);
	}
	if (exists(g_config)) do_clean(s_config, SILENT);
	get(s_config, SILENT, 0);
	sprintf(buf, "cmp -s %setc/config %s/bitkeeper.config", BitKeeper, bin);
	if (system(buf) == 0) {
		gethelp("chkconfig_inaccurate", bin, stdout);
		return (1);
	}
	return (0);
}

private	void
make_comment(char *cmt, char *commentFile)
{
	int fd;

	if ((fd = open(commentFile, O_CREAT|O_TRUNC|O_WRONLY, 0664)) == -1)  {
		perror("commit");
		exit(1);
	}
	write(fd, cmt, strlen(cmt));
	close(fd);
}


#ifdef OLD_LICENSE
int
checkLog(int quiet, int resync)
{
	char	ans[MAXLINE], buf[MAXLINE];

	strcpy(buf, getlog(NULL, quiet));
	if (strneq("ask_open_logging:", buf, 17)) {
		gethelp("open_log_query", logAddr(), stdout);
		printf("OK [y/n]? ");
		fgets(ans, sizeof(ans), stdin);
		if ((ans[0] == 'Y') || (ans[0] == 'y')) {
			setlog(&buf[17]);
			return (0);
		} else {
			gethelp("log_abort", logAddr(), stdout);
			return (1);
		}
	} else if (strneq("ask_close_logging:", buf, 18)) {
		gethelp("close_log_query", logAddr(), stdout);
		printf("OK [y/n]? ");
		fgets(ans, sizeof(ans), stdin);
		if ((ans[0] == 'Y') || (ans[0] == 'y')) setlog(&buf[18]);
		return (0);
	} else if (streq("need_seats", buf)) {
		gethelp("seat_info", "", stdout);
		return (1);
	} else if (streq("commit_and_mailcfg", buf)) {
		return (0);
	} else if (streq("commit_and_maillog", buf)) {
		return (0);
	} else {
		fprintf(stderr, "unknown return code <%s>\n", buf);
		return (1);
	}
}

int
isPhantomUser(char *user)
{
	int len = strlen(user);
	/*
	 * "patch" is a phantom user generated by import.sh
	 */
	return ((len == 6) && strneq("patch@", user, 6));
}


char *
getlog(char *user, int quiet)
{
	static char buf[MAXLINE];
	char	*dspec = ":P:@:HOST:";
	char	s_cset[MAXPATH] = CHANGESET;
	char	cset[MAXPATH], config[MAXPATH];
	char	logaddr[MAXLINE], user_buf[MAXLINE];
	char	*rc;
	kvpair	kv;
	int	seat = 1, total_user = 0;

	unless (user)  {
		sprintf(user_buf, "%s@%s", sccs_getuser(), sccs_gethost());
		user = user_buf;
	}
	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "getlog: can not find project root.\n");
		exit(1);
	}
	sprintf(user_list, "%s/bk_users%d", TMP_PATH, getpid());
	sprintf(cset, "%s/bk_cset%d", TMP_PATH, getpid());
	sprintf(config, "%s/bk_configZ%d", TMP_PATH, getpid());
	f = fopen(user_list, "wb");
	do_prs(s_cset, 0, dspec, f);
	fclose(f);
	get(s_cset, SILENT|PRINT, cset);
	f = fopen(user_list, "ab");
	f1 = fopen(cset, "rt");
	while (fgets(buf, sizeof(buf), f1)) { /* extract user */
		char	*p, *q;

		for (p = buf; *p != ' '; p++);
		for (q = ++p; *q != '|'; q++);
		*q++ = '\n'; *q = '\0';
		fputs(p, f);
	}
	fclose(f1);
	fclose(f);
	unlink(cset);

	init_aliases();
	userList = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	log_ok = mdbm_open(NULL, 0, 0, GOOD_PSIZE);
	f = fopen(user_list, "r");
	while (fgets(buf, sizeof(buf), f)) {
		chop(buf);
		mdbm_store_str(userList, cname_user(buf), "",  MDBM_INSERT);
	}
	fclose(f);
	unlink(user_list);
	/* count user */
	for (kv = mdbm_first(userList);
				kv.key.dsize != 0; kv = mdbm_next(userList)) {

		unless (isPhantomUser(kv.key.dptr)) total_user++;
	}
	mdbm_close(userList);
	sprintf(buf, "%setc/SCCS/s.config", BitKeeper);
	get(buf, SILENT|PRINT,  config);
	f1 = fopen(config, "rt");
	while (fgets(buf, sizeof(buf), f1)) {
		char	*p;

		chop(buf);
		if (strneq("logging_ok:", buf, 11)) {
			p = strchr(buf, ':');
			p++;
			while (strchr(" \t", *p)) p++;
			if (streq("YES", p)) continue; /* old syntax */
			if (streq("yes", p)) continue; /* old syntax */
			mdbm_store_str(log_ok, p, "", MDBM_INSERT);
		} else if (strneq("logging:", buf, 8)) {
			p = strchr(buf, ':');
			p++;
			while (strchr(" \t", *p)) p++;
			strcpy(logaddr, p);
		} else if (strneq("seats:", buf, 6)) {
			p = strchr(buf, ':');
			seat = atoi(++p);
		}
	}
	fclose(f1);
	unlink(config);
	if ((total_user > seat) && !is_open_logging(logaddr)) {
		rc = "need_seats";
	} else if (is_ok(user)) {
		/*
		 * This also implied mailing the config file 
		 * if it is a close logging address
		 */
		rc = "commit_and_maillog";
	} else if (is_open_logging(logaddr)) {
		sprintf(buf, "ask_open_logging:%s", user);
		rc = buf;
	} else  if (!streq(logAddr(), "none")) {
		sprintf(buf, "ask_close_logging:%s", user);
		rc = buf;
	} else {
		/* logger is "none" */
		rc = "commit_and_mailcfg";
	}
	if (userDB) mdbm_close(userDB);
	if (hostDB) mdbm_close(hostDB);
	userDB = hostDB = 0;
	return rc;
}

private int
is_ok(char *user)
{
	if (mdbm_fetch_str(log_ok, user)) return 1;
	if (mdbm_fetch_str(log_ok, cname_user(user))) return 1;
	return 0;
}

private void
do_delta(char *file, char *d_comment)
{
	sccs *s;
	delta *d = (delta *) calloc(1, sizeof(*d));

	s = sccs_init(file, 0, NULL);
	assert(s);
	d->comments = addLine(d->comments, strdup(d_comment));
	sccs_delta(s, SILENT|DELTA_DONTASK, d, 0, 0, 0);
	sccs_free(s);
}

int
setlog(char *user)
{
	char	line[MAXLINE], comment[MAXLINE];
	char	s_config[MAXPATH], g_config[MAXPATH], x_config[MAXPATH];
	int	done = 0;

	if (sccs_cd2root(0, 0) == -1) {
		fprintf(stderr, "setlog: can not find project root.\n");
		exit(1);
	}
	sprintf(g_config, "%setc/config", BitKeeper);
	sprintf(s_config, "%setc/SCCS/s.config", BitKeeper);
	sprintf(x_config, "%s/bk_configW%d", TMP_PATH, getpid());
	if (exists(g_config)) do_clean(s_config, SILENT);
	get(s_config, SILENT|GET_EDIT|GET_SKIPGET, 0);
	get(s_config, SILENT|PRINT, x_config);
	f1 = fopen(x_config, "rt");
	f = fopen(g_config, "wb");
	while (fgets(line, sizeof(line), f1)) {
		fputs(line, f);
		if (!done && (strncmp("logging:", line, 8) == 0)) {
			fprintf(f, "logging_ok:	%s\n", user);
			done = 1;
		}
	}
	fclose(f);
	fclose(f1);
	sprintf(comment, "logging_ok:%s", user);
	do_delta(s_config, comment);
	unlink(x_config);
	return (0);
}
#endif
