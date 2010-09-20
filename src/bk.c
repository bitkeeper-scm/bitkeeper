#include "system.h"
#include "sccs.h"
#include "range.h"
#include "bkd.h"
#include "cmd.h"
#include "logging.h"
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"
#include "nested.h"
#include "progress.h"

#define	BK "bk"

char	*editor = 0, *bin = 0;
char	*BitKeeper = "BitKeeper/";	/* XXX - reset this? */
char	**bk_environ;
jmp_buf	exit_buf;
int	bk_isSubCmd = 0;	/* if 1, BK called us and sent seed */
int	spawn_tcl;		/* needed in crypto.c:bk_preSpawnHook() */
ltc_math_descriptor	ltc_mp;
char	*prog;			/* name of the bk command being run, like co */
char	*title;			/* if set, use this instead of prog for pbars */
char	*start_cwd;		/* if -R or -P, where did I start? */
unsigned int turnTransOff;	/* for transactions, see nested.h */

private	char	*buffer = 0;	/* copy of stdin */
private char	*log_versions = "!@#$%^&*()-_=+[]{}|\\<>?/";	/* 25 of 'em */
#define	LOGVER	0

private	void	bk_atexit(void);
private	void	bk_cleanup(int ret);
private	char	cmdlog_buffer[MAXPATH*4];
private	int	cmdlog_flags;
private	int	cmdlog_locks;
private int	cmd_run(char *prog, int is_bk, int ac, char **av);
private	int	hasArgs(char **av);
private	void	showproc_start(char **av);
private	void	showproc_end(char *cmdlog_buffer, int ret);

#define	MAXARGS	1024
#define	MAXPROCDEPTH	30	/* fallsafe, don't recurse deeper than this */

private void
save_gmon(void)
{
	char	buf[200];
	int	i = 0;

	do {
		sprintf(buf, "gmon.%d", i++);
	} while (exists(buf));
	rename("gmon.out", buf);
}

