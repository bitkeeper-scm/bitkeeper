#ifdef WIN32
#include <windows.h>
#endif
#include "system.h"
#include "sccs.h" 

#define BK "bk"

char	*editor = 0, *pager = 0, *bin = 0;
char	*BitKeeper = "BitKeeper/";	/* XXX - reset this? */

char	*find_wish();
char	*find_perl5();
extern	void getoptReset();
private void platformInit(char **av);
private int sfiles(int ac, char **av);

int unedit_main(int, char **);
int unlock_main(int, char **);
int files_main(int, char **);
int bkd_main(int, char **);
int setup_main(int, char **);
int commit_main(int, char **);
int status_main(int, char **);
int version_main(int, char **);
int help_main(int, char **);
int users_main(int, char **);
int parent_main(int, char **);
int clone_main(int, char **);
int send_main(int, char **);
int unwrap_main(int, char **);
int receive_main(int, char **);
int fix_main(int, char **);
int undo_main(int, char **);
int sendbug_main(int, char **);
int export_main(int, char **);
int setlog_main(int, char **);
int getlog_main(int, char **);
int gethelp_main(int, char **);
int logaddr_main(int, char **);
int sendconfig_main(int, char **);
int takepatch_main(int, char **);
int clean_main(int, char **);
int prs_main(int, char **);
int mv_main(int, char **);
int rm_main(int, char **);
int get_main(int, char **);
int delta_main(int, char **);
int sinfo_main(int, char **);
int sccscat_main(int, char **);
int cset_main(int, char **);
int diffs_main(int, char **);
int sccslog_main(int, char **);
int rmdel_main(int, char **);
int getuser_main(int, char **);
int gethost_main(int, char **);
int admin_main(int, char **);
int g2sccs_main(int, char **);
int key2rev_main(int, char **);
int lines_main(int, char **);
int sfiles_main(int, char **);
int sfio_main(int, char **);
int sids_main(int, char **);
int check_main(int, char **);
int chksum_main(int, char **);
int fdiff_main(int, char **);
int adler32_main(int, char **);
int lod_main(int, char **);
int range_main(int, char **);
int rechksum_main(int, char **);
int renumber_main(int, char **);
int stripdel_main(int, char **);
int smoosh_main(int, char **);
int undos_main(int, char **);
int what_main(int, char **);
int gca_main(int, char **);
int mtime_main(int, char **);
int zone_main(int, char **);
int isascii_main(int, char **);
int r2c_main(int, char **);
int pending_main(int, char **);
int resolve_main(int, char **);
int push_main(int, char **);
int names_main(int, char **);
int lock_main(int, char **);
int repo_main(int, char **);
int pull_main(int, char **);
int log_main(int, char **);
int abort_main(int, char **);
int mklock_main(int, char **);

struct command cmdtbl[100] = {
	{"unlock", unlock_main },
	{"files", files_main },
	{"bkd", bkd_main },
	{"setup", setup_main },
	{"commit", commit_main},
	{"status", status_main},
	{"version", version_main},
	{"help", help_main},
	{"users", users_main},
	{"parent", parent_main},
	{"clone", clone_main},
	{"send", send_main},
	{"unwrap", unwrap_main},
	{"receive", receive_main},
	{"fix", fix_main},
	{"undo", undo_main},
	{"sendbug", sendbug_main},
	{"export", export_main},
	{"setlog", setlog_main},
	{"getlog", getlog_main},
	{"gethelp", gethelp_main},
	{"logaddr", logaddr_main},
	{"sendconfig", sendconfig_main},
	{"takepatch", takepatch_main},
	{"clean", clean_main},
	{"unedit", unedit_main},
	{"unget", unedit_main},	/* aliases */
	{"prs", prs_main},
	{"mv", mv_main},
	{"sccsmv", mv_main},
	{"rm", rm_main},
	{"sccsrm", rm_main},
	{"get", get_main},
	{"edit", get_main},	/* aliases */
	{"co", get_main},
	{"delta", delta_main},
	{"ci", delta_main},
	{"new", delta_main},	/* aliases */
	{"sinfo", sinfo_main},
	{"info", sinfo_main},	/* aliases */
	{"sccscat", sccscat_main},
	{"cset", cset_main},
	{"diffs", diffs_main},
	{"sccslog", sccslog_main},
	{"rmdel", rmdel_main},
	{"getuser", getuser_main},
	{"gethost", gethost_main},
	{"admin", admin_main},
	{"g2sccs", g2sccs_main},
	{"key2rev", key2rev_main},
	{"lines", lines_main},
	{"sfiles", sfiles_main},
	{"sfio", sfio_main},
	{"sids", sids_main},
	{"check", check_main},
	{"chksum", chksum_main},
	{"fdiff", fdiff_main},
	{"adler32", adler32_main},
	{"lod", lod_main},
	{"range", range_main},
	{"rechksum", rechksum_main},
	{"renumber", renumber_main},
	{"stripdel", stripdel_main},
	{"smoosh", smoosh_main},
	{"undos", undos_main},
	{"what", what_main},
	{"gca", gca_main},
	{"mtime", mtime_main},
	{"zone", zone_main},
	{"isascii", isascii_main},
	{"r2c", r2c_main},
	{"rev2cset", r2c_main},
	{"pending", pending_main},
	{"resolve", resolve_main},
	{"push", push_main},
	{"names", names_main},
	{"repo", repo_main},
	{"lock", lock_main},
	{"pull", pull_main},
	{"log", log_main},
	{"abort", abort_main},
	{"mklock", mklock_main}, /* for regression test only */
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
	char	*prog;

