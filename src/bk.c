#include "system.h"
#include "sccs.h"
#include "range.h"
#include "bkd.h"
#include "cmd.h"
#include "tomcrypt/mycrypt.h"
#include "tomcrypt/randseed.h"

#define	BK "bk"

extern	int	test_release;
extern	unsigned build_timet;

char	*editor = 0, *bin = 0;
char	*BitKeeper = "BitKeeper/";	/* XXX - reset this? */
char	**bk_environ;
project	*bk_proj = 0;
jmp_buf	exit_buf;
char	cmdlog_buffer[MAXPATH*4];
int	cmdlog_flags;
char 	*upgrade_msg =
"This feature is not available in this version of BitKeeper, to upgrade\n\
please contact sales@bitmover.com\n"; 
int	bk_isSubCmd = 0;	/* if 1, BK called us and sent seed */

private char	*log_versions = "!@#$%^&*()-_=+[]{}|\\<>?/";	/* 25 of 'em */
#define	LOGVER	0


int	launch_wish(char *script, char **av);
private	void	cmdlog_exit(void);
private	int	cmdlog_repo;
private	void	cmdlog_dump(int, char **);
private int	cmd_run(char *prog, int is_bk, char *sopts, int ac, char **av);
private int	usage(void);

extern	void	platformInit(char **av);
extern	int	proj_cd2root(project *p);

private int
usage()
{
	system("bk help -s bk");
	exit(1);
}

#define	MAXARGS	1024