int
main(int volatile ac, char **av, char **env)
{
	int	i, c, ret;
	int	is_bk = 0, dashr = 0, remote = 0, quiet = 0;
	int	iterator = 0, nested = 0;
	char	*csp, *p, *dir = 0, *locking = 0;
	char	*envargs = 0;
	char	**sopts = 0;
	longopt	lopts[] = {
		{ "title;", 300 },	// title for progress bar
		{ "cd;", 301 },		// cd to dir
		{ 0, 0 },
	};

	trace_init(av[0]);
	ltc_mp = ltm_desc;
	for (i = 3; i < 20; i++) close(i);
	reserveStdFds();
	spawn_preHook = bk_preSpawnHook;
	showproc_start(av);

	if (getenv("BK_REGRESSION") && exists("/build/die")) {
		fprintf(stderr, "Forced shutdown.\n");
		exit(1);
	}

#ifdef	WIN32
	/*
	 * See if the win32 layer needs to enable retry loops. This
	 * can be set to the number of times to try, but if it's set
	 * to anything other than a number (or zero) it means don't
	 * retry.
	 */
	if (p = getenv("BK_WIN_NORETRY")) win32_retry(strtol(p, 0, 10));
#endif

	signal(SIGPIPE, SIG_IGN); /* no-op on win32 */

	/*
	 * Support redirection via BK_STDIO=$HOST:$PORT.
	 * Similarly for STDERR except that it is write only.
	 */
	if ((p = getenv("BK_STDIO")) && !streq(p, "DONE")) {
		int	sock;

		if ((sock = tcp_connect(p, -1)) > 0) {
			dup2(sock, 0);
			dup2(sock, 1);
			close(sock);
		} else {
			fprintf(stderr, "Unable to reset stdio to %s\n", p);
		}
		putenv("BK_STDIO=DONE");
	}
	if ((p = getenv("BK_STDERR")) && !streq(p, "DONE")) {
		int	sock;

		if ((sock = tcp_connect(p, -1)) > 0) {
			dup2(sock, 2);
			close(sock);
		} else {
			fprintf(stderr, "Unable to reset stderr to %s\n", p);
		}
		putenv("BK_STDERR=DONE");
	}

	/*
	 * Windows seems to have a problem with stderr under rxvt's.
	 * Force unbuffered mode.
	 */
	setbuf(stderr, 0);

	cmdlog_buffer[0] = 0;
	cmdlog_flags = 0;
	if (i = setjmp(exit_buf)) {
		ret = (i >= 1000) ? (i - 1000) : 1;
		goto out;
	}
	atexit(bk_atexit);
	platformInit(av);
	bk_environ = env;

	i = rand_checkSeed();
	if (getenv("_BK_DEBUG_CHECKSEED")) {
		char	val[64];

		sprintf(val, "%d", i);
		cmdlog_addnote("checkseed", val);
	}
	bk_isSubCmd = !i;

	/*
	 * The goal of this is to cause bk to not call a different bk as a
	 * subprocess.  Coverity hit a problem where their bkd was still
	 * running after they had installed over it.  So they were calling
	 * a newer bk and something blew up.
	 */
	if (bk_isSubCmd &&
	    (p = getenv("_BK_VERSION")) && !streq(bk_vers, p) &&
	    !getenv("_BK_NO_VERSION_CHECK")) {
		error("bk: %s being called by %s not supported.\n",
		    bk_vers, p);
		if (getenv("_BK_IN_BKD")) drain();
		exit(1);
	}
	safe_putenv("_BK_VERSION=%s", bk_vers);

	unless (bin) {
		fprintf(stderr,
		    "Unable to find the BitKeeper bin directory, aborting\n");
		return (1);
	}
	fslayer_enable(1);
	unless (getenv("BK_TMP")) bktmpenv();

	/*
	 * Determine if this should be a trial version of bk.
	 * Add versions that are not tagged will automaticly expire
	 * in 2 weeks.
	 */
	if (test_release && (time(0) > (time_t)bk_build_timet + 3600*24*14)) {
		char	*nav[] = {"version", 0};

		version_main(1, nav);
		exit(1);
	}

	/* stderr write(2) wrapper for progress bars */
	stderr->_write = progress_syswrite;

	/*
	 * Parse our options if called as "bk".
	 * We support most of the sfiles options.
	 */
	prog = basenm(av[0]);
	if (streq(prog, "sccs")) prog = "bk";
	if (streq(prog, "bk")) {
		if (av[1] && streq(av[1], "--help") && !av[2]) {
			system("bk help bk");
			return (0);
		}
		is_bk = 1;
		while ((c = getopt(ac, av,
			"?;^@|1aB;cdDgGhjL|lnN|pPqr|Rs|uUxz;", lopts)) != -1) {
			switch (c) {
			    case 'N':
				dashr = nested = 1;
				/* FALLTHOUGH */
			    case '1': case 'a': case 'c': case 'd':
			    case 'D': case 'g': case 'G': case 'j': case 'l':
			    case 'n': case 'p': case 'u': case 'U': case 'x':
			    case 'h': case '^':
 				sopts = addLine(sopts,
 				    aprintf("-%c%s", c, optarg ? optarg : ""));
				break;
			    case '?': envargs = optarg; break;
			    case '@': remote = 1; break;
			    case 'B': buffer = optarg; break;
			    case 'q': quiet = 1; break;
			    case 'L': locking = optarg; break;
			    case 'P':				/* doc 2.0 */
				if (proj_cd2product() && proj_cd2root()) {
					fprintf(stderr, 
					    "bk: Cannot find product root.\n");
					return(1);
				}
				break;
			    case 'r':				/* doc 2.0 */
				if (dashr) {
					fprintf(stderr,
					    "bk: Only one -r allowed\n");
					return (1);
				}
				dir = optarg;
				dashr++;
				break;
			    case 'R':				/* doc 2.0 */
				if (proj_cd2root()) {
					fprintf(stderr, 
					    "bk: Cannot find package root.\n");
					return(1);
				}
				break;
			    case 's': iterator = 1; break;	// nested_each 
			    case 'z': break;	/* remote will eat it */
			    case 300: title = optarg; break;
			    case 301:
				if (chdir(optarg)) {
					fprintf(stderr,
					    "bk: Cannot chdir to %s\n", p);
					return (1);
				}
				break;
			    default: bk_badArg(c, av);
			}
		}

		/*
		 * Make -A/-N be honored only in a nested collection.
		 * If we just ignore them otherwise the right thing
		 * happens, i.e,
		 * bk -Ar co => bk -r co
		 * bk -N co => bk -r co
		 */
		if ((nested || iterator) && !proj_isEnsemble(0)) {
			nested = iterator = 0;
		}
		if (nested) putenv("_BK_FIX_NESTED_PATH=YES");

		/* -'?VAR=val&VAR2=val2' */
		if (envargs) {
			hash	*h = hash_new(HASH_MEMHASH);

			if (hash_fromStr(h, envargs)) return (1);
			EACH_HASH(h) {
				safe_putenv("%s=%s",
				    (char *)h->kptr, (char *)h->vptr);
			}
			hash_free(h);
		}
		if (remote) {
			start_cwd = strdup(proj_cwd());
			cmdlog_start(av, 0);
			ret = remote_bk(quiet, ac, av);
			goto out;
		}

		if (iterator && !getenv("_BK_ITERATOR")) {
			putenv("_BK_ITERATOR=YES");
			ret = nested_each(quiet, ac, av);
			goto out;
		}

		if (dashr) {
			if (dir) {
				unless (chdir(dir) == 0) {
					perror(dir);
					return (1);
				}
			} else {
				// Silently try and go to the product
				if (nested) proj_cd2product();
				if (proj_cd2root()) {
					fprintf(stderr,
					    "bk: Cannot find package root.\n");
					return(1);
				}
			}
		}
		start_cwd = strdup(proj_cwd());
		if (locking) {
			int	waitsecs;

			if (locking[1]) {
				waitsecs = strtoul(locking+1, &p, 10);
				unless (*p == 0) {
bad_locking:				fprintf(stderr,
					    "bk: unknown option -L%s\n",
					    locking);
					ret = 1;
					goto out;
				}
			} else if ((p = proj_configval(0, "lockwait")) &&
			    isdigit(*p)) {
				waitsecs = atoi(p);
			} else {
				waitsecs = 30;
			}
			while (1) {
				if (*locking == 'r') {
					unless (repository_rdlock(0)) break;
				} else if (*locking == 'w') {
					unless (repository_wrlock(0)) break;
				} else {
					goto bad_locking;
				}
				if (waitsecs == 0) {
					fprintf(stderr,
					    "%s: failed to get repository %s "
					    "lock.\n",
					    av[0],
					    (*locking == 'r') ?
					    "read" : "write");
					ret = 2;
					goto out;
				}
				--waitsecs;
				sleep(1);
			}
		}

		/*
		 * Allow "bk [-sfiles_opts] -r" as an alias for
		 * cd2root
		 * bk sfiles [-sfiles_opts]
		 */
		if (dashr && !av[optind]) {
			sopts = unshiftLine(sopts, strdup("sfiles"));
			sopts = addLine(sopts, 0);
			av = &sopts[1];
			ac = nLines(sopts);
			prog = av[0];
			goto run;
		}

		unless (prog = av[optind]) {
			if (getenv("_BK_ITERATOR")) exit(0);
			usage();
		}
		if (nested && streq(prog, "check")) {
			fprintf(stderr,
			    "bk: -N option cannot be used with check\n");
			return (1);
		}
		for (ac = 0; av[ac] = av[optind++]; ac++);
		if (dashr) {
			unless (streq(prog, "sfiles") || streq(prog, "sfind")) {
				if (streq(prog, "check")) {
					putenv("_BK_CREATE_MISSING_DIRS=1");
				}
				if (sfiles(sopts)) return (1);
				sopts = 0; /* sfiles() free'd */
				if (streq(prog, "check")) {
					putenv("_BK_CREATE_MISSING_DIRS=");
				}
				/* we have bk [-r...] cmd [opts] ... */
				/* we want cmd [opts] ... - */
				av[ac++] = "-";
				av[ac] = 0;
			}
		}
		prog = av[0];
	}

run:	trace_init(prog);	/* again 'cause we changed prog */

	/*
	 * Don't recurse too deep.
	 */
	if ((csp = getenv("_BK_CALLSTACK"))) {
		if (strcnt(csp, ':') >= MAXPROCDEPTH) {
			fprintf(stderr, "BK callstack: %s\n", csp);
			fprintf(stderr, "BK callstack too deep, aborting.\n");
			exit(1);
		}
		safe_putenv("_BK_CALLSTACK=%s:%s", csp, prog);
	} else {
		safe_putenv("_BK_CALLSTACK=%s", prog);
	}

	/*
	 * XXX - we could check to see if we are licensed for SAM and make
	 * this conditional.
	 */
	if (dashr) nested_check();
	getoptReset();
	if (exists("gmon.out")) save_gmon();

#ifdef	WIN32
	/* This gets rid of an annoying message when sfiles is killed */
	nt_loadWinSock();
#endif
	cmdlog_start(av, 0);
	if (buffer) {
		unless (streq(buffer, "stdin")) {
			fprintf(stderr, "bk: only -Bstdin\n");
			exit(1);
		}
		buffer = bktmp(0, "stdin");
		fd2file(0, buffer);
		close(0);
		open(buffer, O_RDONLY, 0);
	}
	ret = cmd_run(prog, is_bk, ac, av);
	if (locking && streq(locking, "r")) repository_rdunlock(0, 0);
	if (locking && streq(locking, "w")) repository_wrunlock(0, 0);
out:
	progress_restoreStderr();
	cmdlog_end(ret, 0);
	bk_cleanup(ret);
	/* flush stdout/stderr, needed for bk-remote on windows */
	fflush(stdout);
	fflush(stderr);
	freeLines(sopts, free);
#ifdef	WIN32
	close(1);
#endif
	if (getenv("BK_CLOSEALL")) {
		/*
		 * The old code used to close stderr here.  Add check
		 * so that can be added back if it is needed for some
		 * odd reason.
		 * --lm3di
		 */
		close(1);
		close(2);
	}

	/* close stdin so that sfiles will bail out */
	close(0);

	/* belt and suspenders */
	sfilesDied(1);

	if (ret < 0) ret = 1;	/* win32 MUST have a positive return */
	return (ret);
}