	/*
	 * XXX TODO: implement "__logCommand"
	 */

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
				} else if (sccs_cd2root(0, 0) == -1) {
					fprintf(stderr, 
					    "bk: Can not find project root.\n");
					return(1);
				}
				dashr++;
				break;
			    case 'R':
				if (sccs_cd2root(0, 0) == -1) {
					fprintf(stderr, 
					    "bk: Can not find project root.\n");
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
				return (sfiles(ac, av));
			}
		}
		prog = av[0];
	}
	getoptReset();

	/*
	 * look up the internal command 
	 */
	for (i = 0; cmdtbl[i].name; i++) {
		if (streq(cmdtbl[i].name, prog)){
			return (cmdtbl[i].func(ac, av));
		}
	}
	unless(is_bk) {
		fprintf(stderr, "%s is not a linkable command\n",  prog);
		exit(1);
	}

	/*
	 * Is it is a perl 4 script ?
	 */
	if (streq(av[0], "pmerge")) {
		argv[0] = "perl"; 
		sprintf(cmd_path, "%s/%s", bin, av[0]);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) argv[i] = av[j];
		argv[i] = 0;
		return (spawnvp_ex(_P_WAIT, argv[0], argv));;
	}

	/*
	 * Is it is a perl 5 script ?
	 */
	if (streq(av[0], "mkdiffs") ||
	    streq(av[0], "rcs2sccs")) {
		argv[0] = find_perl5();
		sprintf(cmd_path, "%s/%s", bin, av[0]);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) argv[i] = av[j];
		argv[i] = 0;
		return (spawnvp_ex(_P_WAIT, argv[0], argv));;
	}

	/*
	 * Handle Gui script
	 */
	if (streq(av[0], "fm") ||
	    streq(av[0], "fm3") ||
	    streq(av[0], "citool") ||
	    streq(av[0], "sccstool") ||
	    streq(av[0], "fmtool") ||
	    streq(av[0], "fm3tool") ||
	    streq(av[0], "difftool") ||
	    streq(av[0], "helptool") ||
	    streq(av[0], "csettool") ||
	    streq(av[0], "renametool")) {
		argv[0] = find_wish();
		sprintf(cmd_path, "%s/%s", bin, av[0]);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) argv[i] = av[j];
		argv[i] = 0;
		return (spawnvp_ex(_P_WAIT, argv[0], argv));
	}

	/*
	 * Handle shell script
	 */
	if (streq(av[0], "resync") || 
		streq(av[0], "import")) {
#ifdef WIN32
		argv[0] = "bash"; /* because the script uses getopt */
#else
		argv[0] = "/bin/sh";
#endif
		sprintf(cmd_path, "%s/%s", bin, av[0]);
		argv[1] = cmd_path;
		for (i = 2, j = 1; av[j]; i++, j++) {
			argv[i] = av[j];
		}
		argv[i] = 0;
		for (i = 0; argv[i] != 0;  i++) {
		}
		return (spawnvp_ex(_P_WAIT, argv[0], argv));
	}

	/*
	 * Is it a known C program ?
	 */
	if (streq(av[0], "patch") ||
	    streq(av[0], "diff3")) {
		return (spawnvp_ex(_P_WAIT, av[0], av));
	}

	/*
	 * If we get here, it is a 
	 * a) bk shell function
	 *    or
	 * b) external program/script
	 * XXX This is slow because we are going thru the shell
	 */
#ifdef WIN32
	argv[0] = "bash"; /* because the script uses getopt */
#else
	argv[0] = "/bin/sh";
