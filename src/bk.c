#include "system.h"
#include "sccs.h"
#include "range.h"
#include "bkd.h"
#include "cmd.h"
#include "tomcrypt.h"
#include "tomcrypt/randseed.h"

#define	BK "bk"

extern	int	test_release;
extern	unsigned build_timet;

char	*editor = 0, *bin = 0;
char	*BitKeeper = "BitKeeper/";	/* XXX - reset this? */
char	**bk_environ;
jmp_buf	exit_buf;
char	cmdlog_buffer[MAXPATH*4];
int	cmdlog_flags;
int	bk_isSubCmd = 0;	/* if 1, BK called us and sent seed */
int	spawn_tcl;		/* needed in crypto.c:bk_preSpawnHook() */
ltc_math_descriptor	ltc_mp;
char	*prog;

private	char	*buffer = 0;	/* copy of stdin */
private char	*log_versions = "!@#$%^&*()-_=+[]{}|\\<>?/";	/* 25 of 'em */
#define	LOGVER	0

private	void	cmdlog_exit(void);
private	int	cmdlog_repo;
private int	cmd_run(char *prog, int is_bk, int ac, char **av);
private int	usage(void);
private	void	showproc_start(char **av);
private	void	showproc_end(char *cmdlog_buffer, int ret);

private int
usage(void)
{
	system("bk help -s bk");
	exit(1);
}

#define	MAXARGS	1024

