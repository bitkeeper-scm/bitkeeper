#include "system.h"
#include "sccs.h"
#include "range.h"
#include "bkd.h"

#define	BK "bk"

extern	int	test_release;
extern	unsigned build_timet;

char	*editor = 0, *pager = 0, *bin = 0;
char	*BitKeeper = "BitKeeper/";	/* XXX - reset this? */
char	**bk_environ;
project	*bk_proj = 0;
jmp_buf	exit_buf;
char	cmdlog_buffer[MAXPATH*4];
int	cmdlog_flags;
char 	*upgrade_msg =
"This feature is not available in this version of BitKeeper, to upgrade\n\
please contact sales@bitmover.com\n"; 

private char	*log_versions = "!@#$%^&*()-_=+[]{}|\\<>?/";	/* 25 of 'em */
#define	LOGVER	0


char	*find_wish(void);
char	*find_perl5(void);
private	void	cmdlog_exit(void);
private	int	cmdlog_repo;
private	void	cmdlog_dump(int, char **);
private int	run_cmd(char *prog, int is_bk, char *sopts, int ac, char **av);
private int	usage(void);

extern	void	platformInit(char **av);
extern	int	proj_cd2root(project *p);

/* KEEP THIS SORTED! */
int	_g2sccs_main(int, char **);
int	abort_main(int, char **);
int	adler32_main(int, char **);
int	admin_main(int, char **);
int	annotate_main(int, char **);
int	applyall_main(int, char **);
int	approve_main(int, char **);
int	base64_main(int, char **);
int	bkd_main(int, char **);
int	cat_main(int, char **);
int	changes_main(int, char **);
int	check_main(int, char **);
int	checksum_main(int, char **);
int	clean_main(int, char **);
int	cleanpath_main(int, char **);
int	clone_main(int, char **);
int	comments_main(int, char **);
int	commit_main(int, char **);
int	config_main(int, char **);
int	converge_main(int, char **);
int	cp_main(int, char **);
int	create_main(int, char **);
int	crypto_main(int, char **);
int	cset_main(int, char **);
int	csetprune_main(int, char **);
int	deledit_main(int, char **);
int	delget_main(int, char **);
int	delta_main(int, char **);
int	diffs_main(int, char **);
int	diffsplit_main(int, char **);
int	exists_main(int, char **);
int	export_main(int, char **);
int	f2csets_main(int, char **);
int	fdiff_main(int, char **);
int	find_main(int, char **);
int	findkey_main(int, char **);
int	fix_main(int, char **);
int	fixlod_main(int, char **);
int	gca_main(int, char **);
int	get_main(int, char **);
int	gethelp_main(int, char **);
int	gethost_main(int, char **);
int	getmsg_main(int, char **);
int	getuser_main(int, char **);
int	gnupatch_main(int, char **);
int	gone_main(int, char **);
int	graft_main(int, char **);
int	grep_main(int, char **);
int	gzip_main(int, char **);
int	help_main(int, char **);
int	helpsearch_main(int, char **);
int	helptopics_main(int, char **);
int	hostme_main(int, char **);
int	idcache_main(int, char **);
int	isascii_main(int, char **);
int	key2rev_main(int, char **);
int	key2path_main(int, char **);
int	keysort_main(int, char **);
int	keyunlink_main(int, char **);
int	lconfig_main(int, char **);
int	level_main(int, char **);
int	license_main(int, char **);
int	lines_main(int, char **);
int	link_main(int, char **);
int	listkey_main(int, char **);
int	lock_main(int, char **);
int	lod_main(int, char **);
int	log_main(int, char **);
int	logflags_main(int, char **);
int	logging_main(int, char **);
int	loggingaccepted_main(int ac, char **av);
int	loggingask_main(int ac, char **av);
int	loggingto_main(int, char **);
int	mailsplit_main(int, char **);
int	mail_main(int, char **);
int	makepatch_main(int, char **);
int	merge_main(int, char **);
int	mklock_main(int, char **);
int	mtime_main(int, char **);
int	multiuser_main(int, char **);
int	mv_main(int, char **);
int	mvdir_main(int, char **);
int	mydiff_main(int, char **);
int	names_main(int, char **);
int	newroot_main(int, char **);
int	opark_main(int, char **);
int	ounpark_main(int, char **);
int	parent_main(int, char **);
int	park_main(int, char **);
int	pending_main(int, char **);
int	preference_main(int, char **);
int	probekey_main(int, char **);
int	prompt_main(int, char **);
int	prs_main(int, char **);
int	prunekey_main(int, char **);
int	pull_main(int, char **);
int	push_main(int, char **);
int	pwd_main(int, char **);
int	r2c_main(int, char **);
int	range_main(int, char **);
int	rcheck_main(int, char **);
int	rclone_main(int, char **);
int	rcs2sccs_main(int, char **);
int	rcsparse_main(int, char **);
int	receive_main(int, char **);
int	renumber_main(int, char **);
int	relink_main(int, char **);
int	repo_main(int, char **);
int	resolve_main(int, char **);
int	rm_main(int, char **);
int	rmdel_main(int, char **);
int	root_main(int, char **);
int	rset_main(int, char **);
int	sane_main(int, char **);
int	sccs2bk_main(int, char **);
int	sccscat_main(int, char **);
int	sccslog_main(int, char **);
int	scompress_main(int, char **);
int	send_main(int, char **);
int	sendbug_main(int, char **);
int	set_main(int, char **);
int	setup_main(int, char **);
int	sfiles_main(int, char **);
int	sfind_main(int, char **);
int	sfio_main(int, char **);
int	shrink_main(int, char **);
int	sinfo_main(int, char **);
int	smerge_main(int, char **);
int	shellSplit_test_main(int ac, char **av);
int	socket2pipe_main(int, char **);
int	sort_main(int, char **);
int	sortmerge_main(int, char **);
int	status_main(int, char **);
int	stripdel_main(int, char **);
int	strings_main(int, char **);
int	synckeys_main(int, char **);
int	tagmerge_main(int, char **);
int	takepatch_main(int, char **);
int	testdates_main(int, char **);
int	timestamp_main(int, char **);
int	unbk_main(int, char **);
int	undo_main(int, char **);
int	undos_main(int, char **);
int	unedit_main(int, char **);
int	unlink_main(int, char **);
int	unlock_main(int, char **);
int	unpark_main(int, char **);
int	unpull_main(int, char **);
int	unwrap_main(int, char **);
int	users_main(int, char **);
int	uuencode_main(int, char **);
int	uudecode_main(int, char **);
int	val_main(int, char **);
int	version_main(int, char **);
int	what_main(int, char **);
int	xflags_main(int, char **);
int	zone_main(int, char **);

