#include "system.h"
#include "sccs.h" 

#define BK "bk"

char	*editor = 0, *pager = 0, *bin = 0;
char	*BitKeeper = "BitKeeper/";	/* XXX - reset this? */
project	*bk_proj = 0;
jmp_buf	exit_buf;
char	cmdlog_buffer[MAXPATH*4];
char 	*upgrade_msg =
"This feature is not available in this version of BitKeeper, to upgrade\n\
please contact sales@bitmover.com\n"; 


char	*find_wish();
char	*find_perl5();
void	cmdlog_exit(void);
int	cmdlog_repo;
private	void	cmdlog_dump(int, char **);
private int	run_cmd(char *prog, int is_bk, int ac, char **av);

extern	void	getoptReset();
extern	void	platformInit(char **av);
extern	int proj_cd2root(project *p);
int abort_main(int, char **);
int adler32_main(int, char **);
int admin_main(int, char **);
int annotate_main(int, char **);
int bkd_main(int, char **);
int cat_main(int, char **);
int changes_main(int, char **);
int check_main(int, char **);
int chksum_main(int, char **);
int clean_main(int, char **);
int clone_main(int, char **);
int commit_main(int, char **);
int config_main(int, char **);
int _createlod_main(int, char **);
int createlod_main(int, char **);
int cset_main(int, char **);
int delta_main(int, char **);
int diffs_main(int, char **);
int export_main(int, char **);
int fdiff_main(int, char **);
int find_main(int, char **);
int fix_main(int, char **);
int _g2sccs_main(int, char **);
int gca_main(int, char **);
int get_main(int, char **);
int gethelp_main(int, char **);
int gethost_main(int, char **);
int getmsg_main(int, char **);
int getuser_main(int, char **);
int gnupatch_main(int, char **);
int graft_main(int, char **);
int grep_main(int, char **);
int gone_main(int, char **);
int gzip_main(int, char **);
int help_main(int, char **);
int helpsearch_main(int, char **);
int helptopics_main(int, char **);
int idcache_main(int, char **);
int isascii_main(int, char **);
int key2rev_main(int, char **);
int keysort_main(int, char **);
int lconfig_main(int, char **);
int lines_main(int, char **);
int listkey_main(int, char **);
int lock_main(int, char **);
int lod_main(int, char **);
int log_main(int, char **);
int logging_main(int, char **);
int loggingaccepted_main(int ac, char **av);
int loggingask_main(int ac, char **av);
int loggingto_main(int, char **);
int mail_main(int, char **);
int makepatch_main(int, char **);
int merge_main(int, char **);
int converge_main(int, char **);
int mklock_main(int, char **);
int mtime_main(int, char **);
int mv_main(int, char **);
int names_main(int, char **);
int oclone_main(int, char **);
int opull_main(int, char **);
int opush_main(int, char **);
int parent_main(int, char **);
int park_main(int, char **);
int pending_main(int, char **);
int probekey_main(int, char **);
int prs_main(int, char **);
int pull_main(int, char **);
int push_main(int, char **);
int pwd_main(int, char **);
int r2c_main(int, char **);
int range_main(int, char **);
int rcs2sccs_main(int, char **);
int rcsparse_main(int, char **);
int receive_main(int, char **);
int rechksum_main(int, char **);
int renumber_main(int, char **);
int repo_main(int, char **);
int resolve_main(int, char **);
int root_main(int, char **);
int rset_main(int, char **);
int rm_main(int, char **);
int rmdel_main(int, char **);
int sane_main(int, char **);
int sccscat_main(int, char **);
int sccslog_main(int, char **);
int send_main(int, char **);
int sendbug_main(int, char **);
int setlod_main(int, char **);
int setup_main(int, char **);
int sfiles_main(int, char **);
int sfind_main(int, char **);
int sfio_main(int, char **);
int sinfo_main(int, char **);
int status_main(int, char **);
#ifdef WIN32
int socket2pipe_main(int, char **);
#endif
int sort_main(int, char **);
int sortmerge_main(int, char **);
int stripdel_main(int, char **);
int takepatch_main(int, char **);
int undo_main(int, char **);
int undos_main(int, char **);
int unedit_main(int, char **);
int unlink_main(int, char **);
int unlock_main(int, char **);
int unpark_main(int, char **);
int unwrap_main(int, char **);
int users_main(int, char **);
int version_main(int, char **);
int what_main(int, char **);
int zone_main(int, char **);
int _f2csets_main(int, char **);