private char *
milli(void)
{
	struct	timeval	tv;
	u64	now, start;
	double	d;
	static	char time[20];	/* 12345.999\0 plus slop */

	gettimeofday(&tv, 0);
	unless (getenv("BK_SEC")) {
		safe_putenv("BK_SEC=%u", (u32)tv.tv_sec);
		safe_putenv("BK_MSEC=%u", (u32)tv.tv_usec / 1000);
		d = 0;
	} else {
		start = (u64)atoi(getenv("BK_SEC")) * (u64)1000;
		start += (u64)atoi(getenv("BK_MSEC"));
		now = (u64)tv.tv_sec * (u64)1000;
		now += (u64)(tv.tv_usec / 1000);
		d = now - start;
	}
	sprintf(time, "%6.3f", d / 1000.0);
	return (time);
}

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
main(int ac, char **av, char **env)
{
	int	i, c, si, is_bk = 0, dashr = 0, remote = 0, quiet = 0;
	int	ret;
	char	*p, *dir = 0, *locking = 0;
	char	sopts[30];

	ltc_mp = ltm_desc;
	for (i = 3; i < 20; i++) close(i);
	reserveStdFds();
	spawn_preHook = bk_preSpawnHook;
	showproc_start(av);

	if (getenv("BK_REGRESSION") && exists("/build/die")) {
		fprintf(stderr, "Forced shutdown.\n");
		exit(1);
	}

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

	unless (getenv("BK_TMP")) bktmpenv();
	/*
	 * Windows seems to have a problem with stderr under rxvt's.
	 * Force unbuffered mode.
	 */
	setbuf(stderr, 0);

	cmdlog_buffer[0] = 0;
	cmdlog_flags = 0;
	if (i = setjmp(exit_buf)) {
		i -= 1000;
		cmdlog_end(i);
		return (i >= 0 ? i : 1);
	}
	atexit(cmdlog_exit);
	platformInit(av);
	bk_environ = env;

	i = rand_checkSeed();
	if (getenv("_BK_DEBUG_CHECKSEED")) {
		sprintf(sopts, "%d", i);
		cmdlog_addnote("checkseed", sopts);
	}
	bk_isSubCmd = !i;

	unless (bin) {
		fprintf(stderr,
		    "Unable to find the BitKeeper bin directory, aborting\n");
		return (1);
	}
	if (av[1] && streq(av[1], "bin") && !av[2]) {
		printf("%s\n", bin ? bin : "no path found");
		exit(0);
	}
	if (av[1] && streq(av[1], "path") && !av[2]) {
		printf("%s\n", getenv("PATH"));
		exit(0);
	}

	/*
	 * Determine if this should be a trial version of bk.
	 * Add versions that are not tagged will automaticly expire
	 * in 2 weeks.
	 */
	if (test_release && (time(0) > (time_t)build_timet + 3600*24*14)) {
		version_main(0, 0);
		exit(1);
	}

	/* bk _realpath is mainly for win32 */
	if (av[1] && streq(av[1], "_realpath") && (!av[2] || !av[3])) {
		char buf[MAXPATH], real[MAXPATH];

		if (av[2]) {
			strcpy(buf, av[2]);
		} else {
			getcwd(buf, sizeof(buf));
		}
		getRealName(buf, NULL, real);
		printf("%s => %s\n", buf, real);
		exit(0);
	}

	/*
	 * Parse our options if called as "bk".
	 * We support most of the sfiles options.
	 */
	sopts[si = 0] = '-';
	prog = basenm(av[0]);
	if (streq(prog, "sccs")) prog = "bk";
	if (streq(prog, "bk")) {
		if (av[1] && streq(av[1], "--help") && !av[2]) {
			system("bk help bk");
			return (0);
		}
		is_bk = 1;
		while ((c =
		    getopt(ac, av, "@|1aB;cCdDgGjL|lnpqr|RuUxz;")) != -1) {
			switch (c) {
			    case '1': case 'a': case 'c': case 'C': case 'd':
			    case 'D': case 'g': case 'G': case 'j': case 'l':
			    case 'n': case 'p': case 'u': case 'U': case 'x':
				sopts[++si] = c;
				break;
			    case '@': remote = 1; break;
			    case 'B': buffer = optarg; break;
			    case 'q': quiet = 1; break;
			    case 'L': locking = optarg; break;
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
			    case 'z': break;	/* remote will eat it */
			    default:
				usage();
			}
		}

		if (remote) return (remote_bk(quiet, ac, av));

		if (dashr) {
			if (dir) {
				unless (chdir(dir) == 0) {
					perror(dir);
					return (1);
				}
			} else if (proj_cd2root()) {
				fprintf(stderr,
				    "bk: Cannot find package root.\n");
				return(1);
			}
		}
		if (locking) {
			if (streq(locking, "r")) {
				if (repository_rdlock()) {
					fprintf(stderr,
"%s: failed to get repository read lock.\n", av[0]);
					return (1);
				}
			} else if (streq(locking, "w")) {
				if (repository_wrlock()) {
					fprintf(stderr,
"%s: failed to get repository write lock.\n", av[0]);
					return (1);
				}
			} else {
				fprintf(stderr,
				    "bk: unknown option -L%s\n", locking);
				return (1);
			}
		}

		/*
		 * Allow "bk [-sfiles_opts] -r" as an alias for
		 * cd2root
		 * bk sfiles [-sfiles_opts]
		 */
		sopts[++si] = 0;
		if (dashr && !av[optind]) {
			prog = av[0] = "sfiles";
			if (si > 1) {
				av[1] = sopts;
				av[ac = 2] = 0;
			} else {
				av[ac = 1] = 0;
			}
			goto run;
		}

		unless (prog = av[optind]) usage();
		for (ac = 0; av[ac] = av[optind++]; ac++);
		if (dashr) {
			unless (streq(prog, "sfiles") || streq(prog, "sfind")) {
				if (sfiles(si > 1 ? sopts : 0)) return (1);
				/* we have bk [-r...] cmd [opts] ... */
				/* we want cmd [opts] ... - */
				av[ac++] = "-";
				av[ac] = 0;
			}
		}
		prog = av[0];
	}

run:	getoptReset();
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
	if (locking && streq(locking, "r")) repository_rdunlock(0);
	if (locking && streq(locking, "w")) repository_wrunlock(0);

	/* flush stdout/stderr, needed for bk-remote on windows */
	fflush(stdout);
	close(1);
	fflush(stderr);
	close(2);
	cmdlog_end(ret);
	exit(ret);
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
		/* Handle restricted commands */
		if ((cmd->restricted && !bk_isSubCmd) ||
		    (cmd->pro && !bk_commercial())) {
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
		return (spawn_cmd(_P_WAIT, av));

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

private void
cmdlog_exit(void)
{
	/*
	 * XXX While almost all bkd command call this function on
	 * exit. (via the atexit() interface), there is one exception:
	 * on win32, the top level bkd service thread cannot process atexit()
	 * when the serice shutdown. (XP consider this an error)
	 * Fortunately, the bkd spawns a child process to process each
	 * new connection. The child process do follow the the normal
	 * exit path and process atexit().
	 */
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
	bktmpcleanup();
	lockfile_cleanup();
	if (cmdlog_buffer[0]) cmdlog_end(LOG_BADEXIT);

	/*
	 * XXX TODO: We need to make win32 serivce child process send the
	 * the error log to via the serive log interface. (Service process
	 * cannot send messages to tty/desktop without special configuration).
	 */
	repository_lockcleanup();
	proj_reset(0);		/* flush data cached in proj struct */
}

private	struct {
	char	*name;
	int	flags;
} repolog[] = {
	{"abort", CMD_FAST_EXIT},
	{"check", CMD_FAST_EXIT},
	{"collapse", CMD_WRLOCK|CMD_WRUNLOCK},
	{"commit", CMD_WRLOCK|CMD_WRUNLOCK},
	{"fix", CMD_WRLOCK|CMD_WRUNLOCK},
	{"license", CMD_FAST_EXIT},
	{"pull", CMD_BYTES|CMD_WRLOCK|CMD_WRUNLOCK},
	{"push", CMD_BYTES|CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote changes part1", CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote changes part2", CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote clone",
	    CMD_BYTES|CMD_RDLOCK|CMD_RDUNLOCK|CMD_FAST_EXIT|CMD_BAM},
	{"remote pull part1", CMD_BYTES|CMD_RDLOCK},
	{"remote pull part2",
	    CMD_BYTES|CMD_RDUNLOCK|CMD_FAST_EXIT|CMD_BAM},
	{"remote push part1", CMD_BYTES|CMD_WRLOCK},
	{"remote push part2",
	    CMD_BYTES|CMD_FAST_EXIT|CMD_WRUNLOCK|CMD_BAM},
	{"remote push part3",
	    CMD_BYTES|CMD_FAST_EXIT|CMD_WRUNLOCK},
	{"remote rclone part1", CMD_BYTES},
	{"remote rclone part2", CMD_BYTES|CMD_FAST_EXIT|CMD_BAM},
	{"remote rclone part3", CMD_BYTES|CMD_FAST_EXIT},
	{"remote quit", CMD_FAST_EXIT},
	{"remote rdlock", CMD_RDLOCK},
	{"remote rdunlock", CMD_WRUNLOCK},
	{"remote wrlock", CMD_WRLOCK},
	{"remote wrunlock", CMD_WRUNLOCK},
	{"synckeys", CMD_RDLOCK|CMD_RDUNLOCK},
	{"undo", CMD_WRLOCK|CMD_WRUNLOCK},
	{ 0, 0 },
};

void
cmdlog_start(char **av, int httpMode)
{
	int	i, len, do_lock = 1;
	int	is_remote = strneq("remote ", av[0], 7);
	char	*repo1, *repo2;
	char	*p;

	cmdlog_buffer[0] = 0;
	cmdlog_repo = 0;
	cmdlog_flags = 0;

	for (i = 0; repolog[i].name; i++) {
		if (streq(repolog[i].name, av[0])) {
			cmdlog_flags = repolog[i].flags;
			cmdlog_repo = i;
			break;
		}
	}

	/*
	 * If either side of the connection thinks it has BAM data then
	 * we will add in the extra data passes to the protocol.
	 */
	if ((cmdlog_flags & CMD_BAM) &&
	    (((p = getenv("BK_BAM")) && streq(p, "YES")) || bp_hasBAM())){
		/* in BAM-mode, allow another part */
		cmdlog_flags &= ~CMD_FAST_EXIT;
		unless (httpMode) cmdlog_flags &= ~(CMD_RDUNLOCK|CMD_WRUNLOCK);
	}

	/*
	 * When in http mode, since push/pull part 1 and part 2 run in
	 * seperate process and the lock is tied to a process id, we must
	 * complete the lock unlock cycle in part 1. Restart the lock when
	 * we enter part 2, with the up-to-date pid.
	 */
	if (httpMode) {
		if (cmdlog_flags & CMD_WRLOCK) cmdlog_flags |= CMD_WRUNLOCK;
		if (cmdlog_flags & CMD_WRUNLOCK) cmdlog_flags |= CMD_WRLOCK;
		if (cmdlog_flags & CMD_RDLOCK) cmdlog_flags |= CMD_RDUNLOCK;
		if (cmdlog_flags & CMD_RDUNLOCK) cmdlog_flags |= CMD_RDLOCK;
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
	if (getenv("BK_TRACE")) ttyprintf("CMD %s\n", cmdlog_buffer);

	unless (proj_root(0)) return;

	/*
	 * Provide a way to do nested repo operations.  Used by import
	 * which calls commit.
	 * Locking protocol is that BK_NO_REPO_LOCK=YES means we are already
	 * locked, skip it, but change it to BK_NO_REPO_LOCK=DIDNT to make
	 * sure we don't unlock either.
	 */
	if (cmdlog_flags & (CMD_WRLOCK|CMD_RDLOCK)) {
		char	*p = getenv("BK_NO_REPO_LOCK");

		if (p && streq(p, "YES")) {
			putenv("BK_NO_REPO_LOCK=DIDNT");
			do_lock = 0;
		}
	}

	if (is_remote && (cmdlog_flags & (CMD_WRLOCK|CMD_RDLOCK)) &&
	    (repo1 = getenv("BK_REPO_ID")) && (repo2 = proj_repoID(0))) {
		i = streq(repo1, repo2);
		if (i) {
			out("ERROR-can't connect to same repo_id\n");
			if (getenv("BK_REGRESSION")) usleep(100000);
			drain();
			exit(1);
		}
	}
	if (do_lock && (cmdlog_flags & CMD_WRLOCK)) {
		if (i = repository_wrlock()) {
			unless (is_remote || !proj_root(0)) {
				repository_lockers(0);
			}
			switch (i) {
			    case LOCKERR_LOST_RACE:
				/* It would be nice if these went to stderr
				 * for local processes.
				 */
				out(LOCK_WR_BUSY);
				break;
			    case LOCKERR_PERM:
				out(LOCK_PERM);
				break;
			    default:
				out(LOCK_UNKNOWN);
				break;
			}
			out("\n");
			/*
			 * Eat message sent by bkd client. (e.g. push_part1) 
			 * We need this to make the bkd error message show up
			 * on the client side.
			 */
			if (is_remote) drain();
			exit(1);
		}
	}
	if (do_lock && (cmdlog_flags & CMD_RDLOCK)) {
		if (i = repository_rdlock()) {
			unless (is_remote || !proj_root(0)) {
				repository_lockers(0);
			}
			switch (i) {
			    case LOCKERR_LOST_RACE:
				out(LOCK_RD_BUSY);
				break;
			    case LOCKERR_PERM:
				out(LOCK_PERM);
				break;
			    default:
				out(LOCK_UNKNOWN);
				break;
			}
			out("\n");
			/*
			 * Eat message sent by bkd client. (e.g. pull_part1) 
			 * We need this to make the bkd error message show up
			 * on the client side.
			 */
			if (is_remote) drain();
			exit(1);
		}
	}
	if (cmdlog_flags & CMD_BYTES) save_byte_count(0); /* init to zero */
	if (is_remote) {
		char	*repoID = getenv("BK_REPO_ID");
		if (repoID) cmdlog_addnote("rmtc", repoID);
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
	off_t	logsize;
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
	logsize = fsize(fileno(f));
	fclose(f);

#define	LOG_MAXSIZE	(1<<20)
	if (rotate && logsize > LOG_MAXSIZE) {
		char	old[MAXPATH];

		sprintf(old, "%s-older", path);
		rename(path, old);
	} else {
		chmod(path, 0666);
	}
	return (0);
}

int
cmdlog_end(int ret)
{
	int	flags = cmdlog_flags & CMD_FAST_EXIT;
	char	*log;
	int	len, savelen;
	kvpair	kv;

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
	bktmpcleanup();
	unless (cmdlog_buffer[0]) return (flags);

	/* add last minute notes */
	if (cmdlog_repo && (cmdlog_flags&CMD_BYTES)) {
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
	write_log("cmd_log", 0, "%s", log);
	if (cmdlog_repo) write_log("repo_log", LOG_MAXSIZE, "%s", log);
	free(log);

	/*
	 * If error and repo command, force unlock, force exit
	 * See also bkd.c, bottom of do_cmds().
	 */
	if ((cmdlog_flags & (CMD_RDLOCK|CMD_WRLOCK|CMD_BAM)) && ret) {
		cmdlog_flags |= CMD_RDUNLOCK|CMD_WRUNLOCK;
		cmdlog_flags |= CMD_FAST_EXIT;
	}
	if (cmdlog_flags & (CMD_WRUNLOCK|CMD_RDUNLOCK)) repository_unlock(0);

#ifndef	NOPROC
	rmdir_findprocs();
#endif
#ifdef	MACOS_VER
	/* XXX - someday maybe they'll fix this and we can enable this check */
	if (getenv("BK_REGRESSION") && (MACOS_VER < 1030)) {
#else
	if (getenv("BK_REGRESSION")) {
#endif
		int	i;
		struct	stat sbuf;
		char	buf[100];

		for (i = 3; i < 20; i++) {
		    	buf[0] = i;	// so gcc doesn't warn us it's unused
			if (fstat(i, &sbuf)) continue;
#if	defined(F_GETFD) && defined(FD_CLOEXEC)
			if (fcntl(i, F_GETFD) & FD_CLOEXEC) continue;
#endif
			ttyprintf(
			    "%s: warning fh %d left open\n", cmdlog_buffer, i);
#ifndef	NOPROC
			sprintf(buf,
			    "/bin/ls -l /proc/%d/fd | grep '%d -> ' >/dev/tty",
			    getpid(), i);
			system(buf);
#endif
		}
	}
out:
	cmdlog_buffer[0] = 0;
	cmdlog_repo = 0;
	cmdlog_flags = 0;
	return (flags);
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
	while ((c = getopt(ac, av, "ac;")) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    case 'c':
			cutoff = range_cutoff(optarg + 1);
			break;
		    default:
			system("bk help -s cmdlog");
			return (1);
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
	int	i, j, ac, ret;
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
	if (waitpid(pid, &ret, 0) < 0) {
		return (126);
	} else if (!WIFEXITED(ret)) {
		return (127);
	} else {
		return (WEXITSTATUS(ret));
	}
}

static	char	*prefix;

private void
showproc_start(char **av)
{
	int	i;
	FILE	*f;

	if (prefix) free(prefix);
	prefix = getenv("_BK_SP_PREFIX");
	unless (prefix) prefix = "";
	prefix = strdup(prefix);

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