char *
milli(void)
{
	struct	timeval	tv;
	u64	now, start;
	static	char time[20];

	gettimeofday(&tv, 0);
	unless (getenv("BK_SEC")) {
		safe_putenv("BK_SEC=%u", tv.tv_sec);
		safe_putenv("BK_MSEC=%u", tv.tv_usec / 1000);
		return ("0");
	}
	start = (u64)atoi(getenv("BK_SEC")) * (u64)1000;
	start += (u64)atoi(getenv("BK_MSEC"));
	now = (u64)tv.tv_sec * (u64)1000;
	now += (u64)(tv.tv_usec / 1000);
	sprintf(time, "%u", (u32)(now - start));
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
	int	i, c, si, is_bk = 0, dashr = 0;
	int	ret;
	char	*prog, *argv[MAXARGS];
	char	sopts[30];

	for (i = 3; i < 20; i++) close(i);
	reserveStdFds();
	spawn_preHook = bk_preSpawnHook;
	if (getenv("BK_SHOWPROC")) {
		FILE	*f;

		if (f = fopen(DEV_TTY, "w")) {
			fprintf(f, "BK (%u t: %5s)", getpid(), milli());
			for (i = 0; av[i]; ++i) fprintf(f, " %s", av[i]);
			fprintf(f, "\n");
			fclose(f);
		}
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
	assert(bin);

	bk_isSubCmd = !rand_checkSeed();

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
	if (av[1] && streq(av[1], "_realpath") && !av[2]) {
		char buf[MAXPATH], real[MAXPATH];

		getcwd(buf, sizeof(buf));
		getRealName(buf, NULL, real);
		printf("%s => %s\n", buf, real);
		exit(0);
	}
	if (av[1] && streq(av[1], "--help") && !av[2]) {
		system("bk help bk");
		exit(0);
	}

	argv[0] = "help";
	argv[1] = 0;

	if (!bk_proj || !bk_proj->root || !isdir(bk_proj->root)) {
		bk_proj = proj_init(0);
	}

	/*
	 * Parse our options if called as "bk".
	 * We support most of the sfiles options.
	 */
	sopts[si = 0] = '-';
	prog = basenm(av[0]);
	if (streq(prog, "sccs")) prog = "bk";
	if (streq(prog, "bk")) {
		is_bk = 1;
		while ((c = getopt(ac, av, "acdDeEgijlnpr|RSuUvx")) != -1) {
			switch (c) {
			    case 'a': case 'c': case 'd': case 'D':
			    case 'e': case 'E': case 'g': case 'i':
			    case 'j': case 'l': case 'n': case 'p':
			    case 'S': case 'u': case 'U': case 'v':
			    case 'x':				/* doc 2.0 */
				sopts[++si] = c;
				break;
			    case 'h':				/* undoc? 2.0 */
				return (help_main(1, argv));
			    case 'r':				/* doc 2.0 */
				if (optarg) {
					unless (chdir(optarg) == 0) {
						perror(optarg);
						return (1);
					}
					proj_free(bk_proj);
					bk_proj = proj_init(0);
				} else unless (proj_cd2root(bk_proj)) {
					fprintf(stderr, 
					    "bk: Cannot find package root.\n");
					return(1);
				}
				dashr++;
				break;
			    case 'R':				/* doc 2.0 */
				unless (proj_cd2root(bk_proj)) {
					fprintf(stderr, 
					    "bk: Cannot find package root.\n");
					return(1);
				}
				break;
			    default:
				usage();
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
				signal(SIGPIPE, SIG_IGN); /* no-op on win32 */
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

	if (streq(prog, "cmdlog")) {
		cmdlog_dump(ac, av);
		return (0);
	}

	cmdlog_start(av, 0);
	ret = cmd_run(prog, is_bk, si > 1 ? sopts : 0, ac, av);
	cmdlog_end(ret);
	exit(ret);
}

/*
 * The commands here needed to be spawned, not execed, so command
 * logging works.
 */
private int
cmd_run(char *prog, int is_bk, char *sopts, int ac, char **av)
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

#define	LOG_MAXSIZE	(1<<20)
#define	LOG_BADEXIT	-100000		/* some non-valid exit */

private void
cmdlog_exit(void)
{
	/*
	 * XXX While almost all bkd command call this function on
	 * exit. (via the atexit() interface), there is one exception:
	 * on win32, the top level bkd service thread cannot process atexit()
	 * when the serice shutdown. (XP consider this an error)
 	 * Fortuately, the bkd spawn a child process to process each
	 * new connection. The child process do follow the the normal
	 * exit path and process atexit().
	 *  
	 */
	purify_list();
	bktmpcleanup();
	if (cmdlog_buffer[0]) cmdlog_end(LOG_BADEXIT);

	/*
	 * XXX TODO: We need to make win32 serivce child process send the
	 * the error log to via the serive log interface. (Service process
	 * cannot send messages to tty/desktop without special configuration).
	 */
	repository_lockcleanup();
}

private	struct {
	char	*name;
	int	flags;
} repolog[] = {
	{"abort", CMD_FAST_EXIT},
	{"check", CMD_FAST_EXIT},
	{"commit", CMD_WRLOCK|CMD_WRUNLOCK},
	{"pull", CMD_BYTES|CMD_WRLOCK|CMD_WRUNLOCK},
	{"push", CMD_BYTES|CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote changes part1", CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote changes part2", CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote clone", CMD_BYTES|CMD_FAST_EXIT|CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote pull part1", CMD_BYTES|CMD_RDLOCK},
	{"remote pull part2", CMD_BYTES|CMD_FAST_EXIT|CMD_RDUNLOCK},
	{"remote pull", CMD_BYTES|CMD_FAST_EXIT|CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote push part1", CMD_BYTES|CMD_WRLOCK},
	{"remote push part2", CMD_BYTES|CMD_FAST_EXIT|CMD_WRUNLOCK},
	{"remote push", CMD_BYTES|CMD_FAST_EXIT|CMD_WRLOCK|CMD_WRUNLOCK},
	{"remote rclone part1", CMD_BYTES},
	{"remote rclone part2", CMD_BYTES|CMD_FAST_EXIT},
	{"synckeys", CMD_RDLOCK|CMD_RDUNLOCK},
	{"undo", 0},
	{ 0, 0 },
};

void
cmdlog_start(char **av, int httpMode)
{
	int	i, len, do_lock = 1;

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
	 * When in http mode, since push/pull part 1 and part 2 run in
	 * seperate process and the lock is tied to a process id, we must
	 * complete the lock unlock cycle in part 1. Restart the lock when
	 * we enter part 2, with the up-to-date pid.
	 */
	if (httpMode) {
		if (strneq(av[0], "remote push part", 16)) {
			int	meta = 0;
			for (i = 1; av[i]; i++) {
				if (streq("-e", av[i])) meta = 1;
			}
			if (meta) cmdlog_flags &= ~(CMD_WRLOCK|CMD_WRUNLOCK);
		}
		if (cmdlog_flags & CMD_WRLOCK) cmdlog_flags |= CMD_WRUNLOCK;
		if (cmdlog_flags & CMD_WRUNLOCK) cmdlog_flags |= CMD_WRLOCK;
		if (cmdlog_flags & CMD_RDLOCK) cmdlog_flags |= CMD_RDUNLOCK;
		if (cmdlog_flags & CMD_RDUNLOCK) cmdlog_flags |= CMD_RDLOCK;
	}

	unless (bk_proj && bk_proj->root) return;

	for (len = 1, i = 0; av[i]; i++) {
		len += strlen(av[i]) + 1;
		if (len >= sizeof(cmdlog_buffer)) continue;
		if (i) strcat(cmdlog_buffer, " ");
		strcat(cmdlog_buffer, av[i]);
	}
	if (getenv("BK_TRACE")) ttyprintf("CMD %s\n", cmdlog_buffer);

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

	if (do_lock && (cmdlog_flags & CMD_WRLOCK)) {
		if (i = repository_wrlock()) {
			unless (strneq("remote ", av[0], 7) || !bk_proj) {
				repository_lockers(bk_proj);
			}
			switch (i) {
			    case LOCKERR_LOST_RACE:
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
			if (strneq("remote ", av[0], 7)) drain();
			exit(1);
		}
	}
	if (do_lock && (cmdlog_flags & CMD_RDLOCK)) {
		if (i = repository_rdlock()) {
			unless (strneq("remote ", av[0], 7) || !bk_proj) {
				repository_lockers(bk_proj);
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
			if (strneq("remote ", av[0], 7)) drain();
			exit(1);
		}
	}
	if (cmdlog_flags & CMD_BYTES) save_byte_count(0); /* init to zero */
	if (strneq("remote ", av[0], 7)) {
		char	*repoid = getenv("BK_REPO_ID");
		if (repoid) cmdlog_addnote("rmtc", repoid);
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
write_log(char *root, char *file, int rotate, char *format, ...)
{
	FILE	*f;
	char	path[MAXPATH];
	off_t	logsize;
	va_list	ap;

	sprintf(path, "%s/BitKeeper/log/%s", root, file);
	unless (f = fopen(path, "a")) {
		sprintf(path, "%s/%s", root, BKROOT);
		unless (exists(path)) return (1);
		sprintf(path, "%s/BitKeeper/log/%s", root, file);
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
	bktmpcleanup();
	unless (cmdlog_buffer[0] && bk_proj && bk_proj->root) {
		return (flags);
	}

	/* add last minute notes */
	if (cmdlog_repo && (cmdlog_flags&CMD_BYTES)) {
		char	buf[20];

		sprintf(buf, "%u", (u32)get_byte_count());
		cmdlog_addnote("xfered", buf);
	}

	if (getenv("BK_SHOWPROC")) {
		FILE	*f;

		if (f = fopen(DEV_TTY, "w")) {
			fprintf(f, "END(%u t: %5s)", getpid(), milli());
			fprintf(f, " %s = %d\n", cmdlog_buffer, ret);
			fclose(f);
		}
	}
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
	if (write_log(bk_proj->root, "cmd_log", 0, "%s", log)) {
		goto out;
	}
	if (cmdlog_repo &&
	    write_log(bk_proj->root, "repo_log", LOG_MAXSIZE, "%s", log)) {
		goto out;
	}
	free(log);

	/*
	 * If error and repo command, force unlock, force exit
	 * See also bkd.c, bottom of do_cmds().
	 */
	if ((cmdlog_flags & CMD_WRLOCK) && ret) {
		cmdlog_flags |= CMD_WRUNLOCK;
		cmdlog_flags |= CMD_FAST_EXIT;
	}
	if ((cmdlog_flags & CMD_RDLOCK) && ret) {
		cmdlog_flags |= CMD_RDUNLOCK;
		cmdlog_flags |= CMD_FAST_EXIT;
	}
	if (cmdlog_flags & (CMD_WRUNLOCK|CMD_RDUNLOCK)) repository_unlock(0);

	cmdlog_buffer[0] = 0;
	cmdlog_repo = 0;
	cmdlog_flags = 0;
out:
	rmdir_findprocs();
	if (!getenv("NOCLOSE") && getenv("BK_REGRESSION")) {
		int	i;
		for (i = 3; i < 20; i++) {
			if (close(i)) continue;
			fprintf(stderr, "%u: warning fh %d left open\n",
			    getpid(), i);
		}
	}
	return (flags);
}

private	void
cmdlog_dump(int ac, char **av)
{
	FILE	*f;
	time_t	t, cutoff = 0;
	char	*p;
	char	*version;
	char	*user;
	char	buf[MAXPATH*3];
	int	yelled = 0, c, all = 0;
	RANGE_DECL;

	unless (bk_proj && bk_proj->root) return;
	while ((c = getopt(ac, av, "ac;")) != -1) {
		switch (c) {
		    case 'a': all = 1; break;
		    RANGE_OPTS('c', 0);
		    default:
usage:			system("bk help cmdlog");
			return;
		}
	}
	if (things && d[0]) cutoff = rangeCutOff(d[0]);
	sprintf(buf, "%s/BitKeeper/log/%s", bk_proj->root,
	    (all ? "cmd_log" : "repo_log"));
	f = fopen(buf, "r");
	unless (f) return;
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
}

int
launch_wish(char *script, char **av)
{
	char	*path;
	int	i, ret;
	pid_t	pid;
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
		}
		if (executable(path)) {
			safe_putenv("TCL_LIBRARY=%s/tcltk/lib/tcl8.4", bin);
			safe_putenv("TK_LIBRARY=%s/tcltk/lib/tk8.4", bin);
		} else {
			free(path);
			path = 0;
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
	argv[0] = path;
	if (strchr(script, '/')) {
		strcpy(cmd_path, script);
	} else {
		sprintf(cmd_path, "%s/gui/lib/%s", bin, script);
	}
	argv[1] = cmd_path;
	i = 0;
	while (1) {
		if (i >= (MAXARGS-10)) {
			fprintf(stderr, "bk: too many args\n");
			exit(1);
		}
		argv[i+2] = av[i];
		unless (av[i]) break;
		i++;
	}
	if ((pid = spawnvp_ex(_P_NOWAIT, argv[0], argv)) < 0) {
		fprintf(stderr, "bk: cannot spawn %s\n", argv[0]);
	}
#ifdef	WIN32
	/*
	 * If we are about to call a GUI command hide the console
	 * since we won't be using it.  This is so that we don't have
	 * a unused console windows in the background of the GUIs.
	 * WARNING: after this we shouldn't try to do any console IO.
	 * This does not work on Win/Me (probably also Win/98)
	 */
	unless (isWin98()) FreeConsole();
#endif
	if (waitpid(pid, &ret, 0) < 0) {
		return (126);
	} else if (!WIFEXITED(ret)) {
		return (127);
	} else {
		return (WEXITSTATUS(ret));
	}
}

char *
shell(void)
{
	char	*sh;

	/*
	 * Remember that in the regressions we have a restricted PATH.
	 * Search for BK_LIMITPATH
	 */
#ifndef	WIN32
	if (sh = getenv("BK_SHELL")) return (sh);
	if (sh = whichp("bash", 0, 1)) return (sh);
	if (sh = whichp("ksh", 0, 1)) return (sh);
#endif
	if (sh = whichp("sh", 0, 1)) return (sh);
	assert("No shell" == 0);
	return (0);	/* Windows warns otherwise */
}
