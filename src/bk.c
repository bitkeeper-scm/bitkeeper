#include "system.h"
#include "sccs.h" 

#define BK "bk"

char	*editor = 0, *pager = 0, *bin = 0;
char	*BitKeeper = "BitKeeper/";	/* XXX - reset this? */
project	*bk_proj = 0;

private	char log_buffer[MAXPATH*4];

char	*find_wish();
char	*find_perl5();
extern	void	getoptReset();
private	void	log_start(char **av);
private	void	log_end(int ret);
private	void	log_dump();
private	void	log_exit(void);

int _createlod_main(int, char **);
int abort_main(int, char **);
int adler32_main(int, char **);
int admin_main(int, char **);
int bkd_main(int, char **);
int check_main(int, char **);
int chksum_main(int, char **);
int clean_main(int, char **);
int clone_main(int, char **);
int commit_main(int, char **);
int config_main(int, char **);
int createlod_main(int, char **);
int cset_main(int, char **);
int delta_main(int, char **);
int diffs_main(int, char **);
int export_main(int, char **);
int fdiff_main(int, char **);
int find_main(int, char **);
int fix_main(int, char **);
int g2sccs_main(int, char **);
int gca_main(int, char **);
int get_main(int, char **);
int gethelp_main(int, char **);
int gethost_main(int, char **);
int getuser_main(int, char **);
int graft_main(int, char **);
int help_main(int, char **);
int isascii_main(int, char **);
int key2rev_main(int, char **);
int keysort_main(int, char **);
int lines_main(int, char **);
int lock_main(int, char **);
int lod_main(int, char **);
int log_main(int, char **);
int logging_main(int, char **);
int loggingaccepted_main(int ac, char **av);
int loggingask_main(int ac, char **av);
int loggingto_main(int, char **);
int mklock_main(int, char **);
int mtime_main(int, char **);
int mv_main(int, char **);
int names_main(int, char **);
int parent_main(int, char **);
int pending_main(int, char **);
int prs_main(int, char **);
int pull_main(int, char **);
int push_main(int, char **);
int r2c_main(int, char **);
int range_main(int, char **);
int rcs2sccs_main(int, char **);
int rcsparse_main(int, char **);
int receive_main(int, char **);
int rechksum_main(int, char **);
int renumber_main(int, char **);
int repo_main(int, char **);
int resolve_main(int, char **);
int rm_main(int, char **);
int rmdel_main(int, char **);
int sccscat_main(int, char **);
int sccslog_main(int, char **);
int send_main(int, char **);
int sendbug_main(int, char **);
int setlod_main(int, char **);
int setup_main(int, char **);
int sfiles_main(int, char **);
int sfio_main(int, char **);
int sids_main(int, char **);
int sinfo_main(int, char **);
int smoosh_main(int, char **);
int status_main(int, char **);
int stripdel_main(int, char **);
int takepatch_main(int, char **);
int undo_main(int, char **);
int undos_main(int, char **);
int unedit_main(int, char **);
int unlock_main(int, char **);
int unwrap_main(int, char **);
int users_main(int, char **);
int version_main(int, char **);
int what_main(int, char **);
int zone_main(int, char **);