#endif
	sprintf(cmd_path, "%s/bk.script", bin);
	argv[1] = cmd_path;
	for (i = 2, j = 0; av[j]; i++, j++) argv[i] = av[j];
	argv[i] = 0;
	return (spawnvp_ex(_P_WAIT, argv[0], argv));
}

private int
sfiles(int ac, char **av)
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
	close(1); dup(pfd); close(pfd);
	status = sfiles_main(1, sav);
	fflush(stdout);
	close(1);
	waitpid(pid, &status, 0);
	if (WIFEXITED(status)) return(WEXITSTATUS(status));
#ifndef WIN32
	if (WIFSIGNALED(status)) {
		fprintf(stderr,
		    "Child was signaled with %d\n",
		    WTERMSIG(status));
		exit(WTERMSIG(status));
	}
#endif
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
		sprintf(buf, "%s -v | grep 'version 5.0' > %s",
			perl_path, DEV_NULL);
		if (system(buf) == 0)	return(perl_path);
next:		p = ++s;
	}
	fprintf(stderr, "Can not find perl5 to run\n");
	exit(1);
}


// XXX TODO move this function to the port directory
// This should be done *after* we merge with the main tree
private void
platformInit(char **av)
{
	char	*p, *t, *s;
	static	char buf[MAXPATH], buf1[MAXPATH];
	char	buf2[10 * MAXPATH], link[MAXPATH];
	int	add2path = 1;
	int	n;
	int	flags = SILENT;	/* for debugging */

	if (bin) return;
	if ((editor = getenv("EDITOR")) == NULL) editor = strdup("vi");
	if ((pager = getenv("PAGER")) == NULL) pager = strdup(PAGER);

	unless (p = getenv("PATH")) return;	/* and pray */
#ifdef WIN32
	setmode(1, _O_BINARY);
	setmode(2, _O_BINARY);
	localName2bkName(av[0], buf1);	av[0] = buf1;
	localName2bkName(p, buf2);	p = buf2;
#endif

	/*
	 * Find the program and if it is a symlink, then add where it
	 * points to the path.
	 * Otherwise, set the bin dir to whereever we found the program.
	 */
	if (IsFullPath(av[0]) && executable(av[0])) {
		verbose((stderr, "USING fullpath %s\n", av[0]));
		strcpy(buf, av[0]);
gotit:		
		if ((n = readlink(buf, link, sizeof(link))) != -1) {
			add2path = 1;
			link[n] = 0;
			verbose((stderr, "LINK %s->%s\n", buf, link));
			strcpy(buf, link);
		}
		t = strrchr(buf, '/');
		*t = 0;
#ifdef WIN32
		/*
		 * Convert to short path name, because the shell 
		 * script can not handle space in path name.
		 */
		GetShortPathName(buf, buf, sizeof(buf));
#endif
		localName2bkName(buf, buf);
		bin = buf; /* buf is static */

		if (add2path) {
			/*
			 * Hide the malloc from purify,
			 * We can not free it until we exit anyway.
			 */
			s = (malloc)(2* strlen(buf) + strlen(p) + 30);
			sprintf(s, "PATH=%s%c%s/gnu/bin%c%s",
			    		buf, PATH_DELIM, buf, PATH_DELIM, p);
			putenv(s);
		}
#ifdef WIN32
		/* convert to lower case: because W98 gives us upper case av */
		p = av[0];
		while (*p) { *p = tolower(*p); p++; }
		p = strrchr(av[0], '.');
		if (p && streq(".exe", p)) *p = 0; /* remove .exe */
#endif
		return;
	}

	/* partially specified paths are respected */
	if (t = strchr(av[0], '/')) {
		verbose((stderr, "USING partial %s\n", av[0]));
		getcwd(buf, sizeof(buf));
		strcat(buf, "/");
		strcat(buf, av[0]);
		goto gotit;
	}
	
	for (t = s = p; *s;) {
		t = strchr(s, PATH_DELIM);
		if (t) *t = 0;
		sprintf(buf, "%s/%s", s, av[0]);
		if (executable(buf)) {
		verbose((stderr, "USING PATH %s\n", buf));
			unless (IsFullPath(s)) {
				getcwd(buf, sizeof(buf));
				strcat(buf, "/");
				strcat(buf, s);
				strcat(buf, "/");
				strcat(buf, av[0]);
			} else {
				add2path = 0;
			}
			if (t) *t = PATH_DELIM;
			goto gotit;
			
		}
		if (t) {
			*t = ':';
			s = t + 1;
		} else {
			break;
		}
		
	}
	return;
}