/*
 * The commands here needed to be spawned, not execed, so command
 * logging works.
 */
private int
cmd_run(char *prog, int is_bk, int ac, char **av)
{
	int	i, j;
	CMD	*cmd;
	char	cmd_path[MAXPATH];
	char	*argv[MAXARGS];

	cmd = cmd_lookup(prog, strlen(prog));

	/* Handle aliases */
	if (cmd && cmd->alias) {
		cmd = cmd_lookup(cmd->alias, strlen(cmd->alias));
		assert(cmd);
	}
	unless (is_bk || (cmd && cmd->fcn)) {
		fprintf(stderr, "%s is not a linkable command\n",  prog);
		return (1);
	}
	if (cmd) {
		/* handle pro only commands */
		if (cmd->pro) {
			unless (proj_root(0)) {
				fprintf(stderr,
				    "%s: cannot find package root\n", prog);
				return (1);
			}
			if (bk_notLicensed(0, LIC_ADM, 0)) return (1);
		}

		/* Handle restricted commands */
		if (cmd->restricted && !bk_isSubCmd) {
			/* error message matches shell message */
			cmd = 0;
		}
	}
	/* unknown commands fall through to bk.script */
	switch (cmd ? cmd->type : CMD_BK_SH) {
	    case CMD_INTERNAL:		/* handle internal command */
		assert(cmd->fcn);
		if ((ac == 2) && streq("--help", av[1])) {
			sys("bk", "help", prog, SYS);
			return (0);
		}
		return (cmd->fcn(ac, av));

	    case CMD_GUI:		/* Handle Gui script */
		return (launch_wish(cmd->name, av+1));

	    case CMD_SHELL:		/* Handle shell scripts */
		argv[0] = shell();
		sprintf(cmd_path, "%s/%s", bin, prog);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) {
			if (i >= (MAXARGS-10)) {
				fprintf(stderr, "bk: too many args\n");
				return (1);
			}
			argv[i] = av[j];
		}
		argv[i] = 0;
		return (spawn_cmd(_P_WAIT, argv));

	    case CMD_CPROG:		/* Handle C programs */
		argv[0] = cmd->name;
		for (i = 1; av[i]; i++) {
			if (i >= (MAXARGS-10)) {
				fprintf(stderr, "bk: too many args\n");
				return(1);
			}
			argv[i] = av[i];
		}
		argv[i] = 0;
		return (spawn_cmd(_P_WAIT, argv));

	    case CMD_BK_SH:
		/* Handle GUI test */
		if (streq(prog, "guitest")) {
			sprintf(cmd_path, "%s/t/guitest", bin);
			return (launch_wish(cmd_path, av+1));
		}

		/*
		 * If we get here, it is a
		 * a) bk shell function
		 *    or
		 * b) external program/script
		 * XXX This is slow because we are going thru the shell
		 */
		argv[0] = shell();
		sprintf(cmd_path, "%s/bk.script", bin);
		argv[1] = cmd_path;
		for (i = 2, j = 0; av[j]; i++, j++) {
			if (i >= (MAXARGS-10)) {
				fprintf(stderr, "bk: too many args\n");
				return (1);
			}
			argv[i] = av[j];
		}
		argv[i] = 0;
		return (spawn_cmd(_P_WAIT, argv));
	    default:
		/* should never get here */
		fprintf(stderr, "bk: '%s' not setup correctly.\n", prog);
		return (1);
	}
}