struct command cmdtbl[100] = {
	{"_createlod", _createlod_main},
	{"_find", find_main }, /* internal helper function */
	{"_logging", logging_main},
	{"_loggingaccepted", loggingaccepted_main},
	{"_loggingask", loggingask_main},
	{"_loggingto", loggingto_main},
	{"abort", abort_main},
	{"adler32", adler32_main},
	{"admin", admin_main},
	{"bkd", bkd_main },
	{"check", check_main},
	{"chksum", chksum_main},
	{"ci", delta_main},
	{"clean", clean_main},
	{"clone", clone_main},
	{"co", get_main},
	{"commit", commit_main},
	{"config", config_main},
	{"createlod", createlod_main},
	{"cset", cset_main},
	{"delta", delta_main},
	{"diffs", diffs_main},
	{"edit", get_main},	/* aliases */
	{"export", export_main},
	{"fdiff", fdiff_main},
	{"fix", fix_main},
	{"g2sccs", g2sccs_main},
	{"gca", gca_main},
	{"get", get_main},
	{"gethelp", gethelp_main},
	{"gethost", gethost_main},
	{"getuser", getuser_main},
	{"graft", graft_main},
	{"help", help_main},
	{"info", sinfo_main},	/* aliases */
	{"isascii", isascii_main},
	{"key2rev", key2rev_main},
	{"keysort", keysort_main},
	{"lines", lines_main},
	{"lock", lock_main},
	{"lod", lod_main},
	{"log", log_main},
	{"mklock", mklock_main}, /* for regression test only */
	{"mtime", mtime_main},
	{"mv", mv_main},
	{"names", names_main},
	{"new", delta_main},	/* aliases */
	{"parent", parent_main},
	{"pending", pending_main},
	{"prs", prs_main},
	{"pull", pull_main},
	{"push", push_main},
	{"r2c", r2c_main},
	{"range", range_main},
	{"rcs2sccs", rcs2sccs_main},
	{"rcsparse", rcsparse_main},
	{"receive", receive_main},
	{"rechksum", rechksum_main},
	{"renumber", renumber_main},
	{"repo", repo_main},
	{"resolve", resolve_main},
	{"rev2cset", r2c_main},
	{"rm", rm_main},
	{"rmdel", rmdel_main},
	{"sccscat", sccscat_main},
	{"sccslog", sccslog_main},
	{"sccsmv", mv_main},
	{"sccsrm", rm_main},
	{"send", send_main},
	{"sendbug", sendbug_main},
	{"setlod", setlod_main},
	{"setup", setup_main },
	{"sfiles", sfiles_main},
	{"sfio", sfio_main},
	{"sids", sids_main},
	{"sinfo", sinfo_main},
	{"smoosh", smoosh_main},
	{"status", status_main},
	{"stripdel", stripdel_main},
	{"takepatch", takepatch_main},
	{"undo", undo_main},
	{"undos", undos_main},
	{"unedit", unedit_main},
	{"unget", unedit_main},	/* aliases */
	{"unlock", unlock_main },
	{"unwrap", unwrap_main},
	{"users", users_main},
	{"version", version_main},
	{"what", what_main},
	{"zone", zone_main},

	{0, 0},
};

int
usage()
{
	fprintf(stderr, "usage bk [-r[dir]] | -R command [options] [args]\n");
	printf("Try bk help for help.\n");
	exit(1);
}