/* do not change the next line, it's parsed in helpcheck.pl */
struct command cmdtbl[] = {
	{"_adler32", adler32_main},
	{"_converge", converge_main},
	{"_createlod", _createlod_main},
	{"_f2csets", _f2csets_main},
	{"_find", find_main },
	{"_g2sccs", _g2sccs_main},
	{"_get", get_main},
	{"_gzip", gzip_main }, 
	{"_keysort", keysort_main},
	{"_lconfig", lconfig_main},	
	{"_lines", lines_main},	
	{"_listkey", listkey_main},	
	{"_log", log_main},
	{"_logging", logging_main},
	{"_loggingaccepted", loggingaccepted_main},
	{"_loggingask", loggingask_main},
	{"_loggingto", loggingto_main},
	{"_mail", mail_main},
	{"_probekey", probekey_main},
#ifdef WIN32
	{"_socket2pipe", socket2pipe_main},
#endif
	{"_sort", sort_main},
	{"_sortmerge", sortmerge_main},
	{"_unlink", unlink_main },
	{"abort", abort_main},		/* doc 2.0 */	
	{"admin", admin_main},		/* doc 2.0 */
	{"annotate", annotate_main},	/* doc 2.0 */
	{"bkd", bkd_main },		/* doc 2.0 */
	{"changes", changes_main},	/* doc 2.0 */
	{"check", check_main},		/* doc 2.0 */
	{"chksum", chksum_main},	/* doc 2.0 */
	{"ci", delta_main},		/* doc 2.0 */
	{"clean", clean_main},		/* doc 2.0 */
	{"clone", clone_main},		/* doc 2.0 */
	{"co", get_main},		/* doc 2.0 */
	{"commit", commit_main},	/* doc 2.0 */
	{"config", config_main},	/* doc 2.0 */
	{"createlod", createlod_main},	/* undoc? 2.0 */
	{"cset", cset_main},		/* doc 2.0 */
	{"delta", delta_main},		/* doc 2.0 */
	{"diffs", diffs_main},		/* doc 2.0 */
	{"edit", get_main},		/* aliases */	/* doc 2.0 */
	{"export", export_main},	/* doc 2.0 */
	{"fdiff", fdiff_main},		/* undoc? 2.0 */
	{"fix", fix_main},		/* doc 2.0 */
	{"gca", gca_main},		/* doc 2.0 */
	{"get", get_main},		/* doc 2.0 */
	{"gethelp", gethelp_main},	/* undoc? 2.0 */
	{"gethost", gethost_main},	/* doc 2.0 */
	{"getmsg", getmsg_main},	/* undoc? 2.0 */
	{"getuser", getuser_main},	/* doc 2.0 */
	{"graft", graft_main},		/* undocumented */ /* undoc? 2.0 */
	{"grep", grep_main},		/* doc 2.0 */
	{"gnupatch", gnupatch_main},	/* doc 2.0 */
	{"gone", gone_main},		/* doc 2.0 */
	{"help", help_main},		/* doc 2.0 */
	{"helpsearch", helpsearch_main},	/* undoc? 2.0 */
	{"helptopics", helptopics_main},	/* undoc? 2.0 */
	{"info", sinfo_main},		/* doc 2.0 */
	{"idcache", idcache_main},	/* undoc? 2.0 */
	{"isascii", isascii_main},	/* doc 2.0 */
	{"key2rev", key2rev_main},	/* doc 2.0 */
	{"lock", lock_main},		/* doc 2.0 */
	{"lod", lod_main},	/* XXX - doc 2.0 - says doesn't work yet */
	{"makepatch", makepatch_main},		/* doc 2.0 */
	{"merge", merge_main},		/* doc 2.0 */
	{"mklock", mklock_main},	/* regression test */ /* undoc 2.0 */
	{"mtime", mtime_main},		/* regression test */ /* undoc 2.0 */
	{"mv", mv_main},		/* doc 2.0 */
	{"names", names_main},		/* doc 2.0 */
	{"new", delta_main},		/* aliases */		/* doc 2.0 */
	{"oclone", oclone_main},	/* undoc? 2.0 */
	{"opull", opull_main},		/* old pull */ /* undoc? 2.0 */
	{"opush", opush_main},		/* old push */ /* undoc? 2.0 */
	{"parent", parent_main},	/* doc 2.0 */
	{"park", park_main},		/* doc 2.0 */
	{"pending", pending_main},	/* doc 2.0 */
	{"prs", prs_main},		/* doc 2.0 */
	{"pull", pull_main},		/* doc 2.0 */
	{"push", push_main},		/* doc 2.0 */
	{"pwd", pwd_main},	/* regression test */ /* undoc? 2.0 */
	{"r2c", r2c_main},		/* doc 2.0 */
	{"range", range_main},		/* XXX - doc 2.0 it sucks*/
	{"rcs2sccs", rcs2sccs_main},	/* doc 2.0 */
	{"rcsparse", rcsparse_main},	/* doc 2.0 */
	{"receive", receive_main},	/* doc 2.0 */
	{"rechksum", rechksum_main},	/* doc 2.0 */
	{"renumber", renumber_main},	/* doc 2.0 */
	{"repo", repo_main},		/* obsolete */ /* undoc 2.0 */
	{"resolve", resolve_main},	/* doc 2.0 */
	{"rev2cset", r2c_main},		/* alias */	/* doc 2.0 as r2c */
	{"root", root_main},		/* doc 2.0 */
	{"rset", rset_main},		/* doc 2.0 */
	{"rm", rm_main},		/* doc 2.0 */
	{"rmdel", rmdel_main},		/* doc 2.0 */
	{"sane", sane_main},		/* doc 2.0 */
	{"sccscat", sccscat_main},	/* doc 2.0 as annotate */
	{"sccslog", sccslog_main},	/* doc 2.0 */
	{"sccsmv", mv_main},		/* alias */	/* doc 2.0 as mv */
	{"sccsrm", rm_main},		/* alias */	/* doc 2.0 as mv */
	{"send", send_main},		/* doc 2.0 */
	{"sendbug", sendbug_main},	/* doc 2.0 */
	{"setlod", setlod_main},	/* doc 2.0 as lod */
	{"setup", setup_main },		/* doc 2.0 */
	{"sfiles", sfind_main}, 	/* aliases */ /* doc 2.0 */
	{"sfind", sfind_main},		/* doc 2.0 as sfiles */
	{"sfio", sfio_main},		/* doc 2.0 */
	{"sinfo", sinfo_main},		/* alias */	/* doc 2.0 as info */
	{"status", status_main},	/* doc 2.0 */
	{"stripdel", stripdel_main},	/* doc 2.0 */
	{"takepatch", takepatch_main},	/* doc 2.0 */
	{"undo", undo_main},		/* doc 2.0 */
	{"undos", undos_main},		/* doc 2.0 */
	{"unedit", unedit_main},	/* doc 2.0 */
	{"unget", unedit_main},		/* aliases */	/* doc 2.0 as unedit */
	{"unlock", unlock_main },	/* doc 2.0 */
	{"unpark", unpark_main},	/* doc 2.0 */
	{"unwrap", unwrap_main},	/* doc 2.0 */
	{"users", users_main},		/* doc 2.0 */
	{"version", version_main},	/* doc 2.0 */
	{"what", what_main},		/* doc 2.0 */
	{"zone", zone_main},		/* doc 2.0 */