#define	LOG_BADEXIT	-100000		/* some non-valid exit */

/*
 * This function is installed as an atexit() handler.
 * In general, it shouldn't be needed but it is here as a fallback.
 *
 * On the normal exit path, both calls below will shortcircuit and
 * do nothing.
 *
 * But if exit() is called in libc or some other place that doesn't
 * include sccs.h and the macro replacing exit() with a longjmp, then
 * this function will make sure that the cmdlog is updated and bk
 * cleans up after itself.
 */
private void
bk_atexit(void)
{
	progress_restoreStderr();
	/*
	 * XXX While almost all bkd commands call this function on
	 * exit. (via the atexit() interface), there is one exception:
	 * on win32, the top level bkd service thread cannot process atexit()
	 * when the serice shutdown. (XP consider this an error)
	 * Fortunately, the bkd spawns a child process to process each
	 * new connection. The child process do follow the the normal
	 * exit path and process atexit().
	 */
	cmdlog_end(LOG_BADEXIT, 0);
	bk_cleanup(LOG_BADEXIT);
}

/*
 * Called before exiting, this function freed cached memory and looks for
 * other cleanups like stale lockfiles.
 */
private void
bk_cleanup(int ret)
{
	static	int	done = 0;

	if (done) return;
	done = 1;

	purify_list();

	/* this is attached to stdin and we have to clean it up or
	 * bktmpcleanup() will deadlock on windows.
	 */
	if (buffer && exists(buffer)) {
		close(0);
		unlink(buffer);
		free(buffer);
		buffer = 0;
	}
	notifier_flush();
	lockfile_cleanup();

	/*
	 * XXX TODO: We need to make win32 serivce child process send the
	 * the error log to via the serive log interface. (Service process
	 * cannot send messages to tty/desktop without special configuration).
	 */
	repository_lockcleanup(0);
	proj_reset(0);		/* flush data cached in proj struct */
	fslayer_enable(0);

	fslayer_cleanup();

#ifndef	NOPROC
	rmdir_findprocs();
#endif

#if	defined(__linux__)
	/*
	 * Test for filehandles left open at the end of regressions
	 * We only do with if bk exits successful as we can't fix
	 * all error paths.
	 * On linux if you comment out this section and run under
	 * valgrind these problems are easier to find.
	 * - put an abort() after the ttyprintf below
	 * - use --track-fds=yes --trace-children=yes -q
	 * 
	 * or just run with strace and see what was opened
	 */
	if ((ret == 0) && getenv("BK_REGRESSION")) {
		int	i, fd, len;
		char	buf[100];
		char	procf[100];

		for (i = 3; i < 20; i++) {
			/* if we can dup() it, then it is open */
			if ((fd = dup(i)) < 0) continue;
			close(fd);

			/* look for info in /proc */
			sprintf(procf, "/proc/%u/fd/%d", (u32)getpid(), i);
			if ((len = readlink(procf, buf, sizeof(buf))) < 0) {
				len = 0;
			}
			buf[len] = 0;
			ttyprintf("%s: warning fh %d left open %s\n",
			    prog, i, buf);
			//abort();
		}
	}
#endif
	bktmpcleanup();
	trace_free();
}

/*
 * If a command is listed below, then it will be logged in
 * BitKeeper/log/repo_log, in addition to the cmd_log.
 * Also optional flags for commands can be specified.
 */
private	struct {
	char	*name;
	int	flags;
#define	CMD_BYTES		0x00000001	/* log command byte count */
#define	CMD_WRLOCK		0x00000002	/* write lock */
#define	CMD_RDLOCK		0x00000004	/* read lock */
#define	CMD_REPOLOG		0x00000008	/* log in repolog, all below */
#define	CMD_QUIT		0x00000010	/* mark quit command */
#define	CMD_NOREPO		0x00000020	/* don't assume in repo */
#define	CMD_NESTED_WRLOCK	0x00000040	/* nested write lock */
#define	CMD_NESTED_RDLOCK	0x00000080	/* nested read lock */
#define	CMD_SAMELOCK		0x00000100	/* grab a repolock that matches
						 * the nested lock we have */
#define	CMD_COMPAT_NOSI		0x00000200	/* compat, no server info */
#define	CMD_IGNORE_RESYNC	0x00000400	/* ignore resync lock */
#define	CMD_LOCK_PRODUCT	0x00000800	/* lock product, not comp */
} repolog[] = {
	{"abort",
	    CMD_COMPAT_NOSI|CMD_WRLOCK|CMD_NESTED_WRLOCK|CMD_IGNORE_RESYNC},
	{"attach", 0},
	{"bk", CMD_COMPAT_NOSI}, /* bk-remote */
	{"check", CMD_COMPAT_NOSI},
	{"clone", 0},		/* locking handled in clone.c */
	{"collapse", CMD_WRLOCK|CMD_NESTED_WRLOCK},
	{"commit", CMD_WRLOCK|CMD_NESTED_WRLOCK},
	{"fix", CMD_WRLOCK|CMD_NESTED_WRLOCK},
	{"get", CMD_COMPAT_NOSI},
	{"kill", CMD_NOREPO|CMD_COMPAT_NOSI},
	{"license", CMD_NOREPO},
	{"pull", CMD_BYTES|CMD_WRLOCK|CMD_NESTED_WRLOCK|CMD_LOCK_PRODUCT},
	{"push", CMD_BYTES|CMD_RDLOCK|CMD_NESTED_RDLOCK|CMD_LOCK_PRODUCT},
	{"remote abort",
	     CMD_COMPAT_NOSI|CMD_WRLOCK|CMD_NESTED_WRLOCK|CMD_IGNORE_RESYNC},
	{"remote changes part1", CMD_RDLOCK},
	{"remote changes part2", CMD_RDLOCK},
	{"remote clone", CMD_BYTES|CMD_RDLOCK|CMD_NESTED_RDLOCK},
	{"remote pull part1", CMD_BYTES|CMD_RDLOCK|CMD_NESTED_RDLOCK},
	{"remote pull part2", CMD_BYTES|CMD_RDLOCK},
	{"remote push part1", CMD_BYTES|CMD_WRLOCK|CMD_NESTED_WRLOCK},
	{"remote push part2", CMD_BYTES|CMD_WRLOCK},
	{"remote push part3", CMD_BYTES|CMD_WRLOCK},
	{"remote rclone part1", CMD_NOREPO|CMD_BYTES},
	{"remote rclone part2", CMD_NOREPO|CMD_BYTES},
	{"remote rclone part3", CMD_NOREPO|CMD_BYTES},
	{"remote quit", CMD_NOREPO|CMD_QUIT},
	{"remote rdlock", CMD_RDLOCK},
	{"remote nested", CMD_SAMELOCK},
	{"remote wrlock", CMD_WRLOCK},
	{"synckeys", CMD_RDLOCK},
	{"tagmerge", CMD_WRLOCK|CMD_NESTED_WRLOCK},
	{"undo", CMD_WRLOCK|CMD_NESTED_WRLOCK},
	{ 0, 0 },
};