int
main(int ac, char **av)
{
	int	i, j;
	char	cmd_path[MAXPATH];
	char	*argv[100];
	int	c;
	int	is_bk = 0, dashr = 0;
	int	ret;
	char	*prog;

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
	argv[0] = "help";
	argv[1] = 0;

	if (!bk_proj || !bk_proj->root || !isdir(bk_proj->root)) {
		bk_proj = proj_init(0);
	}
	log_buffer[0] = 0;

	/*
	 * Parse our options if called as "bk".
	 */
	prog = basenm(av[0]);
	if (streq(prog, "bk")) {
		is_bk = 1;
		while ((c = getopt(ac, av, "rR")) != -1) {
			switch (c) {
			    case 'h':
				return (help_main(1, argv));
			    case 'r':
				if (av[optind] && isdir(av[optind])) {
					unless (chdir(av[optind]) == 0) {
						perror(av[optind]);
						return (1);
					}
					optind++;
				} else unless (proj_cd2root(bk_proj)) {
					fprintf(stderr, 
					    "bk: Can not find package root.\n");
					return(1);
				}
				dashr++;
				break;
			    case 'R':
				unless (proj_cd2root(bk_proj)) {
					fprintf(stderr, 
					    "bk: Can not find package root.\n");
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
			unless (streq(prog, "sfiles")) {
				getoptReset();
				return (bk_sfiles(ac, av));
			}
		}
		prog = av[0];
	}
	getoptReset();

	if (streq(prog, "cmdlog")) {
		log_dump();
		return (0);
	}

	/*
	 * look up the internal command 
	 */
	for (i = 0; cmdtbl[i].name; i++) {
		if (streq(cmdtbl[i].name, prog)){
			log_start(av);
			ret = cmdtbl[i].func(ac, av);
			log_end(ret);
			exit(ret);
		}
	}
	unless(is_bk) {
		fprintf(stderr, "%s is not a linkable command\n",  prog);
		exit(1);
	}

	/*
	 * Is it a perl 4 script ?
	 */
	if (streq(prog, "pmerge")) {
		argv[0] = "perl"; 
		sprintf(cmd_path, "%s/%s", bin, prog);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) argv[i] = av[j];
		argv[i] = 0;
		log_start(argv);
		ret = spawnvp_ex(_P_WAIT, argv[0], argv);
		log_end(ret);
		exit(ret);
	}

	/*
	 * Is it a perl 5 script ?
	 */
	if (streq(prog, "mkdiffs")) {
		argv[0] = find_perl5();
		sprintf(cmd_path, "%s/%s", bin, prog);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) argv[i] = av[j];
		argv[i] = 0;
		log_start(argv);
		ret = spawnvp_ex(_P_WAIT, argv[0], argv);
		log_end(ret);
		exit(ret);
	}

	/*
	 * Handle Gui script
	 */
	if (streq(prog, "fm") ||
	    streq(prog, "fm3") ||
	    streq(prog, "citool") ||
	    streq(prog, "sccstool") ||
	    streq(prog, "setuptool") ||
	    streq(prog, "fmtool") ||
	    streq(prog, "fm3tool") ||
	    streq(prog, "difftool") ||
	    streq(prog, "helptool") ||
	    streq(prog, "csettool") ||
	    streq(prog, "renametool")) {
		argv[0] = find_wish();
		sprintf(cmd_path, "%s/%s", bin, prog);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) argv[i] = av[j];
		argv[i] = 0;
		log_start(argv);
		ret = spawnvp_ex(_P_WAIT, argv[0], argv);
		log_end(ret);
		exit(ret);
	}

	/*
	 * Handle shell script
	 */
	if (streq(prog, "resync") || streq(prog, "import")) {
		argv[0] = shell();
		sprintf(cmd_path, "%s/%s", bin, prog);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) {
			argv[i] = av[j];
		}
		argv[i] = 0;
		log_start(argv);
		ret = spawnvp_ex(_P_WAIT, argv[0], argv);
		log_end(ret);
		exit(ret);
	}

	/*
	 * Is it a known C program ?
	 */
	if (streq(prog, "patch") || streq(prog, "diff3")) {
		log_start(av);
		ret = spawnvp_ex(_P_WAIT, av[0], av);
		log_end(ret);
		exit(ret);
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
	for (i = 2, j = 0; av[j]; i++, j++) argv[i] = av[j];
	argv[i] = 0;
	ret = spawnvp_ex(_P_WAIT, argv[0], argv);
	log_end(ret);
	exit(ret);
}

#define	LOG_MAXSIZE	(32<<10)
#define	LOG_BADEXIT	-100000		/* some non-valid exit */

void
log_exit(void)
{
	if (log_buffer[0]) log_end(LOG_BADEXIT);
}

private	void
log_start(char **av)
{
	int	i, len = 0;

	unless (bk_proj && bk_proj->root) return;
	for (i = 0; av[i]; i++) {
		len += strlen(av[i]);
		if (len >= sizeof(log_buffer)) continue;
		if (i) {
			strcat(log_buffer, " ");
			strcat(log_buffer, av[i]);
		} else {
			strcpy(log_buffer, av[i]);
		}
	}
}