struct	command cmdtbl[] = {
	{"_adler32", adler32_main},
	{"_applyall", applyall_main},
	{"_converge", converge_main},
	{"_cleanpath", cleanpath_main},
	{"_exists", exists_main},
	{"_find", find_main },
	{"_g2sccs", _g2sccs_main},
	{"_get", get_main},
	{"_gzip", gzip_main }, 
	{"_keysort", keysort_main},
	{"_key2path", key2path_main},
	{"_keyunlink", keyunlink_main },
	{"_lconfig", lconfig_main},	
	{"_lines", lines_main},	
	{"_link", link_main},	
	{"_listkey", listkey_main},	
	{"_log", log_main},
	{"_logflags", logflags_main},
	{"_logging", logging_main},
	{"_loggingaccepted", loggingaccepted_main},
	{"_loggingask", loggingask_main},
	{"_loggingto", loggingto_main},
	{"_mail", mail_main},
	{"_preference", preference_main},
	{"_probekey", probekey_main},
	{"_prunekey", prunekey_main},
	{"_rclone", rclone_main},
	{"_scompress", scompress_main},		/* undoc? 2.0 */
	{"_socket2pipe", socket2pipe_main},	/* for win32 only */
	{"_sort", sort_main},
	{"_sortmerge", sortmerge_main},
	{"_shellSplit_test", shellSplit_test_main},
	{"_strings", strings_main},
	{"_timestamp", timestamp_main},
	{"_unlink", unlink_main },
	{"abort", abort_main},			/* doc 2.0 */	
	{"add", delta_main},			/* doc 2.0 */
	{"admin", admin_main},			/* doc 2.0 */
	{"approve", approve_main},		/* doc 2.0 */
	{"annotate", annotate_main},		/* doc 2.0 */
	{"base64", base64_main},		/* need doc 2.2 */
	{"bkd", bkd_main },			/* doc 2.0 */
	{"cat", cat_main},			/* doc 2.0 */
	{"changes", changes_main},		/* doc 2.0 */
	{"check", check_main},			/* doc 2.0 */
	{"checksum", checksum_main},		/* doc 2.0 */
	{"ci", delta_main},			/* doc 2.0 */
	{"clean", clean_main},			/* doc 2.0 */
	{"clone", clone_main},			/* doc 2.0 */
	{"co", get_main},			/* doc 2.0 */
	{"comment", comments_main}, /* alias for Linus, remove... */
	{"comments", comments_main},
	{"commit", commit_main},		/* doc 2.0 */
	{"config", config_main},		/* doc 2.0 */
	{"cp", cp_main},
	{"create", create_main},		/* doc 2.0 */
	{"crypto", crypto_main},		/* needs doc 2.2 */
	{"cset", cset_main},			/* doc 2.0 */
	{"csetprune", csetprune_main},
	{"f2csets", f2csets_main},		/* undoc? 2.0 */
	{"delta", delta_main},			/* doc 2.0 */
	{"deledit", deledit_main},		/* doc 2.0 */
	{"delget", delget_main},		/* doc 2.0 */
	{"diffs", diffs_main},			/* doc 2.0 */
	{"diffsplit", diffsplit_main},
	{"edit", get_main},	/* aliases */	/* doc 2.0 */
	{"enter", delta_main},			/* doc 2.0 */
	{"export", export_main},		/* doc 2.0 */
	{"fdiff", fdiff_main},			/* undoc? 2.0 */
	{"findkey", findkey_main},		/* doc 2.0 */
	{"fix", fix_main},			/* doc 2.0 */
	{"_fix_lod1", fixlod_main},		/* undoc 2.0 */
	{"gca", gca_main},			/* doc 2.0 */
	{"get", get_main},			/* doc 2.0 */
	{"gethelp", gethelp_main},		/* undoc? 2.0 */
	{"gethost", gethost_main},		/* doc 2.0 */
	{"getmsg", getmsg_main},		/* undoc? 2.0 */
	{"getuser", getuser_main},		/* doc 2.0 */
	{"graft", graft_main},			/* undoc? 2.0 */
	{"grep", grep_main},			/* doc 2.0 */
	{"gnupatch", gnupatch_main},		/* doc 2.0 */
	{"gone", gone_main},			/* doc 2.0 */
	{"help", help_main},			/* doc 2.0 */
	{"helpsearch", helpsearch_main},	/* undoc 2.0 */
	{"helptopics", helptopics_main},	/* undoc 2.0 */
	{"hostme", hostme_main},		/* undoc 2.0 */
	{"info", sinfo_main},			/* doc 2.0 */
	{"idcache", idcache_main},		/* undoc? 2.0 */
	{"isascii", isascii_main},		/* doc 2.0 */
	{"key2rev", key2rev_main},		/* doc 2.0 */
	{"level", level_main},			/* doc 2.0 */
	{"license", license_main},		/* undoc */
	{"lock", lock_main},			/* doc 2.0 */
	{"lod", lod_main},	/* XXX - doc 2.0 - says doesn't work yet */
	{"log", log_main},
	{"mailsplit", mailsplit_main},
	{"makepatch", makepatch_main},		/* doc 2.0 */
	{"merge", merge_main},			/* doc 2.0 */
	{"mklock", mklock_main},	/* regression test */ /* undoc 2.0 */
	{"mtime", mtime_main},		/* regression test */ /* undoc 2.0 */
	{"mv", mv_main},			/* doc 2.0 */
	{"mvdir", mvdir_main},			/* doc 2.0 */
	{"multiuser", multiuser_main},		/* doc 2.0 */
	{"mydiff", mydiff_main},
	{"names", names_main},			/* doc 2.0 */
	{"newroot", newroot_main},		/* doc 2.0 */
	{"new", delta_main},	/* aliases */	/* doc 2.0 */
	{"opark", opark_main},			/* doc 2.0 */
	{"ounpark", ounpark_main},			/* doc 2.0 */
	{"parent", parent_main},		/* doc 2.0 */
	{"park", park_main},			/* doc 2.0 */
	{"pending", pending_main},		/* doc 2.0 */
	{"prompt", prompt_main},
	{"prs", prs_main},			/* doc 2.0 */
	{"pull", pull_main},			/* doc 2.0 */
	{"push", push_main},			/* doc 2.0 */
	{"pwd", pwd_main},/* regression test */ /* undoc? 2.0 */
	{"r2c", r2c_main},			/* doc 2.0 */
	{"range", range_main},		/* XXX - doc 2.0 it sucks*/
	{"rcheck", rcheck_main},		/* doc 2.0 */
	{"rcs2sccs", rcs2sccs_main},		/* doc 2.0 */
	{"rcsparse", rcsparse_main},		/* doc 2.0 */
	{"receive", receive_main},		/* doc 2.0 */
	{"rechksum", checksum_main},		/* obsolete - alias */
	{"relink", relink_main},
	{"renumber", renumber_main},		/* doc 2.0 */
	{"repo", repo_main},	/* obsolete */ 	/* undoc 2.0 */
	{"resolve", resolve_main},		/* doc 2.0 */
	{"rev2cset", r2c_main},	/* alias */	/* doc 2.0 as r2c */
	{"root", root_main},			/* doc 2.0 */
	{"rset", rset_main},			/* doc 2.0 */
	{"rm", rm_main},			/* doc 2.0 */
	{"rmdel", rmdel_main},			/* doc 2.0 */
	{"sane", sane_main},			/* doc 2.0 */
	{"sccs2bk", sccs2bk_main},		/* undoc? 2.0 */
	{"sccscat", sccscat_main},		/* doc 2.0 as annotate */
	{"sccsdiff", diffs_main},		/* doc 2.0 */
	{"sccslog", sccslog_main},		/* doc 2.0 */
	{"sccsmv", mv_main},	/* alias */	/* doc 2.0 as mv */
	{"sccsrm", rm_main},	/* alias */	/* doc 2.0 as mv */
	{"send", send_main},			/* doc 2.0 */
	{"sendbug", sendbug_main},		/* doc 2.0 */
	{"set", set_main},
	{"setup", setup_main },			/* doc 2.0 */
	{"shrink", shrink_main}, 		/* undoc? 2.0 */
	{"sfiles", sfind_main}, /* aliases */ 	/* doc 2.0 */
	{"sfind", sfind_main},			/* doc 2.0 as sfiles */
	{"sfio", sfio_main},			/* doc 2.0 */
	{"sinfo", sinfo_main},	/* alias */	/* doc 2.0 as info */
	{"smerge", smerge_main},		/* doc needed 2.0 */
	{"status", status_main},		/* doc 2.0 */
	{"stripdel", stripdel_main},		/* doc 2.0 */
	{"synckeys", synckeys_main},
	{"tagmerge", tagmerge_main},		/* */
	{"takepatch", takepatch_main},		/* doc 2.0 */
	{"testdates", testdates_main},		/* undoc 2.0 */
	{"unbk", unbk_main},			/* undoc? 2.0 */
	{"undo", undo_main},			/* doc 2.0 */
	{"undos", undos_main},			/* doc 2.0 */
	{"unedit", unedit_main},		/* doc 2.0 */
	{"unget", unedit_main},	/* aliases */	/* doc 2.0 as unedit */
	{"unlock", unlock_main },		/* doc 2.0 */
	{"unpark", unpark_main},		/* doc 2.0 */
	{"unpull", unpull_main},		/* doc 2.0 */
	{"unwrap", unwrap_main},		/* doc 2.0 */
	{"users", users_main},			/* doc 2.0 */
	{"user", users_main},			/* aliases of "bk users" */
	{"uudecode", uudecode_main},
	{"uuencode", uuencode_main},
	{"val", val_main},			/* doc 2.0 */
	{"version", version_main},		/* doc 2.0 */
	{"what", what_main},			/* doc 2.0 */
	{"xflags", xflags_main},		/* doc 2.0 */
	{"zone", zone_main},			/* doc 2.0 */