void
cmdlog_start(char **av, int bkd_cmd)
{
	int	i, len, do_lock = 1;
	char	*repo1, *repo2, *nlid = 0;
	char	*error_msg = 0;
	project	*proj = 0;

	cmdlog_buffer[0] = 0;
	cmdlog_flags = 0;
	for (i = 0; repolog[i].name; i++) {
		if (strieq(repolog[i].name, av[0])) {
			cmdlog_flags = repolog[i].flags;
			cmdlog_flags |= CMD_REPOLOG;
			break;
		}
	}

	/*
	 * When in http mode, each connection of push will be
	 * obtaining a separate repository lock.  For a BAM push a
	 * RESYNC dir is created in push_part2 and resolved at the end
	 * of push_part3.  The lock created at the start of push_part3
	 * needs to know that it is OK to ignore the existing RESYNC.
	 */
	if (getenv("_BKD_HTTP")) {
		if (streq(av[0], "remote push part3")) {
			putenv("_BK_IGNORE_RESYNC_LOCK=YES");
		}
	}

	for (len = 1, i = 0; av[i]; i++) {
		len += strlen(av[i]) + 1;
		if (len >= sizeof(cmdlog_buffer)) continue;
		if (i) strcat(cmdlog_buffer, " ");
		strcat(cmdlog_buffer, av[i]);
	}

	unless (proj_root(0)) goto out;

	/*
	 * If we want a product lock (push/pull) and we're in a component,
	 * pop up and grab the product's proj so we lock that one.  But
	 * only if we're not already in a nested op (_BK_TRANSACTION).
	 */
	if ((cmdlog_flags & CMD_LOCK_PRODUCT) && proj_isComponent(0) &&
	    !getenv("_BK_TRANSACTION")) {
		/* if in a product, act like a cd2product, as cmd will do it */
		proj = proj_product(0);
	}
	if (cmdlog_flags & CMD_SAMELOCK) {
		if (nlid = getenv("_NESTED_LOCK")) {
			if (nested_mine(proj, nlid, 1)) {
				cmdlog_flags |= CMD_WRLOCK;
				TRACE("SAMELOCK: got a %s", "write lock");
			} else if (nested_mine(proj, nlid, 0)) {
				cmdlog_flags |= CMD_RDLOCK;
				TRACE("SAMELOCK: got a %s", "read lock");
			} else {
				TRACE("SAMELOCK: not mine: %s", nlid);
				error_msg = aprintf("%s\n", LOCK_UNKNOWN+6);
				goto out;
			}
		}
		if (cmdlog_locks &&
		    ((cmdlog_locks & (CMD_RDLOCK|CMD_WRLOCK)) != 
		    (cmdlog_flags & (CMD_RDLOCK|CMD_WRLOCK)))) {
			TRACE("SAMELOCK: locks=%x != flags=%x",
			    cmdlog_locks, cmdlog_flags);
			error_msg = aprintf("%s\n", LOCK_UNKNOWN+6);
			goto out;
		}
	}
	/*
	 * cmdlog_locks remember all the repository locks obtained by
	 * this process so if another command wants the same lock type
	 * we won't try to get it again.  This is used for example in
	 * pull part1,2,3 so with each http:// connection will
	 * reaquire the lock, but a normal bk:// connection will only
	 * get the lock once.
	 */
	cmdlog_flags &= ~cmdlog_locks;

	/*
	 * Provide a way to do nested repo operations.  Used by import
	 * which calls commit.
	 * Locking protocol is that BK_NO_REPO_LOCK=YES means we are already
	 * locked, skip it, but clear it to avoid inheritance.
	 */
	if (cmdlog_flags &
	    (CMD_WRLOCK|CMD_RDLOCK|CMD_NESTED_WRLOCK|CMD_NESTED_RDLOCK)) {
		char	*p = getenv("BK_NO_REPO_LOCK");

		if (p && streq(p, "YES")) {
			putenv("BK_NO_REPO_LOCK=");
			do_lock = 0;
		}
	}

	if (bkd_cmd && (cmdlog_flags & (CMD_WRLOCK|CMD_RDLOCK)) &&
	    (repo1 = getenv("BK_REPO_ID")) && (repo2 = proj_repoID(proj))) {
		i = streq(repo1, repo2);
		if (i) {
			error_msg = strdup("can't connect to same repo_id\n");
			if (getenv("BK_REGRESSION")) usleep(100000);
			goto out;
		}
	}

	/*
	 * Special case for abort: if it has args it is a remote cmd so
	 * don't lock. If it is called from bk, the enclosing command is
	 * supposed to do the locking so don't lock either.
	 *
	 * This latter case applies to undo as well since it's called
	 * from bk abort.
	 */
	if ((streq("abort", av[0]) && (hasArgs(av) || bk_isSubCmd)) ||
	    (streq("undo", av[0]) && bk_isSubCmd)) {
		do_lock = 0;
	}

	if (do_lock && (cmdlog_flags & CMD_WRLOCK)) {
		if (cmdlog_flags & CMD_IGNORE_RESYNC) {
			putenv("_BK_IGNORE_RESYNC_LOCK=1");
		}
		i = repository_wrlock(proj);
		if (cmdlog_flags & CMD_IGNORE_RESYNC) {
			putenv("_BK_IGNORE_RESYNC_LOCK=");
		}
		if (i) {
			unless (bkd_cmd || !proj_root(proj)) {
				repository_lockers(proj);
				if (proj_isEnsemble(proj)) {
					nested_printLockers(proj, stderr);
				}
			}
			switch (i) {
				/* Gross +6 is to skip ERROR- */
			    case LOCKERR_LOST_RACE:
				/* It would be nice if these went to stderr
				 * for local processes.
				 */
				error_msg = aprintf("%s\n", LOCK_WR_BUSY+6);
				break;
			    case LOCKERR_PERM:
				error_msg = aprintf("%s\n", LOCK_PERM+6);
				break;
			    default:
				error_msg = aprintf("%s\n", LOCK_UNKNOWN+6);
				break;
			}
			/*
			 * Eat message sent by bkd client. (e.g. push_part1) 
			 * We need this to make the bkd error message show up
			 * on the client side.
			 */
			goto out;
		}
		cmdlog_locks |= CMD_WRLOCK;
	}
	if (do_lock && (cmdlog_flags & CMD_RDLOCK)) {
		if (i = repository_rdlock(proj)) {
			unless (bkd_cmd || !proj_root(proj)) {
				repository_lockers(proj);
				if (proj_isEnsemble(proj)) {
					nested_printLockers(proj, stderr);
				}
			}
			switch (i) {
			    case LOCKERR_LOST_RACE:
				error_msg = aprintf("%s\n", LOCK_RD_BUSY+6);
				break;
			    case LOCKERR_PERM:
				error_msg = aprintf("%s\n", LOCK_PERM+6);
				break;
			    default:
				error_msg = aprintf("%s\n", LOCK_UNKNOWN+6);
				break;
			}
			/*
			 * Eat message sent by bkd client. (e.g. pull_part1) 
			 * We need this to make the bkd error message show up
			 * on the client side.
			 */
			goto out;
		}
		cmdlog_locks |= CMD_RDLOCK;
	}
	if (do_lock &&
	    (cmdlog_flags & (CMD_NESTED_RDLOCK | CMD_NESTED_WRLOCK)) &&
	    proj_isEnsemble(proj) &&
	    /*
	     * Remote clone from a component without already having a
	     * nested lock means we're doing a populate, so skip
	     * nested locking. Note that repository locking has
	     * already been handled above.
	     */
	    !(proj_isComponent(proj) &&
		strneq(cmdlog_buffer, "remote clone", 12)) &&
	    /*
	     * Port is another command that just works at a component
	     * level, so we skip the nested locking.
	     */
	    !getenv("BK_PORT_ROOTKEY")) {
		if (nlid = getenv("_NESTED_LOCK")) {
			TRACE("checking: %s", nlid);
			unless (nested_mine(proj, nlid,
				(cmdlog_flags & CMD_NESTED_WRLOCK))) {
				error_msg = nested_errmsg();
				goto out;
			}
		} else if (cmdlog_flags & CMD_NESTED_WRLOCK) {
			unless (nlid = nested_wrlock(proj_product(proj))) {
				error_msg = nested_errmsg();
				goto out;
			}
			safe_putenv("_NESTED_LOCK=%s", nlid);
			free(nlid);
			cmdlog_locks |= CMD_NESTED_WRLOCK;
			TRACE("%s", "NESTED_WRLOCK");
		} else if (cmdlog_flags & CMD_NESTED_RDLOCK) {
			unless (nlid = nested_rdlock(proj_product(proj))) {
				error_msg = nested_errmsg();
				goto out;
			}
			safe_putenv("_NESTED_LOCK=%s", nlid);
			free(nlid);
			cmdlog_locks |= CMD_NESTED_RDLOCK;
			TRACE("%s", "NESTED_RDLOCK");
		} else {
			/* fail? */
		}
	}
	if (cmdlog_flags & CMD_BYTES) save_byte_count(0); /* init to zero */
	if (bkd_cmd) {
		char	*repoID = getenv("BK_REPO_ID");
		if (repoID) cmdlog_addnote("rmtc", repoID);
	}
out:
	if (bkd_cmd &&
	    (!(cmdlog_flags & CMD_COMPAT_NOSI) || bk_hasFeature(FEAT_SAMv3))) {
		/*
		 * COMPAT: Old bk's don't expect a serverInfo block
		 * before the error, but since we have the environment
		 * here, we can just check what version of bk we're
		 * talking to and either send the error before or
		 * after the serverInfo block
		 */
		if (sendServerInfo((cmdlog_flags & CMD_NOREPO) || error_msg)) {
			drain();
			repository_unlock(proj, 0);
			exit(1);
		}
	}
	if (error_msg) {
		error("%s", error_msg);
		free(error_msg);
		if (bkd_cmd) {
			drain();
			repository_unlock(proj, 0);
		}
		exit (1);
	}

}