private	void
log_end(int ret)
{
	FILE	*f;
	char	*user;
	char	path[MAXPATH];

	unless (log_buffer[0] && bk_proj && bk_proj->root) return;
	sprintf(path, "%s/BitKeeper/log/cmd_log", bk_proj->root);
	unless (f = fopen(path, "a")) {
		sprintf(path, "%s/%s", bk_proj->root, BKROOT);
		unless (exists(path)) return;
		sprintf(path, "%s/BitKeeper/log/cmd_log", bk_proj->root);
		mkdirf(path);
		unless (f = fopen(path, "a")) return;
	}
	user = sccs_getuser();
	fprintf(f, "%s@%u: ", user ? user : "Phantom User", time(0));
	if (ret == LOG_BADEXIT) {
		fprintf(f, "%s = ?\n", log_buffer);
	} else {
		fprintf(f, "%s = %d\n", log_buffer, ret);
	}
	if (fsize(fileno(f)) > LOG_MAXSIZE) {
		char	old[MAXPATH];

		sprintf(old, "%s-older", path);
		fclose(f);
		rename(path, old);
	} else {
		fclose(f);
	}
	log_buffer[0] = 0;
}

private	void
log_dump()
{
	FILE	*f;
	time_t	t;
	char	*p;
	char	buf[4096];

	unless (bk_proj && bk_proj->root) return;
	sprintf(buf, "%s/BitKeeper/log/cmd_log", bk_proj->root);
	unless (f = fopen(buf, "r")) return;
	while (fgets(buf, sizeof(buf), f)) {
		unless (p = strchr(buf, '@')) continue;
		*p++ = 0;
		t = strtoul(p, 0, 0);
		unless (p = strchr(p, ':')) continue;
		p++;
		printf("%s %.19s%s", buf, ctime(&t), p);
	}
	fclose(f);
}

int
bk_sfiles(int ac, char **av)
{
	pid_t	pid;
	int	i;
	int	j, pfd;
	int	status;
	char	*sav[2] = {"sfiles", 0};
	char	*cmds[100] = {"bk"};

	assert(ac < 95);
	for (i = 1, j = 0; cmds[i] = av[j]; i++, j++);
	cmds[i++] = "-";
	cmds[i] = 0;
	if ((pid = spawnvp_wPipe(cmds, &pfd)) == -1) {
		fprintf(stderr, "can not spawn %s %s\n", cmds[0], cmds[1]);
		return(1);
	} 
	log_start(sav);
	close(1); dup(pfd); close(pfd);
	if (status = sfiles_main(1, sav)) {
		kill(pid, SIGTERM);
		wait(0);
		log_end(status);
		exit(status);
	}
	fflush(stdout);
	close(1);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) {
		log_end(WEXITSTATUS(status));
		exit(WEXITSTATUS(status));
	}
#ifndef WIN32
	if (WIFSIGNALED(status)) {
		fprintf(stderr,
		    "Child was signaled with %d\n",
		    WTERMSIG(status));
		log_end(WTERMSIG(status));
		exit(WTERMSIG(status));
	}
#endif
	log_end(100);
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
	fprintf(stderr, "Can not find wish to run\n");
	exit(1);
}

char *
find_perl5()
{
	char buf[MAXLINE];
	char *p, *s;
	char path[MAXLINE];
	static char perl_path[MAXPATH];
	int more = 1;

	p  = getenv("PATH");
	if (p) {;
		sprintf(path, "%s:/usr/local/bin", p);
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
		sprintf(perl_path, "%s/perl.exe", p);
		unless (exists(perl_path)) goto next;
		return(perl_path); /* win32 perl is version 5 */
#else
		sprintf(perl_path, "%s/perl", p);
		unless (executable(perl_path)) goto next;
#endif
		sprintf(buf, "%s -v | grep -E '(version 5.0)|(perl, v5.[0-9])' > %s",
			perl_path, DEV_NULL);
		if (system(buf) == 0)	return(perl_path);
next:		p = ++s;
	}
	fprintf(stderr, "Can not find perl5 to run\n");
	exit(1);
}