	{0, 0},
};

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

	if (getenv("BK_SHOWPROC")) {
		FILE	*f = fopen(DEV_TTY, "w");

		fprintf(f, "BK (%u t: %5s)", getpid(), milli());
		for (i = 0; av[i]; ++i) fprintf(f, " %s", av[i]);
		fprintf(f, "\n");
		fclose(f);
	}

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
		av = &av[optind];
		for (ac = 0; av[ac]; ac++);
		if (dashr) {
			unless (streq(prog, "sfiles") || streq(prog, "sfind")) {
				getoptReset();
				signal(SIGPIPE, SIG_IGN); /* no-op on win32 */
				return (bk_sfiles(si > 1 ? sopts : 0, ac, av));
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
	ret = run_cmd(prog, is_bk, si > 1 ? sopts : 0, ac, av);
	cmdlog_end(ret);
	exit(ret);
}

private int
run_cmd(char *prog, int is_bk, char *sopts, int ac, char **av)
{
	int	i, j, ret;
	char	cmd_path[MAXPATH];
	char	*argv[MAXARGS];

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
	    streq(prog, "fm2tool") ||
	    streq(prog, "fm3tool") ||
	    streq(prog, "difftool") ||
	    streq(prog, "helptool") ||
	    streq(prog, "csettool") ||
	    streq(prog, "renametool") ||
	    streq(prog, "msgtool")) {
		sig_catch(SIG_IGN);
		argv[0] = find_wish();
		if (streq(prog, "fm2tool")) prog = "fmtool";
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
	{"pull", CMD_BYTES|CMD_WRLOCK|CMD_WRUNLOCK},
	{"push", CMD_BYTES|CMD_RDLOCK|CMD_RDUNLOCK},
	{"commit", CMD_WRLOCK|CMD_WRUNLOCK},
	{"remote pull", CMD_BYTES|CMD_FAST_EXIT|CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote push", CMD_BYTES|CMD_FAST_EXIT|CMD_WRLOCK|CMD_WRUNLOCK},
	{"remote pull part1", CMD_BYTES|CMD_RDLOCK},
	{"remote pull part2", CMD_BYTES|CMD_FAST_EXIT|CMD_RDUNLOCK},
	{"remote push part1", CMD_BYTES|CMD_WRLOCK},
	{"remote push part2", CMD_BYTES|CMD_FAST_EXIT|CMD_WRUNLOCK},
	{"remote clone", CMD_BYTES|CMD_FAST_EXIT|CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote rclone part1", CMD_BYTES},
	{"remote rclone part2", CMD_BYTES|CMD_FAST_EXIT},
	{"synckeys", CMD_RDLOCK|CMD_RDUNLOCK},
	{"pending_part1", CMD_RDLOCK|CMD_RDUNLOCK},
	{"pending_part2", CMD_RDLOCK|CMD_RDUNLOCK},
	/*
	 * This is a hack because we short circuit part2 in changes.c.
	 * It opens a tiny race.
	 */
	{"remote changes part1", CMD_RDLOCK|CMD_RDUNLOCK},
	{"remote changes part2", CMD_RDLOCK|CMD_RDUNLOCK},
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
	char	*user;
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
	user = sccs_getuser();
	fprintf(f, "%c%s %lu %s: ",
	    log_versions[LOGVER],
	    user ? user : "Phantom.User", time(0), bk_vers);
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
		FILE	*f = fopen(DEV_TTY, "w");

		fprintf(f, "END(%u t: %5s)", getpid(), milli());
		fprintf(f, " %s = %d\n", cmdlog_buffer, ret);
		fclose(f);
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
		return (flags);
	}
	if (cmdlog_repo &&
	    write_log(bk_proj->root, "repo_log", LOG_MAXSIZE, "%s", log)) {
		return (flags);
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
bk_sfiles(char *opts, int ac, char **av)
{
	pid_t	pid;
	int	i;
	int	j, pfd;
	int	status;
	int	sac = 1;
	char	*sav[3] = {"sfind", 0, 0};
	char	*cmds[100] = {"bk"};

	assert(ac < 95);
	for (i = 1, j = 0; cmds[i] = av[j]; i++, j++);
	cmds[i++] = "-";
	cmds[i] = 0;
	if ((pid = spawnvp_wPipe(cmds, &pfd, 0)) == -1) {
		fprintf(stderr, "cannot spawn %s %s\n", cmds[0], cmds[1]);
		return(1);
	} 
	if (opts) {
		sav[1] = opts;
		sac++;
	}
	cmdlog_start(sav, 0);
	close(1); dup(pfd); close(pfd);
	if (status = sfind_main(sac, sav)) {
		kill(pid, SIGTERM);
		waitpid(pid, 0, 0);
		cmdlog_end(status);
		exit(status);
	}
	fflush(stdout);
	close(1);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		cmdlog_end(WEXITSTATUS(status));
		exit(WEXITSTATUS(status));
	}
	if (WIFSIGNALED(status)) {
		fprintf(stderr,
		    "Child was signaled with %d\n",
		    WTERMSIG(status));
		cmdlog_end(WTERMSIG(status));
		exit(WTERMSIG(status));
	}
	cmdlog_end(100);
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
		sprintf(prog_path, "%s/%s%s", p, prog, EXE);
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

	p = getenv("BK_WISH");
	if (p) {
		strcpy(wish_path,p);
		unless (exists(wish_path)) {
			fprintf(stderr, "bk: bad value for BK_WISH (%s)\n",
				wish_path);
			exit(1);
		}
		return (wish_path);
	}

#ifdef	__APPLE__
	strcpy(wish_path,
	    "/Applications/Wish Shell.app/Contents/MacOS/Wish Shell");
	if (exists(wish_path)) return (wish_path);
#endif
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
		sprintf(wish_path, "%s/wish83%s", p, EXE);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish8.3%s", p, EXE);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish82%s", p, EXE);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish8.2%s", p, EXE);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish81%s", p, EXE);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish8.1%s", p, EXE);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish80%s", p, EXE);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish8.0%s", p, EXE);
		if (exists(wish_path)) return (wish_path);
		sprintf(wish_path, "%s/wish%s", p, EXE);
		if (exists(wish_path)) return (wish_path);
		p = ++s;
	}
	fprintf(stderr,
		"Cannot find the \"wish\" interpreter, this usually means\n"
		"the Tcl/Tk package is not installed on your system or it\n"
		"is not in your path\n");
	exit(1);
}