private	MDBM	*notes = 0;

void
cmdlog_addnote(char *key, char *val)
{
	unless (notes) notes = mdbm_mem();
	mdbm_store_str(notes, key, val, MDBM_REPLACE);
}

int
write_log(char *file, int rotate, char *format, ...)
{
	FILE	*f;
	char	*root;
	char	path[MAXPATH];
	struct	stat	sb;
	va_list	ap;

	unless (root = proj_root(0)) return (1);
	strcpy(path, root);
	if (proj_isResync(0)) concat_path(path, path, RESYNC2ROOT);
	concat_path(path, path, "/BitKeeper/log/");
	concat_path(path, path, file);
	unless (f = fopen(path, "a")) {
		concat_path(path, root, BKROOT);
		unless (exists(path)) return (1);
		concat_path(path, root, "/BitKeeper/log/");
		concat_path(path, path, file);
		unless (mkdirf(path)) return (1);
		unless (f = fopen(path, "a")) {
			fprintf(stderr, "Cannot open %s\n", path);
			return (1);
		}
	}
	fprintf(f, "%c%s %lu %s: ",
	    log_versions[LOGVER],
	    sccs_user(), time(0), bk_vers);
	va_start(ap, format);
	vfprintf(f, format, ap);
	va_end(ap);
	fputc('\n', f);
	if (fstat(fileno(f), &sb)) {
		/* ignore errors */
		sb.st_size = 0;
		sb.st_mode = 0666;
	}
	fclose(f);

	if (sb.st_mode != 0666) chmod(path, 0666);
#define	LOG_MAXSIZE	(1<<20)
	if (rotate && (sb.st_size > LOG_MAXSIZE)) {
		char	old[MAXPATH];

		sprintf(old, "%s-older", path);
		rename(path, old);
	}
	return (0);
}