	{0, 0},
};

int
usage()
{
	system("bk help -s bk");
	exit(1);
}

#define	MAXARGS	1024

int
main(int ac, char **av)
{
	int	i, c, is_bk = 0, dashr = 0;
	int	flags, ret;
	char	*prog, *argv[MAXARGS];

	cmdlog_buffer[0] = 0;
	if (i = setjmp(exit_buf)) {
		i -= 1000;
		cmdlog_end(i, 0);
		return (i >= 0 ? i : 1);
	}
	atexit(cmdlog_exit);
	platformInit(av); 
	assert(bin);
	if (av[1] && streq(av[1], "bin") && !av[2]) {
		printf("%s\n", bin ? bin : "no path found");
		exit(0);
	}
	if (av[1] && streq(av[1], "path") && !av[2]) {
		printf("%s\n", getenv("PATH"));
		exit(0);
	}
#ifdef WIN32
	if (av[1] && streq(av[1], "_realpath") && !av[2]) {
		char buf[MAXPATH], real[MAXPATH];

		nt_getcwd(buf, sizeof(buf));
		getRealName(buf, NULL, real);
		printf("%s => %s\n", buf, real);
		exit(0);
	}
#endif
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
	 */
	prog = basenm(av[0]);
	if (streq(prog, "bk")) {
		is_bk = 1;
		while ((c = getopt(ac, av, "hrR")) != -1) {
			switch (c) {
			    case 'h':
				return (help_main(1, argv));
			    case 'r':
				if (av[optind] && isdir(av[optind])) {
					unless (chdir(av[optind]) == 0) {
						perror(av[optind]);
						return (1);
					}
					proj_free(bk_proj);
					bk_proj = proj_init(0);
					optind++;
				} else unless (proj_cd2root(bk_proj)) {
					fprintf(stderr, 
					    "bk: Cannot find package root.\n");
					return(1);
				}
				dashr++;
				break;
			    case 'R':
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
		unless (prog = av[optind]) usage();
		av = &av[optind];
		for (ac = 0; av[ac]; ac++);
		if (dashr) {
			unless (streq(prog, "sfiles") || streq(prog, "sfind")) {
				getoptReset();
#ifndef WIN32
				signal(SIGPIPE, SIG_IGN);
#endif
				return (bk_sfiles(ac, av));
			}
		}
		prog = av[0];
	}
	getoptReset();

	if (streq(prog, "cmdlog")) {
		cmdlog_dump(ac, av);
		return (0);
	}

	flags = cmdlog_start(av, 0);
	ret = run_cmd(prog, is_bk, ac, av);
	cmdlog_end(ret, flags);
	exit(ret);
}

private int
spawn_cmd(int flag, char **av)
{
	int ret;

	ret = spawnvp_ex(flag, av[0], av); 
#ifndef WIN32
	/*
	 * This test always failed with bash in cygwin1.1.6, why ?
	 */
	unless (WIFEXITED(ret)) {
		fprintf(stderr, "bk: cannot spawn %s\n", av[0]);
		return (127);
	}
#endif
	return (WEXITSTATUS(ret));
}


private int
run_cmd(char *prog, int is_bk, int ac, char **av)
{
	int	i, j, ret;
	char	cmd_path[MAXPATH];
	char	*argv[MAXARGS];
	static	int trace = -1;

	if (trace == -1) trace = getenv("BK_TRACE") != 0;
	if (trace) fprintf(stderr, "RUN bk %s\n", prog);

	/*
	 * look up the internal command 
	 */
	for (i = 0; cmdtbl[i].name; i++) {
		if (streq(cmdtbl[i].name, prog)){
			ret = cmdtbl[i].func(ac, av);
			return (ret);
		}
	}
	unless (is_bk) {
		fprintf(stderr, "%s is not a linkable command\n",  prog);
		return (1);
	}

	/*
	 * Is it a perl 4 script ?
	 */
	if (streq(prog, "pmerge")) {
		argv[0] = "perl"; 
		sprintf(cmd_path, "%s/%s", bin, prog);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) {
			if (i >= (MAXARGS-10)) {
				fprintf(stderr, "bk: too many args\n");
				exit(1);
			}
			argv[i] = av[j];
		}
		argv[i] = 0;
		return (spawn_cmd(_P_WAIT, argv));
	}

	/*
	 * Handle Gui script
	 */
	if (streq(prog, "fm") ||
	    streq(prog, "fm3") ||
	    streq(prog, "citool") ||
	    streq(prog, "_citool") ||
	    streq(prog, "sccstool") ||
	    streq(prog, "histtool") ||
	    streq(prog, "revtool") ||
	    streq(prog, "histool") ||
	    streq(prog, "setuptool") ||
	    streq(prog, "fmtool") ||
	    streq(prog, "fm3tool") ||
	    streq(prog, "difftool") ||
	    streq(prog, "helptool") ||
	    streq(prog, "csettool") ||
	    streq(prog, "renametool")) {
		signal(SIGINT, SIG_IGN);
#ifndef WIN32
		signal(SIGQUIT, SIG_IGN);
#endif
		signal(SIGTERM, SIG_IGN);
		argv[0] = find_wish();
		if (streq(prog, "sccstool")) prog = "revtool";
		if (streq(prog, "histool")) prog = "revtool";
		if (streq(prog, "histtool")) prog = "revtool";
		if (streq(prog, "revtool")) prog = "revtool";
		sprintf(cmd_path, "%s/%s", bin, prog);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) {
			if (i >= (MAXARGS-10)) {
				fprintf(stderr, "bk: too many args\n");
				exit(1);
			}
			argv[i] = av[j];
		}
		argv[i] = 0;
		return (spawn_cmd(_P_WAIT, argv));
	}

	/*
	 * Handle shell script
	 */
	if (streq(prog, "resync") || streq(prog, "import")) {
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
	}

	/*
	 * Is it a known C program ?
	 */
	if (streq(prog, "patch") ||
	    streq(prog, "diff3")) {
		return (spawn_cmd(_P_WAIT, av));
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
}

#define	LOG_MAXSIZE	(1<<20)
#define	LOG_BADEXIT	-100000		/* some non-valid exit */

void
cmdlog_exit(void)
{
	purify_list();
	if (cmdlog_buffer[0]) cmdlog_end(LOG_BADEXIT, 0);
}

private	struct {
	char	*name;
	int	flags;
} repolog[] = {
	{"pull", CMD_BYTES},
	{"push", CMD_BYTES},
	{"commit", CMD_WRLOCK|CMD_WRUNLOCK},
	{"remote pull", CMD_BYTES|CMD_FAST_EXIT|CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote push", CMD_BYTES|CMD_FAST_EXIT|CMD_WRLOCK|CMD_WRUNLOCK},
	{"remote pull part1", CMD_BYTES|CMD_RDLOCK},
	{"remote pull part2", CMD_BYTES|CMD_FAST_EXIT|CMD_RDUNLOCK},
	{"remote push part1", CMD_BYTES|CMD_WRLOCK},
	{"remote push part2", CMD_BYTES|CMD_FAST_EXIT|CMD_WRUNLOCK},
	{"remote clone", CMD_BYTES|CMD_FAST_EXIT|CMD_RDLOCK|CMD_RDUNLOCK},
	{ 0, 0 },
};

int
cmdlog_start(char **av, int httpMode)
{
	int	i, len, cflags = 0;

	cmdlog_buffer[0] = 0;
	cmdlog_repo = 0;

	for (i = 0; repolog[i].name; i++) {
		if (streq(repolog[i].name, av[0])) {
			cflags = repolog[i].flags;
			cmdlog_repo = 1;
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
		if (cflags & CMD_WRLOCK) cflags |= CMD_WRUNLOCK;
		if (cflags & CMD_WRUNLOCK) cflags |= CMD_WRLOCK;
		if (cflags & CMD_RDLOCK) cflags |= CMD_RDUNLOCK;
		if (cflags & CMD_RDUNLOCK) cflags |= CMD_RDLOCK;
	}            

	unless (bk_proj && bk_proj->root) return (cflags);

	if (cmdlog_repo) {
		sprintf(cmdlog_buffer,
		    "%s:%s", sccs_gethost(), fullname(bk_proj->root, 0));
	}
	for (len = 1, i = 0; av[i]; i++) {
		len += strlen(av[i]) + 1;
		if (len >= sizeof(cmdlog_buffer)) continue;
		if (i || cmdlog_repo) {
			strcat(cmdlog_buffer, " ");
			strcat(cmdlog_buffer, av[i]);
		} else {
			strcpy(cmdlog_buffer, av[i]);
		}
	}

	if (cflags & CMD_WRLOCK) {
		if (i = repository_wrlock()) {
			if (i == -1) {
				out(LOCK_WR_BUSY);
			} else if (i == -2) {
				out(LOCK_PERM);
			} else {
				out(LOCK_UNKNOWN);
			}
			out("\n");
			exit(1);
		}
	}
	if (cflags & CMD_RDLOCK) {
		if (i = repository_rdlock()) {
			if (i == -1) {
				out(LOCK_RD_BUSY);
			} else if (i == -2) {
				out(LOCK_PERM);
			} else {
				out(LOCK_UNKNOWN);
			}
			out("\n");
			exit(1);
		}
	}
	if (cflags & CMD_BYTES) save_byte_count(0); /* init to zero */
	return (cflags);

}

int
cmdlog_end(int ret, int flags)
{
	FILE	*f;
	char	*user, *file;
	char	path[MAXPATH];

	extern char bk_vers[];

	purify_list();
	unless (cmdlog_buffer[0] && bk_proj && bk_proj->root) return (flags);


	if (cmdlog_repo) {
		file = "repo_log";
	} else {
		file = "cmd_log";
	}
	sprintf(path, "%s/BitKeeper/log/%s", bk_proj->root, file);
	unless (f = fopen(path, "a")) {
		sprintf(path, "%s/%s", bk_proj->root, BKROOT);
		unless (exists(path)) return(flags);
		sprintf(path, "%s/BitKeeper/log/%s", bk_proj->root, file);
		mkdirf(path);
		unless (f = fopen(path, "a")) {
			fprintf(stderr, "Cannot open %s\n", path);
			return (flags);
		}
	}

	user = sccs_getuser();
	fprintf(f, "%s %lu %s: ",
	    user ? user : "Phantom User", time(0), bk_vers);
	if (ret == LOG_BADEXIT) {
		fprintf(f, "%s = ?\n", cmdlog_buffer);
	} else {
		fprintf(f, "%s = %d", cmdlog_buffer, ret);
		if (flags&CMD_BYTES) {
			fprintf(f, " xfered=%u", (u32)get_byte_count());
		}
		fputs("\n", f);
	}
	if (!cmdlog_repo && (fsize(fileno(f)) > LOG_MAXSIZE)) {
		char	old[MAXPATH];

		sprintf(old, "%s-older", path);
		fclose(f);
		rename(path, old);
	} else {
		fclose(f);
		chmod(path, 0666);
	}

	/*
	 * If error and repo command, force unlock, force exit
	 */
	if ((flags & CMD_WRLOCK) && ret) {
		flags |= CMD_WRUNLOCK;
		flags |= CMD_FAST_EXIT;
	}
	if ((flags & CMD_RDLOCK) && ret) {
		flags |= CMD_RDUNLOCK;
		flags |= CMD_FAST_EXIT;
	}
	if (flags & CMD_WRUNLOCK) repository_wrunlock(0);
	if (flags & CMD_RDUNLOCK) repository_rdunlock(0);

	cmdlog_buffer[0] = 0;
	cmdlog_repo = 0;
	return (flags);
}

private	void
cmdlog_dump(int ac, char **av)
{
	FILE	*f;
	time_t	t;
	char	*p;
	char	*version;
	char	buf[4096];

	
	if (ac == 2 && streq("--help", av[1])) { 
		system("bk help cmdlog");
		return;
	}

	unless (bk_proj && bk_proj->root) return;
	if (av[1] && streq(av[1], "-a")) {
		sprintf(buf,
	    "sort -n +1 %s/BitKeeper/log/repo_log %s/BitKeeper/log/cmd_log", 
		    bk_proj->root, bk_proj->root);
		f = popen(buf, "r");
	} else {
		sprintf(buf, "%s/BitKeeper/log/repo_log", bk_proj->root);
		f = fopen(buf, "r");
	}
	unless (f) return;
	while (fgets(buf, sizeof(buf), f)) {
		for (p = buf; (*p != ' ') && (*p != '@'); p++);
		*p++ = 0;
		t = strtoul(p, &version, 0);
		while (isspace(*version)) ++version;
		if (*version == ':') {
			p = version;
			*p = 0;
			version = 0;
		} else {
			unless (p = strchr(p, ':')) continue;
			*p = 0;
		}
		printf("%s %.19s %14s %s", buf, ctime(&t),
		    version ? version : "", 1+p);
	}
	if (av[1] && streq(av[1], "-a")) {
		pclose(f);
	} else {
		fclose(f);
	}
}

int
bk_sfiles(int ac, char **av)
{
	pid_t	pid;
	int	i;
	int	j, pfd;
	int	status;
	char	*sav[2] = {"sfind", 0};
	char	*cmds[100] = {"bk"};

	assert(ac < 95);
	for (i = 1, j = 0; cmds[i] = av[j]; i++, j++);
	cmds[i++] = "-";
	cmds[i] = 0;
	if ((pid = spawnvp_wPipe(cmds, &pfd, 0)) == -1) {
		fprintf(stderr, "cannot spawn %s %s\n", cmds[0], cmds[1]);
		return(1);
	} 
	cmdlog_start(sav, 0);
	close(1); dup(pfd); close(pfd);
	if (status = sfind_main(1, sav)) {
		kill(pid, SIGTERM);
		waitpid(pid, 0, 0);
		cmdlog_end(status, 0);
		exit(status);
	}
	fflush(stdout);
	close(1);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		cmdlog_end(WEXITSTATUS(status), 0);
		exit(WEXITSTATUS(status));
	}
#ifndef WIN32
	if (WIFSIGNALED(status)) {
		fprintf(stderr,
		    "Child was signaled with %d\n",
		    WTERMSIG(status));
		cmdlog_end(WTERMSIG(status), 0);
		exit(WTERMSIG(status));
	}
#endif
	cmdlog_end(100, 0);
	exit(100);
}

char *
find_prog(char *prog)
{
	char *p, *s;
	char path[MAXLINE];
	static char prog_path[MAXPATH];
	int more = 1;

	p  = getenv("PATH");
	if (p) {;
		sprintf(path, "%s%c/usr/local/bin", p, PATH_DELIM);
		localName2bkName(path, path);
	} else {
		strcpy(path, "/usr/local/bin");
	}
	p = path;
	while (more) {
		for (s = p; (*s != PATH_DELIM) && (*s != '\0');  s++);
		if (*s == '\0') more = 0;
		*s = '\0';
#ifdef WIN32
		sprintf(prog_path, "%s/%s.exe", p, prog);
#else
		sprintf(prog_path, "%s/%s", p, prog);
#endif
		if (exists(prog_path)) return (prog_path);
		p = ++s;
	}
	return (0);
}

char *
find_wish()
{
	char *p, *s;
	char path[MAXLINE];
	static char wish_path[MAXPATH];
	int more = 1;

	p  = getenv("PATH");
	if (p) {;
		sprintf(path, "%s%c/usr/local/bin", p, PATH_DELIM);
		localName2bkName(path, path);
	} else {
		strcpy(path, "/usr/local/bin");
	}
	p = path;
	while (more) {
		for (s = p; (*s != PATH_DELIM) && (*s != '\0');  s++);
		if (*s == '\0') more = 0;
		*s = '\0';
#ifdef WIN32
		sprintf(wish_path, "%s/wish83.exe", p);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish82.exe", p);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish81.exe", p);
		if (exists(wish_path)) return (wish_path);
#else
		sprintf(wish_path, "%s/wish8.2", p);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish8.0", p);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish", p);
		if (exists(wish_path)) return (wish_path);
#endif
		p = ++s;
	}
	fprintf(stderr, "Cannot find wish to run\n");
	exit(1);
}