int
cmdlog_end(int ret, int bkd_cmd)
{
	int	rc = cmdlog_flags & CMD_QUIT;
	char	*log;
	int	len, savelen;
	kvpair	kv;

	notifier_flush();
	unless (cmdlog_buffer[0]) goto out;

	/* add last minute notes */
	if (cmdlog_flags & CMD_BYTES) {
		char	buf[20];

		sprintf(buf, "%u", (u32)get_byte_count());
		cmdlog_addnote("xfered", buf);
	}

	showproc_end(cmdlog_buffer, ret);

	/* If we have no project root then bail out */
	unless (proj_root(0)) goto out;

	len = strlen(cmdlog_buffer) + 20;
	EACH_KV (notes) len += kv.key.dsize + kv.val.dsize;
	log = malloc(len);
	if (ret == LOG_BADEXIT) {
		sprintf(log, "%s = ?", cmdlog_buffer);
	} else {
		sprintf(log, "%s = %d", cmdlog_buffer, ret);
	}
	savelen = len;
	len = strlen(log);
	EACH_KV (notes) {
	    	log[len++] = ' ';
		strcpy(&log[len], kv.key.dptr);
		len += kv.key.dsize - 1;
	    	log[len++] = '=';
		strcpy(&log[len], kv.val.dptr);
		len += kv.val.dsize - 1;
	}
	assert(len < savelen);
	mdbm_close(notes);
	notes = 0;
	write_log("cmd_log", 1, "%s", log);
	if (cmdlog_flags & CMD_REPOLOG) {
		/*
		 * commands in the repolog table above get written
		 * to the repo_log in addition to the cmd_log and
		 * the repo_log is never rotated.
		 */
		write_log("repo_log", 0, "%s", log);
	}
	free(log);
	if ((!bkd_cmd || (bkd_cmd && ret )) &&
	    (cmdlog_locks & (CMD_NESTED_WRLOCK|CMD_NESTED_RDLOCK))) {
		char	*nlid;

		nlid = getenv("_NESTED_LOCK");
		assert(nlid);
		if (ret && !streq(prog, "abort")) {
			if (nested_abort(0, nlid)) {
				error("%s", nested_errmsg());
			}
		} else {
			if (nested_unlock(0, nlid)) {
				error("%s", nested_errmsg());
				// XXX we need to fail command here
				//     *ret = 1;  ?
			}
		}
	}

	if (!bkd_cmd && (cmdlog_locks & (CMD_WRLOCK|CMD_RDLOCK))) {
		repository_unlock(0, 0);
	}
out:
	cmdlog_buffer[0] = 0;
	cmdlog_flags = 0;
	return (rc);
}

int
cmdlog_main(int ac, char **av)
{
	FILE	*f;
	time_t	t, cutoff = 0;
	char	*p;
	char	*version;
	char	*user;
	char	buf[MAXPATH*3];
	int	yelled = 0, c, all = 0;

	unless (proj_root(0)) return (1);
	while ((c = getopt(ac, av, "ac;", 0)) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    case 'c':
			cutoff = range_cutoff(optarg + 1);
			break;
		    default: bk_badArg(c, av);
		}
	}
	concat_path(buf, proj_root(0), "/BitKeeper/log/");
	concat_path(buf, buf, (all ? "cmd_log" : "repo_log"));
	f = fopen(buf, "r");
	unless (f) return (1);
	while (fgets(buf, sizeof(buf), f)) {
		user = buf;
		for (p = log_versions; *p; ++p) {
			if (*p == buf[0]) {
				if ((p-log_versions) > LOGVER) {
					if (yelled) goto nextline;
					printf("cannot display this "
					       "log entry; please upgrade\n");
					yelled = 1;
					goto nextline;
				}
				user = buf+1;
				break;
			}
		}

		for (p = user; (*p != ' ') && (*p != '@'); p++);
		*p++ = 0;
		t = strtoul(p, &version, 0);
		while (isspace(*version)) ++version;
		if (*version == ':') {
			p = version;
			*p = 0;
			version = 0;
		} else {
			char *q;

			unless (p = strchr(p, ':')) continue;
			*p = 0;

			unless (isalnum(*version)) {
				version = 0;
			} else  {
				for (q = 1+version; *q; ++q) {
					if ( (*q & 0x80) || (*q < ' ') ) {
						version = 0;
						break;
					}
				}
			}
		}
		unless (t >= cutoff) continue;
		printf("%s %.19s %14s %s", user, ctime(&t),
		    version ? version : "", 1+p);
nextline:	;
	}
	fclose(f);
	return (0);
}

int
launch_wish(char *script, char **av)
{
	char	*path;
	int	i, j, ac, ret, wret;
	pid_t	pid;
	struct	{
		char	*name;	/* -colormap */
		int	hasarg;	/* like -colormap name */
	}	wishargs[] = {
			{ "-colormap", 1 },
			{ "-display", 1 },
			{ "-geometry", 1 },
			{ "-name", 1 },
			{ "-sync", 0 },
			{ "-use", 1 },
			{ "-visual", 1 },
			{ 0, 0 }
		};
	char	cmd_path[MAXPATH];
	char	*argv[MAXARGS];

	/* If they set this, they can set TCL_LIB/TK_LIB as well */
	unless ((path = getenv("BK_WISH")) && executable(path)) path = 0;
	unless (path) {
		if (gui_useAqua()) {
			path = aprintf(
			    "%s/gui/bin/BitKeeper.app/Contents/MacOS/BitKeeper",
			    bin);
		} else {
			path = aprintf("%s/gui/bin/bkgui", bin);
			if (executable(path)) {
				safe_putenv("TCL_LIBRARY=%s/gui/lib/tcl8.5",
				    bin);
				safe_putenv("TK_LIBRARY=%s/gui/lib/tk8.5",
				    bin);
			} else {
				free(path);
				path = 0;
			}
		}
	}
	unless (path) {
		fprintf(stderr, "Cannot find the graphical interpreter.\n");
		exit(1);
	}
	putenv("BK_GUI=YES");
	unless (gui_useDisplay()) {
		fprintf(stderr,
		    "Cannot find a display to use (set $DISPLAY?).\n");
		exit(1);
	}
	sig_catch(SIG_IGN);
	argv[ac=0] = path;
	if (strchr(script, '/')) {
		strcpy(cmd_path, script);
	} else {
		sprintf(cmd_path, "%s/gui/lib/%s", bin, script);
	}
	argv[++ac] = cmd_path;
	/*
	 * Pass 1, get all the wish args first.
	 */
	for (i = 0; av[i]; i++) {
		if (ac >= (MAXARGS-10)) {
			fprintf(stderr, "bk: too many args\n");
			exit(1);
		}
		for (j = 0; wishargs[j].name; j++) {
			if (streq(wishargs[j].name, av[i])) {
				argv[++ac] = av[i];
				if (wishargs[j].hasarg) argv[++ac] = av[++i];
				break;
			}
		}
	}
	argv[++ac] = "--";
	/*
	 * Pass two, get all the other args.
	 */
	for (i = 0; av[i]; i++) {
		if (ac >= (MAXARGS-10)) {
			fprintf(stderr, "bk: too many args\n");
			exit(1);
		}
		for (j = 0; wishargs[j].name; j++) {
			if (streq(wishargs[j].name, av[i])) {
				break;
			}
		}
		if (wishargs[j].name) {
			if (wishargs[j].hasarg) i++;
			continue;
		}
		argv[++ac] = av[i];
	}
	argv[ac+1] = 0;
	if (streq(argv[ac], "--")) argv[ac] = 0;
	spawn_tcl = 1;
	if ((pid = spawnvp(_P_NOWAIT, argv[0], argv)) < 0) {
		fprintf(stderr, "bk: cannot spawn %s\n", argv[0]);
	}
	spawn_tcl = 0;
#ifdef	WIN32
	/*
	 * If we are about to call a GUI command hide the console
	 * since we won't be using it.  This is so that we don't have
	 * a unused console windows in the background of the GUIs.
	 * WARNING: after this we shouldn't try to do any console IO.
	 */
	FreeConsole();
#endif
	wret = waitpid(pid, &ret, 0);
	upgrade_maybeNag(0);
	if (wret < 0) {
		return (126);
	} else if (!WIFEXITED(ret)) {
		return (127);
	} else {
		return (WEXITSTATUS(ret));
	}
}

/*
 * It's an arg if it doesn't begin with '-' or is after '--'.
 */
private	int
hasArgs(char **av)
{
	int	i;

	for (i = 1; av[i]; i++) {
		if ((av[i][0] != '-') ||
		    ((av[i][1] == '-') && !av[i][2] && av[i+1])) {
			return (1);
		}
	}
	return (0);
}

static	char	*prefix;

private void
showproc_start(char **av)
{
	int	i;
	char	*p;
	FILE	*f;

	if (prefix) free(prefix);
	prefix = getenv("_BK_SP_PREFIX");
	unless (prefix) prefix = "";
	prefix = strdup(prefix);

	// Make it so that SHOWTERSE works like SHOWPROC so you can use just 1
	if ((p = getenv("BK_SHOWTERSE")) && !getenv("BK_SHOWPROC")) {
		safe_putenv("BK_SHOWPROC=%s", p);
	}
	unless (f = efopen("BK_SHOWPROC")) return;
	unless (getenv("BK_SHOWTERSE")) {
		fprintf(f, "BK  (%5u %5s)%s", getpid(), milli(), prefix);
	}
	for (i = 0; av[i]; ++i) fprintf(f, " %s", av[i]);
	fprintf(f, " [%s]", proj_cwd());
	fprintf(f, "\n");
	fclose(f);
	safe_putenv("_BK_SP_PREFIX=%s  ", prefix);
}

private void
showproc_end(char *cmdlog_buffer, int ret)
{
	FILE	*f;
	kvpair	kv;

	unless (f = efopen("BK_SHOWPROC")) return;
	unless (getenv("BK_SHOWTERSE")) {
		fprintf(f, "END (%5u %5s)%s", getpid(), milli(), prefix);
	}
	fprintf(f, " %s = %d", cmdlog_buffer, ret);
	if (notes) {
		fprintf(f, " (");
		EACH_KV(notes) fprintf(f, " %s=%s", kv.key.dptr, kv.val.dptr);
		fprintf(f, " )");
	}
	fprintf(f, "\n");
	fclose(f);
}
